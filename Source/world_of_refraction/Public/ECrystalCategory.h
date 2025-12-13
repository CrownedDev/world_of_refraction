// ECrystalCategory.h
// Crystal category enum for item classification
//
// Determines what a crystal can be used for:
// - Item: Consumable in combat, item effects active
// - Refined: Slottable on weapons/rings, 6 spell slots, no item effects
// - Evolution: Slottable on weapons/rings/characters, locked+custom spells, grants evolution

#pragma once

#include "CoreMinimal.h"
#include "ECrystalCategory.generated.h"

/**
 * ECrystalCategory
 * Defines the usage category for a crystal
 */
UENUM(BlueprintType)
enum class ECrystalCategory : uint8
{
    /** Consumable crystal - uses item effects in combat */
    Item        UMETA(DisplayName = "Item (Consumable)"),
    
    /** Refined crystal - slottable on weapons/rings, provides element + 6 spell slots */
    Refined     UMETA(DisplayName = "Refined (Slottable)"),
    
    /** Evolution crystal - grants evolution state, has locked spells + stat modifiers */
    Evolution   UMETA(DisplayName = "Evolution (Grants Evolution)")
};
