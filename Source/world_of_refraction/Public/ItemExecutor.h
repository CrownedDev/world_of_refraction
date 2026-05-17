// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "ItemEffectType.h"
#include "ESpellElement.h"
#include "ESkillEffectType.h"
#include "ItemExecutor.generated.h"

class UItemData;
class UCharacterDataComponent;
class UCharacterData;
class USkillEffectManager;

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
};

// ========================================
// DELEGATES
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, AActor *, User, UItemData *, Item, const FItemUseResult &, Result);
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
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Item Executor|Events")
	FOnItemUsed OnItemUsed;

	UPROPERTY(BlueprintAssignable, Category = "Item Executor|Events")
	FOnGambleResult OnGambleResult;

private:
	// ========================================
	// EFFECT HANDLERS
	// ========================================

	/** Garnet - Direct damage */
	void ExecuteDamageEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Sapphire - Healing */
	void ExecuteHealingEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Citrine - restore target EP %, apply Lightning buildup to the user */
	void ExecuteEnergyRestoreEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

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

	/** Iolite - Cleanse (ally: remove debuffs / enemy: remove buffs) */
	void ExecuteCleanseEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

	/** Quartz - StatusClear (reduce target status bar + grant elemental resistance) */
	void ExecuteStatusClearEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult);

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
	USkillEffectManager *GetSkillEffectManager() const;
	bool IsGenericCharacter(AActor *Actor) const;
	bool IsBrokenDarknessCharacter(AActor *Actor) const;

	/** True if User and Target are on the same team (resolved via UTurnManager). */
	bool IsAlly(AActor *User, AActor *Target) const;

	/** Map an element to its GrantXxxImmunity effect type (None for Generic/BrokenDarkness).
	 *  Mirrors UStatusBuildupManager's private resolver, which UItemExecutor cannot reach. */
	static ESkillEffectType GetElementImmunityType(ESpellElement Element);

	// ========================================
	// STATE
	// ========================================

	/** Cached SkillEffectManager */
	UPROPERTY()
	USkillEffectManager *SkillEffectManagerRef = nullptr;
};
