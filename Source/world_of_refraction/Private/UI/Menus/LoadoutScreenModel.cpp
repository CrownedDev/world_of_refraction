// LoadoutScreenModel.cpp

#include "UI/Menus/LoadoutScreenModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Durability/GearRepair.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/GearSockets.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Algo/Count.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FLoadoutSlot.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WornGear.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutScreen"

namespace LoadoutScreenBuild
{
	/** The detail panel names this many skills, then "+N". */
	constexpr int32 MAX_NAMED_SKILLS = 3;

	FText SocketLabel(const FRuntimeAttachedItem &Item, const FText &WhenEmpty)
	{
		if (Item.IsEvolution())
		{
			return LOCTEXT("SocketEvolution", "Evolution");
		}
		if (Item.IsAugmentStone())
		{
			return LOCTEXT("SocketStone", "Stone");
		}
		if (Item.IsCrystal())
		{
			return LOCTEXT("SocketCrystal", "Crystal");
		}
		if (Item.IsFusion())
		{
			return LOCTEXT("SocketFusion", "Fusion");
		}
		return WhenEmpty;
	}

	FText AttachmentName(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEvolution())
		{
			return Item.Evolution.Item ? FText::FromString(Item.Evolution.Item->ItemName) : FText::GetEmpty();
		}
		if (Item.IsFusion())
		{
			return LOCTEXT("FusionName", "Fusion"); // retired (D-141): only an old asset still holds one
		}
		return WoRGameNames::GetCrystalTypeName(StatStone::AsRowId(Item.Crystal.Id).Type);
	}

	FLoadoutScreenSocket MakeSocket(const FRuntimeAttachedItem &Item, const FText &WhenEmpty)
	{
		FLoadoutScreenSocket Socket;
		Socket.bEmpty = Item.IsEmpty();
		Socket.bEvolution = Item.IsEvolution();
		Socket.bBroken = !Socket.bEmpty && Item.IsBroken(); // B-534
		Socket.bWorn = Item.IsCrystal() && Item.GetCurrentDurability() < Item.GetMaxDurability();
		Socket.Label = SocketLabel(Item, WhenEmpty);
		if (!Socket.bEmpty)
		{
			Socket.Name = AttachmentName(Item);
			Socket.Element = Item.GetElement();
		}
		return Socket;
	}

	template <typename TSkill>
	FText JoinSkillNames(const TArray<TObjectPtr<TSkill>> &Skills)
	{
		TArray<FText> Names;
		int32 Count = 0;
		for (const TSkill *Skill : Skills)
		{
			if (!Skill)
			{
				continue;
			}
			if (++Count <= MAX_NAMED_SKILLS)
			{
				Names.Add(FText::FromString(Skill->Name));
			}
		}
		FText Joined = FText::Join(LOCTEXT("SkillSeparator", ", "), Names);
		return Count > MAX_NAMED_SKILLS ? FText::Format(LOCTEXT("SkillsAndMore", "{0} +{1}"), Joined, FText::AsNumber(Count - MAX_NAMED_SKILLS)) : Joined;
	}

	void AddDetail(FLoadoutScreenRow &Row, const FText &Label, const FText &Value)
	{
		if (!Value.IsEmpty())
		{
			Row.Details.Add({Label, Value});
		}
	}

	/** D-206 (B-472): only what is inside costs: "Crystal 1", "Evolution 2 + Stone 1 = 3", or "Free" for bare gear. */
	FText FormatCostLine(const FLoadoutSlot &Slot, int32 Cost)
	{
		TArray<FText> Parts;
		for (const FRuntimeAttachedItem *Socket : {&Slot.Gear.AttachedItem, &Slot.Gear.StoneSocket})
		{
			const int32 SocketCost = FLoadoutRules::AttachmentCost(*Socket);
			if (!Socket->IsEmpty() && SocketCost > 0)
			{
				Parts.Add(FText::Format(LOCTEXT("CostPart", "{0} {1}"), SocketLabel(*Socket, FText::GetEmpty()), FText::AsNumber(SocketCost)));
			}
		}
		if (Parts.Num() == 0)
		{
			return LOCTEXT("CostFree", "Free");
		}
		const FText Sum = FText::Join(LOCTEXT("CostPlus", " + "), Parts);
		return Parts.Num() == 1 ? Sum : FText::Format(LOCTEXT("CostSum", "{0} = {1}"), Sum, FText::AsNumber(Cost));
	}

	FText FormatDurability(const FRuntimeAttachedItem &Item)
	{
		const int32 Max = Item.GetMaxDurability();
		return Item.IsEmpty() || Max <= 0 ? FText::GetEmpty()
										  : FText::Format(LOCTEXT("Durability", "{0} / {1}"), FText::AsNumber(Item.GetCurrentDurability()), FText::AsNumber(Max));
	}

	FLoadoutScreenRow MakeGearRow(const FLoadoutSlot &Slot)
	{
		const FGearInstance &Gear = Slot.Gear;
		FLoadoutScreenRow Row;
		Row.Name = FText::FromString(Gear.Asset->GetDisplayName());
		Row.Tier = WoRGameNames::GetTierLetter(Gear.Tier);
		Row.Sockets.Add(MakeSocket(Gear.AttachedItem, LOCTEXT("SocketCrystal", "Crystal")));
		Row.Sockets.Add(MakeSocket(Gear.StoneSocket, LOCTEXT("SocketStone", "Stone")));
		Row.bCrystalFixed = GearSockets::IsCrystalFixed(Gear);
		Row.AcceptedCrystal = Gear.Asset ? Gear.Asset->AcceptedCrystalType : ECrystalType::None;
		Row.Cost = FLoadoutRules::SlotCost(Slot);
		Row.CostLine = FormatCostLine(Slot, Row.Cost);

		const FText KindFormat = LOCTEXT("KindAndType", "{0} · {1}");
		if (const UWeaponData *Weapon = Gear.GetWeapon())
		{
			Row.Kind = ELoadoutScreenRowKind::Weapon;
			Row.KindLine = FText::Format(KindFormat, LOCTEXT("KindWeapon", "Weapon"), WoRGameNames::GetWeaponTypeName(Weapon->WeaponType));
			const FText Wield = WoRGameNames::GetWieldModeName(Weapon->WieldMode); // D-180: the wield mode in the kind line
			if (!Wield.IsEmpty())
			{
				Row.KindLine = FText::Format(KindFormat, Row.KindLine, Wield);
			}
			AddDetail(Row, LOCTEXT("DetailPower", "Power"), Weapon->Power > 0 ? FText::AsNumber(Weapon->Power) : FText::GetEmpty());
			AddDetail(Row, LOCTEXT("DetailAbilities", "Abilities"), JoinSkillNames(Slot.AssignedAbilities));
		}
		else if (const URingData *Adapter = Gear.GetAdapter())
		{
			Row.Kind = ELoadoutScreenRowKind::Adapter;
			Row.KindLine = FText::Format(KindFormat, LOCTEXT("KindAdapter", "Adapter"), WoRGameNames::GetAdapterFormName(Adapter->Form));
			AddDetail(Row, LOCTEXT("DetailSpells", "Spells"), JoinSkillNames(Gear.AssignedSpells));
		}
		const ESpellElement Element = Gear.AttachedItem.GetElement();
		AddDetail(Row, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
		AddDetail(Row, FText::GetEmpty(), FormatDurability(Gear.AttachedItem)); // D-231: the wear number alone, no word
		return Row;
	}

	/** B-309d4: the screen place of a slot's WornGearLayout place. */
	ELoadoutScreenPlace PlaceOf(int32 Place)
	{
		using namespace WornGearLayout;
		return Place == PRIMARY_INDEX ? ELoadoutScreenPlace::Primary
			   : Place == SECONDARY_INDEX ? ELoadoutScreenPlace::Secondary
			   : Place >= FIRST_ADAPTER_INDEX ? ELoadoutScreenPlace::Adapter
											  : ELoadoutScreenPlace::None;
	}

	FLoadoutScreenRow MakeEmptyGearRow(const FText &Name, ELoadoutScreenPlace Place)
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::EmptyGear;
		Row.Name = Name;
		Row.Place = Place;
		Row.bCountsToBudget = false; // nothing in it costs nothing; no number shown
		Row.Note = LOCTEXT("NoteEmptyGear", "Empty.");
		return Row;
	}

	/** Places the class's fight reads, as FCombatLoadout's slot mirror (ForEachReadLegacySlot). */
	bool PlaceTakesWeapons(ELoadoutScreenPlace Place)
	{
		return Place == ELoadoutScreenPlace::Primary || Place == ELoadoutScreenPlace::Secondary;
	}

	bool PlaceTakesAdapters(ELoadoutScreenPlace Place, ECharacterClass Class)
	{
		// RefillActivePrimary: a Resonator's adapters are its adapter loadout, never the primary.
		return Place == ELoadoutScreenPlace::Adapter || (Place == ELoadoutScreenPlace::Primary && Class != ECharacterClass::Resonator);
	}

	/** Inside FCombatLoadout::EditSlots: the primary place emptied, and the evolution seat with it; also a weapon or
	 *  adapter hidden under the primary's other type, which EditSlots would otherwise keep (as the old clear dropped it). */
	void ClearPrimary(FCombatLoadout &Loadout)
	{
		Loadout.ClearSlotAt(WornGearLayout::PRIMARY_INDEX);
		Loadout.PrimaryWeapon.Clear();
		Loadout.PrimaryRing.Clear();
		Loadout.PrimaryEvolution = FEvolutionAttachment();
		Loadout.PrimaryEvolutionInstance = FGuid();
		Loadout.EvolutionSpells.Reset();
		Loadout.PrimarySlotType = EPrimarySlotType::None;
	}

	/** The owned pieces the class's fight reads from Loadout. */
	TSet<FGuid> GearInLoadout(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FCombatLoadout Mirror = Loadout;
		Mirror.BuildSlotsFromOldFields(Class);
		TSet<FGuid> InLoadout;
		for (const FLoadoutSlot &Slot : Mirror.Slots)
		{
			if (Slot.GearID.IsValid())
			{
				InLoadout.Add(Slot.GearID);
			}
		}
		return InLoadout;
	}
}

