// TrialRunSubsystem.cpp

#include "Trial/TrialRunSubsystem.h"
#include "Trial/TrialMapLook.h"
#include "Player/RunStateComponent.h"
#include "Character/OverCapRules.h"
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
#include "Inventory/InventoryData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Misc/Paths.h"
#include "Merchant/MerchantInteractable.h"
#include "Merchant/MerchantStockRoll.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Shop/MerchantShopSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
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

    /** The local party's members by character key, with their class (Generic when the member has no data). */
    TMap<FName, ECharacterClass> GetLocalPartyClasses(UGameInstance *GI)
    {
        TMap<FName, ECharacterClass> Members;
        const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
        {
            for (const FPartyMember &Member : Party->Members)
            {
                Members.Add(UPoolSubsystem::GetCharacterKey(Member),
                            Member.CharacterData ? Member.CharacterData->CharacterClass : ECharacterClass::Generic);
            }
        }
        Members.Remove(NAME_None);
        return Members;
    }

    /** B-523 (D-224 (2), Q-523): a kit member starts the run with no learned or bought skills. It keeps the skills
     *  its worn record names apart from gear picks (innate, Broken Darkness pools, its own evolution's: they stay with
     *  the kit, WearClassDefaultKit) and any picks the kit authors; the kit's StartingSkills go into the list, not a slot. */
    void BeginKitRunSkills(UPoolSubsystem &Pool, FName Key, const FClassDefaultLoadout &Kit)
    {
        TArray<const UObject *> Keep;
        if (const FWornGear *Worn = Pool.GetWornGear(Key))
        {
            Keep.Append(Worn->InnateSpells);
            Keep.Append(Worn->EvolutionSpells);
            for (const FBDElementSpellPool &BDPool : Worn->BDSpellPools)
            {
                Keep.Append(BDPool.Spells);
            }
        }
        for (const FClassDefaultPiece &Piece : Kit.Pieces)
        {
            Keep.Append(Piece.SkillPicks);
        }
        TArray<const UObject *> Starting;
        Starting.Append(Kit.StartingSkills);
        Pool.BeginRunSkills(Key, Keep, Starting);
    }

    /** B-347 (D-137): opens the run layer and takes the local party's worn loadouts on the run, free. B-512 (D-221): a
     *  member in Kits wears its class kit instead (reset already, ResetClassDefaultKit), after the snapshot. */
    void OpenRunLayerWithWornLoadouts(UGameInstance *GI, UPoolSubsystem &Pool,
                                      const TMap<FName, const FClassDefaultLoadout *> &Kits = {})
    {
        TSet<FName> MemberKeys;
        GetLocalPartyClasses(GI).GetKeys(MemberKeys);
        Pool.BeginRunLayer();
        Pool.SnapshotRunStartGear(MemberKeys); // B-508 (D-219): a quit puts these back
        TSet<FName> HomeWearers = MemberKeys;
        for (const TPair<FName, const FClassDefaultLoadout *> &Kit : Kits)
        {
            for (const FGuid &ID : Pool.WearClassDefaultKit(Kit.Key, *Kit.Value))
            {
                Pool.TakeOnRun(ID); // the kit and the character's own evolution; home gear stays home
            }
            HomeWearers.Remove(Kit.Key);
            BeginKitRunSkills(Pool, Kit.Key, *Kit.Value);
        }
        Pool.TakeHomeGearOnRun(HomeWearers);
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
    BeatenMiniBosses.Reset(); // B-187b
    EncounterRevivals.Reset();
    bHasRestStand = false; // B-187c
    bHasPendingReturnStand = false;
    bRunBossBeaten = false;
    RunWipes = 0; // B-362
    RunMainBossLosses = 0;
    LastRestSpot = NAME_None;
    DroppedPrisms = 0; // B-362b
    RunTally = FRunPayout(); // B-073
    bAtRestSpot = false; // B-363
    bLeavingEarly = false; // B-187
    bAbandoned = false; // B-439
    PayoutMultiplier = RunRewards::BASE_PAYOUT_MULTIPLIER;
    OtherPlayerDrops.Reset(); // B-235
    DropEncounters.Reset();
    DropSpot = FVector::ZeroVector; // B-362c
    bDropHasSpot = false;
    RunLevels.Reset(); // B-223: the run's pillar levels end with it (D-102)
    UnspentLevels.Reset();
    FoundFeatures.Reset();
    RunMemories.Reset();
    NotifyRunProgressChanged(); // B-232d (review): the copy clears too
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingFieldStrike = FFieldStrike(); // B-214f
    PendingZone = FEncounterZoneInfo();
    PendingTierSteps = 0; // B-187b
    PendingEnemyTierSteps.Reset();
    PendingMiniBossSources.Reset();
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
        // B-529: a member who reaches a trial with no worn record (the hub never drew it) is seeded with its starting
        // gear first, so the run-start record is that, not the kit: the kit is per run (D-221) and must not become its
        // first record and block the starting gear for good.
        const UPartySessionSubsystem *Session = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
        if (const UParty *Party = Session ? Session->GetLocalParty() : nullptr)
        {
            for (const FPartyMember &Member : Party->Members)
            {
                const UInventoryData *Start = Member.CharacterData ? Member.CharacterData->Inventory : nullptr;
                if (Member.bRunOnly || !Start)
                {
                    continue;
                }
                // The innate list and pools as a pawn's first draw reads them: the template's active authored loadout.
                FWornGear Innate;
                if (!Start->SavedLoadouts.IsEmpty())
                {
                    const FSavedLoadout &Authored = Start->SavedLoadouts[FMath::Clamp(Start->DefaultActiveLoadoutIndex, 0, Start->SavedLoadouts.Num() - 1)];
                    Innate = FCombatLoadout::CreateFromSavedLoadout(Authored).ToWornGear(Member.CharacterData->CharacterClass);
                }
                Innate.WheelPick.FieldSkill = Member.FieldSkill;
                Innate.WheelPick.FieldAttackIndex = Member.FieldAttackIndex;
                Innate.WheelPick.bPicked = Member.bFieldAttackPicked;
                if (Pool->SeedStartingGear(UPoolSubsystem::GetCharacterKey(Member), Start, Innate))
                {
                    UE_LOG(LogWoRLoadout, Warning, TEXT("[TrialRun] %s entered a trial with no worn record: its starting gear was seeded first"),
                           *Member.CharacterData->GetName());
                }
            }
        }
        // B-512 (D-221): every run starts on the class kit, reset before the layer opens (a crystal it frees is the
        // player's own, not a find). A member with no kit keeps what it wears (FindClassDefaults logs why).
        TMap<FName, const FClassDefaultLoadout *> Kits;
        for (const TPair<FName, ECharacterClass> &Member : GetLocalPartyClasses(GetGameInstance()))
        {
            if (const FClassDefaultLoadout *Kit = FindClassDefaults(Member.Value))
            {
                Pool->ResetClassDefaultKit(Member.Key, *Kit);
                Kits.Add(Member.Key, Kit);
            }
        }
        OpenRunLayerWithWornLoadouts(GetGameInstance(), *Pool, Kits);
        UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
        if (PartySession && !Kits.IsEmpty())
        {
            PartySession->PullWheelPicksFromPool(); // the cleared picks reach the members
        }
    }
}

