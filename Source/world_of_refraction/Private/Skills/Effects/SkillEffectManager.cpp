// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/TickMath.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Skills/Effects/SkillTriggerUtils.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Effects/SkillEffectDisplayNames.h"
#include "Skills/Effects/StatModifierTable.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "AbilitySystemComponent.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoREffectSpecBuilder.h"
#include "GAS/WoRCombatEvents.h"
#include "Tags/WoRGameplayTags.h"
#include "WoRLog.h"

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
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Bound to ActionExecutor::OnDamageDealt"));
		}
		else
		{
			UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] ActionExecutor not available at Initialize — OnDamageDealt binding skipped"));
		}

		// Bind OnDefenseResolved listener — drives defense-outcome conditional effects (C3b).
		if (UDefenseSystem *DS = GI->GetSubsystem<UDefenseSystem>())
		{
			DS->OnDefenseResolved.AddDynamic(this, &USkillEffectManager::OnDefenseResolvedHandler);
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Bound to DefenseSystem::OnDefenseResolved"));
		}
		else
		{
			UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] DefenseSystem not available at Initialize — OnDefenseResolved binding skipped"));
		}
	}

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Initialized"));
}

void USkillEffectManager::Deinitialize()
{
	ClearAllEffects();

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Deinitialized"));

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
		UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] ApplyEffect: Target is null"));
		return EEffectApplicationResult::Rejected;
	}

	// Check immunity
	if (IsImmuneToEffectType(Target, Effect.EffectType))
	{
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s is immune to %s"),
			   *Target->GetName(), *Effect.EffectName);
		return EEffectApplicationResult::Rejected;
	}

	// Identity = (holder, EffectID, source) (D-012, D-053). Gear arrives stamped with its item;
	// anything else is keyed on the applying actor + the skill or reason it applied with.
	if (Effect.SourceKey == 0)
	{
		Effect.SourceKey = EffectIdentity::MakeSourceKey(Source ? Source->GetUniqueID() : 0, SourceAbility);
	}

	// D2: fires-once gate — reject re-application this match BEFORE any stacking/refresh,
	// so a fires-once effect can never stack or refresh either. One use per holder and
	// source (D-011): two wearers of the same gear each get theirs. A doesn't-stack effect
	// (D-053) has one use per holder whichever source brings it ("revive once per fight").
	const FWoRFiredOnceKey FiredOnceKey{Target, Effect.EffectID, Effect.bDoesNotStack ? 0 : Effect.SourceKey};
	if (Effect.bFiresOncePerMatch && FiredOnceThisMatch.Contains(FiredOnceKey))
	{
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s rejected on %s — already fired once this match (ID=%d)"),
			   *Effect.EffectName, *Target->GetName(), Effect.EffectID);
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
	// ActiveEffects, so it never matches the same-source re-apply — policy-safe by construction.
	// (Contrast ESkillEffectTiming::Immediate, which stores-then-tears-down via bPendingRemoval.)
	// IsInstant() is the single derive point: a zero-duration, non-permanent effect. Permanent
	// gear/auras are excluded via their explicit bPermanent toggle (no longer derived from
	// duration-0, which now means INSTANT cleanly).
	if (Effect.IsInstant())
	{
		ApplyEffectLogic(Target, Effect);
		OnEffectApplied.Broadcast(Target, Effect);
		// A fires-once instant spends its use too, or it could fire every time (B-062).
		if (Effect.bFiresOncePerMatch)
		{
			FiredOnceThisMatch.Add(FiredOnceKey);
		}
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Instant %s applied to %s (Value: %.1f, not stored)"),
			   *Effect.EffectName, *Target->GetName(), Effect.EffectValue);
		return EEffectApplicationResult::Applied;
	}

	ApplyTimingRule(Target, Effect);

	// Get or create effect array for this actor
	TArray<FActiveSkillEffect> &Effects = ActiveEffects.FindOrAdd(Target);

	// The copy this source already put on the holder, if any (D-012, D-053). Another
	// source's copy is a separate effect, unless the definition says it doesn't stack.
	FActiveSkillEffect *ExistingEffect = FindEffectByIdentity(Target, Effect.EffectID, Effect.SourceKey);
	if (ExistingEffect && UWoREffectMirrorComponent::OwnsLifetime(Target))
	{
		// B-042h: a same-source re-apply of a copy whose GE GAS already ended starts it again.
		Target->FindComponentByClass<UWoREffectMirrorComponent>()->ForgetEnded(*ExistingEffect);
	}

	if (!ExistingEffect && Effect.bDoesNotStack && FindEffectByID(Target, Effect.EffectID))
	{
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s: %s rejected — doesn't stack, another source's copy is held"),
			   *Target->GetName(), *Effect.EffectName);
		return EEffectApplicationResult::Rejected;
	}

	if (ExistingEffect)
	{
		// Re-apply from the same source REPLACES (Q-044 A, the deckbuilder rule): the value is
		// re-snapshotted from this application (D-012) and the duration refreshes, gated by
		// the per-effect bRefreshDurationOnReapply opt-out. Maintenance mechanics (Defend's
		// constant 50%, on-hit Burn at a constant magnitude) re-apply the same value, so they
		// still just refresh.
		const bool bValueChanged = !FMath::IsNearlyEqual(Effect.EffectValue, ExistingEffect->EffectValue);

		// Stacking: a stackable re-apply adds a stack (count climbs) and takes this
		// application's per-stack value. At MAX stacks this branch is skipped and the plain
		// replace below runs.
		if (Effect.bCanStack && ExistingEffect->CanAddStack())
		{
			ExistingEffect->CurrentStacks++;
			ExistingEffect->EffectValue = Effect.EffectValue;

			if (Effect.bRefreshDurationOnReapply)
			{
				ExistingEffect->RemainingTurns = Effect.InitialDuration;
				ExistingEffect->HolderTurns = Effect.HolderTurns;
			}

			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s: %s stacked to %d on %s"),
				   *Target->GetName(), *Effect.EffectName, ExistingEffect->CurrentStacks, *Target->GetName());

			OnEffectStacksChanged.Broadcast(Target, *ExistingEffect, ExistingEffect->CurrentStacks);

			// Stacking raises the effective (stacked) value -> notify speed consumers.
			if (IsSpeedEffect(Effect.EffectType))
			{
				NotifySpeedChanged(Target);
			}

			return EEffectApplicationResult::StackAdded;
		}

		// Non-stacking (or at max stacks): replace the value, refresh the duration.
		const float PreviousValue = ExistingEffect->EffectValue;
		ExistingEffect->EffectValue = Effect.EffectValue;

		// A changed magnitude on a speed effect must re-notify the turn order.
		if (bValueChanged && IsSpeedEffect(Effect.EffectType))
		{
			NotifySpeedChanged(Target);
		}

		if (Effect.bRefreshDurationOnReapply)
		{
			ExistingEffect->RemainingTurns = Effect.InitialDuration;
			ExistingEffect->HolderTurns = Effect.HolderTurns;
		}

		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s: %s re-applied by the same source — value %.1f -> %.1f, %d turns left"),
			   *Target->GetName(), *Effect.EffectName, PreviousValue, Effect.EffectValue, ExistingEffect->RemainingTurns);

		OnEffectDurationChanged.Broadcast(Target, *ExistingEffect, ExistingEffect->RemainingTurns);
		return EEffectApplicationResult::DurationRefreshed;
	}

	// New effect - apply it
	Effects.Add(Effect);

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Applied %s to %s (ID: %d, Duration: %d, Value: %.1f)"),
		   *Effect.EffectName, *Target->GetName(), Effect.EffectID, Effect.RemainingTurns, Effect.EffectValue);

	// Fire-on-application (Option A: new effects only). Re-applications return before this point
	// (replace and stack both exit in the ExistingEffect branch above), so a re-apply can never
	// reach here — Option A safety by construction. Runs ApplyEffectLogic
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
		FiredOnceThisMatch.Add(FiredOnceKey);
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
		// effect index yields PackEffectID(DefID, BundleIndex, payload) — the same definition
		// gives the same EffectID. The item it came from is the source (D-053), so two items
		// with that definition each add their own copy. One authored effect still yields N
		// runtime effects (one per payload).
		TArray<FActiveSkillEffect> Runtimes = FActiveSkillEffect::CreateAllFromSkillEffect(
			Source.EffectName, G.DefID, Source, G.BundleIndex);

		for (FActiveSkillEffect &Runtime : Runtimes)
		{
			Runtime.SourceKey = G.SourceKey;
			Runtime.bFromGear = true;
			ApplyEffect(Target, Runtime, nullptr, Source.EffectName, -1);
		}
	}
}

