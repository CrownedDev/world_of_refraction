// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skills/Effects/SkillEffectManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Skills/Effects/SkillTriggerUtils.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Effects/SkillEffectDisplayNames.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Engine/GameInstance.h"

// ========================================
// SUBSYSTEM LIFECYCLE
// ========================================

void USkillEffectManager::Initialize(FSubsystemCollectionBase &Collection)
{
	// Force ActionExecutor + DefenseSystem to initialize before this subsystem so the
	// OnDamageDealt / OnDefenseResolved bindings below see fully-constructed delegates.
	Collection.InitializeDependency(UActionExecutor::StaticClass());
	Collection.InitializeDependency(UDefenseSystem::StaticClass());

	Super::Initialize(Collection);

	ActiveEffects.Empty();
	NextInstanceID = 1;

	// Bind OnDamageDealt listener — drives Lifesteal + ApplyBurn/Chill/StunToTarget.
	if (UGameInstance *GI = GetGameInstance())
	{
		if (UActionExecutor *AE = GI->GetSubsystem<UActionExecutor>())
		{
			AE->OnDamageDealt.AddDynamic(this, &USkillEffectManager::OnDamageDealtHandler);
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Bound to ActionExecutor::OnDamageDealt"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillEffectManager] ActionExecutor not available at Initialize — OnDamageDealt binding skipped"));
		}

		// Bind OnDefenseResolved listener — drives defense-outcome conditional effects (C3b).
		if (UDefenseSystem *DS = GI->GetSubsystem<UDefenseSystem>())
		{
			DS->OnDefenseResolved.AddDynamic(this, &USkillEffectManager::OnDefenseResolvedHandler);
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Bound to DefenseSystem::OnDefenseResolved"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillEffectManager] DefenseSystem not available at Initialize — OnDefenseResolved binding skipped"));
		}
	}

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

EEffectApplicationResult USkillEffectManager::ApplyEffect(AActor *Target, FActiveSkillEffect Effect,
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

	// D2: fires-once gate — reject re-application this match BEFORE any stacking/refresh,
	// so a fires-once effect can never stack or refresh either.
	if (Effect.bFiresOncePerMatch && FiredOnceThisMatch.Contains(Effect.EffectID))
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s rejected — already fired once this match (ID=%d)"),
			   *Effect.EffectName, Effect.EffectID);
		return EEffectApplicationResult::Rejected;
	}

	// Set source info
	Effect.SourceActor = Source;
	Effect.SourceAbilityName = SourceAbility;
	Effect.SourceTeamIndex = SourceTeam;
	Effect.InitialDuration = Effect.RemainingTurns;

	// INSTANT lane — a true one-shot. It has already passed the immunity + fires-once gates
	// above (so it stays interceptable; HealBlock enforcement lands here in a later cluster).
	// Run its Server* logic on this LOCAL effect via the type->primitive switch (ApplyEffectLogic
	// only reads fields + calls Server*/subsystems — safe on a non-stored temp), broadcast, and
	// RETURN WITHOUT storing. Placed BEFORE FindEffectByID + Effects.Add: an Instant never enters
	// ActiveEffects, so it never matches the re-apply strength gate — policy-safe by construction.
	// (Contrast ESkillEffectTiming::Immediate, which stores-then-tears-down via bPendingRemoval.)
	// IsInstant() is the single derive point: a zero-duration, non-permanent effect. Permanent
	// gear/auras are excluded via their explicit bPermanent toggle (no longer derived from
	// duration-0, which now means INSTANT cleanly).
	if (Effect.IsInstant())
	{
		ApplyEffectLogic(Target, Effect);
		OnEffectApplied.Broadcast(Target, Effect);
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Instant %s applied to %s (Value: %.1f, not stored)"),
			   *Effect.EffectName, *Target->GetName(), Effect.EffectValue);
		return EEffectApplicationResult::Applied;
	}

	// Get or create effect array for this actor
	TArray<FActiveSkillEffect> &Effects = ActiveEffects.FindOrAdd(Target);

	// Check for existing effect with same ID
	FActiveSkillEffect *ExistingEffect = FindEffectByID(Target, Effect.EffectID);

	if (ExistingEffect)
	{
		// Re-apply strength policy (matched per-Target by EffectID). Strength is the
		// single-application magnitude (Abs, so the one signed type — ModifyStatusResist —
		// compares by how-strong not which-way). STRONGER overwrites the value; duration
		// refreshes on equal-or-stronger, gated by the per-effect bRefreshDurationOnReapply
		// opt-out. WEAKER is ignored entirely. EQUAL refreshes duration only — this is what
		// preserves the maintenance mechanics (Defend's constant 50%, on-hit Burn/Chill at a
		// constant per-attacker magnitude, Stun's 0.0) that sustain via equal re-application.
		const float IncomingStrength = Effect.Strength();
		const float ActiveStrength = ExistingEffect->Strength();
		const bool bStronger = IncomingStrength > ActiveStrength;
		const bool bWeaker = IncomingStrength < ActiveStrength;

		// Stacking: a stackable re-apply still adds a stack (count climbs), and per-stack
		// strength tracks the strongest seen (never falls). A weaker re-apply still stacks
		// but neither lowers the value nor refreshes. At MAX stacks this branch is skipped
		// and the strength gate below runs, so a stronger hit at cap still upgrades the value.
		if (Effect.bCanStack && ExistingEffect->CanAddStack())
		{
			ExistingEffect->CurrentStacks++;

			if (bStronger)
			{
				ExistingEffect->EffectValue = Effect.EffectValue; // track strongest (by Abs)
			}

			if (!bWeaker && Effect.bRefreshDurationOnReapply)
			{
				ExistingEffect->RemainingTurns = Effect.InitialDuration;
			}

			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s: %s stacked to %d on %s"),
				   *Target->GetName(), *Effect.EffectName, ExistingEffect->CurrentStacks, *Target->GetName());

			OnEffectStacksChanged.Broadcast(Target, *ExistingEffect, ExistingEffect->CurrentStacks);

			// Stacking raises the effective (stacked) value -> notify speed consumers.
			if (IsSpeedEffect(Effect.EffectType))
			{
				NotifySpeedChanged(Target);
			}

			return EEffectApplicationResult::StackAdded;
		}

		// Non-stacking (or at max stacks): the strength gate.
		// WEAKER -> ignored entirely: no value change, no duration refresh.
		if (bWeaker)
		{
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s: %s ignored — weaker than active (%.1f < %.1f)"),
				   *Target->GetName(), *Effect.EffectName, IncomingStrength, ActiveStrength);
			return EEffectApplicationResult::Rejected;
		}

		// STRONGER -> overwrite the value (always). EQUAL -> keep the value.
		if (bStronger)
		{
			ExistingEffect->EffectValue = Effect.EffectValue;

			// A changed magnitude on a speed effect must re-notify the turn order.
			if (IsSpeedEffect(Effect.EffectType))
			{
				NotifySpeedChanged(Target);
			}
		}

		// Duration refreshes on equal-or-stronger, gated by the per-effect opt-out.
		if (Effect.bRefreshDurationOnReapply)
		{
			ExistingEffect->RemainingTurns = Effect.InitialDuration;
		}

		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s: %s %s (strength %.1f vs %.1f)"),
			   *Target->GetName(), *Effect.EffectName,
			   bStronger ? TEXT("upgraded + duration refreshed") : TEXT("duration refreshed"),
			   IncomingStrength, ActiveStrength);

		OnEffectDurationChanged.Broadcast(Target, *ExistingEffect, ExistingEffect->RemainingTurns);
		return EEffectApplicationResult::DurationRefreshed;
	}

	// New effect - apply it
	Effects.Add(Effect);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applied %s to %s (ID: %d, Duration: %d, Value: %.1f)"),
		   *Effect.EffectName, *Target->GetName(), Effect.EffectID, Effect.RemainingTurns, Effect.EffectValue);

	// Fire-on-application (Option A: new effects only). Re-applications return before this point
	// (stronger/equal/weaker/stack all exit in the ExistingEffect branch above), so a rejected
	// weaker re-app can never reach here — Option A safety by construction. Runs ApplyEffectLogic
	// once on the STORED copy (Effects.Last()) so bProcessedThisTurn persists, then leaves the
	// effect stored to keep ticking on its windows. Excludes Immediate (fires + tears down just
	// below) and bDelayFirstExecution opt-outs; Instant already returned at the IsInstant() lane.
	if (!Effects.Last().bDelayFirstExecution &&
		Effects.Last().ProcessTiming != ESkillEffectTiming::Immediate)
	{
		ApplyEffectLogic(Target, Effects.Last());
		Effects.Last().bProcessedThisTurn = true; // same-turn guard: skip a same-turn window re-fire
	}

	// Process immediate effects right away
	if (Effect.ProcessTiming == ESkillEffectTiming::Immediate)
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

	// D2: record fires-once on the NEW-apply path ONLY. A fires-once effect is rejected at
	// the gate above before it can reach the stack/refresh branches, so this lands exactly once.
	if (Effect.bFiresOncePerMatch)
	{
		FiredOnceThisMatch.Add(Effect.EffectID);
	}

	return EEffectApplicationResult::Applied;
}

