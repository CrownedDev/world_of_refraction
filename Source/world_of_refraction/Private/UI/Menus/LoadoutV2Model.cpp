// LoadoutV2Model.cpp

#include "UI/Menus/LoadoutV2Model.h"
#include "Loadout/WornGear.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WeaponSetRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/EffectText.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/WoRIconTable.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2"

namespace LoadoutV2Build
{
	FText AttachmentName(const FRuntimeAttachedItem &Item); // below: the one name of a socketed piece (the row and the box)

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
		return WoRGameNames::GetHeldPiecesName(Set.WeaponEntry.Weapon, Set.HasOffHand() ? Set.OffHandEntry.Weapon : nullptr); // B-513b: as the wheel
	}

	FLoadoutV2Entry MakeWeaponEntry(const FWeaponLoadoutEntry &Set, ELoadoutV2Place Place)
	{
		FLoadoutV2Entry Entry;
		Entry.Kind = ELoadoutV2EntryKind::Weapon;
		Entry.Place = Place;
		Entry.Name = SetName(Set);
		Entry.Tier = Set.WeaponEntry.Tier;
		Entry.Sockets = {IconOf(Set.WeaponEntry.AttachedItem), IconOf(Set.WeaponEntry.StoneSocket)};
		Entry.SocketName = AttachmentName(Set.WeaponEntry.AttachedItem);
		Entry.SocketBadges = {WoRLoadoutV2::MakeSocketIcon(Set.WeaponEntry.AttachedItem), WoRLoadoutV2::MakeSocketIcon(Set.WeaponEntry.StoneSocket)};
		if (const UWeaponData *Weapon = Set.WeaponEntry.Weapon)
		{
			Entry.TypeIcon.Kind = ELoadoutV2IconKind::WeaponType;
			Entry.TypeIcon.WeaponType = Weapon->WeaponType;
			Entry.TypeIcon.Name = WoRGameNames::GetHeldWeaponTypeName(Weapon, Set.HasOffHand() ? Set.OffHandEntry.Weapon : nullptr); // B-513, B-515
		}
		Entry.SkillSlots = Set.GetSkillSlotCount();
		return Entry;
	}

	FLoadoutV2Entry MakeAdapterEntry(const FRingLoadoutEntry &Ring, ELoadoutV2Place Place, int32 AdapterIndex)
	{
		FLoadoutV2Entry Entry;
		Entry.Kind = ELoadoutV2EntryKind::Adapter;
		Entry.Place = Place;
		Entry.AdapterIndex = AdapterIndex;
		Entry.Name = WoRGameNames::GetAdapterName(Ring.RingEntry.Ring, Ring.RingEntry.AttachedItem); // B-514b: "Fire Ring"
		Entry.Tier = Ring.RingEntry.Tier; // TODO: stage 2b, the built average of the ring and its crystal (D-224 (1))
		Entry.Sockets = {IconOf(Ring.RingEntry.AttachedItem), IconOf(Ring.RingEntry.StoneSocket)};
		Entry.SocketName = AttachmentName(Ring.RingEntry.AttachedItem);
		Entry.SocketBadges = {WoRLoadoutV2::MakeSocketIcon(Ring.RingEntry.AttachedItem), WoRLoadoutV2::MakeSocketIcon(Ring.RingEntry.StoneSocket)};
		Entry.TypeIcon = WoRLoadoutV2::MakeAdapterIcon(Ring.RingEntry); // B-530 (D-229): the frame with its crystal
		return Entry;
	}

	/** The cells the loadout uses for the class: the gear the class fights with, and the character's own evolution. */
	int32 CellsUsed(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FCombatLoadout Mirror = Loadout;
		Mirror.BuildSlotsFromOldFields(Class);
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
			// An evolution whose item is gone still reads as one, on the row and in the box alike (B-482b).
			return Item.Evolution.Item ? FText::FromString(Item.Evolution.Item->ItemName) : LOCTEXT("AttachmentEvolutionNoItem", "Evolution");
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

	/** Adds a line; null when it has nothing to say and isn't added. */
	FLoadoutV2DetailLine *AddLine(TArray<FLoadoutV2DetailLine> &Lines, ELoadoutV2DetailKind Kind, const FText &Label, const FText &Value, int32 Depth = 0)
	{
		if (Kind == ELoadoutV2DetailKind::Stat && Value.IsEmpty())
		{
			return nullptr; // a stat with nothing to say isn't shown
		}
		FLoadoutV2DetailLine &Line = Lines.AddDefaulted_GetRef();
		Line.Kind = Kind;
		Line.Depth = Depth;
		Line.Label = Label;
		Line.Value = Value;
		return &Line;
	}

	/** B-485: a Piece line's tier, for its badge. */
	void SetTier(FLoadoutV2DetailLine *Line, EItemTier Tier)
	{
		if (Line)
		{
			Line->bHasTier = true;
			Line->Tier = Tier;
		}
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
			FLoadoutV2DetailLine *Added = AddLine(Lines, ELoadoutV2DetailKind::Effect, FText::GetEmpty(), Line.Text, Line.Depth);
			// B-485: an effect's own line carries its kind, for the icon beside it (headings and conditions have none).
			if (Added && Line.Kind == EEffectTextLineKind::Effect && Effects.IsValidIndex(Line.EffectIndex) && Effects[Line.EffectIndex].Payloads.Num() > 0)
			{
				Added->Icon.Kind = ELoadoutV2IconKind::Effect;
				Added->Icon.EffectType = Effects[Line.EffectIndex].Payloads[0].EffectType;
				Added->Icon.Element = Effects[Line.EffectIndex].Payloads[0].Element; // its own variant, where the table has one
				Added->Icon.Name = StaticEnum<ESkillEffectType>()->GetDisplayNameTextByValue(static_cast<int64>(Added->Icon.EffectType));
			}
		}
	}

	void AddWeaponPiece(TArray<FLoadoutV2DetailLine> &Lines, const FWeaponInventoryEntry &Owned, bool bSimple)
	{
		const UWeaponData *Weapon = Owned.Weapon;
		if (!Weapon)
		{
			return;
		}
		FLoadoutV2DetailLine *Piece = AddLine(Lines, ELoadoutV2DetailKind::Piece, FText::FromString(Weapon->GetDisplayName()), WoRGameNames::GetTierLetter(Owned.Tier));
		SetTier(Piece, Owned.Tier); // B-516: the owned piece's tier (the run layer can lower it), not the asset's
		if (Piece)
		{
			Piece->Icon.Kind = ELoadoutV2IconKind::WeaponType; // B-485
			Piece->Icon.WeaponType = Weapon->WeaponType;
			Piece->Icon.Name = WoRGameNames::GetHeldWeaponTypeName(Weapon); // B-513
			Piece->TypeName = Piece->Icon.Name; // B-487b: no "Type" row
		}
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailPower", "Power"), Weapon->Power > 0 ? FText::AsNumber(Weapon->Power) : FText::GetEmpty());
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailScaling", "Scaling"), ScalingLine(Weapon->StatScaling));
		AddEffects(Lines, Weapon, bSimple);
	}

	void AddSockets(TArray<FLoadoutV2DetailLine> &Lines, const FRuntimeAttachedItem &Socket1, const FRuntimeAttachedItem &Socket2)
	{
		// B-485: each socket line carries its badge; B-487b: and the tier of what it holds.
		const FRuntimeAttachedItem *Sockets[] = {&Socket1, &Socket2};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sockets); ++Index)
		{
			const FRuntimeAttachedItem &Socket = *Sockets[Index];
			if (FLoadoutV2DetailLine *Line = AddLine(Lines, ELoadoutV2DetailKind::Socket, SocketWord(Socket, Index + 1), AttachmentLine(Socket)))
			{
				Line->Icon = WoRLoadoutV2::MakeSocketIcon(Socket);
				Line->bHasTier = WoRLoadoutV2::GetAttachmentTier(Socket, Line->Tier);
			}
		}
	}

	/** D-226 (4): the piece's tier and its socket's, on two lines, in place of B-519's averaged "Skill tier D (F + B)".
	 *  D-227 (5): the tier in brackets, "Adapter (S)", "Crystal (B)", so the whole line is the label (no value). The
	 *  crystal line only while the socket holds a gem or an evolution. */
	void AddPieceAndCrystalTiers(TArray<FLoadoutV2DetailLine> &Lines, const FText &PieceWord, EItemTier PieceTier,
								 const FRuntimeAttachedItem &Socket)
	{
		const auto AddTierLine = [&Lines](const FText &Word, EItemTier Tier)
		{
			FLoadoutV2DetailLine &Line = Lines.AddDefaulted_GetRef();
			Line.Kind = ELoadoutV2DetailKind::Stat;
			Line.Label = FText::Format(LOCTEXT("DetailTierInBrackets", "{0} ({1})"), Word, WoRGameNames::GetTierLetter(Tier));
			SetTier(&Line, Tier);
		};
		AddTierLine(PieceWord, PieceTier);
		if (const TOptional<EItemTier> SocketTier = CrystalEffectTable::GetSocketSkillTier(Socket))
		{
			AddTierLine(LOCTEXT("DetailCrystalWord", "Crystal"), SocketTier.GetValue());
		}
	}

	/** A weapon entry's lines (one weapon, or a pair): the pieces, the sockets, the skill slots. */
	void AddWeaponSet(TArray<FLoadoutV2DetailLine> &Lines, const FWeaponLoadoutEntry &Set, bool bSimple)
	{
		AddWeaponPiece(Lines, Set.WeaponEntry, bSimple);
		if (Set.HasOffHand())
		{
			AddWeaponPiece(Lines, Set.OffHandEntry, bSimple);
		}
		AddSockets(Lines, Set.WeaponEntry.AttachedItem, Set.WeaponEntry.StoneSocket);
		AddPieceAndCrystalTiers(Lines, LOCTEXT("DetailWeaponWord", "Weapon"), Set.WeaponEntry.Tier, Set.WeaponEntry.AttachedItem);
		AddLine(Lines, ELoadoutV2DetailKind::SkillSlots, LOCTEXT("DetailSkillSlots", "Skill slots"), FText::AsNumber(Set.GetSkillSlotCount()));
	}

	/** An adapter's lines: the piece, its form, its effects, its sockets. */
	void AddAdapter(TArray<FLoadoutV2DetailLine> &Lines, const FRingInventoryEntry &Owned, bool bSimple)
	{
		if (const URingData *Adapter = Owned.Ring)
		{
			if (FLoadoutV2DetailLine *Piece = AddLine(Lines, ELoadoutV2DetailKind::Piece, WoRGameNames::GetAdapterName(Adapter, Owned.AttachedItem), WoRGameNames::GetTierLetter(Owned.Tier)))
			{
				SetTier(Piece, Owned.Tier);
				Piece->TypeName = WoRGameNames::GetAdapterFormName(Adapter->Form); // B-487b: no "Type" row
			}
			AddEffects(Lines, Adapter, bSimple);
		}
		AddSockets(Lines, Owned.AttachedItem, Owned.StoneSocket);
		AddPieceAndCrystalTiers(Lines, WoRGameNames::GetAdapterWord(), Owned.Tier, Owned.AttachedItem);
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

	/** B-309d2: Entry's WornGearLayout place; INDEX_NONE for the evolution seat (not a slot). */
	int32 PlaceOf(const FLoadoutV2Entry &Entry)
	{
		switch (Entry.Place)
		{
		case ELoadoutV2Place::Primary:
			return WornGearLayout::PRIMARY_INDEX;
		case ELoadoutV2Place::Secondary:
			return WornGearLayout::SECONDARY_INDEX;
		case ELoadoutV2Place::Adapter:
			return Entry.AdapterIndex == INDEX_NONE ? INDEX_NONE : WornGearLayout::FIRST_ADAPTER_INDEX + Entry.AdapterIndex;
		default:
			return INDEX_NONE;
		}
	}

	/** B-309d2: puts a piece in a place through the slots. */
	void PutPiece(FCombatLoadout &Loadout, ECharacterClass Class, int32 Place, const FLoadoutSlot &Piece)
	{
		Loadout.EditSlots(Class, [Place, &Piece](FCombatLoadout &Edited) { Edited.PutSlotAt(Place, Piece); });
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
			// A shield is an off-hand piece whatever else is set: say that, not "Twin only" (its Handling is DualOnly,
			// which for a shield means it pairs and never stands alone).
			if (Draft.MainHand.Weapon && Draft.MainHand.Weapon->WeaponType == EWeaponType::Shield)
			{
				return WeaponSetRules::GetPairRefusalText(WeaponSetRules::EPairRefusal::ShieldInMainHand);
			}
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
		Seat.Tier = Loadout.PrimaryEvolution.Tier; // B-516b: its own badge, as its detail line
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
	// B-516 / B-516b: two same-named entries read apart, by the tier badge first, then a number.
	TArray<FText> Names;
	TArray<EItemTier> Tiers;
	for (const FLoadoutV2Entry &Entry : Page.Entries)
	{
		Names.Add(Entry.Name);
		Tiers.Add(Entry.Tier);
	}
	WoRGameNames::NumberRepeatedNames(Names, Tiers);
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		Page.Entries[Index].Name = Names[Index];
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
			PutPiece(Loadout, Class, WornGearLayout::PRIMARY_INDEX, FLoadoutSlot::FromWeaponLoadoutEntry(Set));
			return true;
		}
		if (Class == ECharacterClass::Generic && !Loadout.SecondaryWeapon.IsValid())
		{
			PutPiece(Loadout, Class, WornGearLayout::SECONDARY_INDEX, FLoadoutSlot::FromWeaponLoadoutEntry(Set));
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
			PutPiece(Loadout, Class, WornGearLayout::PRIMARY_INDEX, FLoadoutSlot::FromRingLoadoutEntry(Ring));
			return true;
		}
		const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
		if (Limits.MaxAdapters != LoadoutClassTable::NO_LIMIT && CountAdapters(Loadout) >= Limits.MaxAdapters)
		{
			OutRefusal = LOCTEXT("RefuseNoAdapterEntry", "No free adapter entry.");
			return false;
		}
		// A Resonator's adapters, a Generic's adapter entry (D-210): a new place after the last adapter entry.
		PutPiece(Loadout, Class, WornGearLayout::FIRST_ADAPTER_INDEX + Loadout.RingLoadout.Num(), FLoadoutSlot::FromRingLoadoutEntry(Ring));
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
		else if (Pair == WeaponSetRules::EPairRefusal::NotPairable && MainHand.Weapon && !MainHand.Weapon->CanBePaired())
		{
			// The main hand is the one that can't pair: say so, not "this weapon" on a shield's or a second sword's line.
			Choice.Refusal = FText::Format(LOCTEXT("RefuseMainTakesNoOffHand", "{0} takes no off hand."), FText::FromString(MainHand.Weapon->GetDisplayName()));
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

bool WoRLoadoutV2::SetMainHand(FLoadoutV2Draft &Draft, const FWeaponInventoryEntry &MainHand)
{
	Draft.MainHand = MainHand;
	Draft.Socket1 = MainHand.AttachedItem; // Q-476c: sockets stay on their piece; a new main hand brings its own
	Draft.Socket2 = MainHand.StoneSocket;
	if (!Draft.OffHand.IsValid())
	{
		return false;
	}
	const WeaponSetRules::EPairRefusal Pair = WeaponSetRules::CheckPair(MainHand, Draft.OffHand);
	if (Pair != WeaponSetRules::EPairRefusal::NotPairable && Pair != WeaponSetRules::EPairRefusal::DifferentTypes)
	{
		return false;
	}
	Draft.OffHand = FWeaponInventoryEntry();
	return true;
}

FText WoRLoadoutV2::GetWeaponRowName(const FText &Name, const UWeaponData *Weapon)
{
	const FText Type = WoRGameNames::GetHeldWeaponTypeName(Weapon);
	return Type.IsEmpty() ? Name : FText::Format(LOCTEXT("WeaponRowName", "{0} · {1}"), Name, Type);
}

FLoadoutV2Compare WoRLoadoutV2::BuildCompare(const FWeaponInventoryEntry &Equipped, const FWeaponInventoryEntry &Selected)
{
	FLoadoutV2Compare Compare;
	Compare.EquippedPower = Equipped.Weapon ? Equipped.Weapon->Power : 0;
	Compare.SelectedPower = Selected.Weapon ? Selected.Weapon->Power : 0;
	Compare.CellsChange = 0; // D-206: a weapon itself is free; the sockets stay with the entry
	// B-518 (D-224): no built-ins to gain or lose (BuiltInsGained / BuiltInsLost are OBSOLETE, deleted in stage 6).
	return Compare;
}

TArray<FLoadoutV2KindChoice> WoRLoadoutV2::BuildAddKinds(const FCombatLoadout &Loadout, ECharacterClass Class)
{
	using namespace LoadoutV2Build;
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);
	const FLoadoutV2WeaponsPage Page = BuildWeaponsPage(Loadout, Class);
	// B-503u (D-220): a class limit says so ("A Generic holds at most 2 weapons."), in the class's own name.
	const FText ClassName = WoRGameNames::GetCharacterClassName(Class);
	const FText Full = Limits.MaxItems == LoadoutClassTable::NO_LIMIT
						   ? LOCTEXT("RefuseEntriesFull", "Every entry is taken.")
						   : FText::Format(LOCTEXT("RefusePieceLimit", "A {0} holds at most {1} {1}|plural(one=piece,other=pieces)."), ClassName, Limits.MaxItems);
	TArray<FLoadoutV2KindChoice> Kinds;

	FLoadoutV2KindChoice &Weapon = Kinds.AddDefaulted_GetRef();
	Weapon.Kind = ELoadoutV2DraftKind::Weapon;
	Weapon.Name = LOCTEXT("KindWeapon", "Add a weapon");
	const bool bWeaponPlace = PrimaryIsFree(Loadout) || (Class == ECharacterClass::Generic && !Loadout.SecondaryWeapon.IsValid());
	if (!Page.bCanAdd)
	{
		Weapon.Refusal = Full;
	}
	else if (CountWeaponEntries(Loadout, Class) >= Limits.MaxWeapons)
	{
		Weapon.Refusal = FText::Format(LOCTEXT("RefuseWeaponLimit", "A {0} holds at most {1} {1}|plural(one=weapon,other=weapons)."), ClassName, Limits.MaxWeapons);
	}
	else if (!bWeaponPlace)
	{
		Weapon.Refusal = LOCTEXT("RefuseNoWeaponEntry", "No free weapon entry.");
	}

	FLoadoutV2KindChoice &Adapter = Kinds.AddDefaulted_GetRef();
	Adapter.Kind = ELoadoutV2DraftKind::Adapter;
	Adapter.Name = FText::Format(LOCTEXT("KindAdapter", "Add an {0}"), WoRGameNames::GetAdapterWord().ToLower()); // the shared word (D-062)
	const bool bAdapterRoom = Class == ECharacterClass::Caster ? PrimaryIsFree(Loadout)
																: Limits.MaxAdapters == LoadoutClassTable::NO_LIMIT || CountAdapters(Loadout) < Limits.MaxAdapters;
	if (!Page.bCanAdd)
	{
		Adapter.Refusal = Full;
	}
	else if (!bAdapterRoom && Class != ECharacterClass::Caster)
	{
		Adapter.Refusal = FText::Format(LOCTEXT("RefuseAdapterLimit", "A {0} holds at most {1} {1}|plural(one=adapter,other=adapters)."), ClassName, Limits.MaxAdapters);
	}
	else if (!bAdapterRoom)
	{
		Adapter.Refusal = LOCTEXT("RefuseNoAdapterEntry", "No free adapter entry.");
	}

	FLoadoutV2KindChoice &Evolution = Kinds.AddDefaulted_GetRef();
	Evolution.Kind = ELoadoutV2DraftKind::Evolution;
	Evolution.Name = LOCTEXT("KindEvolution", "Add an evolution");
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

namespace LoadoutV2Replace
{
	/** Entry taken out to make room for a piece of another kind. The first entry, which RemoveEntry keeps (D-116), is
	 *  emptied here: the piece replacing it must fill it (ReplaceEntry checks). */
	bool TakeOut(FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry, FText &OutRefusal)
	{
		if (WoRLoadoutV2::RemoveEntry(Loadout, Class, Entry, OutRefusal))
		{
			return true;
		}
		if (Entry.Place != ELoadoutV2Place::Primary && Entry.Place != ELoadoutV2Place::Seat)
		{
			return false;
		}
		OutRefusal = FText::GetEmpty();
		Loadout.EditSlots(Class, [](FCombatLoadout &Edited) // as WoRLoadoutScreen's ClearPrimary
		{
			Edited.ClearSlotAt(WornGearLayout::PRIMARY_INDEX);
			Edited.PrimaryWeapon.Clear(); // and one hidden under the other type, which EditSlots would keep
			Edited.PrimaryRing.Clear();
			Edited.PrimaryEvolution = FEvolutionAttachment();
			Edited.PrimaryEvolutionInstance = FGuid();
			Edited.EvolutionSpells.Reset();
			Edited.PrimarySlotType = EPrimarySlotType::None;
		});
		return true;
	}

	/** Whether ApplyDraft puts a Kind piece in a free first entry (a weapon, the evolution, a Refractor's adapter). */
	bool FillsFirstEntry(ELoadoutV2DraftKind Kind, ECharacterClass Class)
	{
		return Kind != ELoadoutV2DraftKind::Adapter || Class == ECharacterClass::Caster;
	}

	FText KeepFirstEntry()
	{
		return LOCTEXT("RefuseReplaceFirst", "The first entry keeps a weapon: it can't become this.");
	}
}

ELoadoutV2DraftKind WoRLoadoutV2::GetDraftKind(ELoadoutV2EntryKind Kind)
{
	return Kind == ELoadoutV2EntryKind::Adapter ? ELoadoutV2DraftKind::Adapter : Kind == ELoadoutV2EntryKind::Evolution ? ELoadoutV2DraftKind::Evolution : ELoadoutV2DraftKind::Weapon;
}

TArray<FLoadoutV2KindChoice> WoRLoadoutV2::BuildEditKinds(const FCombatLoadout &Loadout, ECharacterClass Class, const FLoadoutV2Entry &Entry)
{
	using namespace LoadoutV2Build;
	FCombatLoadout Without = Loadout;
	FText OutRefusal;
	const bool bOut = LoadoutV2Replace::TakeOut(Without, Class, Entry, OutRefusal);
	TArray<FLoadoutV2KindChoice> Kinds = BuildAddKinds(bOut ? Without : Loadout, Class);
	const ELoadoutV2DraftKind Own = GetDraftKind(Entry.Kind);
	for (FLoadoutV2KindChoice &Kind : Kinds)
	{
		if (Kind.Kind == Own)
		{
			Kind.Refusal = FText::GetEmpty(); // its own place: edit it, or another piece of its kind
		}
		else if (!bOut)
		{
			Kind.Refusal = OutRefusal;
		}
		else if (Kind.Refusal.IsEmpty() && PrimaryIsFree(Without) && !LoadoutV2Replace::FillsFirstEntry(Kind.Kind, Class))
		{
			Kind.Refusal = LoadoutV2Replace::KeepFirstEntry();
		}
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
		Choice.Name = WoRGameNames::GetAdapterName(Owned.Ring, Owned.AttachedItem);
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
	if (Draft.Kind != GetDraftKind(Entry.Kind))
	{
		// B-531 (D-230): another kind replaces the entry; on a copy, so a refusal leaves the loadout as it was.
		FCombatLoadout After = Loadout;
		if (!LoadoutV2Replace::TakeOut(After, Class, Entry, OutRefusal) || !ApplyDraft(After, Class, Draft, OutRefusal))
		{
			return false;
		}
		if (PrimaryIsFree(After))
		{
			OutRefusal = LoadoutV2Replace::KeepFirstEntry();
			return false;
		}
		Loadout = MoveTemp(After);
		return true;
	}
	const FText WrongPlace = LOCTEXT("RefuseWrongPlace", "It can't go in this place.");
	switch (Entry.Kind)
	{
	case ELoadoutV2EntryKind::Weapon:
	{
		const FWeaponLoadoutEntry *Current = FindWeaponSet(Loadout, Entry);
		if (Draft.Kind != ELoadoutV2DraftKind::Weapon || !Current || !Current->IsValid() || PlaceOf(Entry) == INDEX_NONE)
		{
			OutRefusal = WrongPlace;
			return false;
		}
		if (!Draft.MainHand.IsValid())
		{
			OutRefusal = LOCTEXT("RefuseNoMainHand", "Pick a main hand.");
			return false;
		}
		FWeaponLoadoutEntry Set = *Current;
		const bool bSamePiece = Set.WeaponEntry.PersistentID == Draft.MainHand.PersistentID;
		Set.WeaponEntry = Draft.MainHand;
		Set.WeaponEntry.AttachedItem = Draft.Socket1; // the entry's sockets are the main hand's (D-210)
		Set.WeaponEntry.StoneSocket = Draft.Socket2;
		Set.OffHandEntry = Draft.OffHand;
		if (!bSamePiece)
		{
			Set.InitializeFromWeapon(); // a new main hand starts with no slotted picks of the old one
		}
		PutPiece(Loadout, Class, PlaceOf(Entry), FLoadoutSlot::FromWeaponLoadoutEntry(Set));
		return true;
	}
	case ELoadoutV2EntryKind::Adapter:
	{
		const FRingLoadoutEntry *Current = FindAdapter(Loadout, Entry);
		if (Draft.Kind != ELoadoutV2DraftKind::Adapter || !Current || !Current->IsValid() || !Draft.Adapter.IsValid() || PlaceOf(Entry) == INDEX_NONE)
		{
			OutRefusal = WrongPlace;
			return false;
		}
		FRingLoadoutEntry Ring = *Current;
		const bool bSamePiece = Ring.RingEntry.PersistentID == Draft.Adapter.PersistentID;
		Ring.RingEntry = Draft.Adapter;
		Ring.RingEntry.AttachedItem = Draft.Socket1;
		Ring.RingEntry.StoneSocket = Draft.Socket2;
		if (!bSamePiece)
		{
			Ring.InitializeFromRing();
		}
		PutPiece(Loadout, Class, PlaceOf(Entry), FLoadoutSlot::FromRingLoadoutEntry(Ring));
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
	Loadout.EditSlots(Class, [&Removed](FCombatLoadout &Edited)
	{
		if (Edited.SecondaryWeapon.IsValid()) // any class's, as before (a non-Generic's isn't in the mirror)
		{
			Edited.ClearSlotAt(WornGearLayout::SECONDARY_INDEX);
			Edited.SecondaryWeapon.Clear();
			Edited.SecondarySlotType = ESecondarySlotType::None;
			++Removed;
		}
		Edited.bShowPrimary = true;
		// The mirror holds no Caster adapter loadout (EditSlots keeps it), so this takes adapters off a Resonator or a Generic only.
		Removed += Edited.Slots.RemoveAll([](const FLoadoutSlot &Slot) { return Slot.Place >= WornGearLayout::FIRST_ADAPTER_INDEX; });
	});
	return Removed; // D-116: the first entry stays
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildEntryDetails(const FCombatLoadout &Loadout, const FLoadoutV2Entry &Entry, bool bSimple)
{
	using namespace LoadoutV2Build;
	TArray<FLoadoutV2DetailLine> Lines;
	if (const FWeaponLoadoutEntry *Set = FindWeaponSet(Loadout, Entry))
	{
		AddWeaponSet(Lines, *Set, bSimple);
	}
	else if (const FRingLoadoutEntry *Ring = FindAdapter(Loadout, Entry))
	{
		AddAdapter(Lines, Ring->RingEntry, bSimple);
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

FLoadoutV2Icon WoRLoadoutV2::MakeSocketIcon(const FRuntimeAttachedItem &Item)
{
	FLoadoutV2Icon Icon;
	if (Item.IsEmpty())
	{
		Icon.Kind = ELoadoutV2IconKind::EmptySocket;
		Icon.Name = LOCTEXT("IconEmptySocket", "Empty socket");
		return Icon;
	}
	Icon.Name = LoadoutV2Build::AttachmentName(Item);
	if (Item.IsAugmentStone())
	{
		Icon.Kind = ELoadoutV2IconKind::Stone;
		return Icon;
	}
	Icon.Kind = ELoadoutV2IconKind::Element;
	Icon.Element = Item.GetElement();
	return Icon;
}

FLoadoutV2Icon WoRLoadoutV2::MakeAdapterIcon(const FRingInventoryEntry &Adapter)
{
	const FRuntimeAttachedItem &Socket = Adapter.AttachedItem;
	const ESpellElement Element = Socket.IsEmpty() || Socket.IsAugmentStone() ? ESpellElement::None : Socket.GetElement();
	return WoRIcons::MakeAdapterIcon(Element, WoRGameNames::GetAdapterName(Adapter.Ring, Socket));
}

bool WoRLoadoutV2::GetAttachmentTier(const FRuntimeAttachedItem &Item, EItemTier &OutTier)
{
	if (Item.IsEmpty())
	{
		return false;
	}
	OutTier = Item.IsEvolution() ? Item.Evolution.Tier : StatStone::AsRowId(Item.Crystal.Id).Tier;
	return true;
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildWeaponDetails(const FWeaponInventoryEntry &Weapon, bool bSimple)
{
	TArray<FLoadoutV2DetailLine> Lines;
	if (Weapon.Weapon)
	{
		FWeaponLoadoutEntry Alone; // the weapon as a one-weapon entry: its own sockets and skill slots
		Alone.WeaponEntry = Weapon;
		LoadoutV2Build::AddWeaponSet(Lines, Alone, bSimple);
	}
	return Lines;
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildAdapterDetails(const FRingInventoryEntry &Adapter, bool bSimple)
{
	TArray<FLoadoutV2DetailLine> Lines;
	if (Adapter.Ring)
	{
		LoadoutV2Build::AddAdapter(Lines, Adapter, bSimple);
	}
	return Lines;
}

FLoadoutV2DetailLine WoRLoadoutV2::MakeNoteLine(const FText &Text)
{
	FLoadoutV2DetailLine Line;
	Line.Kind = ELoadoutV2DetailKind::Effect; // an unindented plain line
	Line.Value = Text;
	return Line;
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildSocketDetails(const FRuntimeAttachedItem &Item, bool bFirstSocket, bool bSimple)
{
	if (!Item.IsEmpty())
	{
		return BuildAttachmentDetails(Item, bSimple);
	}
	return {MakeNoteLine(bFirstSocket ? LOCTEXT("SocketOneTakes", "Empty. Takes a crystal, an evolution or a stone.")
									  : LOCTEXT("SocketTwoTakes", "Empty. Takes a stone."))};
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildOffHandDetails(const FWeaponInventoryEntry &OffHand, bool bSimple)
{
	if (OffHand.IsValid())
	{
		return BuildWeaponDetails(OffHand, bSimple);
	}
	return {MakeNoteLine(LOCTEXT("OffHandTakes", "Empty: the weapon alone. A second weapon of the same type that can be dual wielded goes here."))};
}

TArray<FLoadoutV2DetailLine> WoRLoadoutV2::BuildAttachmentDetails(const FRuntimeAttachedItem &Item, bool bSimple)
{
	using namespace LoadoutV2Build;
	TArray<FLoadoutV2DetailLine> Lines;
	if (Item.IsEmpty())
	{
		return Lines;
	}
	EItemTier Tier = EItemTier::F_Tier;
	GetAttachmentTier(Item, Tier);
	if (FLoadoutV2DetailLine *Piece = AddLine(Lines, ELoadoutV2DetailKind::Piece, AttachmentName(Item), WoRGameNames::GetTierLetter(Tier)))
	{
		SetTier(Piece, Tier);
		Piece->Icon = MakeSocketIcon(Item); // B-485: its element badge (or the stone's)
		constexpr int32 FILLED_SOCKET = 1; // SocketWord numbers only an empty socket; this one is filled
		Piece->TypeName = SocketWord(Item, FILLED_SOCKET); // B-487b: "Crystal", "Stone", "Evolution"; no "Type" row
	}
	const ESpellElement Element = Item.GetElement();
	AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailElement", "Element"), Element == ESpellElement::None ? FText::GetEmpty() : WoRGameNames::GetElementName(Element));
	if (!bSimple)
	{
		const int32 Cells = Item.IsEvolution()		? LoadoutConstants::LOADOUT_EVOLUTION_COST
							: Item.IsAugmentStone() ? LoadoutConstants::LOADOUT_STONE_COST
													: LoadoutConstants::LOADOUT_CRYSTAL_COST;
		AddLine(Lines, ELoadoutV2DetailKind::Stat, LOCTEXT("DetailCells", "Cells"), FText::AsNumber(Cells));
	}
	// TODO: B-476d: a crystal's and a stone's effect text, and an evolution's passives (no text helper yet).
	return Lines;
}

FString WoRLoadoutV2::GetDetailsString(const TArray<FLoadoutV2DetailLine> &Lines)
{
	FString Out;
	for (const FLoadoutV2DetailLine &Line : Lines)
	{
		Out += FString::Printf(TEXT("\n  %s%s%s%s%s"), *FString::ChrN(Line.Depth * LoadoutV2Build::DEBUG_INDENT_SPACES, TEXT(' ')), *Line.Label.ToString(),
							   Line.Label.IsEmpty() || Line.Value.IsEmpty() ? TEXT("") : TEXT(": "), *Line.Value.ToString(),
							   Line.TypeName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" [%s]"), *Line.TypeName.ToString()));
	}
	return Out;
}

FString WoRLoadoutV2::GetWeaponsPageString(const FLoadoutV2WeaponsPage &Page)
{
	FString Out = FString::Printf(TEXT("%s; Add equipment %s%s"), *Page.CellsLine.ToString(), *Page.AddFrameLine.ToString(), Page.bCanAdd ? TEXT("") : TEXT(" (full)"));
	for (const FLoadoutV2Entry &Entry : Page.Entries)
	{
		Out += FString::Printf(TEXT("\n  %s%s, %d socket(s), %d skill slot(s)"), *Entry.Name.ToString(), Entry.bActive ? TEXT(" [ACTIVE]") : TEXT(""),
							   Entry.Sockets.Num(), Entry.SkillSlots);
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
