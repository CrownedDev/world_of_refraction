// WoRGameModeBase.h
// B-529: the game mode every field map shares (the hub and the trial maps): the player state with the wallet, the
// co-op game state and seamless travel, and the local leader's pawn standing as the party's first member before its
// character data initialises. ATrialGameMode adds the trial's return stand. A map's Blueprint game mode derives from
// this (editor step: BP_Hub_GameMode's parent).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WoRGameModeBase.generated.h"

class UParty;

UCLASS()
class WORLD_OF_REFRACTION_API AWoRGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    /** Picks AWoRPlayerState (the player's wallet, D-046, B-151) and AWoRGameState, with seamless travel (B-227). */
    AWoRGameModeBase();

    /** B-469, B-529: the local leader's pawn stands for the party's first member: its CharacterData is that member's (a
     *  new game's picked character), set before BeginPlay as the battle spawn does. Anyone else's pawn is the class
     *  default's. */
    virtual APawn *SpawnDefaultPawnAtTransform_Implementation(AController *NewPlayer, const FTransform &SpawnTransform) override;

    /** B-529: Pawn (spawned deferred, before BeginPlay) takes Party's first member's character data and member id.
     *  False when there is no such member or the pawn has no character component. */
    static bool StandAsPartyLeader(APawn *Pawn, const UParty *Party);
};
