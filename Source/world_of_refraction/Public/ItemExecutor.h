// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "ItemEffectType.h"
#include "SpellElement.h"
#include "ItemExecutor.generated.h"

class UItemData;
class UCharacterDataComponent;
class UCharacterData;
class UStatusEffectManager;

// ========================================
// QUARTZ TRANSFORM TRACKING
// ========================================

/**
 * Tracks elemental damage absorbed by Quartz crystals
 * Used to determine transformation result
 */
USTRUCT(BlueprintType)
struct FQuartzAbsorptionState
{
	GENERATED_BODY()

	/** Damage absorbed per element */
	UPROPERTY(BlueprintReadOnly, Category = "Quartz")
	TMap<ESpellElement, float> AbsorbedDamage;

	/** Total damage absorbed */
	UPROPERTY(BlueprintReadOnly, Category = "Quartz")
	float TotalAbsorbed = 0.0f;

	/** Transformation threshold (from item tier) */
	UPROPERTY(BlueprintReadOnly, Category = "Quartz")
	float TransformThreshold = 100.0f;

	/** Has transformation occurred? */
	UPROPERTY(BlueprintReadOnly, Category = "Quartz")
	bool bHasTransformed = false;

	/** Resulting element after transformation */
	UPROPERTY(BlueprintReadOnly, Category = "Quartz")
	ESpellElement TransformedElement = ESpellElement::Generic;

	void AbsorbDamage(ESpellElement Element, float Amount)
	{
		// Don't absorb Generic or BrokenDarkness
		if (Element == ESpellElement::Generic || Element == ESpellElement::BrokenDarkness)
			return;

		float &Current = AbsorbedDamage.FindOrAdd(Element);
		Current += Amount;
		TotalAbsorbed += Amount;
	}

	ESpellElement GetDominantElement() const
	{
		ESpellElement Dominant = ESpellElement::Generic;
		float MaxDamage = 0.0f;

		for (const auto &Pair : AbsorbedDamage)
		{
			if (Pair.Value > MaxDamage)
			{
				MaxDamage = Pair.Value;
				Dominant = Pair.Key;
			}
		}
		return Dominant;
	}

	bool CanTransform() const
	{
		return !bHasTransformed && TotalAbsorbed >= TransformThreshold;
	}
};

/**
 * FItemUseResult
 * Detailed result of using an item
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FItemUseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ErrorMessage;

	// Primary effect results
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DamageDealt = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 HealingDone = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 EnergyRestored = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 SelfDamageTaken = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 BuffsApplied = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 DebuffsRemoved = 0;

	// Secondary effect results (S-tier)
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bAppliedSecondaryEffect = false;

	// Character-specific bonuses
	UPROPERTY(BlueprintReadOnly, Category = "Result|Bonuses")
	int32 GenericResistanceApplied = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Bonuses")
	int32 BrokenDarknessEnergyGained = 0;

	// Gamble results (Amethyst)
	UPROPERTY(BlueprintReadOnly, Category = "Result|Gamble")
	bool bWasGamble = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Gamble")
	bool bGambleWon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Gamble")
	FString GambleOutcome;

	// Quartz results
	UPROPERTY(BlueprintReadOnly, Category = "Result|Quartz")
	bool bQuartzTransformed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result|Quartz")
	ESpellElement QuartzNewElement = ESpellElement::Generic;
};

// ========================================
// DELEGATES
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, AActor *, User, UItemData *, Item, const FItemUseResult &, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuartzTransformed, AActor *, Owner, ESpellElement, NewElement, UItemData *, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGambleResult, AActor *, User, bool, bWon, const FString &, Outcome);

/**
 * UItemExecutor
 *
 * Handles all item execution logic including:
 * - All 10 crystal types with tier scaling
 * - Generic character resistance bonuses
 * - Broken Darkness energy bonuses
 * - Citrine self-damage
 * - S-tier secondary effects (burn DOT)
 * - Amethyst gambling
 * - Quartz transformation tracking
 * - Iolite tiered cleansing
 *
 * Usage:
 *   UItemExecutor* ItemExec = GetGameInstance()->GetSubsystem<UItemExecutor>();
 *   FItemUseResult Result = ItemExec->UseItem(User, Item, Target);
 */
