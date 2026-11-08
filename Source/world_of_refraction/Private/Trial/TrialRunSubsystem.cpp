// TrialRunSubsystem.cpp

#include "Trial/TrialRunSubsystem.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryData.h"
#include "Trial/DefeatRules.h"
#include "WoRLog.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Merchant/MerchantInteractable.h"
#include "Merchant/MerchantStockRoll.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Shop/MerchantShopSubsystem.h"
#include "TimerManager.h"
#include "Trial/TrialData.h"
#include "Trial/PrismDropPickup.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Currency/MentorTokens.h"

namespace
{
    TAutoConsoleVariable<int32> CVarTrialSeed(
        TEXT("wor.Trial.Seed"),
        0,
        TEXT("Seed for every following trial run (0 = a fresh seed each run). Trial vendors roll their stock from it. ")
        TEXT("Copy the seed from the [TrialRun] log line to see the same stock again."));

    /** 0 means "no run", so a generated seed of 0 is bumped to this. */
    constexpr int32 TRIAL_NONZERO_SEED_FALLBACK = 1;

    /** B-347 (D-137): opens the run layer and takes the local party's worn loadouts on the run, free. */
    void OpenRunLayerWithWornLoadouts(UGameInstance *GI, UPoolSubsystem &Pool)
    {
        TSet<FName> MemberKeys;
        const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
        {
            for (const FPartyMember &Member : Party->Members)
            {
                MemberKeys.Add(UPoolSubsystem::GetCharacterKey(Member));
            }
        }
        MemberKeys.Remove(NAME_None);
        Pool.BeginRunLayer();
        Pool.TakeHomeGearOnRun(MemberKeys);
    }

    int32 MakeFreshRunSeed()
    {
        const int32 NewSeed = static_cast<int32>(FGuid::NewGuid().A & static_cast<uint32>(MAX_int32));
        return NewSeed != 0 ? NewSeed : TRIAL_NONZERO_SEED_FALLBACK;
    }
}

void UTrialRunSubsystem::EnterTrial(UTrialData *Trial)
{
    if (!Trial)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] EnterTrial: null Trial — ignored."));
        return;
    }
    if (Trial->Level.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] EnterTrial: '%s' has no Level authored — ignored."),
               *Trial->GetName());
        return;
    }

    // The shop's modal bracket (UIOnly input + cursor + pause) must not straddle
    // a level transition — close defensively; no-op when nothing is open.
    if (UMerchantShopSubsystem *Shop = GetGameInstance()->GetSubsystem<UMerchantShopSubsystem>())
    {
        Shop->Close();
    }

    // Trial entry is where carry-over BEGINS: everyone starts at their computed
    // maxima (BD members at 0 EP). Deliberately NOT done on EnterEncounter —
    // battle-to-battle within a trial must preserve what the last fight left.
    if (UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>())
    {
        PartySession->SetAllMembersFull();
    }
    StartRunGearAndGold();

    ActiveTrial = Trial;
    BeatenEncounters.Reset();
    bRunBossBeaten = false;
    RunWipes = 0; // B-362
    RunMainBossLosses = 0;
    LastRestSpot = NAME_None;
    DroppedPrisms = 0; // B-362b
    OtherPlayerDrops.Reset(); // B-235
    DropEncounters.Reset();
    DropSpot = FVector::ZeroVector; // B-362c
    bDropHasSpot = false;
    RunLevels.Reset(); // B-223: the run's pillar levels end with it (D-102)
    UnspentLevels.Reset();
    FoundFeatures.Reset();
    RunMemories.Reset();
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingZone = FEncounterZoneInfo();
    const int32 ForcedSeed = CVarTrialSeed.GetValueOnGameThread();
    RunSeed = ForcedSeed != 0 ? ForcedSeed : MakeFreshRunSeed();
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Entering trial '%s' -> %s (run seed %d; replay with: wor.Trial.Seed %d)"),
           *Trial->Name.ToString(), *Trial->Level.ToString(), RunSeed, RunSeed);
    TravelTo(Trial->Level);
}

