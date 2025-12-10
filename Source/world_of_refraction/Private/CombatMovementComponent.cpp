// CombatMovementComponent.cpp
// Combat movement implementation

#include "CombatMovementComponent.h"
#include "ApproachData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "GameFramework/Actor.h"

UCombatMovementComponent::UCombatMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick when moving
}

void UCombatMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache character data component
    if (AActor *Owner = GetOwner())
    {
        CharacterDataComp = Owner->FindComponentByClass<UCharacterDataComponent>();
    }
}

void UCombatMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    switch (MovementState)
    {
    case ECombatMovementState::Approaching:
    {
        float Speed = CalculateMovementSpeed();
        if (MoveToward(TargetPosition, Speed, DeltaTime))
        {
            CompleteApproach();
        }
        break;
    }

    case ECombatMovementState::Returning:
    {
        float Speed = BaseSpeed * ReturnSpeedMultiplier; // Return is always base speed
        if (MoveToward(GridPosition, Speed, DeltaTime))
        {
            CompleteReturn();
        }
        break;
    }

    default:
        // Not moving, disable tick
        SetComponentTickEnabled(false);
        break;
    }
}

// ==================== MOVEMENT CONTROL ====================

void UCombatMovementComponent::StartApproach(AActor *Target, UApproachData *Approach, float ExecutionRange, const FVector &ArenaCenter)
{
    if (!GetOwner())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatMovement] No owner actor"));
        return;
    }

    // Store grid position before moving
    GridPosition = GetOwner()->GetActorLocation();
    bHasGridPosition = true;
    CachedArenaCenter = ArenaCenter;

    CurrentTarget = Target;
    CurrentApproachData = Approach;

    // Handle no approach data (ranged) - no movement needed
    if (!Approach)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: No approach data, ranged action"),
               *GetOwner()->GetName());

        // Face target but don't move
        if (Target)
        {
            FVector Direction = Target->GetActorLocation() - GetOwner()->GetActorLocation();
            Direction.Z = 0;
            UpdateFacingDirection(Direction);
        }

        // Immediately complete approach
        MovementState = ECombatMovementState::Executing;
        OnApproachComplete.Broadcast();
        return;
    }

    ECombatApproachType ApproachType = Approach->ApproachType;

    // Calculate target position (ExecutionRange away from target)
    if (Target)
    {
        FVector DirectionToTarget = Target->GetActorLocation() - GridPosition;
        DirectionToTarget.Z = 0;
        DirectionToTarget.Normalize();

        TargetPosition = Target->GetActorLocation() - (DirectionToTarget * ExecutionRange);
        TargetPosition.Z = GridPosition.Z; // Keep same height
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatMovement] No target for approach"));
        TargetPosition = GridPosition;
    }

    // Handle Teleport - instant movement
    if (ApproachType == ECombatApproachType::Teleport)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Teleporting to target (%s)"),
               *GetOwner()->GetName(), *Approach->ApproachName);

        // TODO: Play DepartureVFX, DepartureSound, ApproachMontage (vanish)

        TeleportTo(TargetPosition);

        if (Target)
        {
            FVector Direction = Target->GetActorLocation() - TargetPosition;
            Direction.Z = 0;
            UpdateFacingDirection(Direction);
        }

        // TODO: Play ArrivalVFX, ArrivalSound, ArrivalMontage (appear)

        // Immediately complete approach
        MovementState = ECombatMovementState::Executing;
        OnApproachComplete.Broadcast();
        return;
    }

    // Start movement (Direct or Dash)
    MovementState = ECombatMovementState::Approaching;
    SetComponentTickEnabled(true);

    // TODO: Play ApproachMontage, TrailVFX, MovementSound

    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Starting %s approach (%.0f units)"),
           *GetOwner()->GetName(),
           *Approach->ApproachName,
           FVector::Dist(GridPosition, TargetPosition));
}

void UCombatMovementComponent::StartReturn()
{
    if (!bHasGridPosition)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatMovement] No grid position stored, cannot return"));
        return;
    }

    if (!GetOwner())
    {
        return;
    }

    // Check if already at grid position
    float DistanceToGrid = FVector::Dist(GetOwner()->GetActorLocation(), GridPosition);
    if (DistanceToGrid < ArrivalThreshold)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Already at grid position"),
               *GetOwner()->GetName());
        CompleteReturn();
        return;
    }

    MovementState = ECombatMovementState::Returning;
    SetComponentTickEnabled(true);

    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Returning to grid (%.0f units)"),
           *GetOwner()->GetName(), DistanceToGrid);
}

