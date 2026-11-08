// LevelDropPlacement.h
// B-223 (D-102): the player places a dropped pillar level on a party member's chosen pillar. B-363 (D-156): only at a
// rest spot, where a point can also be bought for run Prisms (TryBuyPoint). Dropped levels
// wait in the trial run (UTrialRunSubsystem::GetUnspentLevels); placing one raises that member's live level
// (a spawned member at once, else at its next spawn), clamped at 7 (B-222). Placeholder screen:
// wor.UI.LevelDrop (ULevelDropWidget). Debug: wor.WorldStat.Print.
// B-360c (D-150 point 2, Q-360c): everything takes the party member, so a later copy of a character places and reads
// its own levels (keyed by its member key, UPoolSubsystem::GetCharacterKey(const FPartyMember&)).

#pragma once

#include "CoreMinimal.h"
#include "Character/EWorldPillar.h"

class UCharacterDataComponent;
class UCurrencyComponent;
struct FPartyMember;
class UWorld;

namespace WoRLevelDrops
{
	/** The pillar's player-facing name (Mind / Body / Spirit). */
	WORLD_OF_REFRACTION_API FText GetPillarName(EWorldPillar Pillar);

	/** Member's spawned character component in World (the pawn with its member key), if any. */
	WORLD_OF_REFRACTION_API UCharacterDataComponent *FindSpawnedMember(const UWorld *World, const FPartyMember &Member);

	/** Member's live level in Pillar: its spawned component's, else its asset level plus the run's earned levels
	 *  (head-start lives on the spawned component). */
	WORLD_OF_REFRACTION_API int32 GetLiveLevel(const UWorld *World, const FPartyMember &Member, EWorldPillar Pillar);

	/** Places one unspent level on Member's Pillar. Empty on success, else why not (not at a rest spot (B-363, D-156),
	 *  none waiting, at the cap). */
	WORLD_OF_REFRACTION_API FText TryPlaceLevel(UWorld *World, const FPartyMember &Member, EWorldPillar Pillar);

	/** B-363 (Q-156): the run Prisms Member's next bought point costs: ER-24's ladder per character this run. */
	WORLD_OF_REFRACTION_API int32 GetPointPrice(const UWorld *World, const FPartyMember &Member);

	/** B-363 (D-156, Q-156): buys Member one unspent point with run Prisms from Wallet (null: the first local player's),
	 *  at a rest spot. Empty on
	 *  success, else why not (not at a rest spot, every pillar at the cap (until B-364), too few Prisms). */
	WORLD_OF_REFRACTION_API FText TryBuyPoint(UWorld *World, const FPartyMember &Member, UCurrencyComponent *Wallet = nullptr);

	/** Debug: the run's levels, the unspent count and each party member's live pillars. */
	WORLD_OF_REFRACTION_API FString GetLevelDropString(const UWorld *World);
}
