// WoRSaveGame.cpp

#include "Save/WoRSaveGame.h"
#include "Engine/AssetManager.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "WoRLog.h"

FSavedAssetRef FSavedAssetRef::Make(const UObject *Asset)
{
	FSavedAssetRef Ref;
	if (Asset)
	{
		Ref.Id = Asset->GetPrimaryAssetId();
		Ref.Path = FSoftObjectPath(Asset);
	}
	return Ref;
}

UObject *FSavedAssetRef::Resolve() const
{
	// The id first: it still finds an asset that moved after the save.
	if (Id.IsValid() && UAssetManager::IsInitialized())
	{
		const FSoftObjectPath IdPath = UAssetManager::Get().GetPrimaryAssetPath(Id);
		if (UObject *Asset = IdPath.IsValid() ? IdPath.TryLoad() : nullptr)
		{
			return Asset;
		}
	}
	if (UObject *Asset = Path.IsValid() ? Path.TryLoad() : nullptr)
	{
		return Asset;
	}
	UE_LOG(LogWoR, Warning, TEXT("[Save] Can't find saved asset %s (%s)"), *Id.ToString(), *Path.ToString());
	return nullptr;
}

FSavedCarriedGear FSavedCarriedGear::Make(const FCarriedGearState &Carried)
{
	FSavedCarriedGear Saved;
	Saved.bCarried = Carried.bCarried;
	for (const FCarriedAttachment &Attachment : Carried.Attachments)
	{
		FSavedCarriedAttachment &Out = Saved.Attachments.AddDefaulted_GetRef();
		Out.Holder = FSavedAssetRef::Make(Attachment.Holder);
		Out.Attached = Attachment.Attached;
		Out.EvolutionItem = FSavedAssetRef::Make(Attachment.Attached.Evolution.Item);
		Out.Attached.Evolution.Item = nullptr;
	}
	Saved.bHasPrimaryEvolution = Carried.bHasPrimaryEvolution;
	Saved.PrimaryEvolution = Carried.PrimaryEvolution;
	Saved.PrimaryEvolutionItem = FSavedAssetRef::Make(Carried.PrimaryEvolution.Item);
	Saved.PrimaryEvolution.Item = nullptr;
	return Saved;
}

FCarriedGearState FSavedCarriedGear::Restore() const
{
	FCarriedGearState Carried;
	Carried.bCarried = bCarried;
	for (const FSavedCarriedAttachment &In : Attachments)
	{
		UObject *Holder = In.Holder.Resolve();
		if (!Holder)
		{
			UE_LOG(LogWoR, Warning, TEXT("[Save] Carried attachment dropped: its weapon or ring %s is gone"), *In.Holder.Path.ToString());
			continue;
		}
		FCarriedAttachment &Out = Carried.Attachments.AddDefaulted_GetRef();
		Out.Holder = Holder;
		Out.Attached = In.Attached;
		Out.Attached.Evolution.Item = In.EvolutionItem.IsSet() ? In.EvolutionItem.ResolveAs<UEvolutionItemData>() : nullptr;
	}
	Carried.bHasPrimaryEvolution = bHasPrimaryEvolution;
	Carried.PrimaryEvolution = PrimaryEvolution;
	Carried.PrimaryEvolution.Item = PrimaryEvolutionItem.IsSet() ? PrimaryEvolutionItem.ResolveAs<UEvolutionItemData>() : nullptr;
	return Carried;
}

void UWoRSaveGame::HandlePostLoad()
{
	Super::HandlePostLoad();
	if (LoadedDataVersion < WoRSaveVersion::MIN_SUPPORTED)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] Slot '%s' is version %d, below the minimum %d: wiped (D-060)"),
			   *GetSaveSlotName(), LoadedDataVersion, WoRSaveVersion::MIN_SUPPORTED);
		ResetToDefault();
		bWiped = true;
	}
}

void UWoRSaveGame::ResetToDefault()
{
	Super::ResetToDefault();
	Data = FWoRSaveData();
	bWiped = false;
}