FLoadoutScreenRow WoRLoadoutScreen::BuildGearRow(const FLoadoutSlot &Slot)
{
	return Slot.Gear.Asset ? LoadoutScreenBuild::MakeGearRow(Slot) : FLoadoutScreenRow();
}

TArray<FLoadoutScreenMember> WoRLoadoutScreen::BuildMembers(const UWorld *World)
{
	TArray<FLoadoutScreenMember> Members;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party)
	{
		return Members;
	}
	for (const FPartyMember &PartyMember : Party->Members)
	{
		if (const UCharacterData *Data = PartyMember.CharacterData)
		{
			Members.Add({UPoolSubsystem::GetCharacterKey(PartyMember), FText::FromString(Data->Name), Data->CharacterClass});
		}
	}
	return Members;
}

FLoadoutScreenPage WoRLoadoutScreen::BuildGearPage(const FCombatLoadout &Loadout, ECharacterClass Class)
{
	using namespace LoadoutScreenBuild;

	// The slots the fight reads, never stale (as FLoadoutRules::NonAdapterCost).
	FCombatLoadout Mirror = Loadout;
	Mirror.BuildSlotsFromOldFields(Class);

	FLoadoutScreenPage Page;
	Page.Budget = LoadoutClassTable::Get(Class).Budget;

	// The seat counts whenever the slot is an evolution's, item or not, as the budget check charges it (NonAdapterCost).
	if (Mirror.IsEvolutionLoadout())
	{
		FLoadoutScreenRow Seat;
		Seat.Kind = ELoadoutScreenRowKind::EvolutionSeat;
		Seat.Name = Mirror.PrimaryEvolution.Item ? FText::FromString(Mirror.PrimaryEvolution.Item->ItemName) : LOCTEXT("EvolutionMissing", "Evolution (missing)");
		Seat.KindLine = LOCTEXT("KindEvolution", "Evolution");
		Seat.Tier = WoRGameNames::GetTierLetter(Mirror.PrimaryEvolution.Tier);
		Seat.Cost = LoadoutConstants::EVOLUTION_ALONE_SLOT_COST;
		Seat.CostLine = FText::Format(LOCTEXT("CostEvolutionAlone", "Evolution alone {0}"), FText::AsNumber(Seat.Cost));
		Seat.Place = ELoadoutScreenPlace::Primary;
		AddDetail(Seat, LOCTEXT("DetailSpells", "Spells"), JoinSkillNames(TArray<TObjectPtr<USpellData>>(Mirror.EvolutionSpells)));
		Page.Rows.Add(MoveTemp(Seat));
	}

	int32 WeaponCount = 0;
	int32 AdapterCount = 0;
	for (const FLoadoutSlot &Slot : Mirror.Slots)
	{
		WeaponCount += !Slot.IsEmpty() && Slot.Kind() == EGearKind::Weapon ? 1 : 0;
		AdapterCount += !Slot.IsEmpty() && Slot.Kind() == EGearKind::Adapter ? 1 : 0;
	}
	for (int32 Index = 0; Index < Mirror.Slots.Num(); ++Index)
	{
		const FLoadoutSlot &Slot = Mirror.Slots[Index];
		if (Slot.IsEmpty())
		{
			continue;
		}
		FLoadoutScreenRow Row = MakeGearRow(Slot);
		Row.Place = PlaceOf(Slot.Place);
		Row.AdapterIndex = Row.Place == ELoadoutScreenPlace::Adapter ? Slot.Place - WornGearLayout::FIRST_ADAPTER_INDEX : INDEX_NONE;
		Row.GearID = Slot.GearID;
		if (Row.Kind == ELoadoutScreenRowKind::Weapon && WeaponCount > 1)
		{
			Row.bActive = Index == Mirror.ActiveWeaponSlot;
			Row.Note = LOCTEXT("NoteTwoWeapons", "Only the weapon in hand gives its abilities. Switching in a fight is free.");
		}
		else if (Row.Kind == ELoadoutScreenRowKind::Adapter && AdapterCount > 1)
		{
			Row.bActive = Index == Mirror.ActiveAdapterSlot;
		}
		Page.Rows.Add(MoveTemp(Row));
	}
	for (const FLoadoutScreenRow &Row : Page.Rows)
	{
		Page.Spent += Row.Cost;
	}

	// B-440b: the free places A can fill.
	const bool bHasPrimary = Mirror.IsEvolutionLoadout() || Page.Rows.ContainsByPredicate([](const FLoadoutScreenRow &Row)
																						   { return Row.Place == ELoadoutScreenPlace::Primary; });
	if (!bHasPrimary)
	{
		Page.Rows.Add(MakeEmptyGearRow(LOCTEXT("EmptyPrimary", "Empty slot"), ELoadoutScreenPlace::Primary));
	}
	if (Class == ECharacterClass::Generic && !Page.Rows.ContainsByPredicate([](const FLoadoutScreenRow &Row) { return Row.Place == ELoadoutScreenPlace::Secondary; }))
	{
		Page.Rows.Add(MakeEmptyGearRow(LOCTEXT("EmptyWeapon", "Empty weapon"), ELoadoutScreenPlace::Secondary));
	}
	if (Class == ECharacterClass::Resonator)
	{
		Page.Rows.Add(MakeEmptyGearRow(LOCTEXT("EmptyAdapter", "Empty adapter"), ELoadoutScreenPlace::Adapter));
	}

	// Apparel: its own slots, outside the budget (D-164).
	const FText OutsideBudget = LOCTEXT("OutsideBudget", "Outside budget");
	Page.FirstApparelRow = Page.Rows.Num();
	int32 ApparelShown = 0;
	for (const FApparelEntry &Entry : Mirror.Apparel)
	{
		if (!Entry.Apparel || ApparelShown >= ApparelRules::MAX_APPAREL_SLOTS)
		{
			continue;
		}
		Page.Rows.Add(BuildApparelRow(Entry));
		++ApparelShown;
	}
	for (; ApparelShown < ApparelRules::MAX_APPAREL_SLOTS; ++ApparelShown)
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::EmptyApparel;
		Row.Name = LOCTEXT("EmptyApparel", "Empty apparel");
		Row.bCountsToBudget = false;
		Row.CostLine = OutsideBudget;
		Page.Rows.Add(MoveTemp(Row));
	}
	return Page;
}

