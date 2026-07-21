// PartyInteractionComponent.h
// Walk-up-E invite/dismiss for hub NPCs (Party Assembly POC, Arc 1.5a).
//
// Drop on a placed ACombatCharacter Blueprint. The component spawns its own
// sphere trigger at BeginPlay (mirroring the AMerchantInteractable /
// ATrialDoor interact model) and toggles party membership on Interact():
// owner's CharacterData not in the roster → InviteMember; already rostered →
// DismissMember. Slot-0 leader protection lives in UPartySessionSubsystem —
// no special-casing here.
//
// Extends the merchant/door convention two ways:
// - ViewConeThreshold: a facing filter on top of the range trigger, so an E
//   press recruits the NPC you are looking at, not whoever is nearest.
// - OnAvailabilityChanged: delegate parallel to the on-screen debug hint.
//   The hint gives PIE feedback today (Cluster 3); the delegate is the
//   binding point for the party UI (Cluster 4).

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "PartyInteractionComponent.generated.h"

class UCharacterData;
class UPartySessionSubsystem;
class USphereComponent;

/** Fires when a player pawn enters (true) or leaves (false) interaction
 *  range. Presentation hook for the Cluster 4 party UI — the on-screen
 *  debug hint does not depend on it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyInteractionAvailableChanged, bool, bIsAvailable);

UCLASS(ClassGroup = (Party), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UPartyInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartyInteractionComponent();

	/** Radius of the runtime-created interaction trigger sphere. */
	UPROPERTY(EditAnywhere, Category = "Party", meta = (ClampMin = 50.0))
	float InteractionRadius;

	/** Facing filter for FindNearestInRange, as a camera-forward dot product:
	 *  1.0 = must look directly at the NPC, 0.5 ≈ 60° cone, 0.0 = anywhere in
	 *  front (default), -1.0 = ignore facing entirely. */
	UPROPERTY(EditAnywhere, Category = "Party", meta = (ClampMin = -1.0, ClampMax = 1.0))
	float ViewConeThreshold;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FOnPartyInteractionAvailableChanged OnAvailabilityChanged;

	/** Toggle the owner's party membership: not rostered → invite (by owner
	 *  class); rostered → dismiss (by slot). The hub controller's IA_Interact
	 *  event calls this on the FindNearestInRange result. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void Interact();

	/** The nearest party-interactable whose trigger overlaps Pawn AND whose
	 *  owner passes the view-cone filter, or null. One-node BP wiring for the
	 *  controller's E-event, mirroring the merchant/door statics — range is
	 *  the trigger sphere itself, no distance parameter. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	static UPartyInteractionComponent *FindNearestInRange(APawn *Pawn);

	/** True while a player pawn stands inside the interaction trigger. */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsPawnInRange() const { return PawnInRange.IsValid(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Is Target within QueryingPawn's view cone? Compares camera forward
	 *  against the direction to Target; MinDotProduct is the cone edge (see
	 *  ViewConeThreshold). Falls back to pawn facing when no controller. */
	static bool IsWithinViewCone(const APawn *QueryingPawn, const AActor *Target, float MinDotProduct);

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
							   UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
							   bool bFromSweep, const FHitResult &SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
							 UPrimitiveComponent *OtherComp, int32 OtherBodyIndex);

	UCharacterData *GetOwnerCharacterData() const;
	UPartySessionSubsystem *GetPartySession() const;

	/** "Press E to invite/dismiss <name>" — membership verb resolved per call. */
	FString GetHintText() const;

	/** Runtime-created trigger (BeginPlay, NewObject — NOT a default subobject:
	 *  the component may be added to an already-constructed actor). */
	UPROPERTY()
	TObjectPtr<USphereComponent> TriggerSphere = nullptr;

	/** The player pawn currently inside the trigger (single-player hub —
	 *  latest wins). Weak: never keeps a pawn alive. */
	TWeakObjectPtr<APawn> PawnInRange;
};
