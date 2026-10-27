// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Skills/Effects/BarCapTriggerResolver.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Resistance/ResistanceQuery.h"
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

	// Source-BD-ness from the recorded attacker (LastSource) — same Darkness->Drain
	// vs Darkness->Silence branch as the live path, so the UI preview matches.
	bool bSourceIsBrokenDarkness = false;
	if (AActor *LastSrc = State->LastSource.Get())
	{
		if (UCharacterDataComponent *SrcComp = LastSrc->FindComponentByClass<UCharacterDataComponent>())
		{
			bSourceIsBrokenDarkness = SrcComp->IsBrokenDarkness();
		}
	}
	return BarCapTriggerResolver::ResolveTrigger(State->PendingElement, State->PendingPhysicalType,
												 bSourceIsBrokenDarkness);
}

ESpellElement UStatusBuildupManager::GetPendingElement(AActor *Target) const
{
	if (!Target)
	{
		return ESpellElement::None;
	}

	const FStatusBarState *State = StatusBarStates.Find(Target);
	return State ? State->PendingElement : ESpellElement::None;
}

// ========================================
// RESISTANCE QUERY
// ========================================

float UStatusBuildupManager::GetTotalElementResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType) const
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
	// map) lives there; the axis filter + aggregation belong here, on the
	// buildup-side query.
	const TArray<FActiveSkillEffect> Buffs = EffectMgr->GetEffectsByType(Target, ESkillEffectType::ResistanceBuff);
	const TArray<FActiveSkillEffect> Debuffs = EffectMgr->GetEffectsByType(Target, ESkillEffectType::ResistanceDebuff);

	// Match per effect on EITHER axis: a physical-keyed effect (PhysicalType != None)
	// matches the incoming physical type; otherwise the effect is element-keyed and
	// matches the incoming element. With no physical-keyed effects in play every
	// effect has PhysicalType == None, so this reduces to the original element match.
	const auto MatchesAxis = [Element, PhysicalType](const FActiveSkillEffect &Effect) -> bool
	{
		return (Effect.PhysicalType != EPhysicalDamageType::None)
				   ? (Effect.PhysicalType == PhysicalType)
				   : (Effect.Element == Element);
	};

	float BuffSum = 0.0f;
	for (const FActiveSkillEffect &Effect : Buffs)
	{
		if (MatchesAxis(Effect))
		{
			BuffSum += Effect.GetStackedValue();
		}
	}

	float DebuffSum = 0.0f;
	for (const FActiveSkillEffect &Effect : Debuffs)
	{
		if (MatchesAxis(Effect))
		{
			DebuffSum += Effect.GetStackedValue();
		}
	}

	return (BuffSum - DebuffSum) / CombatConstants::STAT_PERCENT_DIVISOR;
}

float UStatusBuildupManager::GetTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType) const
{
	// B-216: one composition, per damage type (ResistanceQuery.h). The sources and their order are
	// unchanged: General #1-3, the hit's types #4-6, the signed bonus #7-8, then the [-1, 1] clamp.
	return WoRResistance::GetTotalStatusResistance(Target, Element, PhysicalType);
}

// ========================================
// STATUS BAR MUTATION
// ========================================

// GetSourceStatusMultiplierFactor was RETIRED in the T3 consolidation. Its base-stat composition and
// the AddStatusBuildup step-5b transient are now the single getter
// UCharacterDataComponent::GetEffectiveStatusMultiplier (base + additive transient), which the live
// buildup, the BD overload bake, and crystal-wear all read — lockstep by construction.

bool UStatusBuildupManager::AddStatusBuildup(AActor *Source, AActor *Target, float Amount,
											 ESpellElement Element, EPhysicalDamageType PhysicalType,
											 bool bSkipBaseStatAmp)
{
	if (!Target || Amount <= 0.0f)
	{
		return false;
	}

	// Source-BD-ness for the dispatch: a BD caster emits Darkness (post-collapse)
	// but must apply EP-DRAIN, not Silence. Computed at entry so the corrected
	// trigger feeds BOTH the immunity gate below AND the cap-fire reuse (:428).
	// BD-ness is the SOURCE's (caster's) property — a dedicated lookup here, since
	// the StatusMultiplier component fetch later is gated by bSkipBaseStatAmp.
	bool bSourceIsBrokenDarkness = false;
	if (Source)
	{
		if (UCharacterDataComponent *SrcComp = Source->FindComponentByClass<UCharacterDataComponent>())
		{
			bSourceIsBrokenDarkness = SrcComp->IsBrokenDarkness();
		}
	}

	// Resolve which trigger this hit would fire and check target immunities up-front.
	// Buildup never accrues for immune targets (returns false and short-circuits the
	// amplification, resistance, and bar-update steps).
	const ESkillEffectType ResolvedTrigger =
		BarCapTriggerResolver::ResolveTrigger(Element, PhysicalType, bSourceIsBrokenDarkness);
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

	// Steps 5-6 (source amplification, then the target's resistance): WoRBuildupMath over the
	// gathered inputs (B-042l), the same core the GAS buildup execution runs.
	Amount = WoRBuildupMath::Compute(GatherBuildupMathInputs(Source, Target, Amount, Element, PhysicalType, bSkipBaseStatAmp));

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
			if (UBossPhaseComponent *Boss = Target->FindComponentByClass<UBossPhaseComponent>())
			{
				Boss->RecordStatusBreak();
			}
			return true;
		}

		UE_LOG(LogTemp, Verbose, TEXT("[StatusBuildupManager] %s status bar at cap but trigger is None — bar held"),
			   *Target->GetName());
	}

	return false;
}