void UTrialRunSubsystem::StartRunGearAndGold()
{
    // B-178: Gold is the run's currency; a run starts with none.
    if (USaveSubsystem *Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
    {
        Save->ClearRunGold(GetGameInstance()->GetWorld());
    }
    // B-314 (D-111, Q-314) / B-347 (D-137): the run layer opens. The party's worn loadouts go along as they are, free
    // (no start-of-run purchase; Gold is run money for in-run vendors); gear found or bought in the run is run-only.
    // A layer left open by a run that never exited is ended (applied) first: its gear is this session's, not reloaded.
    if (UPoolSubsystem *Pool = GetGameInstance()->GetSubsystem<UPoolSubsystem>())
    {
        Pool->EndRunLayer();
        OpenRunLayerWithWornLoadouts(GetGameInstance(), *Pool);
    }
}

void UTrialRunSubsystem::SetGateChoice(int32 InVariationIndex, ETrialDifficulty InDifficulty)
{
    VariationIndex = InVariationIndex;
    TrialDifficulty = InDifficulty;
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Gate choice: variation %d, difficulty %s (enemy strength x%.2f, payout x%.2f)"),
           VariationIndex, *TrialGate::GetDifficultyName(TrialDifficulty).ToString(), GetEnemyStrengthMultiplier(),
           GetPayoutMultiplier());
}

const FTrialVariation *UTrialRunSubsystem::GetChosenVariation() const
{
    return ActiveTrial && ActiveTrial->Variations.IsValidIndex(VariationIndex) ? &ActiveTrial->Variations[VariationIndex] : nullptr;
}

void UTrialRunSubsystem::ExitTrial()
{
    if (HubLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[TrialRun] ExitTrial: HubLevel not configured (DefaultGame.ini) — ignored."));
        return;
    }

    // B-314 (D-111, D-097): the run layer closes while the members are still here to hear it: home pieces go back to
    // their home state, run-only gear leaves with its tier remembered. Before the exit save below.
    if (UPoolSubsystem *Pool = GetGameInstance()->GetSubsystem<UPoolSubsystem>())
    {
        Pool->EndRunLayer();
    }
    // B-377: a hidden quest's step in progress starts its count again next run; steps done stay (MemoryCatalogue §3.6).
    if (UHiddenQuestSubsystem *Quests = GetGameInstance()->GetSubsystem<UHiddenQuestSubsystem>())
    {
        Quests->EndRun();
    }

    // Hub return is the factory reset: drop every slot back to the sentinel so
    // the next EnterTrial seeds fresh rather than inheriting the last run's
    // damage. Pairs with SetAllMembersFull above.
    if (UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>())
    {
        // B-076 (D-026): characters found in the trial leave with it.
        PartySession->DismissRunMembers();
        PartySession->ClearAllMemberVitals();
    }
    // B-178: the run's Gold is lost when it ends (never banked).
    if (USaveSubsystem *Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
    {
        Save->ClearRunGold(GetGameInstance()->GetWorld());
    }

    // B-361b (D-154, Q-361b): a finished co-op run (its boss beaten) pays every player; leaving early pays nothing.
    if (bRunBossBeaten)
    {
        WoRMentorTokens::PayPlayersTogether(GetGameInstance()->GetWorld(), MentorTokenRules::PER_COOP_RUN, TEXT("a co-op run finished"));
    }
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Exiting trial '%s' -> hub %s"),
           ActiveTrial ? *ActiveTrial->Name.ToString() : TEXT("<none>"),
           *HubLevel.ToString());
    ActiveTrial = nullptr;
    RunSeed = 0;
    VariationIndex = INDEX_NONE;
    TrialDifficulty = TrialGate::DEFAULT_DIFFICULTY;
    BeatenEncounters.Reset();
    bRunBossBeaten = false;
    RunWipes = 0; // B-362
    RunMainBossLosses = 0;
    LastRestSpot = NAME_None;
    DroppedPrisms = 0; // B-362b
    OtherPlayerDrops.Reset(); // B-235
    DropEncounters.Reset();
    DropSpot = FVector::ZeroVector; // B-362c
    bDropHasSpot = false;
    RunLevels.Reset(); // B-223: the run's pillar levels end with it (D-102)
    UnspentLevels.Reset();
    FoundFeatures.Reset();
    RunMemories.Reset();
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingZone = FEncounterZoneInfo();
    // B-362: a run ended by losses leaves from the battle level, with its fight's stash still set; the hub has no fight.
    PendingBattleParty.Reset();
    PendingOpposingParty.Reset();
    EncounterReturnLevel = nullptr;
    // B-090: a trial's exit is a save point.
    GetGameInstance()->GetSubsystem<USaveSubsystem>()->SaveNow(GetGameInstance()->GetWorld(), true);
    TravelTo(HubLevel);
}

