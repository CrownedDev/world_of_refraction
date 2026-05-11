// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillEffectManager.h"
#include "CharacterDataComponent.h"
#include "TurnManager.h"
#include "EStatusType.h"
#include "CombatConstants.h"
#include "InfusionConstants.h"
#include "CharacterDataComponent.h"
#include "StatusDisplayNames.h"

// ========================================
// SUBSYSTEM LIFECYCLE
// ========================================

void USkillEffectManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);

	ActiveEffects.Empty();
	NextInstanceID = 1;

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Initialized"));
}

void USkillEffectManager::Deinitialize()
{
	ClearAllEffects();

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Deinitialized"));

	Super::Deinitialize();
}

// ========================================
// EFFECT APPLICATION
// ========================================

EEffectApplicationResult USkillEffectManager::ApplyEffect(AActor *Target, FStatusEffect Effect,
														   AActor *Source, const FString &SourceAbility, int32 SourceTeam)
{
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillEffectManager] ApplyEffect: Target is null"));
		return EEffectApplicationResult::Rejected;
	}

	// Check immunity
	if (IsImmuneToEffectType(Target, Effect.EffectType))
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s is immune to %s"),
			   *Target->GetName(), *Effect.EffectName);
		return EEffectApplicationResult::Rejected;
	}

	// Set source info
	Effect.SourceActor = Source;
	Effect.SourceAbilityName = SourceAbility;
	Effect.SourceTeamIndex = SourceTeam;
	Effect.InitialDuration = Effect.RemainingTurns;

	// Get or create effect array for this actor
	TArray<FStatusEffect> &Effects = ActiveEffects.FindOrAdd(Target);

	// Check for existing effect with same ID
	FStatusEffect *ExistingEffect = FindEffectByID(Target, Effect.EffectID);

	if (ExistingEffect)
	{
		// Handle stacking
		if (Effect.bCanStack && ExistingEffect->CanAddStack())
		{
			ExistingEffect->CurrentStacks++;

			if (Effect.bRefreshDurationOnReapply)
			{
				ExistingEffect->RemainingTurns = Effect.InitialDuration;
			}

			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s: %s stacked to %d on %s"),
				   *Target->GetName(), *Effect.EffectName, ExistingEffect->CurrentStacks, *Target->GetName());

			OnEffectStacksChanged.Broadcast(Target, *ExistingEffect, ExistingEffect->CurrentStacks);

			// Notify TurnManager if speed effect stacked (value increased)
			if (IsSpeedEffect(Effect.EffectType))
			{
				NotifySpeedChanged(Target);
			}

			return EEffectApplicationResult::StackAdded;
		}
		else if (Effect.bRefreshDurationOnReapply)
		{
			ExistingEffect->RemainingTurns = Effect.InitialDuration;

			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s: %s duration refreshed to %d turns"),
				   *Target->GetName(), *Effect.EffectName, ExistingEffect->RemainingTurns);

			OnEffectDurationChanged.Broadcast(Target, *ExistingEffect, ExistingEffect->RemainingTurns);
			return EEffectApplicationResult::DurationRefreshed;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s: %s rejected (already at max stacks)"),
				   *Target->GetName(), *Effect.EffectName);
			return EEffectApplicationResult::Rejected;
		}
	}

	// New effect - apply it
	Effects.Add(Effect);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applied %s to %s (ID: %d, Duration: %d, Value: %.1f)"),
		   *Effect.EffectName, *Target->GetName(), Effect.EffectID, Effect.RemainingTurns, Effect.EffectValue);

	// Process immediate effects right away
	if (Effect.ProcessTiming == EStatusEffectTiming::Immediate)
	{
		ApplyEffectLogic(Target, Effects.Last());

		// Immediate effects are one-shot, mark for removal
		if (!Effect.bPermanent)
		{
			Effects.Last().bPendingRemoval = true;
		}
	}

	// Notify TurnManager if this is a speed effect
	if (IsSpeedEffect(Effect.EffectType))
	{
		NotifySpeedChanged(Target);
	}

	OnEffectApplied.Broadcast(Target, Effect);
	return EEffectApplicationResult::Applied;
}

void USkillEffectManager::ApplyEffects(AActor *Target, const TArray<FStatusEffect> &Effects,
										AActor *Source, const FString &SourceAbility, int32 SourceTeam)
{
	for (const FStatusEffect &Effect : Effects)
	{
		ApplyEffect(Target, Effect, Source, SourceAbility, SourceTeam);
	}
}

