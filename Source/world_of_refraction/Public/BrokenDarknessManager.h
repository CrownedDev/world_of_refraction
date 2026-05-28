// BrokenDarknessManager.h
// Component for handling BrokenDarkness transformation and absorption energy

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ESpellElement.h"
#include "EDefenseType.h"
#include "DefenseSystem.h"
#include "ItemTier.h"
#include "BrokenDarknessManager.generated.h"

class USpellData;
class UAbilityData;
class UCharacterData;
class UDefenseSystem;
class UCharacterDataComponent;

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrokenDarknessTransformed, AActor *, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnergyAbsorbed, AActor *, Actor, float, AmountAbsorbed, ESpellElement, AbsorbedElement);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOverloadStateChanged, AActor *, Actor, bool, bIsOverloaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStacksChanged, AActor *, Actor, ESpellElement, Element, int32, NewStackCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAlignmentChanged, AActor *, Actor, ESpellElement, OldElement, ESpellElement, NewElement);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOverloadDamage, AActor *, Source, AActor *, Target, float, Damage);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ==================== TRANSFORMATION STATE ====================

	/** Has this character transformed into BrokenDarkness? */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness")
	bool IsTransformed() const { return bIsTransformed; }

	// ==================== BREAK SYSTEM ====================

	/**
	 * Roll for Broken Darkness transformation. Chance is tier-keyed and
	 * infusion-multiplied:
	 *   Tier base: S=1.5%, A=1.0%, B=0.6%, C=0.3%, D=0.1%, E/F=0%
	 *   L0 = base, L1 = base × 1.5, L2 = base × 2.0
	 * Triggered (via ActionExecutor::CheckBrokenDarknessBreak) when an
	 * innate-Darkness character: casts a spell above its stat requirements;
	 * casts an L1/L2-infused spell; or uses an ability that is both above
	 * requirements and infused from a Darkness source.
	 * @param Tier The tier of the spell/ability that triggered the roll
	 * @param InfusionLevel The infusion level of the cast (0/1/2)
	 * @param TriggerReason Debug string for logging what triggered the roll
	 * @return True if transformation occurred
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Break")
	bool RollForBreak(EItemTier Tier, int32 InfusionLevel, const FString &TriggerReason);

	/**
	 * Check if spell exceeds character's stat requirements
	 * @param Spell The spell being cast
	 * @param Character The character's data
	 * @return True if any world stat is below spell requirements
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Break")
	static bool DoesSpellExceedRequirements(USpellData *Spell, UCharacterData *Character);

	/**
	 * Check if ability exceeds character's stat requirements
	 * @param Ability The ability being used
	 * @param Character The character's data
	 * @return True if any world stat is below ability requirements
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Break")
	static bool DoesAbilityExceedRequirements(UAbilityData *Ability, UCharacterData *Character);

	/**
	 * Debug/test hook — forces an unconditional transformation into Broken
	 * Darkness. Bypasses both the break roll (RollForBreak) and the
	 * InnateElement == Darkness eligibility gate, so any character can be
	 * transformed regardless of class or element.
	 *
	 * Intended only for debug paths — CallInEditor buttons, console commands,
	 * automated test code. Not part of the production transformation flow
	 * (that is RollForBreak via ActionExecutor::CheckBrokenDarknessBreak).
	 * Intentionally retained despite having zero production callers.
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

	/**
	 * Grant absorption energy from a non-defense source (e.g. a crystal used
	 * on a BD via ItemExecutor). Routes through the same overload-aware path
	 * as parry/block absorption — writes UCharacterDataComponent::CurrentEP.
	 * No-op if not transformed.
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Absorption")
	void GrantAbsorptionEnergy(float Amount);

	// ==================== OVERLOAD STATE ====================

	/** Check if currently in overload state (Energy > MaxEnergy) */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	bool IsOverloaded() const { return bIsOverloaded; }

	/** Get current overload energy (amount over max), returns 0 if not overloaded */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	float GetOverloadEnergy() const;

	/** Overload capacity — how far CurrentEP may exceed MaxEP before the hard
	 *  cap. Derived: MaxEP × OVERLOAD_CAPACITY_FRACTION (30%), so the overload
	 *  window scales with the BD's stat-derived energy pool. */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	float GetOverloadCapacity() const;

	/**
	 * Process overload tick (call from turn manager at end of turn)
	 * Applies aura damage to enemies, self-damage, energy drain
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Overload")
	void ProcessOverloadTick(const TArray<AActor *> &NearbyEnemies, float StatusMultiplierBonus, float EfficiencyPercent);

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

	/**
	 * Element-gated stack multiplier for status-buildup amplification.
	 * Returns 1.0 when not transformed, or when Element does not match the
	 * current alignment; otherwise returns GetStackStatusMultiplier() (1x/1x/2x/4x).
	 * This is the live entry point consumed by UStatusBuildupManager::AddStatusBuildup
	 * — element gate lives here so callers don't duplicate the matching-element check.
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	float GetElementStackStatusMultiplier(ESpellElement Element) const;

	/** Get maximum stacks allowed */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	int32 GetMaxStacks() const { return MaxAbsorptionStacks; }

	/** Check if at max stacks */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Stacks")
	bool IsAtMaxStacks() const { return CurrentAbsorptionStacks >= MaxAbsorptionStacks; }

	// ==================== HYBRID SPELLS ====================

	/** True if Element is the currently-active absorbed element — the most recent
	 *  absorption (AbsorbedElements.Last()). Absorption is a single active slot:
	 *  earlier entries in the absorbed-element history do NOT count as active. */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Hybrid")
	bool HasAbsorbedElement(ESpellElement Element) const;

	/**
	 * Element-capability predicate shared by combat (ActionExecutor::ValidateAction)
	 * and loadout (LoadoutComponent::GetValidationErrors) validation.
	 *
	 * Broken Darkness characters can cast Darkness (the BD default element) plus
	 * any element absorbed this session. Non-BD characters can cast their innate
	 * element, or anything if their innate element is itself an any-element
	 * source (Reality / BrokenDarkness).
	 *
	 * In both cases the cast is also unlocked when the character has an equipped
	 * crystal channelling the element — ring, weapon, or primary evolution slot
	 * (ULoadoutComponent::HasEquippedSourceForElement).
	 *
	 * Returns true (castable) when the character cannot be resolved — callers
	 * must not block on missing data.
	 *
	 * @param Actor     Casting actor — used to query equipped crystals (may be null)
	 * @param CharComp  Casting character's data component (may be null)
	 * @param BDManager Character's BrokenDarknessManager (may be null)
	 * @param Element   Spell element being checked
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Hybrid")
	static bool IsElementCastable(AActor *Actor,
								  UCharacterDataComponent *CharComp,
								  UBrokenDarknessManager *BDManager,
								  ESpellElement Element);

	/** Get hybrid element (Darkness + last absorbed) */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Hybrid")
	ESpellElement GetHybridElement() const { return LastAbsorbedElement; }

	// ==================== DEFENSE SYSTEM INTEGRATION ====================

	/**
	 * Called by DefenseSystem when a defense window closes
	 * Handles absorption based on defense type and result
	 */
	UFUNCTION(BlueprintCallable, Category = "BrokenDarkness|Defense")
	void OnDefenseResolved(EDefenseType DefenseType, const FDefenseResult &DefenseResult,
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

	/** Calculate current aura range based on MaxEnergy stat investment */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Overload")
	float CalculateAuraRange() const;

private:
	// ==================== PRIVATE METHODS ====================

	/** Resolve the owner's CharacterDataComponent — BD energy lives on its CurrentEP. */
	UCharacterDataComponent *GetCharComp() const;

	/** Bound to CharacterDataComponent::OnEPChanged. Re-evaluates overload on
	 *  every owner energy change — absorption gain, cast spend, and overload
	 *  drain all broadcast OnEPChanged, so this is the single overload trigger. */
	UFUNCTION()
	void HandleOwnerEnergyChanged(int32 InCurrentEP, int32 InMaxEP);

	/** Add absorption energy (writes CurrentEP, clamped to MaxEP + overload capacity) */
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
	void ApplyDamageToActor(AActor *Target, float Damage);

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
	// Broken Darkness energy is unified onto UCharacterDataComponent::CurrentEP
	// — the single spend pool for BD and non-BD characters alike. BD's gain
	// rule (event-driven absorption, no passive regen) survives via the
	// ServerGainEnergy BD early-out. The cap is the stat-derived MaxEP; energy
	// may exceed it by OverloadCapacity into overload.

	/** Energy gained per damage absorbed via parry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float ParryAbsorptionRate = 1.0f;

	/** Energy gained per damage absorbed via block */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BrokenDarkness|Energy")
	float BlockAbsorptionRate = 0.5f;

	// ==================== ABSORBED ELEMENTS ====================

	/** Absorbed-element history this combat. The most recent entry (Last()) is the
	 *  currently active absorbed element; earlier entries are historical and may
	 *  be referenced by future abilities (e.g. re-tapping a past absorption). */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Elements")
	TArray<ESpellElement> AbsorbedElements;

	/** Last element absorbed (used for hybrid spell element) */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Elements")
	ESpellElement LastAbsorbedElement = ESpellElement::Generic;

	// ==================== OVERLOAD STATE ====================

	/** Is currently in overload state? */
	UPROPERTY(BlueprintReadOnly, Category = "BrokenDarkness|Overload")
	bool bIsOverloaded = false;

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
	float ForbiddenCastSelfDamagePercent = 0.25f; // 25% of spell damage to self
};