void USkillEffectManager::ApplyEffects(AActor *Target, const TArray<FActiveSkillEffect> &Effects,
									   AActor *Source, const FString &SourceAbility, int32 SourceTeam)
{
	for (const FActiveSkillEffect &Effect : Effects)
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
	FActiveSkillEffect InfusionEffect = FActiveSkillEffect::CreateFromInfusion(
		AbilityName,
		AbilityID,
		InfusedElement,
		DOTDamage,
		Duration);

	ApplyEffect(Target, InfusionEffect, Source, AbilityName + TEXT(" (Infused)"), SourceTeam);
}

// ========================================
// EQUIPMENT EFFECT APPLICATION
// ========================================

void USkillEffectManager::ApplyEquipmentEffects(
	AActor *Target,
	const TArray<FGatheredEffect> &Effects)
{
	if (!Target)
	{
		return;
	}

	for (const FGatheredEffect &G : Effects)
	{
		const FSkillEffect &Source = G.Effect;
		if (!Source.IsValid())
		{
			continue;
		}

		// Def-identity packing: feeding G.DefID as the window key and G.BundleIndex as the
		// effect index yields PackEffectID(DefID, BundleIndex, payload) — so the SAME
		// definition referenced by any source produces the SAME EffectID and MERGES via
		// ApplyEffect's FindEffectByID path. One authored effect still yields N runtime
		// effects (one per payload).
		TArray<FActiveSkillEffect> Runtimes = FActiveSkillEffect::CreateAllFromSkillEffect(
			Source.EffectName, G.DefID, Source, G.BundleIndex);

		for (FActiveSkillEffect &Runtime : Runtimes)
		{
			ApplyEffect(Target, Runtime, nullptr, Source.EffectName, -1);
		}
	}
}

void USkillEffectManager::WOR_StartingEffects()
{
	UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
	if (!TurnMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WOR_StartingEffects] No TurnManager subsystem."));
		return;
	}

	AActor *Actor = TurnMgr->GetCurrentActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WOR_StartingEffects] No active character (combat not active?)."));
		return;
	}

	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!LC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WOR_StartingEffects] No LoadoutComponent on %s."), *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("=== STARTING EFFECTS — %s ==="), *Actor->GetName());

	auto PrintSpecs = [](const TCHAR *SourceLabel, const TArray<FSkillEffect> &Specs) -> int32
	{
		for (const FSkillEffect &E : Specs)
		{
			UE_LOG(LogTemp, Display, TEXT("  PRE [%s] %s — %d payload(s), %d condition(s)"),
				   SourceLabel,
				   E.EffectName.IsEmpty() ? TEXT("(unnamed)") : *E.EffectName,
				   E.Payloads.Num(), E.Conditions.Num());
		}
		return Specs.Num();
	};

	// PRE — per gear source, mirroring GetActiveEffects' per-class coverage.
	// Debug-only re-enumeration; the TOTAL cross-check below self-reports drift.
	int32 Labeled = 0;
	const FCombatLoadout Loadout = LC->GetActiveLoadout();
	switch (LC->CharacterClass)
	{
	case ECharacterClass::Generic:
		if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
		{
			if (UWeaponData *Weapon = LC->GetActiveWeapon())
			{
				Labeled += PrintSpecs(TEXT("ActiveWeapon"), Weapon->GetStartingEffects());
			}
		}
		else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
		{
			if (URingData *Ring = LC->GetPrimaryRing())
			{
				Labeled += PrintSpecs(TEXT("PrimaryRing"), Ring->GetStartingEffects());
			}
			if (UWeaponData *Weapon = LC->GetActiveWeapon())
			{
				Labeled += PrintSpecs(TEXT("SecondaryWeapon"), Weapon->GetStartingEffects());
			}
		}
		break;

	case ECharacterClass::Resonator:
		if (URingData *Ring = LC->GetActiveRing())
		{
			Labeled += PrintSpecs(TEXT("ActiveRing"), Ring->GetStartingEffects());
		}
		if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
		{
			if (UWeaponData *Weapon = LC->GetPrimaryWeapon())
			{
				Labeled += PrintSpecs(TEXT("PrimaryWeapon"), Weapon->GetStartingEffects());
			}
		}
		break;

	case ECharacterClass::Caster:
	default:
		if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
		{
			if (UWeaponData *Weapon = LC->GetPrimaryWeapon())
			{
				Labeled += PrintSpecs(TEXT("PrimaryWeapon"), Weapon->GetStartingEffects());
			}
		}
		else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
		{
			if (URingData *Ring = LC->GetPrimaryRing())
			{
				Labeled += PrintSpecs(TEXT("PrimaryRing"), Ring->GetStartingEffects());
			}
		}
		break;
	}
	if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
	{
		Labeled += PrintSpecs(TEXT("InnateEvolution"), Loadout.PrimaryEvolution.Item->GetStartingEffects());
	}

	const int32 Total = LC->GetActiveEffects(Actor).Num();
	UE_LOG(LogTemp, Display, TEXT("  PRE total: %d labeled vs %d from GetActiveEffects%s"),
		   Labeled, Total,
		   Labeled == Total ? TEXT(" — MATCH") : TEXT(" — MISMATCH (coverage drift; fix this tool)"));

	// POST — what actually got applied (any window, post def-identity packing; effects now
	// window per-DEFINITION, so there is no single actor window to scan).
	const TArray<FActiveSkillEffect> Active = GetActiveEffects(Actor);
	for (const FActiveSkillEffect &E : Active)
	{
		UE_LOG(LogTemp, Display, TEXT("  POST %s — %s (ID=%d)"), *E.EffectName,
			   *UEnum::GetValueAsString(E.EffectType), E.EffectID);
	}
	UE_LOG(LogTemp, Display, TEXT("  POST applied: %d"), Active.Num());
	UE_LOG(LogTemp, Display, TEXT("======================"));
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
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateFromPhysicalDamageType(
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
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateFromWeaponInfusion(
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

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].EffectID == EffectID)
		{
			FActiveSkillEffect RemovedEffect = Effects[i];
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
	bool bSpeedChanged = false;
	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].EffectName == EffectName)
		{
			FActiveSkillEffect RemovedEffect = Effects[i];

			if (IsSpeedEffect(RemovedEffect.EffectType))
			{
				bSpeedChanged = true;
			}

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

	// Notify TurnManager if any speed effects were removed
	if (bSpeedChanged)
	{
		NotifySpeedChanged(Target);
	}

	return RemovedCount;
}

