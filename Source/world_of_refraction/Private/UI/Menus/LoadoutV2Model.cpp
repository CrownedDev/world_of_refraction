// LoadoutV2Model.cpp

#include "UI/Menus/LoadoutV2Model.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WeaponSetRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/EffectText.h"
#include "UI/Menus/LoadoutScreenModel.h"
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

namespace LoadoutV2Build
{
	/** Spaces per effect-tree depth in GetDetailsString. */
	constexpr int32 DEBUG_INDENT_SPACES = 2;

	int32 CountWeaponEntries(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		const bool bWeaponPrimary = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None;
		int32 Count = bWeaponPrimary && Loadout.PrimaryWeapon.IsValid() ? 1 : 0;
		Count += Class == ECharacterClass::Generic && Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid() ? 1 : 0;
		return Count;
	}

	bool HoldsPiece(const FCombatLoadout &Loadout, const FGuid &ID)
	{
		if (!ID.IsValid())
		{
			return false;
		}
		if (Loadout.PrimaryWeapon.WeaponEntry.PersistentID == ID || Loadout.SecondaryWeapon.WeaponEntry.PersistentID == ID ||
			Loadout.PrimaryWeapon.OffHandEntry.PersistentID == ID || Loadout.SecondaryWeapon.OffHandEntry.PersistentID == ID ||
			Loadout.PrimaryRing.RingEntry.PersistentID == ID)
		{
			return true;
		}
		return Loadout.RingLoadout.ContainsByPredicate([&ID](const FRingLoadoutEntry &Ring) { return Ring.RingEntry.PersistentID == ID; });
	}

	FRuntimeAttachedItem EvolutionAttachment(const FEvolutionInventoryEntry &Owned)
	{
		return WoRLoadoutV2::MakeEvolutionAttachment(Owned);
	}

