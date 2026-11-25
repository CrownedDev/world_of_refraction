// InventoryScreenModel.cpp

#include "UI/Menus/InventoryScreenModel.h"
#include "Equipment/Weapons/WeaponData.h"
#include "UI/WoRIconTable.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/CrystalDescription.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/StatStone.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/FLoadoutSlot.h"
#include "UI/Menus/LoadoutV2Model.h"
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

	/** "Crystal · Fire"; the kind alone with no element. */
	FText KindWithElement(const FText &Kind, ESpellElement Element)
	{
		return Element == ESpellElement::None ? Kind
											   : FText::Format(LOCTEXT("KindAndElement", "{0} · {1}"), Kind, WoRGameNames::GetElementName(Element));
	}

	FInventoryScreenTile MakeEvolutionTile(const FEvolutionInventoryEntry &Entry, const UInventoryComponent &Inventory)
	{
		FInventoryScreenTile Tile;
		Tile.Kind = EInventoryScreenKind::Evolution;
		Tile.TierValue = Entry.Tier;
		// TODO: D-061: the evolution asset's name and description (and an apparel's name) are FString fields.
		Tile.Name = FText::FromString(Entry.Item->ItemName);
		Tile.Tier = WoRGameNames::GetTierLetter(Entry.Tier);
		Tile.Mark = LOCTEXT("MarkEvolution", "Ev");
		Tile.Element = Entry.Item->GetAssociatedElement();
		Tile.KindLine = KindWithElement(LOCTEXT("KindEvolution", "Evolution"), Tile.Element);
		Tile.bWorn = Inventory.IsEvolutionInUse(Entry.InstanceID);
		// B-487: its element's diamond, and a tier badge. B-584 (D-256): framed only while it is slotted.
		Tile.Icon.Kind = Tile.bWorn ? EWoRIconKind::Element : EWoRIconKind::LooseGem;
		Tile.Icon.Element = Tile.Element;
		Tile.Icon.Name = Tile.Name;
		Tile.bTierBadge = true;
		// D-231: the wear number alone, no word
		Tile.Details.Add({FText::GetEmpty(), FText::Format(LOCTEXT("DurabilityOfMax", "{0} / {1}"),
																					 FText::AsNumber(Entry.CurrentDurability),
																					 FText::AsNumber(Entry.Item->MaxDurability))});
		Tile.Note = FText::FromString(Entry.Item->Description);
		return Tile;
	}

	FInventoryScreenTile MakeApparelTile(const FApparelEntry &Entry, const UInventoryComponent &Inventory)
	{
		FInventoryScreenTile Tile;
		Tile.Kind = EInventoryScreenKind::Apparel;
		Tile.TierValue = Entry.Tier;
		Tile.Name = FText::FromString(Entry.Apparel->GetDisplayName());
		Tile.Tier = WoRGameNames::GetTierLetter(Entry.Tier);
		Tile.Mark = LOCTEXT("MarkApparel", "Ap");
		Tile.Element = Entry.GetElement();
		Tile.KindLine = KindWithElement(LOCTEXT("KindApparel", "Apparel"), Tile.Element);
		Tile.bWorn = Inventory.SavedLoadouts.ContainsByPredicate([&Entry](const FCombatLoadout &Loadout)
		{
			return Loadout.Apparel.ContainsByPredicate([&Entry](const FApparelEntry &Worn) { return Worn.PersistentID == Entry.PersistentID; });
		});
		if (Tile.bWorn)
		{
			Tile.Note = LOCTEXT("NoteWorn", "Equipped.");
		}
		// B-475 (D-208): the piece's rolled bonus line, so two copies can be told apart and compared.
		const FText Bonus = ApparelBonusLine::GetText(Entry.BonusLine);
		if (!Bonus.IsEmpty())
		{
			Tile.Details.Add({LOCTEXT("DetailBonusLine", "Bonus"), Bonus});
		}
		return Tile;
	}

	/** The member's evolutions (its spawned store, as the Spiritualist lists them) and its apparel (the pool's, by owner). */
	void AddEvolutionAndApparelTiles(const UPoolSubsystem &Pool, const UInventoryComponent *Inventory, FName Member, TArray<FInventoryScreenTile> &Tiles)
	{
		const AActor *Owner = Inventory ? Inventory->GetOwner() : nullptr;
		if (!Owner)
		{
			return; // the member isn't in this level
		}
		if (const UEvolutionInventoryComponent *Evolutions = Owner->FindComponentByClass<UEvolutionInventoryComponent>())
		{
			for (const FEvolutionInventoryEntry &Entry : Evolutions->Entries)
			{
				if (Entry.Item)
				{
					Tiles.Add(MakeEvolutionTile(Entry, *Inventory));
				}
			}
		}
		for (const FApparelEntry &Entry : Pool.GetOwnedApparel())
		{
			if (Entry.Apparel && Pool.GetGearOwner(Entry.PersistentID) == Member)
			{
				Tiles.Add(MakeApparelTile(Entry, *Inventory));
			}
		}
	}

	bool PassesFilter(const FInventoryScreenTile &Tile, EInventoryScreenFilter Filter)
	{
		switch (Filter)
		{
		case EInventoryScreenFilter::Weapons:
			return Tile.Kind == EInventoryScreenKind::Weapon;
		case EInventoryScreenFilter::Adapters:
			return Tile.Kind == EInventoryScreenKind::Adapter;
		case EInventoryScreenFilter::Crystals:
			return Tile.Kind == EInventoryScreenKind::Stack;
		case EInventoryScreenFilter::Evolutions:
			return Tile.Kind == EInventoryScreenKind::Evolution;
		case EInventoryScreenFilter::Apparel:
			return Tile.Kind == EInventoryScreenKind::Apparel;
		default:
			return true;
		}
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
		Tile.Kind = Weapon ? EInventoryScreenKind::Weapon : EInventoryScreenKind::Adapter;
		Tile.TierValue = Weapon ? Weapon->Tier : Adapter ? Adapter->Tier : EItemTier::F_Tier;
		Tile.Name = Gear.Name.IsEmpty() ? Row.Label : Gear.Name;
		Tile.Tier = Gear.Tier;
		Tile.KindLine = Gear.KindLine;
		Tile.Mark = Weapon ? LOCTEXT("MarkWeapon", "W") : LOCTEXT("MarkAdapter", "A");
		if (Weapon && Weapon->Weapon)
		{
			Tile.Icon.Kind = EWoRIconKind::WeaponType; // B-487
			Tile.Icon.WeaponType = Weapon->Weapon->WeaponType;
			Tile.Icon.Name = WoRGameNames::GetWeaponTypeName(Weapon->Weapon->WeaponType);
		}
		Tile.Element = Slot.Gear.AttachedItem.GetElement();
		if (Adapter)
		{
			Tile.Icon = WoRIcons::MakeAdapterIcon(Tile.Element, Tile.Name); // B-543: the B-530 frame with its crystal, not "A"
		}
		Tile.bWorn = !Row.bInChest && Inventory && !Inventory->GetLoadoutsUsing(Row.GearID).IsEmpty();
		Tile.Details = Gear.Details;
		// B-487b: a filled socket's line carries its badge (drawn in its label's place) and its tier: "Garnet (F)".
		const FRuntimeAttachedItem *SocketItems[] = {&Slot.Gear.AttachedItem, &Slot.Gear.StoneSocket}; // in LoadoutScreenSockets order
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SocketItems) && Gear.Sockets.IsValidIndex(Index); ++Index)
		{
			EItemTier SocketTier = EItemTier::F_Tier;
			if (WoRLoadoutV2::GetAttachmentTier(*SocketItems[Index], SocketTier))
			{
				Tile.Details.Add({Gear.Sockets[Index].Label,
								  FText::Format(LOCTEXT("SocketNameAndTier", "{0} ({1})"), Gear.Sockets[Index].Name, WoRGameNames::GetTierLetter(SocketTier)),
								  WoRLoadoutV2::MakeSocketIcon(*SocketItems[Index])});
			}
		}
		if (Tile.bWorn)
		{
			Tile.Note = LOCTEXT("NoteWorn", "Equipped.");
		}
		return Tile;
	}

	FInventoryScreenTile MakeStackTile(const WoRInventoryVerbs::FInventoryRow &Row)
	{
		const FCrystalId Id = StatStone::AsRowId(Row.Item); // a StatStone reads as its sub-stat's stone (B-353)
		const bool bStone = CrystalTypeHelpers::IsAugmentStoneType(Row.Item.Type);

		FInventoryScreenTile Tile;
		Tile.Row = Row;
		Tile.Kind = EInventoryScreenKind::Stack;
		Tile.TierValue = Id.Tier;
		Tile.Name = WoRGameNames::GetCrystalTypeName(Id.Type);
		const bool bPotion = CrystalTypeHelpers::IsPotionType(Row.Item.Type); // B-579: no tier, no element, the stone's plain icon
		Tile.Tier = bPotion ? FText::GetEmpty() : WoRGameNames::GetTierLetter(Id.Tier);
		Tile.Element = ItemIdentity::GetElement(Row.Item);
		Tile.Mark = bPotion ? LOCTEXT("MarkPotion", "P") : bStone ? LOCTEXT("MarkStone", "S") : LOCTEXT("MarkCrystal", "C");
		Tile.KindLine = bPotion ? LOCTEXT("KindPotion", "Potion")
								: KindWithElement(bStone ? LOCTEXT("KindStone", "Stone") : LOCTEXT("KindCrystal", "Crystal"), Tile.Element);
		if (Row.bInChest)
		{
			Tile.Details.Add({LOCTEXT("DetailStored", "Stored"), FText::AsNumber(Row.Count)});
		}
		else
		{
			Tile.Details.Add({LOCTEXT("DetailStack", "Stack"), FText::AsNumber(Row.Count)}); // D-250: no stack limit
		}
		Tile.Note = CrystalDescription::GetItemEffectText(Row.Item); // B-460
		// B-487: a crystal is its element's diamond (Ice has none of its own: it is keyed Water), a stone the stone;
		// its tier is a badge; its effect sentence gets the effect's icon.
		Tile.Icon.Kind = (bStone || bPotion) ? EWoRIconKind::Stone : EWoRIconKind::LooseGem; // B-584 (D-256): a stack is in no socket
		Tile.Icon.Element = Tile.Element;
		Tile.Icon.Name = Tile.Name;
		Tile.bTierBadge = !bPotion;
		Tile.EffectIcon = WoRIcons::MakeItemEffectIcon(Row.Item);
		return Tile;
	}

	bool IsOnTab(const WoRInventoryVerbs::FInventoryRow &Row, EInventoryScreenTab Tab)
	{
		if (Tab == EInventoryScreenTab::Chest)
		{
			return Row.bInChest;
		}
		if (Tab == EInventoryScreenTab::Loadout)
		{
			return false;
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
	// D-249: the Loadout tab (B-543, next to Gear) is gone: the merged screen's Loadout button replaces it.
	// OBSOLETE (D-249; delete after Crown's PIE pass): EInventoryScreenTab::Loadout and what handles it here and in
	// UInventoryScreenWidget (OpenLoadoutScreen).
	// B-575c (D-250): no Chest tab, items have no limit. OBSOLETE: EInventoryScreenTab::Chest and its page code here,
	// hidden until Crown's PIE pass, then deleted.
	static const TArray<EInventoryScreenTab> Tabs = {EInventoryScreenTab::Gear, EInventoryScreenTab::Pouch};
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
	case EInventoryScreenTab::Loadout:
		return LOCTEXT("TabLoadout", "Loadout");
	}
	return FText::GetEmpty();
}

