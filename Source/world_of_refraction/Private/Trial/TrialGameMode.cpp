// TrialGameMode.cpp

#include "Trial/TrialGameMode.h"

#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"

ATrialGameMode::ATrialGameMode()
{
    PlayerStateClass = AWoRPlayerState::StaticClass();
    // B-227: a listen server's level change keeps its clients (seamless travel; CopyProperties).
    GameStateClass = AWoRGameState::StaticClass();
    bUseSeamlessTravel = true;
}
