// CombatGridSubsystem.cpp
// Combat grid position management implementation

#include "Combat/Grid/CombatGridSubsystem.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "DrawDebugHelpers.h"
#include "Character/CharacterDataComponent.h"

void UCombatGridSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Subsystem initialized"));
}

void UCombatGridSubsystem::Deinitialize()
{
    ClearAllPositions();
    Super::Deinitialize();
}

// ==================== POSITION MANAGEMENT ====================

bool UCombatGridSubsystem::AssignPosition(AActor *Actor, const FCombatGridPosition &Position)
{
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] Cannot assign position to null actor"));
        return false;
    }

    if (!Position.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] Invalid position: %s"), *Position.ToString());
        return false;
    }

    // Check if position is already occupied by another actor
    if (IsPositionOccupied(Position))
    {
        AActor *Occupant = GetActorAtPosition(Position);
        if (Occupant != Actor)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] Position %s already occupied by %s"),
                   *Position.ToString(), *Occupant->GetName());
            return false;
        }
    }

    // Remove actor from any previous position
    RemoveFromGrid(Actor);

    // Assign new position
    ActorPositions.Add(Actor, Position);

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] %s assigned to %s (Dmg: %.0f%%, Def: %.0f%%)"),
           *Actor->GetName(),
           *Position.ToString(),
           (Position.GetDamageModifier() - 1.0f) * 100.0f,
           (Position.GetDefenseModifier() - 1.0f) * 100.0f);

    return true;
}

void UCombatGridSubsystem::RemoveFromGrid(AActor *Actor)
{
    if (Actor && ActorPositions.Contains(Actor))
    {
        ActorPositions.Remove(Actor);
        UE_LOG(LogTemp, Log, TEXT("[CombatGrid] %s removed from grid"), *Actor->GetName());
    }
}

bool UCombatGridSubsystem::GetActorPosition(AActor *Actor, FCombatGridPosition &OutPosition) const
{
    if (!Actor)
    {
        return false;
    }

    const FCombatGridPosition *Found = ActorPositions.Find(Actor);
    if (Found)
    {
        OutPosition = *Found;
        return true;
    }

    return false;
}

bool UCombatGridSubsystem::IsPositionOccupied(const FCombatGridPosition &Position) const
{
    for (const auto &Pair : ActorPositions)
    {
        if (Pair.Value == Position)
        {
            return true;
        }
    }
    return false;
}

AActor *UCombatGridSubsystem::GetActorAtPosition(const FCombatGridPosition &Position) const
{
    for (const auto &Pair : ActorPositions)
    {
        if (Pair.Value == Position)
        {
            return Pair.Key;
        }
    }
    return nullptr;
}

void UCombatGridSubsystem::ClearAllPositions()
{
    int32 Count = ActorPositions.Num();
    ActorPositions.Empty();

    if (Count > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Cleared %d positions"), Count);
    }
}

// ==================== MODIFIER GETTERS ====================

float UCombatGridSubsystem::GetDamageModifier(AActor *Actor) const
{
    FCombatGridPosition Position;
    if (GetActorPosition(Actor, Position))
    {
        return Position.GetDamageModifier();
    }

    // Default to no modifier if not on grid
    return 1.0f;
}

float UCombatGridSubsystem::GetDefenseModifier(AActor *Actor) const
{
    FCombatGridPosition Position;
    if (GetActorPosition(Actor, Position))
    {
        return Position.GetDefenseModifier();
    }

    // Default to no modifier if not on grid
    return 1.0f;
}

ECombatRow UCombatGridSubsystem::GetActorRow(AActor *Actor) const
{
    FCombatGridPosition Position;
    if (GetActorPosition(Actor, Position))
    {
        return Position.Row;
    }

    // Default to middle row
    return ECombatRow::Middle;
}

// ==================== TEAM QUERIES ====================

TArray<AActor *> UCombatGridSubsystem::GetTeamActors(int32 TeamIndex) const
{
    TArray<AActor *> Result;

    for (const auto &Pair : ActorPositions)
    {
        if (Pair.Value.TeamIndex == TeamIndex)
        {
            Result.Add(Pair.Key);
        }
    }

    return Result;
}

TArray<AActor *> UCombatGridSubsystem::GetActorsInRow(int32 TeamIndex, ECombatRow Row) const
{
    TArray<AActor *> Result;

    for (const auto &Pair : ActorPositions)
    {
        if (Pair.Value.TeamIndex == TeamIndex && Pair.Value.Row == Row)
        {
            Result.Add(Pair.Key);
        }
    }

    return Result;
}