FLoadoutScreenRow WoRLoadoutScreen::BuildApparelRow(const FApparelEntry &Entry)
{
	using namespace LoadoutScreenBuild;

	FLoadoutScreenRow Row;
	Row.Kind = ELoadoutScreenRowKind::Apparel;
	Row.Name = Entry.Apparel ? FText::FromString(Entry.Apparel->GetDisplayName()) : FText::GetEmpty();
	Row.KindLine = LOCTEXT("KindApparel", "Apparel");
	Row.Tier = WoRGameNames::GetTierLetter(Entry.Tier);
	Row.GearID = Entry.PersistentID; // B-440f: the owned piece, so a change or take-off finds it
	Row.Sockets.Add(MakeSocket(Entry.AttachedItem, LOCTEXT("SocketCrystal", "Crystal")));
	Row.bCountsToBudget = false;
	Row.CostLine = LOCTEXT("OutsideBudget", "Outside budget");
	const ESpellElement Element = Entry.GetElement();
	AddDetail(Row, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
	AddDetail(Row, LOCTEXT("DetailBonusLine", "Bonus"), ApparelBonusLine::GetText(Entry.BonusLine)); // B-475 (D-208)
	return Row;
}

FLoadoutScreenPage WoRLoadoutScreen::BuildGearPage(UWorld *World, const FLoadoutScreenMember &Member)
{
	const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member.Key);
	if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		FLoadoutScreenPage Page;
		Page.Refusal = FText::Format(LOCTEXT("MemberNotHere", "{0} isn't here."), Member.Name);
		return Page;
	}
	const FCombatLoadout &Loadout = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	FLoadoutScreenPage Page = BuildGearPage(Loadout, Member.Class);
	Page.LoadoutTitle = FormatLoadoutTitle(Loadout.LoadoutName, Inventory->ActiveLoadoutIndex + 1, Inventory->SavedLoadouts.Num());
	return Page;
}

TArray<FLoadoutScreenChoice> WoRLoadoutScreen::BuildGearChoices(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row,
																 const TArray<FWeaponInventoryEntry> &OwnedWeapons,
																 const TArray<FRingInventoryEntry> &OwnedAdapters, const TSet<FGuid> &WornElsewhere)
{
	using namespace LoadoutScreenBuild;

	TArray<FLoadoutScreenChoice> Choices;
	if (Row.Place == ELoadoutScreenPlace::None)
	{
		return Choices;
	}
	const FLoadoutScreenPage Before = BuildGearPage(Loadout, Class);
	const auto Finish = [&](FLoadoutScreenChoice &Choice, const FGuid &GearID)
	{
		FCombatLoadout After = Loadout;
		PutGearChoice(After, Class, Row, Choice);
		Choice.NewSpent = BuildGearPage(After, Class).Spent;
		if (WornElsewhere.Contains(GearID))
		{
			Choice.Refusal = LOCTEXT("RefuseWornElsewhere", "Another member wears it.");
		}
		else if (Choice.NewSpent > Before.Budget && Choice.NewSpent > Before.Spent)
		{
			Choice.Refusal = FText::Format(LOCTEXT("RefuseOverBudget", "Over budget: {0} / {1}."), FText::AsNumber(Choice.NewSpent),
										   FText::AsNumber(Before.Budget));
		}
		Choices.Add(MoveTemp(Choice));
	};

	const int32 WeaponRows = Algo::CountIf(Before.Rows, [](const FLoadoutScreenRow &Each) { return Each.Kind == ELoadoutScreenRowKind::Weapon; });
	if (Row.Kind == ELoadoutScreenRowKind::Weapon && !Row.bActive && WeaponRows > 1)
	{
		FLoadoutScreenChoice InHand;
		InHand.bMakeActive = true;
		InHand.Row = Row;
		InHand.Row.Name = FText::Format(LOCTEXT("ChoiceInHand", "Put {0} in hand"), Row.Name);
		InHand.Row.bActive = false;
		Finish(InHand, FGuid());
	}

	const TSet<FGuid> InLoadout = GearInLoadout(Loadout, Class);
	if (PlaceTakesWeapons(Row.Place))
	{
		for (const FWeaponInventoryEntry &Owned : OwnedWeapons)
		{
			if (Owned.Weapon && Owned.PersistentID.IsValid() && !InLoadout.Contains(Owned.PersistentID))
			{
				FLoadoutScreenChoice Choice;
				Choice.Weapon.WeaponEntry = Owned;
				Choice.Weapon.InitializeFromWeapon();
				Choice.Row = BuildGearRow(FLoadoutSlot::FromWeaponLoadoutEntry(Choice.Weapon));
				Finish(Choice, Owned.PersistentID);
			}
		}
	}
	if (PlaceTakesAdapters(Row.Place, Class))
	{
		for (const FRingInventoryEntry &Owned : OwnedAdapters)
		{
			if (Owned.Ring && Owned.PersistentID.IsValid() && !InLoadout.Contains(Owned.PersistentID))
			{
				FLoadoutScreenChoice Choice;
				Choice.Adapter.RingEntry = Owned;
				Choice.Adapter.InitializeFromRing();
				Choice.Row = BuildGearRow(FLoadoutSlot::FromRingLoadoutEntry(Choice.Adapter));
				Finish(Choice, Owned.PersistentID);
			}
		}
	}
	return Choices;
}