void UTrialRunSubsystem::ExitTrialNextTick()
{
    UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (HubLevel.IsNull() || !World)
    {
        UE_LOG(LogTemp, Error, TEXT("[TrialRun] ExitTrialNextTick: no HubLevel (DefaultGame.ini) or world - the run cannot end; returning to the trial instead."));
        ExitEncounter();
        return;
    }
    // Once only: a second call queued in the same frame finds the run already over.
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if (IsRunActive())
        {
            ExitTrial();
        }
    }));
}

// ==================== ENCOUNTER (T-C1) ====================

void UTrialRunSubsystem::EnterEncounter(UTrialData *Trial, const TArray<UCharacterData *> &BattleParty,
                                        const TArray<UCharacterData *> &OpposingParty, EAIDifficulty Difficulty,
                                        EEncounterRank Rank)
{
    if (!Trial || Trial->EncounterLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] EnterEncounter: %s — ignored."),
               Trial ? TEXT("trial has no EncounterLevel authored") : TEXT("null Trial"));
        return;
    }
    if (BattleParty.Num() == 0 || OpposingParty.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] EnterEncounter: empty roster (BattleParty %d, OpposingParty %d) — ignored."),
               BattleParty.Num(), OpposingParty.Num());
        return;
    }

    // Same modal-bracket defense as EnterTrial.
    if (UMerchantShopSubsystem *Shop = GetGameInstance()->GetSubsystem<UMerchantShopSubsystem>())
    {
        Shop->Close();
    }

    PendingBattleParty = TArray<TObjectPtr<UCharacterData>>(BattleParty);
    PendingOpposingParty = TArray<TObjectPtr<UCharacterData>>(OpposingParty);
    PendingDifficulty = Difficulty;
    PendingRank = Rank;
    EncounterReturnLevel = Trial->Level;
    // B-362c (D-160): where the party stands on the trial map now is where a wipe's Prisms drop.
    UWorld *MapWorld = GetGameInstance()->GetWorld();
    const APlayerController *Leader = MapWorld && IsActiveTrialWorld(MapWorld) ? MapWorld->GetFirstPlayerController() : nullptr;
    const APawn *LeaderPawn = Leader ? Leader->GetPawn() : nullptr;
    if (LeaderPawn)
    {
        SetPendingFightSpot(LeaderPawn->GetActorLocation());
    }

    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Entering encounter %d v %d -> %s (return %s)"),
           BattleParty.Num(), OpposingParty.Num(), *Trial->EncounterLevel.ToString(), *EncounterReturnLevel.ToString());
    TravelTo(Trial->EncounterLevel);
}