FBuildupMathInputs UStatusBuildupManager::GatherBuildupMathInputs(AActor *Source, AActor *Target, float Amount,
																		 ESpellElement Element, EPhysicalDamageType PhysicalType,
																		 bool bSkipBaseStatAmp) const
{
	FBuildupMathInputs Inputs;
	Inputs.Amount = Amount;

	if (Source)
	{
		// Step 5 (+ former 5b) — source StatusMultiplier amplification: base-stat AND transient
		// buff/debuff, now a SINGLE factor from UCharacterDataComponent::GetEffectiveStatusMultiplier
		// (T3 consolidation — GetSourceStatusMultiplierFactor was retired). Gated by bSkipBaseStatAmp
		// for callers that already baked it into the incoming Amount (the BD overload aura: `released =
		// BaseRelease × StatusMult × Efficiency` is the single coupled quantity feeding both drain and
		// self-status — re-applying here would double-count). Steps 5c/6 still run below. 1.0 (no-op)
		// when Source has no CharacterDataComponent.
		if (!bSkipBaseStatAmp)
		{
			if (const UCharacterDataComponent *SrcComp = Source->FindComponentByClass<UCharacterDataComponent>())
			{
				Inputs.SourceStatusMultiplier = SrcComp->GetEffectiveStatusMultiplier();
			}

			// 5b-element: element-KEYED StatusMultiplierBuff/Debuff. The generic (Element == None)
			// layer is already folded into GetEffectiveStatusMultiplier above; this adds ONLY the
			// element-match (non-None) contribution, so the two layers are additive — a matching-element
			// buildup gets generic × element-keyed, a non-matching element gets generic only. Skipped for
			// a None-element (generic / physical) buildup, which has no element to key on (and would
			// otherwise re-read the generic None buffs, double-counting). Cast-driven for free: a
			// spell-granted StatusMultiplierBuff is stamped with the resolved cast element, so e.g. a Fire
			// spell's buff keys to Fire automatically.
			if (Element != ESpellElement::None)
			{
				if (USkillEffectManager *EffectMgr = GetEffectManager())
				{
					const float SmBuffElem = EffectMgr->GetTotalStatModifierForElement(Source, ESkillEffectType::StatusMultiplierBuff, Element);
					const float SmDebuffElem = EffectMgr->GetTotalStatModifierForElement(Source, ESkillEffectType::StatusMultiplierDebuff, Element);
					Inputs.ElementStatusMultiplier = FMath::Max(0.0f, 1.0f + (SmBuffElem - SmDebuffElem) / CombatConstants::STAT_PERCENT_DIVISOR);
				}
			}
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
		if (const UBrokenDarknessManager *SourceBD = Source->FindComponentByClass<UBrokenDarknessManager>())
		{
			Inputs.StackMultiplier = SourceBD->GetElementStackStatusMultiplier(Element);
		}
	}

	// Step 6: the target's DEFENDER-side resistance — the full composition (base Spirit + equipment +
	// attached stone + element/physical effects + class/innate profile + ModifyStatusResist), as its
	// three layers; WoRBuildupMath clamps their sum. Offense-side amps stay out of it. All 0 when
	// Target has no CharacterData (WoRResistance::GetTotalStatusResistance returns 0 then).
	const UCharacterDataComponent *TargetComp = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (TargetComp && TargetComp->CharacterData)
	{
		Inputs.GeneralStatusResistance = WoRResistance::GetGeneralStatusResistance(Target);
		Inputs.HitTypeResistance = WoRResistance::GetHitTypeResistance(Target, Element, PhysicalType);
		Inputs.StatusResistBonus = WoRResistance::GetStatusResistBonus(Target);
	}
	return Inputs;
}

void UStatusBuildupManager::ResetForCombat()
{
	StatusBarStates.Empty();
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
		State->PendingElement = ESpellElement::None;
		State->PendingPhysicalType = EPhysicalDamageType::None;
		State->LastSource = nullptr;
		State->TurnsSinceLastHit = 0;

		// LastSource was just cleared above — broadcast null; the reset bar is
		// neutral anyway (PendingElement == None).
		OnStatusBuildupChanged.Broadcast(Target, 0.0f,
			CombatConstants::STATUS_EFFECT_THRESHOLD, ESpellElement::None, nullptr);
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
