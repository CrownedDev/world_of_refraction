// BrokenDarknessManager.h
// Component for handling BrokenDarkness transformation and absorption energy

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpellElement.h"
#include "EDefenseType.h"
#include "DefenseSystem.h"
#include "BrokenDarknessManager.generated.h"

class USpellData;
class UAbilityData;
class UCharacterData;
class UDefenseSystem;
class UCharacterDataComponent;

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrokenDarknessTransformed, AActor*, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnergyAbsorbed, AActor*, Actor, float, AmountAbsorbed, ESpellElement, AbsorbedElement);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOverloadStateChanged, AActor*, Actor, bool, bIsOverloaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStacksChanged, AActor*, Actor, ESpellElement, Element, int32, NewStackCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAlignmentChanged, AActor*, Actor, ESpellElement, OldElement, ESpellElement, NewElement);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOverloadDamage, AActor*, Source, AActor*, Target, float, Damage);

/**
 * BrokenDarkness Manager Component
 *
 * Handles the special Caster variant that:
 * - Transforms when pushing beyond stat requirements (3% chance)
 * - Uses absorption energy (from parry/block) instead of normal regen
 * - Gains hybrid spells (Darkness + absorbed elements)
 * - Builds stacks for exponential status effect multipliers
 * - Enters overload state when energy exceeds maximum
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
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness")
	bool IsTransformed() const { return bIsTransformed; }

	// ==================== BREAK SYSTEM ====================

	/**
	 * Roll for Broken Darkness transformation (3% chance)
	 * Called when casting above requirements or using L2 infusion
	 * @param TriggerReason Debug string for logging what triggered the roll
	 * @return True if transformation occurred
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Break")
	bool RollForBreak(const FString& TriggerReason);

	/**
	 * Check if spell exceeds character's stat requirements
	 * @param Spell The spell being cast
	 * @param Character The character's data
	 * @return True if any world stat is below spell requirements
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Break")
	static bool DoesSpellExceedRequirements(USpellData* Spell, UCharacterData* Character);

	/**
	 * Check if ability exceeds character's stat requirements
	 * @param Ability The ability being used
	 * @param Character The character's data
	 * @return True if any world stat is below ability requirements
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Break")
	static bool DoesAbilityExceedRequirements(UAbilityData* Ability, UCharacterData* Character);

	/**
	 * Force transformation (for testing/story events)
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Break")
	void ForceTransformation();

	// ==================== FORBIDDEN ELEMENTS ====================

	/**
	 * Check if element is forbidden (Light/Void)
	 * Casting these as hybrids deals self-damage
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Forbidden")
	static bool IsForbiddenElement(ESpellElement Element);

	/**
	 * Check if element can be absorbed (excludes Reality and Generic)
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Forbidden")
	static bool CanAbsorbElement(ESpellElement Element);

	/**
	 * Calculate self-damage for casting forbidden element spell
	 * @param SpellBaseDamage The base damage of the spell being cast
	 * @return Self-damage amount to apply
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Forbidden")
	float CalculateForbiddenCastDamage(float SpellBaseDamage) const;

	/**
	 * Process forbidden element cast - apply self-damage
	 * Call this when BD casts a Dark Light or Dark Void hybrid
	 * @param SpellElement The element of the spell being cast
	 * @param SpellBaseDamage The base damage of the spell
	 * @return True if self-damage was applied
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Forbidden")
	bool ProcessForbiddenCast(ESpellElement SpellElement, float SpellBaseDamage);

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

	// ==================== OVERLOAD STATE ====================

	/** Check if currently in overload state (Energy > MaxEnergy) */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	bool IsOverloaded() const { return bIsOverloaded; }

	/** Get current overload energy (amount over max), returns 0 if not overloaded */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	float GetOverloadEnergy() const;

	/** Get overload capacity (how much over max before forced drain) */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	float GetOverloadCapacity() const { return OverloadCapacity; }

	/**
	 * Process overload tick (call from turn manager at end of turn)
	 * Applies aura damage to enemies, self-damage, energy drain
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Overload")
	void ProcessOverloadTick(const TArray<AActor*>& NearbyEnemies, float EffectDamageMultiplier, float EfficiencyPercent);

	// ==================== ABSORPTION STACKS ====================

	/** Get current stack count for the active alignment element */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	int32 GetCurrentStackCount() const { return CurrentAbsorptionStacks; }

	/** Get current alignment element */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	ESpellElement GetCurrentAlignment() const { return CurrentAlignmentElement; }

	/**
	 * Get stack multiplier for status effects
	 * Stack 0: 1.0x, Stack 1: 1.0x, Stack 2: 2.0x, Stack 3: 4.0x
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	float GetStackStatusMultiplier() const;

	/** Get maximum stacks allowed */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	int32 GetMaxStacks() const { return MaxAbsorptionStacks; }

	/** Check if at max stacks */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	bool IsAtMaxStacks() const { return CurrentAbsorptionStacks >= MaxAbsorptionStacks; }

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

	// ==================== DEFENSE SYSTEM INTEGRATION ====================

	/**
	 * Called by DefenseSystem when a defense window closes
	 * Handles absorption based on defense type and result
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Defense")
	void OnDefenseResolved(EDefenseType DefenseType, const FDefenseResult& DefenseResult,
		ESpellElement AttackElement, float AttackEnergyCost);

	// ==================== DELEGATES ====================

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnBrokenDarknessTransformed OnTransformed;

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnEnergyAbsorbed OnEnergyAbsorbed;

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnOverloadStateChanged OnOverloadStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnStacksChanged OnStacksChanged;

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnAlignmentChanged OnAlignmentChanged;

	UPROPERTY(BlueprintAssignable, Category = "BrokenDarkness|Events")
	FOnOverloadDamage OnOverloadDamage;

private:
	// ==================== PRIVATE METHODS ====================

	/** Add absorption energy (clamped to max + overload capacity) */
	void AddAbsorptionEnergy(float Amount);

	/** Record absorbed element */
	void RecordAbsorbedElement(ESpellElement Element);

	/** Check and update overload state */
	void UpdateOverloadState();

	/** Enter overload state */
	void EnterOverload();

	/** Exit overload state */
	void ExitOverload();

	/** Apply damage to CharacterDataComponent */
	void ApplyDamageToActor(AActor* Target, float Damage);

	/** Process element absorption for stacks */
	void ProcessElementAbsorption(ESpellElement Element);

	/** Reset stacks (called when alignment changes) */
	void ResetStacks();

	/** Calculate energy gained from defense */
	float CalculateAbsorptionEnergy(EDefenseType DefenseType, float AttackEnergyCost) const;

	/** Trigger transformation (internal) */
	void TriggerTransformation();

