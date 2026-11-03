// GearGive.cpp

#include "Inventory/GearGive.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Pool/PoolSubsystem.h"

#define LOCTEXT_NAMESPACE "GearGive"

namespace
{
	UPoolSubsystem *PoolOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	FText NameOf(FName CharacterKey)
	{
		// TODO: the character's display name (the key is the asset path; the pawn may not be spawned).
		return FText::FromString(FPaths::GetBaseFilename(CharacterKey.ToString()));
	}
}

namespace WoRGive
{
	UInventoryComponent *FindMemberInventory(UWorld *World, FName Key)
	{
		if (!World || Key.IsNone())
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UCharacterDataComponent *CharComp = It->FindComponentByClass<UCharacterDataComponent>();
			if (CharComp && UPoolSubsystem::GetCharacterKey(CharComp->CharacterData) == Key)
			{
				if (UInventoryComponent *Inventory = It->FindComponentByClass<UInventoryComponent>())
				{
					return Inventory;
				}
			}
		}
		return nullptr;
	}

	FText TryGiveGear(UWorld *World, FName FromKey, FName ToKey, const FGuid &GearID)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoPool", "No armoury to give from");
		}
		if (FromKey.IsNone() || ToKey.IsNone() || FromKey == ToKey)
		{
			return LOCTEXT("SameCharacter", "Choose another character to give to");
		}
		if (Pool->FindEvolutionInPool(GearID))
		{
			return LOCTEXT("NoEvolutions", "Evolutions can't be given yet");
		}
		if (!Pool->FindWeaponInPool(GearID) && !Pool->FindRingInPool(GearID))
		{
			return LOCTEXT("NotGear", "That isn't a weapon or adapter");
		}
		if (Pool->GetGearOwner(GearID) != FromKey)
		{
			return FText::Format(LOCTEXT("NotTheirs", "{0} doesn't own that"), NameOf(FromKey));
		}
		// D-116: a loadout's primary stays until another primary is swapped in. Checked on the spawned giver; an
		// unspawned giver's next draw refills its primary instead.
		if (const UInventoryComponent *Giver = FindMemberInventory(World, FromKey))
		{
			if (Giver->IsPrimaryInAnyLoadout(GearID))
			{
				return LOCTEXT("GivePrimary", "That is a loadout's primary. Swap in another primary before giving it");
			}
		}
		if (!Pool->GiveGear(GearID, ToKey))
		{
			return LOCTEXT("GiveFailed", "Couldn't give that");
		}
		return FText::GetEmpty();
	}

	FText TryGiveCarried(UWorld *World, FName FromKey, FName ToKey, const FCrystalId &Id, int32 Count)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoPouches", "No pouches to give from");
		}
		if (FromKey.IsNone() || ToKey.IsNone() || FromKey == ToKey)
		{
			return LOCTEXT("SameCharacterItems", "Choose another character to give to");
		}
		if (Count <= 0 || Pool->GetCarriedCount(FromKey, Id) < Count)
		{
			return FText::Format(LOCTEXT("NotCarried", "{0} doesn't carry that many"), NameOf(FromKey));
		}
		if (Pool->GetCarryRoom(ToKey, Id) < Count)
		{
			return FText::Format(LOCTEXT("PouchFull", "{0}'s pouch has no room for {1}"), NameOf(ToKey),
								 FText::FromString(ItemIdentity::GetDisplayName(Id)));
		}
		if (!Pool->GiveCarried(FromKey, ToKey, Id, Count))
		{
			return LOCTEXT("GiveItemsFailed", "Couldn't give those");
		}
		return FText::GetEmpty();
	}

	FString GetGiveableString(const UWorld *World, FName FromKey)
	{
		const UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return TEXT("[Give] no pool");
		}
		FString Out = FString::Printf(TEXT("[Give] %s can give:"), *FPaths::GetBaseFilename(FromKey.ToString()));
		for (const FWeaponInventoryEntry &Weapon : Pool->GetOwnedWeapons())
		{
			if (Pool->GetGearOwner(Weapon.PersistentID) == FromKey)
			{
				Out += FString::Printf(TEXT("\n  weapon %s (%s)"), Weapon.Weapon ? *Weapon.Weapon->Name : TEXT("?"), *Weapon.PersistentID.ToString());
			}
		}
		for (const FRingInventoryEntry &Ring : Pool->GetOwnedRings())
		{
			if (Pool->GetGearOwner(Ring.PersistentID) == FromKey)
			{
				Out += FString::Printf(TEXT("\n  adapter %s (%s)"), Ring.Ring ? *Ring.Ring->Name : TEXT("?"), *Ring.PersistentID.ToString());
			}
		}
		for (const TPair<FCrystalId, int32> &Stack : Pool->GetCarriedItems(FromKey))
		{
			Out += FString::Printf(TEXT("\n  pouch %s x%d"), *ItemIdentity::GetDisplayName(Stack.Key), Stack.Value);
		}
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
