// MentorTokenRules.h
// B-361b (D-154, Q-361b): Mentor Tokens, the eighth balance. Earned only by playing together: each player gets
// RUN_PAYOUT when a run ends with two or more players in the party, and BOSS_PAYOUT for each main boss beaten with
// another player. Spent to teach a learned skill to another player's member (WoRGive::TryTeachSkill), priced by the
// skill's tier. Every number is a named constant for tuning (Crown may change them). Debug: wor.Wallet.Print.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemTier.h"

class UWorld;

namespace MentorTokenRules
{
	/** Fewer players than this and nothing is paid: tokens reward playing together. */
	inline constexpr int32 MIN_PLAYERS_FOR_PAYOUT = 2;
	/** Tokens each player gets when a co-op run ends. TODO: tune in play. */
	inline constexpr int32 RUN_PAYOUT = 1;
	/** Tokens each player gets per main boss beaten with another player. TODO: tune in play. */
	inline constexpr int32 BOSS_PAYOUT = 1;
	/** Teaching a skill of the lowest tier (F) costs this. */
	inline constexpr int32 TEACH_PRICE_AT_LOWEST_TIER = 1;
	/** Each tier above F adds this to the teaching price. */
	inline constexpr int32 TEACH_PRICE_PER_TIER = 1;

	/** What teaching a skill of Tier costs the teacher: 1 at F, one more per tier above (S the most). */
	inline int32 GetTeachPrice(EItemTier Tier)
	{
		return TEACH_PRICE_AT_LOWEST_TIER + TierHelpers::GetTierValue(Tier) * TEACH_PRICE_PER_TIER;
	}

	/** What each player gets when a run ends with PlayerCount players; 0 in solo play. */
	inline int32 GetRunPayout(int32 PlayerCount)
	{
		return PlayerCount >= MIN_PLAYERS_FOR_PAYOUT ? RUN_PAYOUT : 0;
	}

	/** What each player gets for a main boss beaten with PlayerCount players; 0 in solo play. */
	inline int32 GetBossPayout(int32 PlayerCount)
	{
		return PlayerCount >= MIN_PLAYERS_FOR_PAYOUT ? BOSS_PAYOUT : 0;
	}
}

namespace WoRMentor
{
	/** Server only: every player here gets Amount tokens in its own wallet. Returns how many players were paid. */
	WORLD_OF_REFRACTION_API int32 PayEachPlayer(UWorld *World, int32 Amount);

	/** Server: the run is ending; pays RUN_PAYOUT to each player when two or more are here (solo pays none). */
	WORLD_OF_REFRACTION_API int32 PayRunEnd(UWorld *World);

	/** Server: a main boss was beaten; pays BOSS_PAYOUT to each player when two or more are here. */
	WORLD_OF_REFRACTION_API int32 PayBossBeaten(UWorld *World);

	/** Debug: each player's wallet owner and token balance, on one line. */
	WORLD_OF_REFRACTION_API FString GetTokensString(const UWorld *World);
}
