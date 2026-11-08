// NotifyValidation.h
// B-368 (Design/SkillPresentationPipeline.md §6): headless checks of a skill's montage notifies against its arrays.
// - Impact notifies are 0..HitCount-1, each once, in time order (a spell or a skill with a CastArray may have
//   none: its hits are the casts' arrivals).
// - Cast notify indices fit CastArray (an empty CastArray takes any Cast notify: the loose-field fallback).
// - VFX notify indices fit VFXArray.
// - A cinematic window ends at least MIN_DEFENSE_READ_SECONDS before the first defendable moment (§4, D-162):
//   CinematicWindowFits. Windows come with B-370; until then no montage carries one.
// The chain RitualCastMontage → SkillMontage → ReturnMontage is read as one timeline, at the skill's BaseAnimSpeed.
// Debug: wor.Presentation.Validate (NotifyValidationDebug.cpp) prints every offending skill asset.

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatNotify.h"
#include "Combat/Camera/CombatCameraTypes.h"

class UAnimMontage;
class USkillDataBase;

namespace NotifyValidation
{
	/** The least time a defender gets between the end of a cinematic window and the first defendable moment
	 *  (D-162). One source: B-370's FCombatCameraRules (tune it there). */
	constexpr float MIN_DEFENSE_READ_SECONDS = FCombatCameraRules::MinDefenseReadSeconds;

	/** One UCombatNotify on the chain's timeline, in wall-clock seconds from the chain's start. */
	struct FNotifyMark
	{
		ECombatNotifyFamily Family = ECombatNotifyFamily::Impact;
		int32 Index = 0;
		float Seconds = 0.0f;
		float TelegraphLeadSeconds = 0.0f;
	};

	/** What the marks are checked against. */
	struct FSkillArrays
	{
		int32 HitCount = 1;
		int32 CastCount = 0;
		int32 VFXCount = 0;
		/** A spell (or any skill with a CastArray): its hits may be its casts' arrivals, so no Impact notify is
		 *  fine; any Impact notifies it does carry must still be 0..HitCount-1 in order. */
		bool bHitsMayComeFromCasts = false;
	};

	/** The montage's UCombatNotify marks in time order (ties keep authored order), at PlayRate, shifted by
	 *  OffsetSeconds. Other notifies are skipped. */
	WORLD_OF_REFRACTION_API TArray<FNotifyMark> ReadMarks(const UAnimMontage *Montage, float PlayRate, float OffsetSeconds);

	/** The skill's whole chain as one timeline (Ritual, Skill, Return; missing legs are skipped). */
	WORLD_OF_REFRACTION_API TArray<FNotifyMark> ReadSkillMarks(const USkillDataBase &Skill);

	/** One line per problem; empty when the marks fit the arrays. Marks must be in time order. */
	WORLD_OF_REFRACTION_API TArray<FString> CheckMarks(const TArray<FNotifyMark> &Marks, const FSkillArrays &Arrays);

	/** The first defendable moment: the earliest Impact or Cast mark minus its telegraph lead (never below 0).
	 *  False when the marks hold neither.
	 *  TODO: one timeline at BaseAnimSpeed - the runner also multiplies spell speed and action mods, and a deferred
	 *  ritual plays as its own arm turn at 1.0, so the seconds are approximate for ritual spells (index checks exact). */
	WORLD_OF_REFRACTION_API bool FirstDefendableSeconds(const TArray<FNotifyMark> &Marks, float &OutSeconds);

	/** The fairness rule (§4): a window ending at WindowEndSeconds leaves the defender MIN_DEFENSE_READ_SECONDS. */
	WORLD_OF_REFRACTION_API bool CinematicWindowFits(float WindowEndSeconds, float FirstDefendable);

	/** CheckMarks on the skill's chain. Empty for a skill with no SkillMontage (the montage-less path). */
	WORLD_OF_REFRACTION_API TArray<FString> CheckSkill(const USkillDataBase &Skill);

	/** Every skill asset's problems, one line each, after a first line counting the assets scanned. Loads every
	 *  USkillDataBase asset: editor / test only. */
	WORLD_OF_REFRACTION_API FString GetValidationReportString();
}