bool UTrialRunSubsystem::StartRecruitOnKit(const UCharacterData *Data, FName CharacterKey)
{
    UPoolSubsystem *Pool = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
    if (!Data || CharacterKey.IsNone() || !Pool || !Pool->IsRunLayerActive())
    {
        return false;
    }
    const FClassDefaultLoadout *Kit = FindClassDefaults(Data->CharacterClass);
    if (!Kit)
    {
        return false; // FindClassDefaults logs why; the recruit keeps what it wears
    }
    // As a first draw reads it (B-529): the template's active authored loadout gives the innate list and pools.
    const UInventoryData *Start = Data->Inventory;
    if (!Pool->GetWornGear(CharacterKey) && Start && !Start->SavedLoadouts.IsEmpty())
    {
        const FSavedLoadout &Authored = Start->SavedLoadouts[FMath::Clamp(Start->DefaultActiveLoadoutIndex, 0, Start->SavedLoadouts.Num() - 1)];
        Pool->SetWornGear(CharacterKey, FCombatLoadout::CreateFromSavedLoadout(Authored).ToWornGear(Data->CharacterClass));
    }
    Pool->ResetClassDefaultKit(CharacterKey, *Kit);
    for (const FGuid &ID : Pool->WearClassDefaultKit(CharacterKey, *Kit))
    {
        Pool->TakeOnRun(ID);
    }
    // shortcut: its skills are left as today (a run recruit's go to the host when it leaves, D-152); a fresh recruit has
    // no run skills. Snapshot them as BeginKitRunSkills does if a returning character must drop learned ones.
    UE_LOG(LogWoRLoadout, Log, TEXT("[TrialRun] %s joins the run from scratch on its class kit (D-239)"), *Data->GetName());
    return true;
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
    FinishRun();
    TravelTo(HubLevel);
}

