// CombatBaselineConstants.h
// B-224 (Crown, analysis/BaselineNumbers.md "Decided", 2026-10-27): the combat numbers baseline as C++
// tables. Each is the fallback and the FillLadderTable source of a UCombatTuningData row (CombatTuning.h),
// so the asset can retune them without a rebuild. Light on purpose: no UObject include.
// Pinned by the spec WoR.Tuning.Baseline; printed by wor.Tuning.Print.

#pragma once

#include "CoreMinimal.h"
#include "Character/StatConstants.h"
#include "Combat/Damage/TierPowerConstants.h"

namespace CombatBaseline
{
	/** Pillar levels in a run go 0..7 (B-222: WorldStatConstants::LIVE_MAX = MAX_WORLD_STAT_LEVEL). */
	constexpr int32 PILLAR_CURVE_MAX_LEVEL = StatConstants::MAX_WORLD_STAT_LEVEL;

	/** The pillar soft cap (Elden Ring-like): Curve(L) = 1 - (1 - L/7)^2, so L1 0.27, L4 0.82, L7 1.0. */
	inline float CodePillarCurve(int32 Level)
	{
		const float Max = static_cast<float>(PILLAR_CURVE_MAX_LEVEL);
		const float Short = 1.0f - FMath::Clamp(static_cast<float>(Level), 0.0f, Max) / Max;
		return 1.0f - Short * Short;
	}

	/** A plain F weapon's Power; each tier up rides TIER_POWER: F 40, E 52, D 68, C 88, B 114, A 148, S 192. */
	constexpr int32 WEAPON_REFERENCE_POWER_F = 40;

	/** The reference Power of a weapon of its own tier (F = 0 .. S = 6); authored weapons sit within about
	 *  15% of it (BaselineNumbers.md §2.6). Out of range: the F value. */
	inline float CodeWeaponReferencePower(int32 TierIndex)
	{
		return WEAPON_REFERENCE_POWER_F * TierPowerScaling::CodeTierPower(TierIndex);
	}

	/** Main-pillar requirement of a weapon of its tier (F = 0 .. S = 6): an S weapon's full power needs a
	 *  maxed pillar. A second pillar asks a level or two less. Out of range: 0. */
	constexpr int32 WEAPON_REQUIREMENT_BY_TIER[7] = {0, 1, 2, 3, 5, 6, 7}; // F..S
	static_assert(sizeof(WEAPON_REQUIREMENT_BY_TIER) / sizeof(WEAPON_REQUIREMENT_BY_TIER[0]) == 7,
				  "WEAPON_REQUIREMENT_BY_TIER must cover all 7 tiers (F..S).");
	static_assert(WEAPON_REQUIREMENT_BY_TIER[6] == PILLAR_CURVE_MAX_LEVEL, "An S weapon asks for a maxed pillar.");

	inline int32 CodeWeaponRequirement(int32 TierIndex)
	{
		return (TierIndex >= 0 && TierIndex < 7) ? WEAPON_REQUIREMENT_BY_TIER[TierIndex] : 0;
	}

	/** Enemy max HP by encounter rank, on top of the stat formula, zone tier and difficulty. Keyed by the
	 *  EEncounterRank value: Normal 1, Mini-boss 4, Boss 8, Elite 1.5 (appended to the enum, so key 3).
	 *  Targets: normal 3-5 player turns, mini-boss 8-10 rounds, boss 12-14 rounds. */
	constexpr float ENEMY_RANK_HEALTH_NORMAL = 1.0f;
	// B-293d (Q-293d, Crown 2026-11-03): mini-boss and boss fights ran short at Medium (was x3 / x6).
	constexpr float ENEMY_RANK_HEALTH_MINIBOSS = 4.0f;
	constexpr float ENEMY_RANK_HEALTH_BOSS = 8.0f;
	constexpr float ENEMY_RANK_HEALTH_ELITE = 1.5f;
	constexpr int32 ENEMY_RANK_COUNT = 4;
	constexpr float ENEMY_RANK_HEALTH[ENEMY_RANK_COUNT] = {ENEMY_RANK_HEALTH_NORMAL, ENEMY_RANK_HEALTH_MINIBOSS,
														   ENEMY_RANK_HEALTH_BOSS, ENEMY_RANK_HEALTH_ELITE};

	/** Out of range: a normal enemy's x1. */
	inline float CodeEnemyRankHealth(int32 RankValue)
	{
		return (RankValue >= 0 && RankValue < ENEMY_RANK_COUNT) ? ENEMY_RANK_HEALTH[RankValue] : ENEMY_RANK_HEALTH_NORMAL;
	}

	/** B-223 (D-102, Q-223): a normal enemy's chance to drop a pillar level, by its tier (F..S). An elite
	 *  doubles it. TODO: tune with B-293c's nightly runs. */
	constexpr int32 LEVEL_DROP_TIER_COUNT = 7;
	constexpr float LEVEL_DROP_CHANCE_BY_TIER[LEVEL_DROP_TIER_COUNT] = {0.10f, 0.12f, 0.15f, 0.18f, 0.22f, 0.26f, 0.30f};
	constexpr float LEVEL_DROP_ELITE_CHANCE_FACTOR = 2.0f;

	/** Levels a mini-boss (or boss) always gives, by its tier: 1 at F..D, 2 at C..B, 3 at A..S. TODO: tune. */
	constexpr int32 MINIBOSS_LEVELS_BY_TIER[LEVEL_DROP_TIER_COUNT] = {1, 1, 1, 2, 2, 3, 3};

	inline float CodeLevelDropChance(int32 TierIndex)
	{
		return (TierIndex >= 0 && TierIndex < LEVEL_DROP_TIER_COUNT) ? LEVEL_DROP_CHANCE_BY_TIER[TierIndex] : 0.0f;
	}

	inline int32 CodeMiniBossLevels(int32 TierIndex)
	{
		return (TierIndex >= 0 && TierIndex < LEVEL_DROP_TIER_COUNT) ? MINIBOSS_LEVELS_BY_TIER[TierIndex] : 0;
	}
}
