// CosmeticsData.h
// Cosmetic data asset - per-character visual/animation data (defense montages,
// stances, item-use montages, infusion VFX, weather variant).
//
// A character points at one UCosmeticsData via UCharacterData::Cosmetics.
// Multiple characters may share the same asset. No override/fallback logic.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CosmeticsData.generated.h"

class UAnimMontage;
class UStanceData;
class UInfusionDisplayData;

/**
 * UCosmeticsData
 * Visual and animation data shared by characters. Immutable design-time data.
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCosmeticsData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== DEFENSE ANIMATIONS ====================

	/** Dodge left animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *DodgeLeftMontage = nullptr;

	/** Dodge right animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *DodgeRightMontage = nullptr;

	/** Block animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *BlockMontage = nullptr;

	/** Parry animation (used when the weapon loadout entry does not override it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *ParryMontage = nullptr;

	// ==================== COSMETICS ====================

	/** Default unarmed stance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UStanceData *UnarmedStance = nullptr;

	/** Animation for Resonator ring switching */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UAnimMontage *RingSwitchMontage = nullptr;

	/** Visual effect for element infusion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UInfusionDisplayData *InfusionDisplay = nullptr;

	// ==================== ITEM USE ====================

	/** Animation for self-targeted item use (healing, energy, cleanse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Use")
	UAnimMontage *ItemUseSelfMontage = nullptr;

	/** Animation for target-directed item use (damage, ally buffs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Use")
	UAnimMontage *ItemUseTargetMontage = nullptr;

	// ==================== WEATHER ====================

	/** Weather variant equipped for when this character is team leader.
	 *  Leave null to use element default. Generic and Resonator classes ignore this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	UPrimaryDataAsset *EquippedWeatherVariant = nullptr;
};