	FText AttachmentName(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEvolution())
		{
			return Item.Evolution.Item ? FText::FromString(Item.Evolution.Item->ItemName) : FText::GetEmpty();
		}
		return Item.IsEmpty() ? FText::GetEmpty() : WoRGameNames::GetCrystalTypeName(StatStone::AsRowId(Item.Crystal.Id).Type);
	}

	FText SocketWord(const FRuntimeAttachedItem &Item, int32 SocketNumber)
	{
		if (Item.IsEvolution())
		{
			return LOCTEXT("SocketWordEvolution", "Evolution");
		}
		if (Item.IsAugmentStone())
		{
			return LOCTEXT("SocketWordStone", "Stone");
		}
		if (!Item.IsEmpty())
		{
			return LOCTEXT("SocketWordCrystal", "Crystal");
		}
		return FText::Format(LOCTEXT("SocketWordEmpty", "Augment socket {0}"), FText::AsNumber(SocketNumber));
	}

	/** "Fire · B" for a socketed piece, its element when it has one. */
	FText AttachmentLine(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEmpty())
		{
			return LOCTEXT("SocketEmptyLine", "Empty");
		}
		const EItemTier Tier = Item.IsEvolution() ? Item.Evolution.Tier : StatStone::AsRowId(Item.Crystal.Id).Tier;
		const ESpellElement Element = Item.GetElement();
		const FText Name = FText::Format(LOCTEXT("AttachmentNameTier", "{0}  {1}"), AttachmentName(Item), WoRGameNames::GetTierLetter(Tier));
		return Element == ESpellElement::None ? Name : FText::Format(LOCTEXT("AttachmentWithElement", "{0} · {1}"), Name, WoRGameNames::GetElementName(Element));
	}

	FText ScalingLine(const TArray<FPillarScaling> &Scaling)
	{
		TArray<FText> Parts;
		for (const FPillarScaling &Each : Scaling)
		{
			Parts.Add(FText::Format(LOCTEXT("ScalingPart", "{0} {1}"), WoRGameNames::GetPillarName(Each.Pillar),
									StaticEnum<EScalingTier>()->GetDisplayNameTextByValue(static_cast<int64>(Each.Tier))));
		}
		return FText::Join(LOCTEXT("ScalingSeparator", " · "), Parts);
	}

	void AddLine(TArray<FLoadoutV2DetailLine> &Lines, ELoadoutV2DetailKind Kind, const FText &Label, const FText &Value, int32 Depth = 0)
	{
		if (Kind == ELoadoutV2DetailKind::Stat && Value.IsEmpty())
		{
			return; // a stat with nothing to say isn't shown
		}
		FLoadoutV2DetailLine &Line = Lines.AddDefaulted_GetRef();
		Line.Kind = Kind;
		Line.Depth = Depth;
		Line.Label = Label;
		Line.Value = Value;
	}

	/** A gear piece's effects: the ET-50 tree, or its short forms in the simple view. */
	void AddEffects(TArray<FLoadoutV2DetailLine> &Lines, const UEquipmentDataBase *Piece, bool bSimple)
	{
		if (!Piece)
		{
			return;
		}
		TArray<FSkillEffect> Effects = Piece->GetStartingEffects();
		Effects.Append(Piece->GetConditionalEffects());
		if (Effects.IsEmpty())
		{
			return;
		}
		if (bSimple)
		{
			for (const FText &Short : EffectText::BuildShortForm(Effects, EEffectTextSource::Gear))
			{
				AddLine(Lines, ELoadoutV2DetailKind::Effect, FText::GetEmpty(), Short);
			}
			return;
		}
		for (const FEffectTextLine &Line : EffectText::BuildTree(Effects, EEffectTextSource::Gear))
		{
			AddLine(Lines, ELoadoutV2DetailKind::Effect, FText::GetEmpty(), Line.Text, Line.Depth);
		}
	}

	void AddWeaponPiece(TArray<FLoadoutV2DetailLine> &Lines, const FWeaponInventoryEntry &Owned, bool bSimple)
	{
		const UWeaponData *Weapon = Owned.Weapon;
		if (!Weapon)
		{
			return;
		}
		AddLine(Lines, ELoadoutV2DetailKind::Piece, FText::FromString(Weapon->GetDisplayName()), WoRGameNames::GetTierLetter(Weapon->Tier));
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailType", "Type"), WoRGameNames::GetWeaponTypeName(Weapon->WeaponType));
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailPower", "Power"), Weapon->Power > 0 ? FText::AsNumber(Weapon->Power) : FText::GetEmpty());
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailScaling", "Scaling"), ScalingLine(Weapon->StatScaling));
		AddEffects(Lines, Weapon, bSimple);
	}

	void AddSockets(TArray<FLoadoutV2DetailLine> &Lines, const FRuntimeAttachedItem &Socket1, const FRuntimeAttachedItem &Socket2)
	{
		AddLine(Lines, ELoadoutV2DetailKind::Socket, SocketWord(Socket1, 1), AttachmentLine(Socket1));
		AddLine(Lines, ELoadoutV2DetailKind::Socket, SocketWord(Socket2, 2), AttachmentLine(Socket2));
	}

	const FWeaponLoadoutEntry *FindWeaponSet(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry)
	{
		if (Entry.Kind != ELoadoutV2EntryKind::Weapon)
		{
			return nullptr;
		}
		return Entry.Place == ELoadoutV2Place::Primary ? &Loadout.PrimaryWeapon : Entry.Place == ELoadoutV2Place::Secondary ? &Loadout.SecondaryWeapon : nullptr;
	}

	const FRingLoadoutEntry *FindAdapter(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry)
	{
		if (Entry.Kind != ELoadoutV2EntryKind::Adapter)
		{
			return nullptr;
		}
		if (Entry.Place == ELoadoutV2Place::Primary)
		{
			return &Loadout.PrimaryRing;
		}
		return Loadout.RingLoadout.IsValidIndex(Entry.AdapterIndex) ? &Loadout.RingLoadout[Entry.AdapterIndex] : nullptr;
	}

	FWeaponLoadoutEntry *FindWeaponSet(FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry)
	{
		return const_cast<FWeaponLoadoutEntry *>(FindWeaponSet(static_cast<const FCombatLoadout &>(Loadout), Entry));
	}

	FRingLoadoutEntry *FindAdapter(FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry)
	{
		return const_cast<FRingLoadoutEntry *>(FindAdapter(static_cast<const FCombatLoadout &>(Loadout), Entry));
	}

	/** Today's screen's row for Entry (its place and piece), so its rules (TakeOffGear) act on it. */
	FLoadoutScreenRow ScreenRowOf(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry)
	{
		FLoadoutScreenRow Row;
		switch (Entry.Kind)
		{
		case ELoadoutV2EntryKind::Evolution:
			Row.Kind = ELoadoutScreenRowKind::EvolutionSeat;
			Row.Place = ELoadoutScreenPlace::Primary;
			break;
		case ELoadoutV2EntryKind::Adapter:
			Row.Kind = ELoadoutScreenRowKind::Adapter;
			Row.Place = Entry.Place == ELoadoutV2Place::Primary ? ELoadoutScreenPlace::Primary : ELoadoutScreenPlace::Adapter;
			Row.AdapterIndex = Entry.AdapterIndex;
			if (const FRingLoadoutEntry *Ring = FindAdapter(Loadout, Entry))
			{
				Row.GearID = Ring->RingEntry.PersistentID;
			}
			break;
		case ELoadoutV2EntryKind::Weapon:
		default:
			Row.Kind = ELoadoutScreenRowKind::Weapon;
			Row.Place = Entry.Place == ELoadoutV2Place::Secondary ? ELoadoutScreenPlace::Secondary : ELoadoutScreenPlace::Primary;
			if (const FWeaponLoadoutEntry *Set = FindWeaponSet(Loadout, Entry))
			{
				Row.GearID = Set->WeaponEntry.PersistentID;
			}
			break;
		}
		return Row;
	}

	/** The checks CheckDraft makes on the draft's own pieces, before its place (the pair, a twin alone, socket 2, an
	 *  evolution where the class can't put one). Empty when they pass. */
	FText CheckDraftPieces(ECharacterClass Class, const FLoadoutV2Draft &Draft)
	{
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
		return FText::GetEmpty();
	}

	FText CheckCells(const FCombatLoadout &After, ECharacterClass Class, int32 &OutCellsAfter)
	{
		const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
		OutCellsAfter = CellsUsed(After, Class);
		if (OutCellsAfter > Limits.Budget)
		{
			return FText::Format(LOCTEXT("RefuseOverCells", "Over the cells: {0} of {1}."), FText::AsNumber(OutCellsAfter), FText::AsNumber(Limits.Budget));
		}
		return FText::GetEmpty();
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
		const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
		if (Limits.MaxAdapters != LoadoutClassTable::NO_LIMIT && CountAdapters(Loadout) >= Limits.MaxAdapters)
		{
			OutRefusal = LOCTEXT("RefuseNoAdapterEntry", "No free adapter entry.");
			return false;
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
		Loadout.PrimaryEvolutionInstance = Draft.EvolutionInstance; // never a stale id from an earlier evolution
		return true;
	}
	}
}