void UTrialRunSubsystem::SettleRunAsQuit(bool bTravelIfAway)
{
    if (!IsRunActive())
    {
        return;
    }
    UE_LOG(LogWoR, Warning, TEXT("[TrialRun] The game closed during the run of '%s': settled as a quit (D-172, Q-408); the end would have paid %s, forfeited"),
           ActiveTrial ? *ActiveTrial->Name.ToString() : TEXT("<none>"), *RunRewards::GetPayoutString(GetRunEndPayout()));
    bAbandoned = true;
    FinishRun();
    const UWorld *World = GetGameInstance()->GetWorld();
    if (bTravelIfAway && !IsHubWorld(World) && !HubLevel.IsNull())
    {
        TravelTo(HubLevel); // started somewhere else (a trial map in PIE): on to the hub, as a quit would
    }
}

void UTrialRunSubsystem::FinishRun()
{
    // B-314 (D-111, D-097): the run layer closes while the members are still here to hear it: home pieces go back to
    // their home state, run-only gear leaves with its tier remembered. Before the exit save below.
    // B-508 (D-219): one call per ending; a quit (or a closed game) also puts the run-start gear back.
    if (UPoolSubsystem *Pool = GetGameInstance()->GetSubsystem<UPoolSubsystem>())
    {
        Pool->CloseRun(bAbandoned ? EPoolRunEnding::Quit : IsRunLost() ? EPoolRunEnding::Lost : EPoolRunEnding::Kept);
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
        // B-555 (D-237): a quit (or a closed game) keeps the levels from before the run.
        if (!bAbandoned)
        {
            KeepRunLevels();
        }
        // B-076 (D-026): characters found in the trial leave with it.
        PartySession->DismissRunMembers();
        PartySession->ClearAllMemberVitals();
        if (bAbandoned)
        {
            PartySession->PullWheelPicksFromPool(); // B-508: the members' picks follow their put-back records
        }
    }
    // B-178: the run's Gold is lost when it ends (never banked).
    if (USaveSubsystem *Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
    {
        Save->ClearRunGold(GetGameInstance()->GetWorld());
    }

    // B-073 (D-006): the run's tally is paid now, win or lose, to every player (D-103), at the ending's share (D-189).
    PayRunEnd(GetGameInstance()->GetWorld());

    // B-361b (D-154, Q-361b): a finished co-op run (its boss beaten) pays every player; leaving early pays nothing.
    if (bRunBossBeaten && !bAbandoned) // B-439 (D-176): a run that was quit pays nothing
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
    BeatenMiniBosses.Reset(); // B-187b
    EncounterRevivals.Reset();
    bHasRestStand = false; // B-187c
    bHasPendingReturnStand = false;
    bRunBossBeaten = false;
    RunWipes = 0; // B-362
    RunMainBossLosses = 0;
    LastRestSpot = NAME_None;
    DroppedPrisms = 0; // B-362b
    RunTally = FRunPayout(); // B-073
    bAtRestSpot = false; // B-363
    bLeavingEarly = false; // B-187
    bAbandoned = false; // B-439
    PayoutMultiplier = RunRewards::BASE_PAYOUT_MULTIPLIER;
    OtherPlayerDrops.Reset(); // B-235
    DropEncounters.Reset();
    DropSpot = FVector::ZeroVector; // B-362c
    bDropHasSpot = false;
    RunLevels.Reset(); // B-223: the run's pillar levels end with it (D-102)
    UnspentLevels.Reset();
    FoundFeatures.Reset();
    RunMemories.Reset();
    NotifyRunProgressChanged(); // B-232d (review): the copy clears too
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingFieldStrike = FFieldStrike(); // B-214f
    PendingZone = FEncounterZoneInfo();
    PendingTierSteps = 0; // B-187b
    PendingEnemyTierSteps.Reset();
    PendingMiniBossSources.Reset();
    // B-362: a run ended by losses leaves from the battle level, with its fight's stash still set; the hub has no fight.
    PendingBattleParty.Reset();
    PendingOpposingParty.Reset();
    EncounterReturnLevel = nullptr;
    // B-090: a trial's exit is a save point.
    USaveSubsystem *ExitSave = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    ExitSave->SaveNow(GetGameInstance()->GetWorld(), true);
    // B-234d (Q-234d): a run handed over from the old host's copy ends here; the hub gets this player's own party and pool.
    ExitSave->EndHandedOverRun();
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
    bAtRestSpot = false; // B-363: a fight leaves the rest spot
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
    PendingFieldStrike = FFieldStrike(); // B-214f
    PendingZone = FEncounterZoneInfo();
    PendingTierSteps = 0; // B-187b
    PendingEnemyTierSteps.Reset();
    PendingMiniBossSources.Reset();
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

int32 UTrialRunSubsystem::GetRunOverCap(FName CharacterKey, EWorldPillar Pillar) const
{
    const FRunLevels *Levels = RunLevels.Find(CharacterKey);
    return Levels ? Levels->GetOver(Pillar) : 0;
}

bool UTrialRunSubsystem::AddRunOverCap(FName CharacterKey, EWorldPillar Pillar)
{
    if (CharacterKey.IsNone() || GetRunOverCap(CharacterKey, Pillar) >= OverCapRules::MAX_PER_PILLAR)
    {
        return false;
    }
    ++RunLevels.FindOrAdd(CharacterKey).OverFor(Pillar);
    return true;
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
    NotifyRunProgressChanged();
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
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] %s: +%d pillar level(s) to place at a rest spot (%d waiting): wor.Trial.Rest, Level up"),
               *FPaths::GetBaseFilename(CharacterKey.ToString()), Count, Waiting);
        NotifyRunProgressChanged();
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
    NotifyRunProgressChanged();
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

void UTrialRunSubsystem::KeepRunLevels()
{
    UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
    // TODO(D-103 co-op): only this machine's party keeps its levels; a guest's members keep theirs in stage 5.
    UParty *Party = IsRunActive() && PartySession ? PartySession->GetLocalParty() : nullptr;
    if (!Party)
    {
        return;
    }
    for (FPartyMember &Member : Party->Members)
    {
        if (Member.bRunOnly || !Member.CharacterData)
        {
            continue;
        }
        const FRunLevels *Levels = RunLevels.Find(UPoolSubsystem::GetCharacterKey(Member));
        Member.KeptMind = Levels ? Levels->Mind : 0;
        Member.KeptBody = Levels ? Levels->Body : 0;
        Member.KeptSpirit = Levels ? Levels->Spirit : 0;
        Member.bHasKeptLevels = true;
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] %s keeps Mind / Body / Spirit +%d / +%d / +%d (D-237)"), *Member.CharacterData->GetName(),
               Member.KeptMind, Member.KeptBody, Member.KeptSpirit);
    }
}

