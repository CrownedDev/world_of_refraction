// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/EActionType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Infusion/EChargeInfusionType.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "ActionStructs.generated.h"

class USpellData;
class UAbilityData;
class UWeaponAttackData;

/**
 * FAction
 * Represents a combat action to be executed
 * Filled by UI/AI and passed to ActionExecutor
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAction
{
	GENERATED_BODY()

	/** Type of action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	EActionType ActionType = EActionType::None;

	/** Target(s) of the action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TArray<AActor *> Targets;

	/** Spell data (if ActionType == Spell) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	USpellData *SpellData = nullptr;

	/** Ability data (if ActionType == Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UAbilityData *AbilityData = nullptr;

	/** Item crystal identity (when ActionType == Item).
	 *  The slot index in the active loadout is not carried — ActionExecutor
	 *  finds the matching slot by FCrystalId at execution time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	FCrystalId ItemData;

	/** Source of spell (Innate, Ring, Evolution, Item) - determines post-cast logic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Spell")
	ESpellSource SpellSource = ESpellSource::Innate;

	/** Attack data (if ActionType == Attack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Data")
	UWeaponAttackData *AttackData = nullptr;

	/** True only on the fire-time resubmission of a deferred activation (D8 —
	 *  set by the Stage 8c fire path). Costs were paid at ARM: the cost paths
	 *  (CalculateActionEnergyCost / ApplyCommitCosts) skip when this is set, and
	 *  the arm interception ignores the action's ActivationDelay so the fire
	 *  executes instead of re-arming. */
	UPROPERTY()
	bool bIsDeferredFire = false;

	// ==================== INFUSION OPTIONS ====================

	/** Selected infusion source - determines element and weapon stat trade-off
	 *  Set by player via Combat Menu BEFORE charging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion")
	EInfusionSourceOption SelectedSource = EInfusionSourceOption::None;

	/** Spell charge infusion level (0 = none, 1 = status boost, 2 = damage boost)
	 *  L1: 1.5x size, +50% status buildup, base damage
	 *  L2: 2.0x size, base status, +30% damage
	 *  Set by hold-to-charge for SPELLS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpellInfusionLevel = 0;

	/** Ability charge infusion level (0 = none, 1 = status boost, 2 = damage boost)
	 *  L1: +status buildup from SelectedSource (physical or element)
	 *  L2: +30% damage, no status
	 *  Set by hold-to-charge for ABILITIES */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Infusion", meta = (ClampMin = "0", ClampMax = "2"))
	int32 AbilityInfusionLevel = 0;

	// ==================== HELPERS ====================

	bool IsValid() const
	{
		if (ActionType == EActionType::None)
			return false;
		if (ActionType == EActionType::Spell && !SpellData)
			return false;
		if (ActionType == EActionType::Ability && !AbilityData)
			return false;
		if (ActionType == EActionType::Attack && !AttackData)
			return false;
		return true;
	}

	bool RequiresTarget() const
	{
		return ActionType != EActionType::Defend &&
			   ActionType != EActionType::Flee &&
			   ActionType != EActionType::SwitchWeapon;
	}

	FString GetActionName() const
	{
		switch (ActionType)
		{
		case EActionType::Spell:
			return SpellData ? TEXT("Spell") : TEXT("Unknown Spell");
		case EActionType::Ability:
			return AbilityData ? TEXT("Ability") : TEXT("Unknown Ability");
		case EActionType::Item:
			return ItemIdentity::GetDisplayName(ItemData);
		case EActionType::Attack:
			return AttackData ? TEXT("Attack") : TEXT("Basic Attack");
		case EActionType::Defend:
			return TEXT("Defend");
		case EActionType::SwitchWeapon:
			return TEXT("Switch Weapon");
		case EActionType::Flee:
			return TEXT("Flee");
		default:
			return TEXT("Unknown Action");
		}
	}

	/** Is spell charge infusion active? */
	bool IsSpellInfused() const
	{
		return SpellInfusionLevel > 0;
	}

	/** Is ability charge infusion active? */
	bool IsAbilityInfused() const
	{
		return AbilityInfusionLevel > 0;
	}

	/** Get the active charge level for current action type */
	int32 GetChargeLevel() const
	{
		if (ActionType == EActionType::Spell)
		{
			return SpellInfusionLevel;
		}
		else if (ActionType == EActionType::Ability)
		{
			return AbilityInfusionLevel;
		}
		return 0;
	}

	/** Is using physical source (weapon stats apply)? */
	bool IsPhysicalSource() const
	{
		return SelectedSource == EInfusionSourceOption::Raw;
	}

	/** Is using elemental source (weapon stats don't apply)? */
	bool IsElementalSource() const
	{
		return SelectedSource != EInfusionSourceOption::Raw && SelectedSource != EInfusionSourceOption::None;
	}
};

