// SignatureCameraCap.cpp
// B-407: see SignatureCameraCap.h.

#include "Combat/Camera/SignatureCameraCap.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"

namespace SignatureCameraCap
{
	bool IsOverCap(float SequenceSeconds)
	{
		return SequenceSeconds > MaxSignatureSeconds + CapToleranceSeconds;
	}

	float GetCutSeconds(float SequenceSeconds, bool bEnemyCaster)
	{
		return !bEnemyCaster && IsOverCap(SequenceSeconds) ? MaxSignatureSeconds : 0.0f;
	}

	float GetSequenceSeconds(const ULevelSequence *Sequence)
	{
		const UMovieScene *MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
		if (!MovieScene)
		{
			return 0.0f;
		}
		const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
		if (!Range.HasLowerBound() || !Range.HasUpperBound())
		{
			return 0.0f;
		}
		const int32 Ticks = UE::MovieScene::DiscreteSize(Range);
		return Ticks > 0 ? static_cast<float>(MovieScene->GetTickResolution().AsSeconds(FFrameTime(Ticks))) : 0.0f;
	}
}
