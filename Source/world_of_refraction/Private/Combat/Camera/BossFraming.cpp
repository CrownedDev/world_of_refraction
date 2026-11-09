// BossFraming.cpp

#include "Combat/Camera/BossFraming.h"
#include "Combat/Camera/CombatCameraTypes.h"

namespace BossFramingOffsets
{
	// TODO: tune in PIE (B-405). Distances in cm, from the actor's location (its capsule centre).
	/** The low turn view: the camera's height above the acting character. */
	constexpr float LowCameraHeight = 40.0f;
	/** The low turn view: the point above the enemies' centre it aims at (upper body), so it looks up. */
	constexpr float LowLookAtHeight = 220.0f;
	constexpr float KillWideBack = 1200.0f;
	constexpr float KillWideHeight = 520.0f;
	constexpr float KillCloseBack = 260.0f;
	constexpr float KillCloseSide = 140.0f;
	constexpr float KillCloseHeight = 20.0f;
	constexpr float KillCloseLookAtHeight = 110.0f;
}

namespace
{
	FTransform BossFramingLookFrom(const FVector &Location, const FVector &Target)
	{
		return FTransform((Target - Location).Rotation(), Location);
	}

	bool IsBossFramingRank(EEncounterRank Rank)
	{
		return Rank == EEncounterRank::Boss || Rank == EEncounterRank::MiniBoss;
	}
}

EEnemyCameraHeight FBossFramingRules::ResolveHeight(EEnemyCameraHeight Authored, EEncounterRank Rank)
{
	if (Authored != EEnemyCameraHeight::ByRank)
	{
		return Authored;
	}
	return IsBossFramingRank(Rank) ? EEnemyCameraHeight::Low : EEnemyCameraHeight::EyeLevel;
}

EEnemyCameraHeight FBossFramingRules::FightHeight(const TArray<EEnemyCameraHeight> &LivingEnemyHeights)
{
	return LivingEnemyHeights.Contains(EEnemyCameraHeight::Low) ? EEnemyCameraHeight::Low : EEnemyCameraHeight::EyeLevel;
}

FTransform FBossFramingRules::ApplyHeight(const FTransform &View, EEnemyCameraHeight Height, ETurnViewStyle Style, const FCombatCameraFrame &Frame)
{
	using namespace BossFramingOffsets;
	if (Height != EEnemyCameraHeight::Low || Style == ETurnViewStyle::Tactical)
	{
		return View;
	}
	FVector Location = View.GetLocation();
	Location.Z = FMath::Min(Location.Z, Frame.Focus.Z + LowCameraHeight);
	return BossFramingLookFrom(Location, Frame.EnemyCentre + FVector::UpVector * LowLookAtHeight);
}

EKillShotKind FBossFramingRules::KillShotFor(EEncounterRank Rank)
{
	return IsBossFramingRank(Rank) ? EKillShotKind::Boss : EKillShotKind::Normal;
}

float FBossFramingRules::KillShotSeconds(EKillShotKind Kind)
{
	return Kind == EKillShotKind::Boss ? BossKillShotSeconds : NormalKillShotSeconds;
}

bool FBossFramingRules::DarkensView(EKillShotKind Kind)
{
	return Kind == EKillShotKind::Normal;
}

FTransform FBossFramingRules::ComputeKillShot(EKillShotKind Kind, const FCombatCameraFrame &Frame, const FVector &Victim)
{
	using namespace BossFramingOffsets;
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	if (Kind == EKillShotKind::Boss)
	{
		return BossFramingLookFrom(Victim - Forward * KillCloseBack + Right * KillCloseSide + FVector::UpVector * KillCloseHeight,
								   Victim + FVector::UpVector * KillCloseLookAtHeight);
	}
	return BossFramingLookFrom(Frame.ArenaCentre - Forward * KillWideBack + FVector::UpVector * KillWideHeight, Victim);
}