bool WoRLoadoutScreen::PutGearChoice(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row, const FLoadoutScreenChoice &Choice)
{
	using namespace LoadoutScreenBuild;

	if (Choice.bMakeActive)
	{
		if (!PlaceTakesWeapons(Row.Place))
		{
			return false;
		}
		Loadout.bShowPrimary = Row.Place == ELoadoutScreenPlace::Primary; // a Generic's weapon in hand (ToggleEquipment)
		return true;
	}
	const bool bWeapon = Choice.Weapon.IsValid();
	if (!bWeapon && !Choice.Adapter.IsValid())
	{
		return false;
	}
	const FLoadoutSlot Piece = bWeapon ? FLoadoutSlot::FromWeaponLoadoutEntry(Choice.Weapon) : FLoadoutSlot::FromRingLoadoutEntry(Choice.Adapter);
	int32 Place = INDEX_NONE;
	switch (Row.Place)
	{
	case ELoadoutScreenPlace::Primary:
		Place = WornGearLayout::PRIMARY_INDEX;
		break;
	case ELoadoutScreenPlace::Secondary:
		Place = bWeapon ? WornGearLayout::SECONDARY_INDEX : INDEX_NONE;
		break;
	case ELoadoutScreenPlace::Adapter:
		// The row's own place, or a new one after the last adapter entry (as the old list appended).
		Place = bWeapon ? INDEX_NONE : WornGearLayout::FIRST_ADAPTER_INDEX + (Loadout.RingLoadout.IsValidIndex(Row.AdapterIndex) ? Row.AdapterIndex : Loadout.RingLoadout.Num());
		break;
	default:
		break;
	}
	if (Place == INDEX_NONE)
	{
		return false;
	}
	Loadout.EditSlots(Class, [&Piece, Place](FCombatLoadout &Edited)
	{
		if (Place == WornGearLayout::PRIMARY_INDEX)
		{
			ClearPrimary(Edited); // the evolution seat goes with what was there
			Edited.PrimarySlotType = Piece.Gear.IsWeapon() ? EPrimarySlotType::Weapon : EPrimarySlotType::Ring;
		}
		Edited.PutSlotAt(Place, Piece);
	});
	return true;
}

bool WoRLoadoutScreen::TakeOffGear(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row, FText &OutRefusal)
{
	using namespace LoadoutScreenBuild;

	OutRefusal = FText::GetEmpty();
	const bool bNothing = Row.Kind == ELoadoutScreenRowKind::EmptyGear || Row.Place == ELoadoutScreenPlace::None;
	if (bNothing || (Row.Place != ELoadoutScreenPlace::Primary && !UInventoryComponent::StripFromLoadout(Loadout, Row.GearID)))
	{
		OutRefusal = LOCTEXT("RefuseNothingToTakeOff", "Nothing to take off here.");
		return false;
	}
	if (Row.Place != ELoadoutScreenPlace::Primary)
	{
		return true;
	}
	// D-116: the primary is never left empty; a Generic's second weapon moves up into it.
	if (Class == ECharacterClass::Generic && Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
	{
		Loadout.EditSlots(Class, [](FCombatLoadout &Edited)
		{
			const FLoadoutSlot *Found = Edited.FindSlotAt(WornGearLayout::SECONDARY_INDEX); // the mirror holds a Generic's valid second weapon
			if (!ensure(Found))
			{
				return;
			}
			const FLoadoutSlot Second = *Found;
			ClearPrimary(Edited);
			Edited.ClearSlotAt(WornGearLayout::SECONDARY_INDEX);
			Edited.SecondaryWeapon.Clear(); // its type goes to None below, so EditSlots would keep it as hidden
			Edited.PutSlotAt(WornGearLayout::PRIMARY_INDEX, Second);
			Edited.PrimarySlotType = EPrimarySlotType::Weapon;
			Edited.SecondarySlotType = ESecondarySlotType::None;
			Edited.bShowPrimary = true;
		});
		return true;
	}
	OutRefusal = LOCTEXT("RefuseKeepPrimary", "Every loadout keeps a weapon or adapter here. Change it instead.");
	return false;
}

ULoadoutComponent *WoRLoadoutScreen::FindMemberLoadout(UWorld *World, const FLoadoutScreenMember &Member, UInventoryComponent *&OutInventory)
{
	OutInventory = WoRGive::FindMemberInventory(World, Member.Key);
	ULoadoutComponent *Loadout = OutInventory && OutInventory->GetOwner() ? OutInventory->GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
	return Loadout && OutInventory->SavedLoadouts.IsValidIndex(OutInventory->ActiveLoadoutIndex) ? Loadout : nullptr;
}

namespace LoadoutScreenBuild
{
	using WoRLoadoutScreen::FindMemberLoadout;

	FText MemberNotHere(const FLoadoutScreenMember &Member)
	{
		return FText::Format(LOCTEXT("MemberNotHereChange", "{0} isn't here."), Member.Name);
	}
}

TArray<FLoadoutScreenChoice> WoRLoadoutScreen::BuildGearChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row)
{
	UInventoryComponent *Inventory = nullptr;
	if (!LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory))
	{
		return {};
	}
	TSet<FGuid> WornElsewhere;
	for (const FWeaponInventoryEntry &Owned : Inventory->Weapons)
	{
		if (Inventory->IsWornByOtherMember(Owned.PersistentID))
		{
			WornElsewhere.Add(Owned.PersistentID);
		}
	}
	for (const FRingInventoryEntry &Owned : Inventory->Rings)
	{
		if (Inventory->IsWornByOtherMember(Owned.PersistentID))
		{
			WornElsewhere.Add(Owned.PersistentID);
		}
	}
	return BuildGearChoices(Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex], Member.Class, Row, Inventory->Weapons, Inventory->Rings, WornElsewhere);
}

