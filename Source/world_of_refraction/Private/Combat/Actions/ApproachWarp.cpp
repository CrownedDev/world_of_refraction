// ApproachWarp.cpp

#include "Combat/Actions/ApproachWarp.h"

#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Combat/CombatConstants.h"
#include "HAL/IConsoleManager.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

FString FApproachBurst::ToString() const
{
	if (!bHasWindow)
	{
		return TEXT("no warp window");
	}
	return FString::Printf(TEXT("warp %.0f cm: clip %.0f before + %.0f after, burst %.0f cm in %.2f s (%.2f-%.2f, hop x%.2f) = %.0f cm/s%s"),
						   WarpDistance, TravelBefore, TravelAfter, BurstDistance, GetWindowSeconds(), WindowStart, WindowEnd, HopRateScale,
						   BurstSpeed, bUseCameraCut ? TEXT(", camera cut") : TEXT(""));
}

namespace
{
	// TODO: Crown picks the base (B-424 PIE: 0.24 s, the clip's own hop, read as max speed; try 0.35 / 0.45 / 0.55).
	constexpr float DEFAULT_APPROACH_BURST_SECONDS = 0.45f;

	TAutoConsoleVariable<float> CVarApproachBurstSeconds(
		TEXT("wor.Combat.ApproachBurstSeconds"), DEFAULT_APPROACH_BURST_SECONDS,
		TEXT("B-424: how long a melee approach's air-burst dash lasts, in seconds; the hop is slowed to fit and the camera "
			 "rush follows. 0 = the clip's own hop (0.24 s). Read per action, so it can be changed mid-fight."));
}

namespace ApproachWarp
{
	FVector GetStrikePosition(const FVector &Caster, const FVector &Target, float Range)
	{
		const FVector DirToTarget = (Target - Caster).GetSafeNormal2D();
		FVector Strike = Target - DirToTarget * Range;
		Strike.Z = Caster.Z;
		return Strike;
	}

	float GetApproachBurstSeconds()
	{
		return FMath::Max(CVarApproachBurstSeconds.GetValueOnGameThread(), 0.0f);
	}

	FApproachBurst MakeBurst(float Start, float End, float TravelBefore, float TravelAfter, float WarpDistance, float PlayRate, float BurstSeconds)
	{
		const float Rate = FMath::Max(PlayRate, UE_KINDA_SMALL_NUMBER);
		FApproachBurst Burst;
		Burst.bHasWindow = End > Start;
		Burst.WindowStart = Start / Rate;
		const float ClipSeconds = FMath::Max(End - Start, 0.0f) / Rate;
		const float Seconds = Burst.bHasWindow && BurstSeconds > 0.0f ? BurstSeconds : ClipSeconds;
		Burst.WindowEnd = Burst.WindowStart + Seconds;
		Burst.HopRateScale = Seconds > 0.0f ? ClipSeconds / Seconds : 1.0f;
		Burst.TravelBefore = TravelBefore;
		Burst.TravelAfter = TravelAfter;
		Burst.WarpDistance = WarpDistance;
		Burst.BurstDistance = FMath::Max(WarpDistance - TravelBefore - TravelAfter, 0.0f);
		const float Seconds = Burst.GetWindowSeconds();
		Burst.BurstSpeed = Seconds > 0.0f ? Burst.BurstDistance / Seconds : 0.0f;
		Burst.bUseCameraCut = Burst.bHasWindow && Burst.BurstSpeed > CAMERA_CUT_BURST_SPEED;
		return Burst;
	}

	FApproachBurst Measure(const UAnimMontage *Montage, float WarpDistance, float PlayRate, float BurstSeconds)
	{
		if (!Montage)
		{
			return FApproachBurst();
		}
		const FName WarpTargetName(CombatConstants::WARP_TARGET_NAME);
		for (const FAnimNotifyEvent &Event : Montage->Notifies)
		{
			const UAnimNotifyState_MotionWarping *WarpState = Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass);
			const URootMotionModifier_Warp *Warp = WarpState ? Cast<URootMotionModifier_Warp>(WarpState->RootMotionModifier) : nullptr;
			if (!Warp || Warp->WarpTargetName != WarpTargetName)
			{
				continue;
			}
			const float Start = Event.GetTriggerTime();
			const float End = Event.GetEndTriggerTime();
			const float Before = static_cast<float>(UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Montage, 0.0f, Start).GetTranslation().Size2D());
			// The warp only stops short by the travel after its window when the notify says so (UE's own option).
			const float After = Warp->bSubtractRemainingRootMotion
									? static_cast<float>(UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Montage, End, Montage->GetPlayLength()).GetTranslation().Size2D())
									: 0.0f;
			return MakeBurst(Start, End, Before, After, WarpDistance, PlayRate, BurstSeconds);
		}
		return FApproachBurst();
	}
}
