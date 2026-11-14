// CrystalDescription.h
// Free-function description generators for crystals, keyed by FCrystalId. Lifted
// from UEvolutionItemData::GenerateDescription so any consumer (item crystal,
// refined crystal, evolution crystal) can produce description text from just a
// FCrystalId — no UEvolutionItemData asset pointer required.
//
// Three layers:
//  - GetTierDescriptor: the tier adjective ("crude" … "legendary"), tier-only.
//  - GetCrystalText:    "A {adj} {name} crystal." — the shared flavor sentence
//                       any crystal kind can use.
//  - GetItemEffectText: the mechanical effect sentence with numbers, for item/
//                       refined consumable crystals. Self-contained sentence
//                       ending in ".".
//
// Both GetCrystalText and GetItemEffectText return self-contained sentences
// (trailing "."). Evolution crystals use GetCrystalText for their Description
// field and UEvolutionItemData::GetEvolutionEffectText for their effect text,
// which also returns a self-contained "." sentence — keeping period semantics
// consistent across item / evolution paths.
//
// Numerics come from CrystalEffectTable::* — this layer only formats text on
// top of the existing FCrystalId-keyed value tables.
//
// B-460 (D-061): player-facing text, so FText (LOCTEXT / FText::Format in CrystalDescription.cpp).

#pragma once

#include "CoreMinimal.h"
// Kept for the files that read these through this header (before B-460 the table was inline here).
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/StatStone.h"
#include "Inventory/ItemTier.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/CombatConstants.h"

namespace CrystalDescription
{
    /** Tier-flavor adjective. F=crude, E=common, D=refined, C=quality,
     *  B=exceptional, A=masterwork, S=legendary. */
    WORLD_OF_REFRACTION_API FText GetTierDescriptor(EItemTier Tier);

    /** Shared "what kind of item it is" flavor sentence — usable by any
     *  crystal kind. Gems: "A {tier-descriptor} {name-lowercase} crystal."
     *  Stones: "A {tier-descriptor} {Spaced Name}." — a stone is not a crystal,
     *  and its spaced display name (via GetTypeName) keeps its capitalization. */
    WORLD_OF_REFRACTION_API FText GetCrystalText(const FCrystalId &InId);

    /** Mechanical effect sentence for item/refined crystals. Per-CrystalType
     *  switch with S-tier conditional alternates (Sapphire / Emerald / Onyx)
     *  and effect-count branches (Iolite). Reads CrystalEffectTable for
     *  numerics. Pluralises turn counts ("1 turn" vs "N turns") and returns
     *  a self-contained sentence ending in ".". */
    WORLD_OF_REFRACTION_API FText GetItemEffectText(const FCrystalId &InId);
}
