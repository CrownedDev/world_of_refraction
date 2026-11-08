// CombatCinematicWindow.h
// B-370 (D-162, Design/SkillPresentationPipeline.md §4): the Cinematic notify state, a range on a skill montage.
// Inside it the cinematic camera plays; at its end the camera director hands each viewer whose own character is
// targeted to the Defence view. A marker only: the director reads the range when the action starts
// (FCombatCameraRules::ReadWindowTiming). The window is cut short so it ends MinDefenseReadSeconds before the
// first defendable moment. Without one, the window runs from the start to the wind-up end (Q-370).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CombatCinematicWindow.generated.h"

UCLASS(meta = (DisplayName = "Cinematic Window"))
class WORLD_OF_REFRACTION_API UCombatCinematicWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
};
