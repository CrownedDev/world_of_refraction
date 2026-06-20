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
#include "FCombatLoadout.generated.h"

class USpellData;
class UAbilityData;
class UInventoryComponent;
class UEvolutionItemData;
class UStanceData;
class UInfusionDisplayData;
struct FSavedLoadout;

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
     *  debiting the inventory via RemoveItemCount. Called from
     *  ULoadoutComponent::PrepareForBattle. */
    static void ApplyAutoEquip(FCombatLoadout &Loadout, AActor *OwningActor);

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

    /** Item slots (max 6, max 3 Quantity per slot, unique by ECrystalType) */
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

    /** Check if primary is weapon */
    bool HasPrimaryWeapon() const { return PrimarySlotType == EPrimarySlotType::Weapon && PrimaryWeapon.IsValid(); }

    /** Check if primary is ring */
    bool HasPrimaryRing() const { return PrimarySlotType == EPrimarySlotType::Ring && PrimaryRing.IsValid(); }

    /** Check if has secondary weapon */
    bool HasSecondaryWeapon() const { return SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid(); }

    /** Clear all loadout data */
    void Clear();

    /** Initialize empty loadout for a class */
    void InitializeForClass(ECharacterClass CharClass);
};