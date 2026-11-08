// HubInteract.cpp

#include "Hub/HubInteract.h"
#include "Player/WoRPlayerState.h"
#include "Merchant/MerchantInteractable.h"
#include "Party/PartyInteractionComponent.h"
#include "Trial/TrialDoor.h"
#include "Trial/TrialMapFeature.h"
#include "GameFramework/Pawn.h"

namespace HubInteract
{
    namespace
    {
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
        if (ATrialMapFeature *Feature = ATrialMapFeature::FindNearestInRange(Pawn))
        {
            AddCandidate(Candidates, FString::Printf(TEXT("Map feature %s"), *Feature->GetName()), PawnLocation, Feature,
                         [Feature, Pawn]()
                         {
                             // B-235c: the run lives on the host; a client's press goes there.
                             const UWorld *World = Pawn->GetWorld();
                             AWoRPlayerState *Player = World && World->GetNetMode() == NM_Client ? Pawn->GetPlayerState<AWoRPlayerState>() : nullptr;
                             if (Player)
                             {
                                 Player->ServerInteractFeature(Feature);
                                 return;
                             }
                             Feature->Interact(Pawn);
                         });
        }

        Candidates.Sort([](const FInteractCandidate &A, const FInteractCandidate &B)
                        { return A.DistSquared < B.DistSquared; });
        return Candidates;
    }

    bool InteractWithNearest(APawn *Pawn, const TCHAR *LogTag)
    {
        TArray<FInteractCandidate> Candidates = GatherCandidates(Pawn);
        if (Candidates.Num() == 0)
        {
            UE_LOG(LogTemp, Verbose, TEXT("%s Interact: nothing in range"), LogTag);
            return false;
        }
        UE_LOG(LogTemp, Log, TEXT("%s Interact -> %s"), LogTag, *Candidates[0].Label);
        Candidates[0].Interact();
        return true;
    }

    FString GetInteractionStateString(APawn *Pawn, const TCHAR *LogTag)
    {
        const TArray<FInteractCandidate> Candidates = GatherCandidates(Pawn);
        FString Out = FString::Printf(TEXT("%s pawn=%s candidates=%d"), LogTag,
                                      Pawn ? *Pawn->GetName() : TEXT("<none>"), Candidates.Num());
        for (const FInteractCandidate &Candidate : Candidates)
        {
            Out += FString::Printf(TEXT("\n  %s (%.0fcm)"), *Candidate.Label, FMath::Sqrt(Candidate.DistSquared));
        }
        return Out;
    }
}
