// ExplorationPlayerController.cpp
// OBSOLETE (B-158, D-076): AWoRHubController is the one hub controller and owns interact and
// EnsureLocalParty. This class stays until Crown's PIE pass (CLAUDE.md: remove only after the new
// path is proven); its interaction runs through the shared HubInteract helper.

#include "Hub/ExplorationPlayerController.h"
#include "Character/CombatCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Hub/HubInteract.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Party/PartySessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "WoRLog.h"

namespace
{
    const TCHAR *const EXPLORATION_PC_LOG_TAG = TEXT("[ExplorationPC]");
}

void AExplorationPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!ExplorationMappingContext)
    {
        return;
    }
    if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(ExplorationMappingContext, ExplorationMappingPriority);
    }
}

void AExplorationPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ExplorationMappingContext)
    {
        if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            Subsystem->RemoveMappingContext(ExplorationMappingContext);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void AExplorationPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent *EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPC] InputComponent is not Enhanced Input - interact unbound"));
        return;
    }
    if (!InteractAction)
    {
        InteractAction = DefaultInteractAction.LoadSynchronous();
    }
    if (!InteractAction)
    {
        UE_LOG(LogWoR, Error,
               TEXT("[ExplorationPC] No interact action: set DefaultInteractAction in Config/DefaultGame.ini "
                    "(now '%s') or InteractAction on the controller BP (B-205)"),
               *DefaultInteractAction.ToString());
        return;
    }
    EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this,
                              &AExplorationPlayerController::HandleInteractPressed);
}

void AExplorationPlayerController::OnPossess(APawn *InPawn)
{
    Super::OnPossess(InPawn);

    if (!bEnsureLocalPartyOnPossess)
    {
        return;
    }
    UGameInstance *GI = GetGameInstance();
    if (UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr)
    {
        PartySession->EnsureLocalParty(this, LocalPartyPawnClass);
    }
}

void AExplorationPlayerController::HandleInteractPressed(const FInputActionValue & /*Value*/)
{
    InteractWithNearest();
}

void AExplorationPlayerController::InteractWithNearest()
{
    HubInteract::InteractWithNearest(GetPawn(), EXPLORATION_PC_LOG_TAG);
}

FString AExplorationPlayerController::GetInteractionStateString() const
{
    return HubInteract::GetInteractionStateString(GetPawn(), EXPLORATION_PC_LOG_TAG);
}