int32 UTrialRunSubsystem::TakeWipePoints(FName CharacterKey, int32 Points, FRandomStream &Stream)
{
    FRunLevels *Levels = RunLevels.Find(CharacterKey);
    int32 Taken = 0;
    while (Levels && Taken < Points)
    {
        TArray<EWorldPillar, TInlineAllocator<3>> Losable;
        for (const EWorldPillar Pillar : {EWorldPillar::Mind, EWorldPillar::Body, EWorldPillar::Spirit})
        {
            if (Levels->Get(Pillar) > 0 || Levels->GetOver(Pillar) > 0)
            {
                Losable.Add(Pillar);
            }
        }
        if (Losable.IsEmpty())
        {
            break;
        }
        const EWorldPillar Pillar = Losable[Stream.RandHelper(Losable.Num())];
        int32 &Over = Levels->OverFor(Pillar);
        if (Over > 0)
        {
            --Over; // D-237 default: a pillar's over-cap points go first
        }
        else
        {
            --Levels->For(Pillar);
        }
        ++Taken;
    }
    UE_CLOG(Taken > 0, LogTemp, Log, TEXT("[TrialRun] Wipe: %s lost %d run point(s)"), *CharacterKey.ToString(), Taken);
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
    if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
    {
        return;
    }
    // B-189: the trial's element tailors its map's look (sky, fog, sun). Cosmetic, so on every machine that knows
    // the trial (a client's run copy is B-234).
    if (IsRunActive() && ActiveTrial && IsActiveTrialWorld(World))
    {
        TrialMapLook::ApplyTo(World, ActiveTrial->Element);
    }
    // The server places it (a client's map follows the server's; co-op per-player pickups are B-235).
    if (World->GetNetMode() == NM_Client)
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
        Out += FString::Printf(TEXT("\n  %s: Mind +%d, Body +%d, Spirit +%d; over-cap %d/%d/%d; bought %d; %d to place"),
                               *FPaths::GetBaseFilename(Pair.Key.ToString()), Pair.Value.Mind, Pair.Value.Body, Pair.Value.Spirit,
                               Pair.Value.OverMind, Pair.Value.OverBody, Pair.Value.OverSpirit, Pair.Value.Purchases, GetUnspentLevels(Pair.Key));
    }
    for (const TPair<FName, int32> &Pair : UnspentLevels)
    {
        if (!RunLevels.Contains(Pair.Key))
        {
            Out += FString::Printf(TEXT("\n  %s: %d to place"), *FPaths::GetBaseFilename(Pair.Key.ToString()), Pair.Value);
        }
    }
    // B-555 (D-237): what each member keeps in the hub from its last run.
    const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
    if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
    {
        for (const FPartyMember &Member : Party->Members)
        {
            Out += FString::Printf(TEXT("\n  kept by %s: Mind +%d, Body +%d, Spirit +%d%s"), *GetNameSafe(Member.CharacterData.Get()),
                                   Member.KeptMind, Member.KeptBody, Member.KeptSpirit, Member.bHasKeptLevels ? TEXT("") : TEXT(" (no run ended yet)"));
        }
    }
    return Out;
}

