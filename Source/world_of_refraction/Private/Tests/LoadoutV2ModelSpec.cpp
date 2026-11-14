// LoadoutV2ModelSpec.cpp
// B-476 (D-206, D-209, D-210; UIScreens.md #2-3), first part: the v2 loadout screen's model. The Weapons tab lists the
// entries by name (a pair as "A + B"), their two sockets as icons and the weapon in hand; only "Cells used n of 5" and
// the Add weapon frame's "n/5 left" show the cells. The Add gear window's draft is refused in the game's words (a pair
// of two types, a twin-only weapon alone, an evolution where the class can't put one, no free entry, over the cells)
// and goes into the next free place; Compare sets built-ins gained beside lost.
// B-476c: the first question (Weapon / Adapter / Evolution) greys a kind with no free place; the Adapter and Evolution
// lists; an entry edited in its place (a new off hand, a twin-only main hand left alone refused), taken out (the first
// entry stays, D-116; a Generic's second weapon moves up), Remove all gear; the details box, full and simple.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Menus/LoadoutV2Model.h"

namespace
{
	FWeaponInventoryEntry LoadoutV2TestWeapon(const TCHAR *Name, EWeaponType Type, EWeaponHandling Handling = EWeaponHandling::SingleOrDual)
	{
		UWeaponData *Data = NewObject<UWeaponData>(GetTransientPackage());
		Data->Name = Name;
		Data->WeaponType = Type;
		Data->Handling = Handling;
		FWeaponInventoryEntry Entry;
		Entry.Weapon = Data;
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	FRingInventoryEntry LoadoutV2TestAdapter()
	{
		FRingInventoryEntry Entry;
		Entry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.Ring->Name = TEXT("Ember Ring");
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	FRuntimeAttachedItem LoadoutV2TestEvolution()
	{
		FRuntimeAttachedItem Item;
		Item.Kind = EAttachedItemKind::Evolution;
		Item.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		return Item;
	}

	FRuntimeAttachedItem LoadoutV2TestStone()
	{
		FRuntimeAttachedItem Item;
		Item.Kind = EAttachedItemKind::AugmentStone;
		return Item;
	}

	FLoadoutV2Draft LoadoutV2WeaponDraft(const FWeaponInventoryEntry &Main, const FWeaponInventoryEntry &Off = FWeaponInventoryEntry())
	{
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Weapon;
		Draft.MainHand = Main;
		Draft.OffHand = Off;
		return Draft;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutV2ModelSpec, "WoR.UI.LoadoutV2", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutV2ModelSpec)

void FLoadoutV2ModelSpec::Define()
{
	It("lists a Generic's dual entry, its second entry and its cells (UIScreens #2-3)", [this]()
	{
		FCombatLoadout Loadout;
		FLoadoutV2Draft Pair = LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Ashen Blade"), EWeaponType::Sword),
													LoadoutV2TestWeapon(TEXT("Cinder Edge"), EWeaponType::Sword));
		Pair.Socket1 = LoadoutV2TestEvolution(); // 2
		Pair.Socket2 = LoadoutV2TestStone();	 // 1
		FText Refusal;
		TestTrue(TEXT("the pair goes in"), WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Pair, Refusal));
		TestTrue(TEXT("the hammer goes in"),
				 WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Quarry Maul"), EWeaponType::Hammer)), Refusal));
		const FLoadoutV2WeaponsPage Page = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic);
		if (!TestEqual(TEXT("two entries"), Page.Entries.Num(), 2))
		{
			return;
		}
		TestEqual(TEXT("names only, a pair as A + B"), Page.Entries[0].Name.ToString(), FString(TEXT("Ashen Blade + Cinder Edge")));
		TestTrue(TEXT("its sockets as icons"), Page.Entries[0].Sockets.Num() == 2 && Page.Entries[0].Sockets[0] == ELoadoutV2SocketIcon::Evolution &&
												   Page.Entries[0].Sockets[1] == ELoadoutV2SocketIcon::Stone);
		TestTrue(TEXT("the first is in hand"), Page.Entries[0].bActive && !Page.Entries[1].bActive);
		TestEqual(TEXT("cells used 3"), Page.CellsUsed, LoadoutConstants::LOADOUT_EVOLUTION_COST + LoadoutConstants::LOADOUT_STONE_COST);
		TestEqual(TEXT("Cells used 3 of 5"), Page.CellsLine.ToString(), FString(TEXT("Cells used 3 of 5")));
		TestEqual(TEXT("Add weapon: 2/5 left"), Page.AddFrameLine.ToString(), FString(TEXT("2/5 left")));
		TestTrue(TEXT("an adapter entry is still free"), Page.bCanAdd);
	});

	It("refuses a draft in the game's words (D-209, D-210)", [this]()
	{
		FCombatLoadout Empty;
		int32 Cells = 0;
		const FWeaponInventoryEntry Sword = LoadoutV2TestWeapon(TEXT("Sword"), EWeaponType::Sword);
		const FWeaponInventoryEntry Dagger = LoadoutV2TestWeapon(TEXT("Dagger"), EWeaponType::Dagger);
		TestFalse(TEXT("a sword with a dagger"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, LoadoutV2WeaponDraft(Sword, Dagger), Cells).IsEmpty());
		const FWeaponInventoryEntry Twin = LoadoutV2TestWeapon(TEXT("Twin"), EWeaponType::Sword, EWeaponHandling::DualOnly);
		TestFalse(TEXT("a twin-only weapon alone"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, LoadoutV2WeaponDraft(Twin), Cells).IsEmpty());
		TestTrue(TEXT("two swords pair"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, LoadoutV2WeaponDraft(Sword, Twin), Cells).IsEmpty());

		FLoadoutV2Draft EvolvedWeapon = LoadoutV2WeaponDraft(Sword);
		EvolvedWeapon.Socket1 = LoadoutV2TestEvolution();
		TestFalse(TEXT("a Resonator's evolved weapon"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Resonator, EvolvedWeapon, Cells).IsEmpty());
		TestTrue(TEXT("a Generic's evolved weapon"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, EvolvedWeapon, Cells).IsEmpty());
		TestEqual(TEXT("its cells after"), Cells, LoadoutConstants::LOADOUT_EVOLUTION_COST);

		FLoadoutV2Draft EvolvedAdapter;
		EvolvedAdapter.Kind = ELoadoutV2DraftKind::Adapter;
		EvolvedAdapter.Adapter = LoadoutV2TestAdapter();
		EvolvedAdapter.Socket1 = LoadoutV2TestEvolution();
		TestFalse(TEXT("a Generic's evolved adapter"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, EvolvedAdapter, Cells).IsEmpty());

		FLoadoutV2Draft GemInSocket2 = LoadoutV2WeaponDraft(Sword);
		GemInSocket2.Socket2.Kind = EAttachedItemKind::Crystal;
		TestFalse(TEXT("socket 2 takes a stone"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, GemInSocket2, Cells).IsEmpty());
	});

	It("puts a Generic's adapter beside two weapons and refuses a second; holds a Refractor to one entry and 2 cells (D-210)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("A"), EWeaponType::Sword)), Refusal);
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("B"), EWeaponType::Hammer)), Refusal);
		int32 Cells = 0;
		TestFalse(TEXT("a third weapon: no free entry"),
				  WoRLoadoutV2::CheckDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("C"), EWeaponType::Spear)), Cells).IsEmpty());
		FLoadoutV2Draft Adapter;
		Adapter.Kind = ELoadoutV2DraftKind::Adapter;
		Adapter.Adapter = LoadoutV2TestAdapter();
		TestTrue(TEXT("an adapter fits"), WoRLoadoutV2::CheckDraft(Loadout, ECharacterClass::Generic, Adapter, Cells).IsEmpty());
		TestTrue(TEXT("it goes in"), WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Adapter, Refusal));
		TestEqual(TEXT("into the adapter list"), Loadout.RingLoadout.Num(), 1);
		TestFalse(TEXT("a second adapter: refused"), WoRLoadoutV2::CheckDraft(Loadout, ECharacterClass::Generic, Adapter, Cells).IsEmpty());
		TestFalse(TEXT("the Add frame is closed"), WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).bCanAdd);

		FCombatLoadout Refractor;
		FLoadoutV2Draft Heavy = LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Staff"), EWeaponType::Staff));
		Heavy.Socket1 = LoadoutV2TestEvolution();
		Heavy.Socket2 = LoadoutV2TestStone();
		TestFalse(TEXT("Refractor: evolution + stone is over 2 cells"), WoRLoadoutV2::CheckDraft(Refractor, ECharacterClass::Caster, Heavy, Cells).IsEmpty());
		Heavy.Socket2 = FRuntimeAttachedItem();
		TestTrue(TEXT("Refractor: an evolved staff fits"), WoRLoadoutV2::CheckDraft(Refractor, ECharacterClass::Caster, Heavy, Cells).IsEmpty());
	});

	It("greys an off hand that can't pair with its reason", [this]()
	{
		const FWeaponInventoryEntry Main = LoadoutV2TestWeapon(TEXT("Ashen Blade"), EWeaponType::Sword);
		const TArray<FLoadoutV2OffHandChoice> Choices = WoRLoadoutV2::BuildOffHandChoices(
			Main, {Main, LoadoutV2TestWeapon(TEXT("Cinder Edge"), EWeaponType::Sword), LoadoutV2TestWeapon(TEXT("Fang"), EWeaponType::Dagger)}, ECharacterClass::Generic);
		if (TestEqual(TEXT("the main hand itself is not offered"), Choices.Num(), 2))
		{
			TestTrue(TEXT("the other sword fits"), Choices[0].Refusal.IsEmpty());
			TestFalse(TEXT("the dagger is greyed"), Choices[1].Refusal.IsEmpty());
		}
	});

	It("compares built-ins gained and lost", [this]()
	{
		FWeaponInventoryEntry Equipped = LoadoutV2TestWeapon(TEXT("Cinder Edge"), EWeaponType::Sword);
		FWeaponInventoryEntry Selected = LoadoutV2TestWeapon(TEXT("Frost Fang"), EWeaponType::Sword);
		UAbilityData *Shared = NewObject<UAbilityData>(GetTransientPackage());
		UAbilityData *Burn = NewObject<UAbilityData>(GetTransientPackage());
		UAbilityData *Chill = NewObject<UAbilityData>(GetTransientPackage());
		Equipped.Weapon->PresetAbilities = {Shared, Burn};
		Selected.Weapon->PresetAbilities = {Shared, Chill};
		const FLoadoutV2Compare Compare = WoRLoadoutV2::BuildCompare(Equipped, Selected);
		TestEqual(TEXT("one gained"), Compare.BuiltInsGained.Num(), 1);
		TestEqual(TEXT("one lost"), Compare.BuiltInsLost.Num(), 1);
		TestEqual(TEXT("weapons cost no cells"), Compare.CellsChange, 0);
	});

	It("lists the owned weapons the loadout doesn't hold for a new entry, a full class refused (B-476b)", [this]()
	{
		const FWeaponInventoryEntry Held = LoadoutV2TestWeapon(TEXT("Ashen Blade"), EWeaponType::Sword);
		const FWeaponInventoryEntry Spare = LoadoutV2TestWeapon(TEXT("Quarry Maul"), EWeaponType::Hammer);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = Held;
		const TArray<FLoadoutV2MainHandChoice> Generic = WoRLoadoutV2::BuildMainHandChoices(Loadout, ECharacterClass::Generic, {Held, Spare});
		if (TestEqual(TEXT("only the spare"), Generic.Num(), 1))
		{
			TestEqual(TEXT("the maul"), Generic[0].Weapon.PersistentID, Spare.PersistentID);
			TestTrue(TEXT("a Generic has room for a second weapon"), Generic[0].Refusal.IsEmpty());
		}
		const TArray<FLoadoutV2MainHandChoice> Refractor = WoRLoadoutV2::BuildMainHandChoices(Loadout, ECharacterClass::Caster, {Held, Spare});
		if (TestEqual(TEXT("still listed"), Refractor.Num(), 1))
		{
			TestFalse(TEXT("a Refractor's one entry is taken: refused with the reason"), Refractor[0].Refusal.IsEmpty());
		}
	});
	It("asks Weapon / Adapter / Evolution, greying a kind with no free place (B-476c)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("A"), EWeaponType::Sword)), Refusal);
		const TArray<FLoadoutV2KindChoice> Kinds = WoRLoadoutV2::BuildAddKinds(Loadout, ECharacterClass::Generic);
		if (!TestEqual(TEXT("three kinds"), Kinds.Num(), 3))
		{
			return;
		}
		TestTrue(TEXT("a second weapon fits"), Kinds[0].Refusal.IsEmpty());
		TestTrue(TEXT("an adapter fits"), Kinds[1].Refusal.IsEmpty());
		TestFalse(TEXT("the evolution needs the first entry free"), Kinds[2].Refusal.IsEmpty());

		FCombatLoadout Refractor;
		WoRLoadoutV2::ApplyDraft(Refractor, ECharacterClass::Caster, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Staff"), EWeaponType::Staff)), Refusal);
		for (const FLoadoutV2KindChoice &Kind : WoRLoadoutV2::BuildAddKinds(Refractor, ECharacterClass::Caster))
		{
			TestFalse(TEXT("a Refractor's one entry is taken: every kind refused"), Kind.Refusal.IsEmpty());
		}
	});

	It("lists adapters the loadout doesn't hold and evolutions, one in use refused (B-476c)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Resonator, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("A"), EWeaponType::Sword)), Refusal);
		const FRingInventoryEntry Worn = LoadoutV2TestAdapter();
		FLoadoutV2Draft WornDraft;
		WornDraft.Kind = ELoadoutV2DraftKind::Adapter;
		WornDraft.Adapter = Worn;
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Resonator, WornDraft, Refusal);
		const FRingInventoryEntry Spare = LoadoutV2TestAdapter();
		const TArray<FLoadoutV2PieceChoice> Adapters = WoRLoadoutV2::BuildAdapterChoices(Loadout, ECharacterClass::Resonator, {Worn, Spare});
		if (TestEqual(TEXT("only the spare"), Adapters.Num(), 1))
		{
			TestEqual(TEXT("the spare ring"), Adapters[0].Adapter.PersistentID, Spare.PersistentID);
			TestTrue(TEXT("a Resonator has room"), Adapters[0].Refusal.IsEmpty());
		}

		FEvolutionInventoryEntry Free;
		Free.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		Free.InstanceID = FGuid::NewGuid();
		FEvolutionInventoryEntry Busy = Free;
		Busy.InstanceID = FGuid::NewGuid();
		FCombatLoadout Empty;
		const TArray<FLoadoutV2PieceChoice> Evolutions = WoRLoadoutV2::BuildEvolutionChoices(Empty, ECharacterClass::Generic, {Free, Busy}, {Busy.InstanceID});
		if (TestEqual(TEXT("both listed"), Evolutions.Num(), 2))
		{
			TestTrue(TEXT("the free one fits"), Evolutions[0].Refusal.IsEmpty());
			TestEqual(TEXT("it uses 2 cells"), Evolutions[0].CellsAfter, LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST);
			TestFalse(TEXT("the one in use is greyed"), Evolutions[1].Refusal.IsEmpty());
		}
	});

	It("edits an entry in its place: an off hand added, a twin-only main hand left alone refused (B-476c)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		const FWeaponInventoryEntry Twin = LoadoutV2TestWeapon(TEXT("Twin"), EWeaponType::Sword, EWeaponHandling::DualOnly);
		const FWeaponInventoryEntry Blade = LoadoutV2TestWeapon(TEXT("Blade"), EWeaponType::Sword);
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(Blade), Refusal);
		const FLoadoutV2Entry Entry = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries[0];
		FLoadoutV2Draft Draft = WoRLoadoutV2::DraftFromEntry(Loadout, Entry);
		TestEqual(TEXT("the draft holds the entry's main hand"), Draft.MainHand.PersistentID, Blade.PersistentID);
		Draft.OffHand = Twin;
		int32 Cells = 0;
		TestTrue(TEXT("two swords pair"), WoRLoadoutV2::CheckEntryEdit(Loadout, ECharacterClass::Generic, Entry, Draft, Cells).IsEmpty());
		TestTrue(TEXT("it goes in"), WoRLoadoutV2::ReplaceEntry(Loadout, ECharacterClass::Generic, Entry, Draft, Refusal));
		TestEqual(TEXT("one entry, now a pair"), WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries[0].Name.ToString(),
				  FString(TEXT("Blade + Twin")));

		FLoadoutV2Draft TwinAlone = WoRLoadoutV2::DraftFromEntry(Loadout, Entry);
		TwinAlone.MainHand = Twin;
		TwinAlone.OffHand = FWeaponInventoryEntry();
		TestFalse(TEXT("the twin alone is refused"), WoRLoadoutV2::CheckEntryEdit(Loadout, ECharacterClass::Generic, Entry, TwinAlone, Cells).IsEmpty());

		FLoadoutV2Draft Adapter;
		Adapter.Kind = ELoadoutV2DraftKind::Adapter;
		Adapter.Adapter = LoadoutV2TestAdapter();
		TestFalse(TEXT("an adapter can't take a weapon entry's place"), WoRLoadoutV2::ReplaceEntry(Loadout, ECharacterClass::Generic, Entry, Adapter, Refusal));
	});

	It("takes an entry out, keeping the first (D-116), and removes all gear but the first (B-476c)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("First"), EWeaponType::Sword)), Refusal);
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Second"), EWeaponType::Hammer)), Refusal);
		FLoadoutV2Draft Adapter;
		Adapter.Kind = ELoadoutV2DraftKind::Adapter;
		Adapter.Adapter = LoadoutV2TestAdapter();
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Adapter, Refusal);

		FCombatLoadout Copy = Loadout;
		const FLoadoutV2Entry First = WoRLoadoutV2::BuildWeaponsPage(Copy, ECharacterClass::Generic).Entries[0];
		TestTrue(TEXT("the first goes: the second moves up"), WoRLoadoutV2::RemoveEntry(Copy, ECharacterClass::Generic, First, Refusal));
		TestEqual(TEXT("the second is first now"), Copy.PrimaryWeapon.WeaponEntry.Weapon ? Copy.PrimaryWeapon.WeaponEntry.Weapon->GetDisplayName() : FString(),
				  FString(TEXT("Second")));
		const FLoadoutV2Entry Last = WoRLoadoutV2::BuildWeaponsPage(Copy, ECharacterClass::Generic).Entries[0];
		FCombatLoadout Alone = Copy;
		Alone.RingLoadout.Reset();
		TestFalse(TEXT("the only weapon entry stays"), WoRLoadoutV2::RemoveEntry(Alone, ECharacterClass::Generic, Last, Refusal));
		TestFalse(TEXT("with the reason"), Refusal.IsEmpty());

		TestEqual(TEXT("Remove all gear: the second weapon and the adapter"), WoRLoadoutV2::RemoveAllGear(Loadout, ECharacterClass::Generic), 2);
		TestEqual(TEXT("one entry left"), WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries.Num(), 1);
	});

	It("builds the details box: the piece, its stats, sockets and skill slots; the simple view drops the built-ins (B-476c)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		FWeaponInventoryEntry Maul = LoadoutV2TestWeapon(TEXT("Quarry Maul"), EWeaponType::Hammer);
		Maul.Weapon->Power = 50;
		Maul.Weapon->PresetAbilities = {NewObject<UAbilityData>(GetTransientPackage())};
		FLoadoutV2Draft Draft = LoadoutV2WeaponDraft(Maul);
		Draft.Socket2 = LoadoutV2TestStone();
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Draft, Refusal);
		const FLoadoutV2Entry Entry = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries[0];
		const TArray<FLoadoutV2DetailLine> Full = WoRLoadoutV2::BuildEntryDetails(Loadout, Entry, false);
		const auto Count = [](const TArray<FLoadoutV2DetailLine> &Lines, ELoadoutV2DetailKind Kind)
		{
			return Lines.FilterByPredicate([Kind](const FLoadoutV2DetailLine &Line) { return Line.Kind == Kind; }).Num();
		};
		TestEqual(TEXT("one piece"), Count(Full, ELoadoutV2DetailKind::Piece), 1);
		TestEqual(TEXT("two sockets"), Count(Full, ELoadoutV2DetailKind::Socket), 2);
		TestEqual(TEXT("the skill slots"), Count(Full, ELoadoutV2DetailKind::SkillSlots), 1);
		TestTrue(TEXT("power shown"), Full.ContainsByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Value.ToString() == TEXT("50"); }));
		const TArray<FLoadoutV2DetailLine> Simple = WoRLoadoutV2::BuildEntryDetails(Loadout, Entry, true);
		TestTrue(TEXT("the simple view is shorter"), Simple.Num() < Full.Num());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
