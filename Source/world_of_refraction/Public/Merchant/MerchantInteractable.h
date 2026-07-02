// MerchantInteractable.h
// Placeholder hub merchant: a cube with a pawn-overlap trigger. Walk up
// (on-screen "Press E" hint) and Interact() — v1 prints the merchant's stock
// via UMerchantData::GetMerchantString(); Cluster 3 swaps the print for the
// shop UI. 3D models / shop-door interaction come later per the locked design.
//
// Range discovery is the trigger's overlap state — no registration, no traces:
// the controller's E-event calls FindNearestInRange(ControlledPawn) and
// Interact()s the result.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MerchantInteractable.generated.h"

class UMerchantData;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class WORLD_OF_REFRACTION_API AMerchantInteractable : public AActor
{
    GENERATED_BODY()

public:
    AMerchantInteractable();

    /** The merchant this cube fronts. Assigned per level instance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant")
    TObjectPtr<UMerchantData> Merchant;

    /** v1: prints the merchant's stock to screen + Output Log. The hub
     *  controller's IA_Interact event calls this on the FindNearestInRange
     *  result. Cluster 3 replaces the print with opening the shop UI. */
    UFUNCTION(BlueprintCallable, Category = "Merchant")
    void Interact();

    /** True while a player pawn stands inside the interaction trigger. */
    UFUNCTION(BlueprintPure, Category = "Merchant")
    bool IsPawnInRange() const { return PawnInRange.IsValid(); }

    /** The nearest merchant cube whose trigger overlaps Pawn, or null.
     *  One-node BP wiring for the controller's E-event. */
    UFUNCTION(BlueprintCallable, Category = "Merchant")
    static AMerchantInteractable *FindNearestInRange(APawn *Pawn);

protected:
    virtual void BeginPlay() override;

    /** Placeholder cube visual (engine basic shape) — also the root. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> CubeMesh;

    /** Pawn-only overlap sphere; its radius IS the interaction range. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionTrigger;

private:
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
                               UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
                               bool bFromSweep, const FHitResult &SweepResult);

    UFUNCTION()
    void OnTriggerEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
                             UPrimitiveComponent *OtherComp, int32 OtherBodyIndex);

    FString GetHintText() const;

    /** The player pawn currently inside the trigger (single-player hub —
     *  latest wins). Weak: never keeps a pawn alive. */
    TWeakObjectPtr<APawn> PawnInRange;
};