int32 USkillEffectManager::SwapEquipmentEffects(
	AActor *Target,
	const TArray<FGatheredEffect> &OldEffects,
	const TArray<FGatheredEffect> &NewEffects)
{
	if (!Target)
	{
		return 0;
	}
	const auto SameGearEffect = [](const FGatheredEffect &A, const FGatheredEffect &B)
	{
		return A.DefID == B.DefID && A.BundleIndex == B.BundleIndex && A.SourceKey == B.SourceKey;
	};
	const auto Lists = [&SameGearEffect](const TArray<FGatheredEffect> &Effects, const FGatheredEffect &Effect)
	{
		return Effects.ContainsByPredicate([&](const FGatheredEffect &Other) { return SameGearEffect(Other, Effect); });
	};

	int32 Removed = 0;
	for (const FGatheredEffect &Old : OldEffects)
	{
		if (Lists(NewEffects, Old) || !Old.Effect.IsValid())
		{
			continue;
		}
		// The same packing ApplyEquipmentEffects used gives back each payload's EffectID.
		bool bRemovedAny = false;
		for (const FActiveSkillEffect &Runtime :
			 FActiveSkillEffect::CreateAllFromSkillEffect(Old.Effect.EffectName, Old.DefID, Old.Effect, Old.BundleIndex))
		{
			bRemovedAny |= RemoveEffectByIdentity(Target, Runtime.EffectID, Old.SourceKey);
		}
		Removed += bRemovedAny ? 1 : 0;
	}

	// Q-081: only lasting passives come on with a swap; a one-shot, timed or charged starting
	// effect is the fight's start bonus, given once (a swap back would farm it).
	const auto IsPassive = [](const FSkillEffect &Effect)
	{
		return Effect.Payloads.Num() > 0 &&
			   !Effect.Payloads.ContainsByPredicate([](const FSkillEffectPayload &Payload) { return !Payload.bPermanent; });
	};
	TArray<FGatheredEffect> Added;
	for (const FGatheredEffect &New : NewEffects)
	{
		if (!Lists(OldEffects, New) && IsPassive(New.Effect))
		{
			Added.Add(New);
		}
	}
	ApplyEquipmentEffects(Target, Added);

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Gear swap on %s: %d gear effect(s) off, %d on"),
		   *Target->GetName(), Removed, Added.Num());
	return Removed + Added.Num();
}

// FAutoConsoleCommandWithWorld — NOT UFUNCTION(Exec). A UGameInstanceSubsystem is not on the
// engine's FExec dispatch chain, so an Exec here silently no-ops. Console commands resolve from
// any class. "wor." prefix groups them. State is PIE-only, so the chain is null-checked.
#if !UE_BUILD_SHIPPING
namespace
{
	static FAutoConsoleCommandWithWorld GStartingEffectsCmd(
		TEXT("wor.StartingEffects"),
		TEXT("Inspect the active combatant's starting effects per gear source (PIE debug)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
				USkillEffectManager *Mgr = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
				if (Mgr)
				{
					Mgr->WOR_StartingEffects();
				}
				else
				{
					UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] wor.StartingEffects: no subsystem (no PIE world?)."));
				}
			}));

	static FAutoConsoleCommandWithWorld GPrintAllEffectsCmd(
		TEXT("wor.PrintAllEffects"),
		TEXT("Print active effects for every tracked actor (PIE debug)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
				USkillEffectManager *Mgr = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
				if (Mgr)
				{
					Mgr->DebugPrintAllEffects();
				}
				else
				{
					UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] wor.PrintAllEffects: no subsystem (no PIE world?)."));
				}
			}));

	static FAutoConsoleCommandWithWorld GPrintEffectsCmd(
		TEXT("wor.PrintEffects"),
		TEXT("Print active effects for the active combatant (PIE debug)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
				USkillEffectManager *Mgr = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
				if (!Mgr)
				{
					UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] wor.PrintEffects: no subsystem (no PIE world?)."));
					return;
				}
				UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
				AActor *Actor = TurnMgr ? TurnMgr->GetCurrentActor() : nullptr;
				if (!Actor)
				{
					UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] wor.PrintEffects: no active combatant (combat not active?)."));
					return;
				}
				Mgr->DebugPrintEffects(Actor);
			}));
}
#endif // !UE_BUILD_SHIPPING

void USkillEffectManager::WOR_StartingEffects()
{
	UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
	if (!TurnMgr)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[WOR_StartingEffects] No TurnManager subsystem."));
		return;
	}

	AActor *Actor = TurnMgr->GetCurrentActor();
	if (!Actor)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[WOR_StartingEffects] No active character (combat not active?)."));
		return;
	}

	ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!LC)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[WOR_StartingEffects] No LoadoutComponent on %s."), *Actor->GetName());
		return;
	}

	UE_LOG(LogWoREffects, Display, TEXT("=== STARTING EFFECTS — %s ==="), *Actor->GetName());

	auto PrintSpecs = [](const TCHAR *SourceLabel, const TArray<FSkillEffect> &Specs) -> int32
	{
		for (const FSkillEffect &E : Specs)
		{
			UE_LOG(LogWoREffects, Display, TEXT("  PRE [%s] %s — %d payload(s), %d condition(s)"),
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
	switch (LC->GetCharacterClass())
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
	UE_LOG(LogWoREffects, Display, TEXT("  PRE total: %d labeled vs %d from GetActiveEffects%s"),
		   Labeled, Total,
		   Labeled == Total ? TEXT(" — MATCH") : TEXT(" — MISMATCH (coverage drift; fix this tool)"));

	// POST — what actually got applied (any window, post def-identity packing; effects now
	// window per-DEFINITION, so there is no single actor window to scan).
	const TArray<FActiveSkillEffect> Active = GetActiveEffects(Actor);
	for (const FActiveSkillEffect &E : Active)
	{
		UE_LOG(LogWoREffects, Display, TEXT("  POST %s — %s (ID=%d)"), *E.EffectName,
			   *UEnum::GetValueAsString(E.EffectType), E.EffectID);
	}
	UE_LOG(LogWoREffects, Display, TEXT("  POST applied: %d"), Active.Num());
	UE_LOG(LogWoREffects, Display, TEXT("======================"));
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

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Applying %s from %s (Buildup: %d × %.1f × %d hits)"),
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

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Applying weapon-infused DOT: %s (%.1f damage × %.1f multiplier)"),
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

			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %s from %s (by ID)"),
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
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %d effects named '%s' from %s"),
			   RemovedCount, *EffectName, *Target->GetName());
	}

	// Notify TurnManager if any speed effects were removed
	if (bSpeedChanged)
	{
		NotifySpeedChanged(Target);
	}

	return RemovedCount;
}

bool USkillEffectManager::RemoveEffectByIdentity(AActor *Target, int32 EffectID, int32 SourceKey)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return false;
	}

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Target];
	const int32 Index = Effects.IndexOfByPredicate([EffectID, SourceKey](const FActiveSkillEffect &Effect)
	{
		return Effect.EffectID == EffectID && Effect.SourceKey == SourceKey;
	});
	if (Index == INDEX_NONE)
	{
		return false;
	}

	const FActiveSkillEffect RemovedEffect = Effects[Index];
	Effects.RemoveAt(Index);
	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %s from %s (by identity)"),
		   *RemovedEffect.EffectName, *Target->GetName());
	OnEffectRemoved.Broadcast(Target, RemovedEffect);
	if (IsSpeedEffect(RemovedEffect.EffectType))
	{
		NotifySpeedChanged(Target);
	}
	return true;
}

