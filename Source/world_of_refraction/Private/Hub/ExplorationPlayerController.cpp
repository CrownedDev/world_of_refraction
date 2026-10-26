// ExplorationPlayerController.cpp

#include "Hub/ExplorationPlayerController.h"
#include "Character/CombatCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Merchant/MerchantInteractable.h"
#include "Party/PartyInteractionComponent.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/TrialDoor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "InputActionValue.h"

namespace ExplorationInput
{
    constexpr const TCHAR *InteractActionPath =
        TEXT("/Game/Data/Characters/Models/CharacterBase/Input/OpenWorld/IA_Interact.IA_Interact");
}

namespace ExplorationInteract
{
    /** One candidate per interactable kind; the nearest wins. */
    struct FInteractCandidate
    {
        FString Label;
        float DistSquared = TNumericLimits<float>::Max();
        TFunction<void()> Interact;
    };

    void AddCandidate(TArray<FInteractCandidate> &Candidates, const FString &Label, const FVector &From,
                      const AActor *Target, TFunction<void()> Interact)
    {
        FInteractCandidate Candidate;
        Candidate.Label = Label;
        Candidate.DistSquared = Target ? FVector::DistSquared(From, Target->GetActorLocation())
                                       : TNumericLimits<float>::Max();
        Candidate.Interact = MoveTemp(Interact);
        Candidates.Add(MoveTemp(Candidate));
    }

    TArray<FInteractCandidate> GatherCandidates(APawn *Pawn)
    {
        TArray<FInteractCandidate> Candidates;
        if (!Pawn)
        {
            return Candidates;
        }
        const FVector PawnLocation = Pawn->GetActorLocation();

        if (ATrialDoor *Door = ATrialDoor::FindNearestInRange(Pawn))
        {
            AddCandidate(Candidates, FString::Printf(TEXT("Door %s"), *Door->GetName()), PawnLocation, Door,
                         [Door]() { Door->Interact(); });
        }
        if (AMerchantInteractable *Merchant = AMerchantInteractable::FindNearestInRange(Pawn))
        {
            AddCandidate(Candidates, FString::Printf(TEXT("Merchant %s"), *Merchant->GetName()), PawnLocation, Merchant,
                         [Merchant]() { Merchant->Interact(); });
        }
        if (UPartyInteractionComponent *Recruit = UPartyInteractionComponent::FindNearestInRange(Pawn))
        {
            const AActor *Owner = Recruit->GetOwner();
            AddCandidate(Candidates, FString::Printf(TEXT("Recruit %s"), Owner ? *Owner->GetName() : TEXT("?")),
                         PawnLocation, Owner, [Recruit]() { Recruit->Interact(); });
        }

        Candidates.Sort([](const FInteractCandidate &A, const FInteractCandidate &B)
                        { return A.DistSquared < B.DistSquared; });
        return Candidates;
    }
}

AExplorationPlayerController::AExplorationPlayerController()
{
    static ConstructorHelpers::FObjectFinder<UInputAction> InteractFinder(ExplorationInput::InteractActionPath);
    if (InteractFinder.Succeeded())
    {
        InteractAction = InteractFinder.Object;
    }
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
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPC] InteractAction not set (default %s missing?)"),
               ExplorationInput::InteractActionPath);
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
    TArray<ExplorationInteract::FInteractCandidate> Candidates = ExplorationInteract::GatherCandidates(GetPawn());
    if (Candidates.Num() == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPC] Interact: nothing in range"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPC] Interact -> %s"), *Candidates[0].Label);
    Candidates[0].Interact();
}

FString AExplorationPlayerController::GetInteractionStateString() const
{
    const TArray<ExplorationInteract::FInteractCandidate> Candidates = ExplorationInteract::GatherCandidates(GetPawn());
    FString Out = FString::Printf(TEXT("[ExplorationPC] pawn=%s candidates=%d"),
                                  GetPawn() ? *GetPawn()->GetName() : TEXT("<none>"), Candidates.Num());
    for (const ExplorationInteract::FInteractCandidate &Candidate : Candidates)
    {
        Out += FString::Printf(TEXT("\n  %s (%.0fcm)"), *Candidate.Label, FMath::Sqrt(Candidate.DistSquared));
    }
    return Out;
}
