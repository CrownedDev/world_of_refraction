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
	None UMETA(DisplayName = "None"),
	Spell UMETA(DisplayName = "Refraction"),
	Ability UMETA(DisplayName = "Ability"),
	Item UMETA(DisplayName = "Item"),
	// Attack folded into Ability (attack/ability merge) — basic attacks dispatch as Ability with
	// USkillDataBase::IsAttack() true. See the +EnumRedirects(EActionType, Attack->Ability) in
	// DefaultEngine.ini for serialized BP/asset migration.
	Defend UMETA(DisplayName = "Defend"),
	SwitchWeapon UMETA(DisplayName = "Switch Weapon"),
	Flee UMETA(DisplayName = "Flee"),
	// B-170c (Q-170c): ends the turn and banks the actions left. Appended: the enum
	// is stored by value in Blueprints and replicated picks, so members are never reordered.
	EndTurn UMETA(DisplayName = "End Turn")
};
