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
 * - OnTrigger: When condition met (uses ESkillTrigger for condition)
 * - Persistent: Always active, never removed unless explicitly cleared
 * - Instant: One-shot like Immediate, but runs through ApplyEffect's gates then
 *            applies via its Server* primitive and returns WITHOUT being stored
 *            (Immediate stores-then-tears-down; Instant never enters ActiveEffects)
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

	/** Processes when a condition is met (uses ESkillTrigger) */
	OnTrigger UMETA(DisplayName = "Conditional (Uses ESkillTrigger)"),

	/** Always active, stat modifier style effects */
	Persistent UMETA(DisplayName = "Persistent (Always Active)"),

	/** True one-shot: runs its Server* logic through ApplyEffect's gates, then returns
	 *  WITHOUT being stored in ActiveEffects. Unlike Immediate (which stores-then-tears-down,
	 *  briefly exposing it to the re-apply strength gate), an Instant never enters the array —
	 *  so it never matches FindEffectByID and is policy-safe by construction.
	 *  Appended at the END: this enum is serialized — do NOT reorder/insert above. */
	Instant UMETA(DisplayName = "Instant (Hook-Then-Apply, No Store)")
};