int32 USkillEffectManager::RemoveEffectsByType(AActor *Target, ESkillEffectType EffectType)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	bool bWasSpeedEffect = IsSpeedEffect(EffectType);
	int32 RemovedCount = 0;
	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].EffectType == EffectType)
		{
			FActiveSkillEffect RemovedEffect = Effects[i];
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
	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].IsBuff())
		{
			FActiveSkillEffect RemovedEffect = Effects[i];

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
	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].IsDebuff())
		{
			FActiveSkillEffect RemovedEffect = Effects[i];

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
	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		if (Effects[i].IsDOT())
		{
			FActiveSkillEffect RemovedEffect = Effects[i];
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

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];
	bool bSpeedChanged = false;

	// Check for speed effects and broadcast removal for each effect
	for (const FActiveSkillEffect &Effect : Effects)
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

void USkillEffectManager::ResetForNewCombat()
{
	FiredOnceThisMatch.Empty();
	ArmedConditionals.Empty();
	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] FiredOnceThisMatch + ArmedConditionals reset for new combat"));
}

void USkillEffectManager::ArmConditionalEffects(AActor *Actor, const TArray<FGatheredEffect> &Conditionals)
{
	if (!Actor)
	{
		return;
	}
	ArmedConditionals.Add(Actor, Conditionals);
	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Armed %d conditional effect(s) on %s"),
		   Conditionals.Num(), *Actor->GetName());
}

