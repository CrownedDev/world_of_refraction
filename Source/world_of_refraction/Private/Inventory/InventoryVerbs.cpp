// InventoryVerbs.cpp

#include "Inventory/InventoryVerbs.h"

#include "Currency/EconomyService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/ItemTier.h"
#include "Pool/PoolSubsystem.h"

#define LOCTEXT_NAMESPACE "InventoryVerbs"

namespace
{
	UPoolSubsystem *InventoryVerbsPoolOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	UEconomyService *InventoryVerbsEconomyOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
	}

	// TODO: gear names are FString assets; FText with B-127 (D-061).
	FText InventoryVerbsGearLabel(const FString &Name, EItemTier Tier)
	{
		return FText::Format(LOCTEXT("GearLabel", "{0} ({1})"), FText::FromString(Name), FText::FromString(TierHelpers::GetTierName(Tier)));
	}

	FText InventoryVerbsStackLabel(const FCrystalId &Id, int32 Count)
	{
		return FText::Format(LOCTEXT("StackLabel", "{0} x{1}"), FText::FromString(ItemIdentity::GetDisplayName(Id)), Count);
	}

	/** True when GearID is the weaker copy of one of the member's duplicates (D-115). */
	bool InventoryVerbsIsWeakerCopy(const UInventoryComponent *Inventory, const FGuid &GearID)
	{
		return Inventory && Inventory->FindDuplicateGear().ContainsByPredicate([&GearID](const FDuplicateGearPair &Pair) { return Pair.WeakerID == GearID; });
	}
}

namespace WoRInventoryVerbs
{
	const TArray<EInventoryVerb> &GetAllVerbs()
	{
		static const TArray<EInventoryVerb> Verbs = {EInventoryVerb::Give, EInventoryVerb::Store, EInventoryVerb::Take,
													 EInventoryVerb::Dismantle, EInventoryVerb::Convert};
		return Verbs;
	}

	FText GetVerbName(EInventoryVerb Verb)
	{
		switch (Verb)
		{
		case EInventoryVerb::Give:
			return LOCTEXT("VerbGive", "Give");
		case EInventoryVerb::Store:
			return LOCTEXT("VerbStore", "Store");
		case EInventoryVerb::Take:
			return LOCTEXT("VerbTake", "Take");
		case EInventoryVerb::Dismantle:
			return LOCTEXT("VerbDismantle", "Dismantle");
		case EInventoryVerb::Convert:
			return LOCTEXT("VerbConvert", "Convert to Gold");
		}
		return FText::GetEmpty();
	}

	TArray<FInventoryRow> GatherRows(const UWorld *World, FName Member)
	{
		TArray<FInventoryRow> Rows;
		const UPoolSubsystem *Pool = InventoryVerbsPoolOf(World);
		if (!Pool || Member.IsNone())
		{
			return Rows;
		}
		for (const bool bInChest : {false, true})
		{
			const FName Side = bInChest ? UPoolSubsystem::GetChestKey() : Member;
			for (const FWeaponInventoryEntry &Weapon : Pool->GetOwnedWeapons())
			{
				if (Weapon.Weapon && Pool->GetGearOwner(Weapon.PersistentID) == Side)
				{
					FInventoryRow &Row = Rows.AddDefaulted_GetRef();
					Row.GearID = Weapon.PersistentID;
					Row.bInChest = bInChest;
					Row.Label = InventoryVerbsGearLabel(Weapon.Weapon->Name, Weapon.Tier);
				}
			}
			for (const FRingInventoryEntry &Adapter : Pool->GetOwnedRings())
			{
				if (Adapter.Ring && Pool->GetGearOwner(Adapter.PersistentID) == Side)
				{
					FInventoryRow &Row = Rows.AddDefaulted_GetRef();
					Row.GearID = Adapter.PersistentID;
					Row.bInChest = bInChest;
					Row.Label = InventoryVerbsGearLabel(Adapter.Ring->Name, Adapter.Tier);
				}
			}
			const auto AddStacks = [&Rows, bInChest](const TMap<FCrystalId, int32> &Stacks)
			{
				for (const TPair<FCrystalId, int32> &Stack : Stacks)
				{
					FInventoryRow &Row = Rows.AddDefaulted_GetRef();
					Row.Item = Stack.Key;
					Row.Count = Stack.Value;
					Row.bInChest = bInChest;
					Row.Label = InventoryVerbsStackLabel(Stack.Key, Stack.Value);
				}
			};
			if (bInChest)
			{
				AddStacks(Pool->GetCrystals());
				AddStacks(Pool->GetStones());
			}
			else
			{
				AddStacks(Pool->GetCarriedItems(Member));
			}
		}
		return Rows;
	}

