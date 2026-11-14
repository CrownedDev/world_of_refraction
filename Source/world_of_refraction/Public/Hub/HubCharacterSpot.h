// HubCharacterSpot.h
// B-471d (Q-471c, Q-471d; UIScreens.md #20; Q-463 point 2): a character met in the story stands in the hub; Interact
// opens the talk box with its name (UNpcTalkWidget::OpenForCharacter), where Talk lists the conversations it speaks in
// that are unlocked, NEW until played. Placed in the hub map (an editor step: docs/PIE/B-471d.md), one per character.
// Placeholder look: ATrialMapFeature's cube until the character's mesh is set on the placed actor.
// Debug: wor.Talk.Print lists the box's page once it is open.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "HubCharacterSpot.generated.h"

class UCharacterData;

UCLASS()
class WORLD_OF_REFRACTION_API AHubCharacterSpot : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	/** Who stands here (a story character, D-079 aside: the lords met, Q-463). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	TObjectPtr<UCharacterData> Character;

	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

protected:
	virtual FString GetHintText() const override;
};
