// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EStatusType.h"
#include "ESpellElement.h"
#include "StatusBuildupManager.generated.h"

class USkillEffectManager;

// ========================================
// DELEGATE DECLARATIONS
// ========================================

/** Broadcast when a status buildup changes (for UI feedback) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStatusBuildupChanged, AActor *, Target, float, CurrentBuildup, float, MaxBuildup);

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

	/** Get the pending status type (what will trigger when bar fills) */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	EStatusType GetPendingStatus(AActor *Target) const;

	// ========================================
	// STATUS BAR MUTATION
	// ========================================

	/** Add buildup to target's status bar, returns true if triggered.
	 *  Source's StatusMultiplier amplifies the amount; target's base resistance
	 *  and element-matching ResistanceBuff/Debuff effects reduce it. When the
	 *  bar caps, TriggerSkillEffectFromBuildup fires the pending effect via the effect
	 *  manager. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	bool AddStatusBuildup(AActor *Source, AActor *Target, float Amount, EStatusType StatusType, ESpellElement Element);

	/** Reset status bar for target */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	void ResetStatusBar(AActor *Target);

	/** Process status bar decay at turn start. Bar decays per-turn; full reset
	 *  after STATUS_DECAY_FULL_RESET_TURNS without further buildup. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	void ProcessStatusBarDecay(AActor *Target);

	// ========================================
	// RESISTANCE QUERY
	// ========================================

	/** Net element-matching resistance fraction from active ResistanceBuff −
	 *  ResistanceDebuff effects on Target. Returns 0.0–0.5+ (caller clamps).
	 *  Used internally by AddStatusBuildup so per-element resistance buffs
	 *  (e.g., Fire Resistance from items) reduce Fire buildup only, not
	 *  Lightning buildup. Effects with mismatched Element contribute zero.
	 *
	 *  Implementation queries USkillEffectManager::GetEffectsByType for
	 *  ResistanceBuff and ResistanceDebuff — element filtering and aggregation
	 *  happen here. */
	UFUNCTION(BlueprintCallable, Category = "Status|Bar")
	float GetTotalElementResistance(AActor *Target, ESpellElement Element) const;

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
		EStatusType PendingStatus = EStatusType::None;
		ESpellElement PendingElement = ESpellElement::Generic;
		TWeakObjectPtr<AActor> LastSource;
		int32 TurnsSinceLastHit = 0;
	};

	/** Status bar state tracking */
	TMap<TWeakObjectPtr<AActor>, FStatusBarState> StatusBarStates;

	/** Trigger the status effect when bar fills. Looks up the effect manager
	 *  and calls ApplyTriggeredSkillEffect on it. */
	void TriggerSkillEffectFromBuildup(AActor *Source, AActor *Target, EStatusType StatusType, ESpellElement Element);

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
