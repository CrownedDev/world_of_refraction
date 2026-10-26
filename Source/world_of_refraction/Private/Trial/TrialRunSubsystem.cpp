// TrialRunSubsystem.cpp

#include "Trial/TrialRunSubsystem.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Merchant/MerchantStockRoll.h"
#include "Party/PartySessionSubsystem.h"
#include "Shop/MerchantShopSubsystem.h"
#include "TimerManager.h"
#include "Trial/TrialData.h"

namespace
{
    TAutoConsoleVariable<int32> CVarTrialSeed(
        TEXT("wor.Trial.Seed"),
        0,
        TEXT("Seed for every following trial run (0 = a fresh seed each run). Trial vendors roll their stock from it. ")
        TEXT("Copy the seed from the [TrialRun] log line to see the same stock again."));

    /** 0 means "no run", so a generated seed of 0 is bumped to this. */
    constexpr int32 TRIAL_NONZERO_SEED_FALLBACK = 1;

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

    ActiveTrial = Trial;
    const int32 ForcedSeed = CVarTrialSeed.GetValueOnGameThread();
    RunSeed = ForcedSeed != 0 ? ForcedSeed : MakeFreshRunSeed();
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Entering trial '%s' -> %s (run seed %d; replay with: wor.Trial.Seed %d)"),
           *Trial->Name.ToString(), *Trial->Level.ToString(), RunSeed, RunSeed);
    UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Trial->Level);
}

void UTrialRunSubsystem::ExitTrial()
{
    if (HubLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[TrialRun] ExitTrial: HubLevel not configured (DefaultGame.ini) — ignored."));
        return;
    }

    // Hub return is the factory reset: drop every slot back to the sentinel so
    // the next EnterTrial seeds fresh rather than inheriting the last run's
    // damage. Pairs with SetAllMembersFull above.
    if (UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>())
    {
        PartySession->ClearAllMemberVitals();
    }

    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Exiting trial '%s' -> hub %s"),
           ActiveTrial.IsValid() ? *ActiveTrial->Name.ToString() : TEXT("<none>"),
           *HubLevel.ToString());
    ActiveTrial = nullptr;
    RunSeed = 0;
    UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), HubLevel);
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

    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Entering encounter %d v %d -> %s (return %s)"),
           BattleParty.Num(), OpposingParty.Num(), *Trial->EncounterLevel.ToString(), *EncounterReturnLevel.ToString());
    UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Trial->EncounterLevel);
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
                                         { UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), ReturnLevel); }));
}

TArray<FMerchantStockEntry> UTrialRunSubsystem::GetMerchantStock(const UMerchantData *Merchant) const
{
    return Merchant ? MerchantStockRoll::StockFor(*Merchant, ActiveTrial.IsValid(), RunSeed) : TArray<FMerchantStockEntry>();
}

FString UTrialRunSubsystem::GetTrialRunString() const
{
    return FString::Printf(TEXT("TrialRun: active=%s (seed %d) | hub=%s | pending %d v %d (%s) | return=%s"),
                           ActiveTrial.IsValid() ? *ActiveTrial->Name.ToString() : TEXT("<none>"), RunSeed,
                           HubLevel.IsNull() ? TEXT("<UNSET>") : *HubLevel.ToString(),
                           PendingBattleParty.Num(), PendingOpposingParty.Num(),
                           *UEnum::GetValueAsString(PendingRank),
                           EncounterReturnLevel.IsNull() ? TEXT("<none>") : *EncounterReturnLevel.ToString());
}
