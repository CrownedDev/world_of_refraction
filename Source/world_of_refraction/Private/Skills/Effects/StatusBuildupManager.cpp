// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Skills/Effects/BarCapTriggerResolver.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
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

ESkillEffectType UStatusBuildupManager::GetElementImmunityType(ESpellElement Element) const
{
	switch (Element)
	{
	case ESpellElement::Fire:      return ESkillEffectType::GrantFireImmunity;
	case ESpellElement::Water:     return ESkillEffectType::GrantWaterImmunity;
	case ESpellElement::Earth:     return ESkillEffectType::GrantEarthImmunity;
	case ESpellElement::Wind:      return ESkillEffectType::GrantWindImmunity;
	case ESpellElement::Light:     return ESkillEffectType::GrantLightImmunity;
	case ESpellElement::Darkness:  return ESkillEffectType::GrantDarknessImmunity;
	case ESpellElement::Lightning: return ESkillEffectType::GrantLightningImmunity;
	case ESpellElement::Void:      return ESkillEffectType::GrantVoidImmunity;
	case ESpellElement::Reality:   return ESkillEffectType::GrantRealityImmunity;
	case ESpellElement::Generic:
	case ESpellElement::BrokenDarkness:
	default:
		return ESkillEffectType::None;
	}
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

ESkillEffectType UStatusBuildupManager::GetPendingTrigger(AActor *Target) const
{
	if (!Target)
	{
		return ESkillEffectType::None;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	if (!State)
	{
		return ESkillEffectType::None;
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
	const TArray<FActiveSkillEffect> Buffs = EffectMgr->GetEffectsByType(Target, ESkillEffectType::ResistanceBuff);
	const TArray<FActiveSkillEffect> Debuffs = EffectMgr->GetEffectsByType(Target, ESkillEffectType::ResistanceDebuff);

	float BuffSum = 0.0f;
	for (const FActiveSkillEffect &Effect : Buffs)
	{
		if (Effect.Element == Element)
		{
			BuffSum += Effect.GetStackedValue();
		}
	}

	float DebuffSum = 0.0f;
	for (const FActiveSkillEffect &Effect : Debuffs)
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
											 ESpellElement Element, EPhysicalDamageType PhysicalType,
											 bool bSkipBaseStatAmp)
{
	if (!Target || Amount <= 0.0f)
	{
		return false;
	}

	// Resolve which trigger this hit would fire and check target immunities up-front.
	// Buildup never accrues for immune targets (returns false and short-circuits the
	// amplification, resistance, and bar-update steps).
	const ESkillEffectType ResolvedTrigger = BarCapTriggerResolver::ResolveTrigger(Element, PhysicalType);
	if (USkillEffectManager *EffectMgr = GetEffectManager())
	{
		// Global immunity — blocks any buildup.
		if (EffectMgr->HasEffectOfType(Target, ESkillEffectType::GrantAllStatusImmunity))
		{
			UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s has GrantAllStatusImmunity — buildup absorbed"),
				   *Target->GetName());
			return false;
		}

		// Per-trigger-type immunities — gated on the resolved bar-cap trigger.
		// BarCapTriggerResolver maps Lightning/Impact -> Stun, Darkness -> Silenced,
		// Fire/Slash -> DOT. GrantDOTImmunity blocks any DOT regardless of element.
		const bool bImmuneStun =
			(ResolvedTrigger == ESkillEffectType::Stun) &&
			EffectMgr->HasEffectOfType(Target, ESkillEffectType::GrantStunImmunity);

		const bool bImmuneSilence =
			(ResolvedTrigger == ESkillEffectType::Silenced) &&
			EffectMgr->HasEffectOfType(Target, ESkillEffectType::GrantSilenceImmunity);

		const bool bImmuneDOT =
			(ResolvedTrigger == ESkillEffectType::DOT) &&
			EffectMgr->HasEffectOfType(Target, ESkillEffectType::GrantDOTImmunity);

		if (bImmuneStun || bImmuneSilence || bImmuneDOT)
		{
			UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s immune to resolved trigger %s — buildup absorbed"),
				   *Target->GetName(), *UEnum::GetValueAsString(ResolvedTrigger));
			return false;
		}

		// Per-element immunity — block when the incoming buildup Element matches
		// a held GrantXxxImmunity for that element. Distinct from the per-trigger
		// gates above; an actor with GrantFireImmunity blocks Fire-buildup
		// regardless of whether it would resolve to DOT, Stun, or anything else.
		const ESkillEffectType ElemImmunity = GetElementImmunityType(Element);
		if (ElemImmunity != ESkillEffectType::None &&
		    EffectMgr->HasEffectOfType(Target, ElemImmunity))
		{
			UE_LOG(LogTemp, Log, TEXT("[StatusBuildupManager] %s has %s — buildup absorbed"),
				   *Target->GetName(), *UEnum::GetValueAsString(ElemImmunity));
			return false;
		}
	}

	// Get or create state
	FStatusBarState &State = StatusBarStates.FindOrAdd(Target);

	// Apply attacker StatusMultiplier amplification — crystal-aware path.
	// Inlines the StatusMultiplier formula (1 + EffectiveSpirit × points × per-point)
	// against the component's GetEvolutionModifiedSpirit so a slotted primary
	// evolution crystal's pillar modifier feeds the buildup curve.
	// Mirrors UCharacterData::CalculateStatusMultiplier shape; if that formula
	// changes, update here too.
	if (Source)
	{
		// Step 5 — base-stat amp. Gated by bSkipBaseStatAmp for callers that
		// already baked StatusMultiplier into the incoming Amount (e.g. BD
		// overload aura, where `released = BaseRelease × StatusMult × Efficiency`
		// is the single coupled quantity feeding both drain and self-status —
		// re-applying the stat amp here would double-count). Steps 5b/5c/6
		// always run regardless.
		if (!bSkipBaseStatAmp)
		{
			UCharacterDataComponent *SourceComp = Source->FindComponentByClass<UCharacterDataComponent>();
			if (SourceComp && SourceComp->CharacterData)
			{
				const float ModifiedSpirit = SourceComp->GetEvolutionModifiedSpirit();
				const int32 TotalPoints = SourceComp->CharacterData->GetTotalStatusMultiplier();

				// Equipment stat bonus — additive to the asset-driven per-point
				// amplification. Read from the source actor's active loadout.
				int32 BonusPoints = 0;
				if (ULoadoutComponent *SourceLoadout = Source->FindComponentByClass<ULoadoutComponent>())
				{
					const FEquipmentStatBonus Bonus = SourceLoadout->GetActiveStatBonus(Source);
					BonusPoints = Bonus.BonusStatusMultiplier;
				}

				Amount *= 1.0f + (ModifiedSpirit * TotalPoints * CombatConstants::STATUS_MULTIPLIER_PER_POINT)
				              + (BonusPoints * CombatConstants::STATUS_MULTIPLIER_PER_POINT);
			}
		}

		// Skill-effect-driven StatusMultiplier buff/debuff — symmetric with
		// DamageCalculator's DamageBuff/Debuff at GetStatusEffectDamageModifier
		// (:521-523). Aggregated via GetTotalStatModifier (non-element-specific:
		// StatusMultiplier is caster output amplification, not per-element). Sits
		// between the character-stat amplification above and the defender-side
		// resistance reduction below — mirrors the layering in damage.
		if (USkillEffectManager *EffectMgr = GetEffectManager())
		{
			const float SmBuff = EffectMgr->GetTotalStatModifier(Source, ESkillEffectType::StatusMultiplierBuff);
			const float SmDebuff = EffectMgr->GetTotalStatModifier(Source, ESkillEffectType::StatusMultiplierDebuff);
			Amount *= FMath::Max(0.0f, 1.0f + (SmBuff - SmDebuff) / 100.0f);
		}

		// 5c. BD absorption-stack amplification (matching-element only).
		// Restores the stack→buildup link that went dead in sweep-3 (the only
		// caller of the old DamageCalculator::GetBDStackStatusMultiplier was
		// the deleted CalculateStatusBuildup). Stacks amplify STATUS BUILDUP
		// on matching-alignment spells: 1×/1×/2×/4× at stacks 0-3. Separate
		// from the StatusMultiplier-stat amplification above — both layers
		// legitimately apply (stat amp = Spirit-driven, stack amp = absorption-
		// driven). Element gate + transform gate live inside
		// GetElementStackStatusMultiplier; non-BD sources and non-matching
		// elements return 1.0 — safe to call unconditionally.
		if (UBrokenDarknessManager *SourceBD = Source->FindComponentByClass<UBrokenDarknessManager>())
		{
			Amount *= SourceBD->GetElementStackStatusMultiplier(Element);
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

		// Equipment stat bonus — flat additive to resistance using the same
		// per-point shape as the asset-side CalculateResistance formula.
		// Layered before the element-buff stack and re-clamped together.
		if (ULoadoutComponent *TargetLoadout = Target->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus TargetBonus = TargetLoadout->GetActiveStatBonus(Target);
			Resistance += TargetBonus.BonusResistance * CombatConstants::RESISTANCE_PER_POINT;
		}

		Resistance += GetTotalElementResistance(Target, Element);

		// Skill-effect-driven ModifyStatusResist — flat percent-space additive
		// to total resistance. Applied before the final clamp so it cannot
		// push past RESISTANCE_MAX.
		if (USkillEffectManager *EffectMgr = GetEffectManager())
		{
			const float ResistModify = EffectMgr->GetTotalStatModifier(Target, ESkillEffectType::ModifyStatusResist);
			Resistance += ResistModify / 100.0f;
		}

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

	// ResolvedTrigger was computed at function entry for the immunity gate; reuse here.

	OnStatusBuildupChanged.Broadcast(Target, State.CurrentBuildup,
		CombatConstants::STATUS_EFFECT_THRESHOLD, Element, Source);

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
		if (ResolvedTrigger != ESkillEffectType::None)
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

		// LastSource was just cleared above — broadcast null; the reset bar is
		// neutral anyway (PendingElement == Generic).
		OnStatusBuildupChanged.Broadcast(Target, 0.0f,
			CombatConstants::STATUS_EFFECT_THRESHOLD, ESpellElement::Generic, nullptr);
	}
}

void UStatusBuildupManager::ReduceStatusBuildup(AActor *Target, float Fraction)
{
	if (!Target)
	{
		return;
	}

	FStatusBarState *State = StatusBarStates.Find(Target);
	if (!State)
	{
		return;
	}

	Fraction = FMath::Clamp(Fraction, 0.0f, 1.0f);
	State->CurrentBuildup = FMath::Max(0.0f, State->CurrentBuildup * (1.0f - Fraction));

	// PendingElement + LastSource passthrough — UI keeps the current tint
	// (including BD darkening) while the bar drops.
	OnStatusBuildupChanged.Broadcast(Target, State->CurrentBuildup,
		CombatConstants::STATUS_EFFECT_THRESHOLD, State->PendingElement, State->LastSource.Get());

	UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status buildup reduced by %.0f%% -> %.1f"),
		   *Target->GetName(), Fraction * 100.0f, State->CurrentBuildup);
}

void UStatusBuildupManager::ReduceStatusBuildupByAmount(AActor *Target, float Amount)
{
	if (!Target || Amount <= 0.0f)
	{
		return;
	}

	FStatusBarState *State = StatusBarStates.Find(Target);
	if (!State)
	{
		return;
	}

	const float Before = State->CurrentBuildup;
	State->CurrentBuildup = FMath::Max(0.0f, State->CurrentBuildup - Amount);

	// PendingElement + LastSource passthrough — same UI behaviour as the
	// fraction-based ReduceStatusBuildup above. The reduction is "negative
	// buildup" by nature, so no Source is logged.
	OnStatusBuildupChanged.Broadcast(Target, State->CurrentBuildup,
		CombatConstants::STATUS_EFFECT_THRESHOLD, State->PendingElement, State->LastSource.Get());

	UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status buildup reduced by %.1f -> %.1f (was %.1f)"),
		   *Target->GetName(), Amount, State->CurrentBuildup, Before);
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

	// Element + LastSource passthrough on decay - UI keeps current tint
	// (including BD darkening) while bar drops.
	OnStatusBuildupChanged.Broadcast(Target, State->CurrentBuildup,
		CombatConstants::STATUS_EFFECT_THRESHOLD, State->PendingElement, State->LastSource.Get());

	UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status bar decayed: %.1f → %.1f (-%d%%, turn %d)"),
		   *Target->GetName(), OldBuildup, State->CurrentBuildup,
		   FMath::RoundToInt(CombatConstants::STATUS_DECAY_RATE * 100.0f),
		   State->TurnsSinceLastHit);
}

void UStatusBuildupManager::TriggerSkillEffectFromBuildup(AActor *Source, AActor *Target, ESkillEffectType StatusType, ESpellElement Element)
{
	if (!Target || StatusType == ESkillEffectType::None)
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
