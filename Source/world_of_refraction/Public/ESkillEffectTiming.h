// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESkillEffectTiming.generated.h"

/**
 * ESkillEffectTiming
 * Defines WHEN a skill effect processes/activates
 *
 * Processing Rules:
 * - Immediate: Once when applied (instant damage, cleanse, dispel)
 * - StartOfOwnTurn: At start of affected actor's turn (buffs, shields, regen)
 * - EndOfOwnTurn: At end of affected actor's turn (DOTs, lingering damage)
 * - OnTrigger: When condition met (uses EPassiveTrigger for condition)
 * - Persistent: Always active, never removed unless explicitly cleared
 */
UENUM(BlueprintType)
enum class ESkillEffectTiming : uint8
{
	/** One-shot effect, processes once when applied */
	Immediate UMETA(DisplayName = "Immediate (One-Shot)"),

	/** Processes at the start of the affected actor's turn */
	StartOfOwnTurn UMETA(DisplayName = "Start of Own Turn"),

	/** Processes at the end of the affected actor's turn */
	EndOfOwnTurn UMETA(DisplayName = "End of Own Turn"),

	/** Processes when a condition is met (uses EPassiveTrigger) */
	OnTrigger UMETA(DisplayName = "Conditional (Uses EPassiveTrigger)"),

	/** Always active, stat modifier style effects */
	Persistent UMETA(DisplayName = "Persistent (Always Active)")
};