void UTrialRunSubsystem::ExitEncounter()
{
    const TSoftObjectPtr<UWorld> ReturnLevel = !EncounterReturnLevel.IsNull() ? EncounterReturnLevel : HubLevel;
    if (ReturnLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[TrialRun] ExitEncounter: no return level (EncounterReturnLevel AND HubLevel unset) — ignored."));
        return;
    }

    PendingBattleParty.Reset();
    PendingOpposingParty.Reset();
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingZone = FEncounterZoneInfo();
    PendingFightSpot = FVector::ZeroVector; // B-362c
    bHasPendingFightSpot = false;
    EncounterReturnLevel = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Exiting encounter -> %s (deferred one tick)"), *ReturnLevel.ToString());

    // Called from OnCombatResultReady mid-orchestrator-teardown — the OpenLevel
    // must not run inside that broadcast.
    UWorld *World = GetGameInstance()->GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[TrialRun] ExitEncounter: no world to defer through — ignored."));
        return;
    }
    World->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this, [this, ReturnLevel]()
                                         {
                                             // B-090: a fight's exit is a save point (the vitals are banked by now).
                                             GetGameInstance()->GetSubsystem<USaveSubsystem>()->SaveNow(GetGameInstance()->GetWorld(), true);
                                             TravelTo(ReturnLevel);
                                         }));
}

int32 UTrialRunSubsystem::GetRunEarnedLevels(FName CharacterKey, EWorldPillar Pillar) const
{
    const FRunLevels *Levels = RunLevels.Find(CharacterKey);
    return Levels ? Levels->Get(Pillar) : 0;
}

void UTrialRunSubsystem::AddRunEarnedLevels(FName CharacterKey, EWorldPillar Pillar, int32 Count)
{
    if (!CharacterKey.IsNone() && Count > 0)
    {
        RunLevels.FindOrAdd(CharacterKey).For(Pillar) += Count;
    }
}

int32 UTrialRunSubsystem::GetRunLevelPurchases(FName CharacterKey) const
{
    const FRunLevels *Levels = RunLevels.Find(CharacterKey);
    return Levels ? Levels->Purchases : 0;
}

void UTrialRunSubsystem::AddRunLevelPurchase(FName CharacterKey)
{
    if (!CharacterKey.IsNone())
    {
        ++RunLevels.FindOrAdd(CharacterKey).Purchases;
    }
}

void UTrialRunSubsystem::ForgetRunLevels(FName CharacterKey)
{
    RunLevels.Remove(CharacterKey);
    UnspentLevels.Remove(CharacterKey); // B-223b: its waiting levels go with its run
}

int32 UTrialRunSubsystem::GetUnspentLevels(FName CharacterKey) const
{
    return UnspentLevels.FindRef(CharacterKey);
}

int32 UTrialRunSubsystem::GetTotalUnspentLevels() const
{
    int32 Total = 0;
    for (const TPair<FName, int32> &Pair : UnspentLevels)
    {
        Total += Pair.Value;
    }
    return Total;
}

void UTrialRunSubsystem::AddUnspentLevels(FName CharacterKey, int32 Count)
{
    if (Count > 0 && !CharacterKey.IsNone())
    {
        const int32 Waiting = UnspentLevels.FindOrAdd(CharacterKey) += Count;
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] %s: +%d pillar level(s) to place (%d waiting): wor.UI.LevelDrop"),
               *FPaths::GetBaseFilename(CharacterKey.ToString()), Count, Waiting);
    }
}

bool UTrialRunSubsystem::TakeUnspentLevel(FName CharacterKey)
{
    int32 *Waiting = UnspentLevels.Find(CharacterKey);
    if (!Waiting || *Waiting <= 0)
    {
        return false;
    }
    if (--*Waiting <= 0)
    {
        UnspentLevels.Remove(CharacterKey);
    }
    return true;
}

bool UTrialRunSubsystem::RecordWipe(EEncounterRank Rank)
{
    ++RunWipes;
    if (DefeatRules::IsMainBoss(Rank))
    {
        ++RunMainBossLosses;
    }
    const bool bRunOver = DefeatRules::RunEnds(RunWipes, RunMainBossLosses);
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Wipe %d of %d (main-boss losses %d of %d)%s"), RunWipes, DefeatRules::MAX_WIPES_PER_RUN,
           RunMainBossLosses, DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN, bRunOver ? TEXT(": the run is over") : TEXT(""));
    return bRunOver;
}

