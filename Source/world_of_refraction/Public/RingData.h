// RingData.h
// Ring data asset for Resonator class
// Rings provide element, spells, and can break

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ETier.h"
#include "RefractionElement.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "RingData.generated.h"

// Forward declarations
class USpellData;
class UCrystalData;

/**
 * Ring Data Asset - Resonator's element source and spell container
 * 
 * Key Features:
 * - Each ring has an element (locked at creation)
 * - 6 spell slots (variable spells per ring - enables unique builds)
 * - 1 crystal slot (Ilodite/Quartz/Evolution)
 * - Can break from tier mismatch or infusion
 * 
 * Loadout Rules:
 * - Resonator has 6 ring slots
 * - Up to 3 rings of same element allowed (backups)
 * - Ring switching is free action (your turn only)
 * - Ultimate is set per element at loadout (separate from ring)
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API URingData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString RingName = TEXT("Unnamed Ring");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ERefractionElement Element = ERefractionElement::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ETier Tier = ETier::E;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Ring description...");

	// ==================== SPELLS ====================

	/** Default spells that come with the ring (safe to cast) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	TArray<USpellData*> DefaultSpells;

	/** Player-customized spells (may cause break risk if tier mismatch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	TArray<USpellData*> CustomSpells;

	/** Maximum spell slots (default 6, some rings may have more/less) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxSpellSlots = 6;

	// ==================== CRYSTAL SLOT ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal")
	UCrystalData* CrystalSlot = nullptr;

	// ==================== DURABILITY ====================

	/** Maximum durability (how much abuse before break) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "1"))
	float MaxDurability = 100.0f;

	/** Current durability (runtime) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Durability|Runtime")
	float CurrentDurability = 100.0f;

	/** Is this ring broken? (runtime) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Durability|Runtime")
	bool bIsBroken = false;

	// ==================== PRESENTATION ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	UTexture2D* Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	UStaticMesh* RingMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FLinearColor RingColor = FLinearColor::White;

	// ==================== UTILITY FUNCTIONS ====================

	/** Get display name */
	UFUNCTION(BlueprintPure, Category = "Ring")
	FString GetDisplayName() const { return RingName; }

	/** Get tier display string */
	UFUNCTION(BlueprintPure, Category = "Ring")
	FString GetTierString() const { return TierHelpers::GetTierDisplayString(Tier); }

	/** Get element name */
	UFUNCTION(BlueprintPure, Category = "Ring")
	FString GetElementName() const;

	/** Get all available spells (default + custom, excluding nulls) */
	UFUNCTION(BlueprintPure, Category = "Ring")
	TArray<USpellData*> GetAvailableSpells() const;

	/** Get total spell count */
	UFUNCTION(BlueprintPure, Category = "Ring")
	int32 GetSpellCount() const;

	/** Can this ring cast the given spell? (checks element match) */
	UFUNCTION(BlueprintPure, Category = "Ring")
	bool CanCastSpell(USpellData* Spell) const;

	/** Is this a custom spell (higher break risk)? */
	UFUNCTION(BlueprintPure, Category = "Ring")
	bool IsCustomSpell(USpellData* Spell) const;

	// ==================== DURABILITY FUNCTIONS ====================

	/** Get durability as percentage (0.0 to 1.0) */
	UFUNCTION(BlueprintPure, Category = "Ring|Durability")
	float GetDurabilityPercent() const;

	/** Apply durability damage from spell cast */
	UFUNCTION(BlueprintCallable, Category = "Ring|Durability")
	void ApplyDurabilityDamage(float Damage);

	/** Check and handle ring break */
	UFUNCTION(BlueprintCallable, Category = "Ring|Durability")
	bool CheckBreak();

	/** Force break this ring */
	UFUNCTION(BlueprintCallable, Category = "Ring|Durability")
	void ForceBreak();

	/** Reset ring state (for new instances) */
	UFUNCTION(BlueprintCallable, Category = "Ring|Durability")
	void ResetRingState();

	// ==================== BREAK CHANCE CALCULATION ====================

	/** Calculate break chance for casting a spell */
	UFUNCTION(BlueprintPure, Category = "Ring|Break")
	float CalculateBreakChance(USpellData* Spell, bool bIsInfused) const;

	/** Roll for break and apply if triggered */
	UFUNCTION(BlueprintCallable, Category = "Ring|Break")
	bool RollForBreak(float BreakChance);

	// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
