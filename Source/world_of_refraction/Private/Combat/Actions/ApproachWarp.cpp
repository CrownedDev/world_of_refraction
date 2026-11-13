// ApproachWarp.cpp

#include "Combat/Actions/ApproachWarp.h"

#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Combat/CombatConstants.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

FString FApproachBurst::ToString() const
{
	if (!bHasWindow)
	{
		return TEXT("no warp window");
	}
	return FString::Printf(TEXT("warp %.0f cm: clip %.0f before + %.0f after, burst %.0f cm in %.2f s (%.2f-%.2f) = %.0f cm/s%s"),
						   WarpDistance, TravelBefore, TravelAfter, BurstDistance, GetWindowSeconds(), WindowStart, WindowEnd, BurstSpeed,
						   bUseCameraCut ? TEXT(", camera cut") : TEXT(""));
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

	FApproachBurst MakeBurst(float Start, float End, float TravelBefore, float TravelAfter, float WarpDistance, float PlayRate)
	{
		const float Rate = FMath::Max(PlayRate, UE_KINDA_SMALL_NUMBER);
		FApproachBurst Burst;
		Burst.bHasWindow = End > Start;
		Burst.WindowStart = Start / Rate;
		Burst.WindowEnd = End / Rate;
		Burst.TravelBefore = TravelBefore;
		Burst.TravelAfter = TravelAfter;
		Burst.WarpDistance = WarpDistance;
		Burst.BurstDistance = FMath::Max(WarpDistance - TravelBefore - TravelAfter, 0.0f);
		const float Seconds = Burst.GetWindowSeconds();
		Burst.BurstSpeed = Seconds > 0.0f ? Burst.BurstDistance / Seconds : 0.0f;
		Burst.bUseCameraCut = Burst.bHasWindow && Burst.BurstSpeed > CAMERA_CUT_BURST_SPEED;
		return Burst;
	}

	FApproachBurst Measure(const UAnimMontage *Montage, float WarpDistance, float PlayRate)
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
			return MakeBurst(Start, End, Before, After, WarpDistance, PlayRate);
		}
		return FApproachBurst();
	}
}