void USkillEffectManager::ApplyInfusionDOT(
	AActor *Target,
	const FString &AbilityName,
	int32 AbilityID,
	ESpellElement InfusedElement,
	float DOTDamage,
	int32 Duration,
	AActor *Source,
	int32 SourceTeam)
{
	FStatusEffect InfusionEffect = FStatusEffect::CreateFromInfusion(
		AbilityName,
		AbilityID,
		InfusedElement,
		DOTDamage,
		Duration);

	ApplyEffect(Target, InfusionEffect, Source, AbilityName + TEXT(" (Infused)"), SourceTeam);
}

void USkillEffectManager::ApplyEvolutionPassives(
	AActor *Target,
	const FString &EvolutionName,
	int32 EvolutionID,
	const TArray<EStatusType> &PassiveTypes,
	const TArray<float> &PassiveValues)
{
	if (PassiveTypes.Num() != PassiveValues.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillEffectManager] ApplyEvolutionPassives: Mismatched array sizes"));
		return;
	}

	for (int32 i = 0; i < PassiveTypes.Num(); ++i)
	{
		if (PassiveTypes[i] != EStatusType::None)
		{
			FStatusEffect Passive = FStatusEffect::CreateFromEvolutionPassive(
				EvolutionName,
				EvolutionID,
				PassiveTypes[i],
				PassiveValues[i],
				i);

			ApplyEffect(Target, Passive, nullptr, EvolutionName, -1);
		}
	}
}

// ========================================
// WEAPON EFFECT APPLICATION
// ========================================

void USkillEffectManager::ApplyWeaponBonuses(
	AActor *Target,
	const FString &WeaponName,
	int32 WeaponID,
	int32 BonusAttack,
	int32 BonusDefense,
	int32 BonusMagicPower,
	int32 BonusSpeed,
	float BonusCritChance,
	float BonusCritDamage)
{
	TArray<FStatusEffect> Bonuses = FStatusEffect::CreateFromWeaponBonuses(
		WeaponName,
		WeaponID,
		BonusAttack,
		BonusDefense,
		BonusMagicPower,
		BonusSpeed,
		BonusCritChance,
		BonusCritDamage);

	if (Bonuses.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applying %d weapon bonuses from %s to %s"),
			   Bonuses.Num(), *WeaponName, *Target->GetName());
	}

	ApplyEffects(Target, Bonuses, nullptr, WeaponName + TEXT(" (Equipped)"), -1);
}

void USkillEffectManager::RemoveWeaponBonuses(AActor *Target, int32 WeaponID)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return;
	}

	int32 BaseID = WeaponID * 100;
	int32 RemovedCount = 0;
	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	// Remove effects with IDs in the weapon bonus range (BaseID+1 through BaseID+6)
	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		int32 EffectID = Effects[i].EffectID;
		if (EffectID >= BaseID + 1 && EffectID <= BaseID + 6)
		{
			FStatusEffect RemovedEffect = Effects[i];
			Effects.RemoveAt(i);
			RemovedCount++;
			OnEffectRemoved.Broadcast(Target, RemovedEffect);
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %d weapon bonuses (WeaponID: %d) from %s"),
			   RemovedCount, WeaponID, *Target->GetName());
	}
}

void USkillEffectManager::ApplyPhysicalDamageEffect(
	AActor *Target,
	const FString &WeaponName,
	int32 WeaponID,
	uint8 PhysicalType,
	int32 StatusBuildup,
	float InfusionMultiplier,
	int32 HitCount,
	AActor *Source,
	int32 SourceTeam)
{
	FStatusEffect Effect = FStatusEffect::CreateFromPhysicalDamageType(
		WeaponName,
		WeaponID,
		PhysicalType,
		StatusBuildup,
		InfusionMultiplier,
		HitCount);

	// Log what physical effect is being applied
	FString TypeName;
	switch (PhysicalType)
	{
	case 0:
		TypeName = TEXT("Bleed");
		break;
	case 1:
		TypeName = TEXT("Armor Break");
		break;
	case 2:
		TypeName = TEXT("Stun");
		break;
	default:
		TypeName = TEXT("Unknown");
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applying %s from %s (Buildup: %d × %.1f × %d hits)"),
		   *TypeName, *WeaponName, StatusBuildup, InfusionMultiplier, HitCount);

	ApplyEffect(Target, Effect, Source, WeaponName, SourceTeam);
}

void USkillEffectManager::ApplyWeaponInfusionDOT(
	AActor *Target,
	const FString &AbilityName,
	const FString &WeaponName,
	int32 AbilityID,
	ESpellElement InfusedElement,
	float BaseDOTDamage,
	int32 Duration,
	float WeaponInfusionMultiplier,
	AActor *Source,
	int32 SourceTeam)
{
	FStatusEffect Effect = FStatusEffect::CreateFromWeaponInfusion(
		AbilityName,
		WeaponName,
		AbilityID,
		InfusedElement,
		BaseDOTDamage,
		Duration,
		WeaponInfusionMultiplier);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applying weapon-infused DOT: %s (%.1f damage × %.1f multiplier)"),
		   *Effect.EffectName, BaseDOTDamage, WeaponInfusionMultiplier);

	ApplyEffect(Target, Effect, Source, AbilityName + TEXT(" (Weapon Infused)"), SourceTeam);
}

