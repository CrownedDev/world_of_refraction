// CombatMovementComponent.cpp
// Combat movement implementation

#include "CombatMovementComponent.h"
#include "MovementData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "CombatAnimInstance.h"
#include "StatConstants.h"

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

    // Diagnostic state logging
    static int32 LogCounter = 0;
    if (++LogCounter % 30 == 0) // Log every 30 ticks (~0.5s at 60fps) to reduce spam
    {
        FString StateName;
        switch (MovementState)
        {
        case ECombatMovementState::Idle:
            StateName = TEXT("Idle");
            break;
        case ECombatMovementState::Approaching:
            StateName = TEXT("Approaching");
            break;
        case ECombatMovementState::Executing:
            StateName = TEXT("Executing");
            break;
        case ECombatMovementState::Returning:
            StateName = TEXT("Returning");
            break;
        default:
            StateName = TEXT("Unknown");
            break;
        }

        UE_LOG(LogTemp, Display, TEXT("[CombatMovement] %s: State=%s, Montage=%s"),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
               *StateName,
               CurrentMovementMontage ? *CurrentMovementMontage->GetName() : TEXT("None"));
    }

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
        float Speed = BaseSpeed * ReturnSpeedMultiplier;
        if (MoveToward(GridPosition, Speed, DeltaTime))
        {
            CompleteReturn();
        }
        break;
    }

    default:
        SetComponentTickEnabled(false);
        break;
    }
}
// ==================== MOVEMENT CONTROL ====================

void UCombatMovementComponent::StartApproach(AActor *Target, UMovementData *Approach, float ExecutionRange, const FVector &ArenaCenter)
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
    CurrentMovementData = Approach;

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
        OnMovementComplete.Broadcast();
        return;
    }

    ECombatMovementType MovementType = Approach->MovementType;

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
    if (MovementType == ECombatMovementType::Teleport)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Teleporting to target (%s)"),
               *GetOwner()->GetName(), *Approach->MovementName);

        // TODO: Play DepartureVFX, DepartureSound, MovementMontage (vanish)

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
        OnMovementComplete.Broadcast();
        return;
    }

    // Start movement (Direct or Dash)
    MovementState = ECombatMovementState::Approaching;

    // CRITICAL: Enable tick so movement actually happens
    SetComponentTickEnabled(true);

    // Get approach montage
    UAnimMontage *MovementMontage = nullptr;
    if (Approach && Approach->MovementMontage)
    {
        MovementMontage = Approach->MovementMontage;
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Using MovementData montage: %s"),
               *GetOwner()->GetName(), *MovementMontage->GetName());
    }
    else if (DefaultMovementMontage)
    {
        MovementMontage = DefaultMovementMontage;
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Using Default montage: %s"),
               *GetOwner()->GetName(), *MovementMontage->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatMovement] %s: No approach montage available"),
               *GetOwner()->GetName());
    }

    // Play approach animation
    if (MovementMontage)
    {
        PlayMovementMontage(MovementMontage);
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Started approach to %s (%.1f units away)"),
           *GetOwner()->GetName(),
           CurrentTarget ? *CurrentTarget->GetName() : TEXT("position"),
           FVector::Dist(GetOwner()->GetActorLocation(), TargetPosition));

    SetComponentTickEnabled(true);

    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Starting %s approach (%.0f units)"),
           *GetOwner()->GetName(),
           *Approach->MovementName,
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

    // Cache the direction to face during return (toward target/arena center)
    if (CurrentTarget)
    {
        ReturnFacingDirection = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
    }
    else if (!CachedArenaCenter.IsZero())
    {
        ReturnFacingDirection = CachedArenaCenter - GridPosition;
    }
    ReturnFacingDirection.Z = 0;
    ReturnFacingDirection.Normalize();

    // Play return animation - fallback chain
    UAnimMontage *ReturnMontage = GetReturnMontage();
    if (!ReturnMontage && CurrentMovementData && CurrentMovementData->MovementMontage)
    {
        ReturnMontage = CurrentMovementData->MovementMontage;
    }
    if (!ReturnMontage)
    {
        ReturnMontage = DefaultMovementMontage;
    }

    if (ReturnMontage)
    {
        PlayMovementMontage(ReturnMontage);
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] Return montage: %s"), *ReturnMontage->GetName());
    }

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
void UCombatMovementComponent::PlayMovementMontage(UAnimMontage *Montage)
{
    if (!Montage || !GetOwner())
    {
        return;
    }

    ACharacter *Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->GetMesh())
    {
        return;
    }

    // Stop any current movement montage first
    StopMovementMontage();

    UCombatAnimInstance *CombatAnim = Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance());
    if (CombatAnim)
    {
        CombatAnim->PlayMovementMontage(Montage);
        CurrentMovementMontage = Montage;
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Playing movement montage %s via CombatAnimInstance"),
               *GetOwner()->GetName(), *Montage->GetName());
    }
    else
    {
        // Fallback to direct play
        CurrentMovementMontage = Montage;
        Character->PlayAnimMontage(Montage, 1.0f);
        UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Playing movement montage %s (fallback)"),
               *GetOwner()->GetName(), *Montage->GetName());
    }
}

