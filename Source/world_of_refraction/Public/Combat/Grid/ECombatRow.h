// ECombatRow.h
// Defines combat grid row positions

#pragma once

#include "CoreMinimal.h"
#include "ECombatRow.generated.h"

/**
 * Combat grid row positions
 * Determines offensive/defensive bonuses based on positioning
 */
UENUM(BlueprintType)
enum class ECombatRow : uint8
{
    /** Back row: -5% damage, +5% defense - Defensive positioning */
    Back    UMETA(DisplayName = "Back Row"),
    
    /** Middle row: No modifiers - Balanced positioning */
    Middle  UMETA(DisplayName = "Middle Row"),
    
    /** Front row: +5% damage, -5% defense - Aggressive positioning */
    Front   UMETA(DisplayName = "Front Row")
};

/**
 * Helper functions for ECombatRow
 */
namespace CombatRowHelpers
{
    /** Get row index (0=Back, 1=Middle, 2=Front) */
    inline int32 GetRowIndex(ECombatRow Row)
    {
        return static_cast<int32>(Row);
    }
    
    /** Get row from index */
    inline ECombatRow GetRowFromIndex(int32 Index)
    {
        return static_cast<ECombatRow>(FMath::Clamp(Index, 0, 2));
    }
    
    /** Get display name */
    inline FString GetRowName(ECombatRow Row)
    {
        switch (Row)
        {
            case ECombatRow::Back:   return TEXT("Back");
            case ECombatRow::Middle: return TEXT("Middle");
            case ECombatRow::Front:  return TEXT("Front");
            default:                 return TEXT("Unknown");
        }
    }
}