void UTrialRunSubsystem::MarkPendingEncounterBeaten()
{
    for (const FName EncounterId : PendingEncounterSources)
    {
        // Review: the team's own rank, not the fight's (an elite caught in a mini-boss fight leaves no rest spot).
        MarkEncounterBeaten(EncounterId, PendingMiniBossSources.Contains(EncounterId) ? EEncounterRank::MiniBoss : EEncounterRank::Normal);
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
    NotifyRunProgressChanged();
    return true;
}

void UTrialRunSubsystem::RecordFound(FName FeatureId, const FText &What)
{
    FoundFeatures.Add(FeatureId, What);
    UE_LOG(LogWoR, Log, TEXT("[TrialRun] Found %s: %s (%d found this run)"), *FeatureId.ToString(),
           *What.ToString(), FoundFeatures.Num());
    NotifyRunProgressChanged();
}

void UTrialRunSubsystem::NotifyRunProgressChanged() const
{
    if (URunStateComponent *RunState = URunStateComponent::Find(GetGameInstance()))
    {
        RunState->PullRunProgress(); // B-232d: clients read found features, memories and waiting levels from it
    }
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
    // B-187c: USaveSubsystem::RestoreRun puts the saved marks and revivals back after this (RestoreMiniBossRevivals).
    BeatenMiniBosses.Reset();
    EncounterRevivals.Reset();
    bHasRestStand = false;
    bHasPendingReturnStand = false;
    FoundFeatures = Found;
    RunMemories = TArray<TObjectPtr<UMemoryData>>(Memories);
    NotifyRunProgressChanged();
    PendingEncounterSources.Reset();
    PendingEnemyRanks.Reset();
    PendingFirstStrike = EFirstStrike::None;
    PendingFieldStrike = FFieldStrike(); // B-214f
    PendingZone = FEncounterZoneInfo();
    PendingTierSteps = 0; // B-187b
    PendingEnemyTierSteps.Reset();
    PendingMiniBossSources.Reset();
    bAtRestSpot = false; // B-363 review: a resumed run starts away from a rest spot
    // B-073 desktop review: nothing of an earlier run's payout lingers (the save restores the tally and the boss mark).
    bRunBossBeaten = false;
    RunTally = FRunPayout();
    PayoutMultiplier = RunRewards::BASE_PAYOUT_MULTIPLIER;
    // B-314: a resumed run opens a fresh layer; B-314b: USaveSubsystem::RestoreRun then replaces it with the save's own
    // layer when the save has one (the run-only marks and home states as saved).
    UPoolSubsystem *Pool = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
    if (Pool && IsRunActive())
    {
        UE_LOG(LogTemp, Log, TEXT("[TrialRun] Resumed run: a fresh run layer (the save's own layer replaces it when it has one, B-314b)"));
        Pool->DiscardRunLayer(); // the store was reloaded: an open layer's home states are stale
        OpenRunLayerWithWornLoadouts(GetGameInstance(), *Pool); // B-512: no kit here: the saved run already wears it
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

void UTrialRunSubsystem::MarkEncounterBeaten(FName EncounterId, EEncounterRank Rank)
{
    if (EncounterId.IsNone())
    {
        return;
    }
    BeatenEncounters.Add(EncounterId);
    if (Rank == EEncounterRank::MiniBoss)
    {
        BeatenMiniBosses.Add(EncounterId); // B-187b: a rest spot where it stood
    }
}

void UTrialRunSubsystem::SetPendingEnemyTierSteps(const TArray<int32> &Steps)
{
    PendingEnemyTierSteps = Steps;
    PendingTierSteps = 0;
    for (const int32 Step : Steps)
    {
        PendingTierSteps = FMath::Max(PendingTierSteps, Step);
    }
}

bool UTrialRunSubsystem::CanReviveEncounter(FName EncounterId) const
{
    return IsRunActive() && IsEncounterBeaten(EncounterId) && IsBeatenMiniBoss(EncounterId) &&
           GetEncounterRevivals(EncounterId) < MiniBossRevival::MAX_REVIVALS;
}

bool UTrialRunSubsystem::ReviveEncounter(FName EncounterId)
{
    if (!CanReviveEncounter(EncounterId))
    {
        return false;
    }
    BeatenEncounters.Remove(EncounterId);
    BeatenMiniBosses.Remove(EncounterId);
    ++EncounterRevivals.FindOrAdd(EncounterId);
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Revived mini-boss %s (revival %d of %d)"), *EncounterId.ToString(),
           GetEncounterRevivals(EncounterId), MiniBossRevival::MAX_REVIVALS);
    return true;
}

void UTrialRunSubsystem::RestoreMiniBossRevivals(const TArray<FName> &MiniBosses, const TMap<FName, int32> &Revivals)
{
    BeatenMiniBosses.Reset();
    for (const FName Id : MiniBosses)
    {
        if (IsEncounterBeaten(Id))
        {
            BeatenMiniBosses.Add(Id);
        }
    }
    EncounterRevivals = Revivals;
}

bool UTrialRunSubsystem::ConsumePendingReturnStand(FTransform &OutStand)
{
    if (!bHasPendingReturnStand)
    {
        return false;
    }
    OutStand = PendingReturnStand;
    bHasPendingReturnStand = false;
    return true;
}

void UTrialRunSubsystem::ReloadTrialLevel()
{
    if (ActiveTrial && !ActiveTrial->Level.IsNull())
    {
        TravelTo(ActiveTrial->Level);
    }
}

FString UTrialRunSubsystem::GetRevivalsString() const
{
    FString Out = FString::Printf(TEXT("[TrialRun] Mini-boss rest spots (B-187b): %d beaten, max %d revivals each"), BeatenMiniBosses.Num(),
                                  MiniBossRevival::MAX_REVIVALS);
    for (const FName Id : BeatenMiniBosses)
    {
        Out += FString::Printf(TEXT("\n  %s: beaten, %d revival(s)%s"), *FPaths::GetBaseFilename(Id.ToString()), GetEncounterRevivals(Id),
                               CanReviveEncounter(Id) ? TEXT(", can be revived") : TEXT(""));
    }
    for (const TPair<FName, int32> &Pair : EncounterRevivals)
    {
        if (!BeatenMiniBosses.Contains(Pair.Key))
        {
            Out += FString::Printf(TEXT("\n  %s: revived %d time(s), waiting on the map"), *FPaths::GetBaseFilename(Pair.Key.ToString()), Pair.Value);
        }
    }
    // B-187c review: where the party rested, and where the next load puts it.
    Out += bHasRestStand ? FString::Printf(TEXT("\n  rest stand %s"), *RestStand.GetLocation().ToCompactString()) : FString(TEXT("\n  no rest stand yet"));
    Out += bHasPendingReturnStand ? FString::Printf(TEXT("; pending return %s"), *PendingReturnStand.GetLocation().ToCompactString())
                                  : FString(TEXT("; no pending return"));
    return Out;
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
            // B-234d review: a handed-over run's return waits for this travel's hub; it won't come.
            if (USaveSubsystem *Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
            {
                Save->CancelHandedOverRunEnd();
            }
        }
        return;
    }
    UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Level);
}

