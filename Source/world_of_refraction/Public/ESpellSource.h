// ESpellSource.h
// Tracks where a spell originated from for post-cast logic

#pragma once

#include "CoreMinimal.h"
#include "ESpellSource.generated.h"

/**
 * Source of a spell being cast
 * Determines post-cast behavior (break checks, consumption, etc.)
 */
UENUM(BlueprintType)
enum class ESpellSource : uint8
{
	/** Caster's innate spells - no risk */
	Innate UMETA(DisplayName = "Innate"),

	/** From equipped ring - break check after cast */
	Ring UMETA(DisplayName = "Ring"),

	/** From evolution crystal on weapon - TBD */
	Evolution UMETA(DisplayName = "Evolution"),

	/** One-time use spell item - consumed after cast */
	Item UMETA(DisplayName = "Item"),

	/** Spell from weapon crystal - wear applied at commit */
	WeaponCrystal UMETA(DisplayName = "Weapon Crystal")
};
