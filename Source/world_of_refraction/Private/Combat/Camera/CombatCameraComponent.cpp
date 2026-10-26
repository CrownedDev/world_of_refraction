// CombatCameraComponent.cpp

#include "Combat/Camera/CombatCameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

ACameraActor *UCombatCameraComponent::ShowView(ECombatCameraMode NewMode, const FTransform &View)
{
	Mode = NewMode;
	if (!CameraActor)
	{
		UWorld *World = GetWorld();
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), View, Params);
		if (CameraActor)
		{
			// Keep the viewport's aspect instead of letterboxing to the camera's default.
			CameraActor->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	else
	{
		CameraActor->SetActorTransform(View);
	}
	return CameraActor;
}

void UCombatCameraComponent::ReleaseCamera()
{
	if (IsValid(CameraActor))
	{
		CameraActor->Destroy();
	}
	CameraActor = nullptr;
	Mode = ECombatCameraMode::None;
}

void UCombatCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseCamera();
	Super::EndPlay(EndPlayReason);
}
