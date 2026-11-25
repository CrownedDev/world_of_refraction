// MaxResourcesDebug.cpp

#include "Debug/Currency/MaxResourcesDebug.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyTypes.h"
#include "Currency/CurrencyWallet.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Save/SaveSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

namespace
{
	constexpr float MAX_RESOURCES_SCREEN_SECONDS = 6.0f;

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorld GMaxResourcesCmd(
		TEXT("wor.Debug.MaxResources"),
		TEXT("DEBUG (B-623): tops every currency of the played party's wallet (Gold, Prisms, Diamond, Dormant, Elemental and "
			 "Pillar Essence, Mentor Tokens) up to 9999, then saves."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
				UCurrencyComponent *Wallet = CurrencyWallet::FindFor(PC ? PC->GetPawn() : nullptr);
				if (!Wallet)
				{
					UE_LOG(LogWoR, Warning, TEXT("[MaxResources] wor.Debug.MaxResources: no played character with a wallet."));
					return;
				}
				if (!Wallet->GetOwner() || !Wallet->GetOwner()->HasAuthority())
				{
					UE_LOG(LogWoR, Warning, TEXT("[MaxResources] wor.Debug.MaxResources: the host's command (a guest's wallet is the host's to change)."));
					return;
				}
				const TArray<FString> Raised = WoRMaxResources::TopUp(*Wallet);
				const UGameInstance *GI = World->GetGameInstance();
				USaveSubsystem *Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
				const bool bSaved = Raised.Num() > 0 && Save && Save->SaveNow(World, /*bAsync=*/false);
				const FString Report = Raised.Num() > 0
										   ? FString::Printf(TEXT("MaxResources: %s (%s; run money is banked only in a trial)"), *FString::Join(Raised, TEXT(", ")),
															 bSaved ? TEXT("saved") : TEXT("NOT saved"))
										   : FString::Printf(TEXT("MaxResources: every balance is %d or more already"), WoRMaxResources::TEST_BALANCE);
				UE_LOG(LogWoR, Display, TEXT("[MaxResources] %s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, MAX_RESOURCES_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
#endif // !UE_BUILD_SHIPPING
}

TArray<FString> WoRMaxResources::TopUp(UCurrencyComponent &Wallet, int32 Target)
{
	TArray<FString> Lines;
	for (const ECurrencyType Currency : CurrencyBalances::ALL)
	{
		const int32 Before = Wallet.GetBalance(Currency);
		if (Before < Target && Wallet.Add(Currency, Target - Before))
		{
			// The name the player sees (Gold and Prisms are shown swapped, WoRGameNames).
			Lines.Add(FString::Printf(TEXT("%s %d -> %d"), *WoRGameNames::GetCurrencyName(Currency).ToString(), Before, Wallet.GetBalance(Currency)));
		}
	}
	return Lines;
}