FInventoryScreenTile WoRInventoryScreen::BuildStackTile(const WoRInventoryVerbs::FInventoryRow &Row)
{
	return InventoryScreenBuild::MakeStackTile(Row);
}

TArray<EInventoryScreenFilter> WoRInventoryScreen::GetFilters(EInventoryScreenTab Tab)
{
	// Q-461: only the kinds the tab can hold. Stacks are carried in the pouch, so the Gear tab has no Crystals;
	// evolutions and apparel aren't stored yet (Q-323), so the Chest has neither.
	switch (Tab)
	{
	case EInventoryScreenTab::Gear:
		return {EInventoryScreenFilter::All, EInventoryScreenFilter::Weapons, EInventoryScreenFilter::Adapters, EInventoryScreenFilter::Evolutions,
				EInventoryScreenFilter::Apparel};
	case EInventoryScreenTab::Chest:
		return {EInventoryScreenFilter::All, EInventoryScreenFilter::Weapons, EInventoryScreenFilter::Adapters, EInventoryScreenFilter::Crystals};
	default:
		return {};
	}
}

FText WoRInventoryScreen::GetFilterName(EInventoryScreenFilter Filter)
{
	switch (Filter)
	{
	case EInventoryScreenFilter::All:
		return LOCTEXT("FilterAll", "All");
	case EInventoryScreenFilter::Weapons:
		return LOCTEXT("FilterWeapons", "Weapons");
	case EInventoryScreenFilter::Adapters:
		return LOCTEXT("FilterAdapters", "Adapters");
	case EInventoryScreenFilter::Crystals:
		return LOCTEXT("FilterCrystals", "Crystals");
	case EInventoryScreenFilter::Evolutions:
		return LOCTEXT("FilterEvolutions", "Evolutions");
	case EInventoryScreenFilter::Apparel:
		return LOCTEXT("FilterApparel", "Apparel");
	}
	return FText::GetEmpty();
}

