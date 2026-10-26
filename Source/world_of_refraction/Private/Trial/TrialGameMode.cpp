// TrialGameMode.cpp

#include "Trial/TrialGameMode.h"

#include "Player/WoRPlayerState.h"

ATrialGameMode::ATrialGameMode()
{
    PlayerStateClass = AWoRPlayerState::StaticClass();
}
