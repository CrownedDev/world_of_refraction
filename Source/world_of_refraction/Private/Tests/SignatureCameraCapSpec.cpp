// SignatureCameraCapSpec.cpp
// B-407: a player or team skill's own CameraSequence is cut at the cap; an enemy's (a boss's) is not
// (Combat/Camera/SignatureCameraCap.h).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Combat/Camera/SignatureCameraCap.h"
#include "LevelSequence.h"
#include "MovieScene.h"

namespace
{
	constexpr float SIGNATURE_CAP_TEST_TOLERANCE = 0.01f;
	constexpr float SIGNATURE_CAP_TEST_LONG_SECONDS = 4.0f;
	constexpr float SIGNATURE_CAP_TEST_SHORT_SECONDS = 1.5f;

	/** A transient sequence whose playback range is Seconds long. */
	ULevelSequence *MakeSignatureTestSequence(float Seconds)
	{
		ULevelSequence *Sequence = NewObject<ULevelSequence>(GetTransientPackage());
		Sequence->Initialize();
		UMovieScene *MovieScene = Sequence->GetMovieScene();
		const FFrameNumber Duration = MovieScene->GetTickResolution().AsFrameNumber(Seconds);
		MovieScene->SetPlaybackRange(FFrameNumber(0), Duration.Value);
		return Sequence;
	}

	using namespace SignatureCameraCap;
}

BEGIN_DEFINE_SPEC(FSignatureCameraCapSpec, "WoR.Combat.Camera.SignatureCap",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSignatureCameraCapSpec)

void FSignatureCameraCapSpec::Define()
{
	It("cuts a 4 s sequence on a player or team skill at the cap", [this]()
	{
		TestTrue(TEXT("4 s is over the cap"), IsOverCap(SIGNATURE_CAP_TEST_LONG_SECONDS));
		TestEqual(TEXT("cut at the cap"), GetCutSeconds(SIGNATURE_CAP_TEST_LONG_SECONDS, /*bEnemyCaster=*/false), MaxSignatureSeconds, SIGNATURE_CAP_TEST_TOLERANCE);
	});

	It("lets a sequence within the cap play to its end", [this]()
	{
		TestFalse(TEXT("1.5 s is within the cap"), IsOverCap(SIGNATURE_CAP_TEST_SHORT_SECONDS));
		TestEqual(TEXT("not cut"), GetCutSeconds(SIGNATURE_CAP_TEST_SHORT_SECONDS, false), 0.0f);
		TestEqual(TEXT("exactly the cap: not cut"), GetCutSeconds(MaxSignatureSeconds, false), 0.0f);
	});

	It("never caps an enemy's (a boss's) sequence", [this]()
	{
		TestEqual(TEXT("enemy 4 s: plays to its end"), GetCutSeconds(SIGNATURE_CAP_TEST_LONG_SECONDS, /*bEnemyCaster=*/true), 0.0f);
	});

	It("reads a sequence's length from its playback range", [this]()
	{
		TestEqual(TEXT("4 s sequence"), GetSequenceSeconds(MakeSignatureTestSequence(SIGNATURE_CAP_TEST_LONG_SECONDS)), SIGNATURE_CAP_TEST_LONG_SECONDS, SIGNATURE_CAP_TEST_TOLERANCE);
		TestEqual(TEXT("no sequence"), GetSequenceSeconds(nullptr), 0.0f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
