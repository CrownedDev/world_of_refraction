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

namespace TrialReturn
{
	/** B-187d: how far apart players stand when they come back to a rest spot (cm). */
	constexpr float RETURN_SPACING_CM = 150.f;

	/** B-187d: where the Index-th returning player stands: the rest stand for the first, then alternately right and
	 *  left of it, RETURN_SPACING_CM further out each pair. */
	WORLD_OF_REFRACTION_API FTransform GetReturnStand(const FTransform &Stand, int32 Index);
}

UCLASS()
class WORLD_OF_REFRACTION_API ATrialGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    /** Picks AWoRPlayerState: the player's wallet (D-046, B-151). */
    ATrialGameMode();

    /** B-187c: after a mini-boss revive the party comes back where it stood to rest
     *  (UTrialRunSubsystem::ConsumePendingReturnStand), else at a player start as usual. B-187d: every player in
     *  this level comes back there, side by side (TrialReturn::GetReturnStand). */
    virtual void RestartPlayer(AController *NewPlayer) override;

    /** B-469: the local leader's pawn stands for the party's first member: its CharacterData is that member's (a new
     *  game's picked character), set before BeginPlay as the battle spawn does. Anyone else's pawn is the class
     *  default's. */
    virtual APawn *SpawnDefaultPawnAtTransform_Implementation(AController *NewPlayer, const FTransform &SpawnTransform) override;

private:
    /** B-187d: this level's return stand, taken from the run by the first player restarted. */
    FTransform ReturnStand;
    bool bHasReturnStand = false;
    int32 PlayersReturned = 0;
};
