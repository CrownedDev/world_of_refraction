// LoadoutV2ModelSpec.cpp
// B-476 (D-206, D-209, D-210; UIScreens.md #2-3), first part: the v2 loadout screen's model. The Weapons tab lists the
// entries by name (a pair as "A and B", B-513b), their two sockets as icons and the weapon in hand; only "Cells used n of 5" and
// the Add weapon frame's "n/5 left" show the cells. The Add gear window's draft is refused in the game's words (a pair
// of two types, a twin-only weapon alone, an evolution where the class can't put one, no free entry, over the cells)
// and goes into the next free place; Compare sets built-ins gained beside lost.
// B-476c: the first question (Weapon / Adapter / Evolution) greys a kind with no free place; the Adapter and Evolution
// lists; an entry edited in its place (a new off hand, a twin-only main hand left alone refused), taken out (the first
// entry stays, D-116; a Generic's second weapon moves up), Remove all gear; the details box, full and simple.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/WeaponSetRules.h"
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
		TestEqual(TEXT("names only, a pair as A + B (B-513b)"), Page.Entries[0].Name.ToString(), FString(TEXT("Ashen Blade + Cinder Edge")));
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

	It("gives an adapter entry the frame icon with its crystal inside, or empty (B-530, D-229)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		FLoadoutV2Draft Adapter;
		Adapter.Kind = ELoadoutV2DraftKind::Adapter;
		Adapter.Adapter = LoadoutV2TestAdapter();
		TestTrue(TEXT("an empty adapter goes in"), WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Adapter, Refusal));
		const FLoadoutV2WeaponsPage Page = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic);
		const FLoadoutV2Entry *Entry = Page.Entries.FindByPredicate([](const FLoadoutV2Entry &E) { return E.Kind == ELoadoutV2EntryKind::Adapter; });
		if (!TestNotNull(TEXT("the adapter's entry"), Entry))
		{
			return;
		}
		TestTrue(TEXT("the frame, empty inside"), Entry->TypeIcon.Kind == ELoadoutV2IconKind::Adapter && Entry->TypeIcon.Element == ESpellElement::None);
		FRingInventoryEntry Filled = LoadoutV2TestAdapter();
		Filled.AttachedItem.Kind = EAttachedItemKind::Crystal;
		constexpr int32 LIVE_DURABILITY = 10;
		Filled.AttachedItem.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), LIVE_DURABILITY);
		const FLoadoutV2Icon Fire = WoRLoadoutV2::MakeAdapterIcon(Filled);
		TestTrue(TEXT("a Garnet inside: Fire's badge in the frame"), Fire.Kind == ELoadoutV2IconKind::Adapter && Fire.Element == ESpellElement::Fire);
		TestFalse(TEXT("named as the adapter"), Fire.Name.IsEmpty());
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

	It("compares no built-ins: gear has none (B-518, D-224)", [this]()
	{
		FWeaponInventoryEntry Equipped = LoadoutV2TestWeapon(TEXT("Cinder Edge"), EWeaponType::Sword);
		FWeaponInventoryEntry Selected = LoadoutV2TestWeapon(TEXT("Frost Fang"), EWeaponType::Sword);
		UAbilityData *Shared = NewObject<UAbilityData>(GetTransientPackage());
		UAbilityData *Burn = NewObject<UAbilityData>(GetTransientPackage());
		UAbilityData *Chill = NewObject<UAbilityData>(GetTransientPackage());
		Equipped.Weapon->PresetAbilities = {Shared, Burn};
		Selected.Weapon->PresetAbilities = {Shared, Chill};
		const FLoadoutV2Compare Compare = WoRLoadoutV2::BuildCompare(Equipped, Selected);
		TestEqual(TEXT("none gained"), Compare.BuiltInsGained.Num(), 0);
		TestEqual(TEXT("none lost"), Compare.BuiltInsLost.Num(), 0);
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
		TestEqual(TEXT("B-503u (D-220): Add a weapon first"), Kinds[0].Name.ToString(), FString(TEXT("Add a weapon")));
		TestEqual(TEXT("then Add an adapter"), Kinds[1].Name.ToString(), FString(TEXT("Add an adapter")));

		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("B"), EWeaponType::Axe)), Refusal);
		const TArray<FLoadoutV2KindChoice> TwoWeapons = WoRLoadoutV2::BuildAddKinds(Loadout, ECharacterClass::Generic);
		TestEqual(TEXT("two weapons: greyed with the class limit"), TwoWeapons[0].Refusal.ToString(), FString(TEXT("A Generic holds at most 2 weapons.")));
		TestTrue(TEXT("the adapter still fits"), TwoWeapons[1].Refusal.IsEmpty());
		FLoadoutV2Draft Ring;
		Ring.Kind = ELoadoutV2DraftKind::Adapter;
		Ring.Adapter = LoadoutV2TestAdapter();
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Ring, Refusal);
		TestEqual(TEXT("three pieces: every kind says the piece limit"), WoRLoadoutV2::BuildAddKinds(Loadout, ECharacterClass::Generic)[1].Refusal.ToString(),
				  FString(TEXT("A Generic holds at most 3 pieces.")));

		FCombatLoadout Refractor;
		WoRLoadoutV2::ApplyDraft(Refractor, ECharacterClass::Caster, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Staff"), EWeaponType::Staff)), Refusal);
		for (const FLoadoutV2KindChoice &Kind : WoRLoadoutV2::BuildAddKinds(Refractor, ECharacterClass::Caster))
		{
			TestFalse(TEXT("a Refractor's one entry is taken: every kind refused"), Kind.Refusal.IsEmpty());
		}
		TestEqual(TEXT("B-503u: with its class limit"), WoRLoadoutV2::BuildAddKinds(Refractor, ECharacterClass::Caster)[0].Refusal.ToString(),
				  FString(TEXT("A Refractor holds at most 1 piece.")));
		FCombatLoadout Resonator;
		WoRLoadoutV2::ApplyDraft(Resonator, ECharacterClass::Resonator, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("A"), EWeaponType::Sword)), Refusal);
		const TArray<FLoadoutV2KindChoice> ResonatorKinds = WoRLoadoutV2::BuildAddKinds(Resonator, ECharacterClass::Resonator);
		TestEqual(TEXT("a Resonator's one weapon"), ResonatorKinds[0].Refusal.ToString(), FString(TEXT("A Resonator holds at most 1 weapon.")));
		TestTrue(TEXT("its adapters still fit"), ResonatorKinds[1].Refusal.IsEmpty());
	});

	It("gives the Edit window's box the lit slot's piece, or what an empty slot takes (B-503u)", [this]()
	{
		const TArray<FLoadoutV2DetailLine> EmptyFirst = WoRLoadoutV2::BuildSocketDetails(FRuntimeAttachedItem(), /*bFirstSocket=*/true, false);
		TestTrue(TEXT("an empty socket 1: one line, what it takes"), EmptyFirst.Num() == 1 && EmptyFirst[0].Value.ToString().Contains(TEXT("crystal")));
		const TArray<FLoadoutV2DetailLine> EmptySecond = WoRLoadoutV2::BuildSocketDetails(FRuntimeAttachedItem(), false, false);
		TestTrue(TEXT("an empty socket 2 takes a stone"), EmptySecond.Num() == 1 && EmptySecond[0].Value.ToString().Contains(TEXT("stone")));
		FRuntimeAttachedItem Garnet;
		Garnet.Kind = EAttachedItemKind::Crystal;
		constexpr int32 LIVE_DURABILITY = 10;
		Garnet.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), LIVE_DURABILITY);
		TestEqual(TEXT("a filled socket: the crystal's own details"), WoRLoadoutV2::GetDetailsString(WoRLoadoutV2::BuildSocketDetails(Garnet, true, false)),
				  WoRLoadoutV2::GetDetailsString(WoRLoadoutV2::BuildAttachmentDetails(Garnet, false)));
		TestEqual(TEXT("an empty off hand: one line"), WoRLoadoutV2::BuildOffHandDetails(FWeaponInventoryEntry(), false).Num(), 1);
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

	It("a new main hand takes the off hand off when the two no longer pair; a shield as main hand stays refused (Crown, 2026-11-18)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		const FWeaponInventoryEntry Blade = LoadoutV2TestWeapon(TEXT("Blade"), EWeaponType::Sword);
		const FWeaponInventoryEntry Edge = LoadoutV2TestWeapon(TEXT("Edge"), EWeaponType::Sword);
		const FWeaponInventoryEntry Sabre = LoadoutV2TestWeapon(TEXT("Sabre"), EWeaponType::Sword);
		const FWeaponInventoryEntry Claymore = LoadoutV2TestWeapon(TEXT("Claymore"), EWeaponType::Greatsword, EWeaponHandling::SingleOnly);
		const FWeaponInventoryEntry Hatchet = LoadoutV2TestWeapon(TEXT("Hatchet"), EWeaponType::Axe);
		const FWeaponInventoryEntry Shield = LoadoutV2TestWeapon(TEXT("Shield"), EWeaponType::Shield, EWeaponHandling::DualOnly); // as the shield assets are
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(Blade, Edge), Refusal);
		const FLoadoutV2Entry Entry = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries[0];
		const FLoadoutV2Draft Pair = WoRLoadoutV2::DraftFromEntry(Loadout, Entry);
		int32 Cells = 0;

		FLoadoutV2Draft Draft = Pair;
		TestTrue(TEXT("a weapon that can't be dual wielded: the off hand comes off"), WoRLoadoutV2::SetMainHand(Draft, Claymore));
		TestTrue(TEXT("the claymore is the main hand, alone"), Draft.MainHand.PersistentID == Claymore.PersistentID && !Draft.OffHand.IsValid());
		TestTrue(TEXT("and it can be confirmed"), WoRLoadoutV2::CheckEntryEdit(Loadout, ECharacterClass::Generic, Entry, Draft, Cells).IsEmpty());
		FCombatLoadout After = Loadout;
		TestTrue(TEXT("it goes in"), WoRLoadoutV2::ReplaceEntry(After, ECharacterClass::Generic, Entry, Draft, Refusal));
		TestEqual(TEXT("the entry is the claymore alone"), WoRLoadoutV2::BuildWeaponsPage(After, ECharacterClass::Generic).Entries[0].Name.ToString(), FString(TEXT("Claymore")));

		Draft = Pair;
		TestTrue(TEXT("another type: the off hand comes off"), WoRLoadoutV2::SetMainHand(Draft, Hatchet));
		TestFalse(TEXT("the axe alone"), Draft.OffHand.IsValid());

		Draft = Pair;
		TestFalse(TEXT("the same type that pairs: the off hand stays"), WoRLoadoutV2::SetMainHand(Draft, Sabre));
		TestEqual(TEXT("still the old off hand"), Draft.OffHand.PersistentID, Edge.PersistentID);

		Draft = Pair;
		TestFalse(TEXT("a shield as the main hand takes nothing off"), WoRLoadoutV2::SetMainHand(Draft, Shield));
		TestFalse(TEXT("and stays refused"), WoRLoadoutV2::CheckEntryEdit(Loadout, ECharacterClass::Generic, Entry, Draft, Cells).IsEmpty());

		FLoadoutV2Draft Alone = LoadoutV2WeaponDraft(Blade);
		TestFalse(TEXT("no off hand: nothing to take off"), WoRLoadoutV2::SetMainHand(Alone, Claymore));

		// A shield's line says it is an off-hand piece, with or without an off hand set (not "Twin only").
		const FString ShieldWords = WeaponSetRules::GetPairRefusalText(WeaponSetRules::EPairRefusal::ShieldInMainHand).ToString();
		FCombatLoadout Empty;
		TestEqual(TEXT("a shield alone as the main hand"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, LoadoutV2WeaponDraft(Shield), Cells).ToString(), ShieldWords);
		TestEqual(TEXT("a shield over an off hand"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, LoadoutV2WeaponDraft(Shield, Edge), Cells).ToString(), ShieldWords);
		TestTrue(TEXT("a sword with the shield in the off hand goes in"), WoRLoadoutV2::CheckDraft(Empty, ECharacterClass::Generic, LoadoutV2WeaponDraft(Blade, Shield), Cells).IsEmpty());
		const TArray<FLoadoutV2OffHandChoice> UnderClaymore = WoRLoadoutV2::BuildOffHandChoices(Claymore, {Shield}, ECharacterClass::Generic);
		TestTrue(TEXT("under a main hand that can't pair, the shield's line names the main hand"),
				 UnderClaymore.Num() == 1 && UnderClaymore[0].Refusal.ToString() == TEXT("Claymore takes no off hand."));

		TestEqual(TEXT("a weapon row names its type after its name"), WoRLoadoutV2::GetWeaponRowName(FText::FromString(TEXT("Blade")), Blade.Weapon).ToString(),
				  FString(TEXT("Blade · Sword")));
		TestEqual(TEXT("no weapon: the bare name"), WoRLoadoutV2::GetWeaponRowName(FText::FromString(TEXT("Blade")), nullptr).ToString(), FString(TEXT("Blade")));
	});

	It("Edit offers the other kind; it replaces the entry only when confirmed, cancel leaves it (B-531, D-230)", [this]()
	{
		FCombatLoadout Loadout;
		FText Refusal;
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Blade"), EWeaponType::Sword)), Refusal);
		WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Maul"), EWeaponType::Hammer)), Refusal);
		const FLoadoutV2Entry Maul = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries[1];
		const FString Before = WoRLoadoutV2::GetWeaponsPageString(WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic));

		const TArray<FLoadoutV2KindChoice> Kinds = WoRLoadoutV2::BuildEditKinds(Loadout, ECharacterClass::Generic, Maul);
		const FLoadoutV2KindChoice *WeaponKind = Kinds.FindByPredicate([](const FLoadoutV2KindChoice &Kind) { return Kind.Kind == ELoadoutV2DraftKind::Weapon; });
		const FLoadoutV2KindChoice *AdapterKind = Kinds.FindByPredicate([](const FLoadoutV2KindChoice &Kind) { return Kind.Kind == ELoadoutV2DraftKind::Adapter; });
		TestTrue(TEXT("its own kind is open"), WeaponKind && WeaponKind->Refusal.IsEmpty());
		TestTrue(TEXT("an adapter can take its place"), AdapterKind && AdapterKind->Refusal.IsEmpty());

		// Cancel: the window checked a draft of the other kind and closed; nothing was written.
		FLoadoutV2Draft Adapter;
		Adapter.Kind = ELoadoutV2DraftKind::Adapter;
		int32 Cells = 0;
		TestFalse(TEXT("no adapter picked yet: refused"), WoRLoadoutV2::CheckEntryEdit(Loadout, ECharacterClass::Generic, Maul, Adapter, Cells).IsEmpty());
		TestFalse(TEXT("and not written"), WoRLoadoutV2::ReplaceEntry(Loadout, ECharacterClass::Generic, Maul, Adapter, Refusal));
		TestEqual(TEXT("cancel: the entry is as it was"), WoRLoadoutV2::GetWeaponsPageString(WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic)), Before);

		// Confirm: the adapter replaces the Maul.
		Adapter.Adapter = LoadoutV2TestAdapter();
		TestTrue(TEXT("the adapter fits"), WoRLoadoutV2::CheckEntryEdit(Loadout, ECharacterClass::Generic, Maul, Adapter, Cells).IsEmpty());
		TestTrue(TEXT("confirmed: it goes in"), WoRLoadoutV2::ReplaceEntry(Loadout, ECharacterClass::Generic, Maul, Adapter, Refusal));
		const FLoadoutV2WeaponsPage After = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic);
		TestFalse(TEXT("the Maul is out"), Loadout.SecondaryWeapon.IsValid());
		TestTrue(TEXT("the adapter is in"), After.Entries.ContainsByPredicate([](const FLoadoutV2Entry &Entry) { return Entry.Kind == ELoadoutV2EntryKind::Adapter; }));

		// The first entry keeps a weapon (D-116): a Generic's only weapon can't become an adapter.
		FCombatLoadout Alone;
		WoRLoadoutV2::ApplyDraft(Alone, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Blade"), EWeaponType::Sword)), Refusal);
		const FLoadoutV2Entry First = WoRLoadoutV2::BuildWeaponsPage(Alone, ECharacterClass::Generic).Entries[0];
		const TArray<FLoadoutV2KindChoice> FirstKinds = WoRLoadoutV2::BuildEditKinds(Alone, ECharacterClass::Generic, First);
		const FLoadoutV2KindChoice *FirstAdapter = FirstKinds.FindByPredicate([](const FLoadoutV2KindChoice &Kind) { return Kind.Kind == ELoadoutV2DraftKind::Adapter; });
		TestTrue(TEXT("the adapter kind is greyed with why"), FirstAdapter && !FirstAdapter->Refusal.IsEmpty());
		TestFalse(TEXT("and refused"), WoRLoadoutV2::ReplaceEntry(Alone, ECharacterClass::Generic, First, Adapter, Refusal));
		TestTrue(TEXT("the weapon stays"), Alone.PrimaryWeapon.IsValid());
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

	It("builds the details box: the piece, its stats, sockets and skill slots, and no built-ins (B-476c, B-518)", [this]()
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
		TestFalse(TEXT("no Built-in line"), Full.ContainsByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Label.ToString() == TEXT("Built-in"); }));
		const TArray<FLoadoutV2DetailLine> Simple = WoRLoadoutV2::BuildEntryDetails(Loadout, Entry, true);
		TestTrue(TEXT("the simple view is no longer"), Simple.Num() <= Full.Num());
	});

	It("gives one piece's own details for the Add gear window's box: a weapon, a crystal, nothing for nothing (B-484)", [this]()
	{
		constexpr int32 PIECE_TEST_DURABILITY = 5;
		FWeaponInventoryEntry Maul = LoadoutV2TestWeapon(TEXT("Quarry Maul"), EWeaponType::Hammer);
		Maul.Weapon->Power = 50;
		Maul.Weapon->PresetAbilities = {NewObject<UAbilityData>(GetTransientPackage())}; // B-518: a built-in no view lists
		Maul.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Maul.AttachedItem.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), PIECE_TEST_DURABILITY);
		const auto Count = [](const TArray<FLoadoutV2DetailLine> &Lines, ELoadoutV2DetailKind Kind)
		{
			return Lines.FilterByPredicate([Kind](const FLoadoutV2DetailLine &Line) { return Line.Kind == Kind; }).Num();
		};
		const TArray<FLoadoutV2DetailLine> Weapon = WoRLoadoutV2::BuildWeaponDetails(Maul, false);
		TestEqual(TEXT("the weapon alone: one piece"), Count(Weapon, ELoadoutV2DetailKind::Piece), 1);
		TestTrue(TEXT("named"), Weapon.Num() > 0 && Weapon[0].Label.ToString() == TEXT("Quarry Maul"));
		TestTrue(TEXT("its power"), Weapon.ContainsByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Value.ToString() == TEXT("50"); }));
		TestEqual(TEXT("its own two sockets"), Count(Weapon, ELoadoutV2DetailKind::Socket), 2);
		TestEqual(TEXT("its skill slots"), Count(Weapon, ELoadoutV2DetailKind::SkillSlots), 1);
		TestTrue(TEXT("the weapon's simple view is no longer"), WoRLoadoutV2::BuildWeaponDetails(Maul, true).Num() <= Weapon.Num());

		// The same weapon as a one-weapon entry reads the same in the Weapons tab's box.
		FCombatLoadout Loadout;
		FText Refusal;
		FLoadoutV2Draft Draft = LoadoutV2WeaponDraft(Maul);
		Draft.Socket1 = Maul.AttachedItem;
		if (TestTrue(TEXT("the maul goes in"), WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, Draft, Refusal)))
		{
			const FLoadoutV2Entry Entry = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic).Entries[0];
			TestEqual(TEXT("the same lines as its entry's box"), WoRLoadoutV2::GetDetailsString(Weapon),
					  WoRLoadoutV2::GetDetailsString(WoRLoadoutV2::BuildEntryDetails(Loadout, Entry, false)));
		}

		const TArray<FLoadoutV2DetailLine> Gem = WoRLoadoutV2::BuildAttachmentDetails(Maul.AttachedItem, false);
		TestEqual(TEXT("a crystal: one piece"), Count(Gem, ELoadoutV2DetailKind::Piece), 1);
		TestTrue(TEXT("with what it costs"), Gem.ContainsByPredicate([](const FLoadoutV2DetailLine &Line)
																	  { return Line.Value.ToString() == FString::FromInt(LoadoutConstants::LOADOUT_CRYSTAL_COST); }));
		TestTrue(TEXT("the crystal's simple view is shorter"), WoRLoadoutV2::BuildAttachmentDetails(Maul.AttachedItem, true).Num() < Gem.Num());
		TestTrue(TEXT("an empty socket: nothing"), WoRLoadoutV2::BuildAttachmentDetails(FRuntimeAttachedItem(), false).IsEmpty());
		TestTrue(TEXT("no weapon: nothing"), WoRLoadoutV2::BuildWeaponDetails(FWeaponInventoryEntry(), false).IsEmpty());
		TestTrue(TEXT("no adapter: nothing"), WoRLoadoutV2::BuildAdapterDetails(FRingInventoryEntry(), false).IsEmpty());
		TestFalse(TEXT("an adapter: its piece"), WoRLoadoutV2::BuildAdapterDetails(LoadoutV2TestAdapter(), false).IsEmpty());
	});

	It("names what the socket holds the same on the row and in the box; nothing for an empty socket (B-482b)", [this]()
	{
		constexpr int32 SOCKET_TEST_DURABILITY = 5;
		FCombatLoadout Loadout;
		FText Refusal;
		FLoadoutV2Draft WithGem = LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Ashen Blade"), EWeaponType::Sword));
		WithGem.Socket1.Kind = EAttachedItemKind::Crystal;
		WithGem.Socket1.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), SOCKET_TEST_DURABILITY);
		TestTrue(TEXT("the sword with a Garnet goes in"), WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, WithGem, Refusal));
		TestTrue(TEXT("a bare hammer goes in"),
				 WoRLoadoutV2::ApplyDraft(Loadout, ECharacterClass::Generic, LoadoutV2WeaponDraft(LoadoutV2TestWeapon(TEXT("Quarry Maul"), EWeaponType::Hammer)), Refusal));
		const FLoadoutV2WeaponsPage Page = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic);
		if (!TestEqual(TEXT("two entries"), Page.Entries.Num(), 2))
		{
			return;
		}
		const FString RowName = Page.Entries[0].SocketName.ToString();
		TestFalse(TEXT("the row names the gem"), RowName.IsEmpty());
		TestTrue(TEXT("the icon agrees: a crystal"), Page.Entries[0].Sockets[0] == ELoadoutV2SocketIcon::Crystal);
		const TArray<FLoadoutV2DetailLine> Box = WoRLoadoutV2::BuildEntryDetails(Loadout, Page.Entries[0], false);
		const FLoadoutV2DetailLine *SocketLine = Box.FindByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Kind == ELoadoutV2DetailKind::Socket; });
		if (TestNotNull(TEXT("the box's first socket line"), SocketLine))
		{
			TestTrue(TEXT("the box names the same gem"), SocketLine->Value.ToString().StartsWith(RowName));
		}
		TestTrue(TEXT("an empty socket: no name on the row"), Page.Entries[1].SocketName.IsEmpty());
		TestTrue(TEXT("and an empty icon"), Page.Entries[1].Sockets[0] == ELoadoutV2SocketIcon::Empty);

		// B-485: what the row and the box show as icons.
		TestTrue(TEXT("the row's weapon-type icon"), Page.Entries[0].TypeIcon.Kind == ELoadoutV2IconKind::WeaponType && Page.Entries[0].TypeIcon.WeaponType == EWeaponType::Sword);
		TestTrue(TEXT("its two socket badges: the gem's element, then the empty socket"),
				 Page.Entries[0].SocketBadges.Num() == 2 && Page.Entries[0].SocketBadges[0].Kind == ELoadoutV2IconKind::Element &&
					 Page.Entries[0].SocketBadges[0].Name.ToString() == RowName && Page.Entries[0].SocketBadges[1].Kind == ELoadoutV2IconKind::EmptySocket);
		TestTrue(TEXT("an empty socket's badge says so"), Page.Entries[1].SocketBadges[0].Kind == ELoadoutV2IconKind::EmptySocket && !Page.Entries[1].SocketBadges[0].Name.IsEmpty());
		const FLoadoutV2DetailLine *PieceLine = Box.FindByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Kind == ELoadoutV2DetailKind::Piece; });
		TestTrue(TEXT("the box's piece line: the weapon-type icon and a tier for its badge"),
				 PieceLine && PieceLine->Icon.Kind == ELoadoutV2IconKind::WeaponType && PieceLine->bHasTier);
		TestTrue(TEXT("the box's socket line: the gem's badge"), SocketLine && SocketLine->Icon.Kind == ELoadoutV2IconKind::Element);
		// B-487b: the piece comes first (it is the box's header) and says its type itself; no "Type" row.
		TestTrue(TEXT("the piece line is first and carries its type"), Box.Num() > 0 && Box[0].Kind == ELoadoutV2DetailKind::Piece && !Box[0].TypeName.IsEmpty() && Box[0].TypeName.EqualTo(Box[0].Icon.Name));
		TestFalse(TEXT("no Type row"), Box.ContainsByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Label.ToString() == TEXT("Type"); }));
		TestTrue(TEXT("a filled socket line carries its tier"), SocketLine && SocketLine->bHasTier && SocketLine->Tier == EItemTier::F_Tier);
		TestTrue(TEXT("an empty socket line has none"), Box.ContainsByPredicate([](const FLoadoutV2DetailLine &Line)
																		   { return Line.Kind == ELoadoutV2DetailKind::Socket && Line.Icon.Kind == ELoadoutV2IconKind::EmptySocket && !Line.bHasTier; }));
		const TArray<FLoadoutV2DetailLine> GemBox = WoRLoadoutV2::BuildAttachmentDetails(WithGem.Socket1, false);
		TestTrue(TEXT("a crystal's own box: its badge and its tier"),
				 GemBox.Num() > 0 && GemBox[0].Icon.Kind == ELoadoutV2IconKind::Element && GemBox[0].bHasTier && GemBox[0].Tier == EItemTier::F_Tier);
		EItemTier NoTier = EItemTier::S_Tier;
		TestFalse(TEXT("an empty socket has no tier"), WoRLoadoutV2::GetAttachmentTier(FRuntimeAttachedItem(), NoTier));
		TestTrue(TEXT("a stone's badge is the stone"), WoRLoadoutV2::MakeSocketIcon(LoadoutV2TestStone()).Kind == ELoadoutV2IconKind::Stone);

		// An evolution whose item is gone: the row still names it, as the box does.
		Loadout.PrimaryWeapon.WeaponEntry.AttachedItem = FRuntimeAttachedItem();
		Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Evolution;
		const FLoadoutV2WeaponsPage Lost = WoRLoadoutV2::BuildWeaponsPage(Loadout, ECharacterClass::Generic);
		if (TestTrue(TEXT("still an entry"), Lost.Entries.Num() > 0))
		{
			const FString LostName = Lost.Entries[0].SocketName.ToString();
			TestFalse(TEXT("an evolution with no item still has a name on the row"), LostName.IsEmpty());
			const TArray<FLoadoutV2DetailLine> LostBox = WoRLoadoutV2::BuildEntryDetails(Loadout, Lost.Entries[0], false);
			const FLoadoutV2DetailLine *LostLine = LostBox.FindByPredicate([](const FLoadoutV2DetailLine &Line) { return Line.Kind == ELoadoutV2DetailKind::Socket; });
			TestTrue(TEXT("and the box says the same"), LostLine && LostLine->Value.ToString().StartsWith(LostName));
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
