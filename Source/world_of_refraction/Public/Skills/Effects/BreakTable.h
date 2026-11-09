// BreakTable.h
// B-274 (S8): one row per status break: what a capped bar applies, its size, timing and any second
// effect. The trigger per damage type is ElementTable's BreakTrigger (BD's drain stays a resolver
// override); the flavour name is SkillEffectDisplayNames. USkillEffectManager::ApplyTriggeredSkillEffect
// reads this table. A new break = one row (Ice's, B-064a: Water's slow plus damage). Debug: wor.Breaks.Print; spec
// WoR.Status.BreakTable.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/ESkillEffectTiming.h"

/** How a break row's Size is read. */
enum class EBreakSize : uint8
{
	/** A yes/no effect (stun, silence...): the value is BREAK_GATE_VALUE. */
	Gate,
	/** Size is the percent applied. */
	Percent,
	/** Size is a share of the holder's max HP per payout (Burn). */
	ShareOfMaxHP,
	/** Size is a share of the holder's max EP per payout (BD drain), rounded. */
	ShareOfMaxEP,
	/** Size is a share of the holder's max HP dealt once, no lasting effect (Reality Burst). */
	OneShotShareOfMaxHP
};

struct FBreakRow
{
	/** The trigger the capped bar resolves to (BarCapTriggerResolver). */
	ESkillEffectType Trigger;
	/** The effect type applied. */
	ESkillEffectType Applied;
	EBreakSize SizeKind;
	float Size;
	ESkillEffectTiming Timing;
	/** Pays out from the holder's next turn start, not on landing (Burn, drain). */
	bool bDelayFirstExecution;
	/** A second effect applied with the same size and timing (Water: Spell Speed). None for most. */
	ESkillEffectType AlsoApplied;
	/** Places a newly applied break pushes the holder's next turn back (Wind). 0 for most. */
	int32 DelayTurnPlaces;
	/** B-064a: a share of the holder's max HP dealt once on landing, besides the applied effect (Ice, D-090). Can kill,
	 *  as Reality Burst. 0 for most (the rows that leave it out). */
	float DamageShareOfMaxHP = 0.0f;
};

namespace BreakTable
{
	/** The value a gate break carries. */
	constexpr float BREAK_GATE_VALUE = 1.0f;

	/** Burn's damage per payout on a holder without CharacterData (no max HP to share). */
	constexpr float BREAK_DOT_FALLBACK_DAMAGE = 30.0f;

	WORLD_OF_REFRACTION_API const TArray<FBreakRow> &Rows();

	/** The row for Trigger, or null when the trigger has no break row. */
	WORLD_OF_REFRACTION_API const FBreakRow *For(ESkillEffectType Trigger);

	/** Every row, one line each. */
	WORLD_OF_REFRACTION_API FString GetBreakTableString();
}