FText WoRLoadoutScreen::ApplyGearChoice(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, const FLoadoutScreenChoice &Choice)
{
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory);
	if (!Loadout)
	{
		return LoadoutScreenBuild::MemberNotHere(Member);
	}
	if (!Choice.Refusal.IsEmpty())
	{
		return Choice.Refusal;
	}
	FCombatLoadout Probe = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	if (!PutGearChoice(Probe, Member.Class, Row, Choice))
	{
		return LOCTEXT("RefusePlace", "It can't go in this place.");
	}
	FText Refusal;
	Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Row, &Choice, &Member](FCombatLoadout &Changed) { PutGearChoice(Changed, Member.Class, Row, Choice); }, Refusal);
	return Refusal;
}

FText WoRLoadoutScreen::SetRowActive(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row)
{
	if (Row.Kind != ELoadoutScreenRowKind::Weapon || !LoadoutScreenBuild::PlaceTakesWeapons(Row.Place))
	{
		return LOCTEXT("SetActiveNotWeapon", "Only a weapon entry can be set active.");
	}
	if (Row.bActive)
	{
		return LOCTEXT("SetActiveAlready", "That weapon is already in hand.");
	}
	FLoadoutScreenChoice Choice;
	Choice.bMakeActive = true;
	Choice.Row = Row;
	return ApplyGearChoice(World, Member, Row, Choice);
}

FText WoRLoadoutScreen::UnequipRow(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row)
{
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory);
	if (!Loadout)
	{
		return LoadoutScreenBuild::MemberNotHere(Member);
	}
	FText Refusal;
	FCombatLoadout Probe = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	if (!TakeOffGear(Probe, Member.Class, Row, Refusal))
	{
		return Refusal;
	}
	Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Row, &Member](FCombatLoadout &Changed)
						   {
							   FText Ignored;
							   TakeOffGear(Changed, Member.Class, Row, Ignored);
						   },
						   Refusal);
	return Refusal;
}

void WoRLoadoutScreen::FindRowGear(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, FWeaponLoadoutEntry *&OutWeapon, FRingLoadoutEntry *&OutAdapter)
{
	OutWeapon = nullptr;
	OutAdapter = nullptr;
	switch (Row.Place)
	{
	case ELoadoutScreenPlace::Primary:
		OutWeapon = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None ? &Loadout.PrimaryWeapon : nullptr;
		OutAdapter = Loadout.PrimarySlotType == EPrimarySlotType::Ring ? &Loadout.PrimaryRing : nullptr;
		break;
	case ELoadoutScreenPlace::Secondary:
		OutWeapon = &Loadout.SecondaryWeapon;
		break;
	case ELoadoutScreenPlace::Adapter:
		OutAdapter = Loadout.RingLoadout.IsValidIndex(Row.AdapterIndex) ? &Loadout.RingLoadout[Row.AdapterIndex] : nullptr;
		break;
	default:
		break;
	}
}

bool WoRLoadoutScreen::EditRowGear(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row,
									TFunctionRef<bool(FWeaponLoadoutEntry *, FRingLoadoutEntry *)> Edit)
{
	int32 Place = INDEX_NONE;
	switch (Row.Place)
	{
	case ELoadoutScreenPlace::Primary:
		Place = WornGearLayout::PRIMARY_INDEX;
		break;
	case ELoadoutScreenPlace::Secondary:
		Place = WornGearLayout::SECONDARY_INDEX;
		break;
	case ELoadoutScreenPlace::Adapter:
		Place = Row.AdapterIndex == INDEX_NONE ? INDEX_NONE : WornGearLayout::FIRST_ADAPTER_INDEX + Row.AdapterIndex;
		break;
	default:
		return false;
	}
	bool bChanged = false;
	Loadout.EditSlots(Class, [&](FCombatLoadout &Edited)
	{
		const FLoadoutSlot *Slot = Place == INDEX_NONE ? nullptr : Edited.FindSlotAt(Place);
		if (!Slot)
		{
			FWeaponLoadoutEntry *Weapon = nullptr;
			FRingLoadoutEntry *Adapter = nullptr;
			FindRowGear(Edited, Row, Weapon, Adapter);
			bChanged = (Weapon || Adapter) && Edit(Weapon, Adapter);
			return;
		}
		if (Slot->Kind() == EGearKind::Weapon)
		{
			FWeaponLoadoutEntry Entry = Slot->ToWeaponLoadoutEntry();
			bChanged = Edit(&Entry, nullptr);
			if (bChanged)
			{
				Edited.PutSlotAt(Place, FLoadoutSlot::FromWeaponLoadoutEntry(Entry));
			}
			return;
		}
		FRingLoadoutEntry Entry = Slot->ToRingLoadoutEntry();
		bChanged = Edit(nullptr, &Entry);
		if (bChanged)
		{
			Edited.PutSlotAt(Place, FLoadoutSlot::FromRingLoadoutEntry(Entry));
		}
	});
	return bChanged;
}

namespace LoadoutScreenBuild
{
	/** Socket in the piece's entry, or null. */
	FRuntimeAttachedItem *FindSocketItem(FWeaponLoadoutEntry *WeaponSlot, FRingLoadoutEntry *RingSlot, int32 Socket)
	{
		FWeaponInventoryEntry *Weapon = WeaponSlot ? &WeaponSlot->WeaponEntry : nullptr;
		FRingInventoryEntry *Ring = RingSlot ? &RingSlot->RingEntry : nullptr;
		if (Socket == LoadoutScreenSockets::CRYSTAL)
		{
			return Weapon ? &Weapon->AttachedItem : Ring ? &Ring->AttachedItem : nullptr;
		}
		return Socket == LoadoutScreenSockets::STONE ? (Weapon ? &Weapon->StoneSocket : Ring ? &Ring->StoneSocket : nullptr) : nullptr;
	}

	bool HasSocket(const FLoadoutScreenRow &Row, int32 Socket)
	{
		return (Row.Kind == ELoadoutScreenRowKind::Weapon || Row.Kind == ELoadoutScreenRowKind::Adapter) && Row.GearID.IsValid() && Row.Sockets.IsValidIndex(Socket);
	}
}

