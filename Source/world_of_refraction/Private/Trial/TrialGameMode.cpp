// TrialGameMode.cpp

#include "Trial/TrialGameMode.h"

#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

ATrialGameMode::ATrialGameMode()
{
    PlayerStateClass = AWoRPlayerState::StaticClass();
    // B-227: a listen server's level change keeps its clients (seamless travel; CopyProperties).
    GameStateClass = AWoRGameState::StaticClass();
    bUseSeamlessTravel = true;
}

void ATrialGameMode::RestartPlayer(AController *NewPlayer)
{
    UGameInstance *GI = GetGameInstance();
    UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
    FTransform Stand;
    // The host's own player only: a joining friend starts at a player start (co-op places them with B-187d).
    if (Run && NewPlayer && NewPlayer->IsLocalController() && Run->ConsumePendingReturnStand(Stand))
    {
        UE_LOG(LogTemp, Log, TEXT("[TrialGameMode] Back at the rest spot (%s)"), *Stand.GetLocation().ToString());
        RestartPlayerAtTransform(NewPlayer, Stand);
        return;
    }
    Super::RestartPlayer(NewPlayer);
}
