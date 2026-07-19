// TrialRunSubsystem.cpp

#include "Trial/TrialRunSubsystem.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Shop/MerchantShopSubsystem.h"
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

FString UTrialRunSubsystem::GetTrialRunString() const
{
    return FString::Printf(TEXT("TrialRun: active=%s | hub=%s"),
                           ActiveTrial.IsValid() ? *ActiveTrial->Name.ToString() : TEXT("<none>"),
                           HubLevel.IsNull() ? TEXT("<UNSET>") : *HubLevel.ToString());
}