TArray<FLoadoutScreenChoice> WoRLoadoutScreen::BuildSocketChoices(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row, int32 Socket,
																   const TMap<FCrystalId, int32> &Carried, const TArray<FEvolutionInventoryEntry> &Evolutions,
																   const TSet<FGuid> &EvolutionsInUse)
{
	using namespace LoadoutScreenBuild;

	TArray<FLoadoutScreenChoice> Choices;
	if (!HasSocket(Row, Socket) || !Row.Sockets[Socket].bEmpty)
	{
		return Choices;
	}
	const FLoadoutScreenPage Before = BuildGearPage(Loadout, Class);
	const int32 STierBefore = FLoadoutRules::CountEquippedSTierCrystals(Loadout, Class);
	const auto Finish = [&](FLoadoutScreenChoice &Choice, bool bInUse)
	{
		FCombatLoadout After = Loadout;
		PutSocketChoice(After, Class, Row, Socket, Choice);
		Choice.NewSpent = BuildGearPage(After, Class).Spent;
		Choice.Row.bCountsToBudget = false; // the line shows what it adds, not a cost of its own
		Choice.Row.CostLine = FText::Format(LOCTEXT("SocketAdds", "+{0}"), FText::AsNumber(Choice.NewSpent - Before.Spent));
		const int32 STierAfter = FLoadoutRules::CountEquippedSTierCrystals(After, Class);
		if (bInUse)
		{
			Choice.Refusal = LOCTEXT("RefuseEvolutionInUse", "Already in use.");
		}
		else if (!GearSockets::AcceptsCrystal(Row.AcceptedCrystal, Choice.Crystal.Type)) // D-231 point 5 (B-540)
		{
			Choice.Refusal = FText::Format(LOCTEXT("RefuseCrystalType", "This piece takes only {0}."),
										   WoRGameNames::GetCrystalTypeName(Row.AcceptedCrystal));
		}
		else if (STierAfter > LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS && STierAfter > STierBefore)
		{
			Choice.Refusal = LOCTEXT("RefuseSecondSTier", "S-tier: one per loadout.");
		}
		else if (Choice.NewSpent > Before.Budget && Choice.NewSpent > Before.Spent)
		{
			Choice.Refusal = FText::Format(LOCTEXT("RefuseOverBudget", "Over budget: {0} / {1}."), FText::AsNumber(Choice.NewSpent),
										   FText::AsNumber(Before.Budget));
		}
		Choices.Add(MoveTemp(Choice));
	};

	// Carried gems (the crystal socket) or stones (the stone socket), highest tier first. B-480 (D-210): two stones may
	// share an entry, so the crystal socket offers stones too once the stone socket holds one (UInventoryComponent puts
	// a stone there only then).
	const bool bWantStone = Socket == LoadoutScreenSockets::STONE;
	const bool bStoneSocketFull = !bWantStone && Row.Sockets.IsValidIndex(LoadoutScreenSockets::STONE) && !Row.Sockets[LoadoutScreenSockets::STONE].bEmpty;
	TArray<TPair<FCrystalId, int32>> Stacks;
	for (const TPair<FCrystalId, int32> &Stack : Carried)
	{
		const bool bGem = CrystalTypeHelpers::IsGemType(Stack.Key.Type);
		const bool bFits = bWantStone ? !bGem : (bGem || bStoneSocketFull);
		if (Stack.Value > 0 && Stack.Key.Type != ECrystalType::None && bFits && !CrystalTypeHelpers::IsPotionType(Stack.Key.Type)) // a potion fits no socket
		{
			Stacks.Add(Stack);
		}
	}
	Stacks.Sort([](const TPair<FCrystalId, int32> &A, const TPair<FCrystalId, int32> &B)
				{ return A.Key.Tier != B.Key.Tier ? A.Key.Tier > B.Key.Tier : A.Key.Type < B.Key.Type; });
	for (const TPair<FCrystalId, int32> &Stack : Stacks)
	{
		FLoadoutScreenChoice Choice;
		const bool bStone = !CrystalTypeHelpers::IsGemType(Stack.Key.Type);
		Choice.Crystal = Stack.Key;
		Choice.Attachment.Kind = bStone ? EAttachedItemKind::AugmentStone : EAttachedItemKind::Crystal;
		Choice.Attachment.Crystal.Id = StatStone::Normalize(Stack.Key); // as UInventoryComponent attaches it
		Choice.Attachment.Crystal.CurrentDurability = bStone ? 0 : ItemIdentity::GetMaxDurability(Stack.Key);
		Choice.Row.Name = AttachmentName(Choice.Attachment);
		const ESpellElement Element = Choice.Attachment.GetElement();
		const FText Kind = bStone ? LOCTEXT("SocketStone", "Stone") : LOCTEXT("SocketCrystal", "Crystal");
		Choice.Row.KindLine = Element == ESpellElement::None ? Kind : FText::Format(LOCTEXT("KindAndType", "{0} · {1}"), Kind, WoRGameNames::GetElementName(Element));
		Choice.Row.Tier = WoRGameNames::GetTierLetter(Stack.Key.Tier);
		AddDetail(Choice.Row, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
		AddDetail(Choice.Row, FText::GetEmpty(), FormatDurability(Choice.Attachment)); // D-231: the number alone
		AddDetail(Choice.Row, LOCTEXT("DetailCarried", "Carried"), FText::AsNumber(Stack.Value));
		Finish(Choice, false);
	}

	// Owned evolutions go in the crystal socket (an evolution on gear, Q-357).
	if (!bWantStone)
	{
		for (const FEvolutionInventoryEntry &Owned : Evolutions)
		{
			if (!Owned.Item || !Owned.InstanceID.IsValid())
			{
				continue;
			}
			FLoadoutScreenChoice Choice;
			Choice.EvolutionID = Owned.InstanceID;
			Choice.Attachment.Kind = EAttachedItemKind::Evolution;
			Choice.Attachment.Evolution.Item = Owned.Item;
			Choice.Attachment.Evolution.Tier = Owned.Tier;
			Choice.Row.Name = FText::FromString(Owned.Item->ItemName);
			Choice.Row.KindLine = LOCTEXT("KindEvolution", "Evolution");
			Choice.Row.Tier = WoRGameNames::GetTierLetter(Owned.Tier);
			const ESpellElement Element = Choice.Attachment.GetElement();
			AddDetail(Choice.Row, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
			Finish(Choice, EvolutionsInUse.Contains(Owned.InstanceID));
		}
	}
	return Choices;
}

bool WoRLoadoutScreen::PutSocketChoice(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutScreenRow &Row, int32 Socket,
									   const FLoadoutScreenChoice &Choice)
{
	if (!LoadoutScreenBuild::HasSocket(Row, Socket) || Choice.Attachment.IsEmpty())
	{
		return false;
	}
	return EditRowGear(Loadout, Class, Row, [&](FWeaponLoadoutEntry *Weapon, FRingLoadoutEntry *Adapter)
	{
		FRuntimeAttachedItem *Item = LoadoutScreenBuild::FindSocketItem(Weapon, Adapter, Socket);
		if (Item)
		{
			*Item = Choice.Attachment;
		}
		return Item != nullptr;
	});
}

TArray<FLoadoutScreenChoice> WoRLoadoutScreen::BuildSocketChoices(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, int32 Socket,
																   const FCombatLoadout *Preview)
{
	UInventoryComponent *Inventory = nullptr;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Pool || !LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory))
	{
		return {};
	}
	const UEvolutionInventoryComponent *EvolutionInventory = Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>();
	const TArray<FEvolutionInventoryEntry> NoEvolutions;
	const TArray<FEvolutionInventoryEntry> &Evolutions = EvolutionInventory ? EvolutionInventory->Entries : NoEvolutions;
	TSet<FGuid> InUse;
	for (const FEvolutionInventoryEntry &Owned : Evolutions)
	{
		if (Inventory->IsEvolutionInUse(Owned.InstanceID))
		{
			InUse.Add(Owned.InstanceID);
		}
	}
	// AttachCrystalTo* spends from the pouch the member carries: a recruit carries the host's (B-361).
	return BuildSocketChoices(Preview ? *Preview : Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex], Member.Class, Row, Socket,
							  Pool->GetCarriedItems(WoRGive::GetPouchKey(World, Member.Key)),
							  Evolutions, InUse);
}