void UCombatMovementComponent::CancelMovement()
{
    if (MovementState == ECombatMovementState::Idle)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Movement cancelled"),
           GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

    // Teleport back to grid position
    if (bHasGridPosition && GetOwner())
    {
        TeleportTo(GridPosition);
    }

    MovementState = ECombatMovementState::Idle;
    SetComponentTickEnabled(false);
    CurrentTarget = nullptr;

    OnMovementCancelled.Broadcast();
}

void UCombatMovementComponent::OnActionExecutionComplete()
{
    if (MovementState == ECombatMovementState::Executing)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Action complete, starting return"),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        StartReturn();
    }
}

// ==================== MOVEMENT HELPERS ====================

float UCombatMovementComponent::CalculateMovementSpeed() const
{
    float StatMultiplier = 1.0f;

    // Get movement speed from character stats
    if (CharacterDataComp && CharacterDataComp->CharacterData)
    {
        StatMultiplier = CharacterDataComp->CharacterData->CalculateMovementSpeed() / 100.0f;
        // CalculateMovementSpeed returns a value like 100-200, normalize it
        StatMultiplier = FMath::Max(StatMultiplier, 0.5f);
    }

    // Get multiplier from approach data
    float ApproachMultiplier = 1.0f;
    if (CurrentApproachData)
    {
        ApproachMultiplier = CurrentApproachData->GetEffectiveSpeedMultiplier();
    }

    return BaseSpeed * StatMultiplier * ApproachMultiplier;
}

bool UCombatMovementComponent::MoveToward(const FVector &Destination, float Speed, float DeltaTime)
{
    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return true;
    }

    FVector CurrentLocation = Owner->GetActorLocation();
    FVector Direction = Destination - CurrentLocation;
    Direction.Z = 0; // Keep movement horizontal

    float Distance = Direction.Size();

    // Check if arrived
    if (Distance < ArrivalThreshold)
    {
        Owner->SetActorLocation(Destination);
        return true;
    }

    // Calculate movement
    Direction.Normalize();
    float MoveDistance = Speed * DeltaTime;

    // Don't overshoot
    if (MoveDistance > Distance)
    {
        Owner->SetActorLocation(Destination);
        return true;
    }

    // Move toward destination
    FVector NewLocation = CurrentLocation + (Direction * MoveDistance);
    Owner->SetActorLocation(NewLocation);

    // Face movement direction
    UpdateFacingDirection(Direction);

    return false;
}

void UCombatMovementComponent::TeleportTo(const FVector &Position)
{
    if (AActor *Owner = GetOwner())
    {
        Owner->SetActorLocation(Position);
    }
}

void UCombatMovementComponent::UpdateFacingDirection(const FVector &Direction)
{
    if (Direction.IsNearlyZero())
    {
        return;
    }

    if (AActor *Owner = GetOwner())
    {
        FRotator NewRotation = Direction.GetSafeNormal().Rotation();
        Owner->SetActorRotation(NewRotation);
    }
}

void UCombatMovementComponent::CompleteApproach()
{
    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Approach complete"),
           GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

    MovementState = ECombatMovementState::Executing;
    SetComponentTickEnabled(false);

    // Face target
    if (CurrentTarget && GetOwner())
    {
        FVector Direction = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
        Direction.Z = 0;
        UpdateFacingDirection(Direction);
    }

    OnApproachComplete.Broadcast();
}

void UCombatMovementComponent::CompleteReturn()
{
    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Return complete"),
           GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

    // Face arena center (toward opposing team)
    if (GetOwner() && !CachedArenaCenter.IsZero())
    {
        FVector Direction = CachedArenaCenter - GridPosition;
        Direction.Z = 0;
        if (!Direction.IsNearlyZero())
        {
            UpdateFacingDirection(Direction);
        }
    }

    MovementState = ECombatMovementState::Idle;
    SetComponentTickEnabled(false);
    CurrentTarget = nullptr;

    OnReturnComplete.Broadcast();
}

UAnimMontage *UCombatMovementComponent::GetReturnMontage() const
{
    // Return animation is global, not per-approach
    // All characters use the same default return animation
    return DefaultReturnMontage;
}