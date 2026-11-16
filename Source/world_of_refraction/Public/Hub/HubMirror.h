// HubMirror.h
// D-234 (B-541): the hub's mirror (Press E). One mirror for the player's look and, later, the loadout presets
// (D-221 stage 3). Its rows are Spirit, Presets and Cosmetics, and only the ones that work show: today that is
// Spirit alone, so the mirror opens the Spirit screen (UFormsScreenWidget) directly. Placed in the hub map.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "HubMirror.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API AHubMirror : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

protected:
	virtual FString GetHintText() const override;
};
