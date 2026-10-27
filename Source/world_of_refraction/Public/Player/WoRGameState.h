// WoRGameState.h
// B-227 (Design/CoopStateOwnership.md): the replicated state every player of a level shares. Set as
// GameStateClass by the C++ game modes (a Blueprint game mode must pick it too). Empty for now: B-228
// moves the turn order here, B-232 the run state.
// Debug: GetDebugString, `wor.GameState.Print`.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "WoRGameState.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API AWoRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** The players this level holds (PlayerArray), one per line with its net role. */
	FString GetDebugString() const;
};
