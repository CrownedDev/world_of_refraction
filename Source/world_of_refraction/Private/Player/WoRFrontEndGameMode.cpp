// WoRFrontEndGameMode.cpp

#include "Player/WoRFrontEndGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Trial/TrialRunSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "WoRLog.h"

AWoRFrontEndGameMode::AWoRFrontEndGameMode()
{
    DefaultPawnClass = nullptr;
}

bool AWoRFrontEndGameMode::PlayerCanRestart_Implementation(APlayerController * /*Player*/)
{
    return false;
}

bool WoRFrontEnd::HasFrontEnd(const FString &DefaultMapPackage, const FString &HubMapPackage)
{
    return !DefaultMapPackage.IsEmpty() && DefaultMapPackage != HubMapPackage;
}

bool WoRFrontEnd::ShouldFallBackToFrontEnd(bool bHasSave, bool bHasLeader, bool bHasFrontEnd, bool bAutomationTesting)
{
    return !bHasSave && !bHasLeader && bHasFrontEnd && !bAutomationTesting;
}

WoRFrontEnd::EQuitTo WoRFrontEnd::GetQuitTarget(bool bInRun, bool bHasFrontEnd)
{
    return bInRun ? EQuitTo::Hub : bHasFrontEnd ? EQuitTo::FrontEnd : EQuitTo::Desktop;
}

FString WoRFrontEnd::GetDefaultMapPackage()
{
    FString DefaultMap;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), DefaultMap, GEngineIni);
    return DefaultMap.IsEmpty() ? FString() : FSoftObjectPath(DefaultMap).GetLongPackageName();
}

bool WoRFrontEnd::HasFrontEnd(const UGameInstance *GameInstance)
{
    const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
    return TrialRun && TrialRun->HasHubLevel() && HasFrontEnd(GetDefaultMapPackage(), TrialRun->GetHubLevelPackage());
}

bool WoRFrontEnd::IsFrontEndWorld(const UWorld *World)
{
    return World && World->GetAuthGameMode<AWoRFrontEndGameMode>() != nullptr;
}

bool WoRFrontEnd::TravelToFrontEnd(UWorld *World)
{
    if (!World || !HasFrontEnd(World->GetGameInstance()))
    {
        return false;
    }
    const FString Map = GetDefaultMapPackage();
    UE_LOG(LogWoR, Log, TEXT("[FrontEnd] Travel to the menu level %s"), *Map);
    UGameplayStatics::OpenLevel(World, FName(*Map));
    return true;
}
