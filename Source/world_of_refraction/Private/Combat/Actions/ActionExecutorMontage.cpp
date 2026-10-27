// ActionExecutorMontage.cpp
// B-033 (move-only): UActionExecutor's fused-montage runner (montage chain, combat notifies,
// VFX array entries), moved out of ActionExecutor.cpp unchanged.

#include "Combat/Actions/ActionExecutor.h"
#include "ActionExecutorInternal.h"
#include "MotionWarpingComponent.h"
#include "WoRLog.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "AI/AIDecisionManager.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Inventory/ItemExecutor.h"
#include "Equipment/Weapons/WeaponManager.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Equipment/Rings/RingManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Rings/RingManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/GuardRules.h"
#include "Combat/Defense/LuckDodge.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Infusion/HybridSpellColors.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Equipment/Rings/RingData.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Inventory/InventoryComponent.h" // ResolveSpellTier/ResolveAbilityTier (spell-instance ii-c)
#include "Equipment/FEquipmentStatBonus.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Debug/Combat/TierGapDamageDebug.h"

#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Combat/CombatAnimInstance.h"
#include "Combat/CombatNotify.h"
#include "Animation/AnimMontage.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Combat/TurnManager.h"
#include "Combat/Mechanics/RealityBoost.h"
#include "Skills/Definitions/SupportSkillRules.h"
#include "GameFramework/Character.h"
#include "Combat/CombatControl.h"
#include "Combat/Tuning/CombatTuningData.h"

// ==================== FUSED-MONTAGE RUNNER (Stage 12) ====================

void UActionExecutor::BindCombatNotify(AActor *Actor)
{
	if (UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor))
	{
		// Remove first to prevent duplicate binding error
		CombatAnim->OnCombatNotify.RemoveDynamic(this, &UActionExecutor::OnCombatNotifyReceived);
		CombatAnim->OnCombatNotify.AddDynamic(this, &UActionExecutor::OnCombatNotifyReceived);
	}
}

void UActionExecutor::UnbindCombatNotify(AActor *Actor)
{
	if (UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor))
	{
		CombatAnim->OnCombatNotify.RemoveDynamic(this, &UActionExecutor::OnCombatNotifyReceived);
	}
}

UMotionWarpingComponent *UActionExecutor::GetOrCreateWarpComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UMotionWarpingComponent *Existing = Actor->FindComponentByClass<UMotionWarpingComponent>())
	{
		return Existing;
	}

	// Runtime fallback (the spike's pattern) — characters without an authored
	// persistent component still warp.
	UMotionWarpingComponent *Warp = NewObject<UMotionWarpingComponent>(Actor, TEXT("CombatWarp"));
	if (Warp)
	{
		Warp->RegisterComponent();
	}
	return Warp;
}

void UActionExecutor::BeginMontageChain(AActor *Actor, USkillDataBase *Skill, float PlayRate)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// The live chain advances off its OWN handles, never PendingExecutionActor /
	// GetCurrentSkillData() — on a deferred fire finalize nulls PendingExecutionActor
	// mid-Skill, which would otherwise strand the Return leg (return-skip fix A).
	ChainActor = Actor;
	ChainSkill = Skill;

	// Entry to the explicit chain (Option B). The first montage plays IMMEDIATELY
	// (here, via the entry step, which presence-skips through the remaining legs);
	// every montage-end transition is scheduled NEXT-TICK by the dispatcher (the
	// stance-resume stomp avoidance).
	//
	// Deferred FIRE (2a): the ritual cast already played as its own arm turn — the
	// fire skips the ritual leg and starts at Skill (→ Return). A normal cast
	// (delay==0 or non-ritual) runs the full chain from Ritual.
	const bool bDeferredFire = CurrentExecutionContext.IsSet() &&
							   CurrentExecutionContext->Action.bIsDeferredFire;
	PendingMontagePlayRate = PlayRate;
	if (bDeferredFire)
	{
		PlaySkillStep(Actor, Skill);
	}
	else
	{
		PlayRitualStep(Actor, Skill);
	}
}

