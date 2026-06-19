#pragma once
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TargetType.generated.h"

/** DEPRECATED — kept so legacy assets still deserialize correctly. Migrated to
 *  ETargetType + ETargetCount in USkillDataBase::PostLoad. Remove after re-save pass. */
UENUM()
enum class ETargetType_Legacy : uint8
{
    Self         = 0,
    SingleEnemy  = 1,
    AllEnemies   = 2,
    SingleAlly   = 3,
    AllAllies    = 4,
    SingleAnyone = 5,
    Everyone     = 6
};

// Replaces the old 7-value conflated enum
// (Self/SingleEnemy/AllEnemies/SingleAlly/AllAllies/SingleAnyone/Everyone).
// The count axis is now ETargetCount. PostLoad migrates legacy authored values.
UENUM(BlueprintType)
enum class ETargetType : uint8
{
    Self UMETA(DisplayName = "Self"),
    Ally UMETA(DisplayName = "Ally"),
    Enemy UMETA(DisplayName = "Enemy"),
    Anyone UMETA(DisplayName = "Anyone")
};

UENUM(BlueprintType)
enum class ETargetCount : uint8
{
    // Double = exactly 2 targets. For passive/triggered effects: auto-resolved
    // (Double Ally = self + random teammate; Double Enemy = 2 random enemies).
    // For active abilities: player picks 2 targets sequentially in the UI.
    Single UMETA(DisplayName = "Single"),
    Double UMETA(DisplayName = "Double"),
    All UMETA(DisplayName = "All")
};