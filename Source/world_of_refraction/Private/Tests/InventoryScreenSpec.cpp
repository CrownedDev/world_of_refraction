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
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
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
		TestEqual(TEXT("one at a time"), WoRInventoryScreen::GetMoveCount(Pouch.Tiles[0], /*bWholeStack=*/false), 1);
		TestEqual(TEXT("or the whole stack"), WoRInventoryScreen::GetMoveCount(Pouch.Tiles[0], /*bWholeStack=*/true), SCREEN_GEM_COUNT);
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
