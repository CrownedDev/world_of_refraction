// OutfitComponent.cpp

#include "Character/Outfit/OutfitComponent.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
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

	UCharacterFormSubsystem *OutfitForms(UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	}

	/** Placeholder (Backlog rule 4) until the CommonUI wardrobe: wear a piece by asset path. */
	FAutoConsoleCommandWithWorldAndArgs GWearOutfitCmd(
		TEXT("wor.Outfit.Wear"),
		TEXT("PLACEHOLDER: wor.Outfit.Wear <outfit piece asset path> - wear it in its slot (hub or rest spot only, B-279c)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			UCharacterFormSubsystem *Forms = OutfitForms(World);
			UOutfitPieceData *Piece = Args.Num() > 0 ? LoadObject<UOutfitPieceData>(nullptr, *Args[0]) : nullptr;
			if (!Forms || !Piece)
			{
				UE_LOG(LogWoR, Warning, TEXT("[Outfit] usage: wor.Outfit.Wear <outfit piece asset path> (piece not found)"));
				return;
			}
			// Outside the log call: UE_LOG compiles out with NO_LOGGING.
			const bool bWorn = Forms->TryWearOutfitPiece(Piece);
			UE_LOG(LogWoR, Log, TEXT("[Outfit] wear %s: %s"), *Piece->GetName(), bWorn ? TEXT("worn") : TEXT("refused (in a fight, or away from the hub / a rest spot)"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GClearOutfitCmd(
		TEXT("wor.Outfit.Clear"),
		TEXT("PLACEHOLDER: wor.Outfit.Clear <slot: Helmet|Jacket|Shirt|Waist|Trousers|Footwear> - empty that slot (B-279c)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			UCharacterFormSubsystem *Forms = OutfitForms(World);
			const int64 SlotValue = Args.Num() > 0 ? StaticEnum<EOutfitSlot>()->GetValueByNameString(Args[0]) : INDEX_NONE;
			if (!Forms || SlotValue == INDEX_NONE)
			{
				UE_LOG(LogWoR, Warning, TEXT("[Outfit] usage: wor.Outfit.Clear <Helmet|Jacket|Shirt|Waist|Trousers|Footwear>"));
				return;
			}
			const bool bCleared = Forms->TryClearOutfitSlot(static_cast<EOutfitSlot>(SlotValue));
			UE_LOG(LogWoR, Log, TEXT("[Outfit] clear %s: %s"), *Args[0], bCleared ? TEXT("cleared") : TEXT("refused"));
		}));

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
		TObjectPtr<USkeletalMeshComponent> &SlotMesh = SlotMeshes[Index];
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
		else if (SlotMesh->GetAttachParent() != Body)
		{
			SlotMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		// Leader Pose needs the piece skinned to the body's skeleton; warn so a wrong mesh is found in PIE.
		const USkeletalMesh *BodyMesh = Body->GetSkeletalMeshAsset();
		if (BodyMesh && PieceMesh->GetSkeleton() != BodyMesh->GetSkeleton())
		{
			UE_LOG(LogWoR, Warning, TEXT("[Outfit] %s's mesh uses another skeleton than the body (%s); it may not follow the body"), *Piece->GetName(),
				   *GetNameSafe(BodyMesh));
		}
		// TODO: load the pieces asynchronously if outfit changes ever hitch (six synchronous loads today).
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
