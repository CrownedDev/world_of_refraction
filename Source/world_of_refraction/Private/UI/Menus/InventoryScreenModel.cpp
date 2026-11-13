// InventoryScreenModel.cpp

#include "UI/Menus/InventoryScreenModel.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/CrystalDescription.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/StatStone.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/FLoadoutSlot.h"
#include "Pool/PoolSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRInventoryScreen"

namespace InventoryScreenBuild
{
	const UPoolSubsystem *PoolOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	FInventoryScreenTile MakeGearTile(const UPoolSubsystem &Pool, const UInventoryComponent *Inventory, const WoRInventoryVerbs::FInventoryRow &Row)
	{
		const FWeaponInventoryEntry *Weapon = Pool.FindWeaponInPool(Row.GearID);
		const FRingInventoryEntry *Adapter = Weapon ? nullptr : Pool.FindRingInPool(Row.GearID);
		FLoadoutSlot Slot;
		if (Weapon)
		{
			Slot.Gear = FGearInstance::FromWeaponEntry(*Weapon);
		}
		else if (Adapter)
		{
			Slot.Gear = FGearInstance::FromRingEntry(*Adapter);
		}
		const FLoadoutScreenRow Gear = WoRLoadoutScreen::BuildGearRow(Slot); // the loadout screen's words for a piece

		FInventoryScreenTile Tile;
		Tile.Row = Row;
		Tile.Name = Gear.Name.IsEmpty() ? Row.Label : Gear.Name;
		Tile.Tier = Gear.Tier;
		Tile.KindLine = Gear.KindLine;
		Tile.Mark = Weapon ? LOCTEXT("MarkWeapon", "W") : LOCTEXT("MarkAdapter", "A");
		Tile.Element = Slot.Gear.AttachedItem.GetElement();
		Tile.bWorn = !Row.bInChest && Inventory && !Inventory->GetLoadoutsUsing(Row.GearID).IsEmpty();
		Tile.Details = Gear.Details;
		for (const FLoadoutScreenSocket &Socket : Gear.Sockets)
		{
			if (!Socket.bEmpty)
			{
				Tile.Details.Add({Socket.Label, Socket.Name});
			}
		}
		if (Tile.bWorn)
		{
			Tile.Note = LOCTEXT("NoteWorn", "Worn in a loadout.");
		}
		return Tile;
	}

	FInventoryScreenTile MakeStackTile(const WoRInventoryVerbs::FInventoryRow &Row)
	{
		const FCrystalId Id = StatStone::AsRowId(Row.Item); // a StatStone reads as its sub-stat's stone (B-353)
		const bool bStone = CrystalTypeHelpers::IsAugmentStoneType(Row.Item.Type);

		FInventoryScreenTile Tile;
		Tile.Row = Row;
		Tile.Name = WoRGameNames::GetCrystalTypeName(Id.Type);
		Tile.Tier = WoRGameNames::GetTierLetter(Id.Tier);
		Tile.Element = ItemIdentity::GetElement(Row.Item);
		Tile.Mark = bStone ? LOCTEXT("MarkStone", "S") : LOCTEXT("MarkCrystal", "C");
		const FText Kind = bStone ? LOCTEXT("KindStone", "Stone") : LOCTEXT("KindCrystal", "Crystal");
		Tile.KindLine = Tile.Element == ESpellElement::None
							? Kind
							: FText::Format(LOCTEXT("KindAndElement", "{0} · {1}"), Kind, WoRGameNames::GetElementName(Tile.Element));
		if (Row.bInChest)
		{
			Tile.Details.Add({LOCTEXT("DetailStored", "Stored"), FText::AsNumber(Row.Count)});
		}
		else
		{
			Tile.Details.Add({LOCTEXT("DetailStack", "Stack"), FText::Format(LOCTEXT("StackOfMax", "{0} of {1}"), FText::AsNumber(Row.Count),
																		 FText::AsNumber(InventoryConstants::ITEM_STACK_MAX))});
		}
		// TODO: B-460 (D-061): the crystal description table is FString; it becomes FText there.
		Tile.Note = FText::FromString(CrystalDescription::GetItemEffectText(Row.Item));
		return Tile;
	}

	bool IsOnTab(const WoRInventoryVerbs::FInventoryRow &Row, EInventoryScreenTab Tab)
	{
		if (Tab == EInventoryScreenTab::Chest)
		{
			return Row.bInChest;
		}
		return !Row.bInChest && Row.IsGear() == (Tab == EInventoryScreenTab::Gear);
	}