int32 USkillEffectManager::CleanseEffectsByType(AActor *Target, ESkillEffectType EffectType)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return 0;
	}

	int32 RemovedCount = 0;
	for (const FActiveSkillEffect &Effect : TArray<FActiveSkillEffect>(ActiveEffects[Target]))
	{
		if (Effect.EffectType == EffectType && Effect.IsCleansable())
		{
			RemovedCount += RemoveEffectByIdentity(Target, Effect.EffectID, Effect.SourceKey) ? 1 : 0;
		}
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
		if (Effects[i].IsBuff() && Effects[i].IsCleansable())
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

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %d buffs from %s"),
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
		if (Effects[i].IsDebuff() && Effects[i].IsCleansable())
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

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %d debuffs from %s"),
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

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %d DOTs from %s"),
		   RemovedCount, *Target->GetName());

	return RemovedCount;
}

void USkillEffectManager::RemoveAllEffects(AActor *Target)
{
	if (!Target || !ActiveEffects.Contains(Target))
	{
		return;
	}

	// Empty the store BEFORE broadcasting, as every single-effect removal does: listeners
	// (MaxHP/EP pool recompute, the GAS attribute mirror) re-read the target's effects (B-041).
	const TArray<FActiveSkillEffect> Removed = MoveTemp(ActiveEffects[Target]);
	ActiveEffects.Remove(Target);
	bool bSpeedChanged = false;

	for (const FActiveSkillEffect &Effect : Removed)
	{
		if (IsSpeedEffect(Effect.EffectType))
		{
			bSpeedChanged = true;
		}
		OnEffectRemoved.Broadcast(Target, Effect);
	}

	const int32 Count = Removed.Num();

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed all %d effects from %s"),
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
	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] FiredOnceThisMatch + ArmedConditionals reset for new combat"));
}

void USkillEffectManager::ArmConditionalEffects(AActor *Actor, const TArray<FGatheredEffect> &Conditionals)
{
	if (!Actor)
	{
		return;
	}
	ArmedConditionals.Add(Actor, Conditionals);
	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Armed %d conditional effect(s) on %s"),
		   Conditionals.Num(), *Actor->GetName());
}

void USkillEffectManager::ClearAllEffects()
{
	// Empty first, then broadcast (see RemoveAllEffects).
	const auto Removed = MoveTemp(ActiveEffects);
	ActiveEffects.Empty();
	// B-081: the fight is over, so a later gear swap must not re-apply gear effects (IsGearArmed).
	ArmedConditionals.Empty();

	int32 TotalCount = 0;
	for (const auto &Pair : Removed)
	{
		TotalCount += Pair.Value.Num();
		if (Pair.Key.IsValid())
		{
			for (const FActiveSkillEffect &Effect : Pair.Value)
			{
				OnEffectRemoved.Broadcast(Pair.Key.Get(), Effect);
			}
		}
	}

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Cleared all effects (%d total)"), TotalCount);
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

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Removed %d effects from source %s"),
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

	// D-051: a finished holder-turn span ends as the turn starts, before this turn's payouts. B-042h:
	// where GAS owns the lifetime the tracker ends the GEs first and their stored copies follow; the
	// enum expiry then only finds effects GAS doesn't hold.
	UWoRTurnEffectTracker *Tracker = Actor->FindComponentByClass<UWoRTurnEffectTracker>();
	const bool bGASLifetime = UWoREffectMirrorComponent::OwnsLifetime(Actor);
	if (bGASLifetime)
	{
		Tracker->OnHolderTurnStart();
		Actor->FindComponentByClass<UWoREffectMirrorComponent>()->ReleaseEndedCopies();
	}
	ExpireFinishedHolderTurnSpans(Actor);
	if (Tracker && !bGASLifetime)
	{
		Tracker->OnHolderTurnStart(); // the same rule for the GAS effects (B-042)
	}

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Processing start-of-turn effects for %s"),
		   *Actor->GetName());

	// Process start-of-turn effects
	ProcessEffectsWithTiming(Actor, ESkillEffectTiming::StartOfOwnTurn);

	// Process persistent effects (always active, but we re-apply stat mods)
	ProcessEffectsWithTiming(Actor, ESkillEffectTiming::Persistent);

	// Check conditional triggers with current state
	ProcessTriggerEffects(Actor, ESkillTrigger::OnTurnStart);
	WoRCombatEvents::Send(Actor, WoRTags::Event_Combat_TurnStart, Actor); // B-042d, Q-044j
}

