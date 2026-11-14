// LoadoutV2Model.cpp

#include "UI/Menus/LoadoutV2Model.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WeaponSetRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2"

namespace LoadoutV2Build
{
	ELoadoutV2SocketIcon IconOf(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEvolution())
		{
			return ELoadoutV2SocketIcon::Evolution;
		}
		if (Item.IsAugmentStone())
		{
			return ELoadoutV2SocketIcon::Stone;
		}
		return Item.IsEmpty() ? ELoadoutV2SocketIcon::Empty : ELoadoutV2SocketIcon::Crystal;
	}

	FText SetName(const FWeaponLoadoutEntry &Set)
	{
		const FText Main = FText::FromString(Set.WeaponEntry.Weapon ? Set.WeaponEntry.Weapon->GetDisplayName() : FString());
		if (!Set.HasOffHand())
		{
			return Main;
		}
		return FText::Format(LOCTEXT("PairName", "{0} + {1}"), Main, FText::FromString(Set.OffHandEntry.Weapon->GetDisplayName()));
	}

	FLoadoutV2Entry MakeWeaponEntry(const FWeaponLoadoutEntry &Set, ELoadoutV2Place Place)
	{
		FLoadoutV2Entry Entry;
		Entry.Kind = ELoadoutV2EntryKind::Weapon;
		Entry.Place = Place;
		Entry.Name = SetName(Set);
		Entry.Sockets = {IconOf(Set.WeaponEntry.AttachedItem), IconOf(Set.WeaponEntry.StoneSocket)};
		Entry.SkillSlots = Set.GetSkillSlotCount();
		return Entry;
	}

	FLoadoutV2Entry MakeAdapterEntry(const FRingLoadoutEntry &Ring, ELoadoutV2Place Place, int32 AdapterIndex)
	{
		FLoadoutV2Entry Entry;
		Entry.Kind = ELoadoutV2EntryKind::Adapter;
		Entry.Place = Place;
		Entry.AdapterIndex = AdapterIndex;
		Entry.Name = FText::FromString(Ring.RingEntry.Ring ? Ring.RingEntry.Ring->GetDisplayName() : FString());
		Entry.Sockets = {IconOf(Ring.RingEntry.AttachedItem), IconOf(Ring.RingEntry.StoneSocket)};
		return Entry;
	}

	/** The cells the loadout uses for the class: the gear the class fights with, and the character's own evolution. */
	int32 CellsUsed(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FCombatLoadout Mirror = Loadout;
		Mirror.RebuildSlotsFromLegacy(Class);
		int32 Cells = Mirror.IsEvolutionLoadout() ? LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST : 0;
		for (const FLoadoutSlot &Slot : Mirror.Slots)
		{
			Cells += FLoadoutRules::SlotCost(Slot);
		}
		return Cells;
	}

	bool PrimaryIsFree(const FCombatLoadout &Loadout)
	{
		return !Loadout.IsEvolutionLoadout() && !Loadout.PrimaryWeapon.IsValid() && !Loadout.PrimaryRing.IsValid();
	}

	int32 CountAdapters(const FCombatLoadout &Loadout)
	{
		int32 Count = Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid() ? 1 : 0;
		for (const FRingLoadoutEntry &Ring : Loadout.RingLoadout)
		{
			Count += Ring.IsValid() ? 1 : 0;
		}
		return Count;
	}
}

