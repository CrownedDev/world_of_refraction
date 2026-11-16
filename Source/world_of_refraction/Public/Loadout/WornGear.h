// WornGear.h
// D-221 stage 1 (B-505): "wearing now", one record per character, kept in the pool (UPoolSubsystem::GetWornGear) and
// read by every map's draw. The pawn's FCombatLoadout is built from it (FCombatLoadout::CreateFromWornGear) and written
// back on every change (FCombatLoadout::ToWornGear). Plan: docs/Design/LoadoutRestructurePlan.md §1.

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSkillTypes.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FLoadoutSlot.h"
#include "WornGear.generated.h"

class USpellData;

namespace WornGearLayout
{
	// shortcut: until the readers move onto Equipment (stage 2), the list keeps the old fields' places so the old
	// loadout can be rebuilt from it with no loss; stage 2 drops the fixed places for the player's order (D-220).
	constexpr int32 PRIMARY_INDEX = 0;          // the primary weapon (with its off hand) or the primary adapter; empty for none or an evolution
	constexpr int32 SECONDARY_INDEX = 1;        // the Generic's second weapon; empty for none
	constexpr int32 FIRST_ADAPTER_INDEX = 2;    // then the adapter loadout (a Resonator's, or a Generic's one adapter, B-480)
}

/** D-218: the field wheel's pick, moved off FPartyMember (same meaning). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWheelPick
{
	GENERATED_BODY()

	/** FPartyMember::FieldSkill. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	FFieldSkillChoice FieldSkill;

	/** FPartyMember::FieldAttackIndex (Q-503 (3): the place in the list). */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	int32 FieldAttackIndex = FieldSkillRules::FIRST_FIELD_ATTACK_INDEX;

	/** FPartyMember::bFieldAttackPicked. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	bool bPicked = false;

	bool operator==(const FWheelPick &Other) const
	{
		return FieldSkill == Other.FieldSkill && FieldAttackIndex == Other.FieldAttackIndex && bPicked == Other.bPicked;
	}
};

/** D-221: what a character wears now. Pieces by their owned GUIDs; the slot's Gear is a cache, filled on each build. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWornGear
{
	GENERATED_BODY()

	/** Weapons and adapters, in the WornGearLayout places. */
	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	TArray<FLoadoutSlot> Equipment;

	/** The weapon in hand (the ACTIVE entry): replaces bShowPrimary and B-503s2's ActiveWeaponId. Invalid: none. */
	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	FGuid ActiveGearID;

	/** The evolution in the primary place (LA-6 makes it the character's own in stage 2). Invalid: none. */
	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	FGuid EvolutionID;

	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	TArray<TObjectPtr<USpellData>> EvolutionSpells;

	/** Owned apparel pieces (D-164). */
	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	TArray<FGuid> Apparel;

	/** A caster's innate spells (and Broken Darkness's Darkness pool). */
	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	TArray<TObjectPtr<USpellData>> InnateSpells;

	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	TArray<FBDElementSpellPool> BDSpellPools;

	/** D-218. Written apart from the gear (UPoolSubsystem::SetWheelPick); a gear write keeps it. */
	UPROPERTY(BlueprintReadOnly, Category = "Worn")
	FWheelPick WheelPick;

	/** The entry at a WornGearLayout place, or null when the list is shorter. */
	const FLoadoutSlot *GetEntry(int32 Index) const { return Equipment.IsValidIndex(Index) ? &Equipment[Index] : nullptr; }

	/** Every owned piece it names (main and off hands, adapters, the evolution, apparel). */
	TArray<FGuid> GetPieceIDs() const;

	/** Same pieces, places, spells and pick (the slot caches are not compared). */
	bool SameAs(const FWornGear &Other) const;
};