void USkillEffectManager::ProcessEndOfTurnEffects(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Processing end-of-turn effects for %s"),
		   *Actor->GetName());

	// Process end-of-turn effects (DOTs, etc.)
	ProcessEffectsWithTiming(Actor, ESkillEffectTiming::EndOfOwnTurn);

	// Check conditional triggers
	ProcessTriggerEffects(Actor, ESkillTrigger::OnTurnEnd);
	WoRCombatEvents::Send(Actor, WoRTags::Event_Combat_TurnEnd, Actor); // B-042d, Q-044j

	// Tick durations and remove expired effects. B-042h: where GAS owns the lifetime the tracker
	// counts first and the stored copies take its count.
	UWoRTurnEffectTracker *Tracker = Actor->FindComponentByClass<UWoRTurnEffectTracker>();
	const bool bGASLifetime = UWoREffectMirrorComponent::OwnsLifetime(Actor);
	if (bGASLifetime)
	{
		Tracker->OnHolderTurnEnd();
	}
	CountHolderTurnSpans(Actor);
	if (Tracker && !bGASLifetime)
	{
		Tracker->OnHolderTurnEnd();
	}
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

	// Forward index loop (NOT range-based): ConsumeCharge below can RemoveAt the fired effect,
	// which would invalidate a range-based for. Same forward order + identical body, so uncharged
	// effects (ConsumeCharge no-ops, returns false) behave byte-identically to the prior loop.
	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		FActiveSkillEffect &Effect = Effects[i];

		if (Effect.ProcessTiming == ESkillEffectTiming::OnTrigger &&
			Effect.TriggerCondition == Trigger)
		{
			if (IsTriggerConditionMet(Actor, Effect, TriggerValue))
			{
				if (!Effect.bTriggerActive)
				{
					Effect.bTriggerActive = true;
					ApplyEffectLogic(Actor, Effect);

					UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Trigger activated: %s on %s"),
						   *Effect.EffectName, *Actor->GetName());

					// A trigger FIRE consumes a charge (the primary charge consumer). Charges == 0
					// → no-op (unlimited). If charges hit 0 the effect is removed here; compensate
					// the index so the next slot isn't skipped. NOTE: periodic ticks
					// (ProcessEffectsWithTiming) deliberately do NOT consume — a DOT tick is not a
					// charge fire. Per-tick consumption is a future opt-in, not built here.
					if (ConsumeCharge(Actor, Effect))
					{
						--i;
					}
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
	// ApplyEffectLogic can ADD effects to this same actor (DOT BuildupPerTick -> bar cap ->
	// ApplyTriggeredSkillEffect -> ApplyEffect) or REMOVE them (Cleanse / Remove*Debuff), and
	// adding another actor's entry can rehash ActiveEffects — a range-for or held array
	// reference would dangle. So: claim this window's effects up front (mark + copy), then
	// apply the copies, skipping any an earlier apply in this pass removed. Effects appended
	// mid-pass are NOT claimed, so they wait for their own window (bDelayFirstExecution kept).
	TArray<FActiveSkillEffect> *Effects = ActiveEffects.Find(Actor);
	if (!Effects)
	{
		return;
	}

	TArray<FActiveSkillEffect> Claimed;
	for (FActiveSkillEffect &Effect : *Effects)
	{
		if (Effect.ProcessTiming == Timing && !Effect.bProcessedThisTurn)
		{
			Effect.bProcessedThisTurn = true;
			Claimed.Add(Effect);
		}
	}

	for (FActiveSkillEffect &Pending : Claimed)
	{
		const TArray<FActiveSkillEffect> *Live = ActiveEffects.Find(Actor);
		const bool bStillActive = Live && Live->ContainsByPredicate([&Pending](const FActiveSkillEffect &Candidate)
																	 { return Candidate.EffectID == Pending.EffectID &&
																			  Candidate.SourceKey == Pending.SourceKey &&
																			  Candidate.EffectType == Pending.EffectType &&
																			  Candidate.EffectName == Pending.EffectName; });
		if (!bStillActive)
		{
			continue; // removed earlier in this pass (e.g. cleansed)
		}

		ApplyEffectLogic(Actor, Pending);
		OnEffectTriggered.Broadcast(Actor, Pending);
	}
}

// TODO: obsolete since B-225: every timed effect ApplyEffect stores counts holder turns
// (FTurnSpan), so none reaches this plain tick-down. Remove after Crown's PIE pass on B-225.
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

		// Skip immediate effects (already handled), and holder-turn spans (CountHolderTurnSpans)
		if (Effect.ProcessTiming == ESkillEffectTiming::Immediate || Effect.bCountsHolderTurns)
		{
			continue;
		}

		// Tick duration
		Effect.RemainingTurns--;

		OnEffectDurationChanged.Broadcast(Actor, Effect, Effect.RemainingTurns);

		if (Effect.RemainingTurns <= 0)
		{
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s expired on %s"),
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

void USkillEffectManager::CountHolderTurnSpans(AActor *Actor)
{
	TArray<FActiveSkillEffect> *Effects = Actor ? ActiveEffects.Find(Actor) : nullptr;
	if (!Effects)
	{
		return;
	}

	const UWoREffectMirrorComponent *GASLifetime =
		UWoREffectMirrorComponent::OwnsLifetime(Actor) ? Actor->FindComponentByClass<UWoREffectMirrorComponent>() : nullptr;
	TArray<FActiveSkillEffect> Counted;
	for (FActiveSkillEffect &Effect : *Effects)
	{
		if (Effect.bCountsHolderTurns && !Effect.bPermanent)
		{
			const int32 TrackedTurnsLeft = GASLifetime ? GASLifetime->GetTrackedTurnsLeft(Effect) : INDEX_NONE;
			if (TrackedTurnsLeft >= 0)
			{
				Effect.HolderTurns.TurnsLeft = TrackedTurnsLeft; // a read-only copy of the tracker's span (B-042h)
			}
			else
			{
				Effect.HolderTurns.CountHolderTurn();
			}
			Effect.RemainingTurns = Effect.HolderTurns.TurnsLeft;
			Counted.Add(Effect);
		}
	}
	// Every span counted before anyone hears of one, so a listener's mirror sync sees each copy
	// already in line with the tracker (it would read a stale copy as a refresh).
	for (const FActiveSkillEffect &Effect : Counted)
	{
		OnEffectDurationChanged.Broadcast(Actor, Effect, Effect.RemainingTurns);
	}
}

void USkillEffectManager::ExpireFinishedHolderTurnSpans(AActor *Actor)
{
	TArray<FActiveSkillEffect> *Effects = Actor ? ActiveEffects.Find(Actor) : nullptr;
	if (!Effects)
	{
		return;
	}

	TArray<FActiveSkillEffect> Expired;
	for (int32 i = Effects->Num() - 1; i >= 0; --i)
	{
		const FActiveSkillEffect &Effect = (*Effects)[i];
		if (Effect.bCountsHolderTurns && !Effect.bPermanent && Effect.HolderTurns.EndsAtHolderTurnStart())
		{
			Expired.Add(Effect);
			Effects->RemoveAt(i);
		}
	}

	// Store already updated, so listeners re-reading the holder see the effects gone.
	bool bSpeedChanged = false;
	for (const FActiveSkillEffect &Effect : Expired)
	{
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s ended on %s as its turn started (holder-turn span)"),
			   *Effect.EffectName, *Actor->GetName());
		bSpeedChanged |= IsSpeedEffect(Effect.EffectType);
		OnEffectRemoved.Broadcast(Actor, Effect);
	}

	if (bSpeedChanged)
	{
		NotifySpeedChanged(Actor);
	}
}

bool USkillEffectManager::ConsumeCharge(AActor *Actor, FActiveSkillEffect &Effect)
{
	// No charge system on this effect (Charges == 0, the default) — unlimited, nothing to
	// consume. Existing/uncharged effects hit this no-op and stay byte-identical.
	if (Effect.Charges <= 0)
	{
		return false;
	}

	--Effect.Charges;

	// Charges remain → the effect persists (keeps ticking / waiting for its next fire).
	if (Effect.Charges > 0)
	{
		return false;
	}

	// Charges exhausted → remove via THIS path. Independent of TickDurations, which only does
	// turn-expiry and skips bPermanent (~:1010) — so for a permanent charged effect (e.g. a
	// phoenix revive buff) ConsumeCharge is the ONLY reaper. Mirrors the inline removal in
	// TickDurations (~:1037-1040): snapshot → RemoveAt → broadcast OnEffectRemoved.
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

	// Effect is a reference INTO this array (the helper's contract); recover its index by address.
	const int32 Index = static_cast<int32>(&Effect - Effects.GetData());
	if (!Effects.IsValidIndex(Index))
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] ConsumeCharge: effect ref not in %s's list — skipping removal"),
			   *Actor->GetName());
		return false;
	}

	const bool bWasSpeed = IsSpeedEffect(Effect.EffectType);
	FActiveSkillEffect RemovedEffect = Effect;
	Effects.RemoveAt(Index);

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s charges exhausted on %s — removed"),
		   *RemovedEffect.EffectName, *Actor->GetName());

	OnEffectRemoved.Broadcast(Actor, RemovedEffect);

	// A charge-removed speed effect must re-notify the turn order, same as turn-expiry.
	if (bWasSpeed)
	{
		NotifySpeedChanged(Actor);
	}

	return true;
}

float USkillEffectManager::ConsumeLastStandCharge(AActor *Owner)
{
	if (!Owner || !ActiveEffects.Contains(Owner))
	{
		return -1.0f; // no effects at all → no last stand → caller proceeds to death
	}

	TArray<FActiveSkillEffect> &Effects = ActiveEffects[Owner];

	// Multi-LastStand policy: if several LastStand effects are present, the HIGHEST-EffectValue one
	// fires (best one wins) and consumes ONE of ITS charges; the others are left intact. We need the
	// live array entry (not a GetEffectsByType copy) so ConsumeCharge can read its value AND remove
	// it from THIS array when its charges hit 0.
	int32 BestIndex = INDEX_NONE;
	float BestValue = -1.0f;
	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		if (Effects[i].EffectType == ESkillEffectType::LastStand &&
			(BestIndex == INDEX_NONE || Effects[i].EffectValue > BestValue))
		{
			BestValue = Effects[i].EffectValue;
			BestIndex = i;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return -1.0f; // bearer holds no LastStand effect
	}

	// Read the HP% BEFORE consuming — ConsumeCharge may RemoveAt this entry (last charge), which
	// would invalidate the reference. The local snapshot is what we return.
	const float LastStandHPPercent = Effects[BestIndex].EffectValue;

	// Consume ONE charge on the matched effect (C1 path). Charges=1 → removed now (single-use last
	// stand); Charges=N → survives to intercept again; at 0 it's removed (next lethal blow is fatal).
	ConsumeCharge(Owner, Effects[BestIndex]);

	return LastStandHPPercent;
}

bool USkillEffectManager::IsPayoutType(ESkillEffectType EffectType)
{
	switch (EffectType)
	{
	case ESkillEffectType::DOT:
	case ESkillEffectType::HealthRestore:
	case ESkillEffectType::RestoreHPPercent:
	case ESkillEffectType::EnergyRestore:
	case ESkillEffectType::RestoreEnergyPercent:
	case ESkillEffectType::EnergyDrain:
	case ESkillEffectType::DrainEnergy:
	case ESkillEffectType::DrainHP:
	case ESkillEffectType::SelfDamage:
		return true;
	default:
		return false;
	}
}