int32 UCombatGridSubsystem::GetTeamCount(int32 TeamIndex) const
{
    int32 Count = 0;

    for (const auto &Pair : ActorPositions)
    {
        if (Pair.Value.TeamIndex == TeamIndex)
        {
            Count++;
        }
    }

    return Count;
}

// ==================== WORLD POSITIONING ====================

FVector UCombatGridSubsystem::CalculateWorldPosition(const FCombatGridPosition &Position, const FVector &ArenaCenter) const
{
    // Grid layout:
    // Team 0 (Player) on left, Team 1 (Enemy) on right
    // Back row furthest from center, Front row closest

    float TeamDirection = (Position.TeamIndex == 0) ? -1.0f : 1.0f;

    // X = Team separation (left/right from center)
    // Row 0 (Back) = furthest from center
    // Row 2 (Front) = closest to center
    int32 RowIndex = Position.GetRowIndex();
    float RowOffset = (2 - RowIndex) * CombatGridConstants::CELL_SPACING; // Back is furthest
    float XOffset = (CombatGridConstants::TEAM_SEPARATION / 2.0f + RowOffset) * TeamDirection;

    // Y = Column offset (centered around 0)
    // Column 0 = left, Column 1 = center, Column 2 = right
    float YOffset = (Position.Column - 1) * CombatGridConstants::CELL_SPACING;

    // Z = Height offset
    float ZOffset = CombatGridConstants::CHARACTER_HEIGHT_OFFSET;

    return ArenaCenter + FVector(XOffset, YOffset, ZOffset);
}

bool UCombatGridSubsystem::PlaceActorAtGridPosition(AActor *Actor, const FVector &ArenaCenter)
{
    if (!Actor)
    {
        return false;
    }

    FCombatGridPosition Position;
    if (!GetActorPosition(Actor, Position))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] %s has no assigned position"), *Actor->GetName());
        return false;
    }

    FVector WorldPos = CalculateWorldPosition(Position, ArenaCenter);
    Actor->SetActorLocation(WorldPos);

    // Face opponent team
    float YawRotation = (Position.TeamIndex == 0) ? 0.0f : 180.0f;
    Actor->SetActorRotation(FRotator(0.0f, YawRotation, 0.0f));

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Placed %s at %s (World: %s)"),
           *Actor->GetName(),
           *Position.ToString(),
           *WorldPos.ToString());

    return true;
}

void UCombatGridSubsystem::PlaceAllActors(const FVector &ArenaCenter)
{
    for (const auto &Pair : ActorPositions)
    {
        PlaceActorAtGridPosition(Pair.Key, ArenaCenter);
    }
}

// ==================== AUTO-ASSIGNMENT ====================

void UCombatGridSubsystem::AutoAssignTeam(const TArray<AActor *> &TeamActors, int32 TeamIndex, ECombatRow PreferredRow)
{
    if (TeamActors.Num() == 0)
    {
        return;
    }

    if (TeamActors.Num() > CombatGridConstants::MAX_TEAM_SIZE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] Team has %d actors, max is %d"),
               TeamActors.Num(), CombatGridConstants::MAX_TEAM_SIZE);
    }

    // Assign actors to columns 0, 1, 2 in preferred row
    int32 Column = 0;
    for (AActor *Actor : TeamActors)
    {
        if (Column >= CombatGridConstants::GRID_COLUMNS)
        {
            break; // Max 3 per team
        }

        FCombatGridPosition Position(TeamIndex, PreferredRow, Column);
        AssignPosition(Actor, Position);
        Column++;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Auto-assigned %d actors to Team %d (%s row)"),
           FMath::Min(TeamActors.Num(), CombatGridConstants::MAX_TEAM_SIZE),
           TeamIndex,
           *CombatRowHelpers::GetRowName(PreferredRow));
}

// ==================== DEBUG ====================