	TArray<EInventoryVerb> GetVerbsFor(UWorld *World, FName Member, const FInventoryRow &Row)
	{
		if (Row.bInChest)
		{
			return {EInventoryVerb::Take};
		}
		TArray<EInventoryVerb> Verbs = {EInventoryVerb::Give, EInventoryVerb::Store, EInventoryVerb::Dismantle};
		if (Row.IsGear() && InventoryVerbsIsWeakerCopy(WoRGive::FindMemberInventory(World, Member), Row.GearID))
		{
			Verbs.Add(EInventoryVerb::Convert);
		}
		return Verbs;
	}

	FText Run(UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryRow &Row, int32 Count)
	{
		UPoolSubsystem *Pool = InventoryVerbsPoolOf(World);
		if (!Pool || Member.IsNone())
		{
			return LOCTEXT("NoStore", "No character's store to act on");
		}
		if (!GetVerbsFor(World, Member, Row).Contains(Verb))
		{
			return FText::Format(LOCTEXT("VerbNotHere", "{0} doesn't apply to {1}"), GetVerbName(Verb), Row.Label);
		}
		const int32 Moving = FMath::Clamp(Count, 1, FMath::Max(1, Row.Count));
		switch (Verb)
		{
		case EInventoryVerb::Give:
			return Row.IsGear() ? WoRGive::TryGiveGear(World, Member, Receiver, Row.GearID)
								: WoRGive::TryGiveCarried(World, Member, Receiver, Row.Item, Moving);
		case EInventoryVerb::Store:
			if (Row.IsGear())
			{
				return WoRGive::TryPutInChest(World, Member, Row.GearID);
			}
			return Pool->MoveToStorage(Member, Row.Item, Moving) ? FText::GetEmpty() : LOCTEXT("StoreFailed", "Couldn't store that");
		case EInventoryVerb::Take:
			if (Row.IsGear())
			{
				return WoRGive::TryTakeFromChest(World, Member, Row.GearID);
			}
			return Pool->MoveToCarried(Member, Row.Item, Moving) ? FText::GetEmpty() : LOCTEXT("PouchFull", "No room in the pouch");
		case EInventoryVerb::Dismantle:
		case EInventoryVerb::Convert:
			break;
		}

		// Dismantle and Convert go through the economy, which acts on the member's spawned actor.
		UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
		UEconomyService *Economy = InventoryVerbsEconomyOf(World);
		AActor *Owner = Inventory ? Inventory->GetOwner() : nullptr;
		if (!Owner || !Economy)
		{
			return LOCTEXT("NotHere", "That character isn't here to do it");
		}
		bool bDone = false;
		if (Verb == EInventoryVerb::Convert)
		{
			bDone = Economy->ResolveDuplicate(Owner, Row.GearID, /*bConvertToGold=*/true);
		}
		else if (!Row.IsGear())
		{
			bDone = Economy->DismantleCrystal(Owner, Row.Item, Moving);
		}
		else
		{
			bDone = Pool->FindRingInPool(Row.GearID) ? Economy->DismantleRing(Owner, Row.GearID) : Economy->DismantleWeapon(Owner, Row.GearID);
		}
		return bDone ? FText::GetEmpty() : FText::Format(LOCTEXT("VerbFailed", "Couldn't {0} {1}"), GetVerbName(Verb), Row.Label);
	}

	FString GetRowsString(UWorld *World, FName Member)
	{
		FString Out = FString::Printf(TEXT("[Inventory] %s's store and the chest:"), *Member.ToString());
		for (const FInventoryRow &Row : GatherRows(World, Member))
		{
			TArray<FString> Names;
			for (const EInventoryVerb Verb : GetVerbsFor(World, Member, Row))
			{
				Names.Add(GetVerbName(Verb).ToString());
			}
			Out += FString::Printf(TEXT("\n  %s%s: %s"), Row.bInChest ? TEXT("chest ") : TEXT(""), *Row.Label.ToString(),
								   *FString::Join(Names, TEXT(", ")));
		}
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
