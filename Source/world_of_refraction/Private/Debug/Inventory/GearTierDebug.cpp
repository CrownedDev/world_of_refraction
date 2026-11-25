// GearTierDebug.cpp

#include "Debug/Inventory/GearTierDebug.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Save/SaveSubsystem.h"
#include "WoRLog.h"

namespace
{
	constexpr float GEAR_TIER_SCREEN_SECONDS = 6.0f;

	/** B-612: the shown name, the asset's name (DA_Weapon_Sword) or, for a weapon, its type word ("Sword"). */
	bool IsNamed(const UWeaponData &Weapon, const FString &NamePart)
	{
		return Weapon.Name.Contains(NamePart, ESearchCase::IgnoreCase) || Weapon.GetName().Contains(NamePart, ESearchCase::IgnoreCase) ||
			   Weapon.GetWeaponTypeName().Contains(NamePart, ESearchCase::IgnoreCase);
	}

	bool IsNamed(const URingData &Ring, const FString &NamePart)
	{
		return Ring.Name.Contains(NamePart, ESearchCase::IgnoreCase) || Ring.GetName().Contains(NamePart, ESearchCase::IgnoreCase);
	}

	/** Sets Entry.Tier when its asset is named NamePart; the GUID of every named piece (B-612: one already at Tier
	 *  still refreshes its loadout copies, which may have drifted), or an invalid one. */
	template <class TEntry, class TAsset>
	FGuid SetTierIfNamed(TEntry &Entry, const TAsset *Asset, const FString &NamePart, EItemTier Tier, TArray<FString> &OutLines)
	{
		if (!Asset || !IsNamed(*Asset, NamePart))
		{
			return FGuid();
		}
		if (Entry.Tier != Tier)
		{
			OutLines.Add(FString::Printf(TEXT("%s: %s -> %s"), *Asset->Name, *ItemIdentity::GetTierLetter(Entry.Tier), *ItemIdentity::GetTierLetter(Tier)));
			Entry.Tier = Tier;
		}
		return Entry.PersistentID;
	}

	/** "Arming Sword (S), Hand Axe (F)": what the command could have matched, for a report that changed nothing. */
	FString GetOwnedGearString(const UInventoryComponent &Inventory)
	{
		TArray<FString> Names;
		for (const FWeaponInventoryEntry &Entry : Inventory.Weapons)
		{
			Names.Add(FString::Printf(TEXT("%s (%s)"), *GetNameSafe(Entry.Weapon), *ItemIdentity::GetTierLetter(Entry.Tier)));
		}
		for (const FRingInventoryEntry &Entry : Inventory.Rings)
		{
			Names.Add(FString::Printf(TEXT("%s (%s)"), *GetNameSafe(Entry.Ring), *ItemIdentity::GetTierLetter(Entry.Tier)));
		}
		return Names.IsEmpty() ? TEXT("nothing") : FString::Join(Names, TEXT(", "));
	}

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorldAndArgs GSetGearTierCmd(
		TEXT("wor.Debug.SetGearTier"),
		TEXT("DEBUG (B-609a): wor.Debug.SetGearTier <name-part> <F|E|D|C|B|A|S> sets every owned weapon or adapter whose name contains "
			 "the text to that tier, free, in every loadout and the party's shared gear, then saves. E.g. wor.Debug.SetGearTier Sword S"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int64 TierValue = Args.Num() > 1 ? StaticEnum<EItemTier>()->GetValueByNameString(Args[1] + TEXT("_Tier")) : INDEX_NONE;
				if (Args.Num() != 2 || TierValue == INDEX_NONE)
				{
					UE_LOG(LogWoR, Warning, TEXT("[GearTierDebug] usage: wor.Debug.SetGearTier <name-part> <F|E|D|C|B|A|S>, e.g. wor.Debug.SetGearTier Sword S"));
					return;
				}
				const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
				const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
				UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
				if (!Inventory)
				{
					UE_LOG(LogWoR, Warning, TEXT("[GearTierDebug] wor.Debug.SetGearTier: no played character with an inventory."));
					return;
				}

				const TArray<FString> Changed = WoRGearTier::SetOwnedGearTier(*Inventory, Args[0], static_cast<EItemTier>(TierValue));
				const UGameInstance *GI = World->GetGameInstance();
				USaveSubsystem *Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
				const bool bSaved = Changed.Num() > 0 && Save && Save->SaveNow(World, /*bAsync=*/false);
				const FString Report = Changed.Num() > 0
										   ? FString::Printf(TEXT("SetGearTier: %s (%s)"), *FString::Join(Changed, TEXT(", ")), bSaved ? TEXT("saved") : TEXT("NOT saved"))
										   : FString::Printf(TEXT("SetGearTier: nothing changed (no owned weapon or adapter named *%s* that isn't %s already; owned: %s)"),
															 *Args[0], *Args[1], *GetOwnedGearString(*Inventory));
				UE_LOG(LogWoR, Display, TEXT("[GearTierDebug] %s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, GEAR_TIER_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));
#endif // !UE_BUILD_SHIPPING
}

TArray<FString> WoRGearTier::SetOwnedGearTier(UInventoryComponent &Inventory, const FString &NamePart, EItemTier Tier)
{
	TArray<FString> Lines;
	if (NamePart.IsEmpty())
	{
		return Lines;
	}
	TArray<FGuid> Changed;
	for (FWeaponInventoryEntry &Entry : Inventory.Weapons)
	{
		Changed.Add(SetTierIfNamed(Entry, Entry.Weapon, NamePart, Tier, Lines));
	}
	for (FRingInventoryEntry &Entry : Inventory.Rings)
	{
		Changed.Add(SetTierIfNamed(Entry, Entry.Ring, NamePart, Tier, Lines));
	}
	// After both loops: RefreshLoadoutsFromOwned reaches the pool, whose change event can rewrite the owned arrays.
	for (const FGuid &ID : Changed)
	{
		if (ID.IsValid())
		{
			Inventory.RefreshLoadoutsFromOwned(ID);
		}
	}
	return Lines;
}
