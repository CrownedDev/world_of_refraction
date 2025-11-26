// BrokenDarknessManager.h
// Component for handling BrokenDarkness transformation and absorption energy

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpellElement.h"
#include "BrokenDarknessManager.generated.h"

class USpellData;
class UCharacterData;

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrokenDarknessTransformed, AActor *, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnergyAbsorbed, AActor *, Actor, float, AmountAbsorbed, ESpellElement, AbsorbedElement);

/**
 * BrokenDarkness Manager Component
 *
 * Handles the special Caster variant that:
 * - Transforms permanently when casting certain Darkness spells
 * - Uses absorption energy (from parry/block) instead of normal regen
 * - Gains hybrid spells (Darkness + absorbed elements)
 * - Vulnerable against physical enemies (nothing to absorb)
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UBrokenDarknessManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UBrokenDarknessManager();

	virtual void BeginPlay() override;

	// ==================== TRANSFORMATION STATE ====================

	/** Has this character transformed into BrokenDarkness? */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness")
	bool bIsTransformed = false;

	/** Corruption buildup (0-100, transforms at 100) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness")
	float CorruptionBuildup = 0.0f;

	// ==================== ABSORPTION ENERGY ====================

	/** Current absorption energy (replaces normal energy for transformed) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float AbsorptionEnergy = 0.0f;

	/** Maximum absorption energy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float MaxAbsorptionEnergy = 100.0f;

	/** Energy gained per damage absorbed via parry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float ParryAbsorptionRate = 1.0f; // 1:1 damage to energy

	/** Energy gained per damage absorbed via block */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float BlockAbsorptionRate = 0.5f; // 0.5:1 damage to energy

	// ==================== ABSORBED ELEMENTS ====================

	/** Elements absorbed this combat (for hybrid spells) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Elements")
	TArray<ESpellElement> AbsorbedElements;

	/** Last element absorbed (used for hybrid spell element) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Elements")
	ESpellElement LastAbsorbedElement = ESpellElement::Generic;

	// ==================== TRANSFORMATION ====================

	/** Check if a spell can trigger corruption */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness")
	bool CanSpellCorrupt(USpellData *Spell) const;

	/** Process spell cast for corruption chance */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness")
	void ProcessSpellCast(USpellData *Spell);

	/** Add corruption buildup */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness")
	void AddCorruption(float Amount);

	/** Trigger transformation (permanent) */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness")
	void TriggerTransformation();

	/** Is character transformed? */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness")
	bool IsTransformed() const { return bIsTransformed; }

	// ==================== ABSORPTION ====================

	/** Called when successfully parrying an elemental attack */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Absorption")
	void OnSuccessfulParry(float DamageBlocked, ESpellElement DamageElement);

	/** Called when successfully blocking an elemental attack */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Absorption")
	void OnSuccessfulBlock(float DamageBlocked, ESpellElement DamageElement);

	/** Get current absorption energy */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Absorption")
	float GetAbsorptionEnergy() const { return AbsorptionEnergy; }

	/** Spend absorption energy for spell cast */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Absorption")
	bool SpendAbsorptionEnergy(float Amount);

	/** Can afford this energy cost? */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Absorption")
	bool CanAffordEnergy(float Amount) const { return AbsorptionEnergy >= Amount; }

	// ==================== HYBRID SPELLS ====================

	/** Has absorbed this element? */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Hybrid")
	bool HasAbsorbedElement(ESpellElement Element) const;

	/** Can cast hybrid spell with this element? */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Hybrid")
	bool CanCastHybridSpell(ESpellElement SecondaryElement) const;

	/** Get hybrid element (Darkness + last absorbed) */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Hybrid")
	ESpellElement GetHybridElement() const { return LastAbsorbedElement; }

	// ==================== DELEGATES ====================

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnBrokenDarknessTransformed OnTransformed;

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnEnergyAbsorbed OnEnergyAbsorbed;

private:
	/** Add absorption energy (clamped to max) */
	void AddAbsorptionEnergy(float Amount);

	/** Record absorbed element */
	void RecordAbsorbedElement(ESpellElement Element);
};