void UActionExecutor::PlayRitualStep(AActor *Actor, USkillDataBase *Skill)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// Presence-driven: a null RitualCastMontage skips straight to the skill leg.
	if (Skill->RitualCastMontage)
	{
		MontagePhase = EMontagePhase::Ritual;
		CurrentChainMontage = Skill->RitualCastMontage; // tracked-leg identity (return-skip guard)
		UE_LOG(LogTemp, Log, TEXT("[Montage] PlayRitual %s"), *Skill->RitualCastMontage->GetName());
		PlayActionMontageOnActor(Actor, Skill->RitualCastMontage, PendingMontagePlayRate);
		return;
	}

	PlaySkillStep(Actor, Skill);
}

void UActionExecutor::PlaySkillStep(AActor *Actor, USkillDataBase *Skill)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// Always SkillMontage — bIsDeferredFire gates validation/cost, not montage
	// choice; there is no ResolveActiveMontage.
	MontagePhase = EMontagePhase::Skill;
	CurrentChainMontage = Skill->SkillMontage; // tracked-leg identity (return-skip guard)
	UE_LOG(LogTemp, Log, TEXT("[Montage] PlaySkill %s"),
		   Skill->SkillMontage ? *Skill->SkillMontage->GetName() : TEXT("None"));
	PlayActionMontageOnActor(Actor, Skill->SkillMontage, PendingMontagePlayRate);
}

void UActionExecutor::PlayReturnStep(AActor *Actor, USkillDataBase *Skill)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// Presence-driven: no ReturnMontage → finalize now (finalize-after-skill). A caster that died
	// mid-skill (reflect, a counter) doesn't walk its corpse back either (B-067).
	const UCharacterDataComponent *ActorComp = GetCharacterDataComponent(Actor);
	if (!Skill->ReturnMontage || (ActorComp && !ActorComp->bIsAlive))
	{
		FinishMontageChain(Actor);
		return;
	}

	MontagePhase = EMontagePhase::Return;

	// Warp return leg (W1): retarget WarpTarget to the origin snapshot — a
	// ReturnMontage with a warp window travels back to the grid slot; without one
	// it plays in place. Must run BEFORE the montage plays. The snapshot is read
	// from the runner's own fields now (SC-D) — same values, same timing as the
	// component round-trip it replaces.
	if (bHasGridPosition)
	{
		if (UMotionWarpingComponent *Warp = GetOrCreateWarpComponent(Actor))
		{
			Warp->AddOrUpdateWarpTargetFromLocationAndRotation(
				CombatConstants::WARP_TARGET_NAME, GridPosition, GridRotation);
			UE_LOG(LogTemp, Log, TEXT("[Warp] return target %s"),
				   *GridPosition.ToCompactString());
		}
	}

	CurrentChainMontage = Skill->ReturnMontage; // tracked-leg identity (return-skip guard)
	UE_LOG(LogTemp, Log, TEXT("[Montage] PlayReturn %s"), *Skill->ReturnMontage->GetName());
	PlayActionMontageOnActor(Actor, Skill->ReturnMontage, 1.0f);
}

void UActionExecutor::FinishArmTurn(AActor *Actor)
{
	// 2b arm completion — the ritual cast (the arm turn) has ended or was
	// interrupted. NO skill effects, NO damage, NO defense gating, NO chain: the
	// costs were paid and the ritual queued at arm. Unbind the spine, release the
	// context the arm held open (same end-state the synchronous arm produced so
	// the next-action guard is clean), and fire the stashed armed-success callback
	// ONCE — that ends the turn.
	bArmingRitual = false;

	if (Actor)
	{
		UnbindActionAnimationEnd(Actor); // clears bWaitingForAnimationEnd
		UnbindCombatNotify(Actor);
	}

	CurrentExecutionContext.Reset();

	// Execute ends the turn, and the advance can fire a ready Execution turn that
	// RE-BINDS AsyncActionCallback inside this call. Unbinding after Execute wiped
	// that fresh binding (orchestrator stuck in bWaitingForAsyncAction). Same
	// capture-clear-run guard as CompleteAsyncActionFinal.
	FOnActionComplete PendingCallback = AsyncActionCallback;
	AsyncActionCallback.Unbind();
	if (PendingCallback.IsBound())
	{
		// Copy: a nested action launched from the callback may overwrite the member.
		const FActionResult ArmResult = PendingFinalResult;
		PendingCallback.Execute(ArmResult);
	}
}

