// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "StatusBuildupManager.generated.h"

class USkillEffectManager;

// ========================================
// DELEGATE DECLARATIONS
// ========================================

/** Broadcast when a status buildup changes (for UI feedback).
 *  PendingElement lets UI re-tint the bar on every hit per the locked design
 *  (bar colour follows most-recent-hit element). Source is the attacker, so UI
 *  can darken the tint for Broken Darkness attackers; may be null on decay/reset
 *  with no recorded last source. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FOnStatusBuildupChanged,
    AActor *, Target,
    float, CurrentBuildup,
    float, MaxBuildup,
    ESpellElement, PendingElement,
    AActor *, Source);

/**
 * UStatusBuildupManager
 *
 * GameInstanceSubsystem that owns the per-actor status-buildup bar.
 * Buildup accumulates from attacks (spell + ability + weapon); when the bar
 * caps, the appropriate skill effect fires via USkillEffectManager.
 *
 * Split out of USkillEffectManager in 2026-05. The effect manager retains
 * effect tracking (durations, stacks, removal helpers, immediate / triggered
 * application). This manager owns the bar — the two systems talk via a
 * cached pointer:
 *   Buildup bar fills  →  UStatusBuildupManager::TriggerSkillEffectFromBuildup
 *                      →  USkillEffectManager::ApplyTriggeredSkillEffect (lands the effect)
 *
 * Per-element resistance — although the underlying ResistanceBuff / Debuff
 * effects live in USkillEffectManager::ActiveEffects (the effect manager's
 * storage is the plumbing), the public query GetTotalElementResistance lives
 * on this manager: element resistance is a buildup-side concept (it reduces
 * buildup, not damage). The query reaches over to the effect manager via the
 * thin GetEffectsByType accessor.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UStatusBuildupManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ========================================
	// SUBSYSTEM LIFECYCLE
	// ========================================

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// STATUS BAR QUERIES
	// ========================================

	/** Get current status bar percentage for a target (0.0 - 1.0) */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	float GetStatusBarPercent(AActor *Target) const;

	/** Get current status bar buildup value */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	float GetStatusBarBuildup(AActor *Target) const;

	/** Get remaining buildup needed to trigger status */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	float GetBuildupToTrigger(AActor *Target) const;

	/** Get the trigger type that will fire if the bar caps now.
	 *  Resolved live from PendingElement + PendingPhysicalType via
	 *  BarCapTriggerResolver - no cached trigger state. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	ESkillEffectType GetPendingTrigger(AActor *Target) const;

	/** Get the element of the last hit that built the bar.
	 *  Used by UI for bar tinting. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	ESpellElement GetPendingElement(AActor *Target) const;

	// ========================================
	// STATUS BAR MUTATION
	// ========================================

	/** Add buildup to target's status bar, returns true if triggered.
	 *  Source's StatusMultiplier amplifies the amount; target's base resistance
	 *  and element-matching ResistanceBuff/Debuff effects reduce it. When the
	 *  bar caps, the trigger type is resolved internally via BarCapTriggerResolver
	 *  (Element wins over PhysicalType when non-Generic) and fired through the
	 *  effect manager.
	 *
	 *  bSkipBaseStatAmp — when true, skips the source's StatusMultiplier amplification —
	 *  BOTH the base-stat layer (step 5) AND the transient skill-effect buff/debuff layer
	 *  (step 5b). The BD absorption-stack amplification (5c) and target resistance (6)
	 *  still apply. Used by callers that have already baked both into the Amount they pass
	 *  (e.g. the BD overload aura, where `released = BaseRelease × StatusMult × Efficiency`
	 *  — StatusMult now including transient — is the same number that drains absorption AND
	 *  becomes self-status, so re-applying here would double-count). Defaults false so all
	 *  existing callers keep the full pipeline. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	bool AddStatusBuildup(AActor *Source, AActor *Target, float Amount,
	                      ESpellElement Element, EPhysicalDamageType PhysicalType,
	                      bool bSkipBaseStatAmp = false);

	// GetSourceStatusMultiplierFactor was retired in the T3 consolidation — the single source of truth
	// is UCharacterDataComponent::GetEffectiveStatusMultiplier (base + additive transient), read by
	// AddStatusBuildup, the BD overload bake (CombatOrchestrator), and crystal-wear alike.

	/** Reset status bar for target */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	void ResetStatusBar(AActor *Target);

	/** Reduce a target's current status buildup by Fraction (0.0-1.0).
	 *  Fraction 1.0 fully clears the bar; 0.0 is a no-op. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	void ReduceStatusBuildup(AActor *Target, float Fraction);

	/** Reduce a target's current status buildup by a flat absolute Amount.
	 *  Parallel shape to AddStatusBuildup (sweep-4 / StatusDecrease effect type)
	 *  but without StatusMultiplier / Resistance modulation — pure subtraction
	 *  clamped at 0. The existing fraction-based ReduceStatusBuildup stays
	 *  untouched (Quartz items still use it). */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	void ReduceStatusBuildupByAmount(AActor *Target, float Amount);

	/** Process status bar decay at turn start. Bar decays per-turn; full reset
	 *  after STATUS_DECAY_FULL_RESET_TURNS without further buildup. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	void ProcessStatusBarDecay(AActor *Target);

	// ========================================
	// RESISTANCE QUERY
	// ========================================

	/** Net resistance fraction from active ResistanceBuff − ResistanceDebuff
	 *  effects on Target, matched per effect on EITHER axis: a physical-keyed
	 *  effect (PhysicalType != None) matches the incoming PhysicalType; otherwise
	 *  the effect's Element matches the incoming Element. So a Fire-Resistance buff
	 *  reduces Fire buildup only, and a Slash-Resistance buff reduces Slash buildup
	 *  only. Mismatched effects contribute zero. Returns (ΣBuff − ΣDebuff)/100
	 *  (caller clamps). Element-only queries pass PhysicalType = None — no C++
	 *  default (UHT rejects enum defaults on UFUNCTION params); Blueprint callers
	 *  must set the pin explicitly (None for element-only).
	 *
	 *  Implementation queries USkillEffectManager::GetEffectsByType for
	 *  ResistanceBuff and ResistanceDebuff — axis matching + aggregation happen here. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	float GetTotalElementResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType) const;

	/** Unified DEFENDER-side status resistance for an incoming (Element, PhysicalType)
	 *  hit — the single value AddStatusBuildup applies as Amount *= (1 - this).
	 *  Composes all six resistance sources, then clamps to [RESISTANCE_MIN,
	 *  RESISTANCE_MAX] and returns the POST-clamp value:
	 *    1. base Spirit CalculateResistance()      (itself pre-clamped to [0, MAX])
	 *    2. equipment BonusResistance substat
	 *    3. attached ResistanceStone
	 *    4. GetTotalElementResistance (element + physical effects)
	 *    5. GetClassInnateResistance (class/innate profile)
	 *    6. ModifyStatusResist skill-effect
	 *  Offense-side amplification (StatusMultiplier, BD absorption stack) is NOT
	 *  here — that is source-side and applied separately. Returns 0.0 when Target
	 *  or its CharacterData is absent (caller's Amount stays unchanged). */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	float GetTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType) const;

	// ========================================
	// EVENTS
	// ========================================

	/** Broadcast when status bar buildup changes */
	UPROPERTY(BlueprintAssignable, Category = "Status|Bar|Events")
	FOnStatusBuildupChanged OnStatusBuildupChanged;

