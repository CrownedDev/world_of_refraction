// InventoryScreenSpec.cpp
// B-441 (D-181, D-177, D-195): the inventory screen. The model: each tab lists its own things (Gear: the member's
// weapons and adapters, worn ones marked; Pouch: its stacks and its slots; no Chest tab, B-575c), a tile's verb
// menu is Give only (B-575c: no Store / Take), a verb that can't run carries its reason, and "Give to"
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
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/InventoryScreenModel.h"
#include "UI/Menus/InventoryScreenWidget.h"
#include "UI/Menus/LoadoutV2GridModel.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "UI/WoRIconTable.h"

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

	It("on the merged screen (B-598c) the Inventory is a page: the bag fills the grid, the pages turn in a ring, Up reaches the filters then the Gear / Pouch selector", [this]()
	{
		if (!IsReady()) return;
		AddDagger();
		ULoadoutV2ScreenWidget *Screen = CreateWidget<ULoadoutV2ScreenWidget>(World(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen)) return;
		Screen->ActivateWidget();
		TestTrue(TEXT("opens on the Inventory page"), Screen->IsOnInventoryPage() && Screen->IsOnInventorySide() && Screen->GetPageIndex() == ULoadoutV2ScreenWidget::INVENTORY_PAGE);
		TestTrue(TEXT("on the Gear tab"), Screen->GetInventoryTab() == EInventoryScreenTab::Gear);
		TestEqual(TEXT("the Gear tab's tiles are the inventory screen's"), Screen->GetInventorySide().Tiles.Num(), PageOf(EInventoryScreenTab::Gear).Tiles.Num());
		TestTrue(TEXT("at least the sword and the dagger"), Screen->GetInventorySide().Tiles.Num() >= 2);
		TestEqual(TEXT("the left column's five to a row"), Screen->GetGridColumns(), WoRLoadoutV2Grid::COLUMNS);
		TestFalse(TEXT("the middle column has the lit tile's details"), Screen->GetDetails().IsEmpty());
		TestFalse(TEXT("no slot is being filled"), Screen->IsGridOpen());

		// UI_Theme 5a: a move redraws only what changed (the light view), never the whole screen.
		const int32 FullBefore = Screen->GetFullRefreshCount();
		const int32 LightBefore = Screen->GetLightRefreshCount();
		Screen->MoveInventoryTile(1, 0);
		TestEqual(TEXT("Right: the next tile"), Screen->GetInventoryTile(), 1);
		TestTrue(TEXT("a tile move takes the light view only"), Screen->GetFullRefreshCount() == FullBefore && Screen->GetLightRefreshCount() == LightBefore + 1);
		Screen->MoveInventoryTile(-1, 0);
		Screen->MoveInventoryTile(-1, 0);
		TestTrue(TEXT("Left off the first column: stays on the first tile"), Screen->IsOnInventoryPage() && Screen->GetInventoryTile() == 0);

		Screen->ToggleLoadout();
		TestTrue(TEXT("to the Equipment page: the keys are the loadout's"), Screen->IsLoadoutOpen() && !Screen->IsOnInventorySide() && Screen->GetPageIndex() == ULoadoutV2ScreenWidget::EQUIPMENT_PAGE);
		TestTrue(TEXT("the loadout lists the member's weapon set"), Screen->GetPage().Entries.Num() > 0);
		const int32 ListFullBefore = Screen->GetFullRefreshCount();
		const int32 ListLightBefore = Screen->GetLightRefreshCount();
		Screen->MoveSelection(1);
		TestTrue(TEXT("a move down the loadout's list takes the light view too"), Screen->GetFullRefreshCount() == ListFullBefore && Screen->GetLightRefreshCount() == ListLightBefore + 1);
		Screen->MoveSelection(-1);
		TestFalse(TEXT("no inventory side to cross to (D-249's sides are gone)"), Screen->FocusInventorySide());
		Screen->SwitchTab(-1);
		TestTrue(TEXT("Q from Equipment: back to the Inventory page"), Screen->IsOnInventoryPage());

		Screen->MoveInventoryTile(0, -1);
		TestTrue(TEXT("Up off the top row: the filter row"), Screen->IsOnInventoryFilters() && !Screen->IsOnInventoryKinds());
		Screen->MoveInventoryTile(0, -1);
		TestTrue(TEXT("Up again: the Gear / Pouch selector"), Screen->IsOnInventoryKinds() && !Screen->IsOnInventoryFilters());
		Screen->MoveInventoryTile(1, 0);
		TestTrue(TEXT("Right on the selector: the Pouch, the keys stay on it"), Screen->GetInventoryTab() == EInventoryScreenTab::Pouch && Screen->IsOnInventoryKinds());
		Screen->MoveInventoryTile(0, 1);
		TestTrue(TEXT("Down: the Pouch has no filters, so the tiles"), !Screen->IsOnInventoryKinds() && !Screen->IsOnInventoryFilters());
		Screen->MoveInventoryTile(0, -1);
		TestTrue(TEXT("Up from the Pouch's top row: straight to the selector"), Screen->IsOnInventoryKinds());
		Screen->MoveInventoryTile(1, 0);
		TestTrue(TEXT("no Chest (B-575c): in a ring, straight back to Gear"), Screen->GetInventoryTab() == EInventoryScreenTab::Gear);
		Screen->MoveInventoryTile(0, 1);
		TestTrue(TEXT("Down from the selector on Gear: its filter row"), Screen->IsOnInventoryFilters());
		Screen->MoveInventoryTile(0, -1);
		Screen->PressBack();
		TestTrue(TEXT("Back from the selector: the tiles, the screen stays"), !Screen->IsOnInventoryKinds() && Screen->IsActivated());
		const EInventoryScreenSort SortBefore = Screen->GetInventorySort();
		Screen->CycleInventorySort();
		TestTrue(TEXT("the sort steps on"), Screen->GetInventorySort() == WoRInventoryScreen::GetNextSort(SortBefore));

		// The loadout in the middle of something can't be left for the Inventory page under it.
		Screen->ToggleLoadout();
		Screen->PressA();
		if (TestTrue(TEXT("A on the weapon set opens its window"), Screen->IsAdding()))
		{
			Screen->ToggleLoadout();
			TestTrue(TEXT("the page can't change under a window"), Screen->IsLoadoutOpen() && !Screen->GetMessage().IsEmpty());
			TestTrue(TEXT("and that is the Refused cue (UI_Theme 5a)"), Screen->GetLastCue() == EWoRMenuCue::Refused);
			Screen->PressBack();
			TestFalse(TEXT("Back leaves the window"), Screen->IsAdding());
		}
		Screen->ToggleLoadout();
		TestTrue(TEXT("and then back to the Inventory page"), Screen->IsOnInventoryPage() && Screen->IsOnInventorySide());
		TestFalse(TEXT("FocusLoadoutSide is gone (OBSOLETE)"), Screen->FocusLoadoutSide());
	});

	It("B-598c piece 2: the Details column's facts (worn in, owned, sells for) and buttons (Equip on…, Give to…, Sell greyed)", [this]()
	{
		if (!IsReady()) return;
		const FGuid DaggerID = AddDagger();
		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const int32 SwordTile = FindGear(Gear, IDOf(Sword));
		const int32 DaggerTile = FindGear(Gear, DaggerID);
		if (!TestTrue(TEXT("both tiles"), SwordTile != INDEX_NONE && DaggerTile != INDEX_NONE)) return;
		const auto FactOf = [](const TArray<FLoadoutScreenDetail> &Facts, const TCHAR *Label) -> const FLoadoutScreenDetail *
		{ return Facts.FindByPredicate([Label](const FLoadoutScreenDetail &Fact) { return Fact.Label.ToString() == Label; }); };
		const TArray<FLoadoutScreenDetail> SwordFacts = WoRInventoryScreen::BuildFacts(World(), KeyOf(SCREEN_MEMBER), Gear.Tiles[SwordTile]);
		const FLoadoutScreenDetail *SwordWorn = FactOf(SwordFacts, TEXT("Worn in"));
		TestTrue(TEXT("the sword is worn in the active loadout"), SwordWorn && SwordWorn->Value.ToString() != TEXT("Not worn"));
		const FLoadoutScreenDetail *SwordOwned = FactOf(SwordFacts, TEXT("Owned"));
		TestTrue(TEXT("one sword owned"), SwordOwned && SwordOwned->Value.ToString() == TEXT("1"));
		TestNotNull(TEXT("a price by tier"), FactOf(SwordFacts, TEXT("Sells for")));
		const FLoadoutScreenDetail *DaggerWorn = FactOf(WoRInventoryScreen::BuildFacts(World(), KeyOf(SCREEN_MEMBER), Gear.Tiles[DaggerTile]), TEXT("Worn in"));
		TestTrue(TEXT("the dagger is not worn"), DaggerWorn && DaggerWorn->Value.ToString() == TEXT("Not worn"));

		ULoadoutV2ScreenWidget *Screen = CreateWidget<ULoadoutV2ScreenWidget>(World(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen)) return;
		Screen->ActivateWidget();
		const int32 ScreenSword = FindGear(Screen->GetInventorySide(), IDOf(Sword));
		const int32 ScreenDagger = FindGear(Screen->GetInventorySide(), DaggerID);
		const auto LightTile = [Screen](int32 Tile)
		{
			while (Screen->GetInventoryTile() < Tile) Screen->MoveInventoryTile(1, 0);
			while (Screen->GetInventoryTile() > Tile) Screen->MoveInventoryTile(-1, 0);
		};
		LightTile(ScreenSword);
		TestFalse(TEXT("the lit tile's facts are in the Details column"), Screen->GetInventoryFacts().IsEmpty());
		if (!TestEqual(TEXT("three buttons: Equip on, Give to, Sell"), Screen->GetInventoryActions().Num(), 3)) return;
		TestTrue(TEXT("Equip on is open for a weapon"), Screen->GetInventoryActions()[0].Refusal.IsEmpty());
		TestFalse(TEXT("the worn sword can't be given (D-116), with the reason"), Screen->GetInventoryActions()[1].Refusal.IsEmpty());
		TestFalse(TEXT("Sell is greyed: the Blacksmith's"), Screen->GetInventoryActions()[2].Refusal.IsEmpty());
		Screen->PressGive();
		TestFalse(TEXT("a refused Give opens no list, it says why"), Screen->IsGiving() || Screen->GetMessage().IsEmpty());

		LightTile(ScreenDagger);
		TestTrue(TEXT("the spare dagger can be given"), Screen->GetInventoryActions().IsValidIndex(1) && Screen->GetInventoryActions()[1].Refusal.IsEmpty());
		Screen->PressGive();
		TestTrue(TEXT("X: the other members are the buttons"), Screen->IsGiving() && Screen->GetInventoryActions().Num() == SCREEN_PARTY_SIZE - 1);
		Screen->PressBack();
		TestTrue(TEXT("Back: out of the list, the screen stays"), !Screen->IsGiving() && Screen->IsActivated() && Screen->GetInventoryActions().Num() == 3);
		Screen->PressEquipOn();
		TestTrue(TEXT("A, Equip on: the Equipment page, saying what to pick"), Screen->GetPageIndex() == ULoadoutV2ScreenWidget::EQUIPMENT_PAGE && !Screen->GetMessage().IsEmpty());
	});

	It("B-614: a slot's grid has the kind selector: Up reaches it, a refused kind says why, a turn changes the entry's kind and keeps each kind's draft", [this]()
	{
		if (!IsReady()) return;
		ULoadoutV2ScreenWidget *Screen = CreateWidget<ULoadoutV2ScreenWidget>(World(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen)) return;
		Screen->ActivateWidget();
		Screen->ToggleLoadout();
		Screen->PressA(); // the weapon set: the window's first question
		Screen->PressA(); // its own kind: the slots
		Screen->PressA(); // the main hand's grid
		if (!TestTrue(TEXT("the main hand's grid is open"), Screen->IsGridOpen() && Screen->GetSlots().IsValidIndex(0) && Screen->GetSlots()[0] == ELoadoutV2Slot::MainHand)) return;
		const TArray<ELoadoutV2PieceKind> Kinds = Screen->GetGridKinds();
		if (!TestTrue(TEXT("Weapon, Adapter, Evolution, on Weapon"), Kinds == ULoadoutV2ScreenWidget::GetSlotKinds(ELoadoutV2Slot::MainHand) && Screen->GetGridKind() == ELoadoutV2PieceKind::Weapon)) return;
		TestFalse(TEXT("not on the selector yet"), Screen->IsOnGridKinds());
		Screen->MoveInDirection(0, -1);
		TestTrue(TEXT("Up off the top row: the selector"), Screen->IsOnGridKinds());
		TestEqual(TEXT("with the Move cue"), Screen->GetLastCue(), EWoRMenuCue::Move);

		int32 Open = INDEX_NONE;
		for (int32 Index = 1; Index < Kinds.Num(); ++Index)
		{
			const FText Refusal = Screen->GetGridKindRefusal(Kinds[Index]);
			if (Refusal.IsEmpty())
			{
				Open = Open == INDEX_NONE ? Index : Open;
				continue;
			}
			Screen->PickGridKind(Index);
			TestTrue(TEXT("a refused kind: stays on Weapon, says why"), Screen->GetGridKind() == ELoadoutV2PieceKind::Weapon && Screen->GetMessage().EqualTo(Refusal));
		}
		if (Open == INDEX_NONE)
		{
			Screen->TurnGridKind(1);
			TestTrue(TEXT("nothing to turn to: stays, with the reason"), Screen->GetGridKind() == ELoadoutV2PieceKind::Weapon && !Screen->GetMessage().IsEmpty());
			return;
		}
		Screen->TurnGridKind(1);
		TestTrue(TEXT("Right: the first kind that can be turned to (refused ones skipped)"), Screen->GetGridKind() == Kinds[Open]);
		TestTrue(TEXT("the entry is that kind now, its first slot's grid open, the keys still on the selector"),
				 Screen->GetDraft().Kind != ELoadoutV2DraftKind::Weapon && Screen->IsGridOpen() && Screen->IsOnGridKinds());
		Screen->PickGridKind(0);
		TestTrue(TEXT("back to Weapon: the main hand again"), Screen->GetGridKind() == ELoadoutV2PieceKind::Weapon && Screen->GetSlots()[0] == ELoadoutV2Slot::MainHand);
		TestEqual(TEXT("with the weapon draft as it was left: the sword in hand"), Screen->GetDraft().MainHand.PersistentID, IDOf(Sword));
		Screen->MoveInDirection(0, 1);
		TestFalse(TEXT("Down: back to the tiles"), Screen->IsOnGridKinds());
		Screen->PressBack();
		TestTrue(TEXT("Back: the slots, the screen stays"), !Screen->IsGridOpen() && Screen->IsActivated());
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
		TestTrue(TEXT("a weapon tile asks for its weapon-type icon, and no tier badge (B-487)"),
				 Gear.Tiles[DaggerTile].Icon.Kind == EWoRIconKind::WeaponType && !Gear.Tiles[DaggerTile].Icon.Name.IsEmpty() && !Gear.Tiles[DaggerTile].bTierBadge);
		TestTrue(TEXT("the pouch tab holds no gear"), PageOf(EInventoryScreenTab::Pouch).Tiles.IsEmpty());

		// B-543: an adapter tile asks for the B-530 adapter icon (its frame, the crystal inside), not the letter A.
		Inventories[SCREEN_MEMBER]->AddRing(NewObject<URingData>(GetTransientPackage()));
		const FInventoryScreenPage WithAdapter = PageOf(EInventoryScreenTab::Gear);
		const FInventoryScreenTile *Adapter = WithAdapter.Tiles.FindByPredicate([](const FInventoryScreenTile &Tile) { return Tile.Kind == EInventoryScreenKind::Adapter; });
		TestTrue(TEXT("an adapter tile uses the adapter icon"), Adapter && Adapter->Icon.Kind == EWoRIconKind::Adapter && Adapter->Icon.Element == Adapter->Element);
		TestFalse(TEXT("an empty tab says so"), PageOf(EInventoryScreenTab::Pouch).EmptyNote.IsEmpty());
	});

	It("lists Give only on a tile, and has no Chest tab (B-575c, D-250: the chest is off)", [this]()
	{
		if (!IsReady()) return;
		TestFalse(TEXT("no Chest tab"), WoRInventoryScreen::GetTabs().Contains(EInventoryScreenTab::Chest));
		TestEqual(TEXT("Gear and Pouch"), WoRInventoryScreen::GetTabs().Num(), 2);
		const FGuid DaggerID = AddDagger();
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const int32 DaggerTile = FindGear(Gear, DaggerID);
		if (!TestTrue(TEXT("the dagger is a tile"), DaggerTile != INDEX_NONE)) return;
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack carried"), Pouch.Tiles.Num(), 1)) return;
		const TArray<FInventoryScreenVerb> Own = VerbsOf(EInventoryScreenTab::Gear, Gear.Tiles[DaggerTile]);
		const TArray<FInventoryScreenVerb> Stack = VerbsOf(EInventoryScreenTab::Pouch, Pouch.Tiles[0]);
		for (const TArray<FInventoryScreenVerb> *Verbs : {&Own, &Stack})
		{
			TestEqual(TEXT("one verb on the tile"), Verbs->Num(), 1);
			TestNull(TEXT("no Store"), FindVerb(*Verbs, EInventoryVerb::Store));
			TestNull(TEXT("no Take"), FindVerb(*Verbs, EInventoryVerb::Take));
		}
		const FInventoryScreenVerb *Give = FindVerb(Own, EInventoryVerb::Give);
		TestTrue(TEXT("a spare piece can be given"), Give && Give->Refusal.IsEmpty());
		TestTrue(TEXT("the hub placeholder offers Give only too"),
				 WoRInventoryVerbs::GetShownVerbs().Num() == 1 && WoRInventoryVerbs::GetShownVerbs()[0] == EInventoryVerb::Give);
	});

	It("greys Give with the code's reason for a worn primary; the hidden Store rule still refuses it (B-575c)", [this]()
	{
		if (!IsReady()) return;
		const FInventoryScreenPage Gear = PageOf(EInventoryScreenTab::Gear);
		const int32 SwordTile = FindGear(Gear, IDOf(Sword));
		if (!TestTrue(TEXT("the sword is a tile"), SwordTile != INDEX_NONE)) return;
		const TArray<FInventoryScreenVerb> Verbs = VerbsOf(EInventoryScreenTab::Gear, Gear.Tiles[SwordTile]);
		const FInventoryScreenVerb *Give = FindVerb(Verbs, EInventoryVerb::Give);
		if (!TestNotNull(TEXT("Give listed, not hidden"), Give)) return;
		TestFalse(TEXT("Give says why not"), Give->Refusal.IsEmpty());

		// B-575c: Store is off the screen; its OBSOLETE rule stays until Crown's PIE pass. In a run the chest is at
		// home: Store names that instead.
		const FName Member = KeyOf(SCREEN_MEMBER);
		const FText AtHome = WoRInventoryVerbs::GetRefusal(World(), EInventoryVerb::Store, Member, NAME_None, Gear.Tiles[SwordTile].Row);
		TestFalse(TEXT("Store says why not"), AtHome.IsEmpty());
		Pool->BeginRunLayer();
		const FInventoryScreenPage InRun = PageOf(EInventoryScreenTab::Gear);
		const int32 RunTile = FindGear(InRun, IDOf(Sword));
		if (!TestTrue(TEXT("the worn sword came on the run"), RunTile != INDEX_NONE)) return;
		const FText InRunRefusal = WoRInventoryVerbs::GetRefusal(World(), EInventoryVerb::Store, Member, NAME_None, InRun.Tiles[RunTile].Row);
		TestFalse(TEXT("refused in a run"), InRunRefusal.IsEmpty());
		TestFalse(TEXT("for the chest's own reason"), InRunRefusal.EqualTo(AtHome));

		// Q-441: a pouch stack isn't stored during a run either, and the press itself is refused.
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack carried"), Pouch.Tiles.Num(), 1)) return;
		TestFalse(TEXT("a stack's Store is refused in a run"),
				  WoRInventoryVerbs::GetRefusal(World(), EInventoryVerb::Store, Member, NAME_None, Pouch.Tiles[0].Row).IsEmpty());
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
		TestEqual(TEXT("the heading counts the fight items picked of the slots (D-250)"), Pouch.Heading.ToString(),
				  FString::Printf(TEXT("Pouch  1/%d fight items"), InventoryConstants::ITEM_SLOTS_START));
		TestTrue(TEXT("the picked stack's card says it is a fight item"),
				 Pouch.Tiles[0].Details.ContainsByPredicate([](const FLoadoutScreenDetail &Detail) { return Detail.Label.ToString() == TEXT("Fight item"); }));
		const FName Giver = KeyOf(SCREEN_MEMBER);
		const FName Receiver = KeyOf(SCREEN_RECEIVER);
		TestEqual(TEXT("one at a time"), WoRInventoryScreen::GetMoveCount(World(), EInventoryVerb::Store, Giver, NAME_None, Pouch.Tiles[0], /*bWholeStack=*/false), 1);
		TestEqual(TEXT("or the whole stack"), WoRInventoryScreen::GetMoveCount(World(), EInventoryVerb::Store, Giver, NAME_None, Pouch.Tiles[0], /*bWholeStack=*/true),
				  SCREEN_GEM_COUNT);

		// D-250: a whole-stack Give moves the whole stack; the receiver's stack has no size limit.
		Pool->CarryOrStore(Receiver, SCREEN_GEM, InventoryConstants::ITEM_STACK_MAX - 1);
		TestEqual(TEXT("the whole stack is given"), WoRInventoryScreen::GetMoveCount(World(), EInventoryVerb::Give, Giver, Receiver, Pouch.Tiles[0], /*bWholeStack=*/true),
				  SCREEN_GEM_COUNT);
	});

	It("never greys Take for room: a pouch with every slot picked still takes the stack (D-250)", [this]()
	{
		if (!IsReady()) return;
		Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), SCREEN_GEM, SCREEN_GEM_COUNT);
		const FInventoryScreenPage Pouch = PageOf(EInventoryScreenTab::Pouch);
		if (!TestEqual(TEXT("one stack carried"), Pouch.Tiles.Num(), 1)) return;
		TestTrue(TEXT("stored"), WoRInventoryVerbs::Run(World(), EInventoryVerb::Store, KeyOf(SCREEN_MEMBER), NAME_None, Pouch.Tiles[0].Row, SCREEN_GEM_COUNT).IsEmpty());
		FInventoryScreenPage Chest = PageOf(EInventoryScreenTab::Chest);
		int32 Stored = Chest.Tiles.IndexOfByPredicate([](const FInventoryScreenTile &Tile) { return !Tile.Row.IsGear() && Tile.Row.Item == SCREEN_GEM; });
		if (!TestTrue(TEXT("the stack is on the (hidden) Chest page"), Stored != INDEX_NONE)) return;
		// B-575c: Take is off the screen and the stored row is hidden from the hub placeholder; the OBSOLETE rule is
		// checked at the verb level until Crown's PIE pass.
		TestFalse(TEXT("the hub placeholder hides the chest's rows"),
				  WoRInventoryVerbs::GatherShownRows(World(), KeyOf(SCREEN_MEMBER)).ContainsByPredicate([](const WoRInventoryVerbs::FInventoryRow &Row) { return Row.bInChest; }));
		TestTrue(TEXT("Take can run with room"), WoRInventoryVerbs::GetRefusal(World(), EInventoryVerb::Take, KeyOf(SCREEN_MEMBER), NAME_None, Chest.Tiles[Stored].Row).IsEmpty());

		for (const ECrystalType Kind : SCREEN_FILLER_KINDS)
		{
			for (const EItemTier Tier : SCREEN_TIERS)
			{
				Pool->CarryOrStore(KeyOf(SCREEN_MEMBER), FCrystalId(Kind, Tier), 1);
			}
		}
		Chest = PageOf(EInventoryScreenTab::Chest);
		Stored = Chest.Tiles.IndexOfByPredicate([](const FInventoryScreenTile &Tile) { return !Tile.Row.IsGear() && Tile.Row.Item == SCREEN_GEM; });
		if (!TestTrue(TEXT("still on the Chest page"), Stored != INDEX_NONE)) return;
		TestTrue(TEXT("every slot picked, and Take still runs: carrying has no limit"),
				 WoRInventoryVerbs::GetRefusal(World(), EInventoryVerb::Take, KeyOf(SCREEN_MEMBER), NAME_None, Chest.Tiles[Stored].Row).IsEmpty());
	});

	It("says who has a free fight-item slot in Give to, and never refuses a pouch for room (D-250)", [this]()
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
		TestEqual(TEXT("the receiver's fight-item slots are all taken"), Pool->GetUsedItemSlots(KeyOf(SCREEN_RECEIVER)), Pool->GetItemSlotCount(KeyOf(SCREEN_RECEIVER)));
		Receivers = WoRInventoryScreen::BuildReceivers(World(), KeyOf(SCREEN_MEMBER), Pouch.Tiles[0]);
		if (!TestEqual(TEXT("still listed"), Receivers.Num(), SCREEN_PARTY_SIZE - 1)) return;
		TestTrue(TEXT("it can still receive: carrying has no limit"), Receivers[0].Refusal.IsEmpty());
		TestTrue(TEXT("with no free slot to speak of"), Receivers[0].Note.IsEmpty());
		const TArray<FInventoryScreenVerb> PouchVerbs = VerbsOf(EInventoryScreenTab::Pouch, Pouch.Tiles[0]); // kept: FindVerb points into it
		const FInventoryScreenVerb *Give = FindVerb(PouchVerbs, EInventoryVerb::Give);
		TestTrue(TEXT("so Give is not greyed"), Give && Give->Refusal.IsEmpty());
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

		Screen->MoveTab(1); // the Pouch, next to Gear (D-249: the Loadout tab is gone)
		TestTrue(TEXT("a new tab starts unfiltered"), Screen->GetFilter() == EInventoryScreenFilter::All);
		TestTrue(TEXT("and keeps the sort"), Screen->GetSort() == EInventoryScreenSort::Tier);
		Screen->MoveTile(0, -1);
		TestTrue(TEXT("the Pouch has no filter row to reach"), Screen->GetFocus() == EInventoryScreenFocus::Grid);
	});

	It("gives and steps back one level at a time (the screen, headless)", [this]()
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

		// B-575c: the Pouch tab's stack has Give only, no Store (the chest is off).
		Screen->MoveTab(1);
		TestTrue(TEXT("the Pouch is next to Gear (D-249: the Loadout tab is gone)"), Screen->GetTab() == EInventoryScreenTab::Pouch);
		Screen->ToggleWholeStack();
		TestTrue(TEXT("X switches to the whole stack"), Screen->IsWholeStack());
		Screen->Confirm();
		TestTrue(TEXT("A opens the stack's verb menu"), Screen->GetFocus() == EInventoryScreenFocus::Verbs);
		TestTrue(TEXT("Give only, no Store"), Screen->GetVerbs().Num() == 1 && Screen->GetVerbs()[0].Verb == EInventoryVerb::Give);
		Screen->PressBack();
		TestTrue(TEXT("Back returns to the tiles"), Screen->GetFocus() == EInventoryScreenFocus::Grid);

		Screen->PressBack();
		TestFalse(TEXT("Back on the tiles closes it"), Screen->IsActivated());
	});

	It("gives a crystal tile its element's diamond, a tier badge and its effect's icon (B-487)", [this]()
	{
		WoRInventoryVerbs::FInventoryRow Garnet;
		Garnet.Item = FCrystalId(ECrystalType::Garnet, EItemTier::C_Tier);
		Garnet.Count = 1;
		const FInventoryScreenTile Tile = WoRInventoryScreen::BuildStackTile(Garnet);
		TestTrue(TEXT("the element's loose gem (a stack is in no socket, B-584), named for its tooltip"), Tile.Icon.Kind == EWoRIconKind::LooseGem && Tile.Icon.Element == Tile.Element && !Tile.Icon.Name.IsEmpty());
		TestTrue(TEXT("a real element"), Tile.Element != ESpellElement::None);
		TestTrue(TEXT("its tier as a badge"), Tile.bTierBadge && Tile.TierValue == EItemTier::C_Tier);
		TestTrue(TEXT("its effect sentence gets the burn's icon"), !Tile.Note.IsEmpty() && Tile.EffectIcon.Kind == EWoRIconKind::Effect && Tile.EffectIcon.EffectType == ESkillEffectType::DOT);
		// A table with nothing set (the spec never loads the game's own table asset): the tile keeps its letter.
		WoRIcons::SetActiveForTests(NewObject<UWoRIconTable>(GetTransientPackage()));
		TestNull(TEXT("with no icon set the tile keeps its letter"), WoRIcons::Find(Tile.Icon));
		WoRIcons::SetActiveForTests(nullptr);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
