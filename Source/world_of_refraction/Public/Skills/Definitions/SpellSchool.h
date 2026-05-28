// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpellSchool.generated.h"

UENUM(BlueprintType)
enum class ESpellSchool : uint8
{
    Destruction UMETA(DisplayName = "Destruction"),
    Enhancement UMETA(DisplayName = "Enhancement"),
    Restoration UMETA(DisplayName = "Restoration"),
    Conjuration UMETA(DisplayName = "Conjuration")
};