// ========================================
// EFFECT REMOVAL
// ========================================

bool USkillEffectManager::RemoveEffectByID(AActor *Target, int32 EffectID)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return false;
	}

	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].EffectID == EffectID)
		{
			FStatusEffect RemovedEffect = Effects[i];
			bool bWasSpeedEffect = IsSpeedEffect(RemovedEffect.EffectType);

			Effects.RemoveAt(i);

			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %s from %s (by ID)"),
				   *RemovedEffect.EffectName, *Target->GetName());

			OnEffectRemoved.Broadcast(Target, RemovedEffect);

			// Notify TurnManager if speed effect was removed
			if (bWasSpeedEffect)
			{
				NotifySpeedChanged(Target);
			}

			return true;
		}
	}

	return false;
}

int32 USkillEffectManager::RemoveEffectsByName(AActor *Target, const FString &EffectName)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	int32 RemovedCount = 0;
	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].EffectName == EffectName)
		{
			FStatusEffect RemovedEffect = Effects[i];
			Effects.RemoveAt(i);
			RemovedCount++;

			OnEffectRemoved.Broadcast(Target, RemovedEffect);
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %d effects named '%s' from %s"),
			   RemovedCount, *EffectName, *Target->GetName());
	}

	return RemovedCount;
}

int32 USkillEffectManager::RemoveEffectsByType(AActor *Target, EStatusType EffectType)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	bool bWasSpeedEffect = IsSpeedEffect(EffectType);
	int32 RemovedCount = 0;
	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].EffectType == EffectType)
		{
			FStatusEffect RemovedEffect = Effects[i];
			Effects.RemoveAt(i);
			RemovedCount++;

			OnEffectRemoved.Broadcast(Target, RemovedEffect);
		}
	}

	// Notify TurnManager if speed effects were removed
	if (RemovedCount > 0 && bWasSpeedEffect)
	{
		NotifySpeedChanged(Target);
	}

	return RemovedCount;
}

int32 USkillEffectManager::RemoveAllBuffs(AActor *Target)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	int32 RemovedCount = 0;
	bool bSpeedChanged = false;
	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].IsBuff())
		{
			FStatusEffect RemovedEffect = Effects[i];

			if (IsSpeedEffect(RemovedEffect.EffectType))
			{
				bSpeedChanged = true;
			}

			Effects.RemoveAt(i);
			RemovedCount++;

			OnEffectRemoved.Broadcast(Target, RemovedEffect);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %d buffs from %s"),
		   RemovedCount, *Target->GetName());

	// Notify TurnManager if any speed buffs were removed
	if (bSpeedChanged)
	{
		NotifySpeedChanged(Target);
	}

	return RemovedCount;
}

int32 USkillEffectManager::RemoveAllDebuffs(AActor *Target)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	int32 RemovedCount = 0;
	bool bSpeedChanged = false;
	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].IsDebuff())
		{
			FStatusEffect RemovedEffect = Effects[i];

			if (IsSpeedEffect(RemovedEffect.EffectType))
			{
				bSpeedChanged = true;
			}

			Effects.RemoveAt(i);
			RemovedCount++;

			OnEffectRemoved.Broadcast(Target, RemovedEffect);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %d debuffs from %s"),
		   RemovedCount, *Target->GetName());

	// Notify TurnManager if any speed debuffs were removed
	if (bSpeedChanged)
	{
		NotifySpeedChanged(Target);
	}

	return RemovedCount;
}

int32 USkillEffectManager::RemoveAllDOTs(AActor *Target)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	int32 RemovedCount = 0;
	TArray<FStatusEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].IsDOT())
		{
			FStatusEffect RemovedEffect = Effects[i];
			Effects.RemoveAt(i);
			RemovedCount++;

			OnEffectRemoved.Broadcast(Target, RemovedEffect);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %d DOTs from %s"),
		   RemovedCount, *Target->GetName());

	return RemovedCount;
}

void USkillEffectManager::RemoveAllEffects(AActor *Target)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return;
	}

	TArray<FStatusEffect> &Effects = ActiveEffects[Target];
	bool bSpeedChanged = false;

	// Check for speed effects and broadcast removal for each effect
	for (const FStatusEffect &Effect : Effects)
	{
		if (IsSpeedEffect(Effect.EffectType))
		{
			bSpeedChanged = true;
		}
		OnEffectRemoved.Broadcast(Target, Effect);
	}

	int32 Count = Effects.Num();
	Effects.Empty();
	ActiveEffects.Remove(Target);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed all %d effects from %s"),
		   Count, *Target->GetName());

	// Notify TurnManager if any speed effects were removed
	if (bSpeedChanged)
	{
		NotifySpeedChanged(Target);
	}
}

