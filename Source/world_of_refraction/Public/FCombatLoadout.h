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
#include "InventoryConstants.h"
#include "LoadoutConstants.h"
#include "ECharacterClass.h"
#include "EWeaponSlotType.h"
#include "SpellSchool.h"
#include "ESpellElement.h"
#include "FCrystalInventoryEntry.h"
#include "FWeaponLoadoutEntry.h"
#include "FRingLoadoutEntry.h"
#include "FItemLoadoutSlot.h"
#include "FCombatLoadout.generated.h"

class USpellData;
class UAbilityData;
class UInventoryComponent;
class ULoadoutData;
class UItemData;
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

    /** Create FCombatLoadout from LoadoutData asset (for AI enemies) */
    static FCombatLoadout CreateFromAsset(const ULoadoutData *Asset);

    /** Build a runtime combat loadout from a designer-authored FSavedLoadout
     *  struct (inline on UInventoryData). Mirrors CreateFromAsset 1:1; both
     *  factories coexist until ULoadoutData is removed in the final migration
     *  commit. */
    static FCombatLoadout CreateFromSavedLoadout(const FSavedLoadout &SavedLoadout);

    // ==================== PRIMARY EQUIPMENT ====================
    /** Does this character start combat with weapon drawn? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    bool bShowPrimary = true;

    /** Primary slot type (Weapon/Ring/Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    EPrimarySlotType PrimarySlotType = EPrimarySlotType::Weapon;

    /** Primary weapon (when PrimarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FWeaponLoadoutEntry PrimaryWeapon;

    /** Primary ring (Generic/Caster only, when PrimarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FRingLoadoutEntry PrimaryRing;

    /** Primary evolution crystal (when PrimarySlotType == Evolution).
     *  Wrapped in FCrystalInventoryEntry for per-instance durability tracking
     *  (CurrentDurability, InstanceID) — symmetric with weapon/ring crystal
     *  storage. The crystal IS the slot, so no parent wrapper struct is
     *  needed. Access the crystal asset via .Crystal; LoadoutComponent::
     *  GetPrimaryEvolution() preserves the BP-facing UItemData* signature. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FCrystalInventoryEntry PrimaryEvolution;

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

    /** Item slots (max 6, 3 uses each) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
    TArray<FItemLoadoutSlot> ItemSlots;

    // ==================== RUNTIME STATE ====================

    /** Active ring index for Resonator */
    UPROPERTY(BlueprintReadWrite, Category = "Runtime")
    int32 ActiveRingIndex = 0;

    // ==================== VALIDATION ====================

    /** Full validation against class rules and inventory */
    bool Validate(ECharacterClass CharClass, UInventoryComponent *Inventory) const;

    /** Class-specific validation */
    bool ValidateGeneric(UInventoryComponent *Inventory) const;
    bool ValidateCaster(UInventoryComponent *Inventory) const;
    bool ValidateResonator(UInventoryComponent *Inventory) const;

    /**
     * Validate a Broken Darkness spell loadout — InnateSpells as the Darkness
     * pool (<= 6, every spell Darkness element) and BDSpellPools (<= 7 pools,
     * each <= 6 spells, every spell matching its pool's element). Structural
     * only — no inventory ownership check. Shared by FCombatLoadout and
     * ULoadoutData validation. Returns one error string per violation.
     */
    static TArray<FString> ValidateBDSpellLoadout(
        const TArray<USpellData *> &InnateSpells,
        const TArray<FBDElementSpellPool> &BDSpellPools);

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

    /** Reset item uses for battle start */
    void ResetForBattle();
};