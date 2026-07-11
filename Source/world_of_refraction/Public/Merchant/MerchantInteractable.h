// MerchantInteractable.h
// Placeholder hub merchant: a cube with a pawn-overlap trigger. Walk up
// (on-screen "Press E" hint) and Interact() — opens the shop window via
// UMerchantShopSubsystem::OpenForMerchant (Cluster 3a; replaced the v1
// stock print). 3D models / shop-door interaction come later per the
// locked design.
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

    /** Opens the merchant's shop window via UMerchantShopSubsystem::OpenForMerchant
     *  (PawnInRange is the instigator; no-Merchant / no-pawn cases log and bail).
     *  The hub controller's IA_Interact event calls this on the
     *  FindNearestInRange result. */
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
