// CombatMovementComponent.h
// Handles character movement during combat actions (approach to target, return to grid)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Grid/ECombatMovementType.h"
#include "Character/MovementData.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "CombatMovementComponent.generated.h"

class UCombatGridSubsystem;
class UCharacterDataComponent;

// ==================== DELEGATES ====================

/** Broadcast when movement completes (used for both approach and return phases —
 *  ActionExecutor switches its bind target between the two via the
 *  MovementState query). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovementComplete);

/** Broadcast when movement is interrupted/cancelled */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovementCancelled);

// ==================== MOVEMENT STATE ====================

UENUM(BlueprintType)
enum class ECombatMovementState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Approaching UMETA(DisplayName = "Approaching"),
    Executing UMETA(DisplayName = "Executing"),
    Returning UMETA(DisplayName = "Returning")
};

/**
 * Combat Movement Component
 * Manages character movement during combat actions
 *
 * Flow:
 * 1. StartApproach() - Move toward target
 * 2. OnMovementComplete fires - Action system executes attack/ability/spell
 * 3. StartReturn() - Move back to grid position
 * 4. OnMovementComplete fires again - Turn ends
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCombatMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatMovementComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

    // ==================== MOVEMENT CONTROL ====================

    /**
     * Start Movement toward target
     * @param Target The target actor to approach
     * @param Approach Approach data asset (nullptr = no movement)
     * @param ExecutionRange Distance from target to stop
     * @param ArenaCenter Center of arena for grid calculations
     * @param InActionMods Per-action stat modifiers; ActionSpeed contribution
     *                     scales approach + return speed for this action lifecycle.
     */
    void StartApproach(AActor *Target, UMovementData *Approach, float ExecutionRange, const FVector &ArenaCenter, const FActionStatModifiers &InActionMods = FActionStatModifiers());

    /** Enter the Executing state without an approach leg (SC2.5) — makes the
     *  same pre-action state snapshot StartApproach makes (grid position,
     *  arena center, target), so OnActionExecutionComplete/StartReturn behave
     *  identically whether or not movement ran. Facing is NOT set here —
     *  execution-start facing is owned by ActionExecutor::BeginSkillExecution. */
    void EnterExecutingState(AActor *Target, const FVector &ArenaCenter);

    /**
     * Start return movement back to grid position
     * Must have a stored grid position from StartApproach
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Movement")
    void StartReturn();

    /**
     * Cancel any current movement and return to grid immediately
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Movement")
    void CancelMovement();

    /**
     * Signal that action execution is complete, ready to return
     * Called by ActionExecutor after attack/ability/spell finishes
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Movement")
    void OnActionExecutionComplete();

    /** Face the current target (or arena center if no target) */
    UFUNCTION(BlueprintCallable, Category = "Combat Movement")
    void FaceCurrentTarget();
    // ==================== STATE QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    ECombatMovementState GetMovementState() const { return MovementState; }

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    bool IsMoving() const { return MovementState == ECombatMovementState::Approaching || MovementState == ECombatMovementState::Returning; }

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    bool IsIdle() const { return MovementState == ECombatMovementState::Idle; }

    // ==================== EVENTS ====================

    /** Fires when approach OR return completes — the active phase is the
     *  MovementState at broadcast time. Consumers (e.g. ActionExecutor)
     *  subscribe to drive action-execution then turn-end transitions. */
    UPROPERTY(BlueprintAssignable, Category = "Combat Movement|Events")
    FOnMovementComplete OnMovementComplete;

    /** Fires when movement is cancelled */
    UPROPERTY(BlueprintAssignable, Category = "Combat Movement|Events")
    FOnMovementCancelled OnMovementCancelled;

    // ==================== CONFIGURATION ====================

    /** Base action speed (multiplied by character's ActionSpeed stat) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    float BaseSpeed = 400.0f;

    /** Return speed multiplier (1.0 = same as approach) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    float ReturnSpeedMultiplier = 1.0f;

    /** Distance threshold to consider "arrived" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    float ArrivalThreshold = 10.0f;

    /** Default return animation (used if no approach-specific one) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    UAnimMontage *DefaultReturnMontage = nullptr;

    /** Default approach animation (run/dash forward) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    UAnimMontage *DefaultMovementMontage = nullptr;

    /** Whether to face target during return (true) or face movement direction (false) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    bool bFaceTargetDuringReturn = true;

protected:
    virtual void BeginPlay() override;

private:
    // ==================== INTERNAL STATE ====================

    UPROPERTY()
    ECombatMovementState MovementState = ECombatMovementState::Idle;

    UPROPERTY()
    UMovementData *CurrentMovementData = nullptr;

    UPROPERTY()
    AActor *CurrentTarget = nullptr;

    /** Per-action stat modifiers active for the current approach/return.
     *  Set by StartApproach, read by approach + return speed reads,
     *  cleared on return completion or cancellation. ActionSpeed sub-stat
     *  scales both speed reads. */
    UPROPERTY()
    FActionStatModifiers ActiveActionMods;

    /** Currently playing movement montage */
    UPROPERTY()
    UAnimMontage *CurrentMovementMontage = nullptr;

    /** Get return animation to play */
    UAnimMontage *GetReturnMontage() const;

    /** Position to return to after action */
    FVector GridPosition = FVector::ZeroVector;

    /** Current destination */
    FVector TargetPosition = FVector::ZeroVector;

    /** Arena center for grid calculations */
    FVector CachedArenaCenter = FVector::ZeroVector;

    /** Has a valid grid position been stored? */
    bool bHasGridPosition = false;

    /** Cached facing direction for return phase */
    FVector ReturnFacingDirection = FVector::ZeroVector;

    // ==================== CACHED REFERENCES ====================

    UPROPERTY()
    UCharacterDataComponent *CharacterDataComp = nullptr;

    // ==================== MOVEMENT HELPERS ====================

    /** Calculate final action speed based on approach type and stats */
    float CalculateActionSpeed() const;

    /** Move toward destination, returns true if arrived */
    bool MoveToward(const FVector &Destination, float Speed, float DeltaTime);

    /** Instantly teleport to position */
    void TeleportTo(const FVector &Position);

    /** Update actor rotation to face movement direction */
    void UpdateFacingDirection(const FVector &Direction);

    /** Complete approach phase */
    void CompleteApproach();

    /** Complete return phase */
    void CompleteReturn();

    // ==================== HELPER FUNCTIONS ====================

    /** Play movement animation montage */
    void PlayMovementMontage(UAnimMontage *Montage);

    /** Stop current movement montage */
    void StopMovementMontage();
};