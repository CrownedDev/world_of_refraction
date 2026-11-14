// BattleTransitionSpec.cpp
// B-500 (D-212): the battle transition's maths and clock, headless. The look itself (shards, lens, the held
// picture over the map change) needs a viewport: docs/PIE/B-500.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/Transition/BattleTransitionData.h"
#include "Combat/Transition/BattleTransitionMath.h"
#include "Combat/Transition/BattleTransitionSubsystem.h"

namespace
{
	constexpr int32 TRANSITION_TEST_SEED = 500;
	constexpr float TRANSITION_TEST_ASPECT = 16.0f / 9.0f;
	constexpr float TRANSITION_TEST_FRAME = 1.0f / 60.0f;
	constexpr float TRANSITION_LONG_HOLD_SECONDS = 120.0f;
	constexpr double TRANSITION_AREA_TOLERANCE = 1.0e-4;
	/** B-500a: "slow" means behind an even pace at this share of the time. */
	constexpr float TRANSITION_EARLY_SHARE = 0.25f;
	constexpr float TRANSITION_HALF_SHARE = 0.5f;
	constexpr int32 TRANSITION_BLOW_STEPS = 20;
	/** The bevel leaves at least this share of a shard's area to its camera face (half its width, squared, with slack). */
	constexpr double TRANSITION_MIN_FACE_SHARE = 0.2;
	const FVector2D TRANSITION_TEST_HIT(0.35, 0.6);

	double TriangleArea(const FVector2D &A, const FVector2D &B, const FVector2D &C)
	{
		return 0.5 * FMath::Abs(FVector2D::CrossProduct(B - A, C - A));
	}

	/** Frames until the clock reports the blow-away finished; INDEX_NONE if it never does. */
	int32 AdvanceUntilFinished(BattleTransition::FClock &Clock, const UBattleTransitionData &Data, int32 MaxFrames)
	{
		for (int32 Frame = 1; Frame <= MaxFrames; ++Frame)
		{
			if (Clock.Advance(TRANSITION_TEST_FRAME, Data))
			{
				return Frame;
			}
		}
		return INDEX_NONE;
	}
}

