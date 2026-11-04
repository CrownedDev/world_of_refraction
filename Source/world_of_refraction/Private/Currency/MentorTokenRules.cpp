// MentorTokenRules.cpp

#include "Currency/MentorTokenRules.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Player/WoRPlayerState.h"

namespace
{
	/** The wallets of the players in the world, in join order. */
	TArray<UCurrencyComponent *> PlayerWallets(const UWorld *World)
	{
		TArray<UCurrencyComponent *> Wallets;
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return Wallets;
		}
		for (const APlayerState *PS : GameState->PlayerArray)
		{
			const AWoRPlayerState *WoRPS = Cast<AWoRPlayerState>(PS);
			if (UCurrencyComponent *Wallet = WoRPS ? WoRPS->GetWallet() : nullptr)
			{
				Wallets.Add(Wallet);
			}
		}
		return Wallets;
	}
}

namespace WoRMentor
{
	int32 PayEachPlayer(UWorld *World, int32 Amount)
	{
		if (!World || World->GetNetMode() == NM_Client || Amount <= 0)
		{
			return 0;
		}
		int32 Paid = 0;
		for (UCurrencyComponent *Wallet : PlayerWallets(World))
		{
			if (Wallet->Add(ECurrencyType::MentorTokens, Amount))
			{
				++Paid;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Mentor] %d token(s) each to %d player(s)"), Amount, Paid);
		return Paid;
	}

	int32 PayRunEnd(UWorld *World)
	{
		return PayEachPlayer(World, MentorTokenRules::GetRunPayout(PlayerWallets(World).Num()));
	}

	int32 PayBossBeaten(UWorld *World)
	{
		return PayEachPlayer(World, MentorTokenRules::GetBossPayout(PlayerWallets(World).Num()));
	}

	FString GetTokensString(const UWorld *World)
	{
		FString Out = TEXT("[Mentor] tokens:");
		for (const UCurrencyComponent *Wallet : PlayerWallets(World))
		{
			Out += FString::Printf(TEXT(" %s=%d"), *GetNameSafe(Wallet->GetOwner()), Wallet->GetMentorTokens());
		}
		return Out;
	}
}