FText WoRLoadoutScreen::ApplySocketChoice(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, int32 Socket,
										 const FLoadoutScreenChoice &Choice)
{
	UInventoryComponent *Inventory = nullptr;
	if (!LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory))
	{
		return LoadoutScreenBuild::MemberNotHere(Member);
	}
	if (!Choice.Refusal.IsEmpty())
	{
		return Choice.Refusal;
	}
	if (!LoadoutScreenBuild::HasSocket(Row, Socket) || !Row.Sockets[Socket].bEmpty)
	{
		return LOCTEXT("RefuseSocketFull", "Take what's in this socket off first.");
	}
	const bool bAdapter = Row.Kind == ELoadoutScreenRowKind::Adapter;
	const bool bDone = Choice.EvolutionID.IsValid()
						   ? (bAdapter ? Inventory->AttachEvolutionToRing(Row.GearID, Choice.EvolutionID) : Inventory->AttachEvolutionToWeapon(Row.GearID, Choice.EvolutionID))
						   : (bAdapter ? Inventory->AttachCrystalToRing(Row.GearID, Choice.Crystal) : Inventory->AttachCrystalToWeapon(Row.GearID, Choice.Crystal));
	return bDone ? FText::GetEmpty() : LOCTEXT("RefuseSocketAttach", "Couldn't socket that.");
}

FText WoRLoadoutScreen::GetTakeOffSocketWarning(const FLoadoutScreenRow &Row, int32 Socket, FText &OutRefusal)
{
	OutRefusal = FText::GetEmpty();
	if (!LoadoutScreenBuild::HasSocket(Row, Socket) || Row.Sockets[Socket].bEmpty)
	{
		OutRefusal = LOCTEXT("RefuseSocketEmpty", "Nothing in this socket.");
		return FText::GetEmpty();
	}
	const bool bFirstSocket = Socket == LoadoutScreenSockets::CRYSTAL;
	if (bFirstSocket && Row.bCrystalFixed) // D-231 (B-532)
	{
		OutRefusal = LOCTEXT("RefuseFixedCrystal", "This prebuilt's crystal is built in: it can't be taken out.");
		return FText::GetEmpty();
	}
	const FLoadoutScreenSocket &Taken = Row.Sockets[Socket];
	if (bFirstSocket && Taken.bEvolution)
	{
		return FText::Format(LOCTEXT("WarnEvolutionOff", "X again: {0} goes back to your evolutions with some wear (it breaks at none)."), Taken.Name);
	}
	// CL-28b (B-533, D-231): only this socket empties; a socketed gem or stone is used up.
	return FText::Format(LOCTEXT("WarnSocketOff", "X again: {0} comes out and is destroyed. The other socket keeps its own."), Taken.Name);
}

FText WoRLoadoutScreen::TakeOffSocket(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, int32 Socket)
{
	UInventoryComponent *Inventory = nullptr;
	if (!LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory))
	{
		return LoadoutScreenBuild::MemberNotHere(Member);
	}
	FText Refusal;
	if (GetTakeOffSocketWarning(Row, Socket, Refusal).IsEmpty())
	{
		return Refusal;
	}
	const bool bAdapter = Row.Kind == ELoadoutScreenRowKind::Adapter;
	bool bDone = false;
	const bool bStone = Socket == LoadoutScreenSockets::STONE; // B-533: only the chosen socket
	if (!bStone && Row.Sockets[LoadoutScreenSockets::CRYSTAL].bEvolution)
	{
		bDone = bAdapter ? Inventory->RemoveEvolutionFromRing(Row.GearID) : Inventory->RemoveEvolutionFromWeapon(Row.GearID);
	}
	else if (bAdapter)
	{
		const int32 Index = Inventory->Rings.IndexOfByPredicate([&Row](const FRingInventoryEntry &Owned) { return Owned.PersistentID == Row.GearID; });
		bDone = Index != INDEX_NONE && (bStone ? Inventory->RemoveStoneFromRing(Index) : Inventory->RemoveCrystalFromRing(Index));
	}
	else
	{
		const int32 Index = Inventory->Weapons.IndexOfByPredicate([&Row](const FWeaponInventoryEntry &Owned) { return Owned.PersistentID == Row.GearID; });
		bDone = Index != INDEX_NONE && (bStone ? Inventory->RemoveStoneFromWeapon(Index) : Inventory->RemoveCrystalFromWeapon(Index));
	}
	return bDone ? FText::GetEmpty() : LOCTEXT("RefuseSocketRemove", "Couldn't take that off.");
}

namespace LoadoutScreenRestore
{
	/** B-534: the owned gem Restore would work on, and the member's crystal pouch; false when either is missing. */
	bool FindGem(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, UInventoryComponent *&OutInventory,
				 const FRuntimeAttachedItem *&OutGem, const UCrystalInventoryComponent *&OutPouch)
	{
		OutGem = nullptr;
		OutPouch = nullptr;
		if (!LoadoutScreenBuild::FindMemberLoadout(World, Member, OutInventory) || !OutInventory)
		{
			return false;
		}
		if (const FWeaponInventoryEntry *Weapon = OutInventory->FindOwnedWeapon(Row.GearID))
		{
			OutGem = &Weapon->AttachedItem;
		}
		else if (const FRingInventoryEntry *Ring = OutInventory->FindOwnedRing(Row.GearID))
		{
			OutGem = &Ring->AttachedItem;
		}
		OutPouch = OutInventory->GetOwner() ? OutInventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
		return OutGem && OutPouch;
	}