FLoadoutV2WeaponsPage WoRLoadoutV2::BuildWeaponsPage(const FCombatLoadout &Loadout, ECharacterClass Class)
{
	using namespace LoadoutV2Build;
	FLoadoutV2WeaponsPage Page;
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);

	if (Loadout.IsEvolutionLoadout())
	{
		FLoadoutV2Entry Seat;
		Seat.Kind = ELoadoutV2EntryKind::Evolution;
		Seat.Place = ELoadoutV2Place::Seat;
		Seat.Name = Loadout.PrimaryEvolution.Item ? FText::FromString(Loadout.PrimaryEvolution.Item->ItemName) : LOCTEXT("EvolutionMissing", "Evolution (missing)");
		Page.Entries.Add(MoveTemp(Seat));
	}
	int32 WeaponEntries = 0;
	const bool bWeaponPrimary = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None;
	if (bWeaponPrimary && Loadout.PrimaryWeapon.IsValid())
	{
		Page.Entries.Add(MakeWeaponEntry(Loadout.PrimaryWeapon, ELoadoutV2Place::Primary));
		++WeaponEntries;
	}
	else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
	{
		Page.Entries.Add(MakeAdapterEntry(Loadout.PrimaryRing, ELoadoutV2Place::Primary, INDEX_NONE));
	}
	if (Class == ECharacterClass::Generic && Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
	{
		Page.Entries.Add(MakeWeaponEntry(Loadout.SecondaryWeapon, ELoadoutV2Place::Secondary));
		++WeaponEntries;
	}
	if (Class != ECharacterClass::Caster)
	{
		for (int32 Index = 0; Index < Loadout.RingLoadout.Num(); ++Index)
		{
			if (Loadout.RingLoadout[Index].IsValid())
			{
				Page.Entries.Add(MakeAdapterEntry(Loadout.RingLoadout[Index], ELoadoutV2Place::Adapter, Index));
			}
		}
	}
	// The weapon in hand, marked only when there are two weapon entries (as today's screen).
	if (WeaponEntries > 1)
	{
		const ELoadoutV2Place InHand = Loadout.bShowPrimary ? ELoadoutV2Place::Primary : ELoadoutV2Place::Secondary;
		for (FLoadoutV2Entry &Entry : Page.Entries)
		{
			Entry.bActive = Entry.Kind == ELoadoutV2EntryKind::Weapon && Entry.Place == InHand;
		}
	}

	Page.CellsUsed = CellsUsed(Loadout, Class);
	Page.CellsBudget = Limits.Budget;
	Page.CellsLine = FText::Format(LOCTEXT("CellsLine", "Cells used {0} of {1}"), FText::AsNumber(Page.CellsUsed), FText::AsNumber(Page.CellsBudget));
	Page.AddFrameLine = FText::Format(LOCTEXT("AddFrameLine", "{0}/{1} left"), FText::AsNumber(FMath::Max(0, Page.CellsBudget - Page.CellsUsed)),
									  FText::AsNumber(Page.CellsBudget));
	const int32 Adapters = CountAdapters(Loadout);
	const bool bWeaponRoom = WeaponEntries < Limits.MaxWeapons;
	const bool bAdapterRoom = Limits.MaxAdapters == LoadoutClassTable::NO_LIMIT || Adapters < Limits.MaxAdapters;
	const int32 Items = WeaponEntries + Adapters + (Loadout.IsEvolutionLoadout() ? 1 : 0);
	const bool bItemRoom = Limits.MaxItems == LoadoutClassTable::NO_LIMIT || Items < Limits.MaxItems;
	Page.bCanAdd = bItemRoom && (bWeaponRoom || bAdapterRoom);
	return Page;
}

