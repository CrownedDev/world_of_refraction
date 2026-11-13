// VendorScreenSpec.cpp
// B-456a (D-193, D-181): the vendor screen. The model: each vendor's service tabs, which of them are built, a Sell
// row's words and pay, a refused row listed with its reason. The screen, headless: it opens on the first built
// tab, A asks first and the second A runs the service, Back cancels the question, a refused row says why, and X
// takes the whole stack. The services' own rules are WoR.Shop.VendorServices.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/VendorScreenModel.h"
#include "UI/Menus/VendorScreenWidget.h"

namespace
{
	constexpr int32 VENDOR_SCREEN_GEM_COUNT = 3;
	const FCrystalId VENDOR_SCREEN_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FVendorScreenSpec, "WoR.UI.VendorScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	UCharacterData *Member = nullptr;
	UInventoryComponent *Inventory = nullptr;

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Member); }
	UWorld *World() const { return TestWorld->GetWorld(); }
	int32 Money() const { return Inventory->GetOwner()->FindComponentByClass<UCurrencyComponent>()->GetBalance(ECurrencyType::Prisms); }

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

	/** The Blacksmith's screen, as wor.UI.VendorScreen opens it. */
	UVendorScreenWidget *OpenScreen()
	{
		UVendorScreenWidget *Screen = CreateWidget<UVendorScreenWidget>(World(), UVendorScreenWidget::StaticClass());
		if (Screen)
		{
			Screen->ActivateWidget();
			Screen->SetVendor(EMerchantType::Blacksmith);
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
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		Member = FWoRTestWorld::NewCharacterData();
		Member->Name = TEXT("Screen member");
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = Member;

		// As each level spawns a party member: the sword as its loadout's primary, the dagger spare.
		AActor *Actor = TestWorld->SpawnCombatant(Member);
		if (!TestNotNull(TEXT("member actor"), Actor)) return;
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		Inventory->AddWeapon(Dagger);
	});
	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
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
		TestTrue(TEXT("the Combat Master has no shop screen"), WoRVendorScreen::GetTabs(EMerchantType::CombatMaster).IsEmpty());
		for (const EVendorScreenTab Tab : Spiritualist)
		{
			TestFalse(TEXT("every tab is named"), WoRVendorScreen::GetTabName(Tab).IsEmpty());
		}
		TestTrue(TEXT("Sell and Dismantle are built"), WoRVendorScreen::IsBuilt(EVendorScreenTab::Sell) && WoRVendorScreen::IsBuilt(EVendorScreenTab::Dismantle));
		if (!Inventory) return;
		for (const EVendorScreenTab Tab : Spiritualist)
		{
			if (!WoRVendorScreen::IsBuilt(Tab))
			{
				const FVendorScreenPage Page = WoRVendorScreen::BuildPage(World(), EMerchantType::Spiritualist, Tab, Key());
				TestTrue(TEXT("a tab that isn't built lists nothing and says so"), Page.Rows.IsEmpty() && !Page.EmptyNote.IsEmpty());
			}
		}
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

	It("opens on the first built tab; A asks, Back cancels, and the second A sells", [this]()
	{
		if (!Inventory) return;
		UVendorScreenWidget *Screen = OpenScreen();
		if (!TestNotNull(TEXT("screen created"), Screen)) return;
		TestTrue(TEXT("opens on Sell while Buy isn't built"), Screen->GetTab() == EVendorScreenTab::Sell);
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
		Screen->ToggleWholeStack();
		Screen->Confirm();
		Screen->Confirm();
		TestEqual(TEXT("the rest sold"), Pool->GetCarriedCount(Key(), VENDOR_SCREEN_GEM), 0);
		TestEqual(TEXT("paid for all"), Money(), Before + Price * VENDOR_SCREEN_GEM_COUNT);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