bool USkillEffectManager::IsHoldersOwnTurn(const AActor *Target) const
{
	const UTurnManager *TurnManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
	return Target && TurnManager && TurnManager->IsCombatActive() && TurnManager->GetCurrentActor() == Target;
}

void USkillEffectManager::ApplyTimingRule(const AActor *Target, FActiveSkillEffect &Effect) const
{
	if (Effect.bPermanent || Effect.ProcessTiming == ESkillEffectTiming::Immediate)
	{
		return;
	}
	// D-051: every timed effect counts the holder's own turns; the landing turn counts when it is
	// theirs, and the effect ends as the holder's turn after those starts.
	if (!Effect.bCountsHolderTurns)
	{
		Effect.StartHolderTurnSpan(Effect.RemainingTurns);
	}
	// Q-044c: every payout lands at the holder's turn start, exactly N of them. Q-063: one landing
	// on the holder's own turn counts that turn, whose start has passed, so it pays on landing.
	// Persistent (the payload default) runs at every turn start too, so it is timed the same way.
	const bool bPaysPerTurn = Effect.ProcessTiming == ESkillEffectTiming::StartOfOwnTurn ||
							  Effect.ProcessTiming == ESkillEffectTiming::EndOfOwnTurn ||
							  Effect.ProcessTiming == ESkillEffectTiming::Persistent;
	if (bPaysPerTurn && IsPayoutType(Effect.EffectType))
	{
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		Effect.bDelayFirstExecution = !IsHoldersOwnTurn(Target);
	}
}

int32 USkillEffectManager::GetDotTickDamage(AActor *Actor, const FActiveSkillEffect &Effect)
{
	FTickMathInputs Inputs;
	Inputs.Value = Effect.GetStackedValue();
	Inputs.bShareOfMaxHP = Effect.bShareOfMaxHP;
	if (!Effect.bShareOfMaxHP)
	{
		Inputs.ElementResistance = WoRResistance::GetElementDamageResistance(Actor, Effect.Element);
	}
	return WoRTickMath::GetDotTickDamage(Inputs);
}

void USkillEffectManager::ApplyEffectLogic(AActor *Actor, FActiveSkillEffect &Effect)
{
	UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);

	if (!CharComp)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] No CharacterDataComponent on %s"),
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
		const int32 DamageToApply = GetDotTickDamage(Actor, Effect);

		if (DamageToApply > 0)
		{
			CharComp->ServerTakeDamage(DamageToApply);
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s took %d DOT damage from %s (value %.1f, %s; can be lethal)"),
				   *Actor->GetName(), DamageToApply, *Effect.EffectName, Effect.GetStackedValue(), *UEnum::GetValueAsString(Effect.Element));
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
					UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s gained %d %s buildup from %s tick"),
						   *Actor->GetName(), BuildupAmount,
						   *UEnum::GetValueAsString(Effect.Element), *Effect.EffectName);
				}
			}
		}
		break;
	}

		// ==================== HEALING ====================
	case ESkillEffectType::HealthRestore:
		// HealBlock re-gate (per-tick): an ongoing HoT goes dormant while its BEARER (Actor) is
		// HealBlocked — mirrors the apply-time ApplyEffect immunity gate, now also at each tick. The
		// effect PERSISTS (not removed) and its duration still decrements via TickDurations; only
		// this tick's heal is skipped, so it resumes once HealBlock expires (if duration remains).
		if (IsImmuneToEffectType(Actor, ESkillEffectType::HealthRestore))
		{
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s: HoT '%s' suppressed by HealBlock (no heal this tick)"),
				   *Actor->GetName(), *Effect.EffectName);
		}
		else
		{
			// +healing-received amplify (HealBlock mirror) on the BEARER (Actor). Gate-then-amplify:
			// only reached when NOT HealBlocked, so a suppressed bearer is never amplified.
			FTickMathInputs Inputs;
			Inputs.Value = Value;
			Inputs.HealingReceivedPercent = GetTotalStatModifier(Actor, ESkillEffectType::HealingReceivedBuff);
			const int32 HealAmount = WoRTickMath::GetHotTickHeal(Inputs);
			CharComp->ServerHeal(HealAmount);
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s healed %d from %s"),
				   *Actor->GetName(), HealAmount, *Effect.EffectName);
		}
		break;

	// ==================== ENERGY ====================
	case ESkillEffectType::EnergyRestore:
		CharComp->ServerGainEnergy(FMath::RoundToInt(Value));
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s gained %d energy from %s"),
			   *Actor->GetName(), FMath::RoundToInt(Value), *Effect.EffectName);
		break;

	case ESkillEffectType::EnergyDrain:
		CharComp->ServerSpendEnergy(FMath::RoundToInt(Value));
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s lost %d energy from %s"),
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
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] Stat modifier %s active on %s (%.1f%%)"),
			   *Effect.EffectName, *Actor->GetName(), Value);
		break;

	// Targeted cleanses: gear debuffs stay (D-015).
	case ESkillEffectType::RemoveSpeedDebuff:
		CleanseEffectsByType(Actor, ESkillEffectType::SpeedDebuff);
		break;

	case ESkillEffectType::RemoveDamageDebuff:
		CleanseEffectsByType(Actor, ESkillEffectType::DamageDebuff);
		break;

	case ESkillEffectType::RemoveDefenseDebuff:
		CleanseEffectsByType(Actor, ESkillEffectType::DefenseDebuff);
		break;

	// ==================== SELF DAMAGE ====================
	// Wired Session X. Lunge's self-damage effect applied cleanly but had no
	// handler here, so it sat on the actor without ticking (May 2026 PIE).
	case ESkillEffectType::SelfDamage:
		CharComp->ServerTakeDamage(FMath::RoundToInt(Value));
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s took %d self-damage from %s"),
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
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] %s active on %s (gate effect)"),
			   *Effect.EffectName, *Actor->GetName());
		break;

	// RandomSkill (Void break, B-199): presence-read. ACombatOrchestrator::RequestActionFromActor
	// hands the holder's turn to UAIDecisionManager::BuildRandomAction.
	case ESkillEffectType::RandomSkill:
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] %s has RandomSkill active - its turn is played at random"),
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
	// recipient-side healing amplifier (HealBlock mirror) — presence-read only; its value is pulled
	// at the heal sites via GetTotalStatModifier, so there is no per-tick logic here.
	case ESkillEffectType::HealingReceivedBuff:
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] Passive stat modifier %s active on %s (%.1f%%)"),
			   *Effect.EffectName, *Actor->GetName(), Value);
		break;

	// Resource changes — percent restore / drain. Value is the percent (0..100).
	case ESkillEffectType::RestoreHPPercent:
	{
		// HealBlock re-gate (per-tick) — same as HealthRestore above: suppress the heal while the
		// bearer (Actor) is HealBlocked; the effect persists and its duration still decrements.
		if (IsImmuneToEffectType(Actor, ESkillEffectType::RestoreHPPercent))
		{
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s: HoT '%s' (%%-restore) suppressed by HealBlock (no heal this tick)"),
				   *Actor->GetName(), *Effect.EffectName);
		}
		else if (CharComp->MaxHP > 0)
		{
			int32 Amount = FMath::RoundToInt(CharComp->MaxHP * Value / CombatConstants::STAT_PERCENT_DIVISOR);
			// +healing-received amplify (HealBlock mirror) on the BEARER (Actor). Gate-then-amplify.
			const float HealRecvBonus = GetTotalStatModifier(Actor, ESkillEffectType::HealingReceivedBuff);
			Amount = FMath::RoundToInt(Amount * (1.0f + HealRecvBonus / CombatConstants::STAT_PERCENT_DIVISOR));
			if (Amount > 0)
			{
				CharComp->ServerHeal(Amount);
				UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s restored %d HP (%.1f%% of MaxHP) from %s"),
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
				UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s restored %d EP (%.1f%% of MaxEP) from %s"),
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
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s drained %d HP from %s (lethal)"),
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
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s drained %d EP from %s"),
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
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] Passive defensive %s active on %s"),
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
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] Immunity %s active on %s"),
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
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] %s armed on %s — fires on hit (Phase B hook pending)"),
			   *Effect.EffectName, *Actor->GetName());
		break;

	case ESkillEffectType::CleanseSelf:
	{
		const int32 Removed = RemoveAllDebuffs(Actor);
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s cleansed %d debuff(s) via %s"),
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
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] CleanseAllies on %s cleansed %d ally(ies)"),
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
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] GuaranteedCrit active on %s"), *Actor->GetName());
		break;
	case ESkillEffectType::IgnoreDefense:
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] IgnoreDefense active on %s"), *Actor->GetName());
		break;
	case ESkillEffectType::DoubleHit:
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] DoubleHit active on %s"), *Actor->GetName());
		break;
	case ESkillEffectType::LastStand:
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] LastStand armed on %s"), *Actor->GetName());
		break;

	// ==================== ROW MOVES (D-096, B-180) ====================
	// Instant: one row in the target's column. The grid refuses (and logs) a taken cell or the
	// last row; row bonuses read the grid per hit, so they change at once.
	case ESkillEffectType::PushBack:
	case ESkillEffectType::PullForward:
	{
		// A stored (Duration > 0) row move would shove the target again on every tick.
		if (!Effect.IsInstant())
		{
			UE_LOG(LogWoREffects, Warning, TEXT("[SkillEffectManager] %s on %s has a duration; row moves must be authored with Duration 0 - ignored"),
				   *Effect.EffectName, *Actor->GetName());
			break;
		}
		UGameInstance *GI = GetGameInstance();
		UCombatGridSubsystem *Grid = GI ? GI->GetSubsystem<UCombatGridSubsystem>() : nullptr;
		const UActionExecutor *AE = GI ? GI->GetSubsystem<UActionExecutor>() : nullptr;
		if (Grid && AE)
		{
			const bool bPush = Effect.EffectType == ESkillEffectType::PushBack;
			const bool bMoved = bPush ? Grid->PushActorBack(Actor, AE->GetArenaCenter())
									  : Grid->PullActorForward(Actor, AE->GetArenaCenter());
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] %s %s %s via %s"), *Actor->GetName(),
				   bMoved ? TEXT("was") : TEXT("could not be"), bPush ? TEXT("pushed back") : TEXT("pulled forward"),
				   *Effect.EffectName);
		}
		break;
	}

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
					UE_LOG(LogWoREffects, Log,
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
					UE_LOG(LogWoREffects, Log,
						   TEXT("[SkillEffectManager] %s status buildup reduced by %.1f via %s"),
						   *Actor->GetName(), Value, *Effect.EffectName);
				}
			}
		}
		break;
	}

	default:
		UE_LOG(LogWoREffects, Verbose, TEXT("[SkillEffectManager] Unhandled effect type for %s"),
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
	// TODO: B-043 - an attacker-side event and the crit flag (WoRCombatEvents.h).
	WoRCombatEvents::Send(Target, WoRTags::Event_Combat_Hit, Attacker, Damage); // B-042d, Q-044j

	// Lifesteal — heal the attacker for a percent of damage dealt.
	if (HasEffectOfType(Attacker, ESkillEffectType::Lifesteal))
	{
		const float LifestealPct = GetTotalStatModifier(Attacker, ESkillEffectType::Lifesteal) / CombatConstants::STAT_PERCENT_DIVISOR;
		const int32 HealAmount = FMath::Max(1, FMath::RoundToInt(Damage * LifestealPct));
		if (UCharacterDataComponent *Comp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			// HealBlock gates the RECIPIENT — lifesteal self-heals the Attacker, so the immunity
			// check is on Attacker (NOT the Target). Mirrors the ApplyEffect HealthRestore immunity
			// gate; a HealBlocked attacker gains no lifesteal HP.
			if (IsImmuneToEffectType(Attacker, ESkillEffectType::HealthRestore))
			{
				UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Lifesteal on %s blocked by HealBlock"),
					   *Attacker->GetName());
			}
			else
			{
				// +healing-received amplify (HealBlock mirror) on the RECIPIENT (Attacker). Gate-then-amplify.
				const float HealRecvBonus = GetTotalStatModifier(Attacker, ESkillEffectType::HealingReceivedBuff);
				const int32 AmplifiedHeal = FMath::RoundToInt(HealAmount * (1.0f + HealRecvBonus / CombatConstants::STAT_PERCENT_DIVISOR));
				Comp->ServerHeal(AmplifiedHeal);
				UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Lifesteal: %s healed %d from hit on %s"),
					   *Attacker->GetName(), AmplifiedHeal, *Target->GetName());
			}
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
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] ApplyStunToTarget fired on crit: %s stunned %s"),
			   *Attacker->GetName(), *Target->GetName());
	}
}

