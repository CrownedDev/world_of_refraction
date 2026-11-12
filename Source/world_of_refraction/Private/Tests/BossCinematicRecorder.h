// BossCinematicRecorder.h
// Test-only listener for the boss cinematic broadcasts (B-406). Dynamic delegates need a UFUNCTION target, so a spec
// binds this to the camera director and reads back what came, in order.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "BossCinematicRecorder.generated.h"

UCLASS(Transient)
class UBossCinematicRecorder : public UObject
{
	GENERATED_BODY()

public:
	/** Every OnCinematicHudHidden value, in order (true = hidden). */
	TArray<bool> HudHidden;

	/** Every OnBossCinematicFinished kind, in order. */
	TArray<EBossCinematicKind> Finished;

	void Listen(UCombatCameraDirector *Director)
	{
		if (Director)
		{
			Director->OnCinematicHudHidden.AddDynamic(this, &UBossCinematicRecorder::HandleHudHidden);
			Director->OnBossCinematicFinished.AddDynamic(this, &UBossCinematicRecorder::HandleFinished);
		}
	}

	UFUNCTION()
	void HandleHudHidden(bool bHidden) { HudHidden.Add(bHidden); }

	UFUNCTION()
	void HandleFinished(EBossCinematicKind Kind) { Finished.Add(Kind); }
};
