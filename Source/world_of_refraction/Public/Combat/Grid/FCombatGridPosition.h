// FCombatGridPosition.h
// Combat grid position data structure

#pragma once

#include "CoreMinimal.h"
#include "Combat/Grid/ECombatRow.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "FCombatGridPosition.generated.h"

/**
 * Represents a position on the combat grid
 * Each team has a 3x3 grid (9 positions)
 */
USTRUCT(BlueprintType)
struct FCombatGridPosition
{
    GENERATED_BODY()
    
    /** Which team this position belongs to (0 = Player, 1 = Enemy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 TeamIndex = 0;
    
    /** Row position (Back=0, Middle=1, Front=2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    ECombatRow Row = ECombatRow::Middle;
    
    /** Column position (0, 1, or 2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0", ClampMax = "2"))
    int32 Column = 1;
    
    // ==================== CONSTRUCTORS ====================
    
    FCombatGridPosition() = default;
    
    FCombatGridPosition(int32 InTeamIndex, ECombatRow InRow, int32 InColumn)
        : TeamIndex(InTeamIndex)
        , Row(InRow)
        , Column(FMath::Clamp(InColumn, 0, 2))
    {
    }
    
    // ==================== MODIFIER GETTERS ====================
    
    /** Get damage modifier based on row position */
    float GetDamageModifier() const
    {
        switch (Row)
        {
            case ECombatRow::Front:  return CombatGridConstants::FRONT_ROW_DAMAGE_MODIFIER;
            case ECombatRow::Middle: return CombatGridConstants::MIDDLE_ROW_DAMAGE_MODIFIER;
            case ECombatRow::Back:   return CombatGridConstants::BACK_ROW_DAMAGE_MODIFIER;
            default:                 return 1.0f;
        }
    }
    
    /** Get defense modifier based on row position */
    float GetDefenseModifier() const
    {
        switch (Row)
        {
            case ECombatRow::Front:  return CombatGridConstants::FRONT_ROW_DEFENSE_MODIFIER;
            case ECombatRow::Middle: return CombatGridConstants::MIDDLE_ROW_DEFENSE_MODIFIER;
            case ECombatRow::Back:   return CombatGridConstants::BACK_ROW_DEFENSE_MODIFIER;
            default:                 return 1.0f;
        }
    }
    
    // ==================== UTILITY ====================
    
    /** Get row index (0=Back, 1=Middle, 2=Front) */
    int32 GetRowIndex() const
    {
        return CombatRowHelpers::GetRowIndex(Row);
    }
    
    /** Check if position is valid */
    bool IsValid() const
    {
        return TeamIndex >= 0 && TeamIndex <= 1 
            && Column >= 0 && Column < CombatGridConstants::GRID_COLUMNS;
    }
    
    /** Get unique position ID for this team (0-8) */
    int32 GetPositionID() const
    {
        return GetRowIndex() * CombatGridConstants::GRID_COLUMNS + Column;
    }
    
    /** Get formatted string for debugging */
    FString ToString() const
    {
        return FString::Printf(TEXT("Team%d [%s, Col%d]"), 
            TeamIndex, 
            *CombatRowHelpers::GetRowName(Row), 
            Column);
    }
    
    // ==================== OPERATORS ====================
    
    bool operator==(const FCombatGridPosition& Other) const
    {
        return TeamIndex == Other.TeamIndex 
            && Row == Other.Row 
            && Column == Other.Column;
    }
    
    bool operator!=(const FCombatGridPosition& Other) const
    {
        return !(*this == Other);
    }
};
