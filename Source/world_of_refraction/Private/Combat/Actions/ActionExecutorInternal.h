// ActionExecutorInternal.h
// B-033: file-local helpers shared by UActionExecutor's .cpp files (ActionExecutor.cpp and the
// split-out ActionExecutorMontage.cpp / ActionExecutorCosts.cpp). Moved unchanged from
// ActionExecutor.cpp; inline (was static) so each .cpp, and a unity build, sees one definition.
// Private to the module: not under Public/.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Skills/Definitions/SkillDataBase.h"

// The unified skill pointer for an attack/ability action. The legacy AbilityData/AttackData pointers
// were removed (Cluster 4); SkillData is populated at every construction site. Kept as the single named
// read-point that the call sites go through. Spells use SpellData (a separate path).
inline USkillDataBase *ResolveActionSkill(const FAction &Action)
{
	return Action.SkillData;
}

// Stash the current impact's flags onto the per-target context (read index-agnostically by ApplyOneImpact):
// melee passes ResolvedHitFlags + ImpactIndex; spell passes ResolvedCastFlags + CastEntryIndex.
inline void StashHitFlags(FPendingDefenseContext &Ctx, const TArray<FResolvedHitFlags> &Table, int32 Index)
{
	const FResolvedHitFlags F = Table.IsValidIndex(Index) ? Table[Index] : FResolvedHitFlags();
	Ctx.bIgnoreDamage = F.bIgnoreDamage;
	Ctx.bIgnoreStatus = F.bIgnoreStatus;
	Ctx.bInterruptable = F.bInterruptable;
}

// Shared reconciliation (Stage 1): mark the action's animation finished and force-close
// every count-based (ExpectedImpacts>0, melee) defense window — the "animation done" half
// of the "later of (impacts-done, animation-done)" close. Damage is lumped at
// CloseDefenseWindow (Stage 2 splits per-impact), so reconcile == close: complete counts
// close normally; incomplete ones (fewer/missing Impact notifies, or an interrupted montage)
// reconcile here instead of hanging on the 8s failsafe. Called from BOTH the clean Skill-end
// branch and FinishMontageChain (which the interrupted path routes through, skipping Skill-end).
// Idempotent: by the time FinishMontageChain runs on the normal path, Skill-end already drained
// the count-based entries, so this is a no-op. Collect-then-close: CloseDefenseWindow
// re-entrantly removes the PendingDefenses entry, so we must not close mid-iteration.
inline void ReconcileCountBasedDefenseWindows(FActionExecutionContext &ExecContext, UDefenseSystem *DefenseSys)
{
	ExecContext.bAnimationFinished = true;
	if (!DefenseSys)
	{
		return;
	}

	TArray<AActor *> DefendersToClose;
	for (auto &Pair : ExecContext.PendingDefenses)
	{
		if (Pair.Value.ExpectedImpacts > 0)
		{
			if (AActor *Defender = Pair.Key.Get())
			{
				DefendersToClose.Add(Defender);
			}
		}
	}

	for (AActor *Defender : DefendersToClose)
	{
		UE_LOG(LogTemp, Log, TEXT("[ImpactFrame] animation-end reconcile — closing window for %s"),
			   *GetNameSafe(Defender));
		DefenseSys->CloseDefenseWindow(Defender);
	}
}