void USkillEffectManager::ClearAllEffects()
{
	for (auto &Pair : ActiveEffects)
	{
		if (Pair.Key.IsValid())
		{
			for (const FStatusEffect &Effect : Pair.Value)
			{
				OnEffectRemoved.Broadcast(Pair.Key.Get(), Effect);
			}
		}
	}

	int32 TotalCount = 0;
	for (const auto &Pair : ActiveEffects)
	{
		TotalCount += Pair.Value.Num();
	}

	ActiveEffects.Empty();

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Cleared all effects (%d total)"), TotalCount);
}

int32 USkillEffectManager::RemoveEffectsBySource(AActor *Source)
{
	if (!Source)
	{
		return 0;
	}

	int32 TotalRemoved = 0;
	TArray<TWeakObjectPtr<AActor>> ActorsWithSpeedChanges;

	for (auto &Pair : ActiveEffects)
	{
		if (!Pair.Key.IsValid())
			continue;

		TArray<FStatusEffect> &Effects = Pair.Value;
		bool bSpeedChangedForActor = false;

		for (int32 i = Effects.Num() - 1; i >= 0; --i)
		{
			if (Effects[i].SourceActor == Source)
			{
				FStatusEffect RemovedEffect = Effects[i];

				if (IsSpeedEffect(RemovedEffect.EffectType))
				{
					bSpeedChangedForActor = true;
				}

				Effects.RemoveAt(i);
				TotalRemoved++;

				OnEffectRemoved.Broadcast(Pair.Key.Get(), RemovedEffect);
			}
		}

		if (bSpeedChangedForActor)
		{
			ActorsWithSpeedChanges.Add(Pair.Key);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Removed %d effects from source %s"),
		   TotalRemoved, *Source->GetName());

	// Notify TurnManager for all affected actors
	for (const TWeakObjectPtr<AActor> &Actor : ActorsWithSpeedChanges)
	{
		if (Actor.IsValid())
		{
			NotifySpeedChanged(Actor.Get());
		}
	}

	return TotalRemoved;
}

// ========================================
// TURN PROCESSING
// ========================================

void USkillEffectManager::ProcessStartOfTurnEffects(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	ResetTurnFlags(Actor);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Processing start-of-turn effects for %s"),
		   *Actor->GetName());

	// Process start-of-turn effects
	ProcessEffectsWithTiming(Actor, EStatusEffectTiming::StartOfOwnTurn);

	// Process persistent effects (always active, but we re-apply stat mods)
	ProcessEffectsWithTiming(Actor, EStatusEffectTiming::Persistent);

	// Check conditional triggers with current state
	ProcessTriggerEffects(Actor, EPassiveTrigger::OnTurnStart);
}

void USkillEffectManager::ProcessEndOfTurnEffects(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Processing end-of-turn effects for %s"),
		   *Actor->GetName());

	// Process end-of-turn effects (DOTs, etc.)
	ProcessEffectsWithTiming(Actor, EStatusEffectTiming::EndOfOwnTurn);

	// Check conditional triggers
	ProcessTriggerEffects(Actor, EPassiveTrigger::OnTurnEnd);

	// Tick durations and remove expired effects
	TickDurations(Actor);

	// Clean up effects marked for removal
	if (ActiveEffects.Contains(Actor))
	{
		TArray<FStatusEffect> &Effects = ActiveEffects[Actor];

		for (int32 i = Effects.Num() - 1; i >= 0; --i)
		{
			if (Effects[i].bPendingRemoval)
			{
				FStatusEffect RemovedEffect = Effects[i];
				Effects.RemoveAt(i);
				OnEffectRemoved.Broadcast(Actor, RemovedEffect);
			}
		}
	}
}

void USkillEffectManager::ProcessTriggerEffects(AActor *Actor, EPassiveTrigger Trigger, float TriggerValue)
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return;
	}

	TArray<FStatusEffect> &Effects = ActiveEffects[Actor];

	for (FStatusEffect &Effect : Effects)
	{
		if (Effect.ProcessTiming == EStatusEffectTiming::OnTrigger &&
			Effect.TriggerCondition == Trigger)
		{
			if (IsTriggerConditionMet(Actor, Effect, TriggerValue))
			{
				if (!Effect.bTriggerActive)
				{
					Effect.bTriggerActive = true;
					ApplyEffectLogic(Actor, Effect);

					UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Trigger activated: %s on %s"),
						   *Effect.EffectName, *Actor->GetName());
				}
			}
			else
			{
				Effect.bTriggerActive = false;
			}
		}
	}
}

