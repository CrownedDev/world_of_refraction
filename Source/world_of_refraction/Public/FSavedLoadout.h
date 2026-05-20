// FSavedLoadout.h
// Designer-authored loadout configuration stored inline on a UInventoryData
// asset. Mirrors the fields on ULoadoutData but lives inside an asset's
// SavedLoadouts[] array — no separate UPrimaryDataAsset per loadout.
//
// FCombatLoadout::CreateFromSavedLoadout inflates this into the runtime
// FCombatLoadout (per-instance crystal state, item-use counters, etc).
//
// TODO (delete-ULoadoutData commit): FResonatorRingSlot currently lives in
// LoadoutData.h. Before that file is deleted, extract FResonatorRingSlot to
// its own header (or move it next to FRingLoadoutEntry) and drop the
// LoadoutData.h include from this file.

#pragma once

#include "CoreMinimal.h"
#include "LoadoutData.h"
#include "FSavedLoadout.generated.h"

class UWeaponData;
class URingData;
class UItemData;
class USpellData;
class UAbilityData;
class UStanceData;

/**
 * FSavedLoadout
 *
 * Inline asset-side loadout configuration. Held by UInventoryData in a
 * SavedLoadouts[] array; runtime code inflates it via
 * FCombatLoadout::CreateFromSavedLoadout at character spawn.
 *
 * Field set mirrors ULoadoutData (minus Description). Class-specific rules
 * match: Generic = primary + optional secondary weapon; Caster = primary +
 * innate spells; Resonator = primary (Weapon/Evolution) + equipped rings.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSavedLoadout
{
    GENERATED_BODY()

    // ==================== IDENTITY ====================

    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    FString LoadoutName = TEXT("Unnamed Loadout");

    /** Which character class this loadout is designed for */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    ECharacterClass RequiredClass = ECharacterClass::Generic;

    /** If true, character starts combat showing primary equipment stance. If false, starts showing secondary (Generic) or unarmed (Caster/Resonator). Visual/stance flag — does not gate combat capability. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity", meta = (DisplayName = "Starts Showing Primary"))
    bool bShowPrimary = true;

    // ==================== CASTER INNATE SPELLS ====================

    /** Innate spells (Caster only - up to 24, must match InnateElement).
     *  For a Broken Darkness template this is the Darkness pool (max 6). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Caster Spells",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    TArray<USpellData *> InnateSpells;

    /** Broken Darkness per-element spell pools (Fire/Water/Earth/Wind/Light/
     *  Lightning/Void). Authored for BD enemy templates; leave empty for a
     *  normal Caster. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Caster Spells",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    TArray<FBDElementSpellPool> BDSpellPools;

    // ==================== RESONATOR RINGS ====================

    /** Equipped rings (Resonator only - 5 normal, 3 if evolved).
     *  Each slot pairs a ring with its per-loadout spell overrides. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Resonator Rings",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Resonator", EditConditionHides))
    TArray<FResonatorRingSlot> EquippedRings;

    // ==================== PRIMARY EQUIPMENT ====================

    /** Primary slot type (Generic/Caster: Weapon/Ring/Evolution, Resonator: Weapon/Evolution only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Resonator", EditConditionHides))
    EPrimarySlotType PrimarySlotType = EPrimarySlotType::Weapon;

    /** Primary weapon (when PrimarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    UWeaponData *PrimaryWeapon = nullptr;

    /** Primary ring (Generic/Caster only, when PrimarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Resonator && PrimarySlotType == EPrimarySlotType::Ring", EditConditionHides))
    URingData *PrimaryRing = nullptr;

    /** Primary evolution crystal (when PrimarySlotType == Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Evolution", EditConditionHides))
    UItemData *PrimaryEvolution = nullptr;

    // ==================== PRIMARY EQUIPMENT CONFIGURATION ====================

    /** Abilities assigned to primary weapon (max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> PrimaryWeaponAbilities;

    /** Evolution spells (player-found world drops with RequiredEvolutionCrystal validation) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Evolution", EditConditionHides))
    TArray<USpellData *> EvolutionSpells;

    /** Override primary weapon stance (nullptr = use weapon default).
     *  NOTE: Runtime propagation not wired up in commit 1 — declared here so
     *  the field exists for designer authoring; a downstream commit adds a
     *  matching field on FWeaponLoadoutEntry and copies through in
     *  FCombatLoadout::CreateFromSavedLoadout. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    UStanceData *PrimaryWeaponStanceOverride = nullptr;

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    /** Secondary slot type (Generic only - None or Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic", EditConditionHides))
    ESecondarySlotType SecondarySlotType = ESecondarySlotType::None;

    /** Secondary weapon (Generic only, when SecondarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    UWeaponData *SecondaryWeapon = nullptr;

    // ==================== SECONDARY WEAPON CONFIGURATION (Generic only) ====================

    /** Abilities assigned to secondary weapon (Generic only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary|Config",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> SecondaryWeaponAbilities;

    /** Override secondary weapon stance (Generic only, nullptr = use weapon default).
     *  NOTE: Runtime propagation not wired up in commit 1 — see
     *  PrimaryWeaponStanceOverride. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary|Config",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    UStanceData *SecondaryWeaponStanceOverride = nullptr;

    // ==================== ITEMS ====================

    /** Equipped item crystals (max 6 slots, 3 uses each) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Items")
    TArray<UItemData *> EquippedItems;

    // ==================== VALIDATION ====================

    /** Check if loadout is valid for given class */
    bool IsValidForClass(ECharacterClass CharacterClass) const;

    /** Struct-internal validation errors (does not cross-check inventory
     *  ownership — that happens at UInventoryData level). */
    TArray<FString> GetValidationErrors() const;

    /** Quick check if loadout has any internal errors */
    bool HasValidationErrors() const;

    // ==================== ACCESSORS ====================

    /** Get all spells available from this loadout */
    TArray<USpellData *> GetAllSpells() const;

    /** Get all abilities available from this loadout */
    TArray<UAbilityData *> GetAllAbilities() const;

    /** Get primary weapon (nullptr if using ring or evolution) */
    UWeaponData *GetPrimaryWeapon() const;

    /** Get primary ring (nullptr if using weapon or evolution) */
    URingData *GetPrimaryRing() const;

    /** Get primary evolution (nullptr if using weapon or ring) */
    UItemData *GetPrimaryEvolution() const;

    /** Check if this loadout uses evolution */
    bool IsEvolutionLoadout() const { return PrimarySlotType == EPrimarySlotType::Evolution; }
};
