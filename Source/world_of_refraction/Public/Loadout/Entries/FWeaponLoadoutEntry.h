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
#include "Loadout/LoadoutConstants.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "FWeaponLoadoutEntry.generated.h"

class UAbilityData;
class USpellData;
class USkillDataBase;

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

    /** OBSOLETE (B-351, D-145; delete in the D-124 sweep): the position-merge override list. Read only while the
     *  weapon's one skill-slot list is not active (WeaponEntry.bSkillSlotsActive). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    TArray<UAbilityData *> AssignedAbilities;

    /** OBSOLETE (B-351, D-145; delete in the D-124 sweep): an Ability Stone adds slots to the one list instead.
     *  Assigned augment-stone abilities (max 6). Separate channel from
     *  AssignedAbilities — surfaced under the Resonate Weapon menu, valid only
     *  while a augment stone is attached. Does not touch the main Abilities grid. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    TArray<UAbilityData *> AssignedAugmentStoneAbilities;

    /** Assigned spells (max 6, only valid if weapon has crystal).
     *  Sequential override list — see GetAllSpells(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<USpellData *> AssignedSpells;

    /** Per-loadout attack override. If set, replaces the weapon asset's
     *  WeaponAttack for this entry in combat. Base type (attack/ability merge) holding a UAbilityData
     *  with bIsAttack=true. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    USkillDataBase *OverrideAttack = nullptr;

    /** Use this weapon's own parry animation instead of the character's.
     *  Per-equipped-weapon decision. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overrides")
    bool bUseWeaponParryAnimation = true;

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

    /** OBSOLETE (B-351, D-145; delete in the D-124 sweep): built-ins are never locked; the player can replace any.
     *  Get number of locked ability slots (from weapon's preset) */
    int32 GetLockedAbilityCount() const;

    // ==================== ONE SKILL-SLOT LIST (B-351, D-145) ====================

    /** Slots on this weapon: its tier's count (F 1 .. S 6) plus an Ability Stone's slots. */
    int32 GetSkillSlotCount() const;

    /** The list slot by slot (null = an empty slot): the pick, else the built-in at that position. */
    TArray<UAbilityData *> GetSkillSlots() const;

    /** The abilities the weapon offers: the list's filled slots while it is active, else the pre-D-145 merge. */
    TArray<UAbilityData *> GetSkillSlotAbilities() const;

    /** Replace what Slot holds (a built-in or an empty slot) with Ability; activates the list. False past the count. */
    bool ReplaceSkill(int32 Slot, UAbilityData *Ability);

    /** Bring back the built-in (or empty) Slot held before a replace; activates the list. False if nothing was replaced. */
    bool RestoreSkill(int32 Slot);

    /** Get number of customizable ability slots. The cap keys on the weapon's own
     *  tier (CrystalEffectTable::SlotsForContainerTier); MAX_WEAPON_ABILITIES is the
     *  no-weapon fallback ceiling. Defined in the .cpp — keeps WeaponData +
     *  CrystalEffectTable includes out of this header. */
    int32 GetCustomizableAbilityCount() const;

    // ==================== ABILITY ACCESS ====================

    /** OBSOLETE (B-351, D-145; delete in the D-124 sweep): the position merge. Get all abilities (locked + customizable) */
    TArray<UAbilityData *> GetAllAbilities() const;

    /** OBSOLETE (B-351, D-145; delete in the D-124 sweep): the separate stone list.
     *  Get assigned augment-stone abilities (Resonate Weapon menu). Empty unless a
     *  augment stone is attached. No preset merge — augment stones are asset-less. */
    TArray<UAbilityData *> GetAugmentStoneAbilities() const;

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

    /** GetAllSpells before the slot cap: the spells over the cap are what a fight drops (B-196). */
    TArray<USpellData *> GetAllSpellsUncapped() const;

    /** Spell slots the attached gem / evolution (Q-147) or the weapon's tier allows. */
    int32 GetSpellSlotCap() const;

    /** Spells the cap drops. */
    int32 GetSpellsOverCap() const;

    /** Get spell count (delegates to inventory entry) */
    int32 GetSpellCount() const
    {
        return WeaponEntry.GetSpellCount();
    }

    // ==================== VALIDATION HELPERS ====================

    /** Validate abilities match weapon type and are owned */
    bool ValidateAbilities(const struct FAbilityCollection &OwnedAbilities) const;

    /** Validate augment-stone abilities match weapon type and are owned. With no
     *  augment stone attached, valid only when none are assigned. */
    bool ValidateAugmentStoneAbilities(const struct FAbilityCollection &OwnedAbilities) const;

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
        AssignedAugmentStoneAbilities.Empty();
        AssignedSpells.Empty();
        OverrideAttack = nullptr;
        bUseWeaponParryAnimation = true;
    }
};
