// HubJournalStation.h
// B-194 (HubAndStory §4, Q-194): the hub spot (Press E) that opens the placeholder memory journal:
// memories and lore to reread, V's next line, unlocked conversations. Placed in the hub map.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "HubJournalStation.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API AHubJournalStation : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	virtual void Interact(APawn *Pawn) override;

protected:
	virtual FString GetHintText() const override;
};