UCLASS()
class WORLD_OF_REFRACTION_API UItemExecutor : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// MAIN EXECUTION
	// ========================================

	/**
	 * Use an item on target(s)
	 * Handles all crystal types, bonuses, and special mechanics
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor")
	FItemUseResult UseItem(AActor *User, UItemData *Item, AActor *Target);

	/**
	 * Use an item on multiple targets (AOE items)
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor")
	FItemUseResult UseItemMultiTarget(AActor *User, UItemData *Item, const TArray<AActor *> &Targets);

	// ========================================
	// QUARTZ SYSTEM
	// ========================================

	/**
	 * Register a Quartz crystal for transformation tracking
	 * Call when Quartz is equipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor|Quartz")
	void RegisterQuartz(AActor *Owner, UItemData *QuartzItem);

	/**
	 * Unregister Quartz (unequipped or transformed)
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor|Quartz")
	void UnregisterQuartz(AActor *Owner);

	/**
	 * Notify Quartz of incoming elemental damage
	 * Called by damage system when owner takes elemental damage
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor|Quartz")
	void NotifyQuartzDamage(AActor *Owner, ESpellElement Element, float Damage);

	/**
	 * Check if actor has Quartz that can transform
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor|Quartz")
	bool CanQuartzTransform(AActor *Owner) const;

	/**
	 * Force Quartz transformation (if threshold met)
	 * Returns the new element, or None if cannot transform
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor|Quartz")
	ESpellElement TransformQuartz(AActor *Owner);

	/**
	 * Get current Quartz absorption state
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor|Quartz")
	FQuartzAbsorptionState GetQuartzState(AActor *Owner) const;

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Item Executor|Events")
	FOnItemUsed OnItemUsed;

	UPROPERTY(BlueprintAssignable, Category = "Item Executor|Events")
	FOnQuartzTransformed OnQuartzTransformed;

	UPROPERTY(BlueprintAssignable, Category = "Item Executor|Events")
	FOnGambleResult OnGambleResult;

	// ========================================
	// DEBUG
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Item Executor|Debug", CallInEditor)
	void DebugPrintQuartzStates() const;

private:
	// ========================================
	// EFFECT HANDLERS
	// ========================================

	/** Garnet - Direct damage */
	void ExecuteDamageEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Sapphire - Healing */
	void ExecuteHealingEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Citrine - Energy restore with self-damage */
	void ExecuteEnergyRestoreEffect(AActor *User, UItemData *Item, FItemUseResult &OutResult);

	/** Emerald - Speed buff */
	void ExecuteSpeedBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Amber - Defense buff */
	void ExecuteDefenseBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Opal - Crit buff (+ S-tier reveals) */
	void ExecuteCritBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Onyx - Silence */
	void ExecuteSilenceEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Amethyst - Gamble (random effect) */
	void ExecuteGambleEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Iolite - Cleanse (tiered removal + immunity) */
	void ExecuteCleanseEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Quartz - Transform (passive, this just activates it) */
	void ExecuteTransformEffect(AActor *User, UItemData *Item, FItemUseResult &OutResult);

	// ========================================
	// BONUS HANDLERS
	// ========================================

	/** Apply Generic character resistance bonus */
	void ApplyGenericBonus(AActor *User, UItemData *Item, FItemUseResult &OutResult);

	/** Apply Broken Darkness energy bonus */
	void ApplyBrokenDarknessBonus(AActor *User, UItemData *Item, FItemUseResult &OutResult);

	/** Apply S-tier secondary effects (burn DOT) */
	void ApplySecondaryEffect(AActor *Target, UItemData *Item, AActor *Source, FItemUseResult &OutResult);

	// ========================================
	// HELPERS
	// ========================================

	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;
	UCharacterData *GetCharacterData(AActor *Actor) const;
	UStatusEffectManager *GetStatusEffectManager() const;
	bool IsGenericCharacter(AActor *Actor) const;
	bool IsBrokenDarknessCharacter(AActor *Actor) const;

	// ========================================
	// STATE
	// ========================================

	/** Quartz absorption tracking per actor */
	TMap<TWeakObjectPtr<AActor>, FQuartzAbsorptionState> QuartzStates;

	/** Quartz item references */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UItemData>> QuartzItems;

	/** Cached StatusEffectManager */
	UPROPERTY()
	UStatusEffectManager *StatusEffectManagerRef = nullptr;
};
