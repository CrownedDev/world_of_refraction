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
class UNiagaraSystem;
class USoundBase;

namespace CosmeticsPlaceholders
{
	// TODO: art. Engine content stand-ins (editor builds only) until the Perfect Guard art exists.
	inline const TCHAR *PERFECT_GUARD_FLASH_VFX = TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst");
	inline const TCHAR *PERFECT_GUARD_SOUND = TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue");
}

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

	/** OBSOLETE (B-202, Q-182 changed by Crown): every Guard plays BlockMontage; nothing plays
	 *  this any more. Kept until PIE confirms the single Guard animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	UAnimMontage *ParryMontage = nullptr;

	/** Flash at the impact of a Perfect Guard (B-202; a normal Guard has none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	TSoftObjectPtr<UNiagaraSystem> PerfectGuardFlashVFX{FSoftObjectPath(CosmeticsPlaceholders::PERFECT_GUARD_FLASH_VFX)};

	/** Sound at the impact of a Perfect Guard (B-202). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense")
	TSoftObjectPtr<USoundBase> PerfectGuardSound{FSoftObjectPath(CosmeticsPlaceholders::PERFECT_GUARD_SOUND)};

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
