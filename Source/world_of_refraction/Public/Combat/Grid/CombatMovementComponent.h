// CombatMovementComponent.h
// Holds the per-action origin snapshot (warp origin) + Idle/Executing state.
// Position is owned by root-motion montages + warp (W3).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatMovementComponent.generated.h"

class UCombatGridSubsystem;

// ==================== MOVEMENT STATE ====================

UENUM(BlueprintType)
enum class ECombatMovementState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Executing UMETA(DisplayName = "Executing")
};

/**
 * Combat Movement Component
 * Holds the per-action origin snapshot (grid position + rotation) the runner's
 * Motion Warping return leg targets, plus the Idle/Executing state. Position is
 * owned by root-motion montages + warp (W3) — this component no longer moves
 * the actor; it only records where the action started.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCombatMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatMovementComponent();

    // ==================== EXECUTION STATE ====================

    /** Enter the Executing state and snapshot the pre-action pose (grid
     *  position + rotation, arena center, target) — the warp origin the
     *  runner's ReturnMontage leg targets. Facing is NOT set here —
     *  execution-start facing is owned by ActionExecutor::BeginSkillExecution. */
    void EnterExecutingState(AActor *Target, const FVector &ArenaCenter);

    /** Slim per-action reset (W3): clears CurrentTarget and returns to Idle
     *  after the action's montage chain (including the warp return leg) has
     *  finished. Called by ActionExecutor at finalize. No travel — the runner's
     *  root-motion montages already moved the actor. */
    UFUNCTION(BlueprintCallable, Category = "Combat Movement")
    void OnActionExecutionComplete();

    // ==================== STATE QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    ECombatMovementState GetMovementState() const { return MovementState; }

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    bool IsIdle() const { return MovementState == ECombatMovementState::Idle; }

    /** Origin snapshot accessors (W1) — the pre-action pose the return warp
     *  targets. Valid while HasGridPosition() is true. */
    bool HasGridPosition() const { return bHasGridPosition; }
    FVector GetGridPosition() const { return GridPosition; }
    FRotator GetGridRotation() const { return GridRotation; }

private:
    // ==================== INTERNAL STATE ====================

    UPROPERTY()
    ECombatMovementState MovementState = ECombatMovementState::Idle;

    /** Target of the current action — cleared by the slim reset. */
    UPROPERTY()
    AActor *CurrentTarget = nullptr;

    /** Pre-action position — the warp origin the return leg targets (W1). */
    FVector GridPosition = FVector::ZeroVector;

    /** Pre-action rotation — the return-warp target rotation (W1). */
    FRotator GridRotation = FRotator::ZeroRotator;

    /** Arena center cached at execution start (facing fallback). */
    FVector CachedArenaCenter = FVector::ZeroVector;

    /** Has a valid origin snapshot been stored? */
    bool bHasGridPosition = false;
};