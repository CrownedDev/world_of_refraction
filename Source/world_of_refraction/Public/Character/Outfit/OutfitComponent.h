// OutfitComponent.h
// B-279c (Design/CharacterCustomisation.md §4): wears an FOutfitSet on a pawn. One skeletal mesh component per slot,
// attached to the body mesh and following it by Leader Pose (no mesh merging unless profiling asks for it). Each
// piece's material gets its colour keys through a dynamic instance. Event-driven: ApplyOutfit runs when the outfit or
// the body changes, never per tick. Debug: GetOutfitString, wor.Outfit.Print.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Outfit/OutfitPieceData.h"
#include "OutfitComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UOutfitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOutfitComponent();

	/** Wears Outfit on Body: each filled slot gets its piece's mesh (following Body by Leader Pose), each empty slot
	 *  is cleared. Returns how many slots show a mesh (a piece without a mesh is remembered but shows nothing). */
	int32 ApplyOutfit(const FOutfitSet &Outfit, USkeletalMeshComponent *Body);

	/** The piece worn in Slot (null for an empty slot). */
	const UOutfitPieceData *GetWornPiece(EOutfitSlot Slot) const;

	/** True when a worn piece hides the hair. */
	bool HidesHair() const;

	/** Debug: each slot's piece and whether it shows a mesh. */
	FString GetOutfitString() const;

private:
	/** Indexed by EOutfitSlot. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UOutfitPieceData>> WornPieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> SlotMeshes;
};
