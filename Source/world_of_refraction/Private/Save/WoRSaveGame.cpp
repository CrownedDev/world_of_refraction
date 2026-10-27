// WoRSaveGame.cpp

#include "Save/WoRSaveGame.h"
#include "Engine/AssetManager.h"
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
