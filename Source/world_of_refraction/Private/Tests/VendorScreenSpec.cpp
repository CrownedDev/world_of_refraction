// VendorScreenSpec.cpp
// B-456a (D-193, D-181): the vendor screen. The model: each vendor's service tabs, which of them are built, a Sell
// row's words and pay, a refused row listed with its reason. The screen, headless: it opens on the first built
// tab, A asks first and the second A runs the service, Back cancels the question, a refused row says why, and X
// takes the whole stack. The services' own rules are WoR.Shop.VendorServices.
// B-456b: Buy. The model: the vendor's stock with its cost, an empty line skipped, a line the wallet can't pay greyed
// with the reason. The screen: it opens on Buy, A puts a line in the cart, Left takes it back, X buys the cart, and
// R / Y changes who it is bought for. The purchase's own rules are WoR.Economy.BuyFor.
// B-456c: Item slots. The model: a row per member's pouch with the next slot's price, a pouch the wallet can't pay or
// that is full greyed with the reason. The screen: A asks, the second A buys one slot for the focused member. The
// purchase's own rules are WoR.Inventory.ItemSlotUpgrade.
// B-456d: the Spiritualist's Repair, Curses and Merge. Each lists its rows with the price and the service's own
// refusal; A asks, the second A does it; Repair's X repairs everything; Merge's Left / Right picks the tier. The
// services' own rules are WoR.Equipment.GearRepair, WoR.Memory.Curse and the crystal merge spec.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Durability/GearRepair.h"
#include "Loadout/CarriedGearState.h"
#include "Memory/CurseData.h"
#include "Memory/CurseSubsystem.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemSlotUpgrade.h"
#include "Loadout/LoadoutComponent.h"
#include "Merchant/MerchantData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/VendorScreenModel.h"
#include "UI/Menus/VendorScreenWidget.h"

namespace
{
	constexpr int32 VENDOR_SCREEN_GEM_COUNT = 3;
	const FCrystalId VENDOR_SCREEN_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 VENDOR_SCREEN_WALLET = 100000;
	/** The test vendor's shelf: the spear, the gems, and an empty line (skipped). */
	constexpr int32 VENDOR_SCREEN_SPEAR_ROW = 0;
	constexpr int32 VENDOR_SCREEN_GEM_ROW = 1;
	constexpr int32 VENDOR_SCREEN_STOCK_ROWS = 2;
	/** B-456d: a worn gem's durability, the merge test's F gems (worth one D), and a held curse. */
	constexpr int32 VENDOR_SCREEN_WORN = 1;
	constexpr int32 VENDOR_SCREEN_MERGE_F_COUNT = 4;
	const FName VENDOR_SCREEN_CURSE(TEXT("Curse_VendorScreenSpec"));
}