void USkillEffectManager::OnDefenseResolvedHandler(AActor *Defender, AActor *Attacker,
												   EDefenseType DefenseType, bool bPerfect, int32 ImpactIndex,
												   float AttackEnergyCost)
{
	// AttackEnergyCost = the triggering attack's BASE energy cost (pre-efficiency inherent worth).
	// Consumed below by payloads opting into attack-scaled absorb via FSkillEffectPayload::AttackCostScale.

	if (!Defender || !Attacker)
	{
		return;
	}
	// TODO: B-043 - the defense type and perfect flag as event tags when an ability needs them.
	WoRCombatEvents::Send(Defender, WoRTags::Event_Combat_DefenseResolved, Attacker, AttackEnergyCost);

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
				// attacker). Build ONE runtime per payload via the shared BuildRuntimeFromPayload
				// core — NOT CreateAllFromSkillEffect, which expands ALL payloads and would N^2-apply
				// inside this loop. Mirrors ActionExecutor::ApplySkillEffects's per-payload path.
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
					// (PackEffectID(DefID, BundleIndex, PayloadIndex)); the source key below keeps
					// each wearer's item a separate copy with its own fires-once use.
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
						// Built via the shared core. Element = the AUTHORED payload element (None =
						// generic / element-agnostic; a real element = element-keyed, e.g. an OnParry
						// payload granting a Lightning-keyed StatusMultiplierBuff "stack"). Conditions
						// are passed EMPTY on purpose: the defense-outcome conditions were the GATE
						// (already evaluated), so the consequence must tick on its natural timing
						// (DOT end-of-turn, etc.) — empty Conditions means NO OnTrigger promotion in
						// the core. bAutoDetectTimingByType=true preserves the type-based timing the
						// old CreateFromSpellEffect applied.
						FActiveSkillEffect Runtime = FActiveSkillEffect::BuildRuntimeFromPayload(
							E.EffectName,
							PayloadEffectID,
							Payload,
							TArray<FSkillCondition>{}, // deliberate omission — gate already consumed
							E.bStackable,
							E.MaxStacks,
							E.bFiresOncePerMatch,
							Payload.Element,
							/*bAutoDetectTimingByType*/ true);

						// No cast context here (no EffMagMult), but the defender pre-rounds its value
						// (:1703-1708) — overwrite the core's payload-derived value. Reproduces the
						// deleted CreateFromSpellEffect's EXACT expression ((Value!=0)?Value:Magnitude*100,
						// Value=RuntimeValue) so the zero-RuntimeValue fallback stays byte-identical too.
						// ATTACK-SCALED absorb (Cluster B): when the payload opts in (AttackCostScale > 0), its granted
						// value is AttackCostScale × the triggering attack's BASE energy cost (threaded in Cluster A) —
						// e.g. "gain EP = 5% of the parried attack's cost". AttackEnergyCost is 0 when the broadcast
						// couldn't resolve it (IsSet() guard) → scaled payload grants 0, gracefully no absorb. Scale 0
						// = the authored Value/Magnitude path (byte-identical).
						Runtime.EffectValue = (Payload.AttackCostScale > 0.0f)
							? static_cast<float>(FMath::RoundToInt(Payload.AttackCostScale * AttackEnergyCost))
							: ((RuntimeValue != 0) ? static_cast<float>(RuntimeValue) : (Payload.Magnitude * 100.0f));

						// Source = this wearer's item (D-053, D-011): it may land on someone else, so
						// the wearer is part of it — two wearers of the same gear stay two sources.
						Runtime.bDoesNotStack = E.bDoesNotStack;
						if (G.SourceKey != 0)
						{
							Runtime.SourceKey = static_cast<int32>(HashCombine(static_cast<uint32>(G.SourceKey), GetTypeHash(Owner->GetUniqueID())));
						}

						// On the wearer it's a gear effect (cleanse leaves it, D-015); on someone else it isn't.
						Runtime.bFromGear = (AppTarget == Owner);
						ApplyEffect(AppTarget, Runtime, Owner, E.EffectName, OwnerTeam);

						UE_LOG(LogWoREffects, Log, TEXT("[DefenseTrigger] FIRE: %s (ID=%d) %s -> %s outcome=%s perfect=%d"),
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
	// B-042c facade: wor.Effects.UseGAS 1 reads the mirrored GameplayEffects for ported types.
	if (const UAbilitySystemComponent *ASC = UWoREffectMirrorComponent::GetGASReadSource(Actor, EffectType))
	{
		return UWoREffectMirrorComponent::HasEffectOfType(*ASC, EffectType);
	}
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

bool USkillEffectManager::HasCleansableEffectOfType(AActor *Actor, ESkillEffectType EffectType) const
{
	if (const UAbilitySystemComponent *ASC = UWoREffectMirrorComponent::GetGASReadSource(Actor, EffectType))
	{
		return UWoREffectMirrorComponent::HasCleansableEffectOfType(*ASC, EffectType);
	}
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return false;
	}
	return ActiveEffects[Actor].ContainsByPredicate([EffectType](const FActiveSkillEffect &Effect)
	{
		return Effect.EffectType == EffectType && Effect.IsCleansable();
	});
}

bool USkillEffectManager::IsConditionHeld(const FSkillCondition &Condition, AActor *Owner, AActor *Target) const
{
	// The runtime skips a target-side check when there is no target, so it never blocks.
	if (!IsOwnerSide(Condition.Subject) && !Target)
	{
		return true;
	}
	const TArray<AActor *> Subjects = ResolveSubjectActors(Condition.Subject, Owner, Target);
	return Subjects.ContainsByPredicate([&](AActor *A) {
		return IsSingleTriggerMet(A, Condition.Trigger, Condition.Threshold);
	});
}

FGameplayTagContainer USkillEffectManager::GetEffectTags(AActor *Actor) const
{
	// B-042x: per type through HasEffectOfType, so wor.Effects.UseGAS 1 reads the holder's tags.
	// One mirror resync per type: fine for its debug / Blueprint callers, not for a hot path.
	FGameplayTagContainer Tags;
	for (const ESkillEffectType Type : WoRTags::TaggedEffectTypes())
	{
		if (HasEffectOfType(Actor, Type))
		{
			Tags.AddTag(WoRTags::EffectTypeToTag(Type));
		}
	}
	return Tags;
}

float USkillEffectManager::GetTotalStatModifier(AActor *Actor, ESkillEffectType ModifierType) const
{
	if (const UAbilitySystemComponent *ASC = UWoREffectMirrorComponent::GetGASReadSource(Actor, ModifierType))
	{
		return FWoREffectSpecBuilder::SumMagnitude(*ASC, ModifierType);
	}
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

float USkillEffectManager::GetTotalStatModifierForElement(AActor *Actor, ESkillEffectType ModifierType, ESpellElement BuildupElement) const
{
	if (const UAbilitySystemComponent *ASC = UWoREffectMirrorComponent::GetGASReadSource(Actor, ModifierType))
	{
		return FWoREffectSpecBuilder::SumMagnitudeForElement(*ASC, ModifierType, BuildupElement);
	}
	if (!Actor || !ActiveEffects.Contains(Actor))
	{
		return 0.0f;
	}

	float Total = 0.0f;

	for (const FActiveSkillEffect &Effect : ActiveEffects[Actor])
	{
		// EXACT element match (no None-fallback). Callers split the generic (None) and the
		// element-keyed (non-None) reads across two calls so the layers never double-count.
		if (Effect.EffectType == ModifierType && Effect.Element == BuildupElement)
		{
			Total += Effect.GetStackedValue();
		}
	}

	return Total;
}

float USkillEffectManager::GetNetStatModifier(AActor *Actor, ESubStat Stat) const
{
	const FStatModifierRow Row = StatModifierTable::Find(Stat);
	auto Sum = [this, Actor, &Row](ESkillEffectType Type) -> float
	{
		if (Type == ESkillEffectType::None)
		{
			return 0.0f;
		}
		return Row.bGenericElementOnly ? GetTotalStatModifierForElement(Actor, Type, ESpellElement::None)
									   : GetTotalStatModifier(Actor, Type);
	};
	const float Buff = Row.bBuffNeverNegative ? FMath::Max(0.0f, Sum(Row.Buff)) : Sum(Row.Buff);
	// Modify first, then buff, then debuff: the same order the hand-written reads summed in.
	return Sum(Row.Modify) + Buff - Sum(Row.Debuff);
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

int32 USkillEffectManager::GetSealedEP(AActor *Actor) const
{
	const UCharacterDataComponent *Comp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Comp || !IsSilenced(Actor))
	{
		return 0;
	}
	return FMath::RoundToInt(Comp->MaxEP * CombatConstants::SILENCE_SEALED_EP_FRACTION);
}

int32 USkillEffectManager::GetSpendableEP(AActor *Actor) const
{
	const UCharacterDataComponent *Comp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Comp)
	{
		return 0;
	}
	// CurrentEP is the unified spend pool, Broken Darkness included, so the seal applies to it too.
	return FMath::Max(0, Comp->CurrentEP - GetSealedEP(Actor));
}

bool USkillEffectManager::HasActiveDOT(AActor *Actor) const
{
	return HasEffectOfType(Actor, ESkillEffectType::DOT); // B-042x: the Status.DOT tag at UseGAS 1
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
		UE_LOG(LogWoREffects, Display, TEXT("[SkillEffectManager] DEBUG: Actor is null"));
		return;
	}

	if (!ActiveEffects.Contains(Actor))
	{
		UE_LOG(LogWoREffects, Display, TEXT("[SkillEffectManager] DEBUG: %s has no active effects"),
			   *Actor->GetName());
		return;
	}

	const TArray<FActiveSkillEffect> &Effects = ActiveEffects[Actor];

	UE_LOG(LogWoREffects, Display, TEXT("[SkillEffectManager] DEBUG: %s has %d effects:"),
		   *Actor->GetName(), Effects.Num());

	for (int32 i = 0; i < Effects.Num(); ++i)
	{
		const FActiveSkillEffect &Effect = Effects[i];
		UE_LOG(LogWoREffects, Display, TEXT("  [%d] %s (ID:%d, Source:%s/%s key %d%s%s) - %s, Value:%.1f, Duration:%s, Stacks:%d"),
			   i, *Effect.EffectName, Effect.EffectID,
			   *GetNameSafe(Effect.SourceActor.Get()), *Effect.SourceAbilityName, Effect.SourceKey,
			   Effect.bDoesNotStack ? TEXT(", doesn't stack") : TEXT(""), Effect.bFromGear ? TEXT(", from gear") : TEXT(""),
			   *UEnum::GetValueAsString(Effect.EffectType),
			   Effect.EffectValue, *Effect.GetDurationString(), Effect.CurrentStacks);
	}

	int32 FiredOnceUses = 0;
	for (const FWoRFiredOnceKey &Key : FiredOnceThisMatch)
	{
		FiredOnceUses += (Key.Holder == TObjectKey<AActor>(Actor)) ? 1 : 0;
	}
	UE_LOG(LogWoREffects, Display, TEXT("  Once-per-match uses spent on %s: %d"), *Actor->GetName(), FiredOnceUses);
	UE_LOG(LogWoREffects, Display, TEXT("  EP sealed by Silenced: %d, spendable: %d"), GetSealedEP(Actor), GetSpendableEP(Actor));
}

