// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EActionType.generated.h"

/**
 * EActionType
 * Categories of actions that can be executed in combat
 */
UENUM(BlueprintType)
enum class EActionType : uint8
{
	None			UMETA(DisplayName = "None"),
	Spell			UMETA(DisplayName = "Spell"),
	Ability			UMETA(DisplayName = "Ability"),
	Item			UMETA(DisplayName = "Item"),
	Attack			UMETA(DisplayName = "Basic Attack"),
	Ultimate		UMETA(DisplayName = "Ultimate"),
	Defend			UMETA(DisplayName = "Defend"),
	SwitchWeapon	UMETA(DisplayName = "Switch Weapon"),
	Flee			UMETA(DisplayName = "Flee")
};
