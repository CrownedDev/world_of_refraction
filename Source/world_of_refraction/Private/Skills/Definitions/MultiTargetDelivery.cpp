// MultiTargetDelivery.cpp

#include "Skills/Definitions/MultiTargetDelivery.h"

namespace WoRMultiTargetDelivery
{
	FVector GetWaveDirection(const FVector &Caster, const TArray<FVector> &Targets, const FVector &Forward)
	{
		FVector Middle = FVector::ZeroVector;
		for (const FVector &Target : Targets)
		{
			Middle += Target;
		}
		if (Targets.Num() > 0)
		{
			const FVector Toward = (Middle / Targets.Num() - Caster).GetSafeNormal2D();
			if (!Toward.IsZero())
			{
				return Toward;
			}
		}
		const FVector Flat = Forward.GetSafeNormal2D();
		return Flat.IsZero() ? FVector::ForwardVector : Flat;
	}

	TArray<float> GetArrivalSeconds(EMultiTargetDelivery Mode, const FVector &Caster, const TArray<FVector> &Targets, float Speed,
									const FVector &Forward)
	{
		const float SafeSpeed = FMath::Max(Speed, MIN_SPEED);
		const FVector WaveDirection = GetWaveDirection(Caster, Targets, Forward);
		TArray<float> Arrivals;
		Arrivals.Reserve(Targets.Num());
		for (const FVector &Target : Targets)
		{
			const float Distance = (Mode == EMultiTargetDelivery::OneBigWave)
									   ? FMath::Max(0.0f, FVector::DotProduct(Target - Caster, WaveDirection))
									   : FVector::Dist(Caster, Target);
			Arrivals.Add(Distance / SafeSpeed);
		}
		return Arrivals;
	}

	FString GetArrivalString(EMultiTargetDelivery Mode, const TArray<float> &Arrivals, float Speed)
	{
		TArray<FString> Parts;
		for (const float Seconds : Arrivals)
		{
			Parts.Add(FString::Printf(TEXT("%.2f s"), Seconds));
		}
		return FString::Printf(TEXT("%s at %.0f: %s"), *StaticEnum<EMultiTargetDelivery>()->GetNameStringByValue(static_cast<int64>(Mode)),
							   Speed, *FString::Join(Parts, TEXT(", ")));
	}
}