void USkillEffectManager::ClearAllEffects()
{
	for (auto &Pair : ActiveEffects)
	{
		if (Pair.Key.IsValid())
		{
			for (const FActiveSkillEffect &Effect : Pair.Value)
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

		TArray<FActiveSkillEffect> &Effects = Pair.Value;
		bool bSpeedChangedForActor = false;

		for (int32 i = Effects.Num() - 1; i >= 0; --i)
		{
			if (Effects[i].SourceActor == Source)
			{
				FActiveSkillEffect RemovedEffect = Effects[i];

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
	ProcessEffectsWithTiming(Actor, ESkillEffectTiming::StartOfOwnTurn);

	// Process persistent effects (always active, but we re-apply stat mods)
	ProcessEffectsWithTiming(Actor, ESkillEffectTiming::Persistent);

	// Check conditional triggers with current state
	ProcessTriggerEffects(Actor, ESkillTrigger::OnTurnStart);
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
	ProcessEffectsWithTiming(Actor, ESkillEffectTiming::EndOfOwnTurn);

	// Check conditional triggers
	ProcessTriggerEffects(Actor, ESkillTrigger::OnTurnEnd);

	// Tick durations and remove expired effects
	TickDurations(Actor);

	// Clean up effects marked for removal
	if (ActiveEffects.Contains(Actor))
	{
		TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

		for (int32 i = Effects.Num() - 1; i >= 0; --i)
		{
			if (Effects[i].bPendingRemoval)
			{
				FActiveSkillEffect RemovedEffect = Effects[i];
				Effects.RemoveAt(i);
				OnEffectRemoved.Broadcast(Actor, RemovedEffect);
			}
		}
	}
}

void USkillEffectManager::ProcessTriggerEffects(AActor *Actor, ESkillTrigger Trigger, float TriggerValue)
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return;
	}

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

	for (FActiveSkillEffect &Effect : Effects)
	{
		if (Effect.ProcessTiming == ESkillEffectTiming::OnTrigger &&
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

void USkillEffectManager::ProcessEffectsWithTiming(AActor *Actor, ESkillEffectTiming Timing)
{
	if (!ActiveEffects.Contains(Actor))
	{
		return;
	}

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

	for (FActiveSkillEffect &Effect : Effects)
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

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];
	bool bSpeedChanged = false;

	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		FActiveSkillEffect &Effect = Effects[i];

		// Skip permanent effects
		if (Effect.bPermanent)
		{
			continue;
		}

		// Skip immediate effects (already handled)
		if (Effect.ProcessTiming == ESkillEffectTiming::Immediate)
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

			FActiveSkillEffect ExpiredEffect = Effect;
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

void USkillEffectManager::ApplyEffectLogic(AActor *Actor, FActiveSkillEffect &Effect)
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
	case ESkillEffectType::DOT:
	{
		// DOTs can kill — design changed (1-HP clamp removed). Full tick damage
		// applies; CurrentHP can reach 0 and route through the normal death path
		// (ServerTakeDamage -> CheckDeath -> OnDied), identical to a killing blow.
		int32 DamageToApply = FMath::RoundToInt(Value);

		if (DamageToApply > 0)
		{
			CharComp->ServerTakeDamage(DamageToApply);
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s took %d DOT damage from %s (can be lethal)"),
				   *Actor->GetName(), DamageToApply, *Effect.EffectName);
		}

		// Per-tick status buildup — mirrors the per-event amount the effect's
		// source applied with its immediate hit. BuildupPerTick is 0 for DOTs
		// that don't build status (weapon Bleed, ability infusions).
		if (Effect.BuildupPerTick > 0.0f)
		{
			if (UGameInstance *GI = GetGameInstance())
			{
				if (UStatusBuildupManager *SBM = GI->GetSubsystem<UStatusBuildupManager>())
				{
					AActor *BuildupSource = Effect.SourceActor.IsValid() ? Effect.SourceActor.Get() : Actor;
					const int32 BuildupAmount = FMath::RoundToInt(Effect.BuildupPerTick);
					SBM->AddStatusBuildup(BuildupSource, Actor, BuildupAmount,
										  Effect.Element, EPhysicalDamageType::None);
					UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s gained %d %s buildup from %s tick"),
						   *Actor->GetName(), BuildupAmount,
						   *UEnum::GetValueAsString(Effect.Element), *Effect.EffectName);
				}
			}
		}
		break;
	}

		// ==================== HEALING ====================
	case ESkillEffectType::HealthRestore:
		CharComp->ServerHeal(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s healed %d from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	// ==================== ENERGY ====================
	case ESkillEffectType::EnergyRestore:
		CharComp->ServerGainEnergy(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s gained %d energy from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	case ESkillEffectType::EnergyDrain:
		CharComp->ServerSpendEnergy(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s lost %d energy from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	// ==================== STAT MODIFIERS ====================
	// These don't apply immediate changes - they're queried via GetTotalStatModifier
	case ESkillEffectType::DamageBuff:
	case ESkillEffectType::DamageDebuff:
	case ESkillEffectType::DefenseBuff:
	case ESkillEffectType::DefenseDebuff:
	case ESkillEffectType::SpeedBuff:
	case ESkillEffectType::SpeedDebuff:
	case ESkillEffectType::MindBuff:
	case ESkillEffectType::MindDebuff:
	case ESkillEffectType::BodyBuff:
	case ESkillEffectType::BodyDebuff:
	case ESkillEffectType::SpiritBuff:
	case ESkillEffectType::SpiritDebuff:
	case ESkillEffectType::CritChanceBuff:
	case ESkillEffectType::CritChanceDebuff:
	case ESkillEffectType::SpellSpeedBuff:
	case ESkillEffectType::SpellSpeedDebuff:
	case ESkillEffectType::ActionSpeedBuff:
	case ESkillEffectType::ActionSpeedDebuff:
	case ESkillEffectType::RawDamageBuff:
	case ESkillEffectType::RawDamageDebuff:
	case ESkillEffectType::SpellDamageBuff:
	case ESkillEffectType::SpellDamageDebuff:
	case ESkillEffectType::StatusMultiplierBuff:
	case ESkillEffectType::StatusMultiplierDebuff:
	case ESkillEffectType::EfficiencyBuff:
	case ESkillEffectType::EfficiencyDebuff:
	case ESkillEffectType::SpellCostBuff:
	case ESkillEffectType::SpellCostDebuff:
	case ESkillEffectType::ResistanceBuff:
	case ESkillEffectType::ResistanceDebuff:
	case ESkillEffectType::SpellSizeBuff:
	case ESkillEffectType::SpellSizeDebuff:
	case ESkillEffectType::MaxEnergyBuff:
	case ESkillEffectType::MaxEnergyDebuff:
	case ESkillEffectType::MaxHPBuff:
	case ESkillEffectType::MaxHPDebuff:
	case ESkillEffectType::TurnSpeedBuff:
	case ESkillEffectType::TurnSpeedDebuff:
	case ESkillEffectType::LuckBuff:
	case ESkillEffectType::LuckDebuff:
	case ESkillEffectType::ReflexBuff:
	case ESkillEffectType::ReflexDebuff:
	case ESkillEffectType::CritDamageDebuff:
		// Stat modifiers are passive - other systems query GetTotalStatModifier
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Stat modifier %s active on %s (%.1f%%)"),
			   *Effect.EffectName, *Actor->GetName(), Value);
		break;

	case ESkillEffectType::RemoveSpeedDebuff:
		RemoveEffectsByType(Actor, ESkillEffectType::SpeedDebuff);
		break;

	case ESkillEffectType::RemoveDamageDebuff:
		RemoveEffectsByType(Actor, ESkillEffectType::DamageDebuff);
		break;

	case ESkillEffectType::RemoveDefenseDebuff:
		RemoveEffectsByType(Actor, ESkillEffectType::DefenseDebuff);
		break;

	// ==================== SELF DAMAGE ====================
	// Wired Session X. Lunge's self-damage effect applied cleanly but had no
	// handler here, so it sat on the actor without ticking (May 2026 PIE).
	case ESkillEffectType::SelfDamage:
		CharComp->ServerTakeDamage(FMath::RoundToInt(Value));
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s took %d self-damage from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	// ==================== BAR-CAP GATE EFFECTS ====================
	// Presence on the actor is what gates actions; ApplyEffectLogic is a no-op.
	// Gate enforcement:
	//   Stun      -> ActionExecutor::ValidateAction blocks non-Attack/Defend
	//   HealBlock -> IsImmuneToEffectType rejects HealthRestore/RestoreHPPercent at the
	//                ApplyEffect gate (heals route through ApplyEffect) — presence here is
	//                what that query checks for
	//   Silenced  -> EP-cost gates check for active Silenced effect
	case ESkillEffectType::Stun:
	case ESkillEffectType::HealBlock:
	case ESkillEffectType::Silenced:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] %s active on %s (gate effect)"),
			   *Effect.EffectName, *Actor->GetName());
		break;

	// RandomSkill: full implementation pending Session Y/Z (LoadoutComponent
	// random pick + ActionExecutor forced-action injection).
	case ESkillEffectType::RandomSkill:
		UE_LOG(LogTemp, Warning, TEXT("[SkillEffectManager] %s has RandomSkill active - implementation pending"),
			   *Actor->GetName());
		break;

	// ==================== PASSIVE LAYER (Phase 2) ====================
	// Stub cases for the new passive-layer effect types. Each needs its own
	// runtime handler — added in upcoming phases.

	// Stat modifiers — passive-layer percent. Like the existing stat modifiers,
	// these are passive and queried by other systems; no per-tick logic here.
	case ESkillEffectType::ModifyDamageDealt:
	case ESkillEffectType::ReduceDamageTaken:
	case ESkillEffectType::IncreaseDamageTaken:
	case ESkillEffectType::ModifyHealing:
	case ESkillEffectType::ModifyCritChance:
	case ESkillEffectType::ModifyCritDamage:
	case ESkillEffectType::ModifyEnergyCost:
	case ESkillEffectType::ModifyTurnSpeed:
	case ESkillEffectType::ModifyStatusResist:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Passive stat modifier %s active on %s (%.1f%%)"),
			   *Effect.EffectName, *Actor->GetName(), Value);
		break;

	// Resource changes — percent restore / drain. Value is the percent (0..100).
	case ESkillEffectType::RestoreHPPercent:
	{
		if (CharComp->MaxHP > 0)
		{
			const int32 Amount = FMath::RoundToInt(CharComp->MaxHP * Value / CombatConstants::STAT_PERCENT_DIVISOR);
			if (Amount > 0)
			{
				CharComp->ServerHeal(Amount);
				UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s restored %d HP (%.1f%% of MaxHP) from %s"),
					   *Actor->GetName(), Amount, Value, *Effect.EffectName);
			}
		}
		break;
	}
	case ESkillEffectType::RestoreEnergyPercent:
	{
		if (CharComp->MaxEP > 0)
		{
			const int32 Amount = FMath::RoundToInt(CharComp->MaxEP * Value / CombatConstants::STAT_PERCENT_DIVISOR);
			if (Amount > 0)
			{
				CharComp->ServerGainEnergy(Amount);
				UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s restored %d EP (%.1f%% of MaxEP) from %s"),
					   *Actor->GetName(), Amount, Value, *Effect.EffectName);
			}
		}
		break;
	}
	// DrainHP — distinct from DOT: flat damage, no "leave 1 HP" clamp (can be lethal).
	case ESkillEffectType::DrainHP:
	{
		const int32 Amount = FMath::RoundToInt(Value);
		if (Amount > 0)
		{
			CharComp->ServerTakeDamage(Amount);
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s drained %d HP from %s (lethal)"),
				   *Actor->GetName(), Amount, *Effect.EffectName);
		}
		break;
	}
	// DrainEnergy — distinct from the bar-cap EnergyDrain trigger above.
	case ESkillEffectType::DrainEnergy:
	{
		const int32 Amount = FMath::RoundToInt(Value);
		if (Amount > 0)
		{
			CharComp->ServerSpendEnergy(Amount);
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s drained %d EP from %s"),
				   *Actor->GetName(), Amount, *Effect.EffectName);
		}
		break;
	}

	// Defensive — passive shields and damage redirection. No per-tick logic;
	// hooked into the damage pipeline.
	case ESkillEffectType::DamageReflect:
	case ESkillEffectType::ReflectPhysicalDamage:
	case ESkillEffectType::ReflectSpellDamage:
	case ESkillEffectType::Lifesteal:
	case ESkillEffectType::AbsorbDamage:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Passive defensive %s active on %s"),
			   *Effect.EffectName, *Actor->GetName());
		break;

	// Immunities — passive markers. The gate lives in
	// UStatusBuildupManager::AddStatusBuildup, which queries HasEffectOfType on
	// the target before applying buildup. ApplyEffectLogic is a no-op here.
	case ESkillEffectType::GrantDOTImmunity:
	case ESkillEffectType::GrantStunImmunity:
	case ESkillEffectType::GrantSilenceImmunity:
	case ESkillEffectType::GrantAllStatusImmunity:
	case ESkillEffectType::GrantFireImmunity:
	case ESkillEffectType::GrantWaterImmunity:
	case ESkillEffectType::GrantEarthImmunity:
	case ESkillEffectType::GrantWindImmunity:
	case ESkillEffectType::GrantLightImmunity:
	case ESkillEffectType::GrantDarknessImmunity:
	case ESkillEffectType::GrantLightningImmunity:
	case ESkillEffectType::GrantVoidImmunity:
	case ESkillEffectType::GrantRealityImmunity:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Immunity %s active on %s"),
			   *Effect.EffectName, *Actor->GetName());
		break;

	// Status application on trigger — passive markers consumed by ActionExecutor's
	// OnHit / OnCrit broadcast (Phase B, pending missing-hooks pass). When the
	// holder hits a target, ActionExecutor queries HasEffectOfType for these
	// types and calls ApplyTriggeredSkillEffect on the victim with the matching
	// status (DOT for Burn / Chill — by Element, Stun for Stun) using Effect.Element.
	case ESkillEffectType::ApplyBurnToTarget:
	case ESkillEffectType::ApplyChillToTarget:
	case ESkillEffectType::ApplyStunToTarget:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] %s armed on %s — fires on hit (Phase B hook pending)"),
			   *Effect.EffectName, *Actor->GetName());
		break;

	case ESkillEffectType::CleanseSelf:
	{
		const int32 Removed = RemoveAllDebuffs(Actor);
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] %s cleansed %d debuff(s) via %s"),
			   *Actor->GetName(), Removed, *Effect.EffectName);
		break;
	}
	case ESkillEffectType::CleanseAllies:
	{
		UTurnManager *TM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
		if (TM)
		{
			const int32 TeamIndex = TM->GetActorTeam(Actor);
			const TArray<AActor *> Allies = TM->GetTeamMembers(TeamIndex);
			int32 CleansedCount = 0;
			for (AActor *Ally : Allies)
			{
				if (Ally && Ally != Actor)
				{
					RemoveAllDebuffs(Ally);
					++CleansedCount;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] CleanseAllies on %s cleansed %d ally(ies)"),
				   *Actor->GetName(), CleansedCount);
		}
		break;
	}

	// Special mechanics — bespoke handlers in different subsystems.
	case ESkillEffectType::ExtraAction:
	{
		UTurnManager *TM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
		if (TM)
		{
			TM->RequestExtraTurn(Actor);
		}
		break;
	}
	case ESkillEffectType::GuaranteedCrit:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] GuaranteedCrit active on %s"), *Actor->GetName());
		break;
	case ESkillEffectType::IgnoreDefense:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] IgnoreDefense active on %s"), *Actor->GetName());
		break;
	case ESkillEffectType::DoubleHit:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] DoubleHit active on %s"), *Actor->GetName());
		break;
	case ESkillEffectType::Revive:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Revive armed on %s"), *Actor->GetName());
		break;

	// ==================== STATUS BAR MANIPULATION (sweep-4) ====================
	// Instant gauge manipulators. Both fire once on apply (Duration is an
	// authoring field but doesn't drive persistence — Value is consumed up-front).
	// Element comes from Effect.Element which ApplySkillEffects sets to the
	// resolved cast element ONLY for these effect types (other effect types stay
	// Generic to preserve historical behaviour — see ActionExecutor.cpp).
	case ESkillEffectType::StatusIncrease:
	{
		if (Value > 0.0f)
		{
			if (UGameInstance *GI = GetGameInstance())
			{
				if (UStatusBuildupManager *SBM = GI->GetSubsystem<UStatusBuildupManager>())
				{
					AActor *BuildupSource = Effect.SourceActor.IsValid() ? Effect.SourceActor.Get() : Actor;
					SBM->AddStatusBuildup(BuildupSource, Actor, Value,
										  Effect.Element, EPhysicalDamageType::None);
					UE_LOG(LogTemp, Log,
						   TEXT("[SkillEffectManager] %s built %.1f %s status on %s via %s"),
						   *(BuildupSource ? BuildupSource->GetName() : FString(TEXT("?"))),
						   Value, *UEnum::GetValueAsString(Effect.Element),
						   *Actor->GetName(), *Effect.EffectName);
				}
			}
		}
		break;
	}
	case ESkillEffectType::StatusDecrease:
	{
		if (Value > 0.0f)
		{
			if (UGameInstance *GI = GetGameInstance())
			{
				if (UStatusBuildupManager *SBM = GI->GetSubsystem<UStatusBuildupManager>())
				{
					SBM->ReduceStatusBuildupByAmount(Actor, Value);
					UE_LOG(LogTemp, Log,
						   TEXT("[SkillEffectManager] %s status buildup reduced by %.1f via %s"),
						   *Actor->GetName(), Value, *Effect.EffectName);
				}
			}
		}
		break;
	}

	default:
		UE_LOG(LogTemp, Verbose, TEXT("[SkillEffectManager] Unhandled effect type for %s"),
			   *Effect.EffectName);
		break;
	}
}

