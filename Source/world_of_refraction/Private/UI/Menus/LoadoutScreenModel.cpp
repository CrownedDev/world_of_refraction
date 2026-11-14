// LoadoutScreenModel.cpp

#include "UI/Menus/LoadoutScreenModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/FRuntimeAttachedItem.h"
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

	/** "Weapon 1 + Crystal 1 = 2": the frame, plus what sits in it (Q-357). */
	FText FormatCostLine(const FLoadoutSlot &Slot, int32 Cost)
	{
		const bool bWeapon = Slot.Kind() == EGearKind::Weapon;
		const FText FrameName = bWeapon ? LOCTEXT("FrameWeapon", "Weapon") : LOCTEXT("FrameAdapter", "Adapter");
		const int32 FrameCost = FMath::Min(Cost, bWeapon ? LoadoutConstants::WEAPON_FRAME_COST : LoadoutConstants::ADAPTER_FRAME_COST);
		const int32 AttachmentCost = Cost - FrameCost;
		if (AttachmentCost <= 0 || !Slot.Gear.HasAttachment())
		{
			return FText::Format(LOCTEXT("CostFrameOnly", "{0} {1}"), FrameName, FText::AsNumber(Cost));
		}
		return FText::Format(LOCTEXT("CostFrameAndAttachment", "{0} {1} + {2} {3} = {4}"), FrameName, FText::AsNumber(FrameCost),
							 SocketLabel(Slot.Gear.AttachedItem, FText::GetEmpty()), FText::AsNumber(AttachmentCost), FText::AsNumber(Cost));
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
		AddDetail(Row, LOCTEXT("DetailDurability", "Durability"), FormatDurability(Gear.AttachedItem));
		return Row;
	}

	ELoadoutScreenPlace PlaceOf(ELegacySlotOrigin Origin)
	{
		switch (Origin)
		{
		case ELegacySlotOrigin::PrimaryWeapon:
		case ELegacySlotOrigin::PrimaryRing:
			return ELoadoutScreenPlace::Primary;
		case ELegacySlotOrigin::SecondaryWeapon:
			return ELoadoutScreenPlace::Secondary;
		case ELegacySlotOrigin::RingLoadout:
			return ELoadoutScreenPlace::Adapter;
		default:
			return ELoadoutScreenPlace::None;
		}
	}

	FLoadoutScreenRow MakeEmptyGearRow(const FText &Name, ELoadoutScreenPlace Place)
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::EmptyGear;
		Row.Name = Name;
		Row.Place = Place;
		Row.bCountsToBudget = false; // nothing in it costs nothing; no number shown
		Row.Note = LOCTEXT("NoteEmptyGear", "Change to put a piece you own here.");
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

	void ClearPrimary(FCombatLoadout &Loadout)
	{
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
		Mirror.RebuildSlotsFromLegacy(Class);
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
	Mirror.RebuildSlotsFromLegacy(Class);

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
		Row.Place = PlaceOf(Slot.LegacyOrigin);
		Row.AdapterIndex = Slot.LegacyOrigin == ELegacySlotOrigin::RingLoadout ? Slot.LegacyIndex : INDEX_NONE;
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
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::Apparel;
		Row.Name = FText::FromString(Entry.Apparel->GetDisplayName());
		Row.KindLine = LOCTEXT("KindApparel", "Apparel");
		Row.Tier = WoRGameNames::GetTierLetter(Entry.Tier);
		Row.Sockets.Add(MakeSocket(Entry.AttachedItem, LOCTEXT("SocketCrystal", "Crystal")));
		Row.bCountsToBudget = false;
		Row.CostLine = OutsideBudget;
		const ESpellElement Element = Entry.GetElement();
		AddDetail(Row, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
		Page.Rows.Add(MoveTemp(Row));
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

FLoadoutScreenPage WoRLoadoutScreen::BuildGearPage(UWorld *World, const FLoadoutScreenMember &Member)
{
	const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member.Key);
	if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		FLoadoutScreenPage Page;
		Page.Refusal = FText::Format(LOCTEXT("MemberNotHere", "{0} isn't here, so their loadout can't be shown."), Member.Name);
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
		PutGearChoice(After, Row, Choice);
		Choice.NewSpent = BuildGearPage(After, Class).Spent;
		if (WornElsewhere.Contains(GearID))
		{
			Choice.Refusal = LOCTEXT("RefuseWornElsewhere", "Another member wears it.");
		}
		else if (Choice.NewSpent > Before.Budget && Choice.NewSpent > Before.Spent)
		{
			Choice.Refusal = FText::Format(LOCTEXT("RefuseOverBudget", "Over the budget: {0} of {1}."), FText::AsNumber(Choice.NewSpent),
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

bool WoRLoadoutScreen::PutGearChoice(FCombatLoadout &Loadout, const FLoadoutScreenRow &Row, const FLoadoutScreenChoice &Choice)
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
	switch (Row.Place)
	{
	case ELoadoutScreenPlace::Primary:
		ClearPrimary(Loadout);
		Loadout.PrimarySlotType = bWeapon ? EPrimarySlotType::Weapon : EPrimarySlotType::Ring;
		if (bWeapon)
		{
			Loadout.PrimaryWeapon = Choice.Weapon;
		}
		else
		{
			Loadout.PrimaryRing = Choice.Adapter;
		}
		return true;
	case ELoadoutScreenPlace::Secondary:
		if (!bWeapon)
		{
			return false;
		}
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = Choice.Weapon;
		return true;
	case ELoadoutScreenPlace::Adapter:
		if (bWeapon)
		{
			return false;
		}
		if (Loadout.RingLoadout.IsValidIndex(Row.AdapterIndex))
		{
			Loadout.RingLoadout[Row.AdapterIndex] = Choice.Adapter;
		}
		else
		{
			Loadout.RingLoadout.Add(Choice.Adapter);
		}
		return true;
	default:
		return false;
	}
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
		ClearPrimary(Loadout);
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Loadout.SecondaryWeapon;
		Loadout.SecondaryWeapon.Clear();
		Loadout.SecondarySlotType = ESecondarySlotType::None;
		Loadout.bShowPrimary = true;
		return true;
	}
	OutRefusal = LOCTEXT("RefuseKeepPrimary", "Every loadout keeps a weapon or adapter here. Change it instead.");
	return false;
}

namespace LoadoutScreenBuild
{
	/** The member's spawned loadout component, with its inventory; null when the member isn't in this level. */
	ULoadoutComponent *FindMemberLoadout(UWorld *World, const FLoadoutScreenMember &Member, UInventoryComponent *&OutInventory)
	{
		OutInventory = WoRGive::FindMemberInventory(World, Member.Key);
		ULoadoutComponent *Loadout = OutInventory && OutInventory->GetOwner() ? OutInventory->GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
		return Loadout && OutInventory->SavedLoadouts.IsValidIndex(OutInventory->ActiveLoadoutIndex) ? Loadout : nullptr;
	}

	FText MemberNotHere(const FLoadoutScreenMember &Member)
	{
		return FText::Format(LOCTEXT("MemberNotHereChange", "{0} isn't here, so their gear can't be changed."), Member.Name);
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
	if (!PutGearChoice(Probe, Row, Choice))
	{
		return LOCTEXT("RefusePlace", "It can't go in this place.");
	}
	FText Refusal;
	Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Row, &Choice](FCombatLoadout &Changed) { PutGearChoice(Changed, Row, Choice); }, Refusal);
	return Refusal;
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
