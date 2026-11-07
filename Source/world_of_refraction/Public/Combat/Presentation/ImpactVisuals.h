// ImpactVisuals.h
// B-369 (Q-369, Design/SkillPresentationPipeline.md §3): what plays on an impact is chosen in code from the
// outcome, after defense resolves; animators place timing only.
//   Hit: the skill's Impact-role VFX entry → the element default → a generic hit (CosmeticsData)
//   Crit: the same, scaled by CritImpactVFXScale
//   Guard: a steel spark · Perfect Guard: nothing here (UDefenseSystem::PlayPerfectGuardFeedback plays the flash)
//   Dodge: a small whiff, or nothing
// A spell arrival draws its own Impact entry (the projectile / AOE), so a Hit or Crit there adds nothing.
// Spawned at the target's HitPoint socket, the actor location if it has none. Defaults come from the target's
// CosmeticsData (soft references, D-057), the asset that already holds the Perfect Guard flash (Q-369b).
// Debug: wor.ImpactVFX.Log 1 logs each choice (GetChoiceString).

#pragma once

#include "CoreMinimal.h"
#include "Combat/Defense/EDefenseType.h"
#include "Skills/Definitions/ESpellElement.h"

class AActor;
class UNiagaraSystem;
class UCosmeticsData;
class USkillDataBase;

enum class EImpactOutcome : uint8
{
	Hit,
	Crit,
	Guard,
	PerfectGuard,
	Dodge
};

/** Where the chosen visual came from (debug and specs). */
enum class EImpactVisualSource : uint8
{
	None,
	SkillEntry,
	ElementDefault,
	GenericDefault,
	GuardSpark,
	DodgeWhiff,
	/** Played by UDefenseSystem::PlayPerfectGuardFeedback, not here. */
	PerfectGuardFlash,
	/** A spell arrival: the projectile / AOE draws the skill's own Impact entry. */
	SpellArrival
};

struct WORLD_OF_REFRACTION_API FImpactVisualChoice
{
	TSoftObjectPtr<UNiagaraSystem> VFX;
	EImpactVisualSource Source = EImpactVisualSource::None;
	float Scale = 1.0f;

	bool ShouldSpawn() const { return !VFX.IsNull(); }
};

namespace ImpactVisuals
{
	/** The socket an impact visual plays at on the target's mesh. */
	inline const FName HIT_POINT_SOCKET(TEXT("HitPoint"));

	/** A successful Dodge → Dodge; a successful Guard → Guard / PerfectGuard; anything else lands: Crit or Hit. */
	WORLD_OF_REFRACTION_API EImpactOutcome OutcomeFor(EDefenseType DefenseType, bool bSuccess, bool bPerfect, bool bCritical);

	/** The visual for Outcome. Skill and Cosmetics may be null (the chain then skips that step). */
	WORLD_OF_REFRACTION_API FImpactVisualChoice Choose(EImpactOutcome Outcome, const USkillDataBase *Skill, ESpellElement Element,
														const UCosmeticsData *Cosmetics, bool bIsSpellArrival);

	/** The target's HitPoint socket, else its location. */
	WORLD_OF_REFRACTION_API FVector GetSpawnLocation(const AActor *Target);

	/** Chooses with the target's own CosmeticsData and spawns at GetSpawnLocation. */
	WORLD_OF_REFRACTION_API void SpawnForImpact(AActor *Target, EImpactOutcome Outcome, const USkillDataBase *Skill, ESpellElement Element,
												bool bIsSpellArrival);

	/** "Crit: ElementDefault NS_FireHit x1.50". */
	WORLD_OF_REFRACTION_API FString GetChoiceString(EImpactOutcome Outcome, const FImpactVisualChoice &Choice);
}