// ========================================
// EVENT HANDLERS
// ========================================

void USkillEffectManager::OnDamageDealtHandler(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical)
{
	if (!Attacker || !Target || Damage <= 0)
	{
		return;
	}

	// Lifesteal — heal the attacker for a percent of damage dealt.
	if (HasEffectOfType(Attacker, ESkillEffectType::Lifesteal))
	{
		const float LifestealPct = GetTotalStatModifier(Attacker, ESkillEffectType::Lifesteal) / CombatConstants::STAT_PERCENT_DIVISOR;
		const int32 HealAmount = FMath::Max(1, FMath::RoundToInt(Damage * LifestealPct));
		if (UCharacterDataComponent *Comp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			Comp->ServerHeal(HealAmount);
			UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Lifesteal: %s healed %d from hit on %s"),
				   *Attacker->GetName(), HealAmount, *Target->GetName());
		}
	}

	// ApplyBurnToTarget — DOT (Fire) on the victim. Damage-per-turn = stored
	// magnitude on the effect, default 10 if unset; duration 3 turns.
	if (HasEffectOfType(Attacker, ESkillEffectType::ApplyBurnToTarget))
	{
		const float Magnitude = GetTotalStatModifier(Attacker, ESkillEffectType::ApplyBurnToTarget);
		const float Damage_ApplyBurn = Magnitude > 0 ? Magnitude : 10.0f;
		const int32 BurnID = GetTypeHash(Attacker) ^ static_cast<uint32>(ESkillEffectType::ApplyBurnToTarget);
		FActiveSkillEffect Burn = FActiveSkillEffect::CreateDOT(
			TEXT("Burn"), BurnID, Damage_ApplyBurn, 3, ESpellElement::Fire);
		ApplyEffect(Target, Burn, Attacker, TEXT("Burn"), -1);
	}

	// ApplyChillToTarget — DOT (Water) on the victim.
	if (HasEffectOfType(Attacker, ESkillEffectType::ApplyChillToTarget))
	{
		const float Magnitude = GetTotalStatModifier(Attacker, ESkillEffectType::ApplyChillToTarget);
		const float Damage_ApplyChill = Magnitude > 0 ? Magnitude : 10.0f;
		const int32 ChillID = GetTypeHash(Attacker) ^ static_cast<uint32>(ESkillEffectType::ApplyChillToTarget);
		FActiveSkillEffect Chill = FActiveSkillEffect::CreateDOT(
			TEXT("Chill"), ChillID, Damage_ApplyChill, 3, ESpellElement::Water);
		ApplyEffect(Target, Chill, Attacker, TEXT("Chill"), -1);
	}

	// ApplyStunToTarget — bar-cap-style Stun on a CRIT only. Built via CreateBuff
	// (gives 1-turn duration with no per-tick value); ApplyEffectLogic's Stun
	// case at :1056 is a no-op marker — gate enforcement lives at ValidateAction.
	if (bCritical && HasEffectOfType(Attacker, ESkillEffectType::ApplyStunToTarget))
	{
		const int32 StunID = GetTypeHash(Attacker) ^ static_cast<uint32>(ESkillEffectType::ApplyStunToTarget);
		FActiveSkillEffect Stun = FActiveSkillEffect::CreateBuff(
			TEXT("Stun"), StunID, ESkillEffectType::Stun, 0.0f, 1);
		ApplyEffect(Target, Stun, Attacker, TEXT("Stun"), -1);
		UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] ApplyStunToTarget fired on crit: %s stunned %s"),
			   *Attacker->GetName(), *Target->GetName());
	}
}