BEGIN_DEFINE_SPEC(FVendorScreenSpec, "WoR.UI.VendorScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	UWeaponData *Spear = nullptr;
	UMerchantData *Merchant = nullptr;
	UParty *Party = nullptr;
	UCharacterData *Member = nullptr;
	UInventoryComponent *Inventory = nullptr;

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Member); }
	UWorld *World() const { return TestWorld->GetWorld(); }
	AActor *Payer() const { return Inventory->GetOwner(); }
	UCurrencyComponent *Wallet() const { return Payer()->FindComponentByClass<UCurrencyComponent>(); }
	int32 Money() const { return Wallet()->GetBalance(ECurrencyType::Prisms); }

	/** Money and essence for anything on the test vendor's shelf. */
	void Fund()
	{
		Wallet()->Add(ECurrencyType::Prisms, VENDOR_SCREEN_WALLET);
		Wallet()->Add(ECurrencyType::ElementalEssence, VENDOR_SCREEN_WALLET);
	}

	/** A party member as each level spawns it: the sword as its loadout's primary. */
	UInventoryComponent *AddMember(const TCHAR *Name, UCharacterData *&OutData)
	{
		OutData = FWoRTestWorld::NewCharacterData();
		OutData->Name = Name;
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = OutData;
		AActor *Actor = TestWorld->SpawnCombatant(OutData);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Added = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Added->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Added->SavedLoadouts = {Loadout};
		Added->ActiveLoadoutIndex = 0;
		Added->LinkLoadoutsToOwned();
		Added->DrawSharedGear();
		return Added;
	}

	bool Owns(FName Owner, const UWeaponData *Weapon) const
	{
		return Pool->GetOwnedGearOf(Owner).Weapons.ContainsByPredicate([Weapon](const FWeaponInventoryEntry *E) { return E->Weapon == Weapon; });
	}

	FGuid IdOf(const UWeaponData *Weapon) const
	{
		const FWeaponInventoryEntry *Entry = Inventory->Weapons.FindByPredicate([Weapon](const FWeaponInventoryEntry &E) { return E.Weapon == Weapon; });
		return Entry ? Entry->PersistentID : FGuid();
	}

	static int32 FindGear(const FVendorScreenPage &Page, const FGuid &GearID)
	{
		return Page.Rows.IndexOfByPredicate([&GearID](const FVendorScreenRow &Row) { return Row.Row.GearID == GearID; });
	}

	static int32 FindStack(const FVendorScreenPage &Page, const FCrystalId &Item)
	{
		return Page.Rows.IndexOfByPredicate([&Item](const FVendorScreenRow &Row) { return !Row.Row.IsGear() && Row.Row.Item == Item; });
	}

	/** Vendor's screen, as talking to the test vendor opens it, moved to Tab. */
	UVendorScreenWidget *OpenScreen(EVendorScreenTab Tab = EVendorScreenTab::Sell, EMerchantType Vendor = EMerchantType::Blacksmith)
	{
		UVendorScreenWidget *Screen = CreateWidget<UVendorScreenWidget>(World(), UVendorScreenWidget::StaticClass());
		if (Screen)
		{
			Screen->ActivateWidget();
			Screen->SetVendor(Vendor, Merchant, Payer());
			for (int32 Step = 0; Step < WoRVendorScreen::GetTabs(Vendor).Num() && Screen->GetTab() != Tab; ++Step)
			{
				Screen->MoveTab(1);
			}
		}
		return Screen;
	}
END_DEFINE_SPEC(FVendorScreenSpec)

