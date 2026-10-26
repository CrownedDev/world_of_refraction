// TrialGameMode.h
// GameMode anchor for trial levels (Cluster T-C1, revised shape): an empty
// AGameModeBase subclass. BP_TrialGameMode carries the designer config
// (DefaultPawnClass, PlayerControllerClass) in the trial level's world
// settings. Combat furniture is NOT spawned here — the encounter component
// spawns an orchestrator per encounter (see UEncounterComponent).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TrialGameMode.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API ATrialGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    /** Picks AWoRPlayerState: the player's wallet (D-046, B-151). */
    ATrialGameMode();
};