void UActionExecutor::FinishMontageChain(AActor *Actor)
{
	// Last leg done — close the animation phase. bWaitingForAnimationEnd is cleared
	// HERE (inside UnbindActionAnimationEnd) and nowhere else, so finalize can only
	// run after the final montage. The notify spine is unbound here too.
	MontagePhase = EMontagePhase::Done;
	CurrentChainMontage = nullptr; // chain over - drop the tracked-leg identity
	ChainActor = nullptr;		   // chain over - drop the chain-owned handles
	ChainSkill = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Action animation ended - finalizing"));

	// Stage 1 "later of" close — catch-all for the INTERRUPTED path: an interrupted Skill
	// montage routes here (via the bInterrupted branch) WITHOUT passing the Skill-end case,
	// so without this an interrupted attack's count-based windows would hang to the 8s
	// failsafe. Idempotent on the normal path: Skill-end already drained the entries.
	if (CurrentExecutionContext.IsSet())
	{
		ReconcileCountBasedDefenseWindows(*CurrentExecutionContext, GetDefenseSystem());
	}

	// Montage-end facing reassert — nearest living enemy (arena center when none). Not for a corpse (B-067).
	const UCharacterDataComponent *FacingComp = Actor ? GetCharacterDataComponent(Actor) : nullptr;
	if (Actor && (!FacingComp || FacingComp->bIsAlive))
	{
		const UGameInstance *GI = GetWorld()->GetGameInstance();
		UCombatGridSubsystem *Grid = GI ? GI->GetSubsystem<UCombatGridSubsystem>() : nullptr;
		if (Grid)
		{
			Grid->UpdateActorFacing(Actor, CachedArenaCenter);
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Restored facing for %s after animation"),
				   *Actor->GetName());
		}
	}

	// Unbind action animation + runner notify spine
	if (Actor)
	{
		UnbindActionAnimationEnd(Actor);
		UnbindCombatNotify(Actor);
	}
	else
	{
		// B-067: the actor is gone, but the wait must still end or the action hangs to the failsafe.
		bWaitingForAnimationEnd = false;
	}

	// Cleanup spell notify binding
	if (PendingSpellCaster)
	{
		UnbindSpellNotify(PendingSpellCaster);
		ClearPendingSpellData();
	}

	// Animation done — gate on defenses too. TryFinalize fires only if defenses already resolved.
	TryFinalizeAsyncAction();
}

const FSkillVFXEntry *UActionExecutor::GetVFXEntryByRole(const USkillDataBase *Skill, EVFXRole Role)
{
	if (!Skill)
	{
		return nullptr;
	}
	for (const FSkillVFXEntry &Entry : Skill->VFXArray)
	{
		// Null-asset entries don't mask the loose-field fallback.
		if (Entry.Role == Role && !Entry.VFX.IsNull())
		{
			return &Entry;
		}
	}
	return nullptr;
}

USkillDataBase *UActionExecutor::GetCurrentSkillData() const
{
	if (!CurrentExecutionContext.IsSet())
	{
		return nullptr;
	}

	const FAction &Action = CurrentExecutionContext->Action;
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		return Action.SpellData;
	case EActionType::Ability:
		return ResolveActionSkill(Action);
	default:
		return nullptr;
	}
}

