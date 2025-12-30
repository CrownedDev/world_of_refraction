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

    // ==================== PRIMARY EQUIPMENT ====================
    /** Does this character start combat with weapon drawn? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    bool bUsingPrimary = true;

    /** Primary slot type (Weapon/Ring/Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    EPrimarySlotType PrimarySlotType = EPrimarySlotType::Weapon;

    /** Primary weapon (when PrimarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FWeaponLoadoutEntry PrimaryWeapon;

    /** Primary ring (Generic/Caster only, when PrimarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    FRingLoadoutEntry PrimaryRing;

    /** Primary evolution crystal (when PrimarySlotType == Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Primary")
    UItemData *PrimaryEvolution = nullptr;

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

    /** Innate spells (Caster only - max 24) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData *> InnateSpells;

    // ==================== ITEMS ====================

    /** Item slots (max 6, 3 uses each) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
    TArray<FItemLoadoutSlot> ItemSlots;

    // ==================== COSMETICS ====================

    /** Unarmed stance (fallback when no weapon equipped) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
    UStanceData *UnarmedStance = nullptr;
    // In the COSMETICS section, add:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
    UInfusionDisplayData *InfusionDisplay = nullptr;

    // ==================== DEFENSE ANIMATIONS ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    UAnimMontage *DodgeLeftMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    UAnimMontage *DodgeRightMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    UAnimMontage *BlockMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    UAnimMontage *ParryMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bUseWeaponParryAnimation = false;

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