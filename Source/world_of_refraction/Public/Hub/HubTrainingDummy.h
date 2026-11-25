// HubTrainingDummy.h
// B-611: the hub's training dummy (Press E). Starts a practice fight against one dummy that never dies, never attacks
// and passes its turn at once. Nothing is won, spent, worn or saved; the battle menu's Flee row ends it and returns the
// party to where the leader stood. Placed in the hub map; `wor.Hub.TrainingDummy` starts the same fight from anywhere.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "HubTrainingDummy.generated.h"

namespace WoRPractice
{
/** Marks a fighter for a practice fight: it can't fall (HP refills at 0), and a dummy also passes every turn. */
WORLD_OF_REFRACTION_API void MarkFighter(AActor *Fighter, bool bDummy);

/** "Practice fight: yes/no" plus each marked fighter in the world, for the debug command. */
WORLD_OF_REFRACTION_API FString GetPracticeFightString(const UObject *WorldContext);
}

UCLASS()
class WORLD_OF_REFRACTION_API AHubTrainingDummy : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

protected:
	virtual FString GetHintText() const override;
};