FText WoRLoadoutV2::CheckDraft(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Draft &Draft, int32 &OutCellsAfter)
{
	using namespace LoadoutV2Build;
	OutCellsAfter = CellsUsed(Loadout, Class);
	const FText PieceRefusal = CheckDraftPieces(Class, Draft);
	if (!PieceRefusal.IsEmpty())
	{
		return PieceRefusal;
	}
	FCombatLoadout After = Loadout;
	FText Refusal;
	if (!ApplyDraft(After, Class, Draft, Refusal))
	{
		return Refusal;
	}
	return CheckCells(After, Class, OutCellsAfter);
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

TArray<FLoadoutV2MainHandChoice> WoRLoadoutV2::BuildMainHandChoices(const FCombatLoadout &Loadout, ECharacterClass Class,
																	 const TArray<FWeaponInventoryEntry> &OwnedWeapons)
{
	const auto InLoadout = [&Loadout](const FGuid &ID)
	{
		return ID.IsValid() && (Loadout.PrimaryWeapon.WeaponEntry.PersistentID == ID || Loadout.SecondaryWeapon.WeaponEntry.PersistentID == ID ||
								Loadout.PrimaryWeapon.OffHandEntry.PersistentID == ID || Loadout.SecondaryWeapon.OffHandEntry.PersistentID == ID);
	};
	TArray<FLoadoutV2MainHandChoice> Choices;
	for (const FWeaponInventoryEntry &Owned : OwnedWeapons)
	{
		if (!Owned.Weapon || InLoadout(Owned.PersistentID))
		{
			continue;
		}
		FLoadoutV2MainHandChoice &Choice = Choices.AddDefaulted_GetRef();
		Choice.Weapon = Owned;
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Weapon;
		Draft.MainHand = Owned;
		Draft.Socket1 = Owned.AttachedItem; // the piece's own sockets come with it until the window edits them
		Draft.Socket2 = Owned.StoneSocket;
		Choice.Refusal = CheckDraft(Loadout, Class, Draft, Choice.CellsAfter);
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

TArray<FLoadoutV2KindChoice> WoRLoadoutV2::BuildAddKinds(const FCombatLoadout &Loadout, ECharacterClass Class)
{
	using namespace LoadoutV2Build;
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
	const FLoadoutV2WeaponsPage Page = BuildWeaponsPage(Loadout, Class);
	const FText Full = LOCTEXT("RefuseEntriesFull", "Every entry is taken.");
	TArray<FLoadoutV2KindChoice> Kinds;

	FLoadoutV2KindChoice &Weapon = Kinds.AddDefaulted_GetRef();
	Weapon.Kind = ELoadoutV2DraftKind::Weapon;
	Weapon.Name = LOCTEXT("KindWeapon", "Weapon");
	const bool bWeaponPlace = PrimaryIsFree(Loadout) || (Class == ECharacterClass::Generic && !Loadout.SecondaryWeapon.IsValid());
	if (!Page.bCanAdd)
	{
		Weapon.Refusal = Full;
	}
	else if (CountWeaponEntries(Loadout, Class) >= Limits.MaxWeapons || !bWeaponPlace)
	{
		Weapon.Refusal = LOCTEXT("RefuseNoWeaponEntry", "No free weapon entry.");
	}

	FLoadoutV2KindChoice &Adapter = Kinds.AddDefaulted_GetRef();
	Adapter.Kind = ELoadoutV2DraftKind::Adapter;
	Adapter.Name = WoRGameNames::GetAdapterWord();
	const bool bAdapterRoom = Class == ECharacterClass::Caster ? PrimaryIsFree(Loadout)
																: Limits.MaxAdapters == LoadoutClassTable::NO_LIMIT || CountAdapters(Loadout) < Limits.MaxAdapters;
	if (!Page.bCanAdd)
	{
		Adapter.Refusal = Full;
	}
	else if (!bAdapterRoom)
	{
		Adapter.Refusal = LOCTEXT("RefuseNoAdapterEntry", "No free adapter entry.");
	}

	FLoadoutV2KindChoice &Evolution = Kinds.AddDefaulted_GetRef();
	Evolution.Kind = ELoadoutV2DraftKind::Evolution;
	Evolution.Name = LOCTEXT("KindEvolution", "Evolution");
	if (Loadout.IsEvolutionLoadout())
	{
		Evolution.Refusal = LOCTEXT("RefuseOneEvolution", "One evolution per character: it already has one.");
	}
	else if (!PrimaryIsFree(Loadout))
	{
		// TODO: B-476: the character's own evolution beside gear needs its own place (D-210); today it takes the first entry.
		Evolution.Refusal = LOCTEXT("RefuseEvolutionNeedsPrimary", "Free the first entry for the evolution.");
	}
	return Kinds;
}

TArray<FLoadoutV2PieceChoice> WoRLoadoutV2::BuildAdapterChoices(const FCombatLoadout &Loadout, ECharacterClass Class, const TArray<FRingInventoryEntry> &OwnedAdapters)
{
	using namespace LoadoutV2Build;
	TArray<FLoadoutV2PieceChoice> Choices;
	for (const FRingInventoryEntry &Owned : OwnedAdapters)
	{
		if (!Owned.Ring || HoldsPiece(Loadout, Owned.PersistentID))
		{
			continue;
		}
		FLoadoutV2PieceChoice &Choice = Choices.AddDefaulted_GetRef();
		Choice.Kind = ELoadoutV2DraftKind::Adapter;
		Choice.Adapter = Owned;
		Choice.Name = FText::FromString(Owned.Ring->GetDisplayName());
		Choice.Line = WoRGameNames::GetAdapterFormName(Owned.Ring->Form);
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Adapter;
		Draft.Adapter = Owned;
		Draft.Socket1 = Owned.AttachedItem; // its own sockets come with it
		Draft.Socket2 = Owned.StoneSocket;
		Choice.Refusal = CheckDraft(Loadout, Class, Draft, Choice.CellsAfter);
	}
	return Choices;
}

TArray<FLoadoutV2PieceChoice> WoRLoadoutV2::BuildEvolutionChoices(const FCombatLoadout &Loadout, ECharacterClass Class, const TArray<FEvolutionInventoryEntry> &Evolutions,
																   const TSet<FGuid> &InUse)
{
	using namespace LoadoutV2Build;
	TArray<FLoadoutV2PieceChoice> Choices;
	for (const FEvolutionInventoryEntry &Owned : Evolutions)
	{
		if (!Owned.Item || !Owned.InstanceID.IsValid())
		{
			continue;
		}
		FLoadoutV2PieceChoice &Choice = Choices.AddDefaulted_GetRef();
		Choice.Kind = ELoadoutV2DraftKind::Evolution;
		Choice.Evolution = Owned;
		Choice.Name = FText::FromString(Owned.Item->ItemName);
		const FRuntimeAttachedItem Attachment = EvolutionAttachment(Owned);
		const ESpellElement Element = Attachment.GetElement();
		Choice.Line = FText::Format(LOCTEXT("EvolutionChoiceLine", "{0} · {1} cells"),
									Element == ESpellElement::None ? WoRGameNames::GetTierLetter(Owned.Tier) : WoRGameNames::GetElementName(Element),
									FText::AsNumber(LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST));
		if (InUse.Contains(Owned.InstanceID))
		{
			Choice.CellsAfter = CellsUsed(Loadout, Class);
			Choice.Refusal = LOCTEXT("RefuseEvolutionInUse", "It's in use: in a loadout or on other gear.");
			continue;
		}
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Evolution;
		Draft.Socket1 = Attachment;
		Draft.EvolutionInstance = Owned.InstanceID;
		Choice.Refusal = CheckDraft(Loadout, Class, Draft, Choice.CellsAfter);
	}
	return Choices;
}

FRuntimeAttachedItem WoRLoadoutV2::MakeEvolutionAttachment(const FEvolutionInventoryEntry &Owned)
{
	FRuntimeAttachedItem Item;
	Item.Kind = EAttachedItemKind::Evolution;
	Item.Evolution.Item = Owned.Item;
	Item.Evolution.Tier = Owned.Tier;
	Item.Evolution.InstanceID = Owned.InstanceID;
	Item.Evolution.CurrentDurability = Owned.CurrentDurability; // its wear and roll go with it
	Item.Evolution.GeneratedStatBonus = Owned.GeneratedStatBonus;
	Item.Evolution.GeneratedResistance = Owned.GeneratedResistance;
	return Item;
}

FLoadoutV2Draft WoRLoadoutV2::DraftFromEntry(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry)
{
	using namespace LoadoutV2Build;
	FLoadoutV2Draft Draft;
	switch (Entry.Kind)
	{
	case ELoadoutV2EntryKind::Evolution:
		Draft.Kind = ELoadoutV2DraftKind::Evolution;
		Draft.Socket1.Kind = EAttachedItemKind::Evolution;
		Draft.Socket1.Evolution = Loadout.PrimaryEvolution;
		Draft.EvolutionInstance = Loadout.PrimaryEvolutionInstance;
		break;
	case ELoadoutV2EntryKind::Adapter:
		Draft.Kind = ELoadoutV2DraftKind::Adapter;
		if (const FRingLoadoutEntry *Ring = FindAdapter(Loadout, Entry))
		{
			Draft.Adapter = Ring->RingEntry;
			Draft.Socket1 = Ring->RingEntry.AttachedItem;
			Draft.Socket2 = Ring->RingEntry.StoneSocket;
		}
		break;
	case ELoadoutV2EntryKind::Weapon:
	default:
		Draft.Kind = ELoadoutV2DraftKind::Weapon;
		if (const FWeaponLoadoutEntry *Set = FindWeaponSet(Loadout, Entry))
		{
			Draft.MainHand = Set->WeaponEntry;
			Draft.OffHand = Set->OffHandEntry;
			Draft.Socket1 = Set->WeaponEntry.AttachedItem;
			Draft.Socket2 = Set->WeaponEntry.StoneSocket;
		}
		break;
	}
	return Draft;
}

bool WoRLoadoutV2::ReplaceEntry(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry, const FLoadoutV2Draft &Draft, FText &OutRefusal)
{
	using namespace LoadoutV2Build;
	OutRefusal = FText::GetEmpty();
	const FText WrongPlace = LOCTEXT("RefuseWrongPlace", "It can't go in this place.");
	switch (Entry.Kind)
	{
	case ELoadoutV2EntryKind::Weapon:
	{
		FWeaponLoadoutEntry *Set = FindWeaponSet(Loadout, Entry);
		if (Draft.Kind != ELoadoutV2DraftKind::Weapon || !Set || !Set->IsValid())
		{
			OutRefusal = WrongPlace;
			return false;
		}
		if (!Draft.MainHand.IsValid())
		{
			OutRefusal = LOCTEXT("RefuseNoMainHand", "Pick a main hand.");
			return false;
		}
		const bool bSamePiece = Set->WeaponEntry.PersistentID == Draft.MainHand.PersistentID;
		Set->WeaponEntry = Draft.MainHand;
		Set->WeaponEntry.AttachedItem = Draft.Socket1; // the entry's sockets are the main hand's (D-210)
		Set->WeaponEntry.StoneSocket = Draft.Socket2;
		Set->OffHandEntry = Draft.OffHand;
		if (!bSamePiece)
		{
			Set->InitializeFromWeapon(); // a new main hand starts with no slotted picks of the old one
		}
		return true;
	}
	case ELoadoutV2EntryKind::Adapter:
	{
		FRingLoadoutEntry *Ring = FindAdapter(Loadout, Entry);
		if (Draft.Kind != ELoadoutV2DraftKind::Adapter || !Ring || !Ring->IsValid() || !Draft.Adapter.IsValid())
		{
			OutRefusal = WrongPlace;
			return false;
		}
		const bool bSamePiece = Ring->RingEntry.PersistentID == Draft.Adapter.PersistentID;
		Ring->RingEntry = Draft.Adapter;
		Ring->RingEntry.AttachedItem = Draft.Socket1;
		Ring->RingEntry.StoneSocket = Draft.Socket2;
		if (!bSamePiece)
		{
			Ring->InitializeFromRing();
		}
		return true;
	}
	case ELoadoutV2EntryKind::Evolution:
	default:
		if (Draft.Kind != ELoadoutV2DraftKind::Evolution || !Loadout.IsEvolutionLoadout() || !Draft.Socket1.IsEvolution() || !Draft.Socket1.Evolution.Item)
		{
			OutRefusal = WrongPlace;
			return false;
		}
		if (Loadout.PrimaryEvolutionInstance != Draft.EvolutionInstance)
		{
			Loadout.EvolutionSpells.Reset(); // the old evolution's picks don't carry over
		}
		Loadout.PrimaryEvolution = Draft.Socket1.Evolution;
		Loadout.PrimaryEvolutionInstance = Draft.EvolutionInstance;
		return true;
	}
}

FText WoRLoadoutV2::CheckEntryEdit(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry, const FLoadoutV2Draft &Draft,
								   int32 &OutCellsAfter)
{
	using namespace LoadoutV2Build;
	OutCellsAfter = CellsUsed(Loadout, Class);
	const FText PieceRefusal = CheckDraftPieces(Class, Draft);
	if (!PieceRefusal.IsEmpty())
	{
		return PieceRefusal;
	}
	FCombatLoadout After = Loadout;
	FText Refusal;
	if (!ReplaceEntry(After, Class, Entry, Draft, Refusal))
	{
		return Refusal;
	}
	return CheckCells(After, Class, OutCellsAfter);
}

bool WoRLoadoutV2::RemoveEntry(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry, FText &OutRefusal)
{
	return WoRLoadoutScreen::TakeOffGear(Loadout, Class, LoadoutV2Build::ScreenRowOf(Loadout, Entry), OutRefusal);
}

int32 WoRLoadoutV2::RemoveAllGear(FCombatLoadout &Loadout, ECharacterClass Class)
{
	int32 Removed = 0;
	if (Loadout.SecondaryWeapon.IsValid())
	{
		Loadout.SecondaryWeapon.Clear();
		Loadout.SecondarySlotType = ESecondarySlotType::None;
		++Removed;
	}
	Loadout.bShowPrimary = true;
	if (Class != ECharacterClass::Caster)
	{
		for (const FRingLoadoutEntry &Ring : Loadout.RingLoadout)
		{
			Removed += Ring.IsValid() ? 1 : 0;
		}
		Loadout.RingLoadout.Reset();
	}
	return Removed; // D-116: the first entry stays
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildEntryDetails(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry, bool bSimple)
{
	using namespace LoadoutV2Build;
	TArray<FLoadoutV2DetailLine> Lines;
	if (const FWeaponLoadoutEntry *Set = FindWeaponSet(Loadout, Entry))
	{
		AddWeaponPiece(Lines, Set->WeaponEntry, bSimple);
		if (Set->HasOffHand())
		{
			AddWeaponPiece(Lines, Set->OffHandEntry, bSimple);
		}
		if (!bSimple)
		{
			TArray<FText> BuiltIns;
			for (const UAbilityData *Ability : WeaponSetRules::GetSetBuiltIns(*Set))
			{
				if (Ability)
				{
					BuiltIns.Add(FText::FromString(Ability->Name));
				}
			}
			AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailBuiltIns", "Built-in"), FText::Join(LOCTEXT("NameSeparator", ", "), BuiltIns));
		}
		AddSockets(Lines, Set->WeaponEntry.AttachedItem, Set->WeaponEntry.StoneSocket);
		AddLine(Lines, ELoadoutV2DetailKind::SkillSlots, LOCTEXT("DetailSkillSlots", "Skill slots"), FText::AsNumber(Set->GetSkillSlotCount()));
	}
	else if (const FRingLoadoutEntry *Ring = FindAdapter(Loadout, Entry))
	{
		if (const URingData *Adapter = Ring->RingEntry.Ring)
		{
			AddLine(Lines, ELoadoutV2DetailKind::Piece, FText::FromString(Adapter->GetDisplayName()), WoRGameNames::GetTierLetter(Adapter->Tier));
			AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailType", "Type"), WoRGameNames::GetAdapterFormName(Adapter->Form));
			AddEffects(Lines, Adapter, bSimple);
		}
		AddSockets(Lines, Ring->RingEntry.AttachedItem, Ring->RingEntry.StoneSocket);
	}
	else if (Entry.Kind == ELoadoutV2EntryKind::Evolution && Loadout.IsEvolutionLoadout())
	{
		const UEvolutionItemData *Evolution = Loadout.PrimaryEvolution.Item;
		AddLine(Lines, ELoadoutV2DetailKind::Piece, Evolution ? FText::FromString(Evolution->ItemName) : LOCTEXT("EvolutionMissing", "Evolution (missing)"),
				WoRGameNames::GetTierLetter(Loadout.PrimaryEvolution.Tier));
		FRuntimeAttachedItem AsItem;
		AsItem.Kind = EAttachedItemKind::Evolution;
		AsItem.Evolution = Loadout.PrimaryEvolution;
		const ESpellElement Element = AsItem.GetElement();
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailCells", "Cells"), FText::AsNumber(LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST));
		TArray<FText> Spells;
		for (const USpellData *Spell : Loadout.EvolutionSpells)
		{
			if (Spell)
			{
				Spells.Add(FText::FromString(Spell->Name));
			}
		}
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailSpells", "Spells"), FText::Join(LOCTEXT("NameSeparator", ", "), Spells));
		// TODO: B-476d: the evolution's passives as the ET-50 tree (its effects live per infusion on UEvolutionItemData).
	}
	return Lines;
}

FString WoRLoadoutV2::GetDetailsString(const TArray<FLoadoutV2DetailLine> &Lines)
{
	FString Out;
	for (const FLoadoutV2DetailLine &Line : Lines)
	{
		Out += FString::Printf(TEXT("\n  %s%s%s%s"), *FString::ChrN(Line.Depth * LoadoutV2Build::DEBUG_INDENT_SPACES, TEXT(' ')), *Line.Label.ToString(),
							   Line.Label.IsEmpty() || Line.Value.IsEmpty() ? TEXT("") : TEXT(": "), *Line.Value.ToString());
	}
	return Out;
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