void USkillEffectManager::ProcessEffectsWithTiming(AActor *Actor, EStatusEffectTiming Timing)
{
	if (!ActiveEffects.Contains(Actor))
	{
		return;
	}

	TArray<FStatusEffect> &Effects = ActiveEffects[Actor];

	for (FStatusEffect &Effect : Effects)
	{
		if (Effect.ProcessTiming == Timing && !Effect.bProcessedThisTurn)
		{
			ApplyEffectLogic(Actor, Effect);
			Effect.bProcessedThisTurn = true;

			OnEffectTriggered.Broadcast(Actor, Effect);
		}
	}
}

void USkillEffectManager::TickDurations(AActor *Actor)
{
	if (!ActiveEffects.Contains(Actor))
	{
		return;
	}

	TArray<FStatusEffect> &Effects = ActiveEffects[Actor];
	bool bSpeedChanged = false;

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		FStatusEffect &Effect = Effects[i];

		// Skip permanent effects
		if (Effect.bPermanent)
		{
			continue;
		}

		// Skip immediate effects (already handled)
		if (Effect.ProcessTiming == EStatusEffectTiming::Immediate)
		{
			continue;
		}

		// Tick duration
		Effect.RemainingTurns--;

		OnEffectDurationChanged.Broadcast(Actor, Effect, Effect.RemainingTurns);

		if (Effect.RemainingTurns <= 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s expired on %s"),
				   *Effect.EffectName, *Actor->GetName());

			// Track if speed effect expired
			if (IsSpeedEffect(Effect.EffectType))
			{
				bSpeedChanged = true;
			}

			FStatusEffect ExpiredEffect = Effect;
			Effects.RemoveAt(i);

			OnEffectRemoved.Broadcast(Actor, ExpiredEffect);
		}
	}

	// Notify TurnManager once if any speed effects expired
	if (bSpeedChanged)
	{
		NotifySpeedChanged(Actor);
	}
}

void USkillEffectManager::ApplyEffectLogic(AActor *Actor, FStatusEffect &Effect)
{
	UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);

	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillEffectManager] No CharacterDataComponent on %s"),
			   *Actor->GetName());
		return;
	}

	float Value = Effect.GetStackedValue();

	switch (Effect.EffectType)
	{
	// ==================== DOT EFFECTS ====================
	case EStatusType::DOT:
		CharComp->ServerTakeDamage(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s took %d DOT damage from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

		// ==================== HEALING ====================
	case EStatusType::HealthRestore:
		CharComp->ServerHeal(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s healed %d from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	// ==================== ENERGY ====================
	case EStatusType::EnergyRestore:
		CharComp->ServerGainEnergy(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s gained %d energy from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	case EStatusType::EnergyDrain:
		CharComp->ServerSpendEnergy(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s lost %d energy from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	// ==================== STAT MODIFIERS ====================
	// These don't apply immediate changes - they're queried via GetTotalStatModifier
	case EStatusType::DamageBuff:
	case EStatusType::DamageDebuff:
	case EStatusType::DefenseBuff:
	case EStatusType::DefenseDebuff:
	case EStatusType::SpeedBuff:
	case EStatusType::SpeedDebuff:
	case EStatusType::MindBuff:
	case EStatusType::MindDebuff:
	case EStatusType::BodyBuff:
	case EStatusType::BodyDebuff:
	case EStatusType::SpiritBuff:
	case EStatusType::SpiritDebuff:
	case EStatusType::CritChanceBuff:
	case EStatusType::CritChanceDebuff:
	case EStatusType::AttackSpeedBuff:
	case EStatusType::AttackSpeedDebuff:
	case EStatusType::RawDamageBuff:
	case EStatusType::RawDamageDebuff:
	case EStatusType::EffectDamageBuff:
	case EStatusType::EffectDamageDebuff:
	case EStatusType::SpellCostBuff:
	case EStatusType::SpellCostDebuff:
	case EStatusType::ResistanceBuff:
	case EStatusType::ResistanceDebuff:
	case EStatusType::SpellSizeBuff:
	case EStatusType::SpellSizeDebuff:
	case EStatusType::MaxEnergyBuff:
	case EStatusType::MaxEnergyDebuff:
		// Stat modifiers are passive - other systems query GetTotalStatModifier
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Stat modifier %s active on %s (%.1f%%)"),
			   *Effect.EffectName, *Actor->GetName(), Value);
		break;

	case EStatusType::RemoveSpeedDebuff:
		RemoveEffectsByType(Actor, EStatusType::SpeedDebuff);
		break;

	case EStatusType::RemoveDamageDebuff:
		RemoveEffectsByType(Actor, EStatusType::DamageDebuff);
		break;

	case EStatusType::RemoveDefenseDebuff:
		RemoveEffectsByType(Actor, EStatusType::DefenseDebuff);
		break;

	default:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Unhandled effect type for %s"),
			   *Effect.EffectName);
		break;
	}
}

// ========================================
// QUERIES
// ========================================

TArray<FStatusEffect> USkillEffectManager::GetActiveEffects(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return TArray<FStatusEffect>();
	}

	return ActiveEffects[Actor];
}

