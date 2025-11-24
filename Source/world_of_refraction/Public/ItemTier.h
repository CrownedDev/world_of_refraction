// ItemTier.h
// Defines the 7-tier progression system for items (F through S)

#pragma once

#include "CoreMinimal.h"
#include "ItemTier.generated.h"

/**
 * Enum representing the 7 item tiers from weakest (F) to strongest (S)
 * Each tier has different power levels and availability
 */
UENUM(BlueprintType)
enum class EItemTier : uint8
{
    F_Tier UMETA(DisplayName = "F-Tier (Weakest)"),
    E_Tier UMETA(DisplayName = "E-Tier"),
    D_Tier UMETA(DisplayName = "D-Tier"),
    C_Tier UMETA(DisplayName = "C-Tier"),
    B_Tier UMETA(DisplayName = "B-Tier"),
    A_Tier UMETA(DisplayName = "A-Tier"),
    S_Tier UMETA(DisplayName = "S-Tier (Strongest)")
};