// Copyright Epic Games, Inc. All Rights Reserved.

#include "StatusBuildupManager.h"
#include "SkillEffectManager.h"
#include "StatusEffect.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CombatConstants.h"
#include "Engine/GameInstance.h"

// ========================================
// SUBSYSTEM LIFECYCLE
// ========================================

void UStatusBuildupManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);

	StatusBarStates.Empty();

	UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] Initialized"));
}

void UStatusBuildupManager::Deinitialize()
{
	StatusBarStates.Empty();
	EffectManagerRef = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] Deinitialized"));

	Super::Deinitialize();
}

// ========================================
// CACHED CROSS-SUBSYSTEM REFERENCE
// ========================================

USkillEffectManager *UStatusBuildupManager::GetEffectManager() const
{
	if (!EffectManagerRef)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			const_cast<UStatusBuildupManager *>(this)->EffectManagerRef =
				GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return EffectManagerRef;
}

// ========================================
// STATUS BAR QUERIES
// ========================================

float UStatusBuildupManager::GetStatusBarPercent(AActor *Target) const
{
	if (!Target)
	{
		return 0.0f;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	if (!State)
	{
		return 0.0f;
	}

	return FMath::Clamp(State->CurrentBuildup / CombatConstants::STATUS_EFFECT_THRESHOLD, 0.0f, 1.0f);
}

float UStatusBuildupManager::GetStatusBarBuildup(AActor *Target) const
{
	if (!Target)
	{
		return 0.0f;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	return State ? State->CurrentBuildup : 0.0f;
}

float UStatusBuildupManager::GetBuildupToTrigger(AActor *Target) const
{
	if (!Target)
	{
		return CombatConstants::STATUS_EFFECT_THRESHOLD;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	float Current = State ? State->CurrentBuildup : 0.0f;
	return FMath::Max(0.0f, CombatConstants::STATUS_EFFECT_THRESHOLD - Current);
}

EStatusType UStatusBuildupManager::GetPendingStatus(AActor *Target) const
{
	if (!Target)
	{
		return EStatusType::None;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	return State ? State->PendingStatus : EStatusType::None;
}

// ========================================
// RESISTANCE QUERY
// ========================================

float UStatusBuildupManager::GetTotalElementResistance(AActor *Target, ESpellElement Element) const
{
	if (!Target)
	{
		return 0.0f;
	}

	USkillEffectManager *EffectMgr = GetEffectManager();
	if (!EffectMgr)
	{
		return 0.0f;
	}

	// Pull both effect types from the effect manager. The plumbing (ActiveEffects
	// map) lives there; the element filter + aggregation belong here, on the
	// buildup-side query.
	const TArray<FStatusEffect> Buffs = EffectMgr->GetEffectsByType(Target, EStatusType::ResistanceBuff);
	const TArray<FStatusEffect> Debuffs = EffectMgr->GetEffectsByType(Target, EStatusType::ResistanceDebuff);

	float BuffSum = 0.0f;
	for (const FStatusEffect &Effect : Buffs)
	{
		if (Effect.Element == Element)
		{
			BuffSum += Effect.GetStackedValue();
		}
	}

	float DebuffSum = 0.0f;
	for (const FStatusEffect &Effect : Debuffs)
	{
		if (Effect.Element == Element)
		{
			DebuffSum += Effect.GetStackedValue();
		}
	}

	return (BuffSum - DebuffSum) / CombatConstants::STAT_PERCENT_DIVISOR;
}

// ========================================
// STATUS BAR MUTATION
// ========================================

bool UStatusBuildupManager::AddStatusBuildup(AActor *Source, AActor *Target, float Amount, EStatusType StatusType, ESpellElement Element)
{
	if (!Target || Amount <= 0.0f)
	{
		return false;
	}

	// Get or create state
	FStatusBarState &State = StatusBarStates.FindOrAdd(Target);

	// Apply attacker StatusMultiplier amplification: Final = Base * (1 + StatValue / 100)
	if (Source)
	{
		UCharacterDataComponent *SourceComp = Source->FindComponentByClass<UCharacterDataComponent>();
		if (SourceComp && SourceComp->CharacterData)
		{
			const int32 StatusMultiplier = SourceComp->CharacterData->StatusMultiplier;
			Amount *= (1.0f + StatusMultiplier / CombatConstants::STAT_PERCENT_DIVISOR);
		}
	}

	// Apply target's resistance reduction. Effective = base Spirit-Resistance plus
	// element-matching ResistanceBuff/Debuff stack (so a Fire Resistance item buff
	// reduces Fire buildup only). Re-clamped after addition since base CalculateResistance
	// is already clamped to RESISTANCE_MAX.
	UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	if (TargetComp && TargetComp->CharacterData)
	{
		float Resistance = TargetComp->CharacterData->CalculateResistance();
		Resistance += GetTotalElementResistance(Target, Element);
		Resistance = FMath::Clamp(Resistance, 0.0f, CombatConstants::RESISTANCE_MAX);
		Amount *= (1.0f - Resistance);
	}

	// Update pending trigger (last hit wins)
	State.PendingStatus = StatusType;
	State.PendingElement = Element;
	State.LastSource = Source;
	State.TurnsSinceLastHit = 0; // Reset decay counter

	// Add buildup
	float OldBuildup = State.CurrentBuildup;
	State.CurrentBuildup += Amount;

	OnStatusBuildupChanged.Broadcast(Target, State.CurrentBuildup, CombatConstants::STATUS_EFFECT_THRESHOLD);

	UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s status buildup: %.1f → %.1f (+%.1f) - Pending: %s"),
		   *Target->GetName(), OldBuildup, State.CurrentBuildup, Amount,
		   *UEnum::GetValueAsString(State.PendingStatus));

	// Check if triggered
	if (State.CurrentBuildup >= CombatConstants::STATUS_EFFECT_THRESHOLD)
	{
		UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s status bar FULL - Triggering %s"),
			   *Target->GetName(), *UEnum::GetValueAsString(State.PendingStatus));

		TriggerStatusEffect(Source, Target, State.PendingStatus, State.PendingElement);
		ResetStatusBar(Target);
		return true;
	}

	return false;
}

void UStatusBuildupManager::ResetStatusBar(AActor *Target)
{
	if (!Target)
	{
		return;
	}

	if (FStatusBarState *State = StatusBarStates.Find(Target))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status bar reset"), *Target->GetName());

		State->CurrentBuildup = 0.0f;
		OnStatusBuildupChanged.Broadcast(Target, 0.0f, CombatConstants::STATUS_EFFECT_THRESHOLD);
		State->PendingStatus = EStatusType::None;
		State->PendingElement = ESpellElement::Generic;
		State->LastSource = nullptr;
		State->TurnsSinceLastHit = 0;
	}
}

void UStatusBuildupManager::ProcessStatusBarDecay(AActor *Target)
{
	if (!Target)
	{
		return;
	}

	FStatusBarState *State = StatusBarStates.Find(Target);
	if (!State || State->CurrentBuildup <= 0.0f)
	{
		return;
	}

	State->TurnsSinceLastHit++;

	// Full reset after configured turn count
	if (State->TurnsSinceLastHit >= CombatConstants::STATUS_DECAY_FULL_RESET_TURNS)
	{
		UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s status bar decayed to 0 (%d turns)"),
			   *Target->GetName(), CombatConstants::STATUS_DECAY_FULL_RESET_TURNS);
		ResetStatusBar(Target);
		return;
	}

	// Per-turn decay
	float OldBuildup = State->CurrentBuildup;
	State->CurrentBuildup *= (1.0f - CombatConstants::STATUS_DECAY_RATE);

	OnStatusBuildupChanged.Broadcast(Target, State->CurrentBuildup, CombatConstants::STATUS_EFFECT_THRESHOLD);

	UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status bar decayed: %.1f → %.1f (-%d%%, turn %d)"),
		   *Target->GetName(), OldBuildup, State->CurrentBuildup,
		   FMath::RoundToInt(CombatConstants::STATUS_DECAY_RATE * 100.0f),
		   State->TurnsSinceLastHit);
}

void UStatusBuildupManager::TriggerStatusEffect(AActor *Source, AActor *Target, EStatusType StatusType, ESpellElement Element)
{
	if (!Target || StatusType == EStatusType::None)
	{
		return;
	}

	USkillEffectManager *EffectMgr = GetEffectManager();
	if (!EffectMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StatusBuildupManager] TriggerStatusEffect: USkillEffectManager unavailable; cannot apply triggered status"));
		return;
	}

	EffectMgr->ApplyTriggeredStatus(Source, Target, StatusType, Element);
}
