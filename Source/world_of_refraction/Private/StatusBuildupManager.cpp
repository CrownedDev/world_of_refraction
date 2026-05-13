// Copyright Epic Games, Inc. All Rights Reserved.

#include "StatusBuildupManager.h"
#include "SkillEffectManager.h"
#include "StatusEffect.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CombatConstants.h"
#include "BarCapTriggerResolver.h"
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

EStatusType UStatusBuildupManager::GetPendingTrigger(AActor *Target) const
{
	if (!Target)
	{
		return EStatusType::None;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	if (!State)
	{
		return EStatusType::None;
	}
	return BarCapTriggerResolver::ResolveTrigger(State->PendingElement, State->PendingPhysicalType);
}

ESpellElement UStatusBuildupManager::GetPendingElement(AActor *Target) const
{
	if (!Target)
	{
		return ESpellElement::Generic;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	return State ? State->PendingElement : ESpellElement::Generic;
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

bool UStatusBuildupManager::AddStatusBuildup(AActor *Source, AActor *Target, float Amount,
											 ESpellElement Element, EPhysicalDamageType PhysicalType)
{
	if (!Target || Amount <= 0.0f)
	{
		return false;
	}

	// Get or create state
	FStatusBarState &State = StatusBarStates.FindOrAdd(Target);

	// Apply attacker StatusMultiplier amplification via the pillar-scaled
	// CalculateStatusMultiplier (Spirit-driven post pillar move). Single source
	// of truth for buildup scaling — matches the helpers on USpellData /
	// UAbilityData / debug paths.
	if (Source)
	{
		UCharacterDataComponent *SourceComp = Source->FindComponentByClass<UCharacterDataComponent>();
		if (SourceComp && SourceComp->CharacterData)
		{
			Amount *= SourceComp->CharacterData->CalculateStatusMultiplier();
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

	// Update bar state - most recent hit wins on element + physical type
	State.PendingElement = Element;
	State.PendingPhysicalType = PhysicalType;
	State.LastSource = Source;
	State.TurnsSinceLastHit = 0; // Reset decay counter

	// Add buildup
	float OldBuildup = State.CurrentBuildup;
	State.CurrentBuildup += Amount;

	// Resolve the trigger that WOULD fire if bar caps right now.
	const EStatusType ResolvedTrigger = BarCapTriggerResolver::ResolveTrigger(Element, PhysicalType);

	OnStatusBuildupChanged.Broadcast(Target, State.CurrentBuildup,
		CombatConstants::STATUS_EFFECT_THRESHOLD, Element);

	UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s status buildup: %.1f -> %.1f (+%.1f) - Element: %s, Physical: %s, Resolved trigger: %s"),
		   *Target->GetName(), OldBuildup, State.CurrentBuildup, Amount,
		   *UEnum::GetValueAsString(Element),
		   *UEnum::GetValueAsString(PhysicalType),
		   *UEnum::GetValueAsString(ResolvedTrigger));

	// Check if triggered
	if (State.CurrentBuildup >= CombatConstants::STATUS_EFFECT_THRESHOLD)
	{
		// Only fire + reset if we have a real trigger. Otherwise the bar
		// stays at/above cap, waiting for a hit with a real trigger to
		// consume it. Prevents "phantom cap" where the bar fills,
		// immediately empties, and nothing visible happens.
		if (ResolvedTrigger != EStatusType::None)
		{
			UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s status bar FULL - Triggering %s"),
				   *Target->GetName(), *UEnum::GetValueAsString(ResolvedTrigger));

			TriggerSkillEffectFromBuildup(Source, Target, ResolvedTrigger, Element);
			ResetStatusBar(Target);
			return true;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status bar at cap but trigger is None — bar held"),
			   *Target->GetName());
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
		State->PendingElement = ESpellElement::Generic;
		State->PendingPhysicalType = EPhysicalDamageType::None;
		State->LastSource = nullptr;
		State->TurnsSinceLastHit = 0;

		OnStatusBuildupChanged.Broadcast(Target, 0.0f,
			CombatConstants::STATUS_EFFECT_THRESHOLD, ESpellElement::Generic);
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

	// Element passthrough on decay - UI keeps current tint while bar drops.
	OnStatusBuildupChanged.Broadcast(Target, State->CurrentBuildup,
		CombatConstants::STATUS_EFFECT_THRESHOLD, State->PendingElement);

	UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status bar decayed: %.1f → %.1f (-%d%%, turn %d)"),
		   *Target->GetName(), OldBuildup, State->CurrentBuildup,
		   FMath::RoundToInt(CombatConstants::STATUS_DECAY_RATE * 100.0f),
		   State->TurnsSinceLastHit);
}

void UStatusBuildupManager::TriggerSkillEffectFromBuildup(AActor *Source, AActor *Target, EStatusType StatusType, ESpellElement Element)
{
	if (!Target || StatusType == EStatusType::None)
	{
		return;
	}

	USkillEffectManager *EffectMgr = GetEffectManager();
	if (!EffectMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StatusBuildupManager] TriggerSkillEffectFromBuildup: USkillEffectManager unavailable; cannot apply triggered status"));
		return;
	}

	EffectMgr->ApplyTriggeredSkillEffect(Source, Target, StatusType, Element);
}
