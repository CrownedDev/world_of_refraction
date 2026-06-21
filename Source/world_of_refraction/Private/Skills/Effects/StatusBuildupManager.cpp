// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skills/Effects/StatusBuildupManager.h"
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
	if (!Target)
	{
		return 0.0f;
	}

	// Guard moved inside: no CharacterData -> 0 resistance, so the caller's
	// Amount *= (1 - 0) is unchanged (same as the old block being skipped).
	UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	if (!TargetComp || !TargetComp->CharacterData)
	{
		return 0.0f;
	}

	// GENERAL resistance (cluster A2) — blanket positive-only tankiness, MULTIPLICATIVE (matches every
	// other stat's Pattern-P shape). #1 crystal-aware Spirit stat capped ALONE at UNIVERSAL_STAT_CAP
	// (0.5), THEN #2 BonusResistance and #3 attached ResistanceStone MULTIPLY it past 0.5 toward the
	// RESISTANCE_MAX (1.0) general-tankiness ceiling. Floors at 0 — general tankiness never goes
	// negative; the amplify range comes entirely from the MATCHED layer below.
	float General = TargetComp->GetCrystalResistanceStatCapped();   // #1 crystal Spirit stat, capped 0.5
	if (ULoadoutComponent *TargetLoadout = Target->FindComponentByClass<ULoadoutComponent>())
	{
		const FEquipmentStatBonus TargetBonus = TargetLoadout->GetActiveStatBonus(Target);
		General *= (1.0f + TargetBonus.BonusResistance * CombatConstants::RESISTANCE_PER_POINT);   // #2 equip (per-point fraction)

		// #3 Attached ResistanceStone — the DEFENDER's OWN active weapon attachment (live-resolved,
		// fusion-aware). % MULTIPLIER on the capped stat. Inert (×1) unless a ResistanceStone is attached.
		if (const FRuntimeAttachedItem *AttPtr = TargetLoadout->GetActiveWeaponAttachment())
		{
			const FRuntimeAttachedItem &Attachment = *AttPtr;
			General *= (1.0f + CrystalEffectTable::GetAttachedStonePercent(Attachment, ESubStat::Resistance) / CombatConstants::STAT_PERCENT_DIVISOR);
		}
	}
	General = FMath::Clamp(General, 0.0f, CombatConstants::RESISTANCE_MAX);   // general ceiling 1.0, floor 0

	// MATCHED resistance — ADDITIVE, can go negative (the amplify layer). Preserves the matchup
	// composition + the FResistanceBonus zero-sum budget + the blanket ModifyStatusResist curse. Added
	// on top of General; the combined clamp lets these pull the TOTAL negative → amplified buildup.
	float Matched = 0.0f;

	// #4 Element + physical effect resistance (ResistanceBuff/Debuff, matched on the incoming axis).
	Matched += GetTotalElementResistance(Target, Element, PhysicalType);

	// #5 Class / innate-element innate resistance — element column (BD-aliased) + physical column.
	Matched += ClassInnateResistanceTable::GetClassInnateResistance(Target, Element, PhysicalType);

	// #6 Gear per-category resistance — rolled/authored FResistanceBonus (zero-sum), aggregated across
	// equipped pieces. UNTOUCHED by A2: still additive-per-category, read through the SAME class-table
	// column helpers (BD->Darkness alias, (element + physical)/100) so the zero-sum budget is intact.
	if (ULoadoutComponent *GearLoadout = Target->FindComponentByClass<ULoadoutComponent>())
	{
		const FResistanceBonus GearResist = GearLoadout->GetActiveResistanceBonus(Target);
		const ClassInnateResistanceTable::FResistanceRow GearRow{
			GearResist.Fire, GearResist.Water, GearResist.Earth, GearResist.Wind,
			GearResist.Light, GearResist.Darkness, GearResist.Lightning, GearResist.Void,
			GearResist.Reality, GearResist.Slash, GearResist.Pierce, GearResist.Impact};
		Matched += (ClassInnateResistanceTable::GetElementColumn(GearRow, Element) +
		            ClassInnateResistanceTable::GetPhysicalColumn(GearRow, PhysicalType)) /
		           ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}

	// #7 Skill-effect ModifyStatusResist — blanket, sign-aware CURSE/buff. ADDITIVE (the A2 correction):
	// a negative ModifyStatusResist (the ResistanceStone enemy-curse) must reach the amplify range — the
	// multiplicative general would floor it at 0 and kill the curse, so it composes here, not in General.
	if (USkillEffectManager *EffectMgr = GetEffectManager())
	{
		const float ResistModify = EffectMgr->GetTotalStatModifier(Target, ESkillEffectType::ModifyStatusResist);
		Matched += ResistModify / CombatConstants::STAT_PERCENT_DIVISOR;
	}

	// Final: General (≥0 tankiness) + Matched (±). Floor RESISTANCE_MIN (-1.0): a net weakness strips
	// through 0 into negative, where (1 - Resistance) > 1 AMPLIFIES buildup (capped 2x at -1). Upper
	// RESISTANCE_MAX (1.0): resistance > 1 would heal the gauge.
	return FMath::Clamp(General + Matched, CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX);
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

	// Apply attacker StatusMultiplier amplification — the single crystal-aware getter
	// GetEffectiveStatusMultiplier (T3 consolidation): crystal-aware Spirit stat capped ×1.5, gear
	// (BonusStatusMultiplier + StatusStone) multiplicative beyond, additive transient buff/debuff.
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
			if (UCharacterDataComponent *SrcComp = Source->FindComponentByClass<UCharacterDataComponent>())
			{
				Amount *= SrcComp->GetEffectiveStatusMultiplier();
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
		if (UBrokenDarknessManager *SourceBD = Source->FindComponentByClass<UBrokenDarknessManager>())
		{
			Amount *= SourceBD->GetElementStackStatusMultiplier(Element);
		}
	}

	// Apply target's DEFENDER-side resistance reduction — the full six-source
	// composition (base Spirit + equipment + attached stone + element/physical
	// effects + class/innate profile + ModifyStatusResist), clamped, as one value.
	// Offense-side amps (StatusMultiplier, BD absorption stack) are applied ABOVE
	// and deliberately stay out of this. Returns 0 when Target has no CharacterData,
	// so Amount *= (1 - 0) leaves it unchanged (same as the old guarded block).
	Amount *= (1.0f - GetTotalStatusResistance(Target, Element, PhysicalType));

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