void USkillEffectManager::OnDefenseResolvedHandler(AActor *Defender, AActor *Attacker,
												   EDefenseType DefenseType, bool bPerfect, int32 ImpactIndex)
{
	if (!Defender || !Attacker)
	{
		return;
	}

	// The outcome maps to its trigger set (perfect outcomes fire both perfect + base).
	TArray<ESkillTrigger> OutcomeTriggers;
	SkillTriggerUtils::DefenseOutcomeToTriggers(DefenseType, bPerfect, OutcomeTriggers);

	// Reuse ActionExecutor's per-payload target resolver (Decision B); TurnManager supplies
	// the owner's team for Enemy/Ally resolution. Fetched once for both perspectives.
	UActionExecutor *AE = GetGameInstance() ? GetGameInstance()->GetSubsystem<UActionExecutor>() : nullptr;
	UTurnManager *TM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;

	// Evaluate from BOTH perspectives: the defender ("I parried") and the attacker ("my target
	// dodged me"). For each pass, Owner is that actor's armed set, Target is the OTHER actor.
	struct FPerspective { AActor *Owner; AActor *Target; };
	const FPerspective Perspectives[] = { { Defender, Attacker }, { Attacker, Defender } };

	for (const FPerspective &P : Perspectives)
	{
		AActor *Owner = P.Owner;
		AActor *Target = P.Target;

		const TArray<FGatheredEffect> *Armed = ArmedConditionals.Find(Owner);
		if (!Armed)
		{
			continue;
		}

		for (const FGatheredEffect &G : *Armed)
		{
			const FSkillEffect &E = G.Effect;

			// DEFENSE-TRIGGER GUARD (Decision A): only impact-driven effects fire here. A
			// pure-threshold conditional (no defense-outcome trigger in its group) is skipped
			// entirely, so it never leaks into the impact path.
			if (!E.Conditions.ContainsByPredicate(
					[](const FSkillCondition &C) { return SkillTriggerUtils::IsDefenseOutcomeTrigger(C.Trigger); }))
			{
				continue;
			}

			const bool bFires = EvaluateConditionGroup(
				E.Conditions,
				[&](const FSkillCondition &C) { return IsOwnerSide(C.Subject) || Target != nullptr; }, // Participates
				[&](const FSkillCondition &C) {													  // IsMet — dispatch
					if (SkillTriggerUtils::IsDefenseOutcomeTrigger(C.Trigger))
					{
						// Outcome perspective: the subject's actor(s) must include the actor who
						// actually defended (the Defender), so the outcome resolves per owner.
						return OutcomeTriggers.Contains(C.Trigger)
							   && ResolveSubjectActors(C.Subject, Owner, Target).Contains(Defender);
					}
					return ResolveSubjectActors(C.Subject, Owner, Target)
						.ContainsByPredicate([&](AActor *A) { return IsSingleTriggerMet(A, C.Trigger, C.Threshold); });
				});

			if (bFires && AE)
			{
				const int32 OwnerTeam = TM ? TM->GetActorTeam(Owner) : -1;

				// Per-PAYLOAD resolve-and-apply. Each payload carries its own Target/TargetCount,
				// so targets resolve per payload (a parry effect may buff Self AND debuff the
				// attacker). Build ONE runtime per payload via CreateFromSpellEffect — NOT
				// CreateAllFromSkillEffect, which expands ALL payloads and would N^2-apply inside
				// this loop. Mirrors ActionExecutor::ApplySkillEffects's per-payload path.
				for (int32 PayloadIndex = 0; PayloadIndex < E.Payloads.Num(); ++PayloadIndex)
				{
					const FSkillEffectPayload &Payload = E.Payloads[PayloadIndex];
					if (Payload.EffectType == ESkillEffectType::None)
					{
						continue;
					}

					// Defender-perspective ActionTargets = { the OTHER actor }, so a Single-Enemy
					// payload routes to whoever this Owner was fighting in the exchange.
					TArray<AActor *> AppTargets;
					AE->GetEffectTargets(Owner, { Target }, Payload.Target, Payload.TargetCount, OwnerTeam, AppTargets);
					if (AppTargets.Num() == 0)
					{
						continue;
					}

					// Def-identity: same packing as cast / equipment
					// (PackEffectID(DefID, BundleIndex, PayloadIndex)) — a defender-fired effect
					// MERGES with the same def applied elsewhere and shares the fires-once gate.
					const int32 PayloadEffectID =
						EffectIdentity::PackEffectID(G.DefID, G.BundleIndex, PayloadIndex);

					// Gauge manipulators + DOT pass authored Value through; stat modifiers keep
					// the Magnitude*100 percentage shape (mirrors ApplySkillEffects).
					const int32 RuntimeValue =
						(Payload.EffectType == ESkillEffectType::StatusIncrease ||
						 Payload.EffectType == ESkillEffectType::StatusDecrease ||
						 Payload.EffectType == ESkillEffectType::DOT)
							? Payload.Value
							: FMath::RoundToInt(Payload.Magnitude * 100.0f);

					for (AActor *AppTarget : AppTargets)
					{
						// No cast context here, so Element stays None / non-elemental (same intent as
						// the equipment-effect path, CreateAllFromSkillEffect — pending its own migration).
						FActiveSkillEffect Runtime = FActiveSkillEffect::CreateFromSpellEffect(
							E.EffectName, PayloadEffectID, Payload.EffectType, Payload.Magnitude, RuntimeValue,
							Payload.Duration, ESpellElement::None);

						// Authored stacking / fires-once carry over. The defense-outcome conditions
						// were the GATE (already evaluated) — deliberately NOT copied onto the
						// runtime, so the consequence ticks on its natural timing (DOT end-of-turn,
						// etc.) instead of being promoted to an inert OnTrigger effect.
						Runtime.bCanStack = E.bStackable;
						Runtime.MaxStacks = E.MaxStacks;
						Runtime.bFiresOncePerMatch = E.bFiresOncePerMatch;

						ApplyEffect(AppTarget, Runtime, Owner, E.EffectName, OwnerTeam);

						UE_LOG(LogTemp, Log, TEXT("[DefenseTrigger] FIRE: %s (ID=%d) %s -> %s outcome=%s perfect=%d"),
							   *E.EffectName, PayloadEffectID, *GetNameSafe(Owner),
							   *GetNameSafe(AppTarget), *UEnum::GetValueAsString(DefenseType), bPerfect);
					}
				}
			}
		}
	}
}