TArray<FStatusEffect> USkillEffectManager::GetEffectsByType(AActor *Actor, EStatusType EffectType) const
{
	TArray<FStatusEffect> Result;

	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return Result;
	}

	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectType == EffectType)
		{
			Result.Add(Effect);
		}
	}

	return Result;
}

bool USkillEffectManager::HasEffectByID(AActor *Actor, int32 EffectID) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}

	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectID == EffectID)
		{
			return true;
		}
	}

	return false;
}

bool USkillEffectManager::HasEffectOfType(AActor *Actor, EStatusType EffectType) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}

	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectType == EffectType)
		{
			return true;
		}
	}

	return false;
}

float USkillEffectManager::GetTotalStatModifier(AActor *Actor, EStatusType ModifierType) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return 0.0f;
	}

	float Total = 0.0f;

	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectType == ModifierType)
		{
			Total += Effect.GetStackedValue();
		}
	}

	return Total;
}

int32 USkillEffectManager::GetEffectCount(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return 0;
	}

	return ActiveEffects[Actor].Num();
}

int32 USkillEffectManager::GetBuffCount(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return 0;
	}

	int32 Count = 0;
	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.IsBuff())
		{
			Count++;
		}
	}

	return Count;
}

int32 USkillEffectManager::GetDebuffCount(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return 0;
	}

	int32 Count = 0;
	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.IsDebuff())
		{
			Count++;
		}
	}

	return Count;
}

// ========================================
// STATUS CHECKS
// ========================================

bool USkillEffectManager::IsStunned(AActor *Actor) const
{
	// TODO: Add stun effect type when implemented
	return false;
}

bool USkillEffectManager::IsSilenced(AActor *Actor) const
{
	// TODO: Add silence effect type when implemented
	return false;
}

bool USkillEffectManager::HasActiveDOT(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}

	for (const FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.IsDOT())
		{
			return true;
		}
	}

	return false;
}

bool USkillEffectManager::IsImmuneToEffectType(AActor *Actor, EStatusType EffectType) const
{
	// TODO: Implement immunity system via ResistanceBuff at 100%+
	return false;
}

// ========================================
// DEBUG TOOLS
// ========================================

void USkillEffectManager::DebugPrintEffects(AActor *Actor) const
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Display, TEXT("[SkillEffectManager] DEBUG: Actor is null"));
		return;
	}

	if (!ActiveEffects.Contains(Actor))
	{
		UE_LOG(LogTemp, Display, TEXT("[SkillEffectManager] DEBUG: %s has no active effects"),
			   *Actor->GetName());
		return;
	}

	const TArray<FStatusEffect> &Effects = ActiveEffects[Actor];

	UE_LOG(LogTemp, Display, TEXT("[SkillEffectManager] DEBUG: %s has %d effects:"),
		   *Actor->GetName(), Effects.Num());

	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FStatusEffect &Effect = Effects[i];
		UE_LOG(LogTemp, Display, TEXT("  [%d] %s (ID:%d) - %s, Value:%.1f, Duration:%s, Stacks:%d"),
			   i, *Effect.EffectName, Effect.EffectID,
			   *UEnum::GetValueAsString(Effect.EffectType),
			   Effect.EffectValue, *Effect.GetDurationString(), Effect.CurrentStacks);
	}
}

void USkillEffectManager::DebugPrintAllEffects() const
{
	UE_LOG(LogTemp, Display, TEXT("[SkillEffectManager] DEBUG: === ALL ACTIVE EFFECTS ==="));

	int32 TotalEffects = 0;

	for (const auto &Pair : ActiveEffects)
	{
		if (Pair.Key.IsValid())
		{
			DebugPrintEffects(Pair.Key.Get());
			TotalEffects += Pair.Value.Num();
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[SkillEffectManager] DEBUG: Total: %d effects on %d actors"),
		   TotalEffects, ActiveEffects.Num());
}

FString USkillEffectManager::GetEffectsSummary(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return TEXT("No effects");
	}

	const TArray<FStatusEffect> &Effects = ActiveEffects[Actor];

	if (Effects.Num() == 0)
	{
		return TEXT("No effects");
	}

	FString Summary;
	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FStatusEffect &Effect = Effects[i];
		if (i > 0)
			Summary += TEXT("\n");
		Summary += FString::Printf(TEXT("%s %s (%s)"),
								   *Effect.EffectName, *Effect.GetStackString(), *Effect.GetDurationString());
	}

	return Summary;
}

