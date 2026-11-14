// DefeatRules.h
// B-362 (D-155, Q-155): losing a fight. A party wipe costs run pillar points only (points gained in this run; never
// the asset's levels, the head start or anything kept): up to 1 per member for a normal enemy, up to 2 for a
// mini-boss, 0 for the main boss. The party comes back at the last rest spot used (the trial's start until B-187)
// with the enemy team whole; the run ends, back to the hub, after 3 losses to the main boss or 5 wipes in all.
// The counters live on the run (UTrialRunSubsystem, saved with it). Replaces B-072. Debug: wor.Trial.PrintLosses.
// B-362b (D-160, amends D-155): a wipe no longer costs pillar points. The run's whole Prism balance (the run
// currency, ECurrencyType::Gold, D-159) drops at the map fight the party fell to; beating that fight's team recovers
// it; wiping again first loses the earlier drop for good (the Souls bloodstain). A main-boss loss drops the same way.
// B-362c (D-160 as changed): one path. The drop lies as a pickup (APrismDropPickup) where the party stood on the
// trial map when the fight started; walking into it (or interact) puts it back in the wallet. Any fight drops it, map
// team or not. Beating the fight no longer recovers it.

#pragma once

class APawn;
class APlayerState;

#include "CoreMinimal.h"
#include "Character/EWorldPillar.h"
#include "Combat/Encounter/EEncounterRank.h"

class UCurrencyComponent;
class UTrialRunSubsystem;
class UWorld;

namespace DefeatRules
{
	/** OBSOLETE (B-362b, D-160): a wipe takes no pillar points; these and GetPointsLost / PickPillarToLose have no
	 *  game caller; delete after PIE. Run pillar points each member loses at most, by the rank of the enemy. */
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
		/** OBSOLETE (B-362b): always 0. */
		int32 PointsTaken = 0;
		/** B-362b: the run Prisms dropped at this fight, and the earlier drop's, lost for good. */
		int32 PrismsDropped = 0;
		int32 PrismsLostForGood = 0;
		int32 Wipes = 0;
		int32 MainBossLosses = 0;
		int32 Revived = 0;
		bool bRunEnds = false;
		/** B-450 (D-187): the opponent that dealt the last blow, by name, for the wipe card (never its rank). Empty when
		 *  unknown (a status tick, a self-hit). */
		FText LastBlowBy;
	};

	/** The party was wiped by an enemy of Rank: counts it on the run, drops the local wallet's run Prisms at this fight
	 *  (DropPrisms), and, if the run continues, brings the fallen back at full (the rest spot's revive, D-043). The
	 *  caller sends the party back to the trial (the last rest spot used) or, when bRunEnds, to the hub. Server side.
	 *  MemberKeys: the members that fought (logged; they no longer pay points, D-160). B-235: every other player's
	 *  wallet drops too, each into their own drop at the same spot. */
	WORLD_OF_REFRACTION_API FWipeResult ApplyWipe(UWorld *World, EEncounterRank Rank, const TArray<FName> &MemberKeys,
												  const FText &LastBlowBy = FText::GetEmpty());

	/** B-362b: Wallet's whole run balance leaves it and becomes Run's drop at the pending fight. Returns the amount
	 *  dropped; OutLostForGood gets the earlier drop's amount. */
	WORLD_OF_REFRACTION_API int32 DropPrisms(UTrialRunSubsystem &Run, UCurrencyComponent *Wallet, int32 &OutLostForGood);

	/** OBSOLETE (B-362c, D-160 changed: the pickup is the one path; no game caller; delete after PIE).
	 *  B-362b: the pending fight was won; if it came from the drop's map team, the drop goes back into Wallet. Returns
	 *  the amount recovered. */
	WORLD_OF_REFRACTION_API int32 RecoverPrisms(UTrialRunSubsystem &Run, UCurrencyComponent *Wallet);

	/** OBSOLETE (B-362c): RecoverPrisms into the local player's wallet (the battle's victory). */
	WORLD_OF_REFRACTION_API int32 RecoverPrisms(UWorld *World);

	/** B-362c: the drop's pickup was taken: the whole drop goes back into Wallet, once. Returns the amount. With no
	 *  wallet the drop stays. */
	WORLD_OF_REFRACTION_API int32 PickUpDrop(UTrialRunSubsystem &Run, UCurrencyComponent *Wallet);

	/** PickUpDrop into the local player's wallet, saved at once (quitting must not undo it). */
	WORLD_OF_REFRACTION_API int32 PickUpDrop(UWorld *World);

	/** B-235 (D-160 co-op): Pawn's player takes their own drop into their own wallet: the local player through
	 *  PickUpDrop(World), another player through UTrialRunSubsystem::TakeDropForPlayer. Returns the amount (0: none). */
	WORLD_OF_REFRACTION_API int32 PickUpDropFor(UWorld *World, const APawn *Pawn);

	/** B-235: a player's key for their own drop: their unique net id, else their player id. */
	WORLD_OF_REFRACTION_API FString PlayerDropKey(const APlayerState *Player);

	/** B-428 (b, Q-428 = A): one player's share of a leaver's Amount split equally among Players (> 0): Amount / Players,
	 *  plus the remainder for the host. 0 when nobody is left. */
	WORLD_OF_REFRACTION_API int32 LeaverDropShare(int32 Amount, int32 Players, bool bHost);

	/** B-428 (b, Q-428 = A; server): Leaver left the run without extracting. Their dropped Prisms (Gold) are split
	 *  equally into the remaining players' wallets, the remainder to the host, and their drop is removed. Returns the
	 *  amount handed out (0: no run, no drop, or nobody left). Rules PA-37i. */
	WORLD_OF_REFRACTION_API int32 SplitLeaverDrop(UWorld *World, const APlayerState *Leaver);

	/** B-235b: the drops of the players here now: the local player's plus each other present player's. A player who
	 *  left has had their drop split among the rest (B-428, SplitLeaverDrop), so it no longer keeps the pickup up. */
	WORLD_OF_REFRACTION_API int32 GetPresentDroppedPrisms(const UWorld *World);

	/** Debug: the run's wipe and main-boss-loss counts against their limits, the dropped Prisms and their spot, and
	 *  the last rest spot. */
	WORLD_OF_REFRACTION_API FString GetDefeatString(const UWorld *World);
}
