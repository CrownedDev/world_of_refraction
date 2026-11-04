// OutfitPieceData.h
// B-279c (Design/CharacterCustomisation.md): outfits. Every form wears six outfit slots, switchable piece by piece
// (Monster Hunter's model). A piece is a component: its own skeletal mesh, skinned to the shared body and assembled by
// Leader Pose (UOutfitComponent), with a material and colour keys so one mesh serves every colour. Cosmetic only.
// A form's authored outfit and the player's current outfit are an FOutfitSet: one piece per slot.
// Debug: FOutfitSet::ToString, UOutfitPieceData::PrintPiece, wor.Outfit.Print.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "OutfitPieceData.generated.h"

class USkeletalMesh;
class UMaterialInterface;

/** The six outfit slots (CharacterCustomisation.md §1). Append only: saved by position in FOutfitSet. */
UENUM(BlueprintType)
enum class EOutfitSlot : uint8
{
	Helmet,
	Jacket,
	Shirt,
	Waist,
	Trousers,
	Footwear
};

namespace OutfitSlots
{
	constexpr int32 SLOT_COUNT = 6;

	/** The slot's player-facing name. TODO: Crown's slot names (CharacterCustomisation.md §5); placeholders now. */
	WORLD_OF_REFRACTION_API FText GetSlotName(EOutfitSlot Slot);
}

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UOutfitPieceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType OutfitPieceAssetType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit")
	FText PieceName;

	/** The slot the piece fills; it only ever replaces that slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit")
	EOutfitSlot Slot = EOutfitSlot::Jacket;

	/** Skinned to the shared V-template body / Manny; follows the body by Leader Pose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Look")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Look")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Vector parameters set on the material (the gem colour, trims), so one mesh serves every colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Look")
	TMap<FName, FLinearColor> ColourKeys;

	/** The piece hides the hair (a hood, a full helmet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Look")
	bool bHidesHair = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Debug: name, slot, mesh, material and colour keys. */
	FString GetPieceString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintPiece() const;
};

/** One piece per slot (a form's authored outfit, the player's current one). An empty slot wears nothing there. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FOutfitSet
{
	GENERATED_BODY()

	/** Indexed by EOutfitSlot. Kept at SLOT_COUNT entries by the accessors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit", EditFixedSize)
	TArray<TSoftObjectPtr<UOutfitPieceData>> Pieces;

	FOutfitSet() { Pieces.SetNum(OutfitSlots::SLOT_COUNT); }

	TSoftObjectPtr<UOutfitPieceData> Get(EOutfitSlot Slot) const;

	/** Puts Piece in its own slot, replacing only that slot. False (nothing changes) for a null piece. */
	bool SetPiece(UOutfitPieceData *Piece);

	/** Empties one slot. */
	void ClearSlot(EOutfitSlot Slot);

	bool IsEmpty() const;

	/** D-059: the set as six soft paths for the save, and back (a path to an unknown or wrong-slot piece loads empty). */
	TArray<FSoftObjectPath> ToSaved() const;
	static FOutfitSet FromSaved(const TArray<FSoftObjectPath> &Saved);

	bool operator==(const FOutfitSet &Other) const { return Pieces == Other.Pieces; }

	/** Debug: one line per slot. */
	FString ToString() const;
};
