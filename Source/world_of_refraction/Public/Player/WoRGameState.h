// WoRGameState.h
// B-227 (Design/CoopStateOwnership.md): the replicated state every player of a level shares. Set as
// GameStateClass by the C++ game modes (a Blueprint game mode must pick it too). B-228: the turn state
// (UTurnStateComponent); B-232 the run state.
// Debug: GetDebugString, `wor.GameState.Print`.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "WoRGameState.generated.h"

class UTurnStateComponent;

UCLASS()
class WORLD_OF_REFRACTION_API AWoRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AWoRGameState();

	/** B-228: the fight's replicated turn state (current actor, turn order, teams). */
	UTurnStateComponent *GetTurnState() const { return TurnState; }

	/** The players this level holds (PlayerArray), one per line with its net role, then the turn state. */
	FString GetDebugString() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Turn State")
	TObjectPtr<UTurnStateComponent> TurnState;
};
