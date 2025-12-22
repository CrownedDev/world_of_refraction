// FCombatLoadout.h
// Full combat loadout - all equipment, spells, abilities, and items for battle
//
// ARCHITECTURE:
// This is what gets SAVED as a loadout configuration.
// Player can have multiple saved loadouts and switch between them.
// At battle start, active loadout is loaded into LoadoutComponent for runtime tracking.

#pragma once

#include "CoreMinimal.h"
#include "InventoryConstants.h"
#include "LoadoutConstants.h"
#include "ECharacterClass.h"
#include "SpellSchool.h"
#include "FWeaponLoadoutEntry.h"
#include "FRingLoadoutEntry.h"
#include "FItemLoadoutSlot.h"
#include "FCombatLoadout.generated.h"

class USpellData;
class UInventoryComponent;
class ULoadoutData;

/**
 * FCombatLoadout
 * Complete loadout configuration for combat
 *
 * Class-specific rules:
 * - Generic: Primary weapon + Secondary (weapon OR ring)
 * - Caster: Primary (weapon OR ring) + 24 innate spells
 * - Resonator: Primary weapon + 5 ring loadout
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

    // ==================== PRIMARY EQUIPMENT ====================

    /** Primary weapon (Generic/Resonator always, Caster if chose weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FWeaponLoadoutEntry PrimaryWeapon;

    /** Primary ring (Caster only, if chose ring over weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FRingLoadoutEntry PrimaryRing;

    /** Is primary slot a ring? (Caster only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    bool bPrimaryIsRing = false;

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    /** Secondary weapon (Generic only, if chose weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Secondary")
    FWeaponLoadoutEntry SecondaryWeapon;

    /** Secondary ring (Generic only, if chose ring) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Secondary")
    FRingLoadoutEntry SecondaryRing;

    /** Is secondary slot a ring? (Generic only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Secondary")
    bool bSecondaryIsRing = false;

    // ==================== RING LOADOUT (Resonator only) ====================

    /** Equipped rings (Resonator only, max 5, max 2 evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Rings")
    TArray<FRingLoadoutEntry> RingLoadout;

    // ==================== CASTER INNATE SPELLS ====================

    /** Innate spells for Caster (24 total, 6 per school, element-matched) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells|Innate")
    TArray<USpellData *> InnateSpells;

    // ==================== ITEMS ====================

    /** Item slots (max 6, no duplicate crystal types) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
    TArray<FItemLoadoutSlot> ItemSlots;

    // ==================== VALIDATION ====================

    /** Validate entire loadout for a character class */
    bool Validate(ECharacterClass CharClass, UInventoryComponent *Inventory) const;

    /** Validate Generic loadout */
    bool ValidateGeneric(UInventoryComponent *Inventory) const;

    /** Validate Caster loadout */
    bool ValidateCaster(UInventoryComponent *Inventory) const;

    /** Validate Resonator loadout */
    bool ValidateResonator(UInventoryComponent *Inventory) const;

    // ==================== INNATE SPELL HELPERS ====================

    /** Get innate spells by school */
    TArray<USpellData *> GetInnateSpellsBySchool(ESpellSchool School) const;

    /** Get innate spell count for school */
    int32 GetInnateSpellCountForSchool(ESpellSchool School) const;

    /** Check if innate spells are valid (max 6 per school) */
    bool ValidateInnateSpells(ESpellElement InnateElement, const struct FSpellCollection &OwnedSpells) const;

    // ==================== ITEM HELPERS ====================

    /** Check for duplicate crystal types in items */
    bool HasDuplicateItemTypes() const;

    /** Get total item uses available */
    int32 GetTotalItemUses() const;

    // ==================== AGGREGATED ACCESS ====================

    /** Get all available abilities from all equipment */
    TArray<UAbilityData *> GetAllAbilities() const;

    /** Get all available spells from all sources */
    TArray<USpellData *> GetAllSpells() const;

    /** Get all usable item slots */
    TArray<FItemLoadoutSlot> GetUsableItemSlots() const;

    // ==================== INITIALIZATION ====================

    /** Clear entire loadout */
    void Clear();

    /** Initialize for class with defaults */
    void InitializeForClass(ECharacterClass CharClass);
};
