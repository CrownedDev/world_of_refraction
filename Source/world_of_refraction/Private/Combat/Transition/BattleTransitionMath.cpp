// BattleTransitionMath.cpp

#include "Combat/Transition/BattleTransitionMath.h"

#include "Combat/Transition/BattleTransitionData.h"
#include "Math/RandomStream.h"

namespace
{
	/** A cut point moves at most this share of a cell: under half, so no shard turns inside out. */
	constexpr float CELL_JITTER = 0.35f;

	/** Cut points nearer the hit than this (UV) crowd toward it. */
	constexpr double PULL_REACH = 0.5;

	/** The farthest shard cracks at this share of the crack time; the rest is each shard's own tilt. */
	constexpr float CRACK_SPREAD_SHARE = 0.8f;

	/** Every shard moves at least this share of a motion value; its random picks the rest. */
	constexpr float MIN_MOTION_SHARE = 0.4f;

	/** Extra turn as a shard blows past the camera. */
	constexpr float BLOW_AWAY_TUMBLE_DEGREES = 90.0f;

	/** The bevel never eats more than half of a small shard's face. */
	constexpr double MIN_FACE_SCALE = 0.5;

	constexpr float MAX_COLOR_BYTE = 255.0f;

	FVector2D WallToUV(const FVector2D &Wall, float Aspect)
	{
		return FVector2D(Wall.X + 0.5, 0.5 - Wall.Y * Aspect);
	}

	BattleTransition::FShard MakeShard(const FVector2D &A, const FVector2D &B, const FVector2D &C, const FVector2D &HitUV,
									   float Aspect, FRandomStream &Random)
	{
		BattleTransition::FShard Shard;
		Shard.Corners[0] = A;
		Shard.Corners[1] = B;
		Shard.Corners[2] = C;
		Shard.Centroid = (A + B + C) / 3.0;
		const FVector2D FromHit = BattleTransition::UVToWall(Shard.Centroid, Aspect) - BattleTransition::UVToWall(HitUV, Aspect);
		Shard.Delay = FromHit.Size(); // a distance for now; BuildShards scales it to 0..1
		const float OutwardAngle = Random.FRandRange(0.0f, UE_TWO_PI);
		Shard.Outward = FromHit.IsNearlyZero() ? FVector2D(FMath::Cos(OutwardAngle), FMath::Sin(OutwardAngle)) : FromHit.GetSafeNormal();
		Shard.RandomA = Random.FRand();
		Shard.RandomB = Random.FRand();
		// Mostly in the wall's plane, so a turning shard shows its edge and back.
		const float AxisAngle = Random.FRandRange(0.0f, UE_TWO_PI);
		Shard.TumbleAxis = FVector(Random.FRandRange(-0.3f, 0.3f), FMath::Cos(AxisAngle), FMath::Sin(AxisAngle)).GetSafeNormal();
		return Shard;
	}

	/** Collects flat-shaded triangles of a solid whose centre is the origin. */
	struct FShardMeshBuilder
	{
		BattleTransition::FShardMesh Mesh;
		FVector2D Center = FVector2D::ZeroVector; // the shard's centroid on the wall
		float Aspect = 1.0f;
		FColor FaceColor = FColor::Black;

		void AddTriangle(const FVector &A, FVector B, FVector C, bool bEdge)
		{
			// The engine's front face: its normal is (C - A) x (B - A).
			FVector Normal = FVector::CrossProduct(C - A, B - A);
			if (Normal.SizeSquared() < UE_DOUBLE_SMALL_NUMBER * UE_DOUBLE_SMALL_NUMBER)
			{
				return; // no bevel, or a sliver: nothing to draw
			}
			if (FVector::DotProduct(Normal, A + B + C) < 0.0)
			{
				Swap(B, C); // face outward
				Normal = -Normal;
			}
			Normal.Normalize();
			FColor Color = FaceColor;
			Color.R = bEdge ? 255 : 0;
			for (const FVector &Vertex : {A, B, C})
			{
				Mesh.Triangles.Add(Mesh.Vertices.Add(Vertex));
				Mesh.Normals.Add(Normal);
				Mesh.UVs.Add(WallToUV(Center + FVector2D(Vertex.Y, Vertex.Z), Aspect));
				Mesh.Colors.Add(Color);
			}
		}

		void AddQuad(const FVector &A, const FVector &B, const FVector &C, const FVector &D, bool bEdge)
		{
			AddTriangle(A, B, C, bEdge);
			AddTriangle(A, C, D, bEdge);
		}
	};
}

FVector2D BattleTransition::UVToWall(const FVector2D &UV, float Aspect)
{
	return FVector2D(UV.X - 0.5, (0.5 - UV.Y) / FMath::Max(Aspect, UE_KINDA_SMALL_NUMBER));
}

