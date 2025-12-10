// CombatMovementComponent.h
// Handles character movement during combat actions (approach to target, return to grid)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ECombatApproachType.h"
#include "CombatMovementComponent.generated.h"

class UCombatGridSubsystem;
class UCharacterDataComponent;

// ==================== DELEGATES ====================

/** Broadcast when approach movement completes (ready to execute action) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnApproachComplete);

/** Broadcast when return movement completes (back at grid position) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReturnComplete);

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
 * 2. OnApproachComplete fires - Action system executes attack/ability/spell
 * 3. StartReturn() - Move back to grid position
 * 4. OnReturnComplete fires - Turn ends
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
     * Start approach movement toward target
     * @param Target The target actor to approach
     * @param ApproachType How to approach (Direct, Dash, Teleport, None)
     * @param ExecutionRange Distance from target to stop
     * @param ArenaCenter Center of arena for grid calculations
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Movement")
    void StartApproach(AActor *Target, ECombatApproachType ApproachType, float ExecutionRange, const FVector &ArenaCenter);

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

    // ==================== STATE QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    ECombatMovementState GetMovementState() const { return MovementState; }

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    bool IsMoving() const { return MovementState == ECombatMovementState::Approaching || MovementState == ECombatMovementState::Returning; }

    UFUNCTION(BlueprintPure, Category = "Combat Movement")
    bool IsIdle() const { return MovementState == ECombatMovementState::Idle; }

    // ==================== EVENTS ====================

    /** Fires when approach completes - action system should execute attack/ability/spell */
    UPROPERTY(BlueprintAssignable, Category = "Combat Movement|Events")
    FOnApproachComplete OnApproachComplete;

    /** Fires when return completes - turn can end */
    UPROPERTY(BlueprintAssignable, Category = "Combat Movement|Events")
    FOnReturnComplete OnReturnComplete;

    /** Fires when movement is cancelled */
    UPROPERTY(BlueprintAssignable, Category = "Combat Movement|Events")
    FOnMovementCancelled OnMovementCancelled;

    // ==================== CONFIGURATION ====================

    /** Base movement speed (multiplied by character's MovementSpeed stat) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    float BaseSpeed = 400.0f;

    /** Return speed multiplier (1.0 = same as approach) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    float ReturnSpeedMultiplier = 1.0f;

    /** Distance threshold to consider "arrived" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config")
    float ArrivalThreshold = 10.0f;

protected:
    virtual void BeginPlay() override;

private:
    // ==================== INTERNAL STATE ====================

    UPROPERTY()
    ECombatMovementState MovementState = ECombatMovementState::Idle;

    UPROPERTY()
    ECombatApproachType CurrentApproachType = ECombatApproachType::None;

    UPROPERTY()
    AActor *CurrentTarget = nullptr;

    /** Position to return to after action */
    FVector GridPosition = FVector::ZeroVector;

    /** Current destination */
    FVector TargetPosition = FVector::ZeroVector;

    /** Arena center for grid calculations */
    FVector CachedArenaCenter = FVector::ZeroVector;

    /** Has a valid grid position been stored? */
    bool bHasGridPosition = false;

    // ==================== CACHED REFERENCES ====================

    UPROPERTY()
    UCharacterDataComponent *CharacterDataComp = nullptr;

    // ==================== MOVEMENT HELPERS ====================

    /** Calculate final movement speed based on approach type and stats */
    float CalculateMovementSpeed(ECombatApproachType ApproachType) const;

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
};