/**
 * FActionValidationResult
 * Result of validating an action before execution
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionValidationResult
{
	GENERATED_BODY()

	/** Is the action valid to execute? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bIsValid = false;

	/** Error message if not valid (shown to player) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FString ErrorMessage;

	/** Energy cost that would be spent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	int32 EnergyCost = 0;

	// Constructors
	FActionValidationResult() = default;
	FActionValidationResult(bool bValid, const FString &Error = TEXT(""), int32 Cost = 0)
		: bIsValid(bValid), ErrorMessage(Error), EnergyCost(Cost) {}
};

/**
 * FActionResult
 * Result of executing an action
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionResult
{
	GENERATED_BODY()

	/** Was execution successful? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bSuccess = false;

	/** What type of action was executed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	EActionType ActionType = EActionType::None;

	/** Who executed the action */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	AActor *Executor = nullptr;

	/** Who was affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TArray<AActor *> AffectedTargets;

	/** Energy spent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 EnergySpent = 0;

	/** Total damage dealt (sum across all targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 TotalDamageDealt = 0;

	/** Total healing done (sum across all targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 TotalHealingDone = 0;

	/** Was any hit a critical? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bWasCritical = false;

	/** Did any target die from this action? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bCausedDeath = false;

	/** Status effects applied to targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 StatusEffectsApplied = 0;

	/** Error message if failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FString ErrorMessage;

	// ==================== INFUSION TRACKING ====================

	/** Which source option was selected */
	UPROPERTY(BlueprintReadOnly, Category = "Result|Infusion")
	EInfusionSourceOption SourceOptionUsed = EInfusionSourceOption::None;

	/** Whether weapon stats were applied */
	UPROPERTY(BlueprintReadOnly, Category = "Result|Infusion")
	bool bWeaponStatsApplied = false;

	// ==================== DEFENSE SYSTEM DATA ====================

	/**
	 * Final spell/attack size for defense calculations
	 * Used by DefenseSystem to determine if dodge is viable
	 * Size > DodgeThreshold means dodge fails, must Block/Parry
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	float AttackSize = 0.0f;

	/** Base damage before defense applied (for defense calculations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	int32 BaseDamageBeforeDefense = 0;

	/** Element of the attack (for resistance calculations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Defense")
	ESpellElement AttackElement = ESpellElement::Generic;

	// Per-target damage breakdown (for UI/logging)
	TMap<AActor *, int32> DamagePerTarget;
};
// ==================== ASYNC EXECUTION CONTEXT ====================

/**
 * FPendingDefenseContext
 * Tracks a single target's pending defense resolution during async action execution
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPendingDefenseContext
{
	GENERATED_BODY()

	/** Target awaiting defense resolution */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Target;

	/** Attacker who initiated the attack */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Attacker;

	/** Base damage before defense */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 BaseDamage = 0;

	/** Attack size for dodge calculations */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	float AttackSize = 1.0f;

	/** Element of the attack */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	ESpellElement Element = ESpellElement::Generic;

	/** Number of hits to apply */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 HitCount = 1;

	/** Count-based window close (Stage 1): landed-impact tally for this defender,
	 *  incremented by each landed impact from ANY source (melee Impact notify today;
	 *  projectile arrival / AOE resolution / beam tick at Stage 6). */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 ImpactsLanded = 0;

	/** Expected impacts for this defender — the count the window closes at
	 *  (ImpactsLanded == ExpectedImpacts). Set by the OPENER per attack type, NOT
	 *  hardcoded to HitCount: melee = HitCount; single projectile/AOE = 1; beam =
	 *  BeamTickCount; barrage = FSkillCastEntry::Count (Stage 6). */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 ExpectedImpacts = 0;

	/** Can this attack crit? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bCanCrit = true;

	/** Damage per hit (for multi-hit attacks) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 DamagePerHit = 0;

	/** Window duration from attack data */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	float WindowDuration = 0.3f;

	/** Action category — drives ApplyHit damage-stat selection in ApplyDamageAfterDefense.
	 *  Plumbed through from the window-opening orchestrator so the post-defense applicator
	 *  knows whether to read SpellDamage (Spell) or RawDamage (Ability/Attack). */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EActionType ActionType = EActionType::None;

	/** Infusion level (0–2) carried through to FActionHitInput. Spells/abilities populate
	 *  from Action.SpellInfusionLevel/AbilityInfusionLevel; attacks pass 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 InfusionLevel = 0;

	/** Selected infusion source. Plumbed through for future buildup routing (Phase C). */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EInfusionSourceOption SelectedSource = EInfusionSourceOption::None;

	/** Base per-target buildup carried into ApplyDamageAfterDefense. Defense outcome
	 *  modifies this before it reaches ApplyHit (Phase C1 — spells only). */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 BaseStatusBuildup = 0;

	/** Physical damage type carried through the defense pipeline. Drives bar-cap
	 *  trigger resolution when Element is Generic (Session Y). None for spells
	 *  and abilities. */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::None;

	/** Unique ID for this defense context */
	UPROPERTY(BlueprintReadOnly, Category = "Defense", Meta = (IgnoreForMemberInitializationTest))
	FGuid ContextId;

	FPendingDefenseContext()
	{
		ContextId = FGuid::NewGuid();
	}
};