FText WoRInventoryScreen::GetSortName(EInventoryScreenSort Sort)
{
	switch (Sort)
	{
	case EInventoryScreenSort::Kind:
		return LOCTEXT("SortKind", "Kind");
	case EInventoryScreenSort::Tier:
		return LOCTEXT("SortTier", "Tier");
	case EInventoryScreenSort::Name:
		return LOCTEXT("SortName", "Name");
	}
	return FText::GetEmpty();
}

EInventoryScreenSort WoRInventoryScreen::GetNextSort(EInventoryScreenSort Sort)
{
	switch (Sort)
	{
	case EInventoryScreenSort::Kind:
		return EInventoryScreenSort::Tier;
	case EInventoryScreenSort::Tier:
		return EInventoryScreenSort::Name;
	default:
		return EInventoryScreenSort::Kind;
	}
}

FInventoryScreenPage WoRInventoryScreen::BuildPage(UWorld *World, FName Member, EInventoryScreenTab Tab, EInventoryScreenFilter Filter,
												   EInventoryScreenSort Sort)
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
	if (Tab == EInventoryScreenTab::Gear)
	{
		AddEvolutionAndApparelTiles(*Pool, Inventory, Member, Page.Tiles);
	}
	Page.Tiles.RemoveAll([Filter](const FInventoryScreenTile &Tile) { return !PassesFilter(Tile, Filter); });
	if (Sort == EInventoryScreenSort::Tier)
	{
		Page.Tiles.StableSort([](const FInventoryScreenTile &A, const FInventoryScreenTile &B) { return A.TierValue > B.TierValue; });
	}
	else if (Sort == EInventoryScreenSort::Name)
	{
		Page.Tiles.StableSort([](const FInventoryScreenTile &A, const FInventoryScreenTile &B) { return A.Name.CompareTo(B.Name) < 0; });
	}
	switch (Tab)
	{
	case EInventoryScreenTab::Gear:
		Page.EmptyNote = LOCTEXT("EmptyGear", "No gear.");
		break;
	case EInventoryScreenTab::Pouch:
	{
		const FName PouchKey = WoRGive::GetPouchKey(World, Member); // a recruit carries the host's (B-361)
		Page.Slots = Pool->GetItemSlotCount(PouchKey);
		Page.LockedSlots = FMath::Max(0, InventoryConstants::ITEM_SLOTS_MAX - Page.Slots);
		// D-250: the counter is the fight-item pick, not how much is carried (that has no limit).
		Page.Heading = FText::Format(LOCTEXT("PouchHeading", "Pouch  {0}/{1} fight items"), FText::AsNumber(Pool->GetUsedItemSlots(PouchKey)),
									 FText::AsNumber(Page.Slots));
		for (FInventoryScreenTile &Tile : Page.Tiles)
		{
			if (Tile.IsStack() && Pool->IsFightItem(PouchKey, Tile.Row.Item))
			{
				Tile.Details.Add({LOCTEXT("DetailFightItem", "Fight item"),
								  FText::Format(LOCTEXT("FightItemUpTo", "up to {0} a fight"), FText::AsNumber(InventoryConstants::ITEM_STACK_MAX))});
			}
		}
		Page.EmptyNote = LOCTEXT("EmptyPouch", "The pouch is empty.");
		break;
	}
	case EInventoryScreenTab::Chest:
		Page.Heading = LOCTEXT("ChestHeading", "Chest");
		Page.EmptyNote = LOCTEXT("EmptyChest", "The chest is empty.");
		break;
	case EInventoryScreenTab::Loadout:
		Page.EmptyNote = LOCTEXT("LoadoutTabNote", "Select to open this character's Loadout (the Equipment screen).");
		break;
	}
	if (!Page.Tiles.IsEmpty())
	{
		Page.EmptyNote = FText::GetEmpty();
	}
	else if (Filter != EInventoryScreenFilter::All)
	{
		Page.EmptyNote = LOCTEXT("EmptyFilter", "Nothing of this kind here.");
	}
	return Page;
}

