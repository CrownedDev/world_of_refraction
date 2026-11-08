// RunRewards.cpp

#include "Trial/RunRewards.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	// TODO: tune (B-073 first guesses). Indexed by EEncounterRank: Normal, MiniBoss, Boss, Elite.
	constexpr int32 WIN_RUN_PRISMS[] = {20, 60, 120, 35};
	constexpr int32 TALLY_HUB_GOLD[] = {10, 40, 100, 15};
	constexpr int32 TALLY_GEAR_ESSENCE[] = {1, 4, 8, 2};
	constexpr int32 TALLY_SKILL_ESSENCE[] = {1, 4, 8, 2};
	constexpr int32 CHEST_HUB_GOLD = 15;
	constexpr int32 CHEST_GEAR_ESSENCE = 2;
	constexpr int32 CHEST_SKILL_ESSENCE = 2;
	/** Indexed by ETrialDifficulty: Easy, Medium, Hard. */
	constexpr float DIFFICULTY_SCALE[] = {0.75f, 1.0f, 1.5f};
	/** The further you got (D-006): beating the run's boss adds this share to the whole tally. */
	constexpr float BOSS_BEATEN_BONUS = 0.25f;

	int32 RankIndex(EEncounterRank Rank)
	{
		const int32 Index = static_cast<int32>(Rank);
		return Index >= 0 && Index < UE_ARRAY_COUNT(WIN_RUN_PRISMS) ? Index : 0;
	}

	float DifficultyScale(ETrialDifficulty Difficulty)
	{
		const int32 Index = static_cast<int32>(Difficulty);
		return Index >= 0 && Index < UE_ARRAY_COUNT(DIFFICULTY_SCALE) ? DIFFICULTY_SCALE[Index] : 1.0f;
	}

	template <typename TPay>
	int32 ForEachPlayerWallet(UWorld *World, TPay &&Pay)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return 0;
		}
		int32 Paid = 0;
		const AGameStateBase *GameState = World->GetGameState();
		if (GameState)
		{
			for (const APlayerState *Player : GameState->PlayerArray)
			{
				const AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
				if (UCurrencyComponent *Wallet = WoRPlayer ? WoRPlayer->GetWallet() : nullptr)
				{
					Pay(*Wallet);
					++Paid;
				}
			}
		}
		if (Paid == 0)
		{
			// No GameState player yet (a headless world): the local player's wallet.
			const APlayerController *PC = World->GetFirstPlayerController();
			const AWoRPlayerState *Local = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
			if (UCurrencyComponent *Wallet = Local ? Local->GetWallet() : nullptr)
			{
				Pay(*Wallet);
				++Paid;
			}
		}
		return Paid;
	}
}

int32 RunRewards::WinRunPrisms(EEncounterRank Rank, ETrialDifficulty Difficulty)
{
	return FMath::FloorToInt(WIN_RUN_PRISMS[RankIndex(Rank)] * DifficultyScale(Difficulty));
}

FRunPayout RunRewards::FightTally(EEncounterRank Rank)
{
	FRunPayout Tally;
	const int32 Index = RankIndex(Rank);
	Tally.HubGold = TALLY_HUB_GOLD[Index];
	Tally.GearEssence = TALLY_GEAR_ESSENCE[Index];
	Tally.SkillEssence = TALLY_SKILL_ESSENCE[Index];
	return Tally;
}

FRunPayout RunRewards::ChestTally()
{
	FRunPayout Tally;
	Tally.HubGold = CHEST_HUB_GOLD;
	Tally.GearEssence = CHEST_GEAR_ESSENCE;
	Tally.SkillEssence = CHEST_SKILL_ESSENCE;
	return Tally;
}

FRunPayout RunRewards::ScalePayout(const FRunPayout &Tally, ETrialDifficulty Difficulty, bool bBossBeaten, float Multiplier)
{
	const float Scale = DifficultyScale(Difficulty) * (bBossBeaten ? 1.0f + BOSS_BEATEN_BONUS : 1.0f) * FMath::Max(Multiplier, 0.0f);
	FRunPayout Paid;
	Paid.HubGold = FMath::FloorToInt(Tally.HubGold * Scale);
	Paid.GearEssence = FMath::FloorToInt(Tally.GearEssence * Scale);
	Paid.SkillEssence = FMath::FloorToInt(Tally.SkillEssence * Scale);
	return Paid;
}

int32 RunRewards::PayWinToPlayers(UWorld *World, int32 Amount)
{
	if (Amount <= 0)
	{
		return 0;
	}
	const int32 Paid = ForEachPlayerWallet(World, [Amount](UCurrencyComponent &Wallet) { Wallet.Add(ECurrencyType::Gold, Amount); });
	UE_LOG(LogWoR, Log, TEXT("[RunRewards] A win pays %d run Prism(s) to %d player(s)"), Amount, Paid);
	return Paid;
}

int32 RunRewards::PayRunEndToPlayers(UWorld *World, const FRunPayout &Payout)
{
	if (Payout.IsEmpty())
	{
		return 0;
	}
	const int32 Paid = ForEachPlayerWallet(World, [&Payout](UCurrencyComponent &Wallet)
	{
		const TPair<ECurrencyType, int32> Parts[] = {{ECurrencyType::Prisms, Payout.HubGold}, // hub Gold (D-159 naming)
													 {ECurrencyType::GearEssence, Payout.GearEssence},
													 {ECurrencyType::SkillEssence, Payout.SkillEssence}};
		for (const TPair<ECurrencyType, int32> &Part : Parts)
		{
			if (Part.Value > 0)
			{
				Wallet.Add(Part.Key, Part.Value);
			}
		}
	});
	UE_LOG(LogWoR, Log, TEXT("[RunRewards] The run ends: %s to each of %d player(s)"), *GetPayoutString(Payout), Paid);
	return Paid;
}

FString RunRewards::GetPayoutString(const FRunPayout &Payout)
{
	return FString::Printf(TEXT("%d Gold, %d Gear Essence, %d Skill Essence"), Payout.HubGold, Payout.GearEssence, Payout.SkillEssence);
}
