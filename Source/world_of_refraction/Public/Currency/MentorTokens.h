// MentorTokens.h
// B-361b (D-154, Q-361b): Mentor Tokens are earned only by playing together and spent teaching a learned skill to
// another player's character (WoRGive::TryTeachSkill). Each player's own wallet (AWoRPlayerState) holds them.
// Earned: every player when a co-op run is finished (its boss beaten, then the run ends) and when a boss falls with
// two or more players here; nothing in solo play. Debug: wor.Mentor.Print, GetStateString.

#pragma once

#include "CoreMinimal.h"

class APlayerState;
class UCurrencyComponent;
class UWorld;

namespace MentorTokenRules
{
	/** Q-361b (1): paid to each player who finishes a co-op run. */
	constexpr int32 PER_COOP_RUN = 1;
	/** Q-361b (1): paid to each player when a boss falls with another player here. */
	constexpr int32 PER_BOSS_TOGETHER = 1;
	/** Q-361b (2): teaching an F-tier skill costs this; each tier above adds TEACH_COST_PER_TIER (S the most). */
	constexpr int32 TEACH_COST_AT_F = 1;
	constexpr int32 TEACH_COST_PER_TIER = 1;
	/** "Playing together" means at least this many players here. */
	constexpr int32 MIN_PLAYERS_TOGETHER = 2;
}

namespace WoRMentorTokens
{
	/** Tokens to teach Skill (a USpellData or UAbilityData): by the asset's tier, the tier the learner's copy gets.
	 *  0 for anything else. */
	WORLD_OF_REFRACTION_API int32 GetTeachCost(const UObject *Skill);

	/** The players in this world (AWoRPlayerState entries in the game state). */
	WORLD_OF_REFRACTION_API int32 CountPlayers(const UWorld *World);

	/** The player's wallet (AWoRPlayerState::GetWallet), or null. */
	WORLD_OF_REFRACTION_API UCurrencyComponent *WalletOf(const APlayerState *Player);

	/** Server: pays Amount to every player when at least MIN_PLAYERS_TOGETHER are here; Why goes to the log.
	 *  Returns how many players were paid (0 in solo play). */
	WORLD_OF_REFRACTION_API int32 PayPlayersTogether(UWorld *World, int32 Amount, const TCHAR *Why);

	/** Debug: each player's tokens, one line each. */
	WORLD_OF_REFRACTION_API FString GetStateString(const UWorld *World);
}
