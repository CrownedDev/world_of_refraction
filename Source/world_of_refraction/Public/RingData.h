// RingData.h
// Ring data asset for Resonator class spell casting

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "ItemTier.h"
#include "ESpellElement.h"
#include "LoadoutConstants.h"
#include "WorldStatRequirements.h"
#include "FEquipmentStatBonus.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "RingData.generated.h"

class USpellData;
class UItemData;
class UCharacterData;

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

	// If true, spells cannot be customized (conjured-ring equivalent of bAbilitiesLocked)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	bool bSpellsLocked = false;

	// ==================== WORLD STAT REQUIREMENTS ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
	FWorldStatRequirements Requirements;

	// ==================== ENGRAVINGS (STAT BONUSES) ====================

	/** Roll template for this ring's per-instance stat bonuses. Copied into
	 *  FRingInventoryEntry::StatBonus at CreateFromRing time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engravings")
	FEquipmentStatBonus DefaultStatBonus;

	/** When true, FRingInventoryEntry::StatBonus.bLocked is set on instance
	 *  creation — engravings cannot be re-rolled via SpendPendingPoints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engravings")
	bool bStatBonusLocked = false;

	// ==================== INFUSION ====================

	/** Status buildup multiplier when a Resonator's ring is the infusion source */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion",
			  meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float InfusionStatusMultiplier = 1.0f;

	// ==================== MESH ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMesh *RingStaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FRotator MeshRotation = FRotator::ZeroRotator;

	// ==================== PRESENTATION ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	UTexture2D *Icon = nullptr;

	// ==================== CRYSTAL HELPERS ====================

	UFUNCTION(BlueprintPure, Category = "Ring|Crystal")
	bool HasCrystal() const { return SlottedCrystal != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Ring|Crystal")
	ESpellElement GetRingElement() const;

	UFUNCTION(BlueprintPure, Category = "Ring|Crystal")
	bool IsEvolved() const;

	UFUNCTION(BlueprintPure, Category = "Ring|Spells")
	int32 GetMaxSpells() const { return LoadoutConstants::MAX_RING_SPELLS; }

	// ==================== UTILITY ====================

	UFUNCTION(BlueprintPure, Category = "Ring")
	bool MeetsRequirements(const UCharacterData *Character) const
	{
		return Requirements.MeetsRequirements(Character);
	}

	UFUNCTION(BlueprintPure, Category = "Ring")
	FString GetRequirementsSummary(const UCharacterData *Character) const
	{
		return Requirements.GetRequirementsSummary(Character);
	}

	UFUNCTION(BlueprintPure, Category = "Ring")
	FString GetDisplayName() const { return Name; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
