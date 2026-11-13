// ApproachWarpDebug.cpp
// B-424: inspect a montage's approach burst without PIE: wor.Debug.ApproachWarp <MontagePath> [WarpDistanceCm] [PlayRate]

#include "Combat/Actions/ApproachWarp.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace ApproachWarpDebug
{
	/** A front-row strike: teams 800 cm apart, minus the default 100 cm striking range. */
	constexpr float DEFAULT_WARP_DISTANCE = 700.0f;
	constexpr float MESSAGE_SECONDS = 10.0f;

	FString GetApproachWarpString(const FString &MontagePath, float WarpDistance, float PlayRate)
	{
		const UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
		if (!Montage)
		{
			return FString::Printf(TEXT("[ApproachWarp] no montage at %s"), *MontagePath);
		}
		return FString::Printf(TEXT("[ApproachWarp] %s at %.0f cm, rate %.2f: %s"), *Montage->GetName(), WarpDistance, PlayRate,
							   *ApproachWarp::Measure(Montage, WarpDistance, PlayRate).ToString());
	}

	static FAutoConsoleCommand GApproachWarpCommand(
		TEXT("wor.Debug.ApproachWarp"),
		TEXT("DEBUG (B-424): a montage's approach burst: wor.Debug.ApproachWarp <MontagePath> [WarpDistanceCm=700] [PlayRate=1]. "
			 "Prints the WarpTarget window, the clip's own travel around it, the burst speed, and whether the camera would cut."),
		FConsoleCommandWithArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args)
			{
				if (Args.IsEmpty())
				{
					UE_LOG(LogWoR, Warning, TEXT("[ApproachWarp] usage: wor.Debug.ApproachWarp <MontagePath> [WarpDistanceCm] [PlayRate]"));
					return;
				}
				const float Distance = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : DEFAULT_WARP_DISTANCE;
				const float Rate = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 1.0f;
				const FString Line = GetApproachWarpString(Args[0], Distance, Rate);
				UE_LOG(LogWoR, Display, TEXT("%s"), *Line);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(INDEX_NONE, MESSAGE_SECONDS, FColor::Cyan, Line);
				}
			}));
}