private:
	// ========================================
	// STATUS BAR STATE
	// ========================================

	/** Status bar state per actor */
	struct FStatusBarState
	{
		float CurrentBuildup = 0.0f;
		ESpellElement PendingElement = ESpellElement::Generic;
		EPhysicalDamageType PendingPhysicalType = EPhysicalDamageType::None;
		TWeakObjectPtr<AActor> LastSource;
		int32 TurnsSinceLastHit = 0;
	};

	/** Status bar state tracking */
	TMap<TWeakObjectPtr<AActor>, FStatusBarState> StatusBarStates;

	/** Trigger the status effect when bar fills. Looks up the effect manager
	 *  and calls ApplyTriggeredSkillEffect on it. */
	void TriggerSkillEffectFromBuildup(AActor *Source, AActor *Target, ESkillEffectType StatusType, ESpellElement Element);

	/** Map an ESpellElement to its corresponding GrantXxxImmunity flag.
	 *  Returns ESkillEffectType::None for Generic / BrokenDarkness — those
	 *  elements don't have a dedicated immunity. */
	ESkillEffectType GetElementImmunityType(ESpellElement Element) const;

	// ========================================
	// CACHED CROSS-SUBSYSTEM REFERENCE
	// ========================================

	/** Cached effect manager reference. Lazy-initialised on first GetEffectManager call. */
	UPROPERTY()
	USkillEffectManager *EffectManagerRef = nullptr;

	/** Get USkillEffectManager via cached pointer; lazy-acquire on first call.
	 *  Matches the const_cast pattern used by UActionExecutor::GetSkillEffectManager. */
	USkillEffectManager *GetEffectManager() const;
};
