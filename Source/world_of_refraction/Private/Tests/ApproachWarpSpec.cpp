// ApproachWarpSpec.cpp
// B-424 (air-burst dash): the burst a warped approach makes inside its window, the striking spot, and the melee rush
// timed to that burst (or cut past CAMERA_CUT_BURST_SPEED).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Actions/ApproachWarp.h"
#include "Combat/Camera/CombatCameraTypes.h"

namespace
{
	/** The light hit's dash (ShowcaseAnimAnalysis.md): window 0.18-0.42 s, travel after the window ~124 cm. */
	constexpr float DASH_WINDOW_START = 0.18f;
	constexpr float DASH_WINDOW_END = 0.42f;
	constexpr float DASH_TRAVEL_BEFORE = 75.0f;
	constexpr float DASH_TRAVEL_AFTER = 124.0f;
	/** A front-row and a back-row target, from the attacker to the striking spot (cm). */
	constexpr float FRONT_ROW_DISTANCE = 700.0f;
	constexpr float BACK_ROW_DISTANCE = 1100.0f;
	constexpr float FAST_PLAY_RATE = 1.2f;
	constexpr float TOLERANCE = 0.01f;
}

BEGIN_DEFINE_SPEC(FApproachWarpSpec, "WoR.Combat.ApproachWarp", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FApproachWarpSpec)

void FApproachWarpSpec::Define()
{
	Describe("MakeBurst", [this]()
	{
		It("warps only what the clip doesn't travel, inside the window", [this]()
		{
			const FApproachBurst Burst = ApproachWarp::MakeBurst(DASH_WINDOW_START, DASH_WINDOW_END, DASH_TRAVEL_BEFORE, DASH_TRAVEL_AFTER, FRONT_ROW_DISTANCE, 1.0f);
			TestTrue(TEXT("has a window"), Burst.bHasWindow);
			TestEqual(TEXT("burst distance"), Burst.BurstDistance, FRONT_ROW_DISTANCE - DASH_TRAVEL_BEFORE - DASH_TRAVEL_AFTER, TOLERANCE);
			TestEqual(TEXT("burst speed"), Burst.BurstSpeed, Burst.BurstDistance / (DASH_WINDOW_END - DASH_WINDOW_START), TOLERANCE);
			TestFalse(TEXT("a front-row dash is followed by the camera"), Burst.bUseCameraCut);
		});

		It("cuts the camera when the burst is too fast to follow (a back-row target)", [this]()
		{
			const FApproachBurst Burst = ApproachWarp::MakeBurst(DASH_WINDOW_START, DASH_WINDOW_END, DASH_TRAVEL_BEFORE, DASH_TRAVEL_AFTER, BACK_ROW_DISTANCE, 1.0f);
			TestTrue(TEXT("over the cut speed"), Burst.BurstSpeed > ApproachWarp::CAMERA_CUT_BURST_SPEED);
			TestTrue(TEXT("camera cut"), Burst.bUseCameraCut);
		});

		It("shortens the window by the play rate", [this]()
		{
			const FApproachBurst Burst = ApproachWarp::MakeBurst(DASH_WINDOW_START, DASH_WINDOW_END, 0.0f, 0.0f, FRONT_ROW_DISTANCE, FAST_PLAY_RATE);
			TestEqual(TEXT("start"), Burst.WindowStart, DASH_WINDOW_START / FAST_PLAY_RATE, TOLERANCE);
			TestEqual(TEXT("end"), Burst.WindowEnd, DASH_WINDOW_END / FAST_PLAY_RATE, TOLERANCE);
		});

		It("never warps backwards when the clip already travels far enough", [this]()
		{
			const FApproachBurst Burst = ApproachWarp::MakeBurst(DASH_WINDOW_START, DASH_WINDOW_END, DASH_TRAVEL_BEFORE, DASH_TRAVEL_AFTER, 100.0f, 1.0f);
			TestEqual(TEXT("no burst"), Burst.BurstDistance, 0.0f);
			TestFalse(TEXT("no cut"), Burst.bUseCameraCut);
		});

		It("has no window for an empty range, nor without a montage", [this]()
		{
			TestFalse(TEXT("empty window"), ApproachWarp::MakeBurst(0.4f, 0.4f, 0.0f, 0.0f, FRONT_ROW_DISTANCE, 1.0f).bHasWindow);
			TestFalse(TEXT("no montage"), ApproachWarp::Measure(nullptr, FRONT_ROW_DISTANCE, 1.0f).bHasWindow);
		});
	});

	It("puts the striking spot the range short of the target, at the attacker's height", [this]()
	{
		const FVector Strike = ApproachWarp::GetStrikePosition(FVector::ZeroVector, FVector(800.0f, 0.0f, 50.0f), 100.0f);
		TestTrue(TEXT("700 cm out, on the floor"), Strike.Equals(FVector(700.0f, 0.0f, 0.0f), TOLERANCE));
	});

	Describe("FollowApproachBurst", [this]()
	{
		FCameraShot Rush;
		Rush.Start = FTransform(FVector(-420.0f, 0.0f, 0.0f));
		Rush.End = FTransform(FVector(-150.0f, 0.0f, 0.0f));
		Rush.BlendSeconds = 0.25f;
		Rush.MotionBlur = 0.8f;
		FCameraShot AtStrike;
		AtStrike.End = FTransform(FVector(550.0f, 0.0f, 0.0f));

		It("rushes along the burst to the striking spot's framing", [this, Rush, AtStrike]()
		{
			const FCameraShot Shot = FCombatCameraRules::FollowApproachBurst(Rush, AtStrike, DASH_WINDOW_START, DASH_WINDOW_END, false);
			TestTrue(TEXT("starts where the rush starts"), Shot.Start.Equals(Rush.Start));
			TestTrue(TEXT("ends framed at the striking spot"), Shot.End.Equals(AtStrike.End));
			TestEqual(TEXT("waits for the burst"), Shot.DelaySeconds, DASH_WINDOW_START, TOLERANCE);
			TestEqual(TEXT("moves for the burst's length"), Shot.BlendSeconds, DASH_WINDOW_END - DASH_WINDOW_START, TOLERANCE);
			TestEqual(TEXT("keeps the rush blur"), Shot.MotionBlur, Rush.MotionBlur);
		});

		It("holds, then cuts as the burst ends, when too fast to follow", [this, Rush, AtStrike]()
		{
			const FCameraShot Shot = FCombatCameraRules::FollowApproachBurst(Rush, AtStrike, DASH_WINDOW_START, DASH_WINDOW_END, true);
			TestTrue(TEXT("ends framed at the striking spot"), Shot.End.Equals(AtStrike.End));
			TestEqual(TEXT("cuts at the burst's end"), Shot.DelaySeconds, DASH_WINDOW_END, TOLERANCE);
			TestEqual(TEXT("a cut, not a move"), Shot.BlendSeconds, 0.0f);
			TestEqual(TEXT("no blur"), Shot.MotionBlur, 0.0f);
		});
	});
}

#endif