// ========================================
// INTERNAL HELPERS
// ========================================

bool USkillEffectManager::IsTriggerConditionMet(AActor *Actor, const FStatusEffect &Effect, float TriggerValue) const
{
	UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);
	if (!CharComp)
	{
		return false;
	}

	switch (Effect.TriggerCondition)
	{
	case EPassiveTrigger::OnHPBelowThreshold:
	{
		float HPPercent = (CharComp->MaxHP > 0)
							  ? (static_cast<float>(CharComp->CurrentHP) / static_cast<float>(CharComp->MaxHP)) * 100.0f
							  : 0.0f;
		return HPPercent < Effect.TriggerThreshold;
	}

	case EPassiveTrigger::OnHPAboveThreshold:
	{
		float HPPercent = (CharComp->MaxHP > 0)
							  ? (static_cast<float>(CharComp->CurrentHP) / static_cast<float>(CharComp->MaxHP)) * 100.0f
							  : 0.0f;
		return HPPercent > Effect.TriggerThreshold;
	}

	case EPassiveTrigger::OnEnergyBelowThreshold:
	{
		float EPPercent = (CharComp->MaxEP > 0)
							  ? (static_cast<float>(CharComp->CurrentEP) / static_cast<float>(CharComp->MaxEP)) * 100.0f
							  : 0.0f;
		return EPPercent < Effect.TriggerThreshold;
	}

	case EPassiveTrigger::OnEnergyAboveThreshold:
	{
		float EPPercent = (CharComp->MaxEP > 0)
							  ? (static_cast<float>(CharComp->CurrentEP) / static_cast<float>(CharComp->MaxEP)) * 100.0f
							  : 0.0f;
		return EPPercent > Effect.TriggerThreshold;
	}

	case EPassiveTrigger::Always:
		return true;

	// Event-based triggers are always "met" when the event occurs
	case EPassiveTrigger::OnCrit:
	case EPassiveTrigger::OnHit:
	case EPassiveTrigger::OnTakeDamage:
	case EPassiveTrigger::OnKill:
	case EPassiveTrigger::OnDodge:
	case EPassiveTrigger::OnBlock:
	case EPassiveTrigger::OnTurnStart:
	case EPassiveTrigger::OnTurnEnd:
	case EPassiveTrigger::OnSpellCast:
	case EPassiveTrigger::OnAbilityUsed:
	case EPassiveTrigger::OnBattleStart:
	case EPassiveTrigger::OnStatusApplied:
	case EPassiveTrigger::OnStatusReceived:
		return true;

	default:
		return false;
	}
}

FStatusEffect *USkillEffectManager::FindEffectByID(AActor *Actor, int32 EffectID)
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return nullptr;
	}

	for (FStatusEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectID == EffectID)
		{
			return &Effect;
		}
	}

	return nullptr;
}

UCharacterDataComponent *USkillEffectManager::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<UCharacterDataComponent>();
}

void USkillEffectManager::ResetTurnFlags(AActor *Actor)
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return;
	}

	for (FStatusEffect &Effect : ActiveEffects[Actor])
	{
		Effect.ResetTurnFlags();
	}
}

bool USkillEffectManager::IsSpeedEffect(EStatusType EffectType) const
{
	switch (EffectType)
	{
	case EStatusType::SpeedBuff:
	case EStatusType::SpeedDebuff:
	case EStatusType::AttackSpeedBuff:
	case EStatusType::AttackSpeedDebuff:
		return true;
	default:
		return false;
	}
}

void USkillEffectManager::NotifySpeedChanged(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	UGameInstance *GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>();
	if (TurnManager)
	{
		TurnManager->OnActorSpeedChanged(Actor);
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Notified TurnManager of speed change for %s"),
			   *Actor->GetName());
	}
}

