// CameraFramingSpec.cpp
// B-372 (SkillPresentationPipeline.md §6): the framing checks themselves, then every code-built shot (B-371) on an
// empty test arena: what its moment must show projects inside the screen, in clear sight, with the camera clear.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr float FRAMING_TEST_FOV = 90.0f;
	constexpr float FRAMING_TEST_AHEAD = 1000.0f;
	/** A melee distance across the arena, and a second target beside the first (a spread). */
	const FVector FRAMING_TEST_TARGET(800.0f, 0.0f, 0.0f);
	const FVector FRAMING_TEST_SECOND_TARGET(800.0f, 300.0f, 0.0f);
	/** A wall between the camera and the target (review): half-size of the blocking box. */
	const FVector FRAMING_TEST_WALL_EXTENT(20.0f, 200.0f, 200.0f);

	FCameraShotContext FramingTestContext(bool bTwoTargets)
	{
		FCameraShotContext Context;
		Context.Targets.Add(FRAMING_TEST_TARGET);
		if (bTwoTargets)
		{
			Context.Targets.Add(FRAMING_TEST_SECOND_TARGET);
		}
		return Context;
	}
}

BEGIN_DEFINE_SPEC(FCameraFramingSpec, "WoR.Camera.Framing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCameraFramingSpec)

void FCameraFramingSpec::Define()
{
	It("a point ahead is on screen; beside the view, above it or behind it is not", [this]()
	{
		const FTransform Camera = FTransform::Identity; // looking along +X
		TestTrue(TEXT("straight ahead"), CameraFraming::IsOnScreen(Camera, FRAMING_TEST_FOV, CameraFraming::DEFAULT_ASPECT, FVector(FRAMING_TEST_AHEAD, 0.0f, 0.0f)));
		TestFalse(TEXT("past the side (45 degrees half view)"), CameraFraming::IsOnScreen(Camera, FRAMING_TEST_FOV, CameraFraming::DEFAULT_ASPECT, FVector(FRAMING_TEST_AHEAD, 2.0f * FRAMING_TEST_AHEAD, 0.0f)));
		TestFalse(TEXT("above (16:9 is shorter)"), CameraFraming::IsOnScreen(Camera, FRAMING_TEST_FOV, CameraFraming::DEFAULT_ASPECT, FVector(FRAMING_TEST_AHEAD, 0.0f, 0.7f * FRAMING_TEST_AHEAD)));
		TestFalse(TEXT("behind"), CameraFraming::IsOnScreen(Camera, FRAMING_TEST_FOV, CameraFraming::DEFAULT_ASPECT, FVector(-FRAMING_TEST_AHEAD, 0.0f, 0.0f)));
		TestFalse(TEXT("in the edge margin"), CameraFraming::IsOnScreen(Camera, FRAMING_TEST_FOV, CameraFraming::DEFAULT_ASPECT, FVector(FRAMING_TEST_AHEAD, 0.99f * FRAMING_TEST_AHEAD, 0.0f)));
	});

	It("each moment asks for its own points", [this]()
	{
		FCameraShotContext Context = FramingTestContext(true);
		TestEqual(TEXT("wind-up: caster and targets"), CameraFraming::GetRequiredPoints(ECameraShotMoment::WindUp, Context).Num(), 3);
		TestEqual(TEXT("impact: the first target"), CameraFraming::GetRequiredPoints(ECameraShotMoment::Impact, Context).Num(), 1);
		TestEqual(TEXT("spread: every target"), CameraFraming::GetRequiredPoints(ECameraShotMoment::Spread, Context).Num(), 2);
	});

	It("every code-built shot frames its moment on the empty arena, in every style", [this]()
	{
		FWoRTestWorld Arena;
		if (!TestTrue(TEXT("arena"), Arena.Create())) return;
		const TPair<ECameraShotMoment, bool> Cases[] = {
			{ECameraShotMoment::WindUp, false}, {ECameraShotMoment::Cast, false}, {ECameraShotMoment::Impact, false}, {ECameraShotMoment::Spread, true}};
		for (const ECameraShotStyle Style : {ECameraShotStyle::Default, ECameraShotStyle::Close, ECameraShotStyle::Wide})
		{
			FSkillCameraTuning Tuning;
			Tuning.Style = Style;
			for (const TPair<ECameraShotMoment, bool> &Case : Cases)
			{
				const FCameraShotContext Context = FramingTestContext(Case.Value);
				const FCameraShot Shot = FCombatCameraRules::ComputeShot(Case.Key, Context, Tuning);
				const CameraFraming::FFramingReport Report = CameraFraming::CheckShot(Arena.GetWorld(), Case.Key, Shot, Context, {});
				TestTrue(FString::Printf(TEXT("%s, %s: %s"), *UEnum::GetValueAsString(Style), *UEnum::GetValueAsString(Case.Key), *Report.ToString()),
						 Report.IsGood());
			}
		}
	});

	It("a wall between the camera and the target blocks the view (review)", [this]()
	{
		FWoRTestWorld Arena;
		if (!TestTrue(TEXT("arena"), Arena.Create())) return;
		UWorld *World = Arena.GetWorld();
		const FVector Camera(0.0f, 0.0f, 0.0f);
		const FVector WallAt(FRAMING_TEST_AHEAD * 0.5f, 0.0f, 0.0f);
		AActor *Wall = World ? World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(WallAt)) : nullptr;
		if (!TestNotNull(TEXT("wall"), Wall)) return;
		UBoxComponent *Box = NewObject<UBoxComponent>(Wall);
		Box->SetBoxExtent(FRAMING_TEST_WALL_EXTENT);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetRootComponent(Box);
		Box->RegisterComponent();
		Box->SetWorldLocation(WallAt);
		TestFalse(TEXT("the line through the wall"), CameraFraming::IsLineClear(World, Camera, FVector(FRAMING_TEST_AHEAD, 0.0f, 0.0f), {}));
		TestTrue(TEXT("ignoring the wall: clear"), CameraFraming::IsLineClear(World, Camera, FVector(FRAMING_TEST_AHEAD, 0.0f, 0.0f), {Wall}));
		TestFalse(TEXT("a camera inside the wall"), CameraFraming::IsCameraClear(World, WallAt));
		TestTrue(TEXT("a camera beside it"), CameraFraming::IsCameraClear(World, Camera));
	});

	It("a missing world counts as clear", [this]()
	{
		TestTrue(TEXT("line"), CameraFraming::IsLineClear(nullptr, FVector::ZeroVector, FVector(FRAMING_TEST_AHEAD, 0.0f, 0.0f), {}));
		TestTrue(TEXT("camera"), CameraFraming::IsCameraClear(nullptr, FVector::ZeroVector));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