// ========================================
// QUERIES
// ========================================

TArray<FActiveSkillEffect> USkillEffectManager::GetActiveEffects(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return TArray<FActiveSkillEffect>();
	}

	return ActiveEffects[Actor];
}

TArray<FActiveSkillEffect> USkillEffectManager::GetEffectsByType(AActor *Actor, ESkillEffectType EffectType) const
{
	TArray<FActiveSkillEffect> Result;

	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return Result;
	}

	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
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

	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectID == EffectID)
		{
			return true;
		}
	}

	return false;
}

bool USkillEffectManager::HasEffectOfType(AActor *Actor, ESkillEffectType EffectType) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}

	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.EffectType == EffectType)
		{
			return true;
		}
	}

	return false;
}

float USkillEffectManager::GetTotalStatModifier(AActor *Actor, ESkillEffectType ModifierType) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return 0.0f;
	}

	float Total = 0.0f;

	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
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
	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
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
	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
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
	return HasEffectOfType(Actor, ESkillEffectType::Stun);
}

bool USkillEffectManager::IsSilenced(AActor *Actor) const
{
	return HasEffectOfType(Actor, ESkillEffectType::Silenced);
}

bool USkillEffectManager::HasActiveDOT(AActor *Actor) const
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}

	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
	{
		if (Effect.IsDOT())
		{
			return true;
		}
	}

	return false;
}

bool USkillEffectManager::IsImmuneToEffectType(AActor *Actor, ESkillEffectType EffectType) const
{
	// Universal immunity short-circuits everything.
	if (HasEffectOfType(Actor, ESkillEffectType::GrantAllStatusImmunity))
	{
		return true;
	}

	// Per-status-type gates. Element-specific (GrantFireImmunity etc.) live in
	// StatusBuildupManager::AddStatusBuildup which has the incoming Element in
	// scope — this query lacks that context and only handles trigger-type matches.
	switch (EffectType)
	{
	case ESkillEffectType::Stun:
		return HasEffectOfType(Actor, ESkillEffectType::GrantStunImmunity);
	case ESkillEffectType::Silenced:
		return HasEffectOfType(Actor, ESkillEffectType::GrantSilenceImmunity);
	case ESkillEffectType::DOT:
		return HasEffectOfType(Actor, ESkillEffectType::GrantDOTImmunity);
	// HealBlock gates ALL healing on the TARGET ("this character can't be healed").
	// A heal applied to a HealBlocked target is rejected here — the FIRST thing
	// ApplyEffect does — so it catches both instant item heals and any heal routed
	// through ApplyEffect, at one point. Keys on the heal TARGET (Actor == Target at
	// the call site), not the healer. Revive (ServerResurrect) bypasses ApplyEffect
	// entirely and is not a HealthRestore effect-type, so it stays ungated by design.
	case ESkillEffectType::HealthRestore:
	case ESkillEffectType::RestoreHPPercent:
		return HasEffectOfType(Actor, ESkillEffectType::HealBlock);
	default:
		return false;
	}
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

	const TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

	UE_LOG(LogTemp, Display, TEXT("[SkillEffectManager] DEBUG: %s has %d effects:"),
		   *Actor->GetName(), Effects.Num());

	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FActiveSkillEffect &Effect = Effects[i];
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

	const TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

	if (Effects.Num() == 0)
	{
		return TEXT("No effects");
	}

	FString Summary;
	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FActiveSkillEffect &Effect = Effects[i];
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

bool USkillEffectManager::IsTriggerConditionMet(AActor *Actor, const FActiveSkillEffect &Effect, float TriggerValue, AActor *Target) const
{
	(void)TriggerValue;

	// Cluster C: the N-condition group takes precedence when present. Empty group falls
	// through to the legacy 2-field logic, so the synthetic factories (infusion / physical
	// damage) that only write the old fields stay byte-identical.
	if (Effect.Conditions.Num() > 0)
	{
		// Subject-aware: owner-side subjects (Self/SelfTeam) always participate; target-side
		// (Target/TargetTeam) participate only when a Target was passed (null → skipped, the
		// pre-C2a behaviour). IsMet resolves the subject's actor(s) and ANY-folds the single-
		// actor state check over them (a one-element set reduces to the prior single check).
		return EvaluateConditionGroup(
			Effect.Conditions,
			[&](const FSkillCondition &C) { return IsOwnerSide(C.Subject) || Target != nullptr; },
			[&](const FSkillCondition &C) {
				const TArray<AActor *> Subjects = ResolveSubjectActors(C.Subject, Actor, Target);
				return Subjects.ContainsByPredicate([&](AActor *A) {
					return IsSingleTriggerMet(A, C.Trigger, C.Threshold);
				});
			});
	}

	const bool bPrimaryMet = IsSingleTriggerMet(Actor, Effect.TriggerCondition, Effect.TriggerThreshold);

	if (Effect.SecondaryTriggerCondition == ESkillTrigger::None)
	{
		return bPrimaryMet;
	}

	const bool bSecondaryMet = IsSingleTriggerMet(Actor, Effect.SecondaryTriggerCondition, Effect.SecondaryTriggerThreshold);

	return Effect.bRequireBothTriggers
			   ? (bPrimaryMet && bSecondaryMet)
			   : (bPrimaryMet || bSecondaryMet);
}

bool USkillEffectManager::IsSingleTriggerMet(AActor *Actor, ESkillTrigger Trigger, float Threshold) const
{
	UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);
	if (!CharComp)
	{
		return false;
	}

	switch (Trigger)
	{
	case ESkillTrigger::OnHPBelowThreshold:
	{
		float HPPercent = (CharComp->MaxHP > 0)
							  ? (static_cast<float>(CharComp->CurrentHP) / static_cast<float>(CharComp->MaxHP)) * 100.0f
							  : 0.0f;
		return HPPercent < Threshold;
	}

	case ESkillTrigger::OnHPAboveThreshold:
	{
		float HPPercent = (CharComp->MaxHP > 0)
							  ? (static_cast<float>(CharComp->CurrentHP) / static_cast<float>(CharComp->MaxHP)) * 100.0f
							  : 0.0f;
		return HPPercent > Threshold;
	}

	case ESkillTrigger::OnEnergyBelowThreshold:
	{
		float EPPercent = (CharComp->MaxEP > 0)
							  ? (static_cast<float>(CharComp->CurrentEP) / static_cast<float>(CharComp->MaxEP)) * 100.0f
							  : 0.0f;
		return EPPercent < Threshold;
	}

	case ESkillTrigger::OnEnergyAboveThreshold:
	{
		float EPPercent = (CharComp->MaxEP > 0)
							  ? (static_cast<float>(CharComp->CurrentEP) / static_cast<float>(CharComp->MaxEP)) * 100.0f
							  : 0.0f;
		return EPPercent > Threshold;
	}

	case ESkillTrigger::Always:
		return true;

	// Event-based triggers are always "met" when the event occurs
	case ESkillTrigger::OnCrit:
	case ESkillTrigger::OnHit:
	case ESkillTrigger::OnTakeDamage:
	case ESkillTrigger::OnKill:
	case ESkillTrigger::OnDodge:
	case ESkillTrigger::OnBlock:
	case ESkillTrigger::OnTurnStart:
	case ESkillTrigger::OnTurnEnd:
	case ESkillTrigger::OnSpellCast:
	case ESkillTrigger::OnAbilityUsed:
	case ESkillTrigger::OnBattleStart:
	case ESkillTrigger::OnStatusApplied:
	case ESkillTrigger::OnStatusReceived:
		return true;

	default:
		return false;
	}
}

