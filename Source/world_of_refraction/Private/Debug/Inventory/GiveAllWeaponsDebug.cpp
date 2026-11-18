// GiveAllWeaponsDebug.cpp

#include "Debug/Inventory/GiveAllWeaponsDebug.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Pool/PoolSubsystem.h"
#include "WoRLog.h"

namespace
{
	constexpr float GIVE_ALL_WEAPONS_SCREEN_SECONDS = 8.0f;
}

int32 WoRGiveAllWeapons::GiveWeapons(UInventoryComponent &Inventory, const TArray<UWeaponData *> &Weapons, int32 Count, TArray<FString> &OutLines)
{
	// The room and the duplicate prompt are for play; here every weapon must arrive, quietly.
	TGuardValue<bool> PastTheRoom(Inventory.bPassingGearThrough, true);
	TGuardValue<bool> Quiet(Inventory.bSuppressDuplicateNotice, true);
	int32 Total = 0;
	for (UWeaponData *Weapon : Weapons)
	{
		int32 Added = 0;
		for (int32 Copy = 0; Weapon && Copy < Count; ++Copy)
		{
			Added += Inventory.AddWeapon(Weapon) ? 1 : 0;
		}
		OutLines.Add(FString::Printf(TEXT("%s: %d of %d added%s"), *GetNameSafe(Weapon), Added, Count, Added < Count ? TEXT(" (REFUSED)") : TEXT("")));
		Total += Added;
	}
	return Total;
}

FString WoRGiveAllWeapons::GiveAllWeapons(UWorld *World, int32 Count)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	const UPoolSubsystem *Pool = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Inventory || !Pool)
	{
		return TEXT("no played character with an inventory and a pool here");
	}
	if (World->GetNetMode() == NM_Client)
	{
		return TEXT("only the host can give weapons");
	}
	if (Pool->IsRunLayerActive())
	{
		return TEXT("a run is open: weapons given now would be run-only; use it in the hub");
	}

	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UWeaponData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.AssetName.LexicalLess(B.AssetName); });
	TArray<UWeaponData *> Weapons;
	int32 NotLoaded = 0;
	for (const FAssetData &Asset : Assets)
	{
		if (UWeaponData *Weapon = Cast<UWeaponData>(Asset.GetAsset()))
		{
			Weapons.Add(Weapon);
		}
		else
		{
			++NotLoaded;
			UE_LOG(LogWoR, Warning, TEXT("[GiveAllWeapons] %s did not load as a weapon - skipped"), *Asset.GetObjectPathString());
		}
	}

	TArray<FString> Lines;
	const int32 Total = GiveWeapons(*Inventory, Weapons, FMath::Clamp(Count, 1, MAX_COUNT), Lines);
	for (const FString &Line : Lines)
	{
		if (Line.EndsWith(TEXT("(REFUSED)")))
		{
			UE_LOG(LogWoR, Warning, TEXT("[GiveAllWeapons] %s"), *Line);
		}
		else
		{
			UE_LOG(LogWoR, Log, TEXT("[GiveAllWeapons] %s"), *Line);
		}
	}
	return FString::Printf(TEXT("%d piece(s) of %d weapon(s) given, %d not loaded; the character now owns %d (over its room of %d: loot and buys are refused until some go)"),
						   Total, Weapons.Num(), NotLoaded, Inventory->Weapons.Num(), InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS);
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GGiveAllWeaponsCmd(
		TEXT("wor.Debug.GiveAllWeapons"),
		TEXT("DEBUG: gives the played character [count] (default 2, at most 5) of every weapon asset as home gear, past its room. Hub only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int32 Count = Args.IsEmpty() ? WoRGiveAllWeapons::DEFAULT_COUNT : FCString::Atoi(*Args[0]);
				const FString Report = WoRGiveAllWeapons::GiveAllWeapons(World, Count);
				UE_LOG(LogWoR, Log, TEXT("[GiveAllWeapons] %s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, GIVE_ALL_WEAPONS_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
}
#endif // !UE_BUILD_SHIPPING