	/** Why Tile can't be given to anyone; empty when at least one member can take it. */
	FText GetGiveRefusal(UWorld *World, FName Member, const FInventoryScreenTile &Tile)
	{
		if (Tile.Row.bInChest)
		{
			return WoRInventoryVerbs::GetRefusal(World, EInventoryVerb::Give, Member, NAME_None, Tile.Row);
		}
		const TArray<FInventoryScreenReceiver> Receivers = WoRInventoryScreen::BuildReceivers(World, Member, Tile);
		if (Receivers.IsEmpty())
		{
			return LOCTEXT("NoOneToGiveTo", "There is no one else to give it to");
		}
		const bool bAnyCan = Receivers.ContainsByPredicate([](const FInventoryScreenReceiver &Receiver) { return Receiver.Refusal.IsEmpty(); });
		return bAnyCan ? FText::GetEmpty() : Receivers[0].Refusal;
	}
}

const TArray<EInventoryScreenTab> &WoRInventoryScreen::GetTabs()
{
	static const TArray<EInventoryScreenTab> Tabs = {EInventoryScreenTab::Gear, EInventoryScreenTab::Pouch, EInventoryScreenTab::Chest};
	return Tabs;
}

FText WoRInventoryScreen::GetTabName(EInventoryScreenTab Tab)
{
	switch (Tab)
	{
	case EInventoryScreenTab::Gear:
		return LOCTEXT("TabGear", "Gear");
	case EInventoryScreenTab::Pouch:
		return LOCTEXT("TabPouch", "Pouch");
	case EInventoryScreenTab::Chest:
		return LOCTEXT("TabChest", "Chest");
	}
	return FText::GetEmpty();
}

FInventoryScreenPage WoRInventoryScreen::BuildPage(UWorld *World, FName Member, EInventoryScreenTab Tab)
{
	using namespace InventoryScreenBuild;

	FInventoryScreenPage Page;
	const UPoolSubsystem *Pool = PoolOf(World);
	if (!Pool || Member.IsNone())
	{
		Page.EmptyNote = LOCTEXT("NoParty", "There is no party to show.");
		return Page;
	}
	const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
	for (const WoRInventoryVerbs::FInventoryRow &Row : WoRInventoryVerbs::GatherRows(World, Member))
	{
		if (IsOnTab(Row, Tab))
		{
			Page.Tiles.Add(Row.IsGear() ? MakeGearTile(*Pool, Inventory, Row) : MakeStackTile(Row));
		}
	}
	switch (Tab)
	{
	case EInventoryScreenTab::Gear:
		Page.EmptyNote = LOCTEXT("EmptyGear", "No weapons or adapters.");
		break;
	case EInventoryScreenTab::Pouch:
	{
		const FName PouchKey = WoRGive::GetPouchKey(World, Member); // a recruit carries the host's (B-361)
		Page.Slots = Pool->GetItemSlotCount(PouchKey);
		Page.LockedSlots = FMath::Max(0, InventoryConstants::ITEM_SLOTS_MAX - Page.Slots);
		Page.Heading = FText::Format(LOCTEXT("PouchHeading", "Pouch  {0} of {1} slots"), FText::AsNumber(Page.Tiles.Num()), FText::AsNumber(Page.Slots));
		Page.EmptyNote = LOCTEXT("EmptyPouch", "The pouch is empty.");
		break;
	}
	case EInventoryScreenTab::Chest:
		Page.Heading = LOCTEXT("ChestHeading", "Home chest");
		Page.EmptyNote = LOCTEXT("EmptyChest", "The chest is empty.");
		break;
	}
	if (!Page.Tiles.IsEmpty())
	{
		Page.EmptyNote = FText::GetEmpty();
	}
	return Page;
}

TArray<FInventoryScreenVerb> WoRInventoryScreen::BuildVerbs(UWorld *World, FName Member, EInventoryScreenTab Tab, const FInventoryScreenTile &Tile)
{
	TArray<FInventoryScreenVerb> Verbs;
	for (const EInventoryVerb Verb : WoRInventoryVerbs::GetAllVerbs())
	{
		if (Verb == EInventoryVerb::Take && Tab != EInventoryScreenTab::Chest)
		{
			continue; // D-195: Take is listed only at storage
		}
		FInventoryScreenVerb &Line = Verbs.AddDefaulted_GetRef();
		Line.Verb = Verb;
		Line.Name = WoRInventoryVerbs::GetVerbName(World, Verb);
		Line.Refusal = Verb == EInventoryVerb::Give ? InventoryScreenBuild::GetGiveRefusal(World, Member, Tile)
													: WoRInventoryVerbs::GetRefusal(World, Verb, Member, NAME_None, Tile.Row);
	}
	return Verbs;
}

