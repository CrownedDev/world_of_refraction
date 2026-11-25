// WoRGameModeBase.cpp

#include "Player/WoRGameModeBase.h"

#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRFrontEndGameMode.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialGameMode.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

AWoRGameModeBase::AWoRGameModeBase()
{
    PlayerStateClass = AWoRPlayerState::StaticClass();
    // B-227: a listen server's level change keeps its clients (seamless travel; CopyProperties).
    GameStateClass = AWoRGameState::StaticClass();
    bUseSeamlessTravel = true;
}

bool AWoRGameModeBase::StandAsPartyLeader(APawn *Pawn, const UParty *Party)
{
    UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    UCharacterData *LeaderData = Party && Party->Members.Num() > 0 ? Party->Members[0].CharacterData.Get() : nullptr;
    if (!CharComp || !LeaderData)
    {
        return false;
    }
    if (CharComp->CharacterData != LeaderData)
    {
        UE_LOG(LogTemp, Log, TEXT("[WoRGameMode] %s stands as the party leader %s (class default %s)"), *Pawn->GetName(),
               *LeaderData->GetName(), *GetNameSafe(CharComp->CharacterData));
        CharComp->CharacterData = LeaderData;
    }
    // B-503s2: the field pawn is the member's own, as a battle pawn is: its pick, guard and weapon reach the record.
    CharComp->PartyMemberId = Party->Members[0].MemberId;
    return true;
}

APawn *AWoRGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController *NewPlayer, const FTransform &SpawnTransform)
{
    const UPartySessionSubsystem *PartySession = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPartySessionSubsystem>() : nullptr;
    const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
    const APlayerController *Player = Cast<APlayerController>(NewPlayer);
    UClass *PawnClass = GetDefaultPawnClassForController(NewPlayer);
    // The local player is the party's first member (a client's player is the host's remote controller here).
    const bool bLocalLeader = Player && Player->IsLocalController() && Party && Party->Members.Num() > 0 &&
                              Party->Members[0].CharacterData &&
                              (!Party->LeaderPlayer.IsValid() || Party->LeaderPlayer.Get() == Player->GetLocalPlayer());
    // B-537 (D-232): Play pressed straight in the hub or a trial with no save goes to the menu level, not the class
    // default (the Character Template). The pawn still spawns; the travel happens on the next tick.
    const USaveSubsystem *Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    const bool bHasLeader = Party && Party->Members.Num() > 0 && Party->Members[0].CharacterData;
    if (Player && Player->IsLocalController() && GetNetMode() == NM_Standalone && // a host's travel would drop its guests
        WoRFrontEnd::ShouldFallBackToFrontEnd(Save && Save->HasSave(), bHasLeader, WoRFrontEnd::HasFrontEnd(GetGameInstance()), GIsAutomationTesting))
    {
        UE_LOG(LogWoR, Warning, TEXT("[WoRGameMode] No save and no party leader: going to the menu level instead of %s"),
               *GetNameSafe(PawnClass));
        WoRFrontEnd::TravelToFrontEnd(GetWorld());
    }
    if (!bLocalLeader || !PawnClass)
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
        UE_LOG(LogTemp, Warning, TEXT("[WoRGameMode] Could not spawn %s for %s"), *PawnClass->GetName(), *GetNameSafe(NewPlayer));
        return nullptr;
    }
    StandAsPartyLeader(Pawn, Party);
    Pawn->FinishSpawning(SpawnTransform);
    return Pawn;
}

void AWoRGameModeBase::RestartPlayer(AController *NewPlayer)
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
        UE_LOG(LogTemp, Log, TEXT("[WoRGameMode] %s back at the return stand (%s), %d of %d placed"), *NewPlayer->GetName(),
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