void UTrialRunSubsystem::RecordFightReward(UWorld *World, EEncounterRank Rank)
{
    if (!IsRunActive())
    {
        return; // a fight outside a run pays nothing
    }
    RunRewards::PayWinToPlayers(World, RunRewards::WinRunPrisms(Rank, TrialDifficulty));
    RunTally += RunRewards::FightTally(Rank);
    UE_LOG(LogWoR, Log, TEXT("[TrialRun] Run tally now %s"), *RunRewards::GetPayoutString(RunTally));
}

void UTrialRunSubsystem::RecordChestReward()
{
    if (IsRunActive())
    {
        RunTally += RunRewards::ChestTally();
    }
}

void UTrialRunSubsystem::LeaveRunEarly()
{
    // Review fix: once only; the share is applied in GetRunEndPayout, so a second press can't cut it again.
    if (!IsRunActive() || bLeavingEarly)
    {
        return;
    }
    if (HubLevel.IsNull())
    {
        // Desktop review: the run can't end without a hub, so its payout isn't cut either.
        UE_LOG(LogWoR, Warning, TEXT("[TrialRun] Can't leave the run: no HubLevel (DefaultGame.ini)"));
        return;
    }
    bLeavingEarly = true;
    UE_LOG(LogWoR, Log, TEXT("[TrialRun] Leaving the run at a rest spot: the end pays %s"), *RunRewards::GetPayoutString(GetRunEndPayout()));
    ExitTrialNextTick();
}