void UCombatGridSubsystem::DebugLogAllPositions() const
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("COMBAT GRID POSITIONS"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));

    if (ActorPositions.Num() == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("  (No actors assigned)"));
    }
    else
    {
        // Team 0
        UE_LOG(LogTemp, Display, TEXT("Team 0 (Player):"));
        for (const auto &Pair : ActorPositions)
        {
            if (Pair.Value.TeamIndex == 0)
            {
                UE_LOG(LogTemp, Display, TEXT("  %s - %s"),
                       *Pair.Key->GetName(),
                       *Pair.Value.ToString());
            }
        }

        // Team 1
        UE_LOG(LogTemp, Display, TEXT("Team 1 (Enemy):"));
        for (const auto &Pair : ActorPositions)
        {
            if (Pair.Value.TeamIndex == 1)
            {
                UE_LOG(LogTemp, Display, TEXT("  %s - %s"),
                       *Pair.Key->GetName(),
                       *Pair.Value.ToString());
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void UCombatGridSubsystem::DebugLogModifiers() const
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("COMBAT GRID MODIFIERS"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));

    for (const auto &Pair : ActorPositions)
    {
        const FCombatGridPosition &Pos = Pair.Value;
        UE_LOG(LogTemp, Display, TEXT("  %s [%s]:  Dmg %+.0f%%  Def %+.0f%%"),
               *Pair.Key->GetName(),
               *CombatRowHelpers::GetRowName(Pos.Row),
               (Pos.GetDamageModifier() - 1.0f) * 100.0f,
               (Pos.GetDefenseModifier() - 1.0f) * 100.0f);
    }

    UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void UCombatGridSubsystem::DebugDrawGrid(const FVector &ArenaCenter, float Duration)
{
    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    // Colors for each row
    const FColor BackColor = FColor::Blue;
    const FColor MiddleColor = FColor::Green;
    const FColor FrontColor = FColor::Red;

    // Draw grid for both teams
    for (int32 TeamIdx = 0; TeamIdx < 2; TeamIdx++)
    {
        for (int32 RowIdx = 0; RowIdx < CombatGridConstants::GRID_ROWS; RowIdx++)
        {
            ECombatRow Row = static_cast<ECombatRow>(RowIdx);
            FColor RowColor = (Row == ECombatRow::Back) ? BackColor : (Row == ECombatRow::Middle) ? MiddleColor
                                                                                                  : FrontColor;

            for (int32 Col = 0; Col < CombatGridConstants::GRID_COLUMNS; Col++)
            {
                FCombatGridPosition Pos(TeamIdx, Row, Col);
                FVector WorldPos = CalculateWorldPosition(Pos, ArenaCenter);

                // Draw sphere at position
                DrawDebugSphere(World, WorldPos, 30.0f, 8, RowColor, false, Duration);

                // Draw position label
                FString Label = FString::Printf(TEXT("T%d %s C%d"),
                                                TeamIdx,
                                                *CombatRowHelpers::GetRowName(Row),
                                                Col);
                DrawDebugString(World, WorldPos + FVector(0, 0, 50), Label, nullptr, FColor::White, Duration);
            }
        }
    }

    // Draw center marker
    DrawDebugSphere(World, ArenaCenter, 20.0f, 8, FColor::Yellow, false, Duration);
    DrawDebugString(World, ArenaCenter + FVector(0, 0, 30), TEXT("CENTER"), nullptr, FColor::Yellow, Duration);

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Debug grid drawn at %s for %.1fs"), *ArenaCenter.ToString(), Duration);
}

void UCombatGridSubsystem::DebugDrawActorPositions(const FVector &ArenaCenter, float Duration)
{
    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    if (ActorPositions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] No actors to draw"));
        return;
    }

    for (const auto &Pair : ActorPositions)
    {
        AActor *Actor = Pair.Key;
        const FCombatGridPosition &Pos = Pair.Value;

        if (!Actor)
        {
            continue;
        }

        FVector WorldPos = CalculateWorldPosition(Pos, ArenaCenter);
        FColor TeamColor = (Pos.TeamIndex == 0) ? FColor::Cyan : FColor::Orange;

        // Draw actor marker
        DrawDebugSphere(World, WorldPos, 50.0f, 12, TeamColor, false, Duration);

        // Draw actor name and modifiers
        FString Label = FString::Printf(TEXT("%s\nDmg:%+.0f%% Def:%+.0f%%"),
                                        *Actor->GetName(),
                                        (Pos.GetDamageModifier() - 1.0f) * 100.0f,
                                        (Pos.GetDefenseModifier() - 1.0f) * 100.0f);
        DrawDebugString(World, WorldPos + FVector(0, 0, 70), Label, nullptr, TeamColor, Duration);

        // Draw line from actor's actual position to grid position
        DrawDebugLine(World, Actor->GetActorLocation(), WorldPos, TeamColor, false, Duration, 0, 2.0f);
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Drew %d actor positions"), ActorPositions.Num());
}

// ==================== FACING ====================

void UCombatGridSubsystem::UpdateAllActorFacing(const FVector &ArenaCenter)
{
    for (const auto &Pair : ActorPositions)
    {
        UpdateActorFacing(Pair.Key, ArenaCenter);
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Updated facing for %d actors"), ActorPositions.Num());
}

void UCombatGridSubsystem::UpdateActorFacing(AActor *Actor, const FVector &ArenaCenter)
{
    if (!Actor)
    {
        return;
    }

    AActor *Target = GetFacingTarget(Actor);
    if (!Target)
    {
        // No target - face arena center
        FVector DirectionToCenter = ArenaCenter - Actor->GetActorLocation();
        DirectionToCenter.Z = 0;
        if (!DirectionToCenter.IsNearlyZero())
        {
            Actor->SetActorRotation(DirectionToCenter.GetSafeNormal().Rotation());
        }
        return;
    }

    // Face the target
    FVector TargetPos = CalculateWorldPosition(ActorPositions[Target], ArenaCenter);
    FVector MyPos = Actor->GetActorLocation();
    FVector Direction = TargetPos - MyPos;
    Direction.Z = 0; // Keep rotation horizontal

    if (!Direction.IsNearlyZero())
    {
        Actor->SetActorRotation(Direction.GetSafeNormal().Rotation());
    }
}

AActor *UCombatGridSubsystem::GetFacingTarget(AActor *Actor) const
{
    FCombatGridPosition MyPos;
    if (!GetActorPosition(Actor, MyPos))
    {
        return nullptr;
    }

    int32 EnemyTeam = (MyPos.TeamIndex == 0) ? 1 : 0;

    // Priority 1: Same column, check rows Front -> Middle -> Back
    TArray<ECombatRow> RowPriority = {ECombatRow::Front, ECombatRow::Middle, ECombatRow::Back};

    for (ECombatRow Row : RowPriority)
    {
        for (const auto &Pair : ActorPositions)
        {
            const FCombatGridPosition &EnemyPos = Pair.Value;

            if (EnemyPos.TeamIndex == EnemyTeam &&
                EnemyPos.Column == MyPos.Column &&
                EnemyPos.Row == Row)
            {
                // Check if alive (has CharacterDataComponent and is alive)
                UCharacterDataComponent *Comp = Pair.Key->FindComponentByClass<UCharacterDataComponent>();
                if (Comp && Comp->bIsAlive)
                {
                    return Pair.Key;
                }
            }
        }
    }

    // Priority 2: Find closest living enemy by grid distance
    AActor *ClosestEnemy = nullptr;
    int32 ClosestDistance = INT_MAX;

    for (const auto &Pair : ActorPositions)
    {
        const FCombatGridPosition &EnemyPos = Pair.Value;

        if (EnemyPos.TeamIndex != EnemyTeam)
        {
            continue;
        }

        // Check if alive
        UCharacterDataComponent *Comp = Pair.Key->FindComponentByClass<UCharacterDataComponent>();
        if (!Comp || !Comp->bIsAlive)
        {
            continue;
        }

        // Calculate grid distance (column diff + row diff, with row weighted)
        // Front row = 2, Middle = 1, Back = 0 for "closeness"
        int32 ColDist = FMath::Abs(EnemyPos.Column - MyPos.Column);
        int32 RowValue = (EnemyPos.Row == ECombatRow::Front) ? 0 : (EnemyPos.Row == ECombatRow::Middle) ? 1
                                                                                                        : 2;
        int32 Distance = ColDist * 10 + RowValue; // Column difference weighted more

        if (Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestEnemy = Pair.Key;
        }
    }

    return ClosestEnemy;
}

// ==================== POSITION MOVEMENT ====================

bool UCombatGridSubsystem::MoveActorToRow(AActor *Actor, ECombatRow NewRow, const FVector &ArenaCenter)
{
    if (!Actor)
    {
        return false;
    }

    FCombatGridPosition CurrentPos;
    if (!GetActorPosition(Actor, CurrentPos))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] MoveActorToRow: %s not on grid"), *Actor->GetName());
        return false;
    }

    // Already at target row
    if (CurrentPos.Row == NewRow)
    {
        return true;
    }

    // Create new position
    FCombatGridPosition NewPos = CurrentPos;
    NewPos.Row = NewRow;

    // Check if new position is occupied
    if (IsPositionOccupied(NewPos))
    {
        AActor *Occupant = GetActorAtPosition(NewPos);
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] MoveActorToRow: Position %s occupied by %s"),
               *NewPos.ToString(), *Occupant->GetName());
        return false;
    }

    // Update position in map
    ActorPositions[Actor] = NewPos;

    // Update world position
    FVector WorldPos = CalculateWorldPosition(NewPos, ArenaCenter);
    Actor->SetActorLocation(WorldPos);

    // Update facing
    UpdateActorFacing(Actor, ArenaCenter);

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] %s moved to %s (Dmg: %+.0f%%, Def: %+.0f%%)"),
           *Actor->GetName(),
           *NewPos.ToString(),
           (NewPos.GetDamageModifier() - 1.0f) * 100.0f,
           (NewPos.GetDefenseModifier() - 1.0f) * 100.0f);

    return true;
}

bool UCombatGridSubsystem::PushActorBack(AActor *Actor, const FVector &ArenaCenter)
{
    if (!Actor)
    {
        return false;
    }

    FCombatGridPosition CurrentPos;
    if (!GetActorPosition(Actor, CurrentPos))
    {
        return false;
    }

    // Determine new row (Front→Middle→Back)
    ECombatRow NewRow;
    switch (CurrentPos.Row)
    {
    case ECombatRow::Front:
        NewRow = ECombatRow::Middle;
        break;
    case ECombatRow::Middle:
        NewRow = ECombatRow::Back;
        break;
    case ECombatRow::Back:
        UE_LOG(LogTemp, Log, TEXT("[CombatGrid] %s already at Back row, cannot push further"),
               *Actor->GetName());
        return false;
    default:
        return false;
    }

    return MoveActorToRow(Actor, NewRow, ArenaCenter);
}

bool UCombatGridSubsystem::PullActorForward(AActor *Actor, const FVector &ArenaCenter)
{
    if (!Actor)
    {
        return false;
    }

    FCombatGridPosition CurrentPos;
    if (!GetActorPosition(Actor, CurrentPos))
    {
        return false;
    }

    // Determine new row (Back→Middle→Front)
    ECombatRow NewRow;
    switch (CurrentPos.Row)
    {
    case ECombatRow::Back:
        NewRow = ECombatRow::Middle;
        break;
    case ECombatRow::Middle:
        NewRow = ECombatRow::Front;
        break;
    case ECombatRow::Front:
        UE_LOG(LogTemp, Log, TEXT("[CombatGrid] %s already at Front row, cannot pull further"),
               *Actor->GetName());
        return false;
    default:
        return false;
    }

    return MoveActorToRow(Actor, NewRow, ArenaCenter);
}

bool UCombatGridSubsystem::SwapActorPositions(AActor *ActorA, AActor *ActorB, const FVector &ArenaCenter)
{
    if (!ActorA || !ActorB || ActorA == ActorB)
    {
        return false;
    }

    FCombatGridPosition PosA, PosB;
    if (!GetActorPosition(ActorA, PosA) || !GetActorPosition(ActorB, PosB))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] SwapActorPositions: One or both actors not on grid"));
        return false;
    }

    // Must be same team
    if (PosA.TeamIndex != PosB.TeamIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatGrid] SwapActorPositions: Cannot swap actors on different teams"));
        return false;
    }

    // Swap positions in map
    ActorPositions[ActorA] = PosB;
    ActorPositions[ActorB] = PosA;

    // Update world positions
    FVector WorldPosA = CalculateWorldPosition(PosB, ArenaCenter);
    FVector WorldPosB = CalculateWorldPosition(PosA, ArenaCenter);
    ActorA->SetActorLocation(WorldPosA);
    ActorB->SetActorLocation(WorldPosB);

    // Update facing for both
    UpdateActorFacing(ActorA, ArenaCenter);
    UpdateActorFacing(ActorB, ArenaCenter);

    UE_LOG(LogTemp, Log, TEXT("[CombatGrid] Swapped %s and %s positions"),
           *ActorA->GetName(), *ActorB->GetName());

    return true;
}

int32 UCombatGridSubsystem::GetPositionKey(const FCombatGridPosition &Position) const
{
    // Unique key: TeamIndex * 100 + RowIndex * 10 + Column
    return Position.TeamIndex * 100 + Position.GetRowIndex() * 10 + Position.Column;
}