/**
 * FActionExecutionContext
 * Tracks an entire async action execution across multiple targets
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionExecutionContext
{
	GENERATED_BODY()

	/** Unique ID for this action execution */
	UPROPERTY(BlueprintReadOnly, Category = "Execution", Meta = (IgnoreForMemberInitializationTest))
	FGuid ExecutionId;

	/** The action being executed */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	FAction Action;

	/** Actor executing the action */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	TWeakObjectPtr<AActor> Executor;

	/** Running result (updated as defenses resolve) */
	FActionResult PartialResult;

	/** Pending defense contexts per target */
	TMap<TWeakObjectPtr<AActor>, FPendingDefenseContext> PendingDefenses;

	/** Count-based close (Stage 1): set true when the hitting animation (the Skill
	 *  montage leg) ends. The melee window closes at the LATER of (ImpactsLanded ==
	 *  ExpectedImpacts) AND this — for melee the animation is normally the later
	 *  signal, so Skill-end reconciles/closes the count-based windows. */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	bool bAnimationFinished = false;

	/** Is execution still in progress? */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	bool bInProgress = false;

	/** Maximum time to wait for all defenses (prevents soft-lock) */
	UPROPERTY(BlueprintReadOnly, Category = "Execution")
	float TimeoutDuration = 10.0f;

	/** Time execution started */
	double StartTime = 0.0;

	/** Per-action stat modifiers accumulated from all active sources
	 *  (Reality innate, Reality slotted, Reality infused, Evolution slotted,
	 *  Evolution infused). Read by damage/status/animation/movement consumers. */
	FActionStatModifiers ActionMods;

	/** Infusion HP cost computed at COMMIT (commit-time CurrentHP, unclamped) and
	 *  deducted at FinalizeAsyncAction — after the infused effect, damage and
	 *  animation resolve — so a lethal cost kills the caster only once the action
	 *  has landed. Stashed (not recomputed at finalize) so the deducted value is
	 *  identical to commit even if the caster loses HP mid-action (e.g. reflected
	 *  damage). 0 when the action pays no infusion HP. */
	int32 PendingInfusionHPCost = 0;

	/** Per-hit damage percent table (length HitCount, sums to 100), resolved
	 *  once in FinalizeDamageInputs from the skill's DamageSplit (D1). Stash
	 *  only for now — the fused-montage runner (Stage 12) consumes it; the
	 *  legacy even-split paths ignore it. */
	TArray<float> ResolvedDamageSplit;

	FActionExecutionContext()
	{
		ExecutionId = FGuid::NewGuid();
	}

	/** Check if all defenses are resolved */
	bool AreAllDefensesResolved() const
	{
		return PendingDefenses.Num() == 0;
	}

	/** Get number of pending defenses */
	int32 GetPendingCount() const
	{
		return PendingDefenses.Num();
	}
};
/**
 * FCombatHitResult
 * Result of a single hit on a single target
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCombatHitResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	AActor *Target = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 DamageDealt = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 HealingDone = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasCritical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasBlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasParried = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bWasDodged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bTargetDied = false;
};

/**
 * FActionHitInput — describes one hit applied to one target. The unified
 * input shape consumed by UActionExecutor::ApplyHit (added in Phase A of the
 * ApplyHit consolidation; see docs/analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md).
 *
 * Single-hit by contract. For multi-hit attacks, BaseDamage is the already-split
 * per-hit value; the orchestrator's ProcessMultiHit loop calls ApplyHit N times
 * with the same input. Per-hit-independent crit and death-break semantics live
 * in the loop.
 *
 * Population rules per orchestrator (Session Y: trigger type resolves in
 * UStatusBuildupManager from Element + PhysicalDamageType — no StatusType is
 * plumbed through the pipeline anymore):
 *
 *   ExecuteSpellAsync:
 *     - ActionType = Spell
 *     - Element = Spell->Element
 *     - BaseStatusBuildup = Spell->CalculateStatusBuildup(...) (if non-raw)
 *     - PhysicalDamageType = None (spells have no physical type)
 *
 *   ExecuteAbilityAsync:
 *     - ActionType = Ability
 *     - Element = bIsInfused ? Attacker.InnateElement : Generic
 *     - PhysicalDamageType = Weapon->PhysicalDamageType (inherits the
 *       wielded weapon's physical type for buildup-bar trigger)
 *
 *   ExecuteAttackAsync:
 *     - ActionType = Attack
 *     - Element = bIsInfused ? Attacker.InnateElement : Generic
 *     - BaseStatusBuildup = Attack->StatusBuildup
 *     - PhysicalDamageType = Weapon->PhysicalDamageType
 *
 *   (ExecuteAttackWithInfusion on UWeaponManager was deleted in Phase C2 —
 *    its only in-source caller had zero live triggers; sync attacks now warn
 *    and fail-result on nullptr AttackData instead of delegating.)
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FActionHitInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit")
	AActor *Attacker = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit")
	AActor *Target = nullptr;

	/** Drives damage-stat selection (Spell → SpellDamage; Ability/Attack → RawDamage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit")
	EActionType ActionType = EActionType::None;

	/** Pre-calculation per-hit damage value. 0 if this hit deals no damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit|Damage")
	int32 BaseDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit|Damage")
	bool bCanCrit = true;

	/** Generic when the hit carries no element. Drives element-interaction and
	 *  per-element resistance routing on the buildup side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit")
	ESpellElement Element = ESpellElement::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit|Infusion")
	int32 InfusionLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit|Infusion")
	EInfusionSourceOption SelectedSource = EInfusionSourceOption::None;

	/** Pre-calculation per-hit buildup value. 0 if this hit applies no buildup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit|Buildup")
	int32 BaseStatusBuildup = 0;

	/** Physical damage type. Drives bar-cap trigger when Element is Generic
	 *  (Session Y). None for spells/abilities; populated for weapon attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit|Buildup")
	EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::None;

	/** Per-action stat modifiers (Reality + Evolution + future per-action buffs).
	 *  Explicit on the input rather than implicit via CurrentExecutionContext —
	 *  closes audit risk #7 from the Phase 2 Apply-Consolidation map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Hit")
	FActionStatModifiers ActionMods;

	/** Reentrancy guard for DoubleHit skill-effect. Set true on the second hit
	 *  ApplyHit dispatches to itself so the DoubleHit check doesn't loop. */
	UPROPERTY(BlueprintReadWrite, Category = "Action Hit")
	bool bIsDoubleHit = false;
};