// GiveTestWeaponsDebug.cpp

#include "Debug/Inventory/GiveTestWeaponsDebug.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Pool/PoolSubsystem.h"
#include "WoRLog.h"

namespace
{
	constexpr float GIVE_TEST_WEAPONS_SCREEN_SECONDS = 8.0f;
}

int32 WoRGiveTestWeapons::GiveWeapons(UInventoryComponent &Inventory, const TArray<UWeaponData *> &Weapons, int32 Count, TArray<FString> &OutLines)
{
	// The duplicate prompt is for play; a test set of copies arrives quietly.
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

FString WoRGiveTestWeapons::GiveTestWeapons(UWorld *World)
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

	TArray<UWeaponData *> Weapons;
	for (const TCHAR *Path : TEST_WEAPON_PATHS)
	{
		if (UWeaponData *Weapon = LoadObject<UWeaponData>(nullptr, Path))
		{
			Weapons.Add(Weapon);
		}
		else
		{
			UE_LOG(LogWoR, Warning, TEXT("[GiveTestWeapons] %s is not a weapon asset - skipped"), Path);
		}
	}

	TArray<FString> Lines;
	const int32 Total = GiveWeapons(*Inventory, Weapons, COUNT_EACH, Lines);
	for (const FString &Line : Lines)
	{
		if (Line.EndsWith(TEXT("(REFUSED)")))
		{
			UE_LOG(LogWoR, Warning, TEXT("[GiveTestWeapons] %s (no room)"), *Line);
		}
		else
		{
			UE_LOG(LogWoR, Log, TEXT("[GiveTestWeapons] %s"), *Line);
		}
	}
	const int32 Asked = UE_ARRAY_COUNT(TEST_WEAPON_PATHS) * COUNT_EACH;
	return FString::Printf(TEXT("%d of %d test weapon(s) given%s"), Total, Asked, Total < Asked ? TEXT(" - see the [GiveTestWeapons] warnings in the log") : TEXT(""));
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorld GGiveTestWeaponsCmd(
		TEXT("wor.Debug.GiveTestWeapons"),
		TEXT("DEBUG: gives the played character 2 each of the test weapons (sword, shield, hand axe, bearded axe) as home gear. Hub only."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const FString Report = WoRGiveTestWeapons::GiveTestWeapons(World);
				UE_LOG(LogWoR, Log, TEXT("[GiveTestWeapons] %s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, GIVE_TEST_WEAPONS_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
}
#endif // !UE_BUILD_SHIPPING
