// InventoryVerbs.cpp

#include "Inventory/InventoryVerbs.h"

#include "Character/CharacterData.h"
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
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "WoRGameNames.h"

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

	/** Q-321: a run-only member owns nothing, so it isn't given gear. */
	bool InventoryVerbsIsRunOnly(const UWorld *World, FName Key)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		const FPartyMember *Member = Party ? Party->Members.FindByPredicate([Key](const FPartyMember &M)
															  { return M.CharacterData && UPoolSubsystem::GetCharacterKey(M.CharacterData) == Key; })
										   : nullptr;
		return Member && Member->bRunOnly;
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

	/** The Gold a convert of GearID gives (UEconomyService::GetDuplicateGoldForTier of its tier). */
	int32 InventoryVerbsConvertGold(const UPoolSubsystem &Pool, const FGuid &GearID)
	{
		const FWeaponInventoryEntry *Weapon = Pool.FindWeaponInPool(GearID);
		const FRingInventoryEntry *Adapter = Weapon ? nullptr : Pool.FindRingInPool(GearID);
		return UEconomyService::GetDuplicateGoldForTier(Weapon ? Weapon->Tier : Adapter ? Adapter->Tier : EItemTier::F_Tier);
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
		return WoRGameNames::GetInventoryVerbName(Verb);
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

	FText GetRefusal(UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryRow &Row)
	{
		const UPoolSubsystem *Pool = InventoryVerbsPoolOf(World);
		if (!Pool || Member.IsNone())
		{
			return LOCTEXT("NoStore", "No character's store to act on");
		}
		if (Row.bInChest != (Verb == EInventoryVerb::Take))
		{
			return Row.bInChest ? LOCTEXT("TakeFirst", "Take it from the chest first") : LOCTEXT("TakeIsForChest", "Only things in the chest can be taken");
		}
		const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
		const bool bPrimary = Row.IsGear() && Inventory && Inventory->IsPrimaryInAnyLoadout(Row.GearID);
		switch (Verb)
		{
		case EInventoryVerb::Give:
			if (Receiver.IsNone() || Receiver == Member)
			{
				return LOCTEXT("GiveSelf", "Choose another character to give to");
			}
			if (Row.IsGear() && InventoryVerbsIsRunOnly(World, Receiver))
			{
				return LOCTEXT("GiveRecruit", "That character is only here for the run and can't own gear");
			}
			if (bPrimary)
			{
				return LOCTEXT("GivePrimary", "This is a loadout's primary. Swap in another primary first");
			}
			return FText::GetEmpty();
		case EInventoryVerb::Store:
			if (Row.IsGear() && Inventory && !Inventory->GetLoadoutsUsing(Row.GearID).IsEmpty())
			{
				return LOCTEXT("StoreWorn", "It's in a loadout. Take it off first");
			}
			return FText::GetEmpty();
		case EInventoryVerb::Take:
			return FText::GetEmpty();
		case EInventoryVerb::Dismantle:
		case EInventoryVerb::Convert:
			break;
		}
		if (!Inventory)
		{
			return LOCTEXT("NotHere", "That character isn't here to do it");
		}
		if (Verb == EInventoryVerb::Convert && (!Row.IsGear() || !InventoryVerbsIsWeakerCopy(Inventory, Row.GearID)))
		{
			return LOCTEXT("NotWeaker", "Only the weaker copy of a duplicate converts to Gold");
		}
		if (Row.IsGear() && Pool->IsRunLayerActive() && !Pool->IsRunOnly(Row.GearID))
		{
			return LOCTEXT("HomeOnRun", "Gear from home can't go during a run; it comes back when the run ends");
		}
		if (bPrimary)
		{
			return LOCTEXT("PrimaryStays", "This is a loadout's primary. Swap in another primary first");
		}
		return FText::GetEmpty();
	}

	TArray<EInventoryVerb> GetVerbsFor(UWorld *World, FName Member, FName Receiver, const FInventoryRow &Row)
	{
		return GetAllVerbs().FilterByPredicate([&](EInventoryVerb Verb) { return GetRefusal(World, Verb, Member, Receiver, Row).IsEmpty(); });
	}

	FText GetConfirmText(UWorld *World, EInventoryVerb Verb, FName Member, const FInventoryRow &Row)
	{
		const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
		const UPoolSubsystem *Pool = InventoryVerbsPoolOf(World);
		if (!Inventory || !Pool || !Row.IsGear())
		{
			return FText::GetEmpty();
		}
		if (Verb == EInventoryVerb::Dismantle)
		{
			// D-016: an equipped piece asks first; anything else goes at once.
			return UEconomyService::GetDismantleWarning(Inventory->GetOwner(), Row.GearID);
		}
		if (Verb == EInventoryVerb::Convert)
		{
			// Converting strips the copy from every loadout too, so a worn copy gets D-016's warning first.
			const FText Convert = FText::Format(LOCTEXT("ConfirmConvert", "Convert {0} to {1} Gold?"), Row.Label, InventoryVerbsConvertGold(*Pool, Row.GearID));
			const FText Worn = UEconomyService::GetDismantleWarning(Inventory->GetOwner(), Row.GearID);
			return Worn.IsEmpty() ? Convert : FText::Format(LOCTEXT("ConfirmConvertWorn", "{0} {1}"), Worn, Convert);
		}
		return FText::GetEmpty();
	}

	FText Run(UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryRow &Row, int32 Count)
	{
		const FText Refusal = GetRefusal(World, Verb, Member, Receiver, Row);
		if (!Refusal.IsEmpty())
		{
			return Refusal;
		}
		UPoolSubsystem *Pool = InventoryVerbsPoolOf(World);
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

		// Dismantle and Convert go through the economy, which acts on the member's spawned actor (GetRefusal checked it).
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
		// A refusal the checks above don't foresee (e.g. during a fight) is logged by the economy with its reason.
		return bDone ? FText::GetEmpty() : FText::Format(LOCTEXT("VerbFailed", "Couldn't {0} {1} (see the log)"), GetVerbName(Verb), Row.Label);
	}

	FString GetRowsString(UWorld *World, FName Member, FName Receiver)
	{
		FString Out = FString::Printf(TEXT("[Inventory] %s's store and the chest:"), *Member.ToString());
		for (const FInventoryRow &Row : GatherRows(World, Member))
		{
			TArray<FString> Names;
			for (const EInventoryVerb Verb : GetVerbsFor(World, Member, Receiver, Row))
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