void USkillEffectManager::DebugPrintAllEffects() const
{
	UE_LOG(LogWoREffects, Display, TEXT("[SkillEffectManager] DEBUG: === ALL ACTIVE EFFECTS ==="));

	int32 TotalEffects = 0;

	for (const auto &Pair : ActiveEffects)
	{
		if (Pair.Key.IsValid())
		{
			DebugPrintEffects(Pair.Key.Get());
			TotalEffects += Pair.Value.Num();
		}
	}

	UE_LOG(LogWoREffects, Display, TEXT("[SkillEffectManager] DEBUG: Total: %d effects on %d actors"),
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
	case ESkillTrigger::OnGuard:
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

FActiveSkillEffect *USkillEffectManager::FindEffectByIdentity(AActor *Actor, int32 EffectID, int32 SourceKey)
{
	TArray<FActiveSkillEffect> *Effects = Actor ? ActiveEffects.Find(Actor) : nullptr;
	if (!Effects)
	{
		return nullptr;
	}

	return Effects->FindByPredicate([EffectID, SourceKey](const FActiveSkillEffect &Effect)
	{
		return Effect.EffectID == EffectID && Effect.SourceKey == SourceKey;
	});
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
		UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Notified TurnManager of speed change for %s"),
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

	// EffectID is set after the switch from the RESOLVED type (RandomDebuff picks one), so the
	// same cap from the same source refreshes and another source adds (D-012).
	FActiveSkillEffect Effect;
	Effect.Element = Element;

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
			Effect.bShareOfMaxHP = true; // resistance already cut the buildup that capped the bar (Q-064)
		}
		else
		{
			Effect.EffectValue = 30.0f; // fallback
		}
		// Pays out at the holder's turn start, exactly once per counted turn (D-051, Q-044c):
		// not on landing, and not at turn end.
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		Effect.bDelayFirstExecution = true;
		break;
	}

	case ESkillEffectType::DefenseDebuff:
		Effect.EffectType = ESkillEffectType::DefenseDebuff;
		Effect.EffectValue = 30.0f; // 30% defence reduction
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::SkipTurn:
		Effect.EffectType = ESkillEffectType::SkipTurn;
		Effect.EffectValue = 1.0f; // gate
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		break;

	case ESkillEffectType::SpeedDebuff:
		Effect.EffectType = ESkillEffectType::SpeedDebuff;
		Effect.EffectValue = 50.0f; // 50%
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::CritDebuff:
		Effect.EffectType = ESkillEffectType::CritChanceDebuff;
		Effect.EffectValue = CombatConstants::LIGHT_BREAK_CRIT_CHANCE_DOWN_PERCENT;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::EnergyDebuff:
		Effect.EffectType = ESkillEffectType::EnergyDrain;
		Effect.EffectValue = 100.0f; // 100% locked
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
		Effect.EffectType = Debuffs[UBattleRandomSubsystem::RandRange(this, 0, Debuffs.Num() - 1)];
		Effect.EffectValue = 30.0f; // 30%
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
	}
	break;

	// ==================== BAR-CAP GATE EFFECTS (Session X) ====================
	case ESkillEffectType::Stun:
		Effect.EffectType = ESkillEffectType::Stun;
		Effect.EffectValue = 1.0f; // gate
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		break;

	case ESkillEffectType::HealBlock:
		Effect.EffectType = ESkillEffectType::HealBlock;
		Effect.EffectValue = 1.0f; // gate
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::Silenced:
		Effect.EffectType = ESkillEffectType::Silenced;
		Effect.EffectValue = 1.0f; // gate
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	case ESkillEffectType::RandomSkill:
		Effect.EffectType = ESkillEffectType::RandomSkill;
		Effect.EffectValue = 1.0f; // gate
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		break;

	// Light break (D-030): crit chance down. Before B-112 this type fell to default and did nothing.
	case ESkillEffectType::CritChanceDebuff:
		Effect.EffectType = ESkillEffectType::CritChanceDebuff;
		Effect.EffectValue = CombatConstants::LIGHT_BREAK_CRIT_CHANCE_DOWN_PERCENT;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	// Water break (D-090): Action Speed and Spell Speed both slowed. Applied as two effects;
	// this one is Action Speed, Spell Speed is added after the switch.
	case ESkillEffectType::ActionSpeedDebuff:
		Effect.EffectType = ESkillEffectType::ActionSpeedDebuff;
		Effect.EffectValue = CombatConstants::WATER_BREAK_SPEED_SLOW_PERCENT;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		break;

	// Broken Darkness break: drains a share of max EP at the holder's turn start.
	case ESkillEffectType::DrainEnergy:
		Effect.EffectType = ESkillEffectType::DrainEnergy;
		if (UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>())
		{
			Effect.EffectValue = FMath::RoundToFloat(TargetComp->MaxEP * CombatConstants::BD_BREAK_EP_DRAIN_FRACTION);
		}
		Effect.ProcessTiming = ESkillEffectTiming::StartOfOwnTurn;
		Effect.bDelayFirstExecution = true;
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
				UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Reality Burst dealt %d (25%% MaxHP, can be lethal) to %s"),
					   BurstDamage, *Target->GetName());
			}
		}
		return; // one-shot, no FActiveSkillEffect applied
	}

	default:
		return;
	}

	// Every break lasts STATUS_CAP_TURNS of the holder's own turns (D-030, D-090), counted
	// by the one rule (FTurnSpan, D-051): it ends as the holder's turn after those starts.
	// ApplyEffect's timing rule then times a payout break (Burn, EP drain) as Q-063 says.
	Effect.StartHolderTurnSpan(CombatConstants::STATUS_CAP_TURNS);
	UTurnManager *TurnManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
	Effect.EffectID = EffectIdentity::MakeStatusCapEffectID(static_cast<uint8>(Effect.EffectType), static_cast<uint8>(Element));
	const EEffectApplicationResult Result = ApplyEffect(Target, Effect, Source, TEXT("Status Bar Trigger"), -1);

	if (Effect.EffectType == ESkillEffectType::ActionSpeedDebuff)
	{
		FActiveSkillEffect SpellSlow = Effect;
		SpellSlow.EffectType = ESkillEffectType::SpellSpeedDebuff;
		SpellSlow.EffectName = SkillEffectDisplayNames::GetDisplayName(ESkillEffectType::SpellSpeedDebuff, Element);
		SpellSlow.EffectID = EffectIdentity::MakeStatusCapEffectID(static_cast<uint8>(SpellSlow.EffectType), static_cast<uint8>(Element));
		if (ApplyEffect(Target, SpellSlow, Source, TEXT("Status Bar Trigger"), -1) == EEffectApplicationResult::Rejected)
		{
			UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Water break's Spell Speed slow refused on %s"), *Target->GetName());
		}
	}

	// Wind break (D-014, D-090): a NEW gust pushes the holder's next turn back. The same
	// attacker's refresh doesn't push again (D-012: same source refreshes, it doesn't add).
	if (Effect.EffectType == ESkillEffectType::SkipTurn && Result == EEffectApplicationResult::Applied && TurnManager)
	{
		TurnManager->DelayNextTurn(Target, CombatConstants::WIND_BREAK_DELAY_PLACES);
	}

	UE_LOG(LogWoREffects, Log, TEXT("[SkillEffectManager] Applied triggered %s to %s for %d holder turn(s)"),
		   *Effect.EffectName, *Target->GetName(), CombatConstants::STATUS_CAP_TURNS);
}