TArray<BattleTransition::FShard> BattleTransition::BuildShards(int32 Seed, const FVector2D &HitUV, float Aspect,
															   const UBattleTransitionData &Data)
{
	const int32 Columns = FMath::Max(Data.ShardColumns, 1);
	const int32 Rows = FMath::Max(Data.ShardRows, 1);
	const double Span = 1.0 + 2.0 * OVERSCAN;
	FRandomStream Random(Seed);

	TArray<FVector2D> Points;
	Points.Reserve((Columns + 1) * (Rows + 1));
	for (int32 Row = 0; Row <= Rows; ++Row)
	{
		for (int32 Column = 0; Column <= Columns; ++Column)
		{
			FVector2D Point(-OVERSCAN + Span * Column / Columns, -OVERSCAN + Span * Row / Rows);
			const bool bOnEdge = Column == 0 || Column == Columns || Row == 0 || Row == Rows;
			if (!bOnEdge) // edge points stay put, so the wall always covers the screen
			{
				Point.X += Random.FRandRange(-CELL_JITTER, CELL_JITTER) * Span / Columns;
				Point.Y += Random.FRandRange(-CELL_JITTER, CELL_JITTER) * Span / Rows;
				const FVector2D FromHit = Point - HitUV;
				const double Nearness = 1.0 - FMath::Min(FromHit.Size() / PULL_REACH, 1.0);
				Point = HitUV + FromHit * (1.0 - Data.HitPull * Nearness);
			}
			Points.Add(Point);
		}
	}

	TArray<FShard> Shards;
	Shards.Reserve(Columns * Rows * 2);
	float FarthestDistance = UE_KINDA_SMALL_NUMBER;
	const auto AddShard = [&](int32 A, int32 B, int32 C)
	{
		const FShard &Shard = Shards.Add_GetRef(MakeShard(Points[A], Points[B], Points[C], HitUV, Aspect, Random));
		FarthestDistance = FMath::Max(FarthestDistance, Shard.Delay);
	};
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const int32 TopLeft = Row * (Columns + 1) + Column;
			const int32 TopRight = TopLeft + 1;
			const int32 BottomLeft = TopLeft + Columns + 1;
			const int32 BottomRight = BottomLeft + 1;
			if (Random.FRand() < 0.5f)
			{
				AddShard(TopLeft, TopRight, BottomRight);
				AddShard(TopLeft, BottomRight, BottomLeft);
			}
			else
			{
				AddShard(TopLeft, TopRight, BottomLeft);
				AddShard(TopRight, BottomRight, BottomLeft);
			}
		}
	}
	for (FShard &Shard : Shards)
	{
		Shard.Delay /= FarthestDistance;
	}
	return Shards;
}

