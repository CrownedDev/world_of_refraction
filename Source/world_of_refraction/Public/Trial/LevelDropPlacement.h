// LevelDropPlacement.h
// B-223 (D-102): the player places a dropped pillar level on a party member's chosen pillar. Dropped levels
// wait in the trial run (UTrialRunSubsystem::GetUnspentLevels); placing one raises that member's live level
// (a spawned member at once, else at its next spawn), clamped at 7 (B-222). Placeholder screen:
// wor.UI.LevelDrop (ULevelDropWidget). Debug: wor.WorldStat.Print.
// B-360c (D-150 point 2, Q-360c): the functions take the party MEMBER, so a later copy of a character places and
// reads its own levels (its member key, its own pawn). The UCharacterData overloads mean the first copy and are
// OBSOLETE (B-360c); delete them in the D-124 sweep once nothing calls them.

#pragma once

#include "CoreMinimal.h"
#include "Character/EWorldPillar.h"

class UCharacterData;
struct FPartyMember;
class UCharacterDataComponent;
class UWorld;

namespace WoRLevelDrops
{
	/** The pillar's player-facing name (Mind / Body / Spirit). */
	WORLD_OF_REFRACTION_API FText GetPillarName(EWorldPillar Pillar);

	/** OBSOLETE (B-360c): the first copy of Member's character; use the FPartyMember overload.
	 *  Member's spawned character component in World, if any. */
	WORLD_OF_REFRACTION_API UCharacterDataComponent *FindSpawnedMember(const UWorld *World, const UCharacterData *Member);

	/** Member's live level in Pillar: its spawned component's, else its asset level plus the run's earned levels
	 *  (head-start lives on the spawned component). */
	WORLD_OF_REFRACTION_API int32 GetLiveLevel(const UWorld *World, const UCharacterData *Member, EWorldPillar Pillar);

	/** Places one unspent level on Member's Pillar. Empty on success, else why not (none waiting, at the cap). */
	WORLD_OF_REFRACTION_API FText TryPlaceLevel(UWorld *World, UCharacterData *Member, EWorldPillar Pillar);

	/** B-360c: the spawned pawn's component for this party member: the pawn stamped with its MemberId, or (an unstamped
	 *  hub pawn) the first copy's pawn of its character. Null when it is not in the level. */
	WORLD_OF_REFRACTION_API UCharacterDataComponent *FindSpawnedMember(const UWorld *World, const FPartyMember &Member);

	/** B-360c: GetLiveLevel for one party member (its own pawn, its own member key). */
	WORLD_OF_REFRACTION_API int32 GetLiveLevel(const UWorld *World, const FPartyMember &Member, EWorldPillar Pillar);

	/** B-360c: TryPlaceLevel for one party member: spends the levels banked under its member key. */
	WORLD_OF_REFRACTION_API FText TryPlaceLevel(UWorld *World, const FPartyMember &Member, EWorldPillar Pillar);

	/** Debug: the run's levels, the unspent count and each party member's live pillars. */
	WORLD_OF_REFRACTION_API FString GetLevelDropString(const UWorld *World);
}