void UTrialRunSubsystem::AbandonRun()
{
    if (!IsRunActive() || bAbandoned)
    {
        return;
    }
    const UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (World && World->GetNetMode() == NM_Client)
    {
        UE_LOG(LogWoR, Warning, TEXT("[TrialRun] Only the host can quit the run (a guest leaves from the rest menu)"));
        return;
    }
    if (HubLevel.IsNull())
    {
        // As LeaveRunEarly: the run can't end without a hub, so nothing is taken either.
        UE_LOG(LogWoR, Warning, TEXT("[TrialRun] Can't quit the run: no HubLevel (DefaultGame.ini)"));
        return;
    }
    UE_LOG(LogWoR, Log, TEXT("[TrialRun] The run is quit: %s is lost, the end pays nothing (D-176)"), *RunRewards::GetPayoutString(GetRunEndPayout()));
    bAbandoned = true;
    ExitTrialNextTick();
}

int32 UTrialRunSubsystem::PayRunEnd(UWorld *World)
{
    const int32 Paid = RunRewards::PayRunEndToPlayers(World, GetRunEndPayout());
    RunTally = FRunPayout();
    return Paid;
}

float UTrialRunSubsystem::RefreshPayoutMultiplier(const TArray<AActor *> &Fighters)
{
    if (!IsRunActive())
    {
        return PayoutMultiplier;
    }
    float BonusPercent = 0.0f;
    for (const AActor *Fighter : Fighters)
    {
        if (const ULoadoutComponent *Loadout = Fighter ? Fighter->FindComponentByClass<ULoadoutComponent>() : nullptr)
        {
            BonusPercent += RunRewards::SumPayoutBonusPercent(Loadout->GatherAttachedEvolutions());
        }
    }
    const float Previous = PayoutMultiplier;
    SetPayoutMultiplier(RunRewards::PayoutMultiplierForBonus(BonusPercent));
    if (!FMath::IsNearlyEqual(Previous, PayoutMultiplier))
    {
        UE_LOG(LogWoR, Log, TEXT("[TrialRun] Payout passives: +%.0f%% -> run-end multiplier x%.2f"), BonusPercent, PayoutMultiplier);
    }
    return PayoutMultiplier;
}