void FVendorScreenSpec::Define()
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
		Spear = NewObject<UWeaponData>(GetTransientPackage());
		Spear->Name = TEXT("Screen spear");
		Merchant = NewObject<UMerchantData>(GetTransientPackage());
		Merchant->MerchantType = EMerchantType::Blacksmith;
		Merchant->Stock.SetNum(VENDOR_SCREEN_STOCK_ROWS + 1); // the last line stays empty
		Merchant->Stock[VENDOR_SCREEN_SPEAR_ROW].Asset = Spear;
		Merchant->Stock[VENDOR_SCREEN_GEM_ROW].Crystal = VENDOR_SCREEN_GEM;
		Merchant->Stock[VENDOR_SCREEN_GEM_ROW].Count = VENDOR_SCREEN_GEM_COUNT;
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();

		// The sword is the member's primary, the dagger spare.
		Inventory = AddMember(TEXT("Screen member"), Member);
		if (!TestNotNull(TEXT("member actor"), Inventory)) return;
		Inventory->AddWeapon(Dagger);
	});
	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
		Spear = nullptr;
		Merchant = nullptr;
		Party = nullptr;
		Member = nullptr;
		Inventory = nullptr;
		TestWorld.Reset();
	});

	It("gives each vendor its service tabs (D-193) and says which aren't built yet", [this]()
	{
		const TArray<EVendorScreenTab> Blacksmith = {EVendorScreenTab::Buy, EVendorScreenTab::Sell, EVendorScreenTab::Dismantle};
		const TArray<EVendorScreenTab> Jeweler = {EVendorScreenTab::Buy, EVendorScreenTab::Sell, EVendorScreenTab::ItemSlots};
		const TArray<EVendorScreenTab> Spiritualist = {EVendorScreenTab::Buy,	 EVendorScreenTab::Sell,	  EVendorScreenTab::Repair,
													   EVendorScreenTab::Curses, EVendorScreenTab::Dismantle, EVendorScreenTab::Merge};
		TestTrue(TEXT("Blacksmith"), WoRVendorScreen::GetTabs(EMerchantType::Blacksmith) == Blacksmith);
		TestTrue(TEXT("Jeweler"), WoRVendorScreen::GetTabs(EMerchantType::Jeweler) == Jeweler);
		TestTrue(TEXT("Spiritualist"), WoRVendorScreen::GetTabs(EMerchantType::Spiritualist) == Spiritualist);
		TestTrue(TEXT("a vendor D-193 doesn't name has Buy alone"),
				 WoRVendorScreen::GetTabs(EMerchantType::CombatMaster) == TArray<EVendorScreenTab>({EVendorScreenTab::Buy}));
		for (const EVendorScreenTab Tab : Spiritualist)
		{
			TestFalse(TEXT("every tab is named"), WoRVendorScreen::GetTabName(Tab).IsEmpty());
		}
		// B-456d: every tab D-193 names is built.
		for (const EVendorScreenTab Tab : Spiritualist)
		{
			TestTrue(TEXT("every Spiritualist tab is built"), WoRVendorScreen::IsBuilt(Tab));
		}
		TestTrue(TEXT("Item slots is built"), WoRVendorScreen::IsBuilt(EVendorScreenTab::ItemSlots));
	});

	It("lists what the member can sell with its pay, and the loadout's primary greyed with the reason", [this]()
	{
		if (!Inventory) return;
		const FVendorScreenPage Page = WoRVendorScreen::BuildPage(World(), EMerchantType::Blacksmith, EVendorScreenTab::Sell, Key());
		const int32 DaggerRow = FindGear(Page, IdOf(Dagger));
		const int32 SwordRow = FindGear(Page, IdOf(Sword));
		if (!TestTrue(TEXT("both weapons are rows"), DaggerRow != INDEX_NONE && SwordRow != INDEX_NONE)) return;
		TestEqual(TEXT("named as the inventory names it"), Page.Rows[DaggerRow].Name.ToString(), FString(TEXT("Screen dagger")));
		TestFalse(TEXT("a kind line"), Page.Rows[DaggerRow].KindLine.IsEmpty());
		TestFalse(TEXT("what it pays"), Page.Rows[DaggerRow].Pay.IsEmpty());
		TestTrue(TEXT("the spare can be sold"), Page.Rows[DaggerRow].Refusal.IsEmpty());
		TestFalse(TEXT("the primary can't, and says why"), Page.Rows[SwordRow].Refusal.IsEmpty());
		TestFalse(TEXT("the Dismantle tab says its rule"),
				  WoRVendorScreen::BuildPage(World(), EMerchantType::Blacksmith, EVendorScreenTab::Dismantle, Key()).Rule.IsEmpty());
	});

	It("Sell: A asks, Back cancels, and the second A sells", [this]()
	{
		if (!Inventory) return;
		UVendorScreenWidget *Screen = OpenScreen();
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		TestTrue(TEXT("RB from Buy is Sell"), Screen->GetTab() == EVendorScreenTab::Sell);
		const FGuid DaggerID = IdOf(Dagger);
		const int32 Before = Money();
		Screen->SelectRow(FindGear(Screen->GetPage(), DaggerID));

		Screen->Confirm();
		TestTrue(TEXT("the first A asks"), Screen->IsConfirming());
		TestNotNull(TEXT("nothing sold yet"), Pool->FindWeaponInPool(DaggerID));
		Screen->PressBack();
		TestFalse(TEXT("Back cancels the question"), Screen->IsConfirming());
		TestNotNull(TEXT("still nothing sold"), Pool->FindWeaponInPool(DaggerID));

		Screen->Confirm();
		Screen->Confirm();
		TestFalse(TEXT("done asking"), Screen->IsConfirming());
		TestNull(TEXT("the second A sells"), Pool->FindWeaponInPool(DaggerID));
		TestTrue(TEXT("paid"), Money() > Before);
		TestFalse(TEXT("the screen says what happened"), Screen->GetStatus().IsEmpty());
		TestEqual(TEXT("the row is gone"), FindGear(Screen->GetPage(), DaggerID), static_cast<int32>(INDEX_NONE));
	});

	It("says why on a refused row and asks nothing; changing the tab drops a pending question", [this]()
	{
		if (!Inventory) return;
		UVendorScreenWidget *Screen = OpenScreen();
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		const int32 SwordRow = FindGear(Screen->GetPage(), IdOf(Sword));
		if (!TestTrue(TEXT("the primary is listed"), SwordRow != INDEX_NONE)) return;
		Screen->SelectRow(SwordRow);
		Screen->Confirm();
		TestFalse(TEXT("no question for a refused row"), Screen->IsConfirming());
		TestEqual(TEXT("the reason is said"), Screen->GetStatus().ToString(), Screen->GetPage().Rows[SwordRow].Refusal.ToString());

		Screen->SelectRow(FindGear(Screen->GetPage(), IdOf(Dagger)));
		Screen->Confirm();
		TestTrue(TEXT("asking about the dagger"), Screen->IsConfirming());
		Screen->MoveTab(1);
		TestTrue(TEXT("RB moves to Dismantle"), Screen->GetTab() == EVendorScreenTab::Dismantle);
		TestFalse(TEXT("and drops the question"), Screen->IsConfirming());
		TestNotNull(TEXT("nothing happened to the dagger"), Pool->FindWeaponInPool(IdOf(Dagger)));
	});

	It("sells one of a stack, or all of it with X", [this]()
	{
		if (!Inventory) return;
		Pool->CarryOrStore(Key(), VENDOR_SCREEN_GEM, VENDOR_SCREEN_GEM_COUNT);
		UVendorScreenWidget *Screen = OpenScreen();
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		const int32 Price = UEconomyService::GetSellPriceForTier(VENDOR_SCREEN_GEM.Tier);
		const int32 Before = Money();
		Screen->SelectRow(FindStack(Screen->GetPage(), VENDOR_SCREEN_GEM));
		Screen->Confirm();
		Screen->Confirm();
		TestEqual(TEXT("one sold"), Pool->GetCarriedCount(Key(), VENDOR_SCREEN_GEM), VENDOR_SCREEN_GEM_COUNT - 1);
		TestEqual(TEXT("paid for one"), Money(), Before + Price);

		Screen->SelectRow(FindStack(Screen->GetPage(), VENDOR_SCREEN_GEM));
		Screen->Confirm();
		Screen->ToggleWholeStack();
		TestFalse(TEXT("X after the question drops it: the amount changed"), Screen->IsConfirming());
		Screen->Confirm();
		Screen->Confirm();
		TestEqual(TEXT("the rest sold"), Pool->GetCarriedCount(Key(), VENDOR_SCREEN_GEM), 0);
		TestEqual(TEXT("paid for all"), Money(), Before + Price * VENDOR_SCREEN_GEM_COUNT);
	});

	It("Buy lists the vendor's stock with its cost, skips an empty line, and greys what the wallet can't pay", [this]()
	{
		if (!Inventory) return;
		Wallet()->Spend(ECurrencyType::Prisms, Money());
		const FVendorScreenPage Poor = WoRVendorScreen::BuildBuyPage(World(), Merchant, Payer(), Key(), {});
		if (!TestEqual(TEXT("the spear and the gems; the empty line is skipped"), Poor.Rows.Num(), VENDOR_SCREEN_STOCK_ROWS)) return;
		const FVendorScreenRow &SpearRow = Poor.Rows[VENDOR_SCREEN_SPEAR_ROW];
		TestEqual(TEXT("named as the loadout screen names it"), SpearRow.Name.ToString(), FString(TEXT("Screen spear")));
		TestFalse(TEXT("a kind line"), SpearRow.KindLine.IsEmpty());
		TestFalse(TEXT("what it costs"), SpearRow.Pay.IsEmpty());
		TestTrue(TEXT("money alone: no second price for the row"), SpearRow.PayMoney.IsEmpty());
		TestFalse(TEXT("an empty wallet is refused, with the reason"), SpearRow.Refusal.IsEmpty());
		TestFalse(TEXT("gems cost an essence too: the row shows the money alone"), Poor.Rows[VENDOR_SCREEN_GEM_ROW].PayMoney.IsEmpty());

		Fund();
		const FVendorScreenPage Rich = WoRVendorScreen::BuildBuyPage(World(), Merchant, Payer(), Key(), {});
		TestTrue(TEXT("with money both can be bought"),
				 Rich.Rows[VENDOR_SCREEN_SPEAR_ROW].Refusal.IsEmpty() && Rich.Rows[VENDOR_SCREEN_GEM_ROW].Refusal.IsEmpty());
		TestFalse(TEXT("no vendor: nothing for sale, and it says so"), WoRVendorScreen::BuildBuyPage(World(), nullptr, Payer(), Key(), {}).EmptyNote.IsEmpty());
	});

	It("Buy: opens on it; A puts the focused line in the cart, Left takes it back, and X buys the cart", [this]()
	{
		if (!Inventory) return;
		Fund();
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::Buy);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		TestTrue(TEXT("opens on Buy"), Screen->GetTab() == EVendorScreenTab::Buy);
		const int32 Before = Money();
		Screen->SelectRow(VENDOR_SCREEN_SPEAR_ROW);
		Screen->Confirm();
		Screen->Confirm();
		if (!TestEqual(TEXT("two presses on one line: one cart line"), Screen->GetCart().Num(), 1)) return;
		TestEqual(TEXT("of two"), Screen->GetCart()[0].Stock.Count, 2);
		TestEqual(TEXT("the row says how many the cart holds"), Screen->GetPage().Rows[VENDOR_SCREEN_SPEAR_ROW].Row.Count, 2);
		TestEqual(TEXT("bought for the payer: the cart says yourself"), Screen->GetCart()[0].ForName.ToString(), FString(TEXT("yourself")));
		Screen->RemoveFocusedFromCart();
		TestEqual(TEXT("Left takes one back"), Screen->GetCart()[0].Stock.Count, 1);
		TestEqual(TEXT("nothing spent yet"), Money(), Before);
		TestFalse(TEXT("nor owned"), Owns(Key(), Spear));

		Screen->Purchase();
		TestTrue(TEXT("X buys: the cart is empty"), Screen->GetCart().IsEmpty());
		TestTrue(TEXT("paid"), Money() < Before);
		TestTrue(TEXT("the spear is the member's"), Owns(Key(), Spear));
		TestFalse(TEXT("the screen says what happened"), Screen->GetStatus().IsEmpty());
	});

	It("Buy: a line the wallet can't pay says why and stays out of the cart; X on an empty cart buys nothing", [this]()
	{
		if (!Inventory) return;
		Wallet()->Spend(ECurrencyType::Prisms, Money());
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::Buy);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		Screen->SelectRow(VENDOR_SCREEN_SPEAR_ROW);
		Screen->Confirm();
		TestTrue(TEXT("nothing in the cart"), Screen->GetCart().IsEmpty());
		TestEqual(TEXT("the reason is said"), Screen->GetStatus().ToString(), Screen->GetPage().Rows[VENDOR_SCREEN_SPEAR_ROW].Refusal.ToString());
		Screen->Purchase();
		TestFalse(TEXT("still not owned"), Owns(Key(), Spear));
	});

	It("Buy: R / Y changes who it is bought for; the cart names them and the weapon becomes theirs", [this]()
	{
		if (!Inventory) return;
		UCharacterData *Friend = nullptr;
		if (!TestNotNull(TEXT("a second member"), AddMember(TEXT("Screen friend"), Friend))) return;
		const FName FriendKey = UPoolSubsystem::GetCharacterKey(Friend);
		Fund();
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::Buy);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		Screen->SelectRow(VENDOR_SCREEN_GEM_ROW);
		Screen->MoveMember(1);
		TestEqual(TEXT("buying for the second member"), Screen->GetMemberIndex(), 1);
		TestEqual(TEXT("the focused line stays put"), Screen->GetSelectedRow(), static_cast<int32>(VENDOR_SCREEN_GEM_ROW));
		Screen->SelectRow(VENDOR_SCREEN_SPEAR_ROW);
		Screen->Confirm();
		if (!TestEqual(TEXT("in the cart"), Screen->GetCart().Num(), 1)) return;
		TestEqual(TEXT("the cart says who it is for"), Screen->GetCart()[0].ForName.ToString(), FString(TEXT("Screen friend")));
		const int32 Before = Money();
		Screen->Purchase();
		TestTrue(TEXT("bought"), Screen->GetCart().IsEmpty());
		TestTrue(TEXT("the payer's wallet paid"), Money() < Before);
		TestTrue(TEXT("the spear is the friend's"), Owns(FriendKey, Spear));
		TestFalse(TEXT("not the payer's"), Owns(Key(), Spear));
	});

	It("Item slots lists each member's pouch with the next slot's price, and greys one the wallet can't pay or that is full", [this]()
	{
		if (!Inventory) return;
		FVendorScreenPage Page = WoRVendorScreen::BuildItemSlotsPage(World(), Payer());
		if (!TestEqual(TEXT("a row per member"), Page.Rows.Num(), 1)) return;
		TestEqual(TEXT("the row is the member's"), Page.Rows[0].Member, Key());
		TestEqual(TEXT("the pouch's slots"), Page.Rows[0].Slots, InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("the most it can have"), Page.Rows[0].SlotsMax, InventoryConstants::ITEM_SLOTS_MAX);
		TestFalse(TEXT("an empty wallet is refused with the reason"), Page.Rows[0].Refusal.IsEmpty());
		TestFalse(TEXT("and still shows the price"), Page.Rows[0].Pay.IsEmpty());
		TestFalse(TEXT("the tab says what a slot is"), Page.Intro.IsEmpty());

		Fund();
		Page = WoRVendorScreen::BuildItemSlotsPage(World(), Payer());
		TestTrue(TEXT("a funded wallet can buy"), Page.Rows[0].Refusal.IsEmpty());
		TestEqual(TEXT("the slot after this one is priced too"), Page.Rows[0].Details.Num(), 1);
		TestFalse(TEXT("what the pouch will hold"), Page.Rows[0].Note.IsEmpty());

		while (Pool->AddItemSlot(Key())) {}
		Page = WoRVendorScreen::BuildItemSlotsPage(World(), Payer());
		TestEqual(TEXT("a full pouch"), Page.Rows[0].Slots, InventoryConstants::ITEM_SLOTS_MAX);
		TestFalse(TEXT("is refused with the reason"), Page.Rows[0].Refusal.IsEmpty());
		TestTrue(TEXT("and prices no further slot"), Page.Rows[0].Details.IsEmpty());
	});

	It("Item slots: A asks, Back cancels, and the second A buys one slot for the focused member", [this]()
	{
		if (!Inventory) return;
		UCharacterData *Friend = nullptr;
		if (!TestNotNull(TEXT("a second member"), AddMember(TEXT("Screen friend"), Friend))) return;
		const FName FriendKey = UPoolSubsystem::GetCharacterKey(Friend);
		Fund();
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::ItemSlots, EMerchantType::Jeweler);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		TestTrue(TEXT("on Item slots"), Screen->GetTab() == EVendorScreenTab::ItemSlots);
		if (!TestEqual(TEXT("a row per member"), Screen->GetPage().Rows.Num(), 2)) return;
		Screen->MoveMember(1);
		TestEqual(TEXT("the rows are the members: no character tab to change"), Screen->GetMemberIndex(), 0);

		const int32 Before = Money();
		Screen->SelectRow(1);
		Screen->Confirm();
		TestTrue(TEXT("the first A asks"), Screen->IsConfirming());
		TestEqual(TEXT("nothing spent yet"), Money(), Before);
		Screen->PressBack();
		TestFalse(TEXT("Back cancels the question"), Screen->IsConfirming());
		Screen->Confirm();
		Screen->Confirm();
		TestEqual(TEXT("the friend's pouch has one more slot"), Pool->GetItemSlotCount(FriendKey), InventoryConstants::ITEM_SLOTS_START + 1);
		TestEqual(TEXT("the member's pouch is as it was"), Pool->GetItemSlotCount(Key()), InventoryConstants::ITEM_SLOTS_START);
		TestEqual(TEXT("the payer's wallet paid the price"), Money(), Before - ItemSlotUpgrade::GetPrice(InventoryConstants::ITEM_SLOTS_START));
		TestEqual(TEXT("the row shows the new slot"), Screen->GetPage().Rows[1].Slots, InventoryConstants::ITEM_SLOTS_START + 1);
		TestFalse(TEXT("the screen says what happened"), Screen->GetStatus().IsEmpty());

		Wallet()->Spend(ECurrencyType::Prisms, Money());
		Screen->MoveTab(1);
		Screen->MoveTab(-1); // back on Item slots, read fresh
		Screen->Confirm();
		TestFalse(TEXT("a pouch the wallet can't pay asks nothing"), Screen->IsConfirming());
		TestFalse(TEXT("the row is refused"), Screen->GetPage().Rows[0].Refusal.IsEmpty());
		TestEqual(TEXT("and says why"), Screen->GetStatus().ToString(), Screen->GetPage().Rows[0].Refusal.ToString());
	});

	It("Repair: lists each worn piece with its price; A asks and the second A repairs it; X asks and the second X repairs the rest", [this]()
	{
		if (!Inventory) return;
		FCarriedGearState &Gear = Party->Members[0].CarriedGear;
		Gear.bCarried = true;
		for (const int32 Durability : {VENDOR_SCREEN_WORN, 0})
		{
			FCarriedAttachment &Carried = Gear.Attachments.AddDefaulted_GetRef();
			Carried.Attached.Kind = EAttachedItemKind::Crystal;
			Carried.Attached.Crystal = FCrystalAttachment(VENDOR_SCREEN_GEM, Durability);
		}
		const int32 WornCost = GearRepair::GetRepairCost(Gear.Attachments[0].Attached);
		const int32 AllCost = GearRepair::GetPartyRepairCost(*Party);
		Wallet()->Spend(ECurrencyType::Prisms, Money());

		FVendorScreenPage Page = WoRVendorScreen::BuildRepairPage(World(), Payer());
		if (!TestEqual(TEXT("a row per worn piece"), Page.Rows.Num(), 2)) return;
		TestTrue(TEXT("the broken one says so"), Page.Rows[1].bStateIsBad && !Page.Rows[1].State.IsEmpty());
		TestFalse(TEXT("an empty wallet is refused with the reason"), Page.Rows[0].Refusal.IsEmpty());
		TestFalse(TEXT("so is repairing everything"), Page.AllRefusal.IsEmpty());

		Fund();
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::Repair, EMerchantType::Spiritualist);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		TestTrue(TEXT("on Repair"), Screen->GetTab() == EVendorScreenTab::Repair);
		const int32 Before = Money();
		Screen->SelectRow(0);
		Screen->Confirm();
		TestTrue(TEXT("the first A asks"), Screen->IsConfirming());
		TestEqual(TEXT("nothing spent yet"), Money(), Before);
		Screen->Confirm();
		TestEqual(TEXT("the worn piece paid for"), Money(), Before - WornCost);
		TestEqual(TEXT("and at full"), Gear.Attachments[0].Attached.GetCurrentDurability(), Gear.Attachments[0].Attached.GetMaxDurability());
		TestEqual(TEXT("one row left"), Screen->GetPage().Rows.Num(), 1);

		Screen->PressRepairAll();
		TestTrue(TEXT("the first X asks"), Screen->IsConfirmingAll());
		Screen->PressBack();
		TestFalse(TEXT("Back cancels it"), Screen->IsConfirmingAll());
		Screen->PressRepairAll();
		Screen->PressRepairAll();
		TestEqual(TEXT("the rest paid for"), Money(), Before - AllCost);
		TestEqual(TEXT("nothing left to repair"), GearRepair::GetPartyRepairCost(*Party), 0);
		TestTrue(TEXT("no rows left"), Screen->GetPage().Rows.IsEmpty());
	});

	It("Curses: lists each held curse with its price; A asks and the second A clears it", [this]()
	{
		if (!Inventory) return;
		UCurseSubsystem *Curses = TestWorld->GetSubsystem<UCurseSubsystem>();
		if (!TestNotNull(TEXT("curses"), Curses)) return;
		Curses->SetCurseAssets({}); // no asset: priced as the defaults (Q-390)
		Curses->AddCurse(VENDOR_SCREEN_CURSE);
		FVendorScreenPage Page = WoRVendorScreen::BuildCursesPage(World(), Payer());
		if (!TestEqual(TEXT("a row per held curse"), Page.Rows.Num(), 1)) return;
		TestFalse(TEXT("an empty wallet is refused with the reason"), Page.Rows[0].Refusal.IsEmpty());
		TestFalse(TEXT("and still shows the price"), Page.Rows[0].Pay.IsEmpty());

		const UCurseData *Priced = GetDefault<UCurseData>();
		Wallet()->Add(Priced->GetClearCurrency(), Priced->GetClearPrice());
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::Curses, EMerchantType::Spiritualist);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		Screen->Confirm();
		TestTrue(TEXT("the first A asks"), Screen->IsConfirming());
		TestTrue(TEXT("still held"), Curses->HasCurse(VENDOR_SCREEN_CURSE));
		Screen->Confirm();
		TestFalse(TEXT("the second A clears it"), Curses->HasCurse(VENDOR_SCREEN_CURSE));
		TestEqual(TEXT("for its price"), Wallet()->GetBalance(Priced->GetClearCurrency()), 0);
		TestTrue(TEXT("no rows left"), Screen->GetPage().Rows.IsEmpty());
	});

	It("Merge: a row per crystal kind at the highest tier it makes; Left / Right picks the tier; the second A merges", [this]()
	{
		if (!Inventory) return;
		Pool->CarryOrStore(Key(), VENDOR_SCREEN_GEM, VENDOR_SCREEN_MERGE_F_COUNT); // four F make one D (§4.5)
		FVendorScreenPage Page = WoRVendorScreen::BuildMergePage(World(), Key(), {});
		if (!TestEqual(TEXT("a row per crystal kind"), Page.Rows.Num(), 1)) return;
		TestTrue(TEXT("aims at the highest tier it can make"), Page.Rows[0].Row.Item == FCrystalId(VENDOR_SCREEN_GEM.Type, EItemTier::D_Tier));
		TestEqual(TEXT("uses, makes, change back"), Page.Rows[0].Details.Num(), 3);
		Wallet()->Spend(ECurrencyType::Prisms, Money());
		Page = WoRVendorScreen::BuildMergePage(World(), Key(), {});
		TestFalse(TEXT("an empty wallet is refused with the reason"), Page.Rows[0].Refusal.IsEmpty());
		TestTrue(TEXT("the same reason the merge gives"),
				 Page.Rows[0].Refusal.EqualTo(UEconomyService::GetMergeRefusal(Payer(), VENDOR_SCREEN_GEM.Type, EItemTier::D_Tier)));

		Fund();
		UVendorScreenWidget *Screen = OpenScreen(EVendorScreenTab::Merge, EMerchantType::Spiritualist);
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		Screen->MoveMergeTarget(-1);
		TestTrue(TEXT("Left: one tier down"), Screen->GetPage().Rows[0].Row.Item.Tier == EItemTier::E_Tier);
		Screen->MoveMergeTarget(-1);
		TestTrue(TEXT("past E it wraps to S"), Screen->GetPage().Rows[0].Row.Item.Tier == EItemTier::S_Tier);
		TestFalse(TEXT("S is out of reach, with the reason"), Screen->GetPage().Rows[0].Refusal.IsEmpty());
		Screen->MoveMergeTarget(1);
		const int32 Before = Money();
		Screen->Confirm();
		TestTrue(TEXT("the first A asks"), Screen->IsConfirming());
		Screen->Confirm();
		const FCrystalId MadeE(VENDOR_SCREEN_GEM.Type, EItemTier::E_Tier);
		TestEqual(TEXT("one E made"), Pool->GetCarriedCount(Key(), MadeE), 1);
		TestEqual(TEXT("two F used"), Pool->GetCarriedCount(Key(), VENDOR_SCREEN_GEM), VENDOR_SCREEN_MERGE_F_COUNT - 2);
		TestEqual(TEXT("for the merge's price"), Money(), Before - EconomyYield::GetMergeCostForTier(EItemTier::E_Tier));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
