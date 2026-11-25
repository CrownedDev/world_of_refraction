// SkillSounds.h
// B-618: plays a skill's SFXArray entries (FSkillSFXEntry). The runner plays a Notify entry on its SFX notify;
// ImpactVisuals::SpawnForImpact plays the Impact entries on a landed hit (a projectile's arrival too).
// Sounds play on the machine that runs the fight. TODO: send them to co-op clients like FCosmeticVFX (B-231c).
// Debug: wor.SkillSFX.Log 1 logs each sound played.

#pragma once

#include "CoreMinimal.h"

class UObject;
class USceneComponent;
class USkillDataBase;
struct FSkillSFXEntry;
enum class EImpactOutcome : uint8;

namespace SkillSounds
{
	/** The Impact entries with a sound to play for Outcome: all of them on a Hit or Crit, none on a Guard, Perfect
	 *  Guard or Dodge (the defence plays its own). */
	WORLD_OF_REFRACTION_API TArray<const FSkillSFXEntry *> ChooseImpactSounds(EImpactOutcome Outcome, const USkillDataBase *Skill);

	/** Plays Entry at Location (loads the sound on first use). False when it has no sound. */
	WORLD_OF_REFRACTION_API bool PlayAtLocation(const UObject *WorldContext, const FSkillSFXEntry &Entry, const FVector &Location);

	/** Plays Entry following Socket of Parent. False when it has no sound. */
	WORLD_OF_REFRACTION_API bool PlayAttached(const FSkillSFXEntry &Entry, USceneComponent *Parent, FName Socket);

	/** "SFX: 0 Release (Notify) SC_Wind_Slash_Cue | 1 Hit (Impact) (no sound)". */
	WORLD_OF_REFRACTION_API FString GetSoundsString(const USkillDataBase *Skill);
}
