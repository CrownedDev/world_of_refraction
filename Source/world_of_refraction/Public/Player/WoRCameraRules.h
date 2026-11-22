// WoRCameraRules.h
// D-255 (Crown, 2026-11-22; B-583): the field camera's limits. In the hub and trial the look goes at most 10 degrees up
// and 55 down (AWoRHubController sets them on its camera manager). From contact to the fight (UEncounterComponent) the
// engaged player's look stays within 30 degrees of yaw and 10 of pitch of where it pointed, inside those limits. The
// fight's own camera is untouched. The camera manager clamps the control rotation (APlayerCameraManager::LimitViewYaw /
// LimitViewPitch, FMath::ClampAngle), so a yaw range across 0/360 needs no special case.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"

namespace WoRCameraRules
{
	/** D-255 (a): with the 550 arm and the 70 socket, 10 up keeps the camera about 60 cm off flat ground. */
	constexpr float FIELD_PITCH_UP_MAX = 10.0f;
	constexpr float FIELD_PITCH_DOWN_MAX = 55.0f;
	/** D-255 (b): how far the look may turn from where it pointed at contact. */
	constexpr float CONTACT_YAW_HALF_RANGE = 30.0f;
	constexpr float CONTACT_PITCH_HALF_RANGE = 10.0f;

	/** A camera manager's look limits, kept so a hold can give back exactly what it found. */
	struct FLookLimits
	{
		float PitchMin = -FIELD_PITCH_DOWN_MAX;
		float PitchMax = FIELD_PITCH_UP_MAX;
		float YawMin = 0.0f;
		float YawMax = 359.999f; // the engine's default: no yaw limit
	};

	inline FLookLimits Read(const APlayerCameraManager &Camera)
	{
		return {Camera.ViewPitchMin, Camera.ViewPitchMax, Camera.ViewYawMin, Camera.ViewYawMax};
	}

	inline void Apply(APlayerCameraManager &Camera, const FLookLimits &Limits)
	{
		Camera.ViewPitchMin = Limits.PitchMin;
		Camera.ViewPitchMax = Limits.PitchMax;
		Camera.ViewYawMin = Limits.YawMin;
		Camera.ViewYawMax = Limits.YawMax;
	}

	/** D-255 (a): the hub and trial pitch limits, yaw free. */
	inline void ApplyFieldLimits(APlayerCameraManager &Camera)
	{
		FLookLimits Limits = Read(Camera);
		Limits.PitchMin = -FIELD_PITCH_DOWN_MAX;
		Limits.PitchMax = FIELD_PITCH_UP_MAX;
		Apply(Camera, Limits);
	}

	/** D-255 (b): the contact limits around Look (a control rotation), the pitch kept inside Field's. */
	inline FLookLimits MakeContactLimits(const FRotator &Look, const FLookLimits &Field)
	{
		const float Pitch = FRotator::NormalizeAxis(Look.Pitch);
		const float Yaw = FRotator::ClampAxis(Look.Yaw);
		FLookLimits Limits;
		Limits.PitchMin = FMath::Max(Pitch - CONTACT_PITCH_HALF_RANGE, Field.PitchMin);
		Limits.PitchMax = FMath::Max(FMath::Min(Pitch + CONTACT_PITCH_HALF_RANGE, Field.PitchMax), Limits.PitchMin);
		Limits.YawMin = Yaw - CONTACT_YAW_HALF_RANGE; // may be below 0 or past 360: ClampAngle wraps it
		Limits.YawMax = Yaw + CONTACT_YAW_HALF_RANGE;
		return Limits;
	}
}