int32 UTrialRunSubsystem::LoseRunPoints(FName CharacterKey, int32 MaxPoints)
{
    FRunLevels *Levels = RunLevels.Find(CharacterKey);
    int32 Taken = 0;
    EWorldPillar Pillar = EWorldPillar::Mind;
    while (Levels && Taken < MaxPoints && DefeatRules::PickPillarToLose(Levels->Mind, Levels->Body, Levels->Spirit, Pillar))
    {
        --Levels->For(Pillar);
        ++Taken;
    }
    return Taken;
}

int32 UTrialRunSubsystem::DropPrisms(int32 Amount)
{
    const int32 LostForGood = DroppedPrisms;
    DroppedPrisms = FMath::Max(Amount, 0);
    DropEncounters = PendingEncounterSources;
    bDropHasSpot = GetPendingFightSpot(DropSpot);
    if (!bDropHasSpot)
    {
        DropSpot = FVector::ZeroVector;
    }
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] %d Prism(s) dropped at %s%s"), DroppedPrisms,
           bDropHasSpot ? *DropSpot.ToCompactString() : TEXT("the trial's start (the fight had no map spot)"),
           LostForGood > 0 ? *FString::Printf(TEXT("; the earlier drop of %d is lost for good"), LostForGood) : TEXT(""));
    return LostForGood;
}

int32 UTrialRunSubsystem::DropPrismsForPlayer(const FString &PlayerKey, int32 Amount)
{
    int32 &Drop = OtherPlayerDrops.FindOrAdd(PlayerKey);
    const int32 LostForGood = Drop;
    Drop = FMath::Max(Amount, 0);
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Player %s dropped %d Prism(s)%s"), *PlayerKey, Drop,
           LostForGood > 0 ? *FString::Printf(TEXT("; their earlier drop of %d is lost for good"), LostForGood) : TEXT(""));
    return LostForGood;
}

int32 UTrialRunSubsystem::TakeDropForPlayer(const FString &PlayerKey)
{
    int32 Taken = 0;
    OtherPlayerDrops.RemoveAndCopyValue(PlayerKey, Taken);
    // Review fix: the spot goes with the last drop, whoever's it was (as TakeDrop does).
    if (GetTotalDroppedPrisms() <= 0)
    {
        DropEncounters.Reset();
        DropSpot = FVector::ZeroVector;
        bDropHasSpot = false;
    }
    if (Taken > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] Player %s picked up their %d dropped Prism(s)"), *PlayerKey, Taken);
    }
    return Taken;
}

int32 UTrialRunSubsystem::GetDroppedPrismsForPlayer(const FString &PlayerKey) const
{
    const int32 *Drop = OtherPlayerDrops.Find(PlayerKey);
    return Drop ? *Drop : 0;
}

int32 UTrialRunSubsystem::GetTotalDroppedPrisms() const
{
    int32 Total = DroppedPrisms;
    for (const TPair<FString, int32> &Pair : OtherPlayerDrops)
    {
        Total += Pair.Value;
    }
    return Total;
}

int32 UTrialRunSubsystem::TakeDrop()
{
    const int32 Taken = DroppedPrisms;
    DroppedPrisms = 0;
    DropEncounters.Reset();
    // B-235: the spot stays while another player's drop still lies there.
    if (GetTotalDroppedPrisms() <= 0)
    {
        DropSpot = FVector::ZeroVector;
        bDropHasSpot = false;
    }
    if (Taken > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] %d dropped Prism(s) picked up"), Taken);
    }
    return Taken;
}

bool UTrialRunSubsystem::GetDropSpot(FVector &OutSpot) const
{
    OutSpot = DropSpot;
    return bDropHasSpot;
}

bool UTrialRunSubsystem::GetPendingFightSpot(FVector &OutSpot) const
{
    OutSpot = PendingFightSpot;
    return bHasPendingFightSpot;
}

void UTrialRunSubsystem::SetPendingFightSpot(const FVector &Spot)
{
    PendingFightSpot = Spot;
    bHasPendingFightSpot = true;
}

