// CombatCameraComponent.h
// One combat camera per character (Design/CombatCamera.md §1). Added to each combatant for one
// fight; the director decides its mode and which camera the viewer sees.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "CombatCameraComponent.generated.h"

class ACameraActor;

UCLASS(ClassGroup = (Combat))
class WORLD_OF_REFRACTION_API UCombatCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Puts this character's camera in Mode at View, spawning it on first use. */
	ACameraActor *ShowView(ECombatCameraMode NewMode, const FTransform &View);

	/** Destroys the camera. Called when the fight ends. */
	void ReleaseCamera();

	ECombatCameraMode GetMode() const { return Mode; }
	ACameraActor *GetCameraActor() const { return CameraActor; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> CameraActor;

	ECombatCameraMode Mode = ECombatCameraMode::None;
};
