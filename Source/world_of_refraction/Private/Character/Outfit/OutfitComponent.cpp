// OutfitComponent.cpp

#include "Character/Outfit/OutfitComponent.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "WoRLog.h"

namespace
{
	/** The piece's one material slot. */
	constexpr int32 OUTFIT_MATERIAL_INDEX = 0;

	FAutoConsoleCommandWithWorld GPrintOutfitCmd(
		TEXT("wor.Outfit.Print"),
		TEXT("DEBUG: the player's current outfit and what each outfitted pawn wears (B-279c)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
			if (const UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr)
			{
				UE_LOG(LogWoR, Log, TEXT("Current %s"), *Forms->GetCurrentOutfit().ToString());
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (const UOutfitComponent *Outfit = It->FindComponentByClass<UOutfitComponent>())
				{
					UE_LOG(LogWoR, Log, TEXT("%s: %s"), *GetNameSafe(*It), *Outfit->GetOutfitString());
				}
			}
		}));
}

UOutfitComponent::UOutfitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WornPieces.SetNum(OutfitSlots::SLOT_COUNT);
	SlotMeshes.SetNum(OutfitSlots::SLOT_COUNT);
}

int32 UOutfitComponent::ApplyOutfit(const FOutfitSet &Outfit, USkeletalMeshComponent *Body)
{
	WornPieces.SetNum(OutfitSlots::SLOT_COUNT);
	SlotMeshes.SetNum(OutfitSlots::SLOT_COUNT);
	int32 Shown = 0;
	for (int32 Index = 0; Index < OutfitSlots::SLOT_COUNT; ++Index)
	{
		const UOutfitPieceData *Piece = Outfit.Get(static_cast<EOutfitSlot>(Index)).LoadSynchronous();
		WornPieces[Index] = Piece;
		USkeletalMesh *PieceMesh = Piece ? Piece->Mesh.LoadSynchronous() : nullptr;
		USkeletalMeshComponent *&SlotMesh = SlotMeshes[Index];
		if (!PieceMesh || !Body)
		{
			if (SlotMesh)
			{
				SlotMesh->SetSkeletalMesh(nullptr);
				SlotMesh->SetVisibility(false);
			}
			if (Piece && !PieceMesh)
			{
				UE_LOG(LogWoR, Warning, TEXT("[Outfit] %s has no mesh; its slot shows nothing"), *Piece->GetName());
			}
			continue;
		}
		if (!SlotMesh)
		{
			SlotMesh = NewObject<USkeletalMeshComponent>(GetOwner(), NAME_None, RF_Transient);
			SlotMesh->SetupAttachment(Body);
			SlotMesh->RegisterComponent();
		}
		SlotMesh->SetSkeletalMesh(PieceMesh);
		SlotMesh->SetLeaderPoseComponent(Body);
		SlotMesh->SetVisibility(true);
		SlotMesh->EmptyOverrideMaterials();
		if (UMaterialInterface *Material = Piece->Material.LoadSynchronous())
		{
			UMaterialInstanceDynamic *Instance = UMaterialInstanceDynamic::Create(Material, SlotMesh);
			for (const TPair<FName, FLinearColor> &Key : Piece->ColourKeys)
			{
				Instance->SetVectorParameterValue(Key.Key, Key.Value);
			}
			SlotMesh->SetMaterial(OUTFIT_MATERIAL_INDEX, Instance);
		}
		++Shown;
	}
	UE_LOG(LogWoR, Log, TEXT("[Outfit] %s wears %d piece(s)"), *GetNameSafe(GetOwner()), Shown);
	return Shown;
}

const UOutfitPieceData *UOutfitComponent::GetWornPiece(EOutfitSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return WornPieces.IsValidIndex(Index) ? WornPieces[Index].Get() : nullptr;
}

bool UOutfitComponent::HidesHair() const
{
	return WornPieces.ContainsByPredicate([](const TObjectPtr<const UOutfitPieceData> &Piece) { return Piece && Piece->bHidesHair; });
}

FString UOutfitComponent::GetOutfitString() const
{
	FString Out = TEXT("[Outfit] worn:");
	for (int32 Index = 0; Index < OutfitSlots::SLOT_COUNT; ++Index)
	{
		const UOutfitPieceData *Piece = GetWornPiece(static_cast<EOutfitSlot>(Index));
		const bool bShown = SlotMeshes.IsValidIndex(Index) && SlotMeshes[Index] && SlotMeshes[Index]->IsVisible();
		Out += FString::Printf(TEXT("\n  %-8s %s%s"), *OutfitSlots::GetSlotName(static_cast<EOutfitSlot>(Index)).ToString(),
							   Piece ? *Piece->GetName() : TEXT("(empty)"), Piece && !bShown ? TEXT(" (no mesh shown)") : TEXT(""));
	}
	return Out;
}