bool UTrialRunSubsystem::IsActiveTrialWorld(const UWorld *World) const
{
    if (!World || !ActiveTrial || ActiveTrial->Level.IsNull())
    {
        return false;
    }
    return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == ActiveTrial->Level.GetLongPackageName();
}

void UTrialRunSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    WorldActorsInitializedHandle =
        FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UTrialRunSubsystem::HandleWorldActorsInitialized);
}

void UTrialRunSubsystem::Deinitialize()
{
    FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
    WorldActorsInitializedHandle.Reset();
    Super::Deinitialize();
}

void UTrialRunSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams &Params)
{
    UWorld *World = Params.World;
    // The server places it (a client's map follows the server's; co-op per-player pickups are B-235).
    if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_Client)
    {
        return;
    }
    if (IsRunActive() && GetTotalDroppedPrisms() > 0 && IsActiveTrialWorld(World)) // B-235: anyone's drop
    {
        APrismDropPickup::PlaceForRun(World, *this);
    }
}

int32 UTrialRunSubsystem::TakeDropIfPendingFight()
{
    const bool bFromDropFight = PendingEncounterSources.ContainsByPredicate([this](FName Id) { return DropEncounters.Contains(Id); });
    if (!bFromDropFight || DroppedPrisms <= 0)
    {
        return 0;
    }
    const int32 Recovered = DroppedPrisms;
    DroppedPrisms = 0;
    DropEncounters.Reset();
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] %d dropped Prism(s) recovered"), Recovered);
    return Recovered;
}

void UTrialRunSubsystem::RestoreDrop(int32 Amount, bool bHasSpot, const FVector &Spot)
{
    DroppedPrisms = FMath::Max(Amount, 0);
    DropEncounters.Reset();
    bDropHasSpot = bHasSpot && DroppedPrisms > 0;
    DropSpot = bDropHasSpot ? Spot : FVector::ZeroVector;
}

void UTrialRunSubsystem::RestoreRunLosses(int32 Wipes, int32 MainBossLosses, FName RestSpotId)
{
    RunWipes = FMath::Max(Wipes, 0);
    RunMainBossLosses = FMath::Max(MainBossLosses, 0);
    LastRestSpot = RestSpotId;
}

FString UTrialRunSubsystem::GetRunLevelsString() const
{
    FString Out = FString::Printf(TEXT("[TrialRun] Run pillar levels (B-223b, per character): %d unspent in all"), GetTotalUnspentLevels());
    for (const TPair<FName, FRunLevels> &Pair : RunLevels)
    {
        Out += FString::Printf(TEXT("\n  %s: Mind +%d, Body +%d, Spirit +%d; bought %d; %d to place"), *FPaths::GetBaseFilename(Pair.Key.ToString()),
                               Pair.Value.Mind, Pair.Value.Body, Pair.Value.Spirit, Pair.Value.Purchases, GetUnspentLevels(Pair.Key));
    }
    for (const TPair<FName, int32> &Pair : UnspentLevels)
    {
        if (!RunLevels.Contains(Pair.Key))
        {
            Out += FString::Printf(TEXT("\n  %s: %d to place"), *FPaths::GetBaseFilename(Pair.Key.ToString()), Pair.Value);
        }
    }
    return Out;
}

void UTrialRunSubsystem::MarkPendingEncounterBeaten()
{
    for (const FName EncounterId : PendingEncounterSources)
    {
        BeatenEncounters.Add(EncounterId);
    }
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] %d map encounter(s) beaten; %d beaten this run"),
           PendingEncounterSources.Num(), BeatenEncounters.Num());
}

bool UTrialRunSubsystem::CollectMemory(UMemoryData *Memory)
{
    if (!Memory || RunMemories.Contains(Memory))
    {
        return false;
    }
    RunMemories.Add(Memory);
    UE_LOG(LogWoR, Log, TEXT("[TrialRun] Memory %s collected (%d this run)"), *Memory->GetMemoryId().ToString(),
           RunMemories.Num());
    return true;
}

