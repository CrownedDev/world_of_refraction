// ApproachWarp.h

#pragma once

#include "CoreMinimal.h"

class UAnimMontage;

/** B-424: the travel of one warped leg (approach or return): the montage's WarpTarget window and how fast the warp
 *  has to move the actor inside it. The air-burst dash warps only the airborne frames, so the extra distance lands
 *  where no foot can slide (docs/analysis/ShowcaseAnimAnalysis.md, "Travel without stretching"). */
struct WORLD_OF_REFRACTION_API FApproachBurst
{
	/** The montage has a Motion Warping window on WarpTarget. Without one nothing below is meaningful. */
	bool bHasWindow = false;
	/** The window, in seconds of real time (montage time over the play rate). */
	float WindowStart = 0.0f;
	float WindowEnd = 0.0f;
	/** The clip's own travel before the window, and after it when the notify subtracts it (cm, flat). */
	float TravelBefore = 0.0f;
	float TravelAfter = 0.0f;
	/** From the actor to the warp target (cm, flat). */
	float WarpDistance = 0.0f;
	/** What the warp covers inside the window (cm), and how fast (cm/s). */
	float BurstDistance = 0.0f;
	float BurstSpeed = 0.0f;
	/** Above ApproachWarp::CAMERA_CUT_BURST_SPEED the burst reads as a blink: the camera cuts instead of rushing along. */
	bool bUseCameraCut = false;

	float GetWindowSeconds() const { return FMath::Max(WindowEnd - WindowStart, 0.0f); }
	FString ToString() const;
};

namespace ApproachWarp
{
	// TODO: tune (Q-424): the burst speed past which the camera cuts to the arrival instead of rushing along.
	constexpr float CAMERA_CUT_BURST_SPEED = 3500.0f;

	/** The striking spot: Range short of Target on the line from Caster, at Caster's height (W1's approach formula). */
	WORLD_OF_REFRACTION_API FVector GetStrikePosition(const FVector &Caster, const FVector &Target, float Range);

	/** The burst for a window [Start, End] (montage seconds) at PlayRate, given the clip's travel around it. Pure. */
	WORLD_OF_REFRACTION_API FApproachBurst MakeBurst(float Start, float End, float TravelBefore, float TravelAfter, float WarpDistance, float PlayRate);

	/** Reads Montage's first WarpTarget window and its root motion, for a leg of WarpDistance cm. No window: bHasWindow false. */
	WORLD_OF_REFRACTION_API FApproachBurst Measure(const UAnimMontage *Montage, float WarpDistance, float PlayRate);
}
