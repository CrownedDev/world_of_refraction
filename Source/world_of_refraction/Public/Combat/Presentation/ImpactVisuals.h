// ImpactVisuals.h
// B-369 (Q-369, Q-369b changed by Crown 2026-11-08, Design/SkillPresentationPipeline.md §3): what plays on an impact is
// chosen in code from the outcome, after defense resolves; animators place timing only. Three layers:
//   1. the skill's effect (cast, projectile, slash) is on the skill (not chosen here);
//   2. the impact, Hit and Crit, is on the skill: every delivery plays the skill's Impact entry (a Crit plays its
//      CritImpact entry if it has one, else the Impact entry larger); with no Impact entry, the element default, then a
//      generic one (UImpactVisualDefaults), so a fireball's impact is always fire;
//   3. the defence outcome is the defender's cosmetic slot (UCosmeticsData): Guard a spark, Perfect Guard the flash
//      UDefenseSystem::PlayPerfectGuardFeedback plays, Dodge a whiff (a cosmetic can swap it, e.g. a transmutation
//      circle when you land a parry; the parked skins / cosmetics work).
// A projectile carries the skill's Impact entry and plays it on arrival, so a projectile's Hit or Crit adds nothing
// here. TODO: a projectile's crit with a CritImpact entry still shows the Impact entry.
// Spawned at the target's HitPoint socket, the actor location if it has none.
// Debug: wor.ImpactVFX.Log 1 logs each choice (GetChoiceString).

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "Combat/Defense/EDefenseType.h"
#include "Skills/Definitions/ESpellElement.h"

class AActor;
class UNiagaraSystem;
class UCosmeticsData;
class UImpactVisualDefaults;
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
	/** The skill's CritImpact entry (Q-369b). */
	SkillCritEntry,
	ElementDefault,
	GenericDefault,
	GuardSpark,
	DodgeWhiff,
	/** Played by UDefenseSystem::PlayPerfectGuardFeedback, not here. */
	PerfectGuardFlash,
	/** A projectile arrival: the projectile plays the skill's own Impact entry. */
	ProjectileArrival
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

	/** The crit scale with no defaults asset. */
	constexpr float FALLBACK_CRIT_SCALE = 1.5f;

	/** The visual for Outcome: Hit / Crit from Skill, then Defaults (element, generic); Guard / Perfect Guard / Dodge
	 *  from the defender's cosmetics. Any of the three may be null (that step is skipped). */
	WORLD_OF_REFRACTION_API FImpactVisualChoice Choose(EImpactOutcome Outcome, const USkillDataBase *Skill, ESpellElement Element,
														const UImpactVisualDefaults *Defaults, const UCosmeticsData *DefenderCosmetics,
														bool bIsProjectileArrival);

	/** The target's HitPoint socket, else its location. */
	WORLD_OF_REFRACTION_API FVector GetSpawnLocation(const AActor *Target);

	/** Chooses with UImpactVisualDefaults::Get and the target's (defender's) CosmeticsData, and spawns at
	 *  GetSpawnLocation. */
	WORLD_OF_REFRACTION_API void SpawnForImpact(AActor *Target, EImpactOutcome Outcome, const USkillDataBase *Skill, ESpellElement Element,
												bool bIsProjectileArrival);

	/** "Crit: ElementDefault NS_FireHit x1.50". */
	WORLD_OF_REFRACTION_API FString GetChoiceString(EImpactOutcome Outcome, const FImpactVisualChoice &Choice);
}