void UTrialRunSubsystem::RecordFound(FName FeatureId, const FText &What)
{
    FoundFeatures.Add(FeatureId, What);
    UE_LOG(LogWoR, Log, TEXT("[TrialRun] Found %s: %s (%d found this run)"), *FeatureId.ToString(),
           *What.ToString(), FoundFeatures.Num());
}

TArray<FMerchantStockEntry> UTrialRunSubsystem::GetMerchantStock(const UMerchantData *Merchant) const
{
    // RunSeed says a run is on: a trial vendor stocks for the run, a home vendor as a home shop.
    return Merchant ? MerchantStockRoll::StockFor(*Merchant, RunSeed != 0, RunSeed) : TArray<FMerchantStockEntry>();
}

FString UTrialRunSubsystem::GetMerchantStockString(const UMerchantData *Merchant) const
{
    if (!Merchant)
    {
        return TEXT("[TrialRun] Merchant=<null>");
    }
    const TArray<FMerchantStockEntry> Stock = GetMerchantStock(Merchant);
    FString Out = FString::Printf(TEXT("[TrialRun] %s: %d of %d line(s) (%s, run seed %d)"), *Merchant->GetName(),
                                  Stock.Num(), Merchant->Stock.Num(), RunSeed != 0 ? TEXT("rolled") : TEXT("home"), RunSeed);
    for (const FMerchantStockEntry &Entry : Stock)
    {
        Out += FString::Printf(TEXT("\n  %s x%d"),
                               Entry.Asset ? *Entry.Asset->GetName() : *ItemIdentity::GetDisplayName(Entry.Crystal), Entry.Count);
    }
    return Out;
}

#if !UE_BUILD_SHIPPING
namespace
{
    FAutoConsoleCommandWithWorld GPrintTrialRunCmd(
        TEXT("wor.Trial.Print"),
        TEXT("Print the trial run: active trial, the gate's variation and difficulty with its multipliers, pending encounter (B-183)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                if (const UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr)
                {
                    UE_LOG(LogTemp, Display, TEXT("%s"), *TrialRun->GetTrialRunString());
                }
            }));

    FAutoConsoleCommandWithWorld GPrintMerchantStockCmd(
        TEXT("wor.Trial.PrintMerchantStock"),
        TEXT("Print what every merchant in this level shows now: its rolled stock in a trial, its full list at home (B-179)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                const UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
                if (!TrialRun)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[TrialRun] wor.Trial.PrintMerchantStock: no subsystem (no PIE world?)."));
                    return;
                }
                int32 Merchants = 0;
                for (TActorIterator<AMerchantInteractable> It(World); It; ++It)
                {
                    UE_LOG(LogTemp, Display, TEXT("%s"), *TrialRun->GetMerchantStockString(It->Merchant));
                    ++Merchants;
                }
                if (Merchants == 0)
                {
                    UE_LOG(LogTemp, Display, TEXT("[TrialRun] wor.Trial.PrintMerchantStock: no merchants in this level."));
                }
            }));
}
#endif // !UE_BUILD_SHIPPING

void UTrialRunSubsystem::RestoreRun(UTrialData *Trial, int32 InRunSeed, int32 InVariationIndex, ETrialDifficulty InDifficulty,
                                    const TArray<FName> &Beaten, const TMap<FName, FText> &Found,
                                    const TArray<UMemoryData *> &Memories)
{
    ActiveTrial = Trial;
    RunSeed = InRunSeed;
    VariationIndex = InVariationIndex;
    TrialDifficulty = InDifficulty;
    BeatenEncounters = TSet<FName>(Beaten);
    FoundFeatures = Found;
    RunMemories = TArray<TObjectPtr<UMemoryData>>(Memories);
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingZone = FEncounterZoneInfo();
    // B-314: a resumed run opens a fresh layer; B-314b: USaveSubsystem::RestoreRun then replaces it with the save's own
    // layer when the save has one (the run-only marks and home states as saved).
    UPoolSubsystem *Pool = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
    if (Pool && IsRunActive())
    {
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] Resumed run: a fresh run layer (the save's own layer replaces it when it has one, B-314b)"));
        Pool->DiscardRunLayer(); // the store was reloaded: an open layer's home states are stale
        OpenRunLayerWithWornLoadouts(GetGameInstance(), *Pool);
    }
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Restored saved run of '%s' (seed %d, %d beaten, %d found, %d memories)"),
           Trial ? *Trial->Name.ToString() : TEXT("<none>"), RunSeed, BeatenEncounters.Num(), FoundFeatures.Num(),
           RunMemories.Num());
}

