// TrialDoor.h
// Placeholder trial door (Cluster T-A): a rectangle with a pawn-overlap box
// trigger, mirroring AMerchantInteractable's interact model. Walk up
// (on-screen "Press E" hint) and Interact() — routes to UTrialRunSubsystem:
// EnterTrial(Trial), or ExitTrial when Trial is null.
//
// ⚠️ Null Trial is DESIGNED, not a missing-asset accident: a door with no
// Trial assigned is the "Return to Hub" door placed inside trial levels (the
// hint text says so). Option 1 per Crown — no separate exit-door class.
//
// Range discovery mirrors the merchant cube — the trigger's overlap state, no
// registration, no traces: the controller's E-event calls
// FindNearestInRange(ControlledPawn) and Interact()s the result. The hub
// controller BP must arbitrate between door and merchant results (two
// class-specific statics; nearest-wins or door-first — BP wiring).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrialDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTrialData;

UCLASS()
class WORLD_OF_REFRACTION_API ATrialDoor : public AActor
{
    GENERATED_BODY()

public:
    ATrialDoor();

    /** The trial this door leads to. NULL = this is a "Return to Hub" door
     *  (routes to UTrialRunSubsystem::ExitTrial — see file header). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trial")
    TObjectPtr<UTrialData> Trial;

    /** Routes the pawn in range to UTrialRunSubsystem: EnterTrial(Trial), or
     *  ExitTrial when Trial is null. The hub/trial controller's IA_Interact
     *  event calls this on the FindNearestInRange result. */
    UFUNCTION(BlueprintCallable, Category = "Trial")
    void Interact();

    /** True while a player pawn stands inside the interaction trigger. */
    UFUNCTION(BlueprintPure, Category = "Trial")
    bool IsPawnInRange() const { return PawnInRange.IsValid(); }

    /** The nearest trial door whose trigger overlaps Pawn, or null.
     *  One-node BP wiring for the controller's E-event. */
    UFUNCTION(BlueprintCallable, Category = "Trial")
    static ATrialDoor *FindNearestInRange(APawn *Pawn);

protected:
    virtual void BeginPlay() override;

    /** Placeholder door slab (engine cube, scaled in-level) — also the root. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    /** Pawn-only overlap box; its extent IS the interaction range. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionTrigger;

private:
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
                               UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
                               bool bFromSweep, const FHitResult &SweepResult);

    UFUNCTION()
    void OnTriggerEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
                             UPrimitiveComponent *OtherComp, int32 OtherBodyIndex);

    FString GetHintText() const;

    /** The player pawn currently inside the trigger (single-player — latest
     *  wins). Weak: never keeps a pawn alive. */
    TWeakObjectPtr<APawn> PawnInRange;
};
