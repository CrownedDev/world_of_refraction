// RunRewards.h
// B-073 (D-006, D-048, D-094, D-159, Q-073): what a run pays.
// - Each win pays run Prisms (ECurrencyType::Gold, shown as Prisms since D-159; run-only, never banked) at once, by the
//   fight's rank and the trial's difficulty; it is what in-run shops and the loadout vendor charge (D-048).
// - Each win (and each chest) also adds to the run's tally: hub Gold (ECurrencyType::Prisms, shown as Gold) and the
//   upgrade materials (Gear and Skill Essence, Q-073). The tally is paid only when the run ends, win or lose (D-006,
//   Nightreign): scaled by the difficulty, a bonus for beating the boss, and one payout multiplier (passives, B-096: evolutions' RunPayoutBonusPercent).
// - In co-op every player is paid in full (D-103).
// Numbers are first guesses: TODO: tune. Debug: GetPayoutString, wor.Trial.PrintRewards.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Trial/TrialGate.h"

class UWorld;
struct FAttachedEvolution;

/** The run's tally, paid at the run's end. */
struct WORLD_OF_REFRACTION_API FRunPayout
{
	/** Hub Gold (ECurrencyType::Prisms, shown as Gold, D-159). */
	int32 HubGold = 0;
	int32 GearEssence = 0;
	int32 SkillEssence = 0;

	FRunPayout &operator+=(const FRunPayout &Other)
	{
		HubGold += Other.HubGold;
		GearEssence += Other.GearEssence;
		SkillEssence += Other.SkillEssence;
		return *this;
	}
	bool IsEmpty() const { return HubGold <= 0 && GearEssence <= 0 && SkillEssence <= 0; }
};

namespace RunRewards
{
	/** The payout multiplier with no passives (B-096 raises it). */
	constexpr float BASE_PAYOUT_MULTIPLIER = 1.0f;

	/** B-096 (Q-096): the most the payout passives can raise the multiplier to (double). TODO: tune. */
	constexpr float MAX_PAYOUT_MULTIPLIER = 2.0f;

	/** B-096: the payout bonus, in percent, that these evolutions give (UEvolutionItemData::RunPayoutBonusPercent);
	 *  a missing or broken one gives nothing. */
	WORLD_OF_REFRACTION_API float SumPayoutBonusPercent(const TArray<FAttachedEvolution> &Attached);

	/** B-096 (Q-096): the multiplier for a party bonus of BonusPercent: BASE + BonusPercent / 100, added up (not
	 *  multiplied), clamped to [BASE, MAX_PAYOUT_MULTIPLIER]. */
	WORLD_OF_REFRACTION_API float PayoutMultiplierForBonus(float BonusPercent);

	/** B-187 (RunStructure §6, Q-187): leaving at a rest spot pays this share of the tally. TODO: tune. */
	constexpr float LEAVE_EARLY_SHARE = 0.5f;

	/** Run Prisms a win pays at once (D-048: what in-run shops charge). */
	WORLD_OF_REFRACTION_API int32 WinRunPrisms(EEncounterRank Rank, ETrialDifficulty Difficulty);

	/** What a win adds to the run's tally (D-094: small from every fight, more from mini-bosses and bosses). */
	WORLD_OF_REFRACTION_API FRunPayout FightTally(EEncounterRank Rank);

	/** What an opened chest adds to the tally (D-094: bigger amounts from chests). */
	WORLD_OF_REFRACTION_API FRunPayout ChestTally();

	/** The tally as paid at the run's end: by the difficulty, the boss bonus and the multiplier, rounded down. */
	WORLD_OF_REFRACTION_API FRunPayout ScalePayout(const FRunPayout &Tally, ETrialDifficulty Difficulty, bool bBossBeaten, float Multiplier);

	/** Server: pays Amount run Prisms to every player's wallet (solo: the one player). Returns how many were paid. */
	WORLD_OF_REFRACTION_API int32 PayWinToPlayers(UWorld *World, int32 Amount);

	/** Server: pays Payout into every player's wallet (D-103: each in full). Returns how many were paid. */
	WORLD_OF_REFRACTION_API int32 PayRunEndToPlayers(UWorld *World, const FRunPayout &Payout);

	/** Debug: "120 Gold, 6 Gear Essence, 6 Skill Essence". */
	WORLD_OF_REFRACTION_API FString GetPayoutString(const FRunPayout &Payout);
}
