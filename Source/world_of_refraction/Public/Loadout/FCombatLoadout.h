// FCombatLoadout.h
// Full combat loadout - all equipment, spells, abilities, and items for battle
//
// ARCHITECTURE:
// This is what gets SAVED as a loadout configuration.
// Player can have multiple saved loadouts and switch between them.
// At battle start, active loadout is loaded into LoadoutComponent for runtime tracking.
//
// CLASS RULES:
// - Generic: Primary (Weapon/Ring/Evolution) + Secondary (Weapon only)
// - Caster: Primary (Weapon/Ring/Evolution) + Innate spells
// - Resonator: Primary (Weapon/Evolution) + Ring loadout (5 normal, 3 evolved)

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutConstants.h"
#include "Character/ECharacterClass.h"
#include "Equipment/Weapons/EWeaponSlotType.h"
#include "Skills/Definitions/SpellSchool.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/Crystals/FEvolutionAttachment.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Loadout/FLoadoutSlot.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "FCombatLoadout.generated.h"

class USpellData;
class UAbilityData;
class UInventoryComponent;
class UEvolutionItemData;
class UStanceData;
class UInfusionDisplayData;
struct FSavedLoadout;
struct FWornGear;

/**
 * FBDElementSpellPool
 * One Broken Darkness element spell pool. Holds up to 6 spells of a single
 * element and is locked until that element has been absorbed in combat
 * (UBrokenDarknessManager::HasAbsorbedElement). The Darkness pool is NOT stored
 * here — it lives in FCombatLoadout::InnateSpells and is always available.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBDElementSpellPool
{
    GENERATED_BODY()

    /** Element this pool channels (Fire/Water/Earth/Wind/Light/Lightning/Void) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    ESpellElement Element = ESpellElement::Generic;

    /** Spells in this pool (max 6, every entry must match Element) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData *> Spells;
};

/**
 * FCombatLoadout
 * Complete loadout configuration for combat
 *
 * Class-specific rules:
 * - Generic: Primary (Weapon/Ring/Evolution) + Secondary (Weapon only)
 * - Caster: Primary (Weapon/Ring/Evolution) + 24 innate spells
 * - Resonator: Primary (Weapon/Evolution) + ring loadout
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCombatLoadout
{
    GENERATED_BODY()

    /** Loadout name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString LoadoutName = TEXT("Default Loadout");

    // ==================== FACTORY ====================

    /** Build a runtime combat loadout from a designer-authored FSavedLoadout
     *  struct (inline on UInventoryData). Zero-context form: delegates to the
     *  context overload below with null inventories, so every instance ref takes
     *  the asset fallback — byte-identical to the pre-shape-B behaviour. */
    static FCombatLoadout CreateFromSavedLoadout(const FSavedLoadout &SavedLoadout);

    /** Context overload (shape B): when a slot's instance ref (FGuid) is valid
     *  AND found in the owned inventories — PersistentID for weapon/ring,
     *  InstanceID for evolution — the owned entry is copied wholesale into the
     *  combat loadout (carrying its roll, pools, and guid) instead of being
     *  rebuilt from the asset. Invalid or unfound refs fall back to the asset
     *  build (today's path). SavedLoadout-sourced per-slot config (abilities,
     *  stances, spell overrides) is applied identically on either branch.
     *  Null contexts disable resolution entirely (= the zero-context form). */
    static FCombatLoadout CreateFromSavedLoadout(const FSavedLoadout &SavedLoadout,
                                                 const class UInventoryComponent *OwnedInventory,
                                                 const class UEvolutionInventoryComponent *OwnedEvolutions);

    /** Refill item slots from the owner's UCrystalInventoryComponent when
     *  bAutoEquipItemsOnCombatStart is true. No-op when the flag is false,
     *  the owner is null, or the owner has no UCrystalInventoryComponent.
     *  Fills each non-full slot up to MAX_QUANTITY_PER_ITEM_SLOT (3),
     *  debiting the inventory via RemoveCrystal. Called from
     *  ULoadoutComponent::PrepareForBattle.
     *  OBSOLETE (B-313, Q-155): no longer called for a member with a pouch (fights use the pouch as it is); a
     *  rest-spot refill of the pouch replaces it. */
    static void ApplyAutoEquip(FCombatLoadout &Loadout, AActor *OwningActor);

    /** B-311 (D-111): back to the authored shape, with each piece referenced by its owned GUID, so a custom loadout
     *  can be kept and rebuilt by CreateFromSavedLoadout. Carried items are not kept (D-111: the pouch holds them). */
    FSavedLoadout ToSavedLoadout(ECharacterClass CharacterClass) const;

    /** D-221 (B-505): the record of what this loadout wears, in the WornGearLayout places (WornGear.h). The ACTIVE
     *  entry is the weapon drawn: a Generic's by bShowPrimary (its secondary when the primary is no weapon), anyone
     *  else's primary weapon when shown. The wheel pick is left default (the pool keeps it apart). */
    FWornGear ToWornGear(ECharacterClass CharacterClass) const;

    /** D-224 (B-517): each entry's picks (by GearID) onto this loadout's copies of the pieces, replacing whatever the
     *  copies held; a piece with no entry gets none. Spell picks need a spell source in the socket. */
    void ApplyEntryPicks(const TArray<FLoadoutSlot> &Entries);

    /** Q-518 (B-518): a piece with no picks fills from the known skills, in order, up to its slot count: a weapon
     *  with the abilities it can hold, a ring with a spell source with the spells of its element (not the innate
     *  ones). For enemies only, until they carry their own skills. TODO: remove in stage 5. */
    void FillEmptyPicksFromKnown(const TArray<UAbilityData *> &KnownAbilities, const TArray<USpellData *> &KnownSpells);

    /** D-221 (B-505): the loadout a record wears, built through CreateFromSavedLoadout. Each piece is taken from the
     *  member's drawn copies (OwnedInventory / OwnedEvolutions; apparel from the pool), so a piece not drawn (gone, or
     *  left at home for a run) leaves its place empty instead of being built from its asset. bShowPrimary follows the
     *  ACTIVE entry; with no weapon primary it can't tell, and FallbackShowPrimary (the lore loadout's) is kept. */
    static FCombatLoadout CreateFromWornGear(const FWornGear &Worn, ECharacterClass CharacterClass,
                                             const class UInventoryComponent *OwnedInventory,
                                             const class UEvolutionInventoryComponent *OwnedEvolutions,
                                             bool bFallbackShowPrimary = true);

    /** B-311 (D-111): a lore loadout (the character asset's own) is rebuilt from the asset each spawn and never kept;
     *  a custom one (the player's) is kept per character. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "1. Identity")
    bool bLore = false;

    /** B-347b (Q-347): a custom loadout drawn during a run that names a home piece the run left at home: the stored
     *  version, written back as it was (this copy had that slot emptied). Moves with the loadout; not saved. */
    TSharedPtr<const FSavedLoadout> HeldHomeLoadout;

    // ==================== PRIMARY EQUIPMENT ====================
    /** Does this character start combat with weapon drawn? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    bool bShowPrimary = true;

    /** Primary slot type (Weapon/Ring/Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    EPrimarySlotType PrimarySlotType = EPrimarySlotType::None;

    /** Primary weapon (when PrimarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FWeaponLoadoutEntry PrimaryWeapon;

    /** Primary ring (Generic/Caster only, when PrimarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FRingLoadoutEntry PrimaryRing;

    /** Active evolution item slotted in the primary loadout slot
     *  (when PrimarySlotType == Evolution). FEvolutionAttachment wraps the
     *  item asset pointer plus current durability for wear tracking. No
     *  InstanceID — slotted evolution items are destroyed on removal, so
     *  no instance-retrieval pathway is needed. Access the item asset via
     *  .Item; LoadoutComponent::GetPrimaryEvolution() preserves the BP-facing
     *  UEvolutionItemData* signature. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FEvolutionAttachment PrimaryEvolution;

    /** InstanceID of the owned FEvolutionInventoryEntry this primary slot was inflated from
     *  (retained from FSavedLoadout::PrimaryEvolutionInstance — previously read once and dropped).
     *  Lets runtime removal/break paths resolve the owned entry to dismantle. Invalid when the
     *  primary slot is not an instance-resolved evolution. Runtime-derived state, not serialized. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FGuid PrimaryEvolutionInstance;

    /** Spells selected from evolution (max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    TArray<USpellData *> EvolutionSpells;

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    /** Secondary slot type (None or Weapon - Ring removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Secondary")
    ESecondarySlotType SecondarySlotType = ESecondarySlotType::None;

    /** Secondary weapon (Generic only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Secondary")
    FWeaponLoadoutEntry SecondaryWeapon;

    // ==================== RESONATOR RINGS ====================

    /** Ring loadout (Resonator only - 5 normal, 3 if evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Rings")
    TArray<FRingLoadoutEntry> RingLoadout;

    // ==================== CASTER INNATE SPELLS ====================

    /** Innate spells (Caster only - max 24).
     *  For Broken Darkness this is the always-available Darkness pool (max 6). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData *> InnateSpells;

    /** Broken Darkness per-element spell pools (Fire/Water/Earth/Wind/Light/
     *  Lightning/Void). Each pool is locked until its element is absorbed.
     *  Empty / unused for non-BD characters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<FBDElementSpellPool> BDSpellPools;

    // ==================== ITEMS ====================

    /** Item slots (max 6, max 3 Quantity per slot, unique by ECrystalType)
     *  OBSOLETE (B-313, D-111, D-117): a party member's fight items are its own pouch (ULoadoutComponent::
     *  GetUsableItems / UseCarriedItem); these slots are read only for an actor with no pouch (enemies) and for
     *  the preset rules / picks still on them (B-313b). Delete after Crown's PIE pass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
    TArray<FItemLoadoutSlot> ItemSlots;

    /** Propagated from FSavedLoadout at CreateFromSavedLoadout. When true,
     *  ApplyAutoEquip refills item slots from UCrystalInventoryComponent at
     *  combat start (called by ULoadoutComponent::PrepareForBattle). */
    UPROPERTY(BlueprintReadWrite, Category = "Items")
    bool bAutoEquipItemsOnCombatStart = false;

    // ==================== RUNTIME STATE ====================

    /** Active ring index for Resonator */
    UPROPERTY(BlueprintReadWrite, Category = "Runtime")
    int32 ActiveRingIndex = 0;

    // ==================== APPAREL (B-380, D-164) ====================

    /** Apparel worn with this loadout: outside the budget, up to ApparelRules::MAX_APPAREL_SLOTS (add through
     *  ApparelRules::AddApparel). Not in the Slots mirror (the budget walks Slots). Saved by pool id in custom loadouts (B-380c, save v29); lore loadouts not yet (B-380d). */
    UPROPERTY(BlueprintReadWrite, Category = "Apparel")
    TArray<FApparelEntry> Apparel;

    // ==================== UNIFIED SLOTS (D-110, mirror) ====================
    // B-302: the weapons and adapters above as one slot list. B-309d4: the writers change the slots and rebuild the old
    // fields from them (RebuildLegacyFromSlots); the slots are built from the old fields only where a loadout is authored
    // in them (a save, a preset, a spec: BuildSlotsFromOldFields). wor.Loadout.CompareSlots prints both sides.

    /** Order: the primary weapon or adapter, the secondary weapon, then the Resonator's adapters. */
    UPROPERTY(BlueprintReadOnly, Category = "Runtime|Slots")
    TArray<FLoadoutSlot> Slots;

    /** The weapon in hand, INDEX_NONE for none (mirrors ULoadoutComponent::GetActiveWeapon). */
    UPROPERTY(BlueprintReadOnly, Category = "Runtime|Slots")
    int32 ActiveWeaponSlot = INDEX_NONE;

    /** The adapter in use, INDEX_NONE for none (the Resonator's active ring, or a primary ring). */
    UPROPERTY(BlueprintReadOnly, Category = "Runtime|Slots")
    int32 ActiveAdapterSlot = INDEX_NONE;

    /** B-309d4 (was RebuildSlotsFromLegacy): the load-time builder. Builds Slots, each in its WornGearLayout place, and
     *  the two active indices from the old fields, by the character's class: only a Generic's bShowPrimary picks which
     *  of two weapons is in hand, and only a Resonator's adapter loadout has an active one (everyone else's adapter is
     *  the primary one), as GetActiveWeapon / GetActiveRing / GetPrimaryRing. For a loadout authored in the old fields
     *  (a save, a preset, a spec) and EditSlots' way in. TODO: goes once B-309c (the save stores the slots) is merged
     *  and B-309f moves the authored presets. */
    void BuildSlotsFromOldFields(ECharacterClass CharacterClass);

    /** B-309d4: the two active indices again from Slots (bShowPrimary or ActiveRingIndex moved); Slots unchanged. */
    void RefreshActiveSlots(ECharacterClass CharacterClass);

    /** B-304, by place since B-309d4: the active loadout's read-side guard. Builds the slots again (BuildSlotsFromOldFields)
     *  only if they no longer hold the old fields' items (same place, kind, asset and GUID, in place order), else
     *  refreshes the active indices. Cheap: no copies when in step. TODO: goes with BuildSlotsFromOldFields; until then
     *  a loadout authored in the old fields (91 spec cases, the presets) reads right (B-309d4 measured). */
    void EnsureSlotsInStep(ECharacterClass CharacterClass);

    /** B-309d: the way back, the old fields rebuilt from Slots by each slot's Place (one direction, for the writers
     *  that move onto the slots): the primary weapon or adapter, the secondary weapon, the adapter loadout by index
     *  (an empty place before a ring stays empty; empty places after the last ring go, and an ActiveRingIndex there
     *  finds no adapter, as before). A filled place sets its type field (a weapon also counts under None); an empty
     *  one keeps it. What the mirror never held (a non-Generic's secondary weapon, a Caster's adapter loadout) is
     *  cleared. Then the two active indices. */
    void RebuildLegacyFromSlots(ECharacterClass CharacterClass);

    /** B-309d2: the writers' way in. Brings Slots fully in step with the old fields (a full rebuild: an in-place edit of
     *  an old entry outside EditSlots is not otherwise seen), runs Edit, which changes the slots (and the fields that are
     *  not slots: the evolution primary, bShowPrimary, the type fields), then rebuilds the old fields from the slots.
     *  An old entry the class's mirror never holds (a weapon under a Ring or Evolution primary, a non-Generic's second
     *  weapon, a Caster's adapter loadout), or one its type would mirror but no slot held (picks with no gear, B-309d3), is
     *  kept as the edit left it; an edit that means to drop one clears it itself.
     *  The first build goes with BuildSlotsFromOldFields (B-309c, B-309f). */
    void EditSlots(ECharacterClass CharacterClass, TFunctionRef<void(FCombatLoadout &)> Edit);

    /** B-309d3: Edit on each weapon and adapter entry the loadout holds, with its WornGearLayout place (one of the two
     *  pointers set), inside EditSlots: a slot's entry is edited and put back in its place (an entry left empty leaves
     *  the place); an old entry no slot mirrors (empty ones too) is edited where it is, which EditSlots keeps. */
    void EditGearEntries(ECharacterClass CharacterClass, TFunctionRef<void(int32 Place, FWeaponLoadoutEntry *, FRingLoadoutEntry *)> Edit);

    /** B-309d2: the slot in a WornGearLayout place, or null. */
    FLoadoutSlot *FindSlotAt(int32 Place);
    const FLoadoutSlot *FindSlotAt(int32 Place) const;

    /** B-309d2: puts Slot in Place, replacing what is there, Slots kept in place order (the mirror's order). */
    FLoadoutSlot &PutSlotAt(int32 Place, const FLoadoutSlot &Slot);

    /** B-309d2: takes the slot out of Place; false when the place was empty. */
    bool ClearSlotAt(int32 Place);

    /** B-304: the old entry behind a slot, by its place and kind (B-309d4), or null (the getters that still return the
     *  old entry types). */
    const FWeaponLoadoutEntry *GetLegacyWeaponEntry(int32 SlotIndex) const;
    const FRingLoadoutEntry *GetLegacyRingEntry(int32 SlotIndex) const;

    // ==================== VALIDATION ====================
    // Live validation lives on ULoadoutComponent::GetValidationErrors —
    // comprehensive inventory ownership + class-rule + element-capability
    // checks, broadcast via OnValidationFailed. The struct-side dispatcher
    // and class-specific Validate* methods were dead code (zero callers) and
    // were removed in feature/integration-gaps-sweep-2; ValidateBDSpellLoadout
    // stays — it's still shared with FSavedLoadout::GetValidationErrors.

    /**
     * Validate a Broken Darkness spell loadout — InnateSpells as the Darkness
     * pool (count <= MAX_EQUIPPED_SLOT_POOL, every spell Darkness element) and
     * BDSpellPools (<= MAX_BD_ELEMENT_POOLS pools, each <= MAX_EQUIPPED_SLOT_POOL
     * spells, every spell matching its pool's element). Structural only — no
     * inventory ownership check.
     *
     * Weight budget (bCheckWeight): when set, also enforces ONE shared point
     * budget across the Darkness pool + EVERY element pool (Σ effective spell
     * cost <= BD_SPELL_BUDGET), using the precomputed Discount (0..3). The asset
     * path (no character) calls with bCheckWeight=false; runtime callers
     * (LoadoutComponent, has character) pass the real discount + true.
     *
     * Returns one error string per violation.
     */
    static TArray<FString> ValidateBDSpellLoadout(
        const TArray<USpellData *> &InnateSpells,
        const TArray<FBDElementSpellPool> &BDSpellPools,
        int32 Discount = 0,
        bool bCheckWeight = false);

    // ==================== ACCESSORS ====================

    /** Get all abilities from this loadout */
    TArray<UAbilityData *> GetAllAbilities() const;

    /** Get all spells from this loadout */
    TArray<USpellData *> GetAllSpells() const;

    /** Get usable item slots (have remaining uses) */
    TArray<FItemLoadoutSlot> GetUsableItemSlots() const;

    /** Get total remaining item uses */
    int32 GetTotalItemUses() const;

    /** Check for duplicate item types */
    bool HasDuplicateItemTypes() const;

    /** Get innate spell count for a school */
    int32 GetInnateSpellCountForSchool(ESpellSchool School) const;

    // ==================== HELPERS ====================

    /** Check if this loadout uses evolution */
    bool IsEvolutionLoadout() const { return PrimarySlotType == EPrimarySlotType::Evolution; }

    /** A weapon in the primary, with a Weapon or None type (AutoPopulateLoadout leaves None): the active rule (B-503q). */
    bool HasPrimaryWeapon() const
    {
        return (PrimarySlotType == EPrimarySlotType::Weapon || PrimarySlotType == EPrimarySlotType::None) && PrimaryWeapon.IsValid();
    }

    /** Check if primary is ring */
    bool HasPrimaryRing() const { return PrimarySlotType == EPrimarySlotType::Ring && PrimaryRing.IsValid(); }

    /** Check if has secondary weapon */
    bool HasSecondaryWeapon() const { return SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid(); }

    /** Clear all loadout data */
    void Clear();

    /** Initialize empty loadout for a class */
    void InitializeForClass(ECharacterClass CharClass);
};