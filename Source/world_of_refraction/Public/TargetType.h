#pragma once
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TargetType.generated.h"

UENUM(BlueprintType)
enum class ETargetType : uint8
{
    Self UMETA(DisplayName = "Self"),
    SingleEnemy UMETA(DisplayName = "Single Enemy"),
    AllEnemies UMETA(DisplayName = "All Enemies"),
    SingleAlly UMETA(DisplayName = "Single Ally"),
    AllAllies UMETA(DisplayName = "All Allies"),
    SingleAnyone UMETA(DisplayName = "Single Anyone"),
    Everyone UMETA(DisplayName = "Everyone")
};