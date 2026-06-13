// CombatMovementComponent.cpp
// Per-action origin snapshot + Idle/Executing state. Position is owned by
// root-motion montages + warp (W3); this component no longer moves the actor.

#include "Combat/Grid/CombatMovementComponent.h"
#include "GameFramework/Actor.h"

UCombatMovementComponent::UCombatMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatMovementComponent::EnterExecutingState(AActor *Target, const FVector &ArenaCenter)
{
    if (!GetOwner())
    {
        return;
    }

    // Pre-action origin snapshot — records the pose the runner's ReturnMontage
    // warp targets. Facing is owned by BeginSkillExecution; rotation is the W1
    // return-warp target rotation.
    GridPosition = GetOwner()->GetActorLocation();
    GridRotation = GetOwner()->GetActorRotation();
    bHasGridPosition = true;
    CachedArenaCenter = ArenaCenter;
    CurrentTarget = Target;
    MovementState = ECombatMovementState::Executing;
}

void UCombatMovementComponent::OnActionExecutionComplete()
{
    // W3 slim reset: the montage chain (PlayReturnStep + warp) already returned
    // the actor inside the animation phase — there is no travel to start. Just
    // clear per-action state so the next action's snapshot starts clean. The
    // origin snapshot (GridPosition/GridRotation) is left to be overwritten by
    // the next EnterExecutingState.
    CurrentTarget = nullptr;
    MovementState = ECombatMovementState::Idle;
}