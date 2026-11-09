// CameraFramingChecks.cpp

#include "Combat/Camera/CameraFramingChecks.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** The field of view and margin the projection accepts (degrees; part of the half-extent). */
	constexpr double FRAMING_MIN_FOV = 1.0;
	constexpr double FRAMING_MAX_FOV = 179.0;
	constexpr double FRAMING_MAX_MARGIN = 0.9;
}

bool CameraFraming::IsOnScreen(const FTransform &Camera, float FovDegrees, float Aspect, const FVector &Point, float Margin)
{
	const FVector Local = Camera.InverseTransformPositionNoScale(Point);
	if (Local.X <= UE_KINDA_SMALL_NUMBER)
	{
		return false; // behind the camera
	}
	const double HalfTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(FovDegrees), FRAMING_MIN_FOV, FRAMING_MAX_FOV) * 0.5));
	const double SafeAspect = FMath::Max(static_cast<double>(Aspect), UE_KINDA_SMALL_NUMBER);
	const double ScreenX = Local.Y / (Local.X * HalfTan);
	const double ScreenY = Local.Z / (Local.X * HalfTan / SafeAspect);
	const double Limit = 1.0 - FMath::Clamp(static_cast<double>(Margin), 0.0, FRAMING_MAX_MARGIN);
	return FMath::Abs(ScreenX) <= Limit && FMath::Abs(ScreenY) <= Limit;
}

TArray<FVector> CameraFraming::GetRequiredPoints(ECameraShotMoment Moment, const FCameraShotContext &Context)
{
	TArray<FVector> Points;
	switch (Moment)
	{
	case ECameraShotMoment::WindUp:
	case ECameraShotMoment::Cast:
		Points.Add(Context.Caster);
		Points.Append(Context.Targets);
		break;
	case ECameraShotMoment::NamedAttack:
		Points.Add(Context.Caster); // B-406: the front shot frames the enemy alone; its targets are behind the camera
		break;
	case ECameraShotMoment::Impact:
		if (!Context.Targets.IsEmpty())
		{
			Points.Add(Context.Targets[0]);
		}
		break;
	case ECameraShotMoment::Spread:
	default:
		Points.Append(Context.Targets);
		break;
	}
	return Points;
}

bool CameraFraming::IsLineClear(const UWorld *World, const FVector &From, const FVector &To, const TArray<const AActor *> &Ignored)
{
	if (!World)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraFramingLine), /*bTraceComplex=*/false);
	for (const AActor *Actor : Ignored)
	{
		Params.AddIgnoredActor(Actor);
	}
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(Hit, From, To, ECC_Camera, Params);
}

bool CameraFraming::IsCameraClear(const UWorld *World, const FVector &Location, float Radius)
{
	if (!World)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraFramingClear), /*bTraceComplex=*/false);
	return !World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(Radius), Params);
}

FString CameraFraming::FFramingReport::ToString() const
{
	return FString::Printf(TEXT("%s: %d point(s), %d off screen, %d line(s) blocked, camera %s"), IsGood() ? TEXT("framed") : TEXT("NOT framed"),
						   PointsChecked, PointsOffScreen, LinesBlocked, bCameraClear ? TEXT("clear") : TEXT("inside geometry"));
}

CameraFraming::FFramingReport CameraFraming::CheckShot(const UWorld *World, ECameraShotMoment Moment, const FCameraShot &Shot,
													  const FCameraShotContext &Context, const TArray<const AActor *> &Ignored, float Aspect)
{
	FFramingReport Report;
	const FVector CameraLocation = Shot.End.GetLocation();
	for (const FVector &Point : GetRequiredPoints(Moment, Context))
	{
		++Report.PointsChecked;
		Report.PointsOffScreen += IsOnScreen(Shot.End, Shot.FieldOfView, Aspect, Point) ? 0 : 1;
		Report.LinesBlocked += IsLineClear(World, CameraLocation, Point, Ignored) ? 0 : 1;
	}
	Report.bCameraClear = IsCameraClear(World, CameraLocation);
	return Report;
}
