// DefeatRules.h
// B-362 (D-155, Q-155): losing a fight. A party wipe costs run pillar points only (points gained in this run; never
// the asset's levels, the head start or anything kept): up to 1 per member for a normal enemy, up to 2 for a
// mini-boss, 0 for the main boss. The party comes back at the last rest spot used (the trial's start until B-187)
// with the enemy team whole; the run ends, back to the hub, after 3 losses to the main boss or 5 wipes in all.
// The counters live on the run (UTrialRunSubsystem, saved with it). Replaces B-072. Debug: wor.Trial.PrintLosses.

#pragma once

#include "CoreMinimal.h"
#include "Character/EWorldPillar.h"
#include "Combat/Encounter/EEncounterRank.h"

class UWorld;

namespace DefeatRules
{
	/** Run pillar points each member loses at most, by the rank of the enemy that beat the party. */
	inline constexpr int32 POINTS_LOST_NORMAL = 1;
	inline constexpr int32 POINTS_LOST_MINI_BOSS = 2;
	inline constexpr int32 POINTS_LOST_MAIN_BOSS = 0;
	/** The run ends after this many wipes in all (every wipe counts, a main-boss loss too: Q-155 point 4). */
	inline constexpr int32 MAX_WIPES_PER_RUN = 5;
	/** The run ends after this many losses to the main boss. */
	inline constexpr int32 MAX_MAIN_BOSS_LOSSES_PER_RUN = 3;

	inline bool IsMainBoss(EEncounterRank Rank) { return Rank == EEncounterRank::Boss; }

	/** The most run points a member loses to an enemy of Rank. An Elite counts as a normal enemy. */
	inline int32 GetPointsLost(EEncounterRank Rank)
	{
		switch (Rank)
		{
		case EEncounterRank::MiniBoss: return POINTS_LOST_MINI_BOSS;
		case EEncounterRank::Boss:     return POINTS_LOST_MAIN_BOSS;
		case EEncounterRank::Normal:
		case EEncounterRank::Elite:
		default:                       return POINTS_LOST_NORMAL;
		}
	}

	/** True once the run's wipes or main-boss losses reach their limits. */
	inline bool RunEnds(int32 Wipes, int32 MainBossLosses)
	{
		return Wipes >= MAX_WIPES_PER_RUN || MainBossLosses >= MAX_MAIN_BOSS_LOSSES_PER_RUN;
	}

	/** The pillar a lost point comes from: the one with the most run points; a tie goes Mind, then Body, then Spirit
	 *  (Q-155 point 2). False when the member has no run points at all. TODO: B-364 (D-157): over-cap points are
	 *  taken first once they exist. */
	inline bool PickPillarToLose(int32 MindPoints, int32 BodyPoints, int32 SpiritPoints, EWorldPillar &OutPillar)
	{
		if (MindPoints <= 0 && BodyPoints <= 0 && SpiritPoints <= 0)
		{
			return false;
		}
		OutPillar = EWorldPillar::Mind;
		int32 Most = MindPoints;
		if (BodyPoints > Most)
		{
			OutPillar = EWorldPillar::Body;
			Most = BodyPoints;
		}
		if (SpiritPoints > Most)
		{
			OutPillar = EWorldPillar::Spirit;
		}
		return true;
	}
}

namespace WoRDefeat
{
	/** What a wipe did. */
	struct WORLD_OF_REFRACTION_API FWipeResult
	{
		int32 PointsTaken = 0;
		int32 Wipes = 0;
		int32 MainBossLosses = 0;
		int32 Revived = 0;
		bool bRunEnds = false;
	};

	/** The party was wiped by an enemy of Rank: counts it on the run, takes each member's run pillar points (MemberKeys:
	 *  the keys of the party members that fought), and, if the run continues, brings the fallen back at full (the rest
	 *  spot's revive, D-043). The caller sends the party back to the trial (the last rest spot used) or, when
	 *  bRunEnds, to the hub. Server side. */
	WORLD_OF_REFRACTION_API FWipeResult ApplyWipe(UWorld *World, EEncounterRank Rank, const TArray<FName> &MemberKeys);

	/** Debug: the run's wipe and main-boss-loss counts against their limits, and the last rest spot used. */
	WORLD_OF_REFRACTION_API FString GetDefeatString(const UWorld *World);
}
