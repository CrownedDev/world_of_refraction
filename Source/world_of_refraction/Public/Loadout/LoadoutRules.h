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
struct FLoadoutSlot;
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
	// B-159: dropped like the category findings above. AbilityOverflow: SlotIndex 0 primary /
	// 1 secondary weapon, SubIndex 0 abilities / 1 augment stone abilities. ItemSlot: a quantity
	// out of range (clamped). UtilityCrystalTierCap: too many utility crystals in a tier band.
	AbilityOverflow,
	ItemSlot,
	UtilityCrystalTierCap
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

/** SlotIndex / SubIndex meanings of an AbilityOverflow finding. */
namespace LoadoutAbilityOverflowIndex
{
	constexpr int32 PRIMARY_WEAPON = 0;
	constexpr int32 SECONDARY_WEAPON = 1;
	constexpr int32 ABILITIES = 0;
	constexpr int32 AUGMENT_STONE_ABILITIES = 1;
}

/** What the rules can check for this loadout. An unset function means "not checked". */
struct WORLD_OF_REFRACTION_API FLoadoutRuleContext
{
	ECharacterClass CharacterClass = ECharacterClass::Generic;

	/** The character is Broken Darkness (D-073: BD rules follow the character, not the authored pools). */
	bool bBrokenDarkness = false;

	/** Mastery discount on spell weights (SpellPoolConstants::SpellSlotDiscount). */
	int32 SpellDiscount = 0;

	/** False skips the spell weight budgets (Q-161: an inventory asset on its own reports them as warnings). */
	bool bCheckSpellBudget = true;

	TFunction<bool(const UWeaponData *)> OwnsWeapon;
	TFunction<bool(const URingData *)> OwnsRing;
	TFunction<bool(const UEvolutionItemData *)> OwnsEvolution;
	TFunction<bool(const UAbilityData *)> KnowsAbility;
	TFunction<bool(const USpellData *)> KnowsSpell;
	TFunction<bool(ESpellElement)> CanCastElement;
};

/** D-110 (Crown, 2026-10-29): what each class may carry. Each class is a budget plus limits; costs are one table for
 *  every class (FLoadoutRules::SlotCost). The evolution seat (an evolution on its own) uses up an item. */
struct FLoadoutClassLimits
{
	int32 Budget = 0;
	int32 MaxItems = 0;
	int32 MaxWeapons = 0;
	int32 MaxAdapters = 0;
};

namespace LoadoutClassTable
{
	/** A limit that doesn't apply (the Resonator's adapter count, CL-11). */
	constexpr int32 NO_LIMIT = INDEX_NONE;

	/** Refractor: budget 3, one item (a weapon, an adapter or an evolution) beside its innate pool. */
	constexpr FLoadoutClassLimits REFRACTOR{3, 1, 1, 1};
	/** Generic: budget 6, two items: up to two weapons, and it keeps one adapter (D-110 B). */
	constexpr FLoadoutClassLimits GENERIC{6, 2, 2, 1};
	/** Resonator: budget 5 (CL-10), one weapon, any number of adapters. */
	constexpr FLoadoutClassLimits RESONATOR{5, NO_LIMIT, 1, NO_LIMIT};

	inline FLoadoutClassLimits Get(ECharacterClass Class)
	{
		switch (Class)
		{
		case ECharacterClass::Caster:
			return REFRACTOR;
		case ECharacterClass::Resonator:
			return RESONATOR;
		case ECharacterClass::Generic:
		default:
			return GENERIC;
		}
	}
}

struct WORLD_OF_REFRACTION_API FLoadoutRules
{
	/** D-110: one cost for every class. Weapon: bare 1 / with a crystal, stone or fusion 2 / with an evolution 3.
	 *  Adapter: 1 / with an evolution 2 (InventoryConstants). The evolution seat is EVOLUTION_PRIMARY_SLOT_COST. */
	static int32 SlotCost(const FLoadoutSlot &Slot);

	/** D-110: the class's budget, item count and per-kind limits over the unified slots (B-302's mirror, rebuilt
	 *  here from the old fields so it is never stale). A Resonator's budget finding stays RingOverflow (cleared by
	 *  dropping adapters); another class's is a diagnostic its slot shapes can't reach today. */
	static TArray<FInvalidSlotFinding> CollectClassLimitFindings(const FCombatLoadout &Loadout, ECharacterClass Class);

	/** Every rule the fight enforces, on the fight's loadout shape. */
	static TArray<FInvalidSlotFinding> CollectFindings(const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context);

	/** Problems only an authored loadout can have: CreateFromSavedLoadout drops them silently
	 *  (a slot type with nothing in it, a Resonator Ring primary, an empty ring slot, extra item slots). */
	static TArray<FString> CollectAuthoredShapeErrors(const FSavedLoadout &Saved);

	static TArray<FString> ToReasons(const TArray<FInvalidSlotFinding> &Findings);

	/** Drops utility crystals from the item slots until every tier band is within its cap
	 *  (D-072, D-077): the last slots in the band lose crystals first. */
	static void TrimUtilityCrystalsToTierCaps(FCombatLoadout &Loadout);
};