TArray<AActor *> USkillEffectManager::ResolveSubjectActors(ECondSubject Subject, AActor *Owner, AActor *Target) const
{
	TArray<AActor *> Out;
	UTurnManager *TM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;

	switch (Subject)
	{
	case ECondSubject::Self:
		if (Owner) { Out.Add(Owner); }
		break;
	case ECondSubject::Target:
		if (Target) { Out.Add(Target); }
		break;
	case ECondSubject::SelfTeam:
		if (Owner && TM) { Out = TM->GetTeamMembers(TM->GetActorTeam(Owner)); }
		break;
	case ECondSubject::TargetTeam:
		if (Target && TM) { Out = TM->GetTeamMembers(TM->GetActorTeam(Target)); }
		break;
	}

	return Out;
}

FActiveSkillEffect *USkillEffectManager::FindEffectByID(AActor *Actor, int32 EffectID)
{
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return nullptr;
	}

	for (FActiveSkillEffect &Effect : ActiveEffects[Actor])
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

	for (FActiveSkillEffect &Effect : ActiveEffects[Actor])
	{
		Effect.ResetTurnFlags();
	}
}

bool USkillEffectManager::IsSpeedEffect(ESkillEffectType EffectType) const
{
	switch (EffectType)
	{
	case ESkillEffectType::SpeedBuff:
	case ESkillEffectType::SpeedDebuff:
	case ESkillEffectType::ActionSpeedBuff:
	case ESkillEffectType::ActionSpeedDebuff:
	case ESkillEffectType::TurnSpeedBuff:
	case ESkillEffectType::TurnSpeedDebuff:
	case ESkillEffectType::ModifyTurnSpeed: // folded into effective speed by CalculateSpeedRatios
	// SpiritBuff/Debuff feed turn speed via the modified-Spirit read in CacheActorStats
	// (Job 2), so speed must recompute on their apply/expiry. Mind/Body pillar buffs
	// deliberately absent — they don't touch turn speed.
	case ESkillEffectType::SpiritBuff:
	case ESkillEffectType::SpiritDebuff:
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

void USkillEffectManager::ApplyTriggeredSkillEffect(AActor *Source, AActor *Target, ESkillEffectType StatusType, ESpellElement Element)
{
	if (!Target || StatusType == ESkillEffectType::None)
	{
		return;
	}

	// Source no longer used for scaling - bar-cap design scales effects off the
	// target (e.g. DOT and Burst use Target MaxHP). Source still passed to
	// ApplyEffect below for attribution.

	FActiveSkillEffect Effect;
	Effect.Element = Element;
	Effect.EffectID = FMath::Rand();

	// Generate element-aware display name
	Effect.EffectName = SkillEffectDisplayNames::GetDisplayName(StatusType, Element);

	// Triggered effects: bar-cap design magnitudes (Session X)
	switch (StatusType)
	{
	case ESkillEffectType::DOT:
	{
		Effect.EffectType = ESkillEffectType::DOT;
		// 8% MaxHP per tick - scaled at apply time via target's MaxHP.
		// TODO: long-term, change DOT processing to interpret Value as % of MaxHP
		// directly; for now we resolve to flat damage here for compatibility with
		// ApplyEffectLogic's int-damage handler.
		if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
		{
			Effect.EffectValue = TargetComp->MaxHP * 0.08f;
		}
		else
		{
			Effect.EffectValue = 30.0f; // fallback
		}
		Effect.RemainingTurns = 2;
		Effect.ProcessTiming = ESkillEffectTiming::EndOfOwnTurn;
		break;
	}

	case ESkillEffectType::DefenseDebuff:
		Effect.EffectType = ESkillEffectType::DefenseDebuff;
		Effect.EffectValue = 30.0f; // 30% defence reduction
		Effect.RemainingTurns = 2;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::SkipTurn:
		Effect.EffectType = ESkillEffectType::SkipTurn;
		Effect.EffectValue = 1.0f; // gate
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		break;

	case ESkillEffectType::SpeedDebuff:
		Effect.EffectType = ESkillEffectType::SpeedDebuff;
		Effect.EffectValue = 50.0f; // 50%
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::CritDebuff:
		Effect.EffectType = ESkillEffectType::CritChanceDebuff;
		Effect.EffectValue = 100.0f; // -100% crit chance
		Effect.RemainingTurns = 2;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::EnergyDebuff:
		Effect.EffectType = ESkillEffectType::EnergyDrain;
		Effect.EffectValue = 100.0f; // 100% locked
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::RandomDebuff:
	{
		// Superseded by RandomSkill below in the new bar-cap design; left wired
		// so any pre-existing trigger mappings still resolve until Session Y.
		TArray<ESkillEffectType> Debuffs = {
			ESkillEffectType::DamageDebuff,
			ESkillEffectType::DefenseDebuff,
			ESkillEffectType::SpeedDebuff,
			ESkillEffectType::CritChanceDebuff};
		Effect.EffectType = Debuffs[FMath::RandRange(0, Debuffs.Num() - 1)];
		Effect.EffectValue = 30.0f; // 30%
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
	}
	break;

	// ==================== BAR-CAP GATE EFFECTS (Session X) ====================
	case ESkillEffectType::Stun:
		Effect.EffectType = ESkillEffectType::Stun;
		Effect.EffectValue = 1.0f; // gate
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		break;

	case ESkillEffectType::HealBlock:
		Effect.EffectType = ESkillEffectType::HealBlock;
		Effect.EffectValue = 1.0f; // gate
		Effect.RemainingTurns = 2;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::Silenced:
		Effect.EffectType = ESkillEffectType::Silenced;
		Effect.EffectValue = 1.0f; // gate
		Effect.RemainingTurns = 2;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::RandomSkill:
		Effect.EffectType = ESkillEffectType::RandomSkill;
		Effect.EffectValue = 1.0f; // gate
		Effect.RemainingTurns = 1;
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		break;

	case ESkillEffectType::BurstDamage:
	{
		// Design changed: 25% MaxHP target-scaled, can kill (1-HP clamp removed).
		// Full damage routes through the normal death path (ServerTakeDamage ->
		// CheckDeath -> OnDied), identical to a killing blow / the DoT change.
		UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
		if (TargetComp)
		{
			int32 BurstDamage = FMath::RoundToInt(TargetComp->MaxHP * 0.25f);

			if (BurstDamage > 0)
			{
				TargetComp->ServerTakeDamage(BurstDamage);
				UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Reality Burst dealt %d (25%% MaxHP, can be lethal) to %s"),
					   BurstDamage, *Target->GetName());
			}
		}
		return; // one-shot, no FActiveSkillEffect applied
	}

	default:
		return;
	}

	Effect.InitialDuration = Effect.RemainingTurns;
	ApplyEffect(Target, Effect, Source, TEXT("Status Bar Trigger"), -1);

	UE_LOG(LogTemp, Log, TEXT("[SkillEffectManager] Applied triggered %s to %s"),
		   *Effect.EffectName, *Target->GetName());
}