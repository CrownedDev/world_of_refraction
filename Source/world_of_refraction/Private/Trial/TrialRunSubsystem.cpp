// TrialRunSubsystem.cpp

#include "Trial/TrialRunSubsystem.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Shop/MerchantShopSubsystem.h"
#include "TimerManager.h"
#include "Trial/TrialData.h"

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

    ActiveTrial = Trial;
    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Entering trial '%s' -> %s"),
           *Trial->Name.ToString(), *Trial->Level.ToString());
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

    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Exiting trial '%s' -> hub %s"),
           ActiveTrial.IsValid() ? *ActiveTrial->Name.ToString() : TEXT("<none>"),
           *HubLevel.ToString());
    ActiveTrial = nullptr;
    UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), HubLevel);
}

// ==================== ENCOUNTER (T-C1) ====================

void UTrialRunSubsystem::EnterEncounter(UTrialData *Trial, const TArray<UCharacterData *> &LocalParty,
                                        const TArray<UCharacterData *> &OpposingParty, EAIDifficulty Difficulty)
{
    if (!Trial || Trial->EncounterLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] EnterEncounter: %s — ignored."),
               Trial ? TEXT("trial has no EncounterLevel authored") : TEXT("null Trial"));
        return;
    }
    if (LocalParty.Num() == 0 || OpposingParty.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialRun] EnterEncounter: empty roster (LocalParty %d, OpposingParty %d) — ignored."),
               LocalParty.Num(), OpposingParty.Num());
        return;
    }

    // Same modal-bracket defense as EnterTrial.
    if (UMerchantShopSubsystem *Shop = GetGameInstance()->GetSubsystem<UMerchantShopSubsystem>())
    {
        Shop->Close();
    }

    PendingLocalParty = TArray<TObjectPtr<UCharacterData>>(LocalParty);
    PendingOpposingParty = TArray<TObjectPtr<UCharacterData>>(OpposingParty);
    PendingDifficulty = Difficulty;
    EncounterReturnLevel = Trial->Level;

    UE_LOG(LogTemp, Log, TEXT("[TrialRun] Entering encounter %d v %d -> %s (return %s)"),
           LocalParty.Num(), OpposingParty.Num(), *Trial->EncounterLevel.ToString(), *EncounterReturnLevel.ToString());
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

    PendingLocalParty.Reset();
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

FString UTrialRunSubsystem::GetTrialRunString() const
{
    return FString::Printf(TEXT("TrialRun: active=%s | hub=%s | pending %d v %d | return=%s"),
                           ActiveTrial.IsValid() ? *ActiveTrial->Name.ToString() : TEXT("<none>"),
                           HubLevel.IsNull() ? TEXT("<UNSET>") : *HubLevel.ToString(),
                           PendingLocalParty.Num(), PendingOpposingParty.Num(),
                           EncounterReturnLevel.IsNull() ? TEXT("<none>") : *EncounterReturnLevel.ToString());
}