void UCombatMovementComponent::StopMovementMontage()
{
    if (!GetOwner())
    {
        return;
    }

    ACharacter *Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return;
    }

    UCombatAnimInstance *CombatAnim = Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance());
    if (CombatAnim)
    {
        CombatAnim->StopMovementMontage();
    }
    else if (CurrentMovementMontage)
    {
        Character->StopAnimMontage(CurrentMovementMontage);
    }

    CurrentMovementMontage = nullptr;
    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Stopped movement montage"), *GetOwner()->GetName());
}

void UCombatMovementComponent::FaceCurrentTarget()
{
    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    FVector FaceDirection = FVector::ZeroVector;

    if (CurrentTarget)
    {
        FaceDirection = CurrentTarget->GetActorLocation() - Owner->GetActorLocation();
    }
    else if (!CachedArenaCenter.IsZero())
    {
        FaceDirection = CachedArenaCenter - Owner->GetActorLocation();
    }

    FaceDirection.Z = 0;
    if (!FaceDirection.IsNearlyZero())
    {
        UpdateFacingDirection(FaceDirection);
    }
}

#include "StatConstants.h"

float UCombatMovementComponent::CalculateMovementSpeed() const
{
    float StatMultiplier = StatConstants::MOVEMENT_SPEED_MIN_MULTIPLIER;

    if (CharacterDataComp && CharacterDataComp->CharacterData)
    {
        int32 MovementPoints = CharacterDataComp->CharacterData->GetTotalMovementSpeed();
        float PointRatio = FMath::Clamp(
            (float)MovementPoints / (float)StatConstants::MAX_SUBSTAT_POINTS_PER_PILLAR,
            0.0f,
            1.0f);

        StatMultiplier = StatConstants::MOVEMENT_SPEED_MIN_MULTIPLIER +
                         (PointRatio * (StatConstants::MOVEMENT_SPEED_MAX_MULTIPLIER - StatConstants::MOVEMENT_SPEED_MIN_MULTIPLIER));
    }

    float ApproachMultiplier = 1.0f;
    if (CurrentMovementData)
    {
        ApproachMultiplier = CurrentMovementData->GetEffectiveSpeedMultiplier();
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

    // Face appropriate direction
    if (MovementState == ECombatMovementState::Returning)
    {
        FaceCurrentTarget();
    }
    else
    {
        UpdateFacingDirection(Direction);
    }

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

    // Stop movement animation - attack animation will play next
    StopMovementMontage();
    SetComponentTickEnabled(false);

    // Face target
    if (CurrentTarget && GetOwner())
    {
        FVector Direction = CurrentTarget->GetActorLocation() - GetOwner()->GetActorLocation();
        Direction.Z = 0;
        UpdateFacingDirection(Direction);
    }

    OnMovementComplete.Broadcast();
}

void UCombatMovementComponent::CompleteReturn()
{
    UE_LOG(LogTemp, Log, TEXT("[CombatMovement] %s: Return complete"),
           GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

    // Stop movement animation
    StopMovementMontage();

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

    OnMovementComplete.Broadcast();
}

UAnimMontage *UCombatMovementComponent::GetReturnMontage() const
{
    // Return animation is global, not per-approach
    // All characters use the same default return animation
    return DefaultReturnMontage;
}