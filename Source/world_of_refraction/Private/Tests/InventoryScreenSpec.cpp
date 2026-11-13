// InventoryScreenSpec.cpp
// B-441 (D-181, D-177, D-195): the inventory screen. The model: each tab lists its own things (Gear: the member's
// weapons and adapters, worn ones marked; Pouch: its stacks and its slots; Chest: what is stored), a tile's verb
// menu is Give and Store with Take only on the Chest tab, a verb that can't run carries its reason, and "Give to"
// says who has room. The screen, headless: A opens the verb menu, Give asks who, Back steps out one level at a time.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/InventoryScreenModel.h"
#include "UI/Menus/InventoryScreenWidget.h"

namespace
{
	constexpr int32 SCREEN_PARTY_SIZE = 2;
	constexpr int32 SCREEN_MEMBER = 0;
	constexpr int32 SCREEN_RECEIVER = 1;
	constexpr int32 SCREEN_GEM_COUNT = 3;
	const FCrystalId SCREEN_GEM(ECrystalType::Quartz, EItemTier::F_Tier);
	/** Two crystal kinds across every tier: more item types than a pouch has slots. */
	const TArray<ECrystalType> SCREEN_FILLER_KINDS = {ECrystalType::Garnet, ECrystalType::Sapphire};
	const TArray<EItemTier> SCREEN_TIERS = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier, EItemTier::C_Tier,
											EItemTier::B_Tier, EItemTier::A_Tier, EItemTier::S_Tier};
}

BEGIN_DEFINE_SPEC(FInventoryScreenSpec, "WoR.UI.InventoryScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	TArray<UCharacterData *> Members;
	TArray<UInventoryComponent *> Inventories;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }
	UWorld *World() const { return TestWorld->GetWorld(); }
	bool IsReady() const { return Pool && Inventories.Num() == SCREEN_PARTY_SIZE && !Inventories.Contains(nullptr); }

	/** Party member Index as each level spawns it: the sword as its primary, then the draw. */
	UInventoryComponent *SpawnMember(int32 Index)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Members[Index]);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	FGuid IDOf(const UWeaponData *Weapon) const
	{
		const FWeaponInventoryEntry *Entry =
			Inventories[SCREEN_MEMBER]->Weapons.FindByPredicate([Weapon](const FWeaponInventoryEntry &E) { return E.Weapon == Weapon; });
		return Entry ? Entry->PersistentID : FGuid();
	}

	FGuid AddDagger()
	{
		Inventories[SCREEN_MEMBER]->AddWeapon(Dagger);
		return IDOf(Dagger);
	}

	FInventoryScreenPage PageOf(EInventoryScreenTab Tab) const { return WoRInventoryScreen::BuildPage(World(), KeyOf(SCREEN_MEMBER), Tab); }

	static int32 FindGear(const FInventoryScreenPage &Page, const FGuid &GearID)
	{
		return Page.Tiles.IndexOfByPredicate([&GearID](const FInventoryScreenTile &Tile) { return Tile.Row.GearID == GearID; });
	}

	static const FInventoryScreenTile *FindKind(const FInventoryScreenPage &Page, EInventoryScreenKind Kind)
	{
		return Page.Tiles.FindByPredicate([Kind](const FInventoryScreenTile &Tile) { return Tile.Kind == Kind; });
	}

	TArray<FInventoryScreenVerb> VerbsOf(EInventoryScreenTab Tab, const FInventoryScreenTile &Tile) const
	{
		return WoRInventoryScreen::BuildVerbs(World(), KeyOf(SCREEN_MEMBER), Tab, Tile);
	}

	static const FInventoryScreenVerb *FindVerb(const TArray<FInventoryScreenVerb> &Verbs, EInventoryVerb Verb)
	{
		return Verbs.FindByPredicate([Verb](const FInventoryScreenVerb &Line) { return Line.Verb == Verb; });
	}
END_DEFINE_SPEC(FInventoryScreenSpec)

void FInventoryScreenSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Screen sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Screen dagger");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < SCREEN_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Screen member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Members.Add(Data);
		}
		for (int32 Index = 0; Index < SCREEN_PARTY_SIZE; ++Index)
		{
			Inventories.Add(SpawnMember(Index));
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
		Members.Empty();
		Inventories.Empty();
		TestWorld.Reset();
	});

	It("lists the member's gear on the Gear tab and marks the worn piece", [this]()
	{
		if (!IsReady()) return;
		const FGuid DaggerID = AddDagger();
		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const int32 SwordTile = FindGear(Gear, IDOf(Sword));
		const int32 DaggerTile = FindGear(Gear, DaggerID);
		if (!TestTrue(TEXT("both weapons are tiles"), SwordTile != INDEX_NONE && DaggerTile != INDEX_NONE)) return;
		TestTrue(TEXT("the loadout's sword is marked worn"), Gear.Tiles[SwordTile].bWorn);
		TestFalse(TEXT("the spare dagger is not"), Gear.Tiles[DaggerTile].bWorn);
		TestEqual(TEXT("named"), Gear.Tiles[DaggerTile].Name.ToString(), FString(TEXT("Screen dagger")));
		TestFalse(TEXT("a kind line"), Gear.Tiles[DaggerTile].KindLine.IsEmpty());
		TestTrue(TEXT("the pouch tab holds no gear"), PageOf(EInventoryScreenTab::Pouch).Tiles.IsEmpty());
		TestFalse(TEXT("an empty tab says so"), PageOf(EInventoryScreenTab::Chest).EmptyNote.IsEmpty());
	});

	It("lists Give and Store on a tile, and Take only on the Chest tab (D-195)", [this]()
	{
		if (!IsReady()) return;
		const FGuid DaggerID = AddDagger();
		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const int32 DaggerTile = FindGear(Gear, DaggerID);
		if (!TestTrue(TEXT("the dagger is a tile"), DaggerTile != INDEX_NONE)) return;
		const TArray<FInventoryScreenVerb> Own = VerbsOf(EInventoryScreenTab::Gear, Gear.Tiles[DaggerTile]);
		const FInventoryScreenVerb *Give = FindVerb(Own, EInventoryVerb::Give);
		const FInventoryScreenVerb *Store = FindVerb(Own, EInventoryVerb::Store);
		TestNull(TEXT("no Take on the Gear tab"), FindVerb(Own, EInventoryVerb::Take));
		if (!TestTrue(TEXT("Give and Store are listed"), Give && Store)) return;
		TestTrue(TEXT("a spare piece can be given and stored"), Give->Refusal.IsEmpty() && Store->Refusal.IsEmpty());

		TestTrue(TEXT("stored"), WoRInventoryVerbs::Run(World(), EInventoryVerb::Store, KeyOf(SCREEN_MEMBER), NAME_None, Gear.Tiles[DaggerTile].Row, 1).IsEmpty());
		const FInventoryScreenPage Chest = PageOf(EInventoryScreenTab::Chest);
		const int32 StoredTile = FindGear(Chest, DaggerID);
		if (!TestTrue(TEXT("the dagger is on the Chest tab"), StoredTile != INDEX_NONE)) return;
		TestEqual(TEXT("and off the Gear tab"), FindGear(PageOf(EInventoryScreenTab::Gear), DaggerID), static_cast<int32>(INDEX_NONE));
		const TArray<FInventoryScreenVerb> Stored = VerbsOf(EInventoryScreenTab::Chest, Chest.Tiles[StoredTile]);
		const FInventoryScreenVerb *Take = FindVerb(Stored, EInventoryVerb::Take);
		if (!TestNotNull(TEXT("Take is listed on the Chest tab"), Take)) return;
		TestTrue(TEXT("and can run"), Take->Refusal.IsEmpty());
		const FInventoryScreenVerb *StoredGive = FindVerb(Stored, EInventoryVerb::Give);
		const FInventoryScreenVerb *StoredStore = FindVerb(Stored, EInventoryVerb::Store);
		TestTrue(TEXT("Give and Store are greyed there, each with a reason"),
				 StoredGive && StoredStore && !StoredGive->Refusal.IsEmpty() && !StoredStore->Refusal.IsEmpty());
	});

	It("greys a verb with the code's reason: a worn primary isn't given or stored", [this]()
	{
		if (!IsReady()) return;
		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const int32 SwordTile = FindGear(Gear, IDOf(Sword));
		if (!TestTrue(TEXT("the sword is a tile"), SwordTile != INDEX_NONE)) return;
		const TArray<FInventoryScreenVerb> Verbs = VerbsOf(EInventoryScreenTab::Gear, Gear.Tiles[SwordTile]);
		const FInventoryScreenVerb *Give = FindVerb(Verbs, EInventoryVerb::Give);
		const FInventoryScreenVerb *Store = FindVerb(Verbs, EInventoryVerb::Store);
		if (!TestTrue(TEXT("both listed, not hidden"), Give && Store)) return;
		TestFalse(TEXT("Give says why not"), Give->Refusal.IsEmpty());
		TestFalse(TEXT("Store says why not"), Store->Refusal.IsEmpty());

		// In a run the chest is at home: Store names that instead.
		const FText AtHome = Store->Refusal;
		Pool->BeginRunLayer();
		const FInventoryScreenPage InRun = PageOf(EInventoryScreenTab::Gear);
		const int32 RunTile = FindGear(InRun, IDOf(Sword));
		if (!TestTrue(TEXT("the worn sword came on the run"), RunTile != INDEX_NONE)) return;
		const TArray<FInventoryScreenVerb> RunVerbs = VerbsOf(EInventoryScreenTab::Gear, InRun.Tiles[RunTile]);
		const FInventoryScreenVerb *RunStore = FindVerb(RunVerbs, EInventoryVerb::Store);
		if (!TestNotNull(TEXT("Store is still listed"), RunStore)) return;
		TestFalse(TEXT("refused in a run"), RunStore->Refusal.IsEmpty());
		TestFalse(TEXT("for the chest's own reason"), RunStore->Refusal.EqualTo(AtHome));

		// Q-441: a pouch stack isn't stored during a run either, and the press itself is refused.
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack carried"), Pouch.Tiles.Num(), 1)) return;
		const FInventoryScreenVerb *StackStore = FindVerb(VerbsOf(EInventoryScreenTab::Pouch, Pouch.Tiles[0]), EInventoryVerb::Store);
		TestTrue(TEXT("a stack's Store is greyed in a run"), StackStore && !StackStore->Refusal.IsEmpty());
		TestFalse(TEXT("and Run refuses it"),
				  WoRInventoryVerbs::Run(World(), EInventoryVerb::Store, KeyOf(SCREEN_MEMBER), NAME_None, Pouch.Tiles[0].Row, 1).IsEmpty());
		TestEqual(TEXT("nothing left the pouch"), Pool->GetCarriedCount(KeyOf(SCREEN_MEMBER), SCREEN_GEM), SCREEN_GEM_COUNT);
	});

	It("draws the pouch's slots and counts its stacks", [this]()
	{
		if (!IsReady()) return;
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack, one tile"), Pouch.Tiles.Num(), 1)) return;
		TestEqual(TEXT("its count"), Pouch.Tiles[0].Row.Count, SCREEN_GEM_COUNT);
		TestEqual(TEXT("the slots it starts with"), Pouch.Slots, InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("the rest are the Jeweler's"), Pouch.LockedSlots, InventoryConstants::ITEM_SLOTS_MAX - InventoryConstants::ITEM_SLOTS_START);
		TestFalse(TEXT("a heading with the slot count"), Pouch.Heading.IsEmpty());
		const FName Giver = KeyOf(SCREEN_MEMBER);
		const FName Receiver = KeyOf(SCREEN_RECEIVER);
		TestEqual(TEXT("one at a time"), WoRInventoryScreen::GetMoveCount(World(), EInventoryVerb::Store, Giver, NAME_None, Pouch.Tiles[0], /*bWholeStack=*/false), 1);
		TestEqual(TEXT("or the whole stack"), WoRInventoryScreen::GetMoveCount(World(), EInventoryVerb::Store, Giver, NAME_None, Pouch.Tiles[0], /*bWholeStack=*/true),
				  SCREEN_GEM_COUNT);

		// A whole-stack Give moves what fits the receiver's stack, not more.
		Pool->CarryOrStore(Receiver, SCREEN_GEM, InventoryConstants::ITEM_STACK_MAX - 1);
		TestEqual(TEXT("only what fits is given"), WoRInventoryScreen::GetMoveCount(World(), EInventoryVerb::Give, Giver, Receiver, Pouch.Tiles[0], /*bWholeStack=*/true), 1);
	});

	It("greys Take when the pouch has no room for the stack", [this]()
	{
		if (!IsReady()) return;
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack carried"), Pouch.Tiles.Num(), 1)) return;
		TestTrue(TEXT("stored"), WoRInventoryVerbs::Run(World(), EInventoryVerb::Store, KeyOf(SCREEN_MEMBER), NAME_None, Pouch.Tiles[0].Row, SCREEN_GEM_COUNT).IsEmpty());
		FInventoryScreenPage Chest = PageOf(EInventoryScreenTab::Chest);
		int32 Stored = Chest.Tiles.IndexOfByPredicate([](const FInventoryScreenTile &Tile) { return !Tile.Row.IsGear() && Tile.Row.Item == SCREEN_GEM; });
		if (!TestTrue(TEXT("the stack is on the Chest tab"), Stored != INDEX_NONE)) return;
		const FInventoryScreenVerb *Take = FindVerb(VerbsOf(EInventoryScreenTab::Chest, Chest.Tiles[Stored]), EInventoryVerb::Take);
		TestTrue(TEXT("Take can run with room"), Take && Take->Refusal.IsEmpty());

		for (const ECrystalType Kind : SCREEN_FILLER_KINDS)
		{
			for (const EItemTier Tier : SCREEN_TIERS)
			{
				Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), FCrystalId(Kind, Tier), 1);
			}
		}
		Chest = PageOf(EInventoryScreenTab::Chest);
		Stored = Chest.Tiles.IndexOfByPredicate([](const FInventoryScreenTile &Tile) { return !Tile.Row.IsGear() && Tile.Row.Item == SCREEN_GEM; });
		if (!TestTrue(TEXT("still on the Chest tab"), Stored != INDEX_NONE)) return;
		Take = FindVerb(VerbsOf(EInventoryScreenTab::Chest, Chest.Tiles[Stored]), EInventoryVerb::Take);
		TestTrue(TEXT("a full pouch greys Take with the reason"), Take && !Take->Refusal.IsEmpty());
	});

	It("says who has room in Give to, and refuses a full pouch", [this]()
	{
		if (!IsReady()) return;
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack"), Pouch.Tiles.Num(), 1)) return;
		TArray<FInventoryScreenReceiver> Receivers = WoRInventoryScreen::BuildReceivers(World(), KeyOf(SCREEN_MEMBER), Pouch.Tiles[0]);
		if (!TestEqual(TEXT("the other member only"), Receivers.Num(), SCREEN_PARTY_SIZE - 1)) return;
		TestEqual(TEXT("it is the receiver"), Receivers[0].Key, KeyOf(SCREEN_RECEIVER));
		TestTrue(TEXT("an empty pouch can receive"), Receivers[0].Refusal.IsEmpty());
		TestFalse(TEXT("and says its room"), Receivers[0].Note.IsEmpty());

		for (const ECrystalType Kind : SCREEN_FILLER_KINDS)
		{
			for (const EItemTier Tier : SCREEN_TIERS)
			{
				Pool->CarryOrStore(KeyOf(SCREEN_RECEIVER), FCrystalId(Kind, Tier), 1);
			}
		}
		TestEqual(TEXT("the receiver's pouch is full"), Pool->GetUsedItemSlots(KeyOf(SCREEN_RECEIVER)), Pool->GetItemSlotCount(KeyOf(SCREEN_RECEIVER)));
		Receivers = WoRInventoryScreen::BuildReceivers(World(), KeyOf(SCREEN_MEMBER), Pouch.Tiles[0]);
		if (!TestEqual(TEXT("still listed"), Receivers.Num(), SCREEN_PARTY_SIZE - 1)) return;
		TestFalse(TEXT("a full pouch is refused with a reason"), Receivers[0].Refusal.IsEmpty());
		const FInventoryScreenVerb *Give = FindVerb(VerbsOf(EInventoryScreenTab::Pouch, Pouch.Tiles[0]), EInventoryVerb::Give);
		TestTrue(TEXT("so Give is greyed: no one can take it"), Give && !Give->Refusal.IsEmpty());
	});

	It("lists the member's evolutions and apparel on the Gear tab, their verbs refused (B-461, Q-323)", [this]()
	{
		if (!IsReady()) return;
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->ItemName = TEXT("Screen evolution");
		TestTrue(TEXT("the evolution is added"), Inventories[SCREEN_MEMBER]->AddEvolutionInstance(Evolution));
		UApparelData *Coat = NewObject<UApparelData>(GetTransientPackage());
		Pool->AddApparelToPool(FApparelEntry::CreateFromApparel(Coat), KeyOf(SCREEN_MEMBER));

		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const FInventoryScreenTile *EvolutionTile = FindKind(Gear, EInventoryScreenKind::Evolution);
		const FInventoryScreenTile *ApparelTile = FindKind(Gear, EInventoryScreenKind::Apparel);
		if (!TestNotNull(TEXT("an evolution tile"), EvolutionTile) || !TestNotNull(TEXT("an apparel tile"), ApparelTile)) return;
		TestEqual(TEXT("the evolution is named"), EvolutionTile->Name.ToString(), FString(TEXT("Screen evolution")));
		TestFalse(TEXT("neither is a stack"), EvolutionTile->IsStack() || ApparelTile->IsStack());
		for (const FInventoryScreenTile *Tile : {EvolutionTile, ApparelTile})
		{
			const TArray<FInventoryScreenVerb> Verbs = VerbsOf(EInventoryScreenTab::Gear, *Tile);
			TestTrue(TEXT("its verbs are listed"), Verbs.Num() > 0);
			TestFalse(TEXT("and every one says why it can't run"),
					  Verbs.ContainsByPredicate([](const FInventoryScreenVerb &Verb) { return Verb.Refusal.IsEmpty(); }));
		}
		const FInventoryScreenPage Others = WoRInventoryScreen::BuildPage(World(), KeyOf(SCREEN_RECEIVER), EInventoryScreenTab::Gear);
		TestNull(TEXT("the other member doesn't list the evolution"), FindKind(Others, EInventoryScreenKind::Evolution));
		TestNull(TEXT("nor the apparel"), FindKind(Others, EInventoryScreenKind::Apparel));
		TestNull(TEXT("the chest holds no evolution"), FindKind(PageOf(EInventoryScreenTab::Chest), EInventoryScreenKind::Evolution));
	});

	It("gives each tab its own filter row and shows only the filter's kind (B-461, Q-461)", [this]()
	{
		if (!IsReady()) return;
		TestFalse(TEXT("the Gear tab has no Crystals filter"), WoRInventoryScreen::GetFilters(EInventoryScreenTab::Gear).Contains(EInventoryScreenFilter::Crystals));
		TestTrue(TEXT("the Chest has one"), WoRInventoryScreen::GetFilters(EInventoryScreenTab::Chest).Contains(EInventoryScreenFilter::Crystals));
		TestFalse(TEXT("and no Evolutions filter"), WoRInventoryScreen::GetFilters(EInventoryScreenTab::Chest).Contains(EInventoryScreenFilter::Evolutions));
		TestTrue(TEXT("the Pouch has no filter row"), WoRInventoryScreen::GetFilters(EInventoryScreenTab::Pouch).IsEmpty());

		AddDagger();
		const FName Member = KeyOf(SCREEN_MEMBER);
		const FInventoryScreenPage Weapons = WoRInventoryScreen::BuildPage(World(), Member, EInventoryScreenTab::Gear, EInventoryScreenFilter::Weapons);
		TestEqual(TEXT("Weapons lists both weapons"), Weapons.Tiles.Num(), 2);
		const FInventoryScreenPage Evolutions = WoRInventoryScreen::BuildPage(World(), Member, EInventoryScreenTab::Gear, EInventoryScreenFilter::Evolutions);
		TestTrue(TEXT("Evolutions lists nothing"), Evolutions.Tiles.IsEmpty());
		TestFalse(TEXT("and says so"), Evolutions.EmptyNote.IsEmpty());
	});

	It("sorts by tier, highest first, and by name (B-461)", [this]()
	{
		if (!IsReady()) return;
		const FName Member = KeyOf(SCREEN_MEMBER);
		Pool->CarryOrStore(Member, FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier), 1);
		Pool->CarryOrStore(Member, FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier), 1);
		const FInventoryScreenPage AsFound = PageOf(EInventoryScreenTab::Pouch);
		const FInventoryScreenPage ByTier =
			WoRInventoryScreen::BuildPage(World(), Member, EInventoryScreenTab::Pouch, EInventoryScreenFilter::All, EInventoryScreenSort::Tier);
		const FInventoryScreenPage ByName =
			WoRInventoryScreen::BuildPage(World(), Member, EInventoryScreenTab::Pouch, EInventoryScreenFilter::All, EInventoryScreenSort::Name);
		if (!TestEqual(TEXT("two stacks"), AsFound.Tiles.Num(), 2) || !TestEqual(TEXT("by tier too"), ByTier.Tiles.Num(), 2) ||
			!TestEqual(TEXT("and by name"), ByName.Tiles.Num(), 2)) return;
		TestTrue(TEXT("as found: the F Sapphire first"), AsFound.Tiles[0].TierValue == EItemTier::F_Tier);
		TestTrue(TEXT("by tier: the S stack first"), ByTier.Tiles[0].TierValue == EItemTier::S_Tier);
		TestTrue(TEXT("by name: in A to Z order"), ByName.Tiles[0].Name.CompareTo(ByName.Tiles[1].Name) <= 0);
		TestTrue(TEXT("Y comes back round to the first sort"),
				 WoRInventoryScreen::GetNextSort(WoRInventoryScreen::GetNextSort(WoRInventoryScreen::GetNextSort(EInventoryScreenSort::Kind))) ==
					 EInventoryScreenSort::Kind);
	});

	It("reaches the filter row with Up, filters with Right, returns with Back, sorts with Y (the screen, headless)", [this]()
	{
		if (!IsReady()) return;
		AddDagger();
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		UInventoryScreenWidget *Screen = CreateWidget<UInventoryScreenWidget>(World(), UInventoryScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		Screen->ActivateWidget();
		Screen->MoveTile(0, -1);
		TestTrue(TEXT("Up off the top row lands on the filters"), Screen->GetFocus() == EInventoryScreenFocus::Filters);
		Screen->MoveFilter(-1);
		TestTrue(TEXT("Left of All stays on All"), Screen->GetFilter() == EInventoryScreenFilter::All);
		Screen->MoveFilter(1);
		TestTrue(TEXT("Right shows Weapons"), Screen->GetFilter() == EInventoryScreenFilter::Weapons);
		TestEqual(TEXT("only the weapons"), Screen->GetPage().Tiles.Num(), 2);
		Screen->PressBack();
		TestTrue(TEXT("Back returns to the tiles"), Screen->GetFocus() == EInventoryScreenFocus::Grid);
		TestTrue(TEXT("with the screen still open"), Screen->IsActivated());
		TestTrue(TEXT("and the filter kept"), Screen->GetFilter() == EInventoryScreenFilter::Weapons);

		Screen->Confirm();
		Screen->CycleSort();
		TestTrue(TEXT("Y does nothing under an open verb menu"),
				 Screen->GetSort() == EInventoryScreenSort::Kind && Screen->GetFocus() == EInventoryScreenFocus::Verbs);
		Screen->PressBack();
		Screen->CycleSort();
		TestTrue(TEXT("on the tiles Y sorts by tier"), Screen->GetSort() == EInventoryScreenSort::Tier);

		Screen->MoveTab(1);
		TestTrue(TEXT("a new tab starts unfiltered"), Screen->GetFilter() == EInventoryScreenFilter::All);
		TestTrue(TEXT("and keeps the sort"), Screen->GetSort() == EInventoryScreenSort::Tier);
		Screen->MoveTile(0, -1);
		TestTrue(TEXT("the Pouch has no filter row to reach"), Screen->GetFocus() == EInventoryScreenFocus::Grid);
	});

	It("gives, stores and steps back one level at a time (the screen, headless)", [this]()
	{
		if (!IsReady()) return;
		const FGuid DaggerID = AddDagger();
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		UInventoryScreenWidget *Screen = CreateWidget<UInventoryScreenWidget>(World(), UInventoryScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		Screen->ActivateWidget();
		TestTrue(TEXT("opens on the Gear tab"), Screen->GetTab() == EInventoryScreenTab::Gear);
		TestTrue(TEXT("with the keys on the tiles"), Screen->GetFocus() == EInventoryScreenFocus::Grid);

		// Give the dagger: A opens the verb menu, Give asks who, A gives.
		Screen->SelectTile(FindGear(Screen->GetPage(), DaggerID));
		Screen->Confirm();
		TestTrue(TEXT("A opens the verb menu"), Screen->GetFocus() == EInventoryScreenFocus::Verbs);
		if (!TestTrue(TEXT("on Give"), Screen->GetVerbs().IsValidIndex(Screen->GetChoice()) && Screen->GetVerbs()[Screen->GetChoice()].Verb == EInventoryVerb::Give)) return;
		Screen->Confirm();
		TestTrue(TEXT("Give asks who"), Screen->GetFocus() == EInventoryScreenFocus::Receivers);
		Screen->PressBack();
		TestTrue(TEXT("Back returns to the verbs"), Screen->GetFocus() == EInventoryScreenFocus::Verbs);
		TestTrue(TEXT("and the screen stays open"), Screen->IsActivated());
		Screen->Confirm();
		Screen->Confirm();
		TestEqual(TEXT("the dagger is the receiver's"), Pool->GetGearOwner(DaggerID), KeyOf(SCREEN_RECEIVER));
		TestTrue(TEXT("the keys are back on the tiles"), Screen->GetFocus() == EInventoryScreenFocus::Grid);
		TestFalse(TEXT("and it says what happened"), Screen->GetStatus().IsEmpty());

		// Store the whole stack from the Pouch tab.
		Screen->MoveTab(1);
		TestTrue(TEXT("the next tab is the Pouch"), Screen->GetTab() == EInventoryScreenTab::Pouch);
		Screen->ToggleWholeStack();
		TestTrue(TEXT("X switches to the whole stack"), Screen->IsWholeStack());
		const int32 StoredBefore = Pool->GetStoredCount(SCREEN_GEM);
		Screen->Confirm();
		Screen->MoveChoice(1);
		if (!TestTrue(TEXT("down to Store"), Screen->GetVerbs().IsValidIndex(Screen->GetChoice()) && Screen->GetVerbs()[Screen->GetChoice()].Verb == EInventoryVerb::Store)) return;
		Screen->Confirm();
		TestEqual(TEXT("the whole stack is stored"), Pool->GetStoredCount(SCREEN_GEM), StoredBefore + SCREEN_GEM_COUNT);
		TestTrue(TEXT("the pouch tab is empty"), Screen->GetPage().Tiles.IsEmpty());

		Screen->PressBack();
		TestFalse(TEXT("Back on the tiles closes it"), Screen->IsActivated());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