bool UTrialRunSubsystem::ResumeRun()
{
    if (!IsRunActive() || ActiveTrial->Level.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] ResumeRun: no run with a level to resume — ignored."));
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Resuming trial '%s' -> %s"), *ActiveTrial->Name.ToString(),
           *ActiveTrial->Level.ToString());
    TravelTo(ActiveTrial->Level);
    return true;
}

bool UTrialRunSubsystem::IsHubWorld(const UWorld *World) const
{
    if (!World || HubLevel.IsNull())
    {
        return false;
    }
    const FString WorldPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    return WorldPackage == HubLevel.GetLongPackageName();
}

FString UTrialRunSubsystem::GetTrialRunString() const
{
    const FTrialVariation *Variation = GetChosenVariation();
    TArray<FString> Found;
    for (const TPair<FName, FText> &Entry : FoundFeatures)
    {
        Found.Add(Entry.Value.ToString());
    }
    // B-224b: each fielded enemy's own rank (its HP), beside the fight's rank.
    const FString EnemyRanks = FString::JoinBy(PendingEnemyRanks, TEXT(","), [](EEncounterRank Rank) { return UEnum::GetValueAsString(Rank); });
    return FString::Printf(TEXT("TrialRun: active=%s (seed %d) | variation=%s | difficulty=%s (enemy x%.2f, payout x%.2f) | beaten=%d | found %d [%s] | memories %d | first strike=%s | zone tier %d (%s) | hub=%s | pending %d v %d (%s; per enemy [%s]) | return=%s"),
                           ActiveTrial ? *ActiveTrial->Name.ToString() : TEXT("<none>"), RunSeed,
                           Variation ? *Variation->Name.ToString() : TEXT("<none>"),
                           *TrialGate::GetDifficultyName(TrialDifficulty).ToString(), GetEnemyStrengthMultiplier(),
                           GetPayoutMultiplier(), BeatenEncounters.Num(), Found.Num(),
                           *FString::Join(Found, TEXT("; ")), RunMemories.Num(), *UEnum::GetValueAsString(PendingFirstStrike), PendingZone.Tier,
                           PendingZone.LootPool.IsValid() ? *PendingZone.LootPool.ToString() : TEXT("no loot pool"),
                           HubLevel.IsNull() ? TEXT("<UNSET>") : *HubLevel.ToString(),
                           PendingBattleParty.Num(), PendingOpposingParty.Num(),
                           *UEnum::GetValueAsString(PendingRank), *EnemyRanks,
                           EncounterReturnLevel.IsNull() ? TEXT("<none>") : *EncounterReturnLevel.ToString());
}

void UTrialRunSubsystem::TravelTo(const TSoftObjectPtr<UWorld> &Level)
{
    UWorld *World = GetGameInstance()->GetWorld();
    const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
    if (NetMode == NM_Client)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] A client can't change level (%s); the server travels for everyone"),
               *Level.ToString());
        return;
    }
    if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
    {
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] Server travel -> %s (clients follow)"), *Level.ToString());
        // TODO: B-232 - a controller that survives seamless travel (the hub's) doesn't run BeginPlay again;
        // rebuild its UI / input in NotifyLoadedWorld when hub <-> trial goes seamless (the hub's BP game mode).
        if (!World->ServerTravel(Level.GetLongPackageName()))
        {
            UE_LOG(LogTemp, Error, TEXT("[TrialRun] Server travel to %s refused (in PIE: net.AllowPIESeamlessTravel 1)"),
                   *Level.ToString());
        }
        return;
    }
    UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Level);
}
