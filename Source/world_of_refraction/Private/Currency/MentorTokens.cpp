// MentorTokens.cpp

#include "Currency/MentorTokens.h"

#include "Currency/CurrencyComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRPlayerState.h"
#include "Skills/Definitions/SkillDataBase.h"

namespace
{
	/** wor.Mentor.Print: each player's Mentor Tokens. */
	FAutoConsoleCommandWithWorld GMentorPrintCmd(
		TEXT("wor.Mentor.Print"),
		TEXT("DEBUG: each player's Mentor Tokens (B-361b, D-154)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *WoRMentorTokens::GetStateString(World));
		}));
}

namespace WoRMentorTokens
{
	int32 GetTeachCost(const UObject *Skill)
	{
		const USkillDataBase *Data = Cast<USkillDataBase>(Skill);
		return Data ? MentorTokenRules::TEACH_COST_AT_F + static_cast<int32>(Data->Tier) * MentorTokenRules::TEACH_COST_PER_TIER : 0;
	}

	int32 CountPlayers(const UWorld *World)
	{
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return 0;
		}
		int32 Players = 0;
		for (const APlayerState *Player : GameState->PlayerArray)
		{
			Players += Cast<AWoRPlayerState>(Player) ? 1 : 0;
		}
		return Players;
	}

	UCurrencyComponent *WalletOf(const APlayerState *Player)
	{
		const AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
		return WoRPlayer ? WoRPlayer->GetWallet() : nullptr;
	}

	int32 PayPlayersTogether(UWorld *World, int32 Amount, const TCHAR *Why)
	{
		const int32 Players = CountPlayers(World);
		if (Amount <= 0 || Players < MentorTokenRules::MIN_PLAYERS_TOGETHER || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTemp, Log, TEXT("[MentorTokens] %s: no tokens (%d player(s) here)"), Why, Players);
			return 0;
		}
		int32 Paid = 0;
		for (const APlayerState *Player : World->GetGameState()->PlayerArray)
		{
			if (UCurrencyComponent *Wallet = WalletOf(Player))
			{
				Wallet->Add(ECurrencyType::MentorTokens, Amount);
				++Paid;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[MentorTokens] %s: %d token(s) to each of %d player(s)"), Why, Amount, Paid);
		return Paid;
	}

	FString GetStateString(const UWorld *World)
	{
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return TEXT("[MentorTokens] no game state");
		}
		FString Out = FString::Printf(TEXT("[MentorTokens] %d player(s):"), CountPlayers(World));
		for (const APlayerState *Player : GameState->PlayerArray)
		{
			if (const UCurrencyComponent *Wallet = WalletOf(Player))
			{
				Out += FString::Printf(TEXT("\n  %s: %d"), *Player->GetPlayerName(), Wallet->GetBalance(ECurrencyType::MentorTokens));
			}
		}
		return Out;
	}
}
