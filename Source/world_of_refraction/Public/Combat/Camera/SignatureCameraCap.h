// SignatureCameraCap.h
// B-407 (study R7, FeelGuide 9 as changed 2026-11-09, Q-407): a player or team skill's own CameraSequence (a signature
// move) is kept short. The director cuts back to the turn view at MaxSignatureSeconds; an enemy caster's sequence (a
// boss or named attack, Q3) runs to its end. NotifyValidation (B-368) warns on a sequence longer than the cap.
// Debug: wor.Camera.SignatureCap (SignatureCameraCapDebug.cpp).

#pragma once

#include "CoreMinimal.h"

class ULevelSequence;

namespace SignatureCameraCap
{
	/** The longest a player or team skill's own sequence plays before the director cuts back (Expedition 33's player
	 *  skill shots are about 1-2 s; Q-407). TODO: tune in PIE (docs/PIE/B-407.md). */
	constexpr float MaxSignatureSeconds = 2.0f;

	/** A sequence this little over the cap is not cut (frame rounding). TODO: tune. */
	constexpr float CapToleranceSeconds = 0.05f;

	/** True when a sequence of SequenceSeconds is longer than the cap. */
	WORLD_OF_REFRACTION_API bool IsOverCap(float SequenceSeconds);

	/** How long the director lets the sequence play before cutting back: MaxSignatureSeconds for a player or team
	 *  caster's sequence over the cap; 0 (play to its end) for an enemy caster or a sequence within the cap. */
	WORLD_OF_REFRACTION_API float GetCutSeconds(float SequenceSeconds, bool bEnemyCaster);

	/** The sequence's playback range in seconds; 0 when it has none. */
	WORLD_OF_REFRACTION_API float GetSequenceSeconds(const ULevelSequence *Sequence);
}
