// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Character/ECharacterClass.h"
#include "Character/StatConstants.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Actions/EActionType.h"
#include <Combat/CombatConstants.h>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "CharacterData.generated.h"

class USpellData;
class UEvolutionItemData;
class UInventoryData;
class UCosmeticsData;

/**
 * Describes what a character loses/gains from evolution
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEvolutionCostResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	bool bCanEvolve = true;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	FString CostDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	FString GainDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Evolution")
	TArray<FString> Warnings;
};

/**
 * Read-only Details-panel mirror of a character's resolved class / innate-element
 * resistance row. Percent values (+resist / -weak), one field per column
 * (Fire..Reality | Slash Pierce Impact). VisibleAnywhere -> greyed, non-editable.
 *
 * NOT a gameplay data source: this is populated FROM
 * ClassInnateResistanceTable::ResolveRow (the single source of truth); the
 * buildup path reads ResolveRow directly, never this struct. Held transient on
 * UCharacterData so it is never serialized into the .uasset.
 *
 * DESIGN-TIME view: the BD signal is the asset's InnateElement == BrokenDarkness.
 * A live Broken Darkness transform resolves the BD row at runtime instead.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FResistanceProfileDisplay
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Fire = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Water = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Earth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Wind = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Light = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances",
			  meta = (ToolTip = "Incoming Broken Darkness attacks alias to this Darkness column."))
	float Darkness = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Lightning = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Void = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Reality = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Slash = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Pierce = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistances")
	float Impact = 0.0f;
};

/**
 * Character Data Asset - Contains all character stats, abilities, and visual data
 * Stats split between Initial (DNA) and World (progression) for clear tracking
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString Name = TEXT("Unnamed Character");

	/** Character class - determines combat style and equipment options */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ECharacterClass CharacterClass = ECharacterClass::Caster;

	/** Innate element - only for Casters (locked at creation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
			  meta = (EditCondition = "CharacterClass == ECharacterClass::Caster", EditConditionHides))
	ESpellElement InnateElement = ESpellElement::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Character description...");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	UTexture2D *Portrait = nullptr;

	// ==================== CONTROL ====================

	/** If true, this character is controlled by AI in combat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Control")
	bool bIsAIControlled = false;

	// ==================== INVENTORY ====================

	/** The character's inventory asset. References the items they own and the
	 *  saved loadout configurations they can switch between. Sole source of
	 *  truth for runtime loadout population. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	UInventoryData *Inventory = nullptr;

	// ==================== WORLD STAT LEVELS ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses",
			  meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldMindLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses",
			  meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldBodyLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|World Bonuses",
			  meta = (ClampMin = "0", ClampMax = "7"))
	int32 WorldSpiritLevel = 0;

	// ==================== INITIAL SUB-STATS (CHARACTER DNA) ====================
	// ==================== MIND SUB-STATS (4) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 Efficiency = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 SpellDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 CritChance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Mind", meta = (ClampMin = "0"))
	int32 SpellSpeed = 0;

	// ==================== BODY SUB-STATS (5) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 Defense = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 ActionSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 RawDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 MaxHealth = 0;

	/** Widens the real-time defense input window (more Reflex = more lead-in time to
	 *  land a parry/dodge). Scales ON TOP of UDefenseSystem::DefenseInputWindow — see
	 *  CalculateReflexWindowBonus. Does NOT affect ActionSpeed (animation pacing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Body", meta = (ClampMin = "0"))
	int32 Reflex = 0;

	// ==================== SPIRIT SUB-STATS (5) ====================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 MaxEnergy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 Resistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 TurnSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 Luck = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub-Stats|Spirit", meta = (ClampMin = "0"))
	int32 StatusMultiplier = 0;

	// ==================== COSMETICS ====================

	/** Visual/animation data (defense montages, stances, item-use montages,
	 *  infusion VFX, weather variant). Sole source of truth — read sites query
	 *  this asset directly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	UCosmeticsData *Cosmetics = nullptr;

	// ==================== CLASS HELPERS ====================

	/** Check if this character should use AI control */
	UFUNCTION(BlueprintPure, Category = "Character|Control")
	bool ShouldUseAI() const { return bIsAIControlled; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool IsGeneric() const { return CharacterClass == ECharacterClass::Generic; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool IsCaster() const { return CharacterClass == ECharacterClass::Caster; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool IsResonator() const { return CharacterClass == ECharacterClass::Resonator; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool CanUseSpells() const { return CharacterClass != ECharacterClass::Generic; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool CanUseAbilities() const { return CharacterClass == ECharacterClass::Generic; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool HasInnateElement() const { return CharacterClass == ECharacterClass::Caster; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool UsesRings() const { return CharacterClass == ECharacterClass::Resonator; }

	UFUNCTION(BlueprintPure, Category = "Character|Class")
	bool CanDualWield() const { return CharacterClass == ECharacterClass::Generic; }

	/** Get element - Caster returns InnateElement, others return Generic */
	UFUNCTION(BlueprintPure, Category = "Character|Class")
	ESpellElement GetElement() const
	{
		return IsCaster() ? InnateElement : ESpellElement::Generic;
	}

	// ==================== RESISTANCES (read-only inspection) ====================
	// Display-layer query over the code-side ClassInnateResistanceTable. Resolves
	// this character's profile row (BD > Generic > Resonator > Caster-by-element)
	// and returns the resistance fraction for a given incoming axis. Asset-side BD
	// is the character-created case (InnateElement == BrokenDarkness); the runtime
	// transform flag lives on UCharacterDataComponent and is honoured by the
	// actor-level ClassInnateResistanceTable::GetClassInnateResistance instead.

	/** Read-only Details-panel mirror of the resolved profile row. Transient —
	 *  never serialized; the C++ table stays the single source of truth. Refreshed
	 *  from ResolveRow on load and whenever CharacterClass / InnateElement change.
	 *  DISPLAY ONLY — no code path reads this as data. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Resistances",
			  meta = (ToolTip = "Innate class/element resistance profile (percent, +resist / -weak). Design-time view: a live Broken Darkness transform resolves the BD row at runtime."))
	FResistanceProfileDisplay ResistanceProfile;

	/** Resistance fraction (+resist / -weak) to an incoming attack ELEMENT.
	 *  Incoming BrokenDarkness aliases to the Darkness column. */
	UFUNCTION(BlueprintPure, Category = "Resistances")
	float GetElementResistance(ESpellElement IncomingElement) const
	{
		const ClassInnateResistanceTable::FResistanceRow Row =
			ClassInnateResistanceTable::ResolveRow(CharacterClass, GetElement(), InnateElement == ESpellElement::BrokenDarkness);
		return ClassInnateResistanceTable::GetElementColumn(Row, IncomingElement) / ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}

	/** Resistance fraction (+resist / -weak) to an incoming PHYSICAL type.
	 *  None contributes 0. */
	UFUNCTION(BlueprintPure, Category = "Resistances")
	float GetPhysicalResistance(EPhysicalDamageType PhysicalType) const
	{
		const ClassInnateResistanceTable::FResistanceRow Row =
			ClassInnateResistanceTable::ResolveRow(CharacterClass, GetElement(), InnateElement == ESpellElement::BrokenDarkness);
		return ClassInnateResistanceTable::GetPhysicalColumn(Row, PhysicalType) / ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}

	// ==================== STAT BUDGET VALIDATION ====================
	// ==================== STAT POOL ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Pool")
	int32 GetTotalPool() const
	{
		return StatConstants::INITIAL_STAT_BUDGET +
			   ((WorldMindLevel + WorldBodyLevel + WorldSpiritLevel) * StatConstants::POINTS_PER_WORLD_STAT_LEVEL);
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Pool")
	int32 GetTotalSpent() const
	{
		return Efficiency + SpellDamage + CritChance + SpellSpeed +
			   Defense + ActionSpeed + RawDamage + MaxHealth + Reflex +
			   MaxEnergy + Resistance + TurnSpeed + Luck + StatusMultiplier;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Pool")
	int32 GetPointsRemaining() const { return GetTotalPool() - GetTotalSpent(); }

	UFUNCTION(BlueprintPure, Category = "Stats|Validation")
	bool IsValidDistribution() const { return GetTotalSpent() <= GetTotalPool(); }

	// ==================== TOTAL SUB-STATS ====================
	// Initial + World points combined

	// ----- MIND (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalEfficiency() const { return Efficiency; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalSpellDamage() const { return SpellDamage; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalCritChance() const { return CritChance; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Mind|Total")
	int32 GetTotalSpellSpeed() const { return SpellSpeed; }

	// ----- BODY (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalDefense() const { return Defense; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalActionSpeed() const { return ActionSpeed; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalRawDamage() const { return RawDamage; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Body|Total")
	int32 GetTotalReflex() const { return Reflex; }

	// ----- SPIRIT (4 stats) -----
	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalMaxEnergy() const { return MaxEnergy; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalResistance() const { return Resistance; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalTurnSpeed() const { return TurnSpeed; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalLuck() const { return Luck; }

	UFUNCTION(BlueprintPure, Category = "Sub-Stats|Spirit|Total")
	int32 GetTotalStatusMultiplier() const { return StatusMultiplier; }

	// ==================== BASE STATS (DERIVED FROM TOTALS) ====================
	// Sum of sub-stat points per category (before world level scaling)

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseMind() const
	{
		// Mind (4): Efficiency, SpellDamage, CritChance, SpellSpeed
		return GetTotalEfficiency() + GetTotalSpellDamage() + GetTotalCritChance() + GetTotalSpellSpeed();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseBody() const
	{
		// Body (5): Defense, ActionSpeed, RawDamage, MaxHealth, Reflex
		return GetTotalDefense() + GetTotalActionSpeed() + GetTotalRawDamage() + GetTotalMaxHealth() + GetTotalReflex();
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Base")
	int32 GetBaseSpirit() const
	{
		// Spirit (5): MaxEnergy, Resistance, TurnSpeed, Luck, StatusMultiplier
		return GetTotalMaxEnergy() + GetTotalResistance() + GetTotalTurnSpeed() + GetTotalLuck() + GetTotalStatusMultiplier();
	}

	// ==================== EFFECTIVE STATS (WITH SCALING) ====================

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveMind() const
	{
		// Decoupled — world multiplier only (1.0-1.49); substats scale off own points × this.
		// No pillar sum = no cross-amplification. Snowball removed (see CombatEconomy_StatRedesign.md).
		return 1.0f + WorldMindLevel * CombatConstants::WORLD_MIND_SCALING_BONUS;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveBody() const
	{
		// Decoupled — world multiplier only (1.0-1.49); substats scale off own points × this.
		// No pillar sum = no cross-amplification. Snowball removed (see CombatEconomy_StatRedesign.md).
		return 1.0f + WorldBodyLevel * CombatConstants::WORLD_BODY_SCALING_BONUS;
	}

	UFUNCTION(BlueprintPure, Category = "Stats|Effective")
	float GetEffectiveSpirit() const
	{
		// Decoupled — world multiplier only (1.0-1.49); substats scale off own points × this.
		// No pillar sum = no cross-amplification. Snowball removed (see CombatEconomy_StatRedesign.md).
		return 1.0f + WorldSpiritLevel * CombatConstants::WORLD_SPIRIT_SCALING_BONUS;
	}

	// ==================== MIND CALCULATIONS ====================
	// Mind (4): Efficiency, SpellDamage, CritChance, SpellSpeed

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateEfficiencyMultiplier() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalEfficiency();
		return FMath::Clamp(
			1.0f - (EffectiveMind * TotalPoints * CombatConstants::EFFICIENCY_PER_POINT),
			1.0f - CombatConstants::EFFICIENCY_MAX,
			1.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateEfficiencyRingBreakReduction() const
	{
		// Only Resonators get ring break reduction from Efficiency
		if (CharacterClass != ECharacterClass::Resonator)
			return 0.0f;

		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalEfficiency();
		return FMath::Clamp(
			EffectiveMind * TotalPoints * CombatConstants::EFFICIENCY_RING_BREAK_PER_POINT,
			0.0f,
			CombatConstants::EFFICIENCY_RING_BREAK_MAX);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateStatusMultiplier() const
	{
		// Drives status buildup scaling (consumed by StatusBuildupManager and
		// the per-skill CalculateStatusBuildup helpers). Spirit-driven — moved
		// off Mind alongside the StatusMultiplier substat pillar move.
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalStatusMultiplier();
		return 1.0f + (EffectiveSpirit * TotalPoints * CombatConstants::STATUS_MULTIPLIER_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateSpellDamage() const
	{
		// Spell damage multiplier — applied once via
		// DamageCalculator::GetAttackerDamageMultiplier for EActionType::Spell.
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalSpellDamage();
		return 1.0f + (EffectiveMind * TotalPoints * CombatConstants::SPELL_DAMAGE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateCritChance() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalCritChance();
		return FMath::Clamp(
			CombatConstants::CRIT_CHANCE_BASE + (EffectiveMind * TotalPoints * CombatConstants::CRIT_CHANCE_PER_POINT),
			CombatConstants::CRIT_CHANCE_BASE,
			1.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Mind")
	float CalculateSpellSpeed() const
	{
		float EffectiveMind = GetEffectiveMind();
		int32 TotalPoints = GetTotalSpellSpeed();
		return CombatConstants::SPELL_SPEED_BASE + (EffectiveMind * TotalPoints * CombatConstants::SPELL_SPEED_PER_POINT);
	}

	// ==================== BODY CALCULATIONS ====================
	// Body (5): Defense, ActionSpeed, RawDamage, MaxHealth, Reflex
	// (MaxHealth lives in HELPER FUNCTIONS below; Reflex's window bonus here.)

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	int32 CalculateFlatDefense() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalDefense();
		return FMath::RoundToInt(EffectiveBody * TotalPoints * CombatConstants::DEFENSE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateActionSpeed() const
	{
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalActionSpeed();
		return CombatConstants::MOVEMENT_SPEED_BASE * (1.0f + EffectiveBody * TotalPoints * CombatConstants::MOVEMENT_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateAnimationSpeed() const
	{
		// Uses same stat as ActionSpeed
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalActionSpeed();
		return CombatConstants::ANIMATION_SPEED_BASE + (EffectiveBody * TotalPoints * CombatConstants::ANIMATION_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateReflexWindowBonus() const
	{
		// Additive SECONDS layered on top of UDefenseSystem::DefenseInputWindow (the
		// tuned base stays on the subsystem). Mirrors the CalculateActionSpeed shape.
		// Reads RAW GetEffectiveBody() — asset-intrinsic, NOT gear-aware yet (gear
		// widening of the window is deferred — see RealTimeDefenseRework §5).
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalReflex();
		return EffectiveBody * TotalPoints * CombatConstants::REFLEX_WINDOW_PER_POINT;
	}

	/** ATTACKER side of the defense-window duel: how many SECONDS this character's speed
	 *  shaves off a defender's input window. Exact mirror of CalculateReflexWindowBonus's
	 *  shape (EffectivePillar × RAW speed points × weight) so equal points + equal world
	 *  level cancel to the base window. Reads the RAW speed substat point getters — NOT
	 *  CalculateActionSpeed/CalculateSpellSpeed (those carry bases + a ×400 movement scale
	 *  that would obliterate the window). Type-aware: physical (Attack/Ability) narrows via
	 *  ActionSpeed/Body; Spell narrows via SpellSpeed/Mind (cross-pillar by design — clean
	 *  cancel for physical, well-defined for spell). NOTE: the spell branch is DORMANT until
	 *  Stage 6 — per-impact resolution is melee-only today, so only the physical branch is
	 *  exercised. Consumed by UDefenseSystem::GetEffectiveDefenseInputWindow. */
	UFUNCTION(BlueprintPure, Category = "Combat|Body")
	float CalculateSpeedWindowPenalty(EActionType AttackType) const
	{
		const bool bSpell = (AttackType == EActionType::Spell);
		const float EffectivePillar = bSpell ? GetEffectiveMind() : GetEffectiveBody();
		const int32 SpeedPts = bSpell ? GetTotalSpellSpeed() : GetTotalActionSpeed();
		return EffectivePillar * SpeedPts * CombatConstants::WINDOW_PER_SPEED_POINT;
	}

	// ==================== SPIRIT CALCULATIONS ====================
	// Spirit (5): MaxEnergy, Resistance, TurnSpeed, Luck, StatusMultiplier

	/** Turn-speed formula with the Spirit input supplied by the caller — the asset
	 *  owns the math; the caller picks WHICH Spirit. The turn-order path
	 *  (TurnManager::CacheActorStats) passes the component's pillar-MODIFIED Spirit;
	 *  CalculateTurnSpeed below passes raw GetEffectiveSpirit(). */
	float CalculateTurnSpeedWithSpirit(float SpiritValue) const
	{
		return CombatConstants::TURN_SPEED_BASE + (SpiritValue * GetTotalTurnSpeed() * CombatConstants::TURN_SPEED_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateTurnSpeed() const
	{
		// NOTE: Moved from Mind to Spirit. Raw-Spirit wrapper (asset-intrinsic value,
		// used by debug prints); combat pacing uses CalculateTurnSpeedWithSpirit with
		// the component's pillar-modified Spirit.
		return CalculateTurnSpeedWithSpirit(GetEffectiveSpirit());
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateLuck() const
	{
		// Multi-system fortune stat. Returns raw luck multiplier (0.0+, no upper
		// cap on input). LUCK_RAW_MAX is the per-consumer NORMALIZATION basis
		// (the "fully lucky" point), not an input ceiling — each consumer clamps
		// its own normalized fraction (LUCK_CRIT_BONUS_MAX, LUCK_DODGE_MAX,
		// LUCK_BREAK_SKIP_MAX, LUCK_DROP_CHANCE_MAX, LUCK_DROP_QUALITY_MAX).
		// Same shape as Resistance, minus the upper clamp.
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalLuck();
		return FMath::Max(
			0.0f,
			EffectiveSpirit * TotalPoints * CombatConstants::LUCK_PER_POINT);
	}

	// ==================== HELPER FUNCTIONS ====================

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateMaxHealth() const
	{
		// NOTE: Moved from Spirit to Body. HP is a physical pool stat.
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalMaxHealth();
		return FMath::RoundToInt(CombatConstants::MAX_HEALTH_BASE + (EffectiveBody * TotalPoints * CombatConstants::MAX_HEALTH_PER_POINT));
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	int32 CalculateMaxEnergy() const
	{
		// Now uses explicit MaxEnergy stat (Spirit-based)
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalMaxEnergy();
		return FMath::RoundToInt(CombatConstants::MAX_ENERGY_BASE + (EffectiveSpirit * TotalPoints * CombatConstants::MAX_ENERGY_PER_POINT));
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Helpers")
	float CalculateRawDamage() const
	{
		// Raw damage multiplier for physical attacks (Body-based)
		float EffectiveBody = GetEffectiveBody();
		int32 TotalPoints = GetTotalRawDamage();
		return 1.0f + (EffectiveBody * TotalPoints * CombatConstants::RAW_DAMAGE_PER_POINT);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	int32 CalculateStatusMultiplierFlat() const
	{
		// Flat StatusMultiplier point value (debug inspection). Spirit-driven
		// after the pillar move from Mind.
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalStatusMultiplier();
		return FMath::RoundToInt(EffectiveSpirit * TotalPoints);
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Spirit")
	float CalculateResistance() const
	{
		// Reduces status effect damage and buildup
		float EffectiveSpirit = GetEffectiveSpirit();
		int32 TotalPoints = GetTotalResistance();
		return FMath::Clamp(
			EffectiveSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT,
			0.0f,
			CombatConstants::RESISTANCE_MAX);
	}

	//~ UObject lifecycle — keep the transient ResistanceProfile display mirror in
	//~ sync with the code-side table. Populate on construct + load; the editor
	//~ live-update is in PostEditChangeProperty below. Display only.
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

private:
	/** Repopulate the transient ResistanceProfile mirror from
	 *  ClassInnateResistanceTable::ResolveRow (the single source of truth).
	 *  Design-time BD signal = InnateElement == BrokenDarkness. Display only —
	 *  never feeds GetClassInnateResistance or the buildup path. */
	void RefreshResistanceProfileDisplay();
};