// LoadoutData.h
// Pre-configured combat loadout asset for AI enemies and player templates
//
// ARCHITECTURE:
// - AI enemies: Designer creates one LoadoutData per enemy type
// - Players: Use LoadoutComponent.SavedLoadouts[] (built from UI)
// - Templates: Pre-made builds for new players / quick-start
//
// At combat start, LoadoutComponent copies this asset into FCombatLoadout

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ECharacterClass.h"
#include "EWeaponSlotType.h"
#include "LoadoutData.generated.h"

class UWeaponData;
class URingData;
class USpellData;
class UAbilityData;
class UItemData;
class UStanceData;
class UAnimMontage;

/**
 * ULoadoutData
 *
 * Pre-configured combat loadout stored as a Data Asset.
 *
 * Usage:
 * - AI Enemies: Designer creates LoadoutData per enemy type
 * - Players: Can use as templates (copied to LoadoutComponent)
 * - Quick-start: Pre-made builds for new players
 *
 * Class-specific rules:
 * - Generic: Primary weapon + Secondary (weapon OR ring)
 * - Caster: Primary (weapon OR ring) + 24 innate spells
 * - Resonator: Primary weapon + 5 ring loadout (max 2 evolved)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API ULoadoutData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString LoadoutName = TEXT("Unnamed Loadout");

    /** Optional description */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    /** Which character class this loadout is designed for */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    ECharacterClass RequiredClass = ECharacterClass::Generic;

    // ==================== PRIMARY EQUIPMENT ====================

    /** Primary slot type - Caster only chooses between Weapon/Ring */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Primary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    EPrimarySlotType PrimarySlotType = EPrimarySlotType::Weapon;

    /** Primary weapon (Generic/Resonator always, Caster if PrimarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Caster || PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    UWeaponData *PrimaryWeapon = nullptr;

    /** Primary ring (Caster only, when PrimarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Primary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster && PrimarySlotType == EPrimarySlotType::Ring", EditConditionHides))
    URingData *PrimaryRing = nullptr;

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    /** Secondary slot type (Generic only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic", EditConditionHides))
    ESecondarySlotType SecondarySlotType = ESecondarySlotType::None;

    /** Secondary weapon (Generic only, when SecondarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    UWeaponData *SecondaryWeapon = nullptr;

    /** Secondary ring (Generic only, when SecondarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Ring", EditConditionHides))
    URingData *SecondaryRing = nullptr;

    // ==================== RESONATOR RINGS ====================

    /** Equipped rings (Resonator only - up to 5, max 2 evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Rings",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Resonator", EditConditionHides))
    TArray<URingData *> EquippedRings;

    // ==================== CASTER INNATE SPELLS ====================

    /** Innate spells (Caster only - up to 24, must match InnateElement) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells|Innate",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    TArray<USpellData *> InnateSpells;

    // ==================== PRIMARY WEAPON CONFIGURATION ====================

    /** Abilities assigned to primary weapon (max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Caster || PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> PrimaryWeaponAbilities;

    /** Spells assigned to primary weapon crystal (max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Caster || PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    TArray<USpellData *> PrimaryWeaponSpells;

    /** Override primary weapon stance (nullptr = use weapon default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Caster || PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    UStanceData *PrimaryWeaponStanceOverride = nullptr;

    // ==================== SECONDARY WEAPON CONFIGURATION (Generic only) ====================

    /** Abilities assigned to secondary weapon (Generic only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> SecondaryWeaponAbilities;

    /** Spells assigned to secondary weapon crystal (Generic only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    TArray<USpellData *> SecondaryWeaponSpells;

    /** Override secondary weapon stance (Generic only, nullptr = use weapon default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    UStanceData *SecondaryWeaponStanceOverride = nullptr;

    // ==================== PRIMARY RING CONFIGURATION (Caster only) ====================

    /** Spells assigned to primary ring (Caster only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring|Primary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster && PrimarySlotType == EPrimarySlotType::Ring", EditConditionHides))
    TArray<USpellData *> PrimaryRingSpells;

    // ==================== SECONDARY RING CONFIGURATION (Generic only) ====================

    /** Spells assigned to secondary ring (Generic only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring|Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Ring", EditConditionHides))
    TArray<USpellData *> SecondaryRingSpells;

    // ==================== ITEMS ====================

    /** Equipped item crystals (max 6 slots, 3 uses each) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
    TArray<UItemData *> EquippedItems;

    // ==================== COSMETICS ====================

    /** Unarmed stance (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
    UStanceData *UnarmedStance = nullptr;

    // ==================== DEFENSE ANIMATIONS ====================

    /** Dodge left animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Animations")
    UAnimMontage *DodgeLeftMontage = nullptr;

    /** Dodge right animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Animations")
    UAnimMontage *DodgeRightMontage = nullptr;

    /** Block animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Animations")
    UAnimMontage *BlockMontage = nullptr;

    /** Parry animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Animations")
    UAnimMontage *ParryMontage = nullptr;

    /** Use weapon's parry animation instead of character's */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense|Options")
    bool bUseWeaponParryAnimation = false;

    // ==================== VALIDATION ====================

    /** Check if loadout is valid for given class */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    bool IsValidForClass(ECharacterClass CharacterClass) const;

    /** Get all validation errors */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    TArray<FString> GetValidationErrors() const;

    /** Quick check if loadout has any errors */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    bool HasValidationErrors() const;

    // ==================== ACCESSORS ====================

    /** Get all spells available from this loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    TArray<USpellData *> GetAllSpells() const;

    /** Get all abilities available from this loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    TArray<UAbilityData *> GetAllAbilities() const;

    /** Get primary weapon (nullptr if using ring) */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    UWeaponData *GetPrimaryWeapon() const;

    /** Get primary ring (nullptr if using weapon or not Caster) */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    URingData *GetPrimaryRing() const;

    // ==================== EDITOR ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

    // ==================== DATA ASSET ====================

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};