	/** "2 Garnet (F), 1 Garnet (E)": Uses grouped by tier, cheapest first (they arrive in that order). */
	FText DescribeUses(const TArray<FCrystalId> &Uses)
	{
		TArray<FText> Parts;
		for (int32 Index = 0; Index < Uses.Num();)
		{
			int32 Count = 0;
			const FCrystalId Id = Uses[Index];
			for (; Index < Uses.Num() && Uses[Index] == Id; ++Index)
			{
				++Count;
			}
			Parts.Add(FText::Format(LOCTEXT("RestoreUse", "{0} {1} ({2})"), FText::AsNumber(Count), WoRGameNames::GetCrystalTypeName(Id.Type),
									WoRGameNames::GetTierLetter(Id.Tier)));
		}
		return FText::Join(LOCTEXT("RestoreUseJoin", ", "), Parts);
	}
}

bool WoRLoadoutScreen::IsRestoreOffered(const FLoadoutScreenRow &Row, int32 Socket)
{
	return Socket == LoadoutScreenSockets::CRYSTAL && LoadoutScreenBuild::HasSocket(Row, Socket) && Row.Sockets[Socket].bWorn &&
		   (Row.bCrystalFixed || !Row.Sockets[Socket].bBroken);
}

FText WoRLoadoutScreen::DescribeRestore(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row, FText &OutRefusal)
{
	OutRefusal = FText::GetEmpty();
	UInventoryComponent *Inventory = nullptr;
	const FRuntimeAttachedItem *Gem = nullptr;
	const UCrystalInventoryComponent *Pouch = nullptr;
	if (!IsRestoreOffered(Row, LoadoutScreenSockets::CRYSTAL) || !LoadoutScreenRestore::FindGem(World, Member, Row, Inventory, Gem, Pouch))
	{
		OutRefusal = LOCTEXT("RefuseRestoreNothing", "Nothing here to restore.");
		return FText::GetEmpty();
	}
	const GearRepair::FRestorePlan Plan = GearRepair::PlanRestore(*Gem, *Pouch);
	if (Plan.Uses.IsEmpty())
	{
		OutRefusal = FText::Format(LOCTEXT("RefuseRestoreNoCrystals", "Restoring it takes {0} crystals; you have none."),
								   WoRGameNames::GetCrystalTypeName(Gem->Crystal.Id.Type));
		return FText::GetEmpty();
	}
	FNumberFormattingOptions WholePercent;
	WholePercent.SetMaximumFractionalDigits(0).SetRoundingMode(ERoundingMode::ToZero);
	const float Share = Plan.MaxDurability > 0 ? static_cast<float>(Plan.NewDurability) / Plan.MaxDurability : 0.0f;
	return FText::Format(LOCTEXT("RestorePlan", "{0} back to {1} for {2}"), Row.Sockets[LoadoutScreenSockets::CRYSTAL].Name,
						 FText::AsPercent(Share, &WholePercent), LoadoutScreenRestore::DescribeUses(Plan.Uses));
}

FText WoRLoadoutScreen::RestoreSocket(UWorld *World, const FLoadoutScreenMember &Member, const FLoadoutScreenRow &Row)
{
	FText Refusal;
	if (DescribeRestore(World, Member, Row, Refusal).IsEmpty())
	{
		return Refusal;
	}
	UInventoryComponent *Inventory = nullptr;
	LoadoutScreenBuild::FindMemberLoadout(World, Member, Inventory);
	return Inventory && Inventory->RestoreCrystal(Row.GearID) ? FText::GetEmpty() : LOCTEXT("RefuseRestore", "Couldn't restore that.");
}

FText WoRLoadoutScreen::FormatLoadoutTitle(const FString &LoadoutName, int32 Index, int32 Count)
{
	return FText::Format(LOCTEXT("LoadoutTitle", "{0} ({1} of {2})"), FText::FromString(LoadoutName), FText::AsNumber(Index), FText::AsNumber(Count));
}

FString WoRLoadoutScreen::GetLoadoutScreenString(UWorld *World)
{
	FString Out = TEXT("[LoadoutScreen]");
	for (const FLoadoutScreenMember &Member : BuildMembers(World))
	{
		const FLoadoutScreenPage Page = BuildGearPage(World, Member);
		Out += FString::Printf(TEXT("\n  %s: %s, budget %d of %d%s"), *Member.Name.ToString(), *Page.LoadoutTitle.ToString(), Page.Spent, Page.Budget,
							   Page.Refusal.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Page.Refusal.ToString()));
		for (const FLoadoutScreenRow &Row : Page.Rows)
		{
			FString Sockets;
			for (const FLoadoutScreenSocket &Socket : Row.Sockets)
			{
				Sockets += FString::Printf(TEXT(" [%s: %s]"), *Socket.Label.ToString(), Socket.bEmpty ? TEXT("empty") : *Socket.Name.ToString());
			}
			Out += FString::Printf(TEXT("\n    %s (%s) %s%s%s | %s"), *Row.Name.ToString(), *Row.Tier.ToString(), *Row.KindLine.ToString(),
								   Row.bActive ? TEXT(" ACTIVE") : TEXT(""), *Sockets, *Row.CostLine.ToString());
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadoutScreenPrintCommand(
	TEXT("wor.UI.PrintLoadout"),
	TEXT("DEBUG (B-440a): what the loadout screen's gear page would show for each party member."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRLoadoutScreen::GetLoadoutScreenString(World));
	}));

static FAutoConsoleCommandWithWorldAndArgs GLoadoutScreenPrintChoicesCommand(
	TEXT("wor.UI.PrintGearChoices"),
	TEXT("DEBUG (B-440b): wor.UI.PrintGearChoices <member> <row> - what the gear picker offers for that row (0-based), with the budget after each and any refusal."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const TArray<FLoadoutScreenMember> Members = WoRLoadoutScreen::BuildMembers(World);
		const int32 MemberIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const int32 RowIndex = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;
		if (!Members.IsValidIndex(MemberIndex))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutScreen] No member %d"), MemberIndex);
			return;
		}
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(World, Members[MemberIndex]);
		if (!Page.Rows.IsValidIndex(RowIndex))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutScreen] %s has no row %d"), *Members[MemberIndex].Name.ToString(), RowIndex);
			return;
		}
		FString Out = FString::Printf(TEXT("[LoadoutScreen] %s, row %d (%s), budget %d of %d:"), *Members[MemberIndex].Name.ToString(), RowIndex,
									  *Page.Rows[RowIndex].Name.ToString(), Page.Spent, Page.Budget);
		for (const FLoadoutScreenChoice &Choice : WoRLoadoutScreen::BuildGearChoices(World, Members[MemberIndex], Page.Rows[RowIndex]))
		{
			Out += FString::Printf(TEXT("\n  %s (%s) -> %d%s"), *Choice.Row.Name.ToString(), *Choice.Row.KindLine.ToString(), Choice.NewSpent,
								   Choice.Refusal.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" REFUSED: %s"), *Choice.Refusal.ToString()));
		}
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *Out);
	}));
#endif

#undef LOCTEXT_NAMESPACE
