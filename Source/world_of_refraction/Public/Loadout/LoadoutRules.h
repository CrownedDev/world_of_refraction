// LoadoutRules.h
// The one loadout rule set (B-028, D-073 = A). Runs on the fight's loadout shape (FCombatLoadout)
// with a context saying what can be checked. Two adapters feed it:
// - ULoadoutComponent (before a fight): the character's class, BD state, mastery discount, ownership
//   and element castability.
// - FSavedLoadout (the editor): the authored loadout converted with CreateFromSavedLoadout. Asset
//   ownership is cross-checked by UInventoryData; there is no character, so castability is unchecked
//   and the spell discount is 0 (the strictest budget).
// Where the old checks disagreed, the fight's answer wins.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"
#include "LoadoutRules.generated.h"

struct FCombatLoadout;
struct FSavedLoadout;
class UWeaponData;
class URingData;
class UEvolutionItemData;
class UAbilityData;
class USpellData;

/** Discriminates which loadout slot an FInvalidSlotFinding refers to.
 *  `None` marks a non-slot guard failure (no inventory, invalid index) —
 *  surfaced but not clearable. `BDPoolSpell` is a non-clearable BD-pool
 *  diagnostic. The *Overflow / DuplicateItems values are "category"
 *  findings (no single slot) that ClearInvalidSlots re-derives the clearing
 *  action for. */
UENUM()
enum class ELoadoutSlotType : uint8
{
	None,
	PrimaryWeapon,
	PrimaryRing,
	PrimaryEvolution,
	SecondaryWeapon,
	ResonatorRing,
	InnateSpell,
	WeaponAbility,
	WeaponSpell,
	RingSpell,
	BDPoolSpell,
	// Category findings — bClearable, but cleared by re-derived rules rather
	// than nulling one addressable slot.
	EvolutionOverflow,
	RingOverflow,
	DuplicateItems,
	// Reported, not cleared yet (B-159 drops them on equip / load).
	AbilityOverflow,
	ItemSlot
};

/** One actionable validation finding, produced by FLoadoutRules — the structured
 *  counterpart to the human-readable strings from GetValidationErrors. SlotType +
 *  SlotIndex make a finding addressable so ClearInvalidSlots can null the exact slot.
 *  bClearable == false marks diagnostics (guards, structural caps, BD pools) that
 *  are reported but never auto-cleared. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FInvalidSlotFinding
{
	GENERATED_BODY()

	UPROPERTY()
	ELoadoutSlotType SlotType = ELoadoutSlotType::None;

	/** Index within the slot category (ability index, spell index, ring index).
	 *  -1 for non-indexed / single slots (primary weapon, primary ring, etc.).
	 *  For RingSpell: the ring-loadout index (-1 = Generic/Caster primary ring). */
	UPROPERTY()
	int32 SlotIndex = 0;

	/** Secondary index for nested slots. For RingSpell: the spell index within
	 *  the ring's AssignedSpells. -1 when unused. */
	UPROPERTY()
	int32 SubIndex = -1;

	UPROPERTY()
	FString Reason;

	UPROPERTY()
	bool bClearable = true;
};

/** What the rules can check for this loadout. An unset function means "not checked". */
struct WORLD_OF_REFRACTION_API FLoadoutRuleContext
{
	ECharacterClass CharacterClass = ECharacterClass::Generic;

	/** The character is Broken Darkness (D-073: BD rules follow the character, not the authored pools). */
	bool bBrokenDarkness = false;

	/** Mastery discount on spell weights (SpellPoolConstants::SpellSlotDiscount). */
	int32 SpellDiscount = 0;

	TFunction<bool(const UWeaponData *)> OwnsWeapon;
	TFunction<bool(const URingData *)> OwnsRing;
	TFunction<bool(const UEvolutionItemData *)> OwnsEvolution;
	TFunction<bool(const UAbilityData *)> KnowsAbility;
	TFunction<bool(const USpellData *)> KnowsSpell;
	TFunction<bool(ESpellElement)> CanCastElement;
};

struct WORLD_OF_REFRACTION_API FLoadoutRules
{
	/** Every rule the fight enforces, on the fight's loadout shape. */
	static TArray<FInvalidSlotFinding> CollectFindings(const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context);

	/** Problems only an authored loadout can have: CreateFromSavedLoadout drops them silently
	 *  (a slot type with nothing in it, a Resonator Ring primary, an empty ring slot, extra item slots). */
	static TArray<FString> CollectAuthoredShapeErrors(const FSavedLoadout &Saved);

	static TArray<FString> ToReasons(const TArray<FInvalidSlotFinding> &Findings);
};