bool UTrialRunSubsystem::IsRunWipedOut() const
{
    return DefeatRules::RunEnds(RunWipes, RunMainBossLosses);
}

FRunPayout UTrialRunSubsystem::GetRunEndPayout() const
{
    if (bAbandoned)
    {
        return FRunPayout(); // B-439 (D-176): quitting loses everything
    }
    if (IsRunLost())
    {
        // B-451 (D-189): out of wipes: a quarter of the run's currencies, no boss bonus.
        return RunRewards::ScalePayout(RunTally, TrialDifficulty, false, PayoutMultiplier * RunRewards::WIPED_OUT_SHARE);
    }
    // B-187 (Q-187): leaving at a rest spot pays the leave share, with no boss bonus (full rewards only for beating
    // the boss or falling in battle).
    const float Multiplier = bLeavingEarly ? PayoutMultiplier * RunRewards::LEAVE_EARLY_SHARE : PayoutMultiplier;
    return RunRewards::ScalePayout(RunTally, TrialDifficulty, bRunBossBeaten && !bLeavingEarly, Multiplier);
}

const FClassDefaultLoadout *UTrialRunSubsystem::FindClassDefaults(ECharacterClass Class) const
{
    const UClassDefaultLoadoutData *Data = ClassDefaultLoadouts.IsNull() ? nullptr : ClassDefaultLoadouts.LoadSynchronous();
    if (!Data)
    {
        UE_LOG(LogWoRLoadout, Warning, TEXT("[TrialRun] No class default kits (ClassDefaultLoadouts '%s' in DefaultGame.ini): the trial keeps what is worn"),
               *ClassDefaultLoadouts.ToString());
        return nullptr;
    }
    const FClassDefaultLoadout *Kit = Data->Defaults.Find(Class);
    if (!Kit)
    {
        UE_LOG(LogWoRLoadout, Warning, TEXT("[TrialRun] %s has no %s kit: the trial keeps what is worn"), *Data->GetName(),
               *UEnum::GetDisplayValueAsText(Class).ToString());
    }
    return Kit;
}

FString UTrialRunSubsystem::GetClassDefaultsString() const
{
    const UClassDefaultLoadoutData *Data = ClassDefaultLoadouts.IsNull() ? nullptr : ClassDefaultLoadouts.LoadSynchronous();
    const UPoolSubsystem *Pool = GetGameInstance()->GetSubsystem<UPoolSubsystem>();
    return FString::Printf(TEXT("%s\n%s"), Data ? *Data->GetDefaultsString() : *FString::Printf(TEXT("[ClassDefaults] no asset at '%s'"), *ClassDefaultLoadouts.ToString()),
                           Pool ? *Pool->GetClassDefaultKitsString() : TEXT("[ClassDefaults] no pool"));
}

static FAutoConsoleCommandWithWorld GTrialPrintClassDefaultsCommand(
    TEXT("wor.Trial.PrintClassDefaults"),
    TEXT("B-511 (D-221): the class default kits (DA_Loadout_ClassDefaults) and every character's minted kit pieces."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
        UE_LOG(LogWoRLoadout, Display, TEXT("%s"), Run ? *Run->GetClassDefaultsString() : TEXT("[ClassDefaults] no run subsystem"));
    }));

static FAutoConsoleCommandWithWorld GTrialPrintRevivalsCommand(
    TEXT("wor.Trial.PrintRevivals"),
    TEXT("B-187b: the beaten mini-bosses (each a rest spot), their revivals and whether they can be revived."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
        UE_LOG(LogWoR, Display, TEXT("%s"), Run ? *Run->GetRevivalsString() : TEXT("[TrialRun] no run subsystem"));
    }));

static FAutoConsoleCommandWithWorld GTrialPrintRewardsCommand(
    TEXT("wor.Trial.PrintRewards"),
    TEXT("B-073: the run's tally, what its end would pay now, and the payout multiplier."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
        if (!Run)
        {
            return;
        }
        UE_LOG(LogWoR, Display, TEXT("[RunRewards] run %s; tally %s; the end would pay %s (x%.2f)"), Run->IsRunActive() ? TEXT("on") : TEXT("off"),
               *RunRewards::GetPayoutString(Run->GetRunTally()), *RunRewards::GetPayoutString(Run->GetRunEndPayout()),
               Run->GetRunPayoutMultiplier());
    }));