void UActionExecutor::OnCombatNotifyReceived(ECombatNotifyFamily Family, int32 Index)
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		return; // stale notify outside an action
	}

	switch (Family)
	{
	case ECombatNotifyFamily::VFX:
	{
		USkillDataBase *Skill = GetCurrentSkillData();
		if (!Skill)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX notify %d with no skill data — skipping"), Index);
			return;
		}
		if (!Skill->VFXArray.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX index %d out of range (%d entries)"),
				   Index, Skill->VFXArray.Num());
			return;
		}
		SpawnVFXArrayEntry(Skill->VFXArray[Index], Index);
		return;
	}

	case ECombatNotifyFamily::Impact:
	{
		// Read ChainActor, not PendingExecutionActor: the latter is nulled by the
		// prior action's CompleteAsyncActionFinal during a deferred fire (re-entrant
		// clobber), so it can be NULL while this action's skill montage is still
		// firing hits. ChainActor is the stable handle, valid through the whole
		// montage chain — same source OnActionAnimationEnded advances off.
		AActor *Executor = ChainActor.Get();
		USkillDataBase *Skill = GetCurrentSkillData();
		const int32 HitCount = Skill ? Skill->HitCount : 0;
		// Fires into the void this stage — Stage 2's per-impact resolver binds OnImpactFrame.
		UE_LOG(LogTemp, Log, TEXT("[ImpactFrame] impact %d of %d for %s"),
			   Index, HitCount, *GetNameSafe(Executor));
		OnImpactFrame.Broadcast(Executor, Index);

		// Per-impact apply (Stage 2) + count-based "later of" close (Stage 1), melee.
		// Each landed impact APPLIES ITS DAMAGE at its frame (ResolvedDamageSplit[Index] of
		// the stashed reduced total) and increments the count. A defender closes only once it
		// has taken ALL its impacts AND the hitting animation has finished (bAnimationFinished);
		// for melee the animation is normally the LATER signal, so the close usually happens at
		// Skill-end (ReconcileCountBasedDefenseWindows) — this branch handles the symmetric
		// "impact after animation" case. ExpectedImpacts==0 ⇒ timer-close window (non-attack) →
		// left untouched. One swing hits all current defenders (melee cleave).
		// Three phases, all DEFERRED out of the iteration: ApplyOneImpact can broadcast
		// OnTargetKilled (subscribers may mutate PendingDefenses) and CloseDefenseWindow
		// re-entrantly removes the entry — neither is safe mid-iteration.
		if (UDefenseSystem *DefenseSys = GetDefenseSystem())
		{
			const bool bAnimationFinished = CurrentExecutionContext->bAnimationFinished;
			TArray<AActor *> DefendersToApply;
			TArray<AActor *> DefendersToClose;
			for (auto &Pair : CurrentExecutionContext->PendingDefenses)
			{
				FPendingDefenseContext &Ctx = Pair.Value;
				if (Ctx.ExpectedImpacts <= 0)
				{
					continue; // timer-close window — not count-based
				}
				AActor *Defender = Pair.Key.Get();
				if (!Defender)
				{
					continue;
				}
				// In-order sanity — the close-time tail [ImpactsLanded, HitCount) assumes
				// contiguous in-order notifies (authoring contract: 0..HitCount-1).
				if (Index != Ctx.ImpactsLanded)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Stage2] impact index %d != drain pointer %d for %s — out-of-order/gapped notify (authoring-contract)"),
						   Index, Ctx.ImpactsLanded, *Defender->GetName());
				}
				DefendersToApply.Add(Defender);
				if (++Ctx.ImpactsLanded >= Ctx.ExpectedImpacts && bAnimationFinished)
				{
					DefendersToClose.Add(Defender);
				}
			}
			// Resolve THIS impact's defense (split-then-reduce + match-and-consume) and apply
			// its reduced slice. One impact frame = one moment, so all defenders share a single
			// ImpactTime. Done after iteration — see above.
			const double ImpactTime = UDefenseSystem::GetDefenseClockSeconds(this);
			for (AActor *Defender : DefendersToApply)
			{
				FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Defender);
				if (!Ctx)
				{
					continue; // removed by a prior impact's death broadcast
				}
				// Melee difficulty source: the per-impact-ordinal table (IsValidIndex guard → Easy ×1.0).
				const FDefenseDifficultyTriple MeleeDifficulty =
					CurrentExecutionContext->ResolvedDifficulty.IsValidIndex(Index)
						? CurrentExecutionContext->ResolvedDifficulty[Index]
						: FDefenseDifficultyTriple();
				ResolveImpactDefense(Defender, Index, ImpactTime, MeleeDifficulty);
				StashHitFlags(*Ctx, CurrentExecutionContext->ResolvedHitFlags, Index);
				ApplyOneImpact(Ctx->Attacker.Get(), Defender, *Ctx, Index, /*bDamageIsPerImpact=*/true);
			}
			// Hybrid B2 interrupt: if THIS impact was authored interruptable and EVERY targeted defender
			// parried/dodged it, abort the rest of the attack. Checked HERE — after all defenders resolved
			// this impact, BEFORE the closes remove their contexts — so AllInterruptSucceeded sees the full
			// set. Single-target degenerates to the one defender's outcome (the grab).
			if (CurrentExecutionContext->ResolvedHitFlags.IsValidIndex(Index) &&
				CurrentExecutionContext->ResolvedHitFlags[Index].bInterruptable &&
				AllInterruptSucceeded())
			{
				InterruptAsyncAction(ChainActor.Get(), ChainSkill);
				return;
			}
			for (AActor *Defender : DefendersToClose)
			{
				UE_LOG(LogTemp, Log, TEXT("[ImpactFrame] impacts + animation done for %s — closing window"),
					   *GetNameSafe(Defender));
				DefenseSys->CloseDefenseWindow(Defender);
			}
		}
		return;
	}

	case ECombatNotifyFamily::Cast:
	{
		// Resolve skill + caster from execution context (works for spells AND
		// abilities/attacks). For spells these match the pending-spell stash
		// exactly (same skill pointer, same caster, AttackElement == Spell->Element);
		// for non-spell skills they're the only source. Size/targets/damage still
		// come from the pending stash — reset to clean defaults (1.0 / empty / 0)
		// after every spell, so abilities get the neutral size multiplier and let
		// DispatchSpellCast resolve targets/damage from the context.
		USkillDataBase *CurrentSkill = GetCurrentSkillData();
		AActor *CurrentCaster = PendingExecutionActor;
		ESpellElement ActionElement = CurrentExecutionContext.IsSet()
										  ? CurrentExecutionContext->PartialResult.AttackElement
										  : ESpellElement::None;

		if (!CurrentSkill || !CurrentCaster)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] Cast notify %d — no active skill or caster"), Index);
			return;
		}

		if (CurrentSkill->CastArray.IsEmpty())
		{
			// Empty-CastArray fallback: the loose-field dispatch (delta-
			// serialization limit — fully-default spells migrated no entry).
			// Spell-only: SpawnSpellVFX reads USpellData loose fields, and
			// abilities authoring Cast VFX always carry a CastArray entry, so
			// they never reach this branch.
			if (USpellData *AsSpell = Cast<USpellData>(CurrentSkill))
			{
				SpawnSpellVFX(CurrentCaster, AsSpell, PendingSpellSize,
							  PendingSpellTargets, PendingSpellDamage);
			}
			return;
		}
		if (!CurrentSkill->CastArray.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] Cast index %d out of range (%d entries)"),
				   Index, CurrentSkill->CastArray.Num());
			return;
		}
		DispatchSpellCast(CurrentCaster, CurrentSkill, ActionElement,
						  CurrentSkill->CastArray[Index], PendingSpellSize,
						  PendingSpellTargets, PendingSpellDamage, Index);
		return;
	}

	case ECombatNotifyFamily::SFX:
		// No production SFX array yet (deferred).
		UE_LOG(LogTemp, Verbose, TEXT("[Runner] SFX notify %d — no SFX array (deferred)"), Index);
		return;
	}
}

