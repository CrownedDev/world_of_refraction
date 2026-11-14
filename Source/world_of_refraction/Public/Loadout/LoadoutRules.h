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
#include "Loadout/LoadoutConstants.h"
#include "LoadoutRules.generated.h"

struct FCombatLoadout;
struct FCrystalId;
struct FLoadoutSlot;
struct FRuntimeAttachedItem;
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
	// out of range (clamped). UtilityCrystalTierCap: too many utility crystals in a tier band (OBSOLETE (B-354):
	// D-143 retired the bands; nothing reports it). EquippedSTierCrystal (B-354, D-143): more than one S-tier crystal
	// equipped; clearable only through the item slots (the gems there are trimmed; attachments stay and keep it reported).
	AbilityOverflow,
	ItemSlot,
	UtilityCrystalTierCap,
	EquippedSTierCrystal,
	// B-472 (D-206): the Refractor's budget finding: its one item's augment stone is dropped (back to the inventory,
	// Q-159 = A) to get under 2.
	PrimaryStone,
	// B-477 (D-209): a weapon set's off hand that may not be there (a pair WeaponSetRules::CheckPair refuses, or a set on
	// a class other than Generic); cleared by emptying the off hand. SubIndex SECONDARY_WEAPON for the secondary set.
	OffHandWeapon
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

	/** B-313b (D-111, Q-313b): the member fights with its pouch, so the loadout's item slots hold no supply (an authored
	 *  pick is an identity only): the item-slot rules skip them. False for an actor with no pouch (enemies, unbound tests). */
	bool bItemsInPouch = false;

	TFunction<bool(const UWeaponData *)> OwnsWeapon;
	TFunction<bool(const URingData *)> OwnsRing;
	TFunction<bool(const UEvolutionItemData *)> OwnsEvolution;
	TFunction<bool(const UAbilityData *)> KnowsAbility;
	TFunction<bool(const USpellData *)> KnowsSpell;
	TFunction<bool(ESpellElement)> CanCastElement;
};

/** D-110 (Crown, 2026-10-29): what each class may carry. Each class is a budget plus limits; costs are one table for
 *  every class (FLoadoutRules::SlotCost, D-206). The evolution seat (an evolution on its own) uses up an item. */
struct FLoadoutClassLimits
{
	int32 Budget = 0;
	int32 MaxItems = 0;
	int32 MaxWeapons = 0;
	int32 MaxAdapters = 0;
};

namespace LoadoutClassTable
{
	/** A limit that doesn't apply (the Resonator's item count). */
	constexpr int32 NO_LIMIT = INDEX_NONE;

	/** Refractor: budget 2 (D-206, was 3), one item (a weapon, an adapter or an evolution) beside its innate pool. */
	constexpr FLoadoutClassLimits REFRACTOR{LoadoutConstants::REFRACTOR_LOADOUT_BUDGET, 1, 1, 1};
	/** Generic: budget 5, three items at most: two weapons and one adapter (D-206; no "one evolved weapon" limit: two
	 *  evolved weapons and an adapter with a crystal, 2 + 2 + 1, fit). */
	constexpr FLoadoutClassLimits GENERIC{LoadoutConstants::LOADOUT_TOTAL_BUDGET, LoadoutConstants::MAX_GENERIC_LOADOUT_ITEMS, 2, 1};
	/** Resonator: budget 5 (CL-10), one weapon, at most 5 adapters (D-206). */
	constexpr FLoadoutClassLimits RESONATOR{LoadoutConstants::LOADOUT_TOTAL_BUDGET, NO_LIMIT, 1, LoadoutConstants::MAX_RESONATOR_ADAPTERS};

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
	/** D-206 (B-472): one cost for every class: the gear is free, only what is in it costs. The crystal socket: a
	 *  crystal or fusion 1, an evolution 2 (an augment stone there 1); the stone socket: a stone 1. So a bare weapon or
	 *  adapter costs 0, either with a crystal 1, with an evolution 2, and a stone adds 1 (LoadoutConstants). The
	 *  evolution seat is EVOLUTION_PRIMARY_SLOT_COST. The pouch capacity costs (InventoryConstants) are separate. */
	static int32 SlotCost(const FLoadoutSlot &Slot);

	/** D-206: what one socket's contents cost in the loadout (0 when empty). */
	static int32 AttachmentCost(const FRuntimeAttachedItem &Attached);

	/** What the class's weapons and evolution seat cost (everything but adapters), over the slots the fight reads.
	 *  The budget finding and ClearInvalidSlots' adapter drop both use it, so they can't disagree. */
	static int32 NonAdapterCost(const FCombatLoadout &Loadout, ECharacterClass Class);

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

	/** B-354 (D-143, Q-354): S-tier crystals equipped in the loadout: each attachment on the gear the class fights with
	 *  (a crystal, an augment stone, a fusion with an S half, an evolution; the secondary for a Generic, the ring loadout
	 *  for a Resonator, as CollectFindings gates them) and the primary evolution, plus each S gem in its item slots
	 *  (bCountItemSlots false: a pouch member's, which hold none, B-313b). */
	static int32 CountEquippedSTierCrystals(const FCombatLoadout &Loadout, ECharacterClass Class, bool bCountItemSlots = true);

	/** B-354: drops the S gems in the item slots that the attached S crystals leave no room for, last slots first. */
	static void TrimSTierItemCrystals(FCombatLoadout &Loadout, ECharacterClass Class);

	/** OBSOLETE (B-354, D-143): drops utility crystals from the item slots until every D-072 band is within its cap. */
	static void TrimUtilityCrystalsToTierCaps(FCombatLoadout &Loadout);

	/** OBSOLETE (B-354, D-143): the D-072 utility-crystal tier band an item counts toward, or INDEX_NONE. */
	static int32 GetUtilityBandIndex(const FCrystalId &Id);

	/** OBSOLETE (B-354, D-143): the D-072 band's cap; 0 for INDEX_NONE or an unknown index. */
	static int32 GetUtilityBandCap(int32 BandIndex);
};
