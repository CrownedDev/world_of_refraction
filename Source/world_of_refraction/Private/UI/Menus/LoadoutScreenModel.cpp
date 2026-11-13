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
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FApparelEntry.h"
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

	if (Mirror.IsEvolutionLoadout() && Mirror.PrimaryEvolution.Item)
	{
		FLoadoutScreenRow Seat;
		Seat.Kind = ELoadoutScreenRowKind::EvolutionSeat;
		Seat.Name = FText::FromString(Mirror.PrimaryEvolution.Item->ItemName);
		Seat.KindLine = LOCTEXT("KindEvolution", "Evolution");
		Seat.Tier = WoRGameNames::GetTierLetter(Mirror.PrimaryEvolution.Tier);
		Seat.Cost = LoadoutConstants::EVOLUTION_ALONE_SLOT_COST;
		Seat.CostLine = FText::Format(LOCTEXT("CostEvolutionAlone", "Evolution alone {0}"), FText::AsNumber(Seat.Cost));
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
#endif

#undef LOCTEXT_NAMESPACE
