// MultiTargetDelivery.h
// B-607 (D-275): when each target of a several-target Projectile cast is hit. OnePerTarget (Slash Volley): a copy
// flies straight at each target, so each is hit after its own distance. OneBigWave (Fan Slash): one wave leaves the
// caster toward the targets' middle, so each is hit when the wave's front has come as far forward as it stands (two
// side by side are hit together, however wide apart). The executor times the wave's hits from this; the copies fly
// as ordinary projectiles. Debug: GetArrivalString.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellDeliveryType.h"

namespace WoRMultiTargetDelivery
{
	/** A speed at or below zero is read as this (units a second), so no hit waits forever. */
	constexpr float MIN_SPEED = 1.0f;

	/** The way the wave travels, flat: from Caster toward the middle of Targets; Forward when they stand on the
	 *  caster (or there are none); +X when that is flat zero too. */
	WORLD_OF_REFRACTION_API FVector GetWaveDirection(const FVector &Caster, const TArray<FVector> &Targets, const FVector &Forward);

	/** Seconds after the cast at which each of Targets is hit, in Targets' order (see this file's header). A target
	 *  behind the wave's start is hit at once. */
	WORLD_OF_REFRACTION_API TArray<float> GetArrivalSeconds(EMultiTargetDelivery Mode, const FVector &Caster, const TArray<FVector> &Targets,
															float Speed, const FVector &Forward);

	/** Debug: "OneBigWave at 1500: 0.20 s, 0.20 s, 0.60 s". */
	WORLD_OF_REFRACTION_API FString GetArrivalString(EMultiTargetDelivery Mode, const TArray<float> &Arrivals, float Speed);
}