bool WoRLoadoutV2::ApplyDraft(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, FText &OutRefusal)
{
	using namespace LoadoutV2Build;
	OutRefusal = FText::GetEmpty();
	switch (Draft.Kind)
	{
	case ELoadoutV2DraftKind::Weapon:
	{
		if (!Draft.MainHand.IsValid())
		{
			OutRefusal = LOCTEXT("RefuseNoMainHand", "Pick a main hand.");
			return false;
		}
		FWeaponLoadoutEntry Set;
		Set.WeaponEntry = Draft.MainHand;
		Set.WeaponEntry.AttachedItem = Draft.Socket1; // the entry's sockets are the main hand's (D-210)
		Set.WeaponEntry.StoneSocket = Draft.Socket2;
		Set.OffHandEntry = Draft.OffHand;
		if (PrimaryIsFree(Loadout))
		{
			Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
			Loadout.PrimaryWeapon = Set;
			return true;
		}
		if (Class == ECharacterClass::Generic && !Loadout.SecondaryWeapon.IsValid())
		{
			Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
			Loadout.SecondaryWeapon = Set;
			return true;
		}
		OutRefusal = LOCTEXT("RefuseNoWeaponEntry", "No free weapon entry.");
		return false;
	}
	case ELoadoutV2DraftKind::Adapter:
	{
		if (!Draft.Adapter.IsValid())
		{
			OutRefusal = LOCTEXT("RefuseNoAdapter", "Pick an adapter.");
			return false;
		}
		FRingLoadoutEntry Ring;
		Ring.RingEntry = Draft.Adapter;
		Ring.RingEntry.AttachedItem = Draft.Socket1;
		Ring.RingEntry.StoneSocket = Draft.Socket2;
		if (Class == ECharacterClass::Caster)
		{
			if (!PrimaryIsFree(Loadout))
			{
				OutRefusal = LOCTEXT("RefuseRefractorOneEntry", "A Refractor carries one entry.");
				return false;
			}
			Loadout.PrimarySlotType = EPrimarySlotType::Ring;
			Loadout.PrimaryRing = Ring;
			return true;
		}
		Loadout.RingLoadout.Add(Ring); // a Resonator's adapters, a Generic's adapter entry (D-210)
		return true;
	}
	case ELoadoutV2DraftKind::Evolution:
	default:
	{
		if (!Draft.Socket1.IsEvolution() || !Draft.Socket1.Evolution.Item)
		{
			OutRefusal = LOCTEXT("RefuseNoEvolution", "Pick an evolution.");
			return false;
		}
		if (!PrimaryIsFree(Loadout))
		{
			// TODO: B-476b: the character's own evolution beside a Generic's two weapons needs its own place (D-210);
			// today it takes the primary slot.
			OutRefusal = LOCTEXT("RefuseEvolutionNeedsPrimary", "Free the first entry for the evolution.");
			return false;
		}
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
		Loadout.PrimaryEvolution = Draft.Socket1.Evolution;
		return true;
	}
	}
}

FText WoRLoadoutV2::CheckDraft(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, int32 &OutCellsAfter)
{
	using namespace LoadoutV2Build;
	OutCellsAfter = CellsUsed(Loadout, Class);
	const bool bGear = Draft.Kind != ELoadoutV2DraftKind::Evolution;
	if (bGear && !Draft.Socket2.IsEmpty() && !Draft.Socket2.IsAugmentStone())
	{
		return LOCTEXT("RefuseSocket2Stone", "The second socket takes a stone.");
	}
	if (Draft.Kind == ELoadoutV2DraftKind::Weapon)
	{
		if (Draft.OffHand.IsValid())
		{
			if (Class != ECharacterClass::Generic)
			{
				return LOCTEXT("RefuseOnlyGenericPairs", "Only a Generic can dual wield.");
			}
			const WeaponSetRules::EPairRefusal Pair = WeaponSetRules::CheckPair(Draft.MainHand, Draft.OffHand);
			if (Pair != WeaponSetRules::EPairRefusal::None)
			{
				return WeaponSetRules::GetPairRefusalText(Pair);
			}
		}
		else if (Draft.MainHand.Weapon && !Draft.MainHand.Weapon->CanBeHeldAlone())
		{
			return LOCTEXT("RefuseTwinOnly", "Twin only: needs two, one item here.");
		}
		if (Draft.Socket1.IsEvolution() && Class == ECharacterClass::Resonator)
		{
			return LOCTEXT("RefuseResonatorEvolution", "Resonators put evolutions on adapters only.");
		}
	}
	if (Draft.Kind == ELoadoutV2DraftKind::Adapter && Draft.Socket1.IsEvolution() && Class == ECharacterClass::Generic)
	{
		return LOCTEXT("RefuseGenericEvolution", "Generics put evolutions on weapons only.");
	}
	FCombatLoadout After = Loadout;
	FText Refusal;
	if (!ApplyDraft(After, Class, Draft, Refusal))
	{
		return Refusal;
	}
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
	if (Draft.Kind == ELoadoutV2DraftKind::Adapter && Limits.MaxAdapters != LoadoutClassTable::NO_LIMIT && CountAdapters(After) > Limits.MaxAdapters)
	{
		return LOCTEXT("RefuseNoAdapterEntry", "No free adapter entry.");
	}
	OutCellsAfter = CellsUsed(After, Class);
	if (OutCellsAfter > Limits.Budget)
	{
		return FText::Format(LOCTEXT("RefuseOverCells", "Over the cells: {0} of {1}."), FText::AsNumber(OutCellsAfter), FText::AsNumber(Limits.Budget));
	}
	return FText::GetEmpty();
}