BEGIN_DEFINE_SPEC(FBattleTransitionSpec, "WoR.Combat.BattleTransition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FBattleTransitionSpec)

void FBattleTransitionSpec::Define()
{
	It("cuts the whole overscanned screen into shards, the same ones for the same seed", [this]()
	{
		const UBattleTransitionData &Data = *GetDefault<UBattleTransitionData>();
		const TArray<BattleTransition::FShard> Shards =
			BattleTransition::BuildShards(TRANSITION_TEST_SEED, TRANSITION_TEST_HIT, TRANSITION_TEST_ASPECT, Data);
		TestEqual(TEXT("Two shards per cell"), Shards.Num(), Data.ShardColumns * Data.ShardRows * 2);

		double Area = 0.0;
		double SmallestArea = TNumericLimits<double>::Max();
		float FarthestDelay = 0.0f;
		for (const BattleTransition::FShard &Shard : Shards)
		{
			const double ShardArea = TriangleArea(Shard.Corners[0], Shard.Corners[1], Shard.Corners[2]);
			Area += ShardArea;
			SmallestArea = FMath::Min(SmallestArea, ShardArea);
			FarthestDelay = FMath::Max(FarthestDelay, Shard.Delay);
			TestTrue(TEXT("Outward is a unit vector"), FMath::IsNearlyEqual(Shard.Outward.Size(), 1.0, 1.0e-3));
		}
		// No gap and no overlap: the areas add up to the screen plus its overscan.
		TestTrue(TEXT("Shards cover the overscanned screen exactly"),
				 FMath::IsNearlyEqual(Area, FMath::Square(1.0 + 2.0 * BattleTransition::OVERSCAN), TRANSITION_AREA_TOLERANCE));
		TestTrue(TEXT("No empty shard"), SmallestArea > 0.0);
		TestEqual(TEXT("The farthest shard cracks last"), FarthestDelay, 1.0f);

		const TArray<BattleTransition::FShard> Again =
			BattleTransition::BuildShards(TRANSITION_TEST_SEED, TRANSITION_TEST_HIT, TRANSITION_TEST_ASPECT, Data);
		const TArray<BattleTransition::FShard> Other =
			BattleTransition::BuildShards(TRANSITION_TEST_SEED + 1, TRANSITION_TEST_HIT, TRANSITION_TEST_ASPECT, Data);
		bool bSame = Again.Num() == Shards.Num();
		bool bOtherDiffers = false;
		for (int32 Index = 0; bSame && Index < Shards.Num(); ++Index)
		{
			bSame = Again[Index].Centroid.Equals(Shards[Index].Centroid, 0.0) && Again[Index].RandomA == Shards[Index].RandomA;
			bOtherDiffers |= !Other[Index].Centroid.Equals(Shards[Index].Centroid, UE_KINDA_SMALL_NUMBER);
		}
		TestTrue(TEXT("Same seed, same shards (the battle map rebuilds them)"), bSame);
		TestTrue(TEXT("Another seed, another cut"), bOtherDiffers);
	});

	It("leaves the frame whole at contact, never reaches the camera in a hold, and passes it in the blow-away", [this]()
	{
		const UBattleTransitionData &Data = *GetDefault<UBattleTransitionData>();
		const TArray<BattleTransition::FShard> Shards =
			BattleTransition::BuildShards(TRANSITION_TEST_SEED, TRANSITION_TEST_HIT, TRANSITION_TEST_ASPECT, Data);
		for (const BattleTransition::FShard &Shard : Shards)
		{
			const BattleTransition::FShardPose AtContact = BattleTransition::EvaluatePose(Shard, Data, 0.0f, 0.0f);
			TestTrue(TEXT("At contact: in place"), AtContact.WallOffset.IsNearlyZero() && FMath::IsNearlyZero(AtContact.TowardCamera));
			TestTrue(TEXT("At contact: not turned"), AtContact.Rotation.Equals(FQuat::Identity));

			const BattleTransition::FShardPose Broken =
				BattleTransition::EvaluatePose(Shard, Data, Data.CrackSeconds + Data.BreakSeconds, 0.0f);
			TestTrue(TEXT("Broken: moved off the wall toward the camera"), Broken.TowardCamera > 0.0f);

			const BattleTransition::FShardPose LongHold = BattleTransition::EvaluatePose(Shard, Data, TRANSITION_LONG_HOLD_SECONDS, 0.0f);
			TestTrue(TEXT("A long hold never reaches the camera"), LongHold.TowardCamera < 1.0f);

			const BattleTransition::FShardPose Blown =
				BattleTransition::EvaluatePose(Shard, Data, TRANSITION_LONG_HOLD_SECONDS, Data.BlowAwaySeconds);
			TestTrue(TEXT("Blown away: past the camera"), Blown.TowardCamera >= 1.0f);
		}
		TestEqual(TEXT("No lens look at contact"), BattleTransition::GetLookAlpha(Data, 0.0f, 0.0f), 0.0f);
		TestEqual(TEXT("Full lens look in the hold"), BattleTransition::GetLookAlpha(Data, TRANSITION_LONG_HOLD_SECONDS, 0.0f), 1.0f);
		TestEqual(TEXT("Lens look gone after the blow-away"),
				  BattleTransition::GetLookAlpha(Data, TRANSITION_LONG_HOLD_SECONDS, Data.BlowAwaySeconds), 0.0f);
	});

	It("with the shipped defaults, opens slowly inside the join window and finishes fast (B-500a)", [this]()
	{
		// Pinned to the C++ defaults on purpose: an even pace (an ease of 1) is a legal tuning in the asset, but not what ships.
		const UBattleTransitionData &Data = *GetDefault<UBattleTransitionData>();
		TestTrue(TEXT("The opening fits inside an encounter's join window, so the map change lands in the hold"),
				 Data.CrackSeconds + Data.BreakSeconds < GetDefault<UEncounterComponent>()->JoinWindowSeconds);
		const TArray<BattleTransition::FShard> Shards =
			BattleTransition::BuildShards(TRANSITION_TEST_SEED, TRANSITION_TEST_HIT, TRANSITION_TEST_ASPECT, Data);
		const float BreakEnd = Data.CrackSeconds + Data.BreakSeconds;
		for (const BattleTransition::FShard &Shard : Shards)
		{
			const float FullBreak = BattleTransition::EvaluatePose(Shard, Data, BreakEnd, 0.0f).TowardCamera;
			const float EarlyBreak =
				BattleTransition::EvaluatePose(Shard, Data, Data.CrackSeconds + Data.BreakSeconds * TRANSITION_EARLY_SHARE, 0.0f).TowardCamera;
			TestTrue(TEXT("A quarter into the break: under a quarter of the way"), EarlyBreak < FullBreak * TRANSITION_EARLY_SHARE);

			// The blow-away: little in the first half (the slow beat), most in the second, never backwards.
			float Previous = BattleTransition::EvaluatePose(Shard, Data, TRANSITION_LONG_HOLD_SECONDS, 0.0f).TowardCamera;
			const float Held = Previous;
			bool bNeverBackwards = true;
			for (int32 Step = 1; Step <= TRANSITION_BLOW_STEPS; ++Step)
			{
				const float Release = Data.BlowAwaySeconds * Step / TRANSITION_BLOW_STEPS;
				const float Now = BattleTransition::EvaluatePose(Shard, Data, TRANSITION_LONG_HOLD_SECONDS + Release, Release).TowardCamera;
				bNeverBackwards &= Now >= Previous;
				Previous = Now;
			}
			TestTrue(TEXT("The blow-away only moves toward the camera"), bNeverBackwards);
			const float HalfRelease = Data.BlowAwaySeconds * TRANSITION_HALF_SHARE;
			const float AtHalf = BattleTransition::EvaluatePose(Shard, Data, TRANSITION_LONG_HOLD_SECONDS + HalfRelease, HalfRelease).TowardCamera;
			TestTrue(TEXT("Half the blow-away's time: under a quarter of its distance"), AtHalf - Held < (Previous - Held) * TRANSITION_EARLY_SHARE);
		}
	});

	It("builds each shard as a closed, outward-facing solid that carries the frame on its camera face", [this]()
	{
		const UBattleTransitionData &Data = *GetDefault<UBattleTransitionData>();
		const TArray<BattleTransition::FShard> Shards =
			BattleTransition::BuildShards(TRANSITION_TEST_SEED, TRANSITION_TEST_HIT, TRANSITION_TEST_ASPECT, Data);
		for (const BattleTransition::FShard &Shard : Shards)
		{
			const BattleTransition::FShardMesh Mesh = BattleTransition::BuildShardMesh(Shard, TRANSITION_TEST_ASPECT, Data);
			const int32 VertexCount = Mesh.Vertices.Num();
			// Two faces, three bevels and three sides of two triangles each; flat shading, so no shared vertices.
			if (!TestEqual(TEXT("Vertices"), VertexCount, (2 + 3 * 2 + 3 * 2) * 3) ||
				!TestTrue(TEXT("Every array matches"), Mesh.Triangles.Num() == VertexCount && Mesh.Normals.Num() == VertexCount &&
														   Mesh.UVs.Num() == VertexCount && Mesh.Colors.Num() == VertexCount))
			{
				return;
			}
			bool bFacesOutward = true;
			bool bCameraFaceCarriesFrame = false;
			for (int32 Index = 0; Index < VertexCount; Index += 3)
			{
				const FVector Middle = (Mesh.Vertices[Index] + Mesh.Vertices[Index + 1] + Mesh.Vertices[Index + 2]) / 3.0;
				bFacesOutward &= FVector::DotProduct(Mesh.Normals[Index], Middle) > 0.0;
				if (Mesh.Normals[Index].Equals(FVector::BackwardVector, UE_KINDA_SMALL_NUMBER))
				{
					// The camera face is not an edge, and its UVs are the shard's own piece of the screen, less the bevel.
					const double FaceShare = TriangleArea(Mesh.UVs[Index], Mesh.UVs[Index + 1], Mesh.UVs[Index + 2]) /
											 TriangleArea(Shard.Corners[0], Shard.Corners[1], Shard.Corners[2]);
					bCameraFaceCarriesFrame = Mesh.Colors[Index].R == 0 && FaceShare > TRANSITION_MIN_FACE_SHARE && FaceShare < 1.0;
				}
			}
			TestTrue(TEXT("Every triangle faces away from the shard's centre"), bFacesOutward);
			TestTrue(TEXT("The camera face shows its piece of the frame"), bCameraFaceCarriesFrame);
		}
	});

	It("carries the clock across the map change: held while frozen, settles on arrival, then blows away and ends", [this]()
	{
		const UBattleTransitionData &Data = *GetDefault<UBattleTransitionData>();
		BattleTransition::FClock Clock;
		TestFalse(TEXT("An idle clock doesn't freeze"), Clock.Freeze());
		Clock.Start();
		Clock.Advance(TRANSITION_TEST_FRAME, Data);
		const float ElapsedAtTravel = Clock.Elapsed;
		TestTrue(TEXT("Playing counts time"), ElapsedAtTravel > 0.0f);

		TestTrue(TEXT("Playing freezes for the map change"), Clock.Freeze());
		Clock.Advance(1.0f, Data);
		TestEqual(TEXT("Frozen: the clock stands still"), Clock.Elapsed, ElapsedAtTravel);
		TestEqual(TEXT("Frozen: the picture covers the screen"), Clock.GetPictureAlpha(Data), 1.0f);
		TestFalse(TEXT("Frozen: no release in place"), Clock.Release());

		TestTrue(TEXT("Frozen arrives"), Clock.Arrive(Data));
		Clock.Advance(10.0f, Data); // the first frame after a load can be seconds long: it counts as one short step
		TestTrue(TEXT("A long frame doesn't skip the settle"), Clock.State == BattleTransition::EState::Arriving);
		TestEqual(TEXT("Arriving: the shards wait behind the picture"), Clock.Elapsed, ElapsedAtTravel);

		const int32 FramesToFinish = AdvanceUntilFinished(Clock, Data, 600);
		const int32 ExpectedFrames = FMath::CeilToInt32((Data.ArrivalSettleSeconds + Data.BlowAwaySeconds) / TRANSITION_TEST_FRAME);
		TestTrue(TEXT("Settle, then the blow-away, then done"), FramesToFinish != INDEX_NONE && FMath::Abs(FramesToFinish - ExpectedFrames) <= 4);
		TestTrue(TEXT("Idle again"), Clock.State == BattleTransition::EState::Idle);
		TestEqual(TEXT("No picture when idle"), Clock.GetPictureAlpha(Data), 0.0f);
	});

	It("blows the shards away in place when released, or when no map change ever comes", [this]()
	{
		const UBattleTransitionData &Data = *GetDefault<UBattleTransitionData>();
		BattleTransition::FClock Released;
		Released.Start();
		TestTrue(TEXT("Playing releases"), Released.Release());
		TestTrue(TEXT("A released clock ends"), AdvanceUntilFinished(Released, Data, 600) != INDEX_NONE);

		BattleTransition::FClock Stuck;
		Stuck.Start();
		const int32 MaxFrames = FMath::CeilToInt32((Data.MaxHoldSeconds + Data.BlowAwaySeconds) / TRANSITION_TEST_FRAME) + 8;
		TestTrue(TEXT("A hold with no map change ends by itself"), AdvanceUntilFinished(Stuck, Data, MaxFrames) != INDEX_NONE);
	});

	It("does nothing on a machine with no view (a server, a headless test)", [this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestTrue(TEXT("Test world created"), TestWorld->Create())) return;
		UBattleTransitionSubsystem *Transition = UBattleTransitionSubsystem::Get(TestWorld->GetWorld());
		if (!TestNotNull(TEXT("Subsystem"), Transition)) return;
		Transition->Play(FVector::ZeroVector);
		TestTrue(TEXT("Still idle"), Transition->GetClock().State == BattleTransition::EState::Idle);
		TestEqual(TEXT("No shards"), Transition->GetShardCount(), 0);
		Transition->Release();
		Transition->Cancel();
		TestWorld.Reset();
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
