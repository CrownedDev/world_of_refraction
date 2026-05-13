// RingData.h
// Ring data asset for Resonator class spell casting

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemTier.h"
#include "ESpellElement.h"
#include "LoadoutConstants.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "RingData.generated.h"

class USpellData;
class UItemData;

/**
 * Ring Data Asset - Resonator's primary spell source
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API URingData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString Name = TEXT("Unnamed Ring");

	/** Ring tier - reference value for the slotted crystal's wear calculations.
	 *  The crystal's tier determines actual wear; ring tier is informational. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EItemTier Tier = EItemTier::E_Tier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Ring description...");

	// ==================== CRYSTAL SLOT ====================

	/** Refined crystal that defines ring's element */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal")
	UItemData *SlottedCrystal = nullptr;

	// ==================== SPELLS ====================

	/** Default spells for this ring - copied to inventory entry when ring obtained
	 *  Lost if crystal is removed; spell vendor reassigns them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells", meta = (TitleProperty = "Name"))
	TArray<USpellData *> DefaultSpells;

	// ==================== CRYSTAL HELPERS ====================

	UFUNCTION(BlueprintPure, Category = "Ring|Crystal")
	bool HasCrystal() const { return SlottedCrystal != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Ring|Crystal")
	ESpellElement GetRingElement() const;

	UFUNCTION(BlueprintPure, Category = "Ring|Crystal")
	bool IsEvolved() const;

	UFUNCTION(BlueprintPure, Category = "Ring|Spells")
	int32 GetMaxSpells() const { return LoadoutConstants::MAX_RING_SPELLS; }
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
