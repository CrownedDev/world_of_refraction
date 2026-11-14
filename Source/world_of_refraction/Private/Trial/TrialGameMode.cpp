// TrialGameMode.cpp

#include "Trial/TrialGameMode.h"

#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialRunSubsystem.h"
#include "Character/CharacterDataComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
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
        FTransform Where = TrialReturn::GetReturnStand(ReturnStand, PlayersReturned++);
        // B-187d review: a side spot behind a wall (or off the floor's edge line) falls back to the stand itself.
        FHitResult Blocked;
        if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Blocked, ReturnStand.GetLocation(), Where.GetLocation(), ECC_Visibility))
        {
            Where = ReturnStand;
        }
        UE_LOG(LogTemp, Log, TEXT("[TrialGameMode] %s back at the rest spot (%s), %d of %d placed"), *NewPlayer->GetName(),
               *Where.GetLocation().ToString(), PlayersReturned, GetNumPlayers());
        RestartPlayerAtTransform(NewPlayer, Where);
        // Once every player who travelled here is placed, later restarts (a joiner, a respawn) use player starts.
        // Merge fix: NumTravellingPlayers is AGameMode-only; seamless travel keeps every travelling controller and its
        // player state, so GetNumPlayers already counts the players still loading in.
        if (PlayersReturned >= GetNumPlayers())
        {
            bHasReturnStand = false;
        }
        return;
    }
    Super::RestartPlayer(NewPlayer);
}

APawn *ATrialGameMode::SpawnDefaultPawnAtTransform_Implementation(AController *NewPlayer, const FTransform &SpawnTransform)
{
    const UPartySessionSubsystem *PartySession = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPartySessionSubsystem>() : nullptr;
    const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
    const APlayerController *Player = Cast<APlayerController>(NewPlayer);
    UClass *PawnClass = GetDefaultPawnClassForController(NewPlayer);
    // The local player is the party's first member (a client's player is the host's remote controller here).
    const bool bLocalLeader = Player && Player->IsLocalController() && Party && Party->Members.Num() > 0 &&
                              (!Party->LeaderPlayer.IsValid() || Party->LeaderPlayer.Get() == Player->GetLocalPlayer());
    UCharacterData *LeaderData = bLocalLeader ? Party->Members[0].CharacterData.Get() : nullptr;
    if (!LeaderData || !PawnClass)
    {
        return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
    }

    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Instigator = GetInstigator();
    SpawnInfo.ObjectFlags |= RF_Transient; // as AGameModeBase's: a player pawn is never saved with the level
    SpawnInfo.bDeferConstruction = true;
    APawn *Pawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TrialGameMode] Could not spawn %s for %s"), *PawnClass->GetName(), *GetNameSafe(NewPlayer));
        return nullptr;
    }
    if (UCharacterDataComponent *CharComp = Pawn->FindComponentByClass<UCharacterDataComponent>())
    {
        if (CharComp->CharacterData != LeaderData)
        {
            UE_LOG(LogTemp, Log, TEXT("[TrialGameMode] %s stands as the party leader %s (class default %s)"), *Pawn->GetName(),
                   *LeaderData->GetName(), *GetNameSafe(CharComp->CharacterData));
            CharComp->CharacterData = LeaderData;
        }
    }
    Pawn->FinishSpawning(SpawnTransform);
    return Pawn;
}
