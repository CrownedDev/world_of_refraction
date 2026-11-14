// HubJournalStation.h
// B-194 (HubAndStory §4, Q-194): the hub spot (Press E) that opens the placeholder memory journal:
// memories and lore to reread, V's next line, unlocked conversations. Placed in the hub map.
// B-471c (Q-463 point 3): it opens the journal screen (UJournalScreenWidget) now; V's lines moved to the vendors' talk
// box (UNpcTalkWidget). Conversations are still played in the journal (Q-471c).

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
	virtual FInteractPrompt GetPrompt() const override;

protected:
	virtual FString GetHintText() const override;
};
