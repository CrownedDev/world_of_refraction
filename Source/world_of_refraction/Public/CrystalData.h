// CrystalData.h
// Crystal data asset for weapon and ring crystal slots

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ECrystalSlotType.h"
#include "ItemTier.h"
#include "SpellElement.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CrystalData.generated.h"

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

	bool HasModifiers() const
	{
		return BonusMind != 0 || BonusBody != 0 || BonusSpirit != 0;
	}
};

/**
 * Crystal Data Asset - Equipment modification crystals for weapons and rings
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
	EItemTier Tier = EItemTier::E_Tier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Crystal description...");

	// ==================== ELEMENT ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	ESpellElement Element = ESpellElement::Fire;

	// ==================== ILODITE ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ilodite",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Ilodite", EditConditionHides,
					  ClampMin = "1.0", ClampMax = "2.0"))
	float EnhancementMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ilodite",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Ilodite", EditConditionHides, ClampMin = "0"))
	int32 BonusDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ilodite",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Ilodite", EditConditionHides, ClampMin = "0"))
	int32 BonusStatusBuildup = 0;

	// ==================== QUARTZ ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quartz",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Quartz", EditConditionHides, ClampMin = "100"))
	float AbsorptionThreshold = 500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Quartz|Runtime")
	float CurrentAbsorption = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Quartz|Runtime")
	ESpellElement AbsorbedElement = ESpellElement::Generic;

	UPROPERTY(BlueprintReadOnly, Category = "Quartz|Runtime")
	bool bHasTransformed = false;

	// ==================== EVOLUTION ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	TArray<USpellData *> EvolutionSpells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	TArray<UAbilityData *> EvolutionAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution",
			  meta = (EditCondition = "CrystalType == ECrystalSlotType::Evolution", EditConditionHides))
	FCrystalStatModifiers StatBonuses;

	// ==================== HELPERS ====================

	UFUNCTION(BlueprintPure, Category = "Crystal")
	FString GetTierString() const { return TierHelpers::GetTierDisplayString(Tier); }

	UFUNCTION(BlueprintCallable, Category = "Crystal|Quartz")
	void AddAbsorption(float DamageAbsorbed, ESpellElement DamageElement)
	{
		if (CrystalType != ECrystalSlotType::Quartz || bHasTransformed)
			return;
		CurrentAbsorption += DamageAbsorbed;
		AbsorbedElement = DamageElement;
	}

	UFUNCTION(BlueprintPure, Category = "Crystal|Quartz")
	bool HasReachedThreshold() const
	{
		return CrystalType == ECrystalSlotType::Quartz &&
			   CurrentAbsorption >= AbsorptionThreshold && !bHasTransformed;
	}

	UFUNCTION(BlueprintPure, Category = "Crystal|Quartz")
	float GetAbsorptionProgress() const
	{
		if (CrystalType != ECrystalSlotType::Quartz || AbsorptionThreshold <= 0.0f)
			return 0.0f;
		return FMath::Clamp(CurrentAbsorption / AbsorptionThreshold, 0.0f, 1.0f);
	}

	UFUNCTION(BlueprintCallable, Category = "Crystal|Quartz")
	void TriggerTransformation()
	{
		if (CrystalType != ECrystalSlotType::Quartz)
			return;
		bHasTransformed = true;
		Element = AbsorbedElement;
	}

	UFUNCTION(BlueprintCallable, Category = "Crystal|Quartz")
	void ResetQuartzState()
	{
		CurrentAbsorption = 0.0f;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override
	{
		EDataValidationResult Result = Super::IsDataValid(Context);

		switch (CrystalType)
		{
		case ECrystalSlotType::Ilodite:
			if (EnhancementMultiplier <= 1.0f && BonusDamage == 0 && BonusStatusBuildup == 0)
				Context.AddWarning(FText::FromString(TEXT("Ilodite crystal has no enhancement effect")));
			break;

		case ECrystalSlotType::Quartz:
			if (AbsorptionThreshold < 100.0f)
			{
				Context.AddError(FText::FromString(TEXT("Quartz absorption threshold too low (min 100)")));
				Result = EDataValidationResult::Invalid;
			}
			break;

		case ECrystalSlotType::Evolution:
			if (EvolutionSpells.Num() == 0 && EvolutionAbilities.Num() == 0 && !StatBonuses.HasModifiers())
				Context.AddWarning(FText::FromString(TEXT("Evolution crystal provides no spells, abilities, or stats")));
			if (Element == ESpellElement::Generic)
				Context.AddWarning(FText::FromString(TEXT("Evolution crystal should have a specific element")));
			break;

		default:
			break;
		}

		return Result;
	}
#endif
};