TArray<FInventoryScreenVerb> WoRInventoryScreen::BuildVerbs(UWorld *World, FName Member, EInventoryScreenTab Tab, const FInventoryScreenTile &Tile)
{
	// Q-323: nothing gives or stores an evolution or apparel yet, so their verbs are listed with that reason.
	const FText NotYet = Tile.Kind == EInventoryScreenKind::Evolution ? LOCTEXT("EvolutionNoVerbs", "Evolutions can't be given or stored yet")
						 : Tile.Kind == EInventoryScreenKind::Apparel ? LOCTEXT("ApparelNoVerbs", "Apparel can't be given or stored yet")
																		 : FText::GetEmpty();
	TArray<FInventoryScreenVerb> Verbs;
	for (const EInventoryVerb Verb : WoRInventoryVerbs::GetShownVerbs()) // B-575c: Give only, no Store / Take
	{
		FInventoryScreenVerb &Line = Verbs.AddDefaulted_GetRef();
		Line.Verb = Verb;
		Line.Name = WoRInventoryVerbs::GetVerbName(World, Verb);
		Line.Refusal = !NotYet.IsEmpty()				  ? NotYet
					   : Verb == EInventoryVerb::Give ? InventoryScreenBuild::GetGiveRefusal(World, Member, Tile)
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
		if (!Tile.IsStack() || !Pool)
		{
			continue;
		}
		// A stack: the receiver's pouch must have room (WoRGive::TryGiveCarried's own checks, said before the press).
		const FName Pouch = WoRGive::GetPouchKey(World, Other.Key);
		const int32 FreeSlots = Pool->GetItemSlotCount(Pouch) - Pool->GetUsedItemSlots(Pouch);
		const bool bFull = Pool->GetCarryRoom(Pouch, Tile.Row.Item) <= 0;
		Receiver.Note = bFull			  ? LOCTEXT("ReceiverFull", "full")
						: FreeSlots > 0 ? FText::Format(LOCTEXT("ReceiverFree", "{0} fight item {0}|plural(one=slot,other=slots) free"), FreeSlots)
										: FText::GetEmpty(); // D-250: carrying has no limit; only free fight-item slots are worth saying
		if (!Receiver.Refusal.IsEmpty())
		{
			continue;
		}
		if (Pouch == WoRGive::GetPouchKey(World, Member))
		{
			Receiver.Refusal = LOCTEXT("SamePouch", "They share a pouch.");
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
	if (!Tile.IsStack() || !bWholeStack)
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
									   Tile.IsStack() ? *FString::Printf(TEXT(" x%d"), Tile.Row.Count) : TEXT(""), Tile.bWorn ? TEXT(" [E]") : TEXT(""),
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
