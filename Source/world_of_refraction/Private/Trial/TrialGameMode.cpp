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

FTransform TrialReturn::GetReturnStand(const FTransform &Stand, int32 Index)
{
    if (Index <= 0)
    {
        return Stand;
    }
    const int32 Pair = (Index + 1) / 2;
    const float Side = (Index % 2 == 1) ? 1.f : -1.f;
    FTransform Out = Stand;
    Out.AddToTranslation(Stand.GetRotation().GetRightVector() * (Side * Pair * RETURN_SPACING_CM));
    return Out;
}

void ATrialGameMode::RestartPlayer(AController *NewPlayer)
{
    UGameInstance *GI = GetGameInstance();
    UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
    FTransform Stand;
    // B-187d: the first player restarted in this level takes the stand; every player after it (a friend too) comes
    // back beside it.
    if (Run && Run->ConsumePendingReturnStand(Stand))
    {
        ReturnStand = Stand;
        bHasReturnStand = true;
        PlayersReturned = 0;
    }
    if (bHasReturnStand && NewPlayer)
    {
        const FTransform Where = TrialReturn::GetReturnStand(ReturnStand, PlayersReturned++);
        UE_LOG(LogTemp, Log, TEXT("[TrialGameMode] %s back at the rest spot (%s)"), *NewPlayer->GetName(), *Where.GetLocation().ToString());
        RestartPlayerAtTransform(NewPlayer, Where);
        return;
    }
    Super::RestartPlayer(NewPlayer);
}
