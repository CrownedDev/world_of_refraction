// LoadoutData.h
// Pre-configured combat loadout asset for AI enemies and player templates
//
// ARCHITECTURE:
// - AI enemies: Designer creates one LoadoutData per enemy type
// - Players: Use LoadoutComponent.SavedLoadouts[] (built from UI)
// - Templates: Pre-made builds for new players / quick-start
//
// At combat start, LoadoutComponent copies this asset into FCombatLoadout
//
// CLASS RULES:
// - Generic: Primary (Weapon/Ring/Evolution) + Secondary (Weapon only)
// - Caster: Primary (Weapon/Ring/Evolution) + Innate spells
// - Resonator (normal): Weapon + 5 ring loadout (max 2 evolved)
// - Resonator (evolved): Evolution + 3 ring loadout (max 1 evolved)

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
class UInfusionDisplayData;

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
 * - Generic: Primary (Weapon/Ring/Evolution) + Secondary (Weapon only)
 * - Caster: Primary (Weapon/Ring/Evolution) + 24 innate spells
 * - Resonator: Primary (Weapon/Evolution) + ring loadout (5 normal, 3 if evolved)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API ULoadoutData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    FString LoadoutName = TEXT("Unnamed Loadout");

    /** Optional description */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    /** Which character class this loadout is designed for */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    ECharacterClass RequiredClass = ECharacterClass::Generic;

    // ==================== CASTER INNATE SPELLS ====================

    /** Innate spells (Caster only - up to 24, must match InnateElement) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Caster Spells",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    TArray<USpellData *> InnateSpells;

    // ==================== RESONATOR RINGS ====================

    /** Equipped rings (Resonator only - 5 normal, 3 if evolved) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Resonator Rings",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Resonator", EditConditionHides))
    TArray<URingData *> EquippedRings;

    /** If true, character starts combat using primary equipment. If false, starts unarmed (Caster/Resonator) or with secondary (Generic). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity", meta = (DisplayName = "Starts Using Primary"))
    bool bUsingPrimary = true;
    // ==================== PRIMARY EQUIPMENT ====================

    /** Primary slot type (Generic/Caster: Weapon/Ring/Evolution, Resonator: Weapon/Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary")
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

    /** Spells from primary equipment - weapon crystal / ring / evolution (max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config")
    TArray<USpellData *> PrimaryEquipmentSpells;

    /** Override primary weapon stance (nullptr = use weapon default) */
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

    /** Spells from secondary weapon crystal (Generic only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary|Config",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    TArray<USpellData *> SecondaryWeaponSpells;

    /** Override secondary weapon stance (Generic only, nullptr = use weapon default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary|Config",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    UStanceData *SecondaryWeaponStanceOverride = nullptr;

    // ==================== ITEMS ====================

    /** Equipped item crystals (max 6 slots, 3 uses each) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Items")
    TArray<UItemData *> EquippedItems;

    // ==================== DEFENSE ANIMATIONS ====================

    /** Dodge left animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Defense")
    UAnimMontage *DodgeLeftMontage = nullptr;

    /** Dodge right animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Defense")
    UAnimMontage *DodgeRightMontage = nullptr;

    /** Block animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Defense")
    UAnimMontage *BlockMontage = nullptr;

    /** Parry animation (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Defense")
    UAnimMontage *ParryMontage = nullptr;

    /** Use weapon's parry animation instead of character's */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6. Defense")
    bool bUseWeaponParryAnimation = false;

    // ==================== COSMETICS ====================

    /** Unarmed stance (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7. Cosmetics")
    UStanceData *UnarmedStance = nullptr;

    /** Infusion visual effect (nullptr = character default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7. Cosmetics")
    UInfusionDisplayData *InfusionDisplay = nullptr;

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

    /** Get primary weapon (nullptr if using ring or evolution) */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    UWeaponData *GetPrimaryWeapon() const;

    /** Get primary ring (nullptr if using weapon or evolution) */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    URingData *GetPrimaryRing() const;

    /** Get primary evolution (nullptr if using weapon or ring) */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    UItemData *GetPrimaryEvolution() const;

    /** Check if this loadout uses evolution */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    bool IsEvolutionLoadout() const { return PrimarySlotType == EPrimarySlotType::Evolution; }

    // ==================== EDITOR ====================

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

    // ==================== DATA ASSET ====================

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};