TArray<FLoadoutV2OffHandChoice> WoRLoadoutV2::BuildOffHandChoices(const FWeaponInventoryEntry &MainHand, const TArray<FWeaponInventoryEntry> &OwnedWeapons,
																 ECharacterClass Class)
{
	TArray<FLoadoutV2OffHandChoice> Choices;
	for (const FWeaponInventoryEntry &Owned : OwnedWeapons)
	{
		if (!Owned.Weapon || (Owned.PersistentID.IsValid() && Owned.PersistentID == MainHand.PersistentID))
		{
			continue;
		}
		FLoadoutV2OffHandChoice &Choice = Choices.AddDefaulted_GetRef();
		Choice.Weapon = Owned;
		if (Class != ECharacterClass::Generic)
		{
			Choice.Refusal = LOCTEXT("RefuseOnlyGenericPairs", "Only a Generic can dual wield.");
			continue;
		}
		const WeaponSetRules::EPairRefusal Pair = WeaponSetRules::CheckPair(MainHand, Owned);
		if (Pair == WeaponSetRules::EPairRefusal::DifferentTypes && MainHand.Weapon)
		{
			// UIScreens: "Pairs only with a sword".
			Choice.Refusal = FText::Format(LOCTEXT("RefusePairsOnlyWith", "Pairs only with a {0}."), WoRGameNames::GetWeaponTypeName(MainHand.Weapon->WeaponType));
		}
		else if (Pair != WeaponSetRules::EPairRefusal::None)
		{
			Choice.Refusal = WeaponSetRules::GetPairRefusalText(Pair);
		}
	}
	return Choices;
}

FLoadoutV2Compare WoRLoadoutV2::BuildCompare(const FWeaponInventoryEntry &Equipped, const FWeaponInventoryEntry &Selected)
{
	FLoadoutV2Compare Compare;
	Compare.EquippedPower = Equipped.Weapon ? Equipped.Weapon->Power : 0;
	Compare.SelectedPower = Selected.Weapon ? Selected.Weapon->Power : 0;
	Compare.CellsChange = 0; // D-206: a weapon itself is free; the sockets stay with the entry
	const TArray<UAbilityData *> Before = Equipped.Weapon ? Equipped.Weapon->PresetAbilities : TArray<UAbilityData *>();
	const TArray<UAbilityData *> After = Selected.Weapon ? Selected.Weapon->PresetAbilities : TArray<UAbilityData *>();
	for (const UAbilityData *Ability : After)
	{
		if (Ability && !Before.Contains(Ability))
		{
			Compare.BuiltInsGained.Add(FText::FromString(Ability->Name));
		}
	}
	for (const UAbilityData *Ability : Before)
	{
		if (Ability && !After.Contains(Ability))
		{
			Compare.BuiltInsLost.Add(FText::FromString(Ability->Name));
		}
	}
	return Compare;
}

FString WoRLoadoutV2::GetWeaponsPageString(const FLoadoutV2WeaponsPage &Page)
{
	FString Out = FString::Printf(TEXT("%s; Add weapon %s%s"), *Page.CellsLine.ToString(), *Page.AddFrameLine.ToString(), Page.bCanAdd ? TEXT("") : TEXT(" (full)"));
	for (const FLoadoutV2Entry &Entry : Page.Entries)
	{
		Out += FString::Printf(TEXT("\n  %s%s, %d socket(s), %d skill slot(s)"), *Entry.Name.ToString(), Entry.bActive ? TEXT(" [ACTIVE]") : TEXT(""),
							   Entry.Sockets.Num(), Entry.SkillSlots);
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