void UActionExecutor::SpawnVFXArrayEntry(const FSkillVFXEntry &Entry, int32 Index)
{
	AActor *Caster = PendingExecutionActor;
	if (!Caster)
	{
		return;
	}

	// Soft ref — the runner resolves at spawn time.
	UNiagaraSystem *System = Entry.VFX.LoadSynchronous();
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX[%d] '%s' has no asset"), Index, *Entry.Label);
		return;
	}

	UNiagaraComponent *Spawned = nullptr;

	// CasterSocket attaches (follows the hand through the montage) — spike-proven.
	if (Entry.Attach == EVFXAttach::CasterSocket)
	{
		ACharacter *Character = Cast<ACharacter>(Caster);
		if (Character && Character->GetMesh() && Character->GetMesh()->DoesSocketExist(Entry.SocketName))
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System, Character->GetMesh(), Entry.SocketName,
				FVector::ZeroVector, FRotator::ZeroRotator, FVector(Entry.Scale),
				EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
		}
	}

	if (!Spawned)
	{
		FVector SpawnLoc;
		if (!ResolveVFXAttachLocation(Entry.Attach, Entry.SocketName, Index, SpawnLoc))
		{
			return; // warned inside
		}
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), System, SpawnLoc, FRotator::ZeroRotator, FVector(Entry.Scale),
			true, true);
	}

	// Element tint — the action's resolved element (set by all three paths),
	// BD-aware like the legacy spell VFX spawns.
	if (Spawned && Entry.bElementTinted)
	{
		UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
		const bool bIsBD = BDManager && BDManager->IsTransformed();
		const ESpellElement Element = CurrentExecutionContext->PartialResult.AttackElement;
		const FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Element, bIsBD);
		Spawned->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
		Spawned->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
	}

	UE_LOG(LogTemp, Log, TEXT("[Runner] VFX[%d] '%s' spawned (attach=%d, scale=%.2f)"),
		   Index, *Entry.Label, (int32)Entry.Attach, Entry.Scale);
}

