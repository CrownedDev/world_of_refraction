// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Actions/ActionStructs.h"
#include "Inventory/ItemEffectType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "ItemExecutor.generated.h"

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemUsed, AActor *, User, FCrystalId, ItemId, const FItemUseResult &, Result);
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
	FItemUseResult UseItem(AActor *User, FCrystalId Id, AActor *Target);

	/**
	 * Use an item on multiple targets (AOE items)
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Executor")
	FItemUseResult UseItemMultiTarget(AActor *User, FCrystalId Id, const TArray<AActor *> &Targets);

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
	void ExecuteDamageEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Sapphire - Healing */
	void ExecuteHealingEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Citrine - restore target EP %, apply Lightning buildup to the user */
	void ExecuteEnergyRestoreEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Emerald - Speed buff */
	void ExecuteSpeedBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Damage stone - self-buff raw (physical) damage % for a fixed duration */
	void ExecuteRawDamageBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Amber - Defense buff */
	void ExecuteDefenseBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Opal - Crit buff (+ S-tier reveals) */
	void ExecuteCritBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** TurnSpeedStone - directional turn-speed buff (ally) / debuff (enemy) at the
	 *  stone's 3-15 magnitude. Pure stat; no turn mechanic (separate from Emerald's
	 *  ExecuteSpeedBuffEffect). */
	void ExecuteTurnSpeedStoneEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** StatusStone - directional StatusMultiplier buff (ally) / debuff (enemy) at the
	 *  stone's 3-15 magnitude. Transient effect (Step-5b layer), separate from the
	 *  attached form's base-stat getter. */
	void ExecuteStatusBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** EfficiencyStone - directional Efficiency buff (ally) / debuff (enemy) at the
	 *  stone's 3-15 magnitude. Transient EfficiencyBuff/Debuff (H0) folded into the
	 *  unified getter, so it reaches BD drain / EP cost / durability automatically. */
	void ExecuteEfficiencyBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** MaxHPStone - directional MaxHP buff (ally) / debuff (enemy) at the stone's 3-15
	 *  magnitude. Transient MaxHPBuff/Debuff (P2a) folded into RecomputeMaxPools and the
	 *  P2b subscribe trigger recomputes the pool — no per-consumer pool wiring here. */
	void ExecuteMaxHPBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** MaxEPStone - directional MaxEnergy buff (ally) / debuff (enemy) at the stone's
	 *  3-15 magnitude. Transient MaxEnergyBuff/Debuff folded into RecomputeMaxPools via
	 *  the P2b trigger. */
	void ExecuteMaxEPBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Onyx - Silence */
	void ExecuteSilenceEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Amethyst - Gamble (random effect) */
	void ExecuteGambleEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Iolite - Cleanse (ally: remove debuffs / enemy: remove buffs) */
	void ExecuteCleanseEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	/** Quartz - StatusClear (reduce target status bar + grant elemental resistance) */
	void ExecuteStatusClearEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	// ========================================
	// BONUS HANDLERS
	// ========================================

	/** Apply Broken Darkness energy absorption — Target is the BD character a
	 *  crystal was used on; it absorbs EP scaled by the crystal's tier. */
	void ApplyBrokenDarknessBonus(AActor *Target, FCrystalId Id, FItemUseResult &OutResult);

	// ========================================
	// HELPERS
	// ========================================

	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;
	USkillEffectManager *GetSkillEffectManager() const;
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
