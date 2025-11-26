// CrystalData.h
// Crystal data asset for weapon and ring crystal slots
// NOTE: Different from ItemData crystals (Garnet, Sapphire, etc.) - these are equipment modifications

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ECrystalSlotType.h"
#include "ETier.h"
#include "RefractionElement.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CrystalData.generated.h"

// Forward declarations
class USpellData;
class UAbilityData;

/**
 * Stat modifiers applied by Evolution crystals
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCrystalStatModifiers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 BonusMind = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 BonusBody = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 BonusSpirit = 0;

	/** Check if any stats are modified */
	bool HasModifiers() const
	{
		return BonusMind != 0 || BonusBody != 0 || BonusSpirit != 0;
	}
};

/**
 * Crystal Data Asset - Equipment modification crystals for weapons and rings
 * 
 * Three types:
 * - Ilodite: Enhances existing effects, replaceable
 * - Quartz: Absorbs element via parry/block, permanent
 * - Evolution: Grants spells + stats, permanent, enables Evolution Infusion
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCrystalData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString CrystalName = TEXT("Unnamed Crystal");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ECrystalSlotType CrystalType = ECrystalSlotType::Ilodite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ETier Tier = ETier::E;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Crystal description...");

	// ==================== ELEMENT ====================
	// Only relevant for Evolution crystals (Ilodite enhances existing, Quartz absorbs)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	ERefractionElement Element = ERefractionElement::Fire;

	// ==================== ILODITE - ENHANCEMENT ====================

	/** Multiplier for existing weapon/ring effects (1.0 = no change, 1.2 = 20% boost) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ilodite",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Ilodite", EditConditionHides,
			ClampMin = "1.0", ClampMax = "2.0"))
	float EnhancementMultiplier = 1.0f;

	/** Bonus damage added to attacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ilodite",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Ilodite", EditConditionHides,
			ClampMin = "0"))
	int32 BonusDamage = 0;

	/** Bonus status buildup added */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ilodite",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Ilodite", EditConditionHides,
			ClampMin = "0"))
	int32 BonusStatusBuildup = 0;

	// ==================== QUARTZ - ABSORPTION ====================

	/** Damage threshold to reach before transformation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quartz",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Quartz", EditConditionHides,
			ClampMin = "100"))
	int32 AbsorptionThreshold = 500;

	/** Current absorbed damage (runtime, not serialized in asset) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Quartz|Runtime")
	int32 CurrentAbsorption = 0;

	/** Element absorbed from parried/blocked attacks (runtime) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Quartz|Runtime")
	ERefractionElement AbsorbedElement = ERefractionElement::Generic;

	/** Has this quartz been transformed? (runtime) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Quartz|Runtime")
	bool bHasTransformed = false;

	// ==================== EVOLUTION - SPELLS & ABILITIES ====================

	/** Spells granted by this evolution crystal */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	TArray<USpellData*> EvolutionSpells;

	/** Abilities granted (Generic class only uses these) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	TArray<UAbilityData*> EvolutionAbilities;

	/** Stat bonuses from evolution */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
		meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	FCrystalStatModifiers StatBonuses;

	// ==================== PRESENTATION ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	UTexture2D* Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FLinearColor CrystalColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	UStaticMesh* CrystalMesh = nullptr;

	// ==================== UTILITY FUNCTIONS ====================

	/** Get display name */
	UFUNCTION(BlueprintPure, Category = "Crystal")
	FString GetDisplayName() const { return CrystalName; }

	/** Get tier display string */
	UFUNCTION(BlueprintPure, Category = "Crystal")
	FString GetTierString() const { return TierHelpers::GetTierDisplayString(Tier); }

	/** Is this crystal removable? */
	UFUNCTION(BlueprintPure, Category = "Crystal")
	bool IsRemovable() const { return CrystalSlotHelpers::IsRemovable(CrystalType); }

	/** Is this crystal permanent once slotted? */
	UFUNCTION(BlueprintPure, Category = "Crystal")
	bool IsPermanent() const { return CrystalSlotHelpers::IsPermanent(CrystalType); }

	/** Does this crystal enable Evolution Infusion? */
	UFUNCTION(BlueprintPure, Category = "Crystal")
	bool EnablesEvolutionInfusion() const 
	{ 
		return CrystalSlotHelpers::EnablesEvolutionInfusion(CrystalType); 
	}

	// ==================== QUARTZ RUNTIME ====================

	/** Add absorption from blocked/parried damage */
	UFUNCTION(BlueprintCallable, Category = "Crystal|Quartz")
	void AddAbsorption(int32 DamageBlocked, ERefractionElement DamageElement);

	/** Check if quartz has reached transformation threshold */
	UFUNCTION(BlueprintPure, Category = "Crystal|Quartz")
	bool HasReachedThreshold() const { return CurrentAbsorption >= AbsorptionThreshold; }

	/** Get absorption progress (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "Crystal|Quartz")
	float GetAbsorptionProgress() const;

	/** Trigger quartz transformation */
	UFUNCTION(BlueprintCallable, Category = "Crystal|Quartz")
	void TriggerTransformation();

	/** Reset quartz state (for new instances) */
	UFUNCTION(BlueprintCallable, Category = "Crystal|Quartz")
	void ResetQuartzState();

	// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
