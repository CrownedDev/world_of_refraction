// ECharacterOrigin.h
// What a character IS — distinct from who is driving it.
//
// Identity lives on the asset (this enum); control is a runtime question answered
// by engine possession (Pawn->IsPlayerControlled() / IsBotControlled()). Splitting
// them is what lets one character be player-driven in solo play and AI-driven as a
// ghost or companion without duplicating the asset.
//
// Replaces UCharacterData::bIsAIControlled, which conflated the two axes and could
// express neither a Player-origin character under AI control nor an Enemy-origin
// character under player control (PvP).

#pragma once

#include "CoreMinimal.h"
#include "ECharacterOrigin.generated.h"

UENUM(BlueprintType)
enum class ECharacterOrigin : uint8
{
	/** Authored opposition. The default — an unauthored character is an enemy. */
	Enemy UMETA(DisplayName = "Enemy"),

	/** A player-side build: party member, or a ghost captured from a player run. */
	Player UMETA(DisplayName = "Player Build"),
};