BattleTransition::FShardPose BattleTransition::EvaluatePose(const FShard &Shard, const UBattleTransitionData &Data,
															float Elapsed, float ReleaseElapsed)
{
	const float CrackAt = Shard.Delay * Data.CrackSeconds * CRACK_SPREAD_SHARE;
	const float CrackTiltSeconds = FMath::Max(Data.CrackSeconds * (1.0f - CRACK_SPREAD_SHARE), UE_KINDA_SMALL_NUMBER);
	const float Crack = FMath::Clamp((Elapsed - CrackAt) / CrackTiltSeconds, 0.0f, 1.0f);

	const float BreakLinear = FMath::Clamp((Elapsed - Data.CrackSeconds) / FMath::Max(Data.BreakSeconds, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float Break = 1.0f - FMath::Square(1.0f - BreakLinear); // fast, then easing into the hold

	const float HoldSeconds = FMath::Max(Elapsed - Data.CrackSeconds - Data.BreakSeconds, 0.0f);
	const float Drift = 1.0f - FMath::Exp(-HoldSeconds / FMath::Max(Data.DriftSeconds, UE_KINDA_SMALL_NUMBER)); // slows, never arrives

	const float BlowLinear = FMath::Clamp(ReleaseElapsed / FMath::Max(Data.BlowAwaySeconds, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float Blow = FMath::Square(BlowLinear); // speeding up past the camera

	const float ShareA = FMath::Lerp(MIN_MOTION_SHARE, 1.0f, Shard.RandomA);
	const float ShareB = FMath::Lerp(MIN_MOTION_SHARE, 1.0f, Shard.RandomB);
	const float TurnSign = Shard.RandomB < 0.5f ? -1.0f : 1.0f;

	FShardPose Pose;
	Pose.WallOffset = Shard.Outward * (ShareA * (Data.BreakSpread * Break + Data.BlowAwaySpread * Blow));
	Pose.TowardCamera = ShareB * (Data.BreakTowardCamera * Break + Data.DriftTowardCamera * Drift) + Data.BlowAwayTowardCamera * Blow;
	const float Degrees = TurnSign * (ShareA * (Data.CrackTiltDegrees * Crack + Data.BreakTumbleDegrees * Break) +
									  ShareB * Data.DriftTumbleDegreesPerSecond * HoldSeconds + BLOW_AWAY_TUMBLE_DEGREES * Blow);
	Pose.Rotation = FQuat(Shard.TumbleAxis, FMath::DegreesToRadians(Degrees));
	return Pose;
}

float BattleTransition::GetLookAlpha(const UBattleTransitionData &Data, float Elapsed, float ReleaseElapsed)
{
	const float On = FMath::SmoothStep(0.0f, Data.CrackSeconds + Data.BreakSeconds, Elapsed);
	const float Off = FMath::Clamp(ReleaseElapsed / FMath::Max(Data.BlowAwaySeconds, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	return On * (1.0f - Off);
}

BattleTransition::FShardMesh BattleTransition::BuildShardMesh(const FShard &Shard, float Aspect, const UBattleTransitionData &Data)
{
	FShardMeshBuilder Builder;
	Builder.Center = UVToWall(Shard.Centroid, Aspect);
	Builder.Aspect = Aspect;
	Builder.FaceColor = FColor(0, static_cast<uint8>(FMath::RoundToInt32(Shard.RandomA * MAX_COLOR_BYTE)),
							   static_cast<uint8>(FMath::RoundToInt32(Shard.Delay * MAX_COLOR_BYTE)), 255);

	FVector2D Rim[3];
	double MeanRadius = 0.0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Rim[Index] = UVToWall(Shard.Corners[Index], Aspect) - Builder.Center;
		MeanRadius += Rim[Index].Size() / 3.0;
	}
	const double FaceScale = FMath::Max(1.0 - Data.ShardBevel / FMath::Max(MeanRadius, UE_DOUBLE_KINDA_SMALL_NUMBER), MIN_FACE_SCALE);

	const double FrontX = -Data.ShardThickness * 0.5; // toward the camera
	const double BackX = Data.ShardThickness * 0.5;
	const double RimX = FMath::Min(FrontX + Data.ShardBevel, BackX);
	const auto At = [](double X, const FVector2D &Wall) { return FVector(X, Wall.X, Wall.Y); };

	Builder.AddTriangle(At(FrontX, Rim[0] * FaceScale), At(FrontX, Rim[1] * FaceScale), At(FrontX, Rim[2] * FaceScale), false);
	Builder.AddTriangle(At(BackX, Rim[0]), At(BackX, Rim[1]), At(BackX, Rim[2]), false);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D &From = Rim[Index];
		const FVector2D &To = Rim[(Index + 1) % 3];
		Builder.AddQuad(At(FrontX, From * FaceScale), At(FrontX, To * FaceScale), At(RimX, To), At(RimX, From), true); // bevel
		Builder.AddQuad(At(RimX, From), At(RimX, To), At(BackX, To), At(BackX, From), true);						   // side
	}
	return MoveTemp(Builder.Mesh);
}

void BattleTransition::FClock::Start()
{
	Reset();
	State = EState::Playing;
}

bool BattleTransition::FClock::Freeze()
{
	if (State != EState::Playing)
	{
		return false;
	}
	State = EState::Frozen;
	return true;
}

bool BattleTransition::FClock::Arrive(const UBattleTransitionData &Data)
{
	if (State != EState::Frozen)
	{
		return false;
	}
	State = EState::Arriving;
	SettleRemaining = Data.ArrivalSettleSeconds;
	return true;
}

bool BattleTransition::FClock::Release()
{
	if (State != EState::Playing)
	{
		return false;
	}
	State = EState::Releasing;
	return true;
}

void BattleTransition::FClock::Reset()
{
	*this = FClock();
}

bool BattleTransition::FClock::Advance(float DeltaSeconds, const UBattleTransitionData &Data)
{
	const float Step = FMath::Clamp(DeltaSeconds, 0.0f, MAX_STEP_SECONDS);
	switch (State)
	{
	case EState::Playing:
		Elapsed += Step;
		if (Elapsed >= Data.MaxHoldSeconds)
		{
			State = EState::Releasing; // no map change came: don't leave the shards up
		}
		return false;
	case EState::Arriving:
		SettleRemaining -= Step;
		if (SettleRemaining <= 0.0f)
		{
			State = EState::Releasing;
		}
		return false;
	case EState::Releasing:
		Elapsed += Step;
		ReleaseElapsed += Step;
		if (ReleaseElapsed >= Data.BlowAwaySeconds)
		{
			Reset();
			return true;
		}
		return false;
	default:
		return false;
	}
}

float BattleTransition::FClock::GetPictureAlpha(const UBattleTransitionData &Data) const
{
	switch (State)
	{
	case EState::Frozen:
	case EState::Arriving:
		return 1.0f;
	case EState::Releasing:
		return 1.0f - FMath::Clamp(ReleaseElapsed / FMath::Max(Data.PictureFadeSeconds, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	default:
		return 0.0f;
	}
}

const TCHAR *BattleTransition::GetStateName(EState State)
{
	switch (State)
	{
	case EState::Idle:      return TEXT("Idle");
	case EState::Playing:   return TEXT("Playing");
	case EState::Frozen:    return TEXT("Frozen");
	case EState::Arriving:  return TEXT("Arriving");
	case EState::Releasing: return TEXT("Releasing");
	default:                return TEXT("?");
	}
}
