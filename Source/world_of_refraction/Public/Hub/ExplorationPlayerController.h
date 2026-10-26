// ExplorationPlayerController.h
// Native player controller for the out-of-combat levels (hub + trial). Replaces
// the BP_HubPlayerController interact graph:
//   - IA_Interact -> the nearest in-range interactable (trial door, merchant,
//     party recruitable) -> Interact().
//   - OnPossess -> UPartySessionSubsystem::EnsureLocalParty at a defined point
//     (creates the solo Trial Party on first possession, rebinds the leader
//     after every level change).
//
// Cut-over: reparent BP_HubPlayerController (and the trial PC) to this class and
// delete its IA_Interact / EnsureLocalParty nodes — otherwise E fires twice.
// Movement / camera input stays wherever it lives today.
// World of Refraction - Hub

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ExplorationPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class ACombatCharacter;
struct FInputActionValue;

UCLASS()
class WORLD_OF_REFRACTION_API AExplorationPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AExplorationPlayerController();

    /** Interact with the nearest in-range interactable. Also callable from BP / console. */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void InteractWithNearest();

    /** Debug: what E would hit right now, with distances. */
    UFUNCTION(BlueprintCallable, Category = "Interaction|Debug")
    FString GetInteractionStateString() const;

protected:
    virtual void OnPossess(APawn *InPawn) override;
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Defaults to IA_Interact. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InteractAction;

    /** Optional: added at ExplorationMappingPriority in BeginPlay. Leave null if the
     *  pawn/BP already adds the context that maps IA_Interact. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> ExplorationMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    int32 ExplorationMappingPriority = 0;

    /** Leader pawn class for the lazily created Trial Party. Null = the
     *  possessed pawn's class, then PartySessionSubsystem's DefaultSoloPawnClass. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    TSoftClassPtr<ACombatCharacter> LocalPartyPawnClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party")
    bool bEnsureLocalPartyOnPossess = true;

private:
    void HandleInteractPressed(const FInputActionValue &Value);
};
