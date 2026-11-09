// CameraFramingChecks.h
// B-372 (SkillPresentationPipeline.md §6): checks that a camera shot frames the action: the points the moment must show
// project inside the screen (with a margin), nothing blocks the line from the camera to them, and the camera is not
// inside geometry. FCombatCameraRules::ComputeShot's shots are checked by CameraFramingSpec on an empty arena; the
// director can log a failing shot in PIE (wor.Camera.CheckFraming 1). Screenshots per pattern are the editor step.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Camera/CombatCameraTypes.h"

class AActor;
class UWorld;

namespace CameraFraming
{
	/** Part of the screen's half-width / half-height kept free at the edges. TODO: tune in PIE. */
	constexpr float SCREEN_MARGIN = 0.05f;

	/** The aspect the checks assume (1080p, D-068). */
	constexpr float DEFAULT_ASPECT = 16.0f / 9.0f;

	/** The camera's radius for the "not inside geometry" check, in cm. */
	constexpr float CAMERA_CLEARANCE_RADIUS = 20.0f;

	/** Whether Point projects inside a perspective view from Camera (looking along its X axis) with a horizontal field of
	 *  view of FovDegrees, keeping Margin free at the edges. False behind the camera. */
	WORLD_OF_REFRACTION_API bool IsOnScreen(const FTransform &Camera, float FovDegrees, float Aspect, const FVector &Point, float Margin = SCREEN_MARGIN);

	/** The points a moment must show: wind-up and cast the caster and every target, an impact the caster and its
	 *  first target (B-403), a spread every target. */
	WORLD_OF_REFRACTION_API TArray<FVector> GetRequiredPoints(ECameraShotMoment Moment, const FCameraShotContext &Context);

	/** No blocking hit on the camera channel (characters ignore it) between From and To, ignoring Ignored. True without
	 *  a world. */
	WORLD_OF_REFRACTION_API bool IsLineClear(const UWorld *World, const FVector &From, const FVector &To, const TArray<const AActor *> &Ignored);

	/** The camera at Location doesn't overlap blocking geometry (a sphere of Radius). True without a world. */
	WORLD_OF_REFRACTION_API bool IsCameraClear(const UWorld *World, const FVector &Location, float Radius = CAMERA_CLEARANCE_RADIUS);

	/** One shot's result. */
	struct WORLD_OF_REFRACTION_API FFramingReport
	{
		int32 PointsChecked = 0;
		int32 PointsOffScreen = 0;
		int32 LinesBlocked = 0;
		bool bCameraClear = true;

		bool IsGood() const { return PointsOffScreen == 0 && LinesBlocked == 0 && bCameraClear; }
		FString ToString() const;
	};

	/** Checks Shot's end view for Moment: every required point on screen and in clear sight, the camera clear. */
	WORLD_OF_REFRACTION_API FFramingReport CheckShot(const UWorld *World, ECameraShotMoment Moment, const FCameraShot &Shot,
													 const FCameraShotContext &Context, const TArray<const AActor *> &Ignored,
													 float Aspect = DEFAULT_ASPECT);
}
