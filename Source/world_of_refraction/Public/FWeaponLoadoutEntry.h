// FWeaponLoadoutEntry.h
// Weapon loadout entry - weapon equipped for combat with assigned abilities and spells
//
// ARCHITECTURE:
// FWeaponInventoryEntry = What you OWN (weapon + crystal state)
// FWeaponLoadoutEntry = How you USE IT (which abilities/spells assigned)
//
// Same weapon can have different loadout configurations (multiple saved loadouts)

#pragma once

#include "CoreMinimal.h"
#include "LoadoutConstants.h"
#include "FWeaponInventoryEntry.h"
#include "FWeaponLoadoutEntry.generated.h"

class UAbilityData;
class USpellData;
class UStanceData;
class UWeaponAttackData;

/**
 * FWeaponLoadoutEntry
 * Weapon configured for combat with assigned abilities and spells
 *
 * Ability slots: 6 total, first N locked by weapon's PresetAbilities
 * Spell slots: 6 total, only if weapon has crystal attached
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWeaponLoadoutEntry
{
    GENERATED_BODY()

    /** The weapon from inventory (includes crystal/evolution state) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FWeaponInventoryEntry WeaponEntry;

    /** Assigned abilities (max 6, first N may be locked) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    TArray<UAbilityData *> AssignedAbilities;

    /** Assigned spells (max 6, only valid if weapon has crystal).
     *  Sequential override list — see GetAllSpells(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData *> AssignedSpells;

    /** Per-loadout attack override. If set, replaces the weapon asset's
     *  WeaponAttack for this entry in combat. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    UWeaponAttackData *OverrideAttack = nullptr;

    /** Use this weapon's own parry animation instead of the character's.
     *  Per-equipped-weapon decision. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    bool bUseWeaponParryAnimation = true;

    /** Per-loadout stance override. When set, ULoadoutComponent::GetCurrentStance
     *  returns this asset instead of the weapon's WeaponStance while this entry
     *  is the actively-shown weapon. Strict override semantics: nullptr means
     *  "use the weapon's default stance". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    UStanceData *StanceOverride = nullptr;

    // ==================== VALIDATION ====================

    /** Check if this loadout entry is valid */
    bool IsValid() const
    {
        return WeaponEntry.IsValid();
    }

    /** Check if weapon can have spells (has crystal or evolution) */
    bool CanHaveSpells() const
    {
        return WeaponEntry.CanCastSpells();
    }

    /** Get number of locked ability slots (from weapon's preset) */
    int32 GetLockedAbilityCount() const;

    /** Get number of customizable ability slots */
    int32 GetCustomizableAbilityCount() const
    {
        return LoadoutConstants::MAX_WEAPON_ABILITIES - GetLockedAbilityCount();
    }

    // ==================== ABILITY ACCESS ====================

    /** Get all abilities (locked + customizable) */
    TArray<UAbilityData *> GetAllAbilities() const;

    /** Get only the locked/preset abilities */
    TArray<UAbilityData *> GetLockedAbilities() const;

    /** Get only the customizable abilities */
    TArray<UAbilityData *> GetCustomizableAbilities() const;

    /** Check if ability slot is locked */
    bool IsAbilitySlotLocked(int32 SlotIndex) const
    {
        return SlotIndex < GetLockedAbilityCount();
    }

    // ==================== SPELL ACCESS ====================

    /** Get all spells: inventory-entry spells as the base list, with
     *  AssignedSpells applied as a sequential override (replace base slots
     *  in order, then append any remainder). Capped at MAX_SPELL_SLOTS. */
    TArray<USpellData *> GetAllSpells() const;

    /** Get spell count (delegates to inventory entry) */
    int32 GetSpellCount() const
    {
        return WeaponEntry.GetSpellCount();
    }

    // ==================== VALIDATION HELPERS ====================

    /** Validate abilities match weapon type and are owned */
    bool ValidateAbilities(const struct FAbilityCollection &OwnedAbilities) const;

    /** Validate spells match crystal element and are owned */
    bool ValidateSpells(const struct FSpellCollection &OwnedSpells) const;

    /** Full validation */
    bool Validate(const FAbilityCollection &OwnedAbilities, const FSpellCollection &OwnedSpells) const
    {
        return ValidateAbilities(OwnedAbilities) && ValidateSpells(OwnedSpells);
    }

    // ==================== INITIALIZATION ====================

    /** Initialize from weapon, copying preset abilities */
    void InitializeFromWeapon();

    /** Clear all assignments */
    void Clear()
    {
        WeaponEntry = FWeaponInventoryEntry();
        AssignedAbilities.Empty();
        AssignedSpells.Empty();
        OverrideAttack = nullptr;
        bUseWeaponParryAnimation = true;
        StanceOverride = nullptr;
    }
};