TArray<FInventoryScreenReceiver> WoRInventoryScreen::BuildReceivers(UWorld *World, FName Member, const FInventoryScreenTile &Tile)
{
	TArray<FInventoryScreenReceiver> Receivers;
	const UPoolSubsystem *Pool = InventoryScreenBuild::PoolOf(World);
	for (const FLoadoutScreenMember &Other : WoRLoadoutScreen::BuildMembers(World))
	{
		if (Other.Key == Member)
		{
			continue;
		}
		FInventoryScreenReceiver &Receiver = Receivers.AddDefaulted_GetRef();
		Receiver.Key = Other.Key;
		Receiver.Name = Other.Name;
		Receiver.Refusal = WoRInventoryVerbs::GetRefusal(World, EInventoryVerb::Give, Member, Other.Key, Tile.Row);
		if (Tile.Row.IsGear() || !Pool)
		{
			continue;
		}
		// A stack: the receiver's pouch must have room (WoRGive::TryGiveCarried's own checks, said before the press).
		const FName Pouch = WoRGive::GetPouchKey(World, Other.Key);
		const int32 FreeSlots = Pool->GetItemSlotCount(Pouch) - Pool->GetUsedItemSlots(Pouch);
		const bool bFull = Pool->GetCarryRoom(Pouch, Tile.Row.Item) <= 0;
		Receiver.Note = bFull			  ? LOCTEXT("ReceiverFull", "full")
						: FreeSlots > 0 ? FText::Format(LOCTEXT("ReceiverFree", "{0} {0}|plural(one=slot,other=slots) free"), FreeSlots)
										: LOCTEXT("ReceiverStackRoom", "room in the stack");
		if (!Receiver.Refusal.IsEmpty())
		{
			continue;
		}
		if (Pouch == WoRGive::GetPouchKey(World, Member))
		{
			Receiver.Refusal = LOCTEXT("SamePouch", "They carry the same pouch");
		}
		else if (bFull)
		{
			Receiver.Refusal = LOCTEXT("NoRoom", "No room in their pouch");
		}
	}
	return Receivers;
}

int32 WoRInventoryScreen::GetMoveCount(const UWorld *World, EInventoryVerb Verb, FName Member, FName Receiver, const FInventoryScreenTile &Tile,
									   bool bWholeStack)
{
	if (Tile.Row.IsGear() || !bWholeStack)
	{
		return 1;
	}
	int32 Count = Tile.Row.Count;
	const UPoolSubsystem *Pool = InventoryScreenBuild::PoolOf(World);
	const FName Into = Verb == EInventoryVerb::Take ? Member : Verb == EInventoryVerb::Give ? Receiver : NAME_None;
	if (Pool && !Into.IsNone())
	{
		Count = FMath::Min(Count, Pool->GetCarryRoom(WoRGive::GetPouchKey(World, Into), Tile.Row.Item));
	}
	return FMath::Max(1, Count);
}

FString WoRInventoryScreen::GetInventoryScreenString(UWorld *World)
{
	FString Out = TEXT("[InventoryScreen]");
	for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
	{
		for (const EInventoryScreenTab Tab : GetTabs())
		{
			const FInventoryScreenPage Page = BuildPage(World, Member.Key, Tab);
			Out += FString::Printf(TEXT("\n  %s, %s: %s%s"), *Member.Name.ToString(), *GetTabName(Tab).ToString(), *Page.Heading.ToString(),
								   *Page.EmptyNote.ToString());
			for (const FInventoryScreenTile &Tile : Page.Tiles)
			{
				TArray<FString> Verbs;
				for (const FInventoryScreenVerb &Verb : BuildVerbs(World, Member.Key, Tab, Tile))
				{
					Verbs.Add(Verb.Refusal.IsEmpty() ? Verb.Name.ToString() : FString::Printf(TEXT("%s (no: %s)"), *Verb.Name.ToString(), *Verb.Refusal.ToString()));
				}
				Out += FString::Printf(TEXT("\n    %s (%s)%s%s, %s: %s"), *Tile.Name.ToString(), *Tile.Tier.ToString(),
									   Tile.Row.IsGear() ? TEXT("") : *FString::Printf(TEXT(" x%d"), Tile.Row.Count), Tile.bWorn ? TEXT(" [E]") : TEXT(""),
									   *Tile.KindLine.ToString(), *FString::Join(Verbs, TEXT(", ")));
			}
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GInventoryScreenPrintCommand(
	TEXT("wor.UI.PrintInventory"),
	TEXT("DEBUG (B-441): what the inventory screen would show for each party member: the three tabs, each tile and its verbs."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRInventoryScreen::GetInventoryScreenString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