protected:
	// ==================== TRANSFORMATION STATE ====================

	/** Has this character transformed into BrokenDarkness? */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness")
	bool bIsTransformed = false;

	// ==================== ABSORPTION ENERGY ====================

	/** Current absorption energy (replaces normal energy for transformed) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float AbsorptionEnergy = 0.0f;

	/** Maximum absorption energy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float MaxAbsorptionEnergy = 100.0f;

	/** Energy gained per damage absorbed via parry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float ParryAbsorptionRate = 1.0f;

	/** Energy gained per damage absorbed via block */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float BlockAbsorptionRate = 0.5f;

	// ==================== ABSORBED ELEMENTS ====================

	/** Elements absorbed this combat (for hybrid spells) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Elements")
	TArray<ESpellElement> AbsorbedElements;

	/** Last element absorbed (used for hybrid spell element) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Elements")
	ESpellElement LastAbsorbedElement = ESpellElement::Generic;

	// ==================== OVERLOAD STATE ====================

	/** Is currently in overload state? */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Overload")
	bool bIsOverloaded = false;

	/** Overload capacity (energy can exceed max by this amount) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Overload")
	float OverloadCapacity = 30.0f;

	/** Base damage per turn while overloaded (to enemies in aura) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Overload")
	float BaseOverloadAuraDamage = 15.0f;

	/** Base self-damage per turn while overloaded */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Overload")
	float BaseOverloadSelfDamage = 15.0f;

	/** Base energy drain per turn while overloaded */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Overload")
	float BaseEnergyDrain = 15.0f;

	// ==================== ABSORPTION STACKS ====================

	/** Current alignment element (determines hybrid spell element) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Stacks")
	ESpellElement CurrentAlignmentElement = ESpellElement::Generic;

	/** Current absorption stacks for the active element (0-3) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Stacks")
	int32 CurrentAbsorptionStacks = 0;

	/** Maximum absorption stacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Stacks")
	int32 MaxAbsorptionStacks = 3;

	/** Consecutive absorptions of current element */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Stacks")
	int32 ConsecutiveAbsorptions = 0;

	// ==================== FORBIDDEN ELEMENTS ====================

	/** Self-damage percentage when casting forbidden elements (Light/Void) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Forbidden")
	float ForbiddenCastSelfDamagePercent = 0.25f;  // 25% of spell damage to self
};