void USkillEffectManager::ApplyImmediateSkillEffect(AActor *Source, AActor *Target, EStatusType StatusType, ESpellElement Element)
{
	if (!Target || StatusType == EStatusType::None)
	{
		return;
	}

	FStatusEffect Effect;
	Effect.Element = Element;
	Effect.EffectID = FMath::Rand();
	Effect.EffectType = StatusType; // Set directly to the generic type

	// Generate element-aware display name
	Effect.EffectName = StatusDisplayNames::GetDisplayName(StatusType, Element);

	// Immediate effects: Weak, longer duration
	switch (StatusType)
	{
	case EStatusType::DOT:
		Effect.EffectValue = 10.0f;
		Effect.RemainingTurns = 3;
		Effect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;
		break;

	case EStatusType::DefenseDebuff:
		Effect.EffectValue = 15.0f; // 15%
		Effect.RemainingTurns = 3;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::SpeedDebuff:
		Effect.EffectValue = 25.0f; // 25%
		Effect.RemainingTurns = 3;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::CritDebuff:
		Effect.EffectValue = 15.0f; // 15%
		Effect.RemainingTurns = 3;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::EnergyDebuff:
		Effect.EffectValue = 25.0f; // 25% locked
		Effect.RemainingTurns = 3;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::RandomDebuff:
	{
		// Pick a random debuff type
		TArray<EStatusType> Debuffs = {
			EStatusType::DamageDebuff,
			EStatusType::DefenseDebuff,
			EStatusType::SpeedDebuff,
			EStatusType::CritDebuff};
		Effect.EffectType = Debuffs[FMath::RandRange(0, Debuffs.Num() - 1)];
		Effect.EffectValue = 15.0f; // 15%
		Effect.RemainingTurns = 3;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
	}
	break;

	case EStatusType::SkipTurn:
	case EStatusType::BurstDamage:
		// These have no immediate effect
		return;

	default:
		return;
	}

	Effect.InitialDuration = Effect.RemainingTurns;
	ApplyEffect(Target, Effect, Source, TEXT("Immediate Status"), -1);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applied immediate %s to %s"),
		   *Effect.EffectName, *Target->GetName());
}

void USkillEffectManager::ApplyTriggeredSkillEffect(AActor *Source, AActor *Target, EStatusType StatusType, ESpellElement Element)
{
	if (!Target || StatusType == EStatusType::None)
	{
		return;
	}

	// Get source data for scaling
	UCharacterData *SourceData = nullptr;
	if (Source)
	{
		UCharacterDataComponent *SourceComp = Source->FindComponentByClass<UCharacterDataComponent>();
		SourceData = SourceComp ? SourceComp->CharacterData : nullptr;
	}

	FStatusEffect Effect;
	Effect.Element = Element;
	Effect.EffectID = FMath::Rand();

	// Generate element-aware display name
	Effect.EffectName = StatusDisplayNames::GetDisplayName(StatusType, Element);

	// Triggered effects: Full power, shorter duration
	switch (StatusType)
	{
	case EStatusType::DOT:
		Effect.EffectType = EStatusType::DOT; // Generic DOT type
		Effect.EffectValue = 30.0f;
		Effect.RemainingTurns = 2;
		Effect.ProcessTiming = EStatusEffectTiming::EndOfOwnTurn;
		break;

	case EStatusType::DefenseDebuff:
		Effect.EffectType = EStatusType::DefenseDebuff;
		Effect.EffectValue = 40.0f; // 40%
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::SkipTurn:
		Effect.EffectType = EStatusType::SkipTurn;
		Effect.EffectValue = 1.0f;
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = EStatusEffectTiming::StartOfOwnTurn;
		break;

	case EStatusType::SpeedDebuff:
		Effect.EffectType = EStatusType::SpeedDebuff;
		Effect.EffectValue = 50.0f; // 50%
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::CritDebuff:
		Effect.EffectType = EStatusType::CritChanceDebuff;
		Effect.EffectValue = 50.0f; // 50%
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::EnergyDebuff:
		Effect.EffectType = EStatusType::EnergyDrain;
		Effect.EffectValue = 100.0f; // 100% locked
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
		break;

	case EStatusType::RandomDebuff:
	{
		TArray<EStatusType> Debuffs = {
			EStatusType::DamageDebuff,
			EStatusType::DefenseDebuff,
			EStatusType::SpeedDebuff,
			EStatusType::CritChanceDebuff};
		Effect.EffectType = Debuffs[FMath::RandRange(0, Debuffs.Num() - 1)];
		Effect.EffectValue = 30.0f; // 30%
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = EStatusEffectTiming::Persistent;
	}
	break;

	case EStatusType::BurstDamage:
		// Apply burst damage scaled by source's RawDamage stat
		if (SourceData)
		{
			int32 BurstDamage = FMath::RoundToInt(SourceData->CalculateRawDamage() * 2.0f);

			UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
			if (TargetComp)
			{
				TargetComp->ServerTakeDamage(BurstDamage);

				UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applied %d raw burst damage to %s"),
					   BurstDamage, *Target->GetName());
			}
		}
		return;

	default:
		return;
	}

	Effect.InitialDuration = Effect.RemainingTurns;
	ApplyEffect(Target, Effect, Source, TEXT("Status Bar Trigger"), -1);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applied triggered %s to %s"),
		   *Effect.EffectName, *Target->GetName());
}