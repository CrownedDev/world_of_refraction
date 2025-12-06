// CombatGridSubsystem.cpp
// Combat grid position management implementation

#include "CombatGridSubsystem.h"
#include "CombatGridConstants.h"

void UCombatGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
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

bool UCombatGridSubsystem::AssignPosition(AActor* Actor, const FCombatGridPosition& Position)
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
        AActor* Occupant = GetActorAtPosition(Position);
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

void UCombatGridSubsystem::RemoveFromGrid(AActor* Actor)
{
    if (Actor && ActorPositions.Contains(Actor))
    {
        ActorPositions.Remove(Actor);
        UE_LOG(LogTemp, Log, TEXT("[CombatGrid] %s removed from grid"), *Actor->GetName());
    }
}

bool UCombatGridSubsystem::GetActorPosition(AActor* Actor, FCombatGridPosition& OutPosition) const
{
    if (!Actor)
    {
        return false;
    }
    
    const FCombatGridPosition* Found = ActorPositions.Find(Actor);
    if (Found)
    {
        OutPosition = *Found;
        return true;
    }
    
    return false;
}

bool UCombatGridSubsystem::IsPositionOccupied(const FCombatGridPosition& Position) const
{
    for (const auto& Pair : ActorPositions)
    {
        if (Pair.Value == Position)
        {
            return true;
        }
    }
    return false;
}

AActor* UCombatGridSubsystem::GetActorAtPosition(const FCombatGridPosition& Position) const
{
    for (const auto& Pair : ActorPositions)
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

float UCombatGridSubsystem::GetDamageModifier(AActor* Actor) const
{
    FCombatGridPosition Position;
    if (GetActorPosition(Actor, Position))
    {
        return Position.GetDamageModifier();
    }
    
    // Default to no modifier if not on grid
    return 1.0f;
}

float UCombatGridSubsystem::GetDefenseModifier(AActor* Actor) const
{
    FCombatGridPosition Position;
    if (GetActorPosition(Actor, Position))
    {
        return Position.GetDefenseModifier();
    }
    
    // Default to no modifier if not on grid
    return 1.0f;
}

ECombatRow UCombatGridSubsystem::GetActorRow(AActor* Actor) const
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

TArray<AActor*> UCombatGridSubsystem::GetTeamActors(int32 TeamIndex) const
{
    TArray<AActor*> Result;
    
    for (const auto& Pair : ActorPositions)
    {
        if (Pair.Value.TeamIndex == TeamIndex)
        {
            Result.Add(Pair.Key);
        }
    }
    
    return Result;
}

TArray<AActor*> UCombatGridSubsystem::GetActorsInRow(int32 TeamIndex, ECombatRow Row) const
{
    TArray<AActor*> Result;
    
    for (const auto& Pair : ActorPositions)
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
    
    for (const auto& Pair : ActorPositions)
    {
        if (Pair.Value.TeamIndex == TeamIndex)
        {
            Count++;
        }
    }
    
    return Count;
}

// ==================== WORLD POSITIONING ====================

FVector UCombatGridSubsystem::CalculateWorldPosition(const FCombatGridPosition& Position, const FVector& ArenaCenter) const
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

bool UCombatGridSubsystem::PlaceActorAtGridPosition(AActor* Actor, const FVector& ArenaCenter)
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

void UCombatGridSubsystem::PlaceAllActors(const FVector& ArenaCenter)
{
    for (const auto& Pair : ActorPositions)
    {
        PlaceActorAtGridPosition(Pair.Key, ArenaCenter);
    }
}

// ==================== AUTO-ASSIGNMENT ====================

void UCombatGridSubsystem::AutoAssignTeam(const TArray<AActor*>& TeamActors, int32 TeamIndex, ECombatRow PreferredRow)
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
    for (AActor* Actor : TeamActors)
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
        for (const auto& Pair : ActorPositions)
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
        for (const auto& Pair : ActorPositions)
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
    
    for (const auto& Pair : ActorPositions)
    {
        const FCombatGridPosition& Pos = Pair.Value;
        UE_LOG(LogTemp, Display, TEXT("  %s [%s]:  Dmg %+.0f%%  Def %+.0f%%"),
            *Pair.Key->GetName(),
            *CombatRowHelpers::GetRowName(Pos.Row),
            (Pos.GetDamageModifier() - 1.0f) * 100.0f,
            (Pos.GetDefenseModifier() - 1.0f) * 100.0f);
    }
    
    UE_LOG(LogTemp, Display, TEXT("========================================"));
}

int32 UCombatGridSubsystem::GetPositionKey(const FCombatGridPosition& Position) const
{
    // Unique key: TeamIndex * 100 + RowIndex * 10 + Column
    return Position.TeamIndex * 100 + Position.GetRowIndex() * 10 + Position.Column;
}
