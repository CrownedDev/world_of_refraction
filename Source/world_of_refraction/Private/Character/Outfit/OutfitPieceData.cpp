// OutfitPieceData.cpp

#include "Character/Outfit/OutfitPieceData.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "Outfit"

const FPrimaryAssetType UOutfitPieceData::OutfitPieceAssetType(TEXT("OutfitPiece"));

FText OutfitSlots::GetSlotName(EOutfitSlot Slot)
{
	switch (Slot)
	{
	case EOutfitSlot::Helmet:   return LOCTEXT("Helmet", "Helmet");
	case EOutfitSlot::Jacket:   return LOCTEXT("Jacket", "Jacket");
	case EOutfitSlot::Shirt:    return LOCTEXT("Shirt", "Shirt");
	case EOutfitSlot::Waist:    return LOCTEXT("Waist", "Waist");
	case EOutfitSlot::Trousers: return LOCTEXT("Trousers", "Trousers");
	case EOutfitSlot::Footwear: return LOCTEXT("Footwear", "Footwear");
	default:                    return FText::GetEmpty();
	}
}

FPrimaryAssetId UOutfitPieceData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(OutfitPieceAssetType, GetFName());
}

FString UOutfitPieceData::GetPieceString() const
{
	FString Out = FString::Printf(TEXT("[Outfit] %s (%s): slot %s, mesh %s, material %s%s"), *GetName(), *PieceName.ToString(),
								  *UEnum::GetValueAsString(Slot), *Mesh.ToString(), *Material.ToString(), bHidesHair ? TEXT(", hides hair") : TEXT(""));
	for (const TPair<FName, FLinearColor> &Key : ColourKeys)
	{
		Out += FString::Printf(TEXT("\n  colour %s = %s"), *Key.Key.ToString(), *Key.Value.ToString());
	}
	return Out;
}

void UOutfitPieceData::PrintPiece() const
{
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetPieceString());
}

TSoftObjectPtr<UOutfitPieceData> FOutfitSet::Get(EOutfitSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Pieces.IsValidIndex(Index) ? Pieces[Index] : TSoftObjectPtr<UOutfitPieceData>();
}

bool FOutfitSet::SetPiece(UOutfitPieceData *Piece)
{
	if (!Piece)
	{
		return false;
	}
	Pieces.SetNum(OutfitSlots::SLOT_COUNT);
	Pieces[static_cast<int32>(Piece->Slot)] = Piece;
	return true;
}

void FOutfitSet::ClearSlot(EOutfitSlot Slot)
{
	Pieces.SetNum(OutfitSlots::SLOT_COUNT);
	Pieces[static_cast<int32>(Slot)].Reset();
}

bool FOutfitSet::IsEmpty() const
{
	return !Pieces.ContainsByPredicate([](const TSoftObjectPtr<UOutfitPieceData> &Piece) { return !Piece.IsNull(); });
}

TArray<FSoftObjectPath> FOutfitSet::ToSaved() const
{
	TArray<FSoftObjectPath> Out;
	Out.SetNum(OutfitSlots::SLOT_COUNT);
	for (int32 Index = 0; Index < OutfitSlots::SLOT_COUNT && Pieces.IsValidIndex(Index); ++Index)
	{
		Out[Index] = Pieces[Index].ToSoftObjectPath();
	}
	return Out;
}

FOutfitSet FOutfitSet::FromSaved(const TArray<FSoftObjectPath> &Saved)
{
	FOutfitSet Out;
	for (int32 Index = 0; Index < OutfitSlots::SLOT_COUNT && Saved.IsValidIndex(Index); ++Index)
	{
		if (Saved[Index].IsNull())
		{
			continue;
		}
		// A piece already in memory (a transient one in a spec) resolves; one on disk loads.
		UObject *Resolved = Saved[Index].ResolveObject();
		const UOutfitPieceData *Piece = Cast<UOutfitPieceData>(Resolved ? Resolved : Saved[Index].TryLoad());
		if (Piece && static_cast<int32>(Piece->Slot) == Index)
		{
			Out.Pieces[Index] = Piece;
		}
		else
		{
			UE_LOG(LogWoR, Warning, TEXT("[Outfit] saved piece %s is unknown or not a %s piece; the slot loads empty"), *Saved[Index].ToString(),
				   *UEnum::GetValueAsString(static_cast<EOutfitSlot>(Index)));
		}
	}
	return Out;
}

FString FOutfitSet::ToString() const
{
	FString Out = TEXT("[Outfit] set:");
	for (int32 Index = 0; Index < OutfitSlots::SLOT_COUNT; ++Index)
	{
		const TSoftObjectPtr<UOutfitPieceData> Piece = Get(static_cast<EOutfitSlot>(Index));
		Out += FString::Printf(TEXT("\n  %-8s %s"), *OutfitSlots::GetSlotName(static_cast<EOutfitSlot>(Index)).ToString(),
							   Piece.IsNull() ? TEXT("(empty)") : *Piece.ToString());
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