bool UActionExecutor::ResolveVFXAttachLocation(EVFXAttach Attach, FName SocketName, int32 Index, FVector &OutLoc) const
{
	AActor *Caster = PendingExecutionActor;
	if (!Caster)
	{
		return false;
	}

	// First valid action target (Target/ImpactPoint modes).
	AActor *Target = nullptr;
	if (CurrentExecutionContext.IsSet())
	{
		for (AActor *Candidate : CurrentExecutionContext->Action.Targets)
		{
			if (IsValid(Candidate))
			{
				Target = Candidate;
				break;
			}
		}
	}

	switch (Attach)
	{
	case EVFXAttach::Target:
		if (!Target)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX[%d] wants Target, none available — skipping"), Index);
			return false;
		}
		OutLoc = Target->GetActorLocation();
		return true;

	case EVFXAttach::ImpactPoint:
		// No warp yet — impacts land on the target, not under the caster
		// (the spike mapped this to caster loc because it WAS warped onto
		// the target; production isn't until the movement arc).
		OutLoc = Target ? Target->GetActorLocation() : Caster->GetActorLocation();
		return true;

	case EVFXAttach::CasterSocket:
		// Attached spawn already failed (missing socket) — actor-location fallback.
		UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX[%d] socket '%s' invalid, using caster location"),
			   Index, *SocketName.ToString());
		OutLoc = Caster->GetActorLocation();
		return true;

	case EVFXAttach::Caster:
	case EVFXAttach::World: // TODO: no authored world point yet — caster loc
	default:
		OutLoc = Caster->GetActorLocation();
		return true;
	}
}
