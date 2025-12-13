// ECrystalCategory.h
// Crystal category enum for item classification
//
// Determines the BASE TYPE of a crystal:
// - Item: Standard elemental crystal, consumable effects
// - Evolution: Special crystal that grants evolution status
//
// NOTE: Slotting is a SEPARATE concern - see bIsRefined on ItemData
// A crystal must be refined (cut) before it can be slotted onto equipment

#pragma once

#include "CoreMinimal.h"
#include "ECrystalCategory.generated.h"

/**
 * ECrystalCategory
 * Defines the base type of a crystal
 */
UENUM(BlueprintType)
enum class ECrystalCategory : uint8
{
    /** Standard elemental crystal - consumable effects when not refined */
    Item UMETA(DisplayName = "Item Crystal"),

    /** Evolution crystal - grants evolution when applied to character or slotted on equipment */
    Evolution UMETA(DisplayName = "Evolution Crystal")
};