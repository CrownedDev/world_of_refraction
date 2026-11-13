// VendorServicesSpec.cpp
// B-445 (D-177, D-193, Q-445): selling and dismantling are a vendor's. Which vendor does which; a sale removes the
// piece and pays half its tier's buy price in the place's currency; the Blacksmith dismantles gear for Gear essence;
// the Spiritualist's Dismantle doesn't take gear; dismantling is refused during a run, and so is selling a home
// piece; a loadout's primary is refused for both. Takes over the Dismantle and Convert cases of InventoryVerbsSpec.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Shop/VendorServices.h"
#include "WoRGameNames.h"

BEGIN_DEFINE_SPEC(FVendorServicesSpec, "WoR.Shop.VendorServices", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	UCharacterData *Member = nullptr;
	UInventoryComponent *Inventory = nullptr;

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Member); }
	UWorld *World() const { return Inventory->GetWorld(); }
	UCurrencyComponent *Wallet() const { return Inventory->GetOwner()->FindComponentByClass<UCurrencyComponent>(); }

	FGuid IdOf(const UWeaponData *Weapon) const
	{
		const FWeaponInventoryEntry *Entry = Inventory->Weapons.FindByPredicate([Weapon](const FWeaponInventoryEntry &E) { return E.Weapon == Weapon; });
		return Entry ? Entry->PersistentID : FGuid();
	}

	/** The member's row for GearID at Vendor's Service; unset when the vendor doesn't list it. */
	TOptional<WoRVendorServices::FVendorRow> RowOf(EMerchantType Vendor, EVendorService Service, const FGuid &GearID) const
	{
		for (const WoRVendorServices::FVendorRow &Row : WoRVendorServices::GatherRows(World(), Vendor, Service, Key()))
		{
			if (Row.GearID == GearID)
			{
				return Row;
			}
		}
		return {};
	}
END_DEFINE_SPEC(FVendorServicesSpec)

void FVendorServicesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Vendor sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Vendor dagger");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		Member = FWoRTestWorld::NewCharacterData();
		Member->Name = TEXT("Vendor member");
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

	It("gives each vendor its services (D-193)", [this]()
	{
		TestTrue(TEXT("Blacksmith sells"), WoRVendorServices::Offers(EMerchantType::Blacksmith, EVendorService::Sell));
		TestTrue(TEXT("Blacksmith dismantles"), WoRVendorServices::Offers(EMerchantType::Blacksmith, EVendorService::Dismantle));
		TestTrue(TEXT("Jeweler sells"), WoRVendorServices::Offers(EMerchantType::Jeweler, EVendorService::Sell));
		TestFalse(TEXT("Jeweler doesn't dismantle"), WoRVendorServices::Offers(EMerchantType::Jeweler, EVendorService::Dismantle));
		TestTrue(TEXT("Spiritualist sells"), WoRVendorServices::Offers(EMerchantType::Spiritualist, EVendorService::Sell));
		TestTrue(TEXT("Spiritualist dismantles"), WoRVendorServices::Offers(EMerchantType::Spiritualist, EVendorService::Dismantle));
		TestFalse(TEXT("the Combat Master does neither"), WoRVendorServices::Offers(EMerchantType::CombatMaster, EVendorService::Sell));
		TestFalse(TEXT("both services are named"), WoRGameNames::GetVendorServiceName(EVendorService::Sell).IsEmpty() ||
													   WoRGameNames::GetVendorServiceName(EVendorService::Dismantle).IsEmpty());
	});

	It("sells a weapon for half its tier's buy price, in the hub's currency (Q-445)", [this]()
	{
		if (!Inventory) return;
		const FGuid DaggerID = IdOf(Dagger);
		const TOptional<WoRVendorServices::FVendorRow> Row = RowOf(EMerchantType::Jeweler, EVendorService::Sell, DaggerID);
		if (!TestTrue(TEXT("the dagger is listed"), Row.IsSet())) return;
		const int32 Price = UEconomyService::GetSellPriceForTier(Row.GetValue().Tier);
		TestEqual(TEXT("half the buy price"), Price, EconomyYield::GetPrismsBaseForTier(Row.GetValue().Tier) / 2);
		const FString HubName = WoRGameNames::GetCurrencyName(ECurrencyType::Prisms).ToString();
		TestTrue(TEXT("the confirm names the hub currency"),
				 WoRVendorServices::GetConfirmText(World(), EVendorService::Sell, Key(), Row.GetValue()).ToString().Contains(HubName));
		const int32 Before = Wallet()->GetBalance(ECurrencyType::Prisms);
		TestTrue(TEXT("sold"), WoRVendorServices::Run(World(), EMerchantType::Jeweler, EVendorService::Sell, Key(), Row.GetValue(), 1).IsEmpty());
		TestNull(TEXT("the dagger is gone"), Pool->FindWeaponInPool(DaggerID));
		TestEqual(TEXT("paid"), Wallet()->GetBalance(ECurrencyType::Prisms), Before + Price);
	});

	It("dismantles a weapon at the Blacksmith for Gear essence", [this]()
	{
		if (!Inventory) return;
		const FGuid DaggerID = IdOf(Dagger);
		const TOptional<WoRVendorServices::FVendorRow> Row = RowOf(EMerchantType::Blacksmith, EVendorService::Dismantle, DaggerID);
		if (!TestTrue(TEXT("the dagger is listed"), Row.IsSet())) return;
		const int32 Before = Wallet()->GetGearEssence();
		TestTrue(TEXT("dismantled"), WoRVendorServices::Run(World(), EMerchantType::Blacksmith, EVendorService::Dismantle, Key(), Row.GetValue(), 1).IsEmpty());
		TestNull(TEXT("the dagger is gone"), Pool->FindWeaponInPool(DaggerID));
		TestEqual(TEXT("Gear essence by its tier"), Wallet()->GetGearEssence(), Before + EconomyYield::GetLevelingEssenceYieldForTier(Row.GetValue().Tier));
	});

	It("keeps gear out of the Spiritualist's Dismantle, which is for evolutions (D-193)", [this]()
	{
		if (!Inventory) return;
		const FGuid DaggerID = IdOf(Dagger);
		TestFalse(TEXT("not listed there"), RowOf(EMerchantType::Spiritualist, EVendorService::Dismantle, DaggerID).IsSet());
		const TOptional<WoRVendorServices::FVendorRow> Row = RowOf(EMerchantType::Blacksmith, EVendorService::Dismantle, DaggerID);
		if (!TestTrue(TEXT("listed at the Blacksmith"), Row.IsSet())) return;
		TestFalse(TEXT("refused at the Spiritualist"),
				  WoRVendorServices::Run(World(), EMerchantType::Spiritualist, EVendorService::Dismantle, Key(), Row.GetValue(), 1).IsEmpty());
		TestFalse(TEXT("the Jeweler doesn't dismantle at all"),
				  WoRVendorServices::Run(World(), EMerchantType::Jeweler, EVendorService::Dismantle, Key(), Row.GetValue(), 1).IsEmpty());
		TestNotNull(TEXT("still owned"), Pool->FindWeaponInPool(DaggerID));
	});

	It("refuses to dismantle during a run, and to sell a piece from home there", [this]()
	{
		if (!Inventory) return;
		const FGuid DaggerID = IdOf(Dagger);
		const TOptional<WoRVendorServices::FVendorRow> Row = RowOf(EMerchantType::Blacksmith, EVendorService::Dismantle, DaggerID);
		if (!TestTrue(TEXT("listed"), Row.IsSet())) return;
		Pool->BeginRunLayer();
		TestFalse(TEXT("no dismantling in a run"),
				  WoRVendorServices::GetRefusal(World(), EMerchantType::Blacksmith, EVendorService::Dismantle, Key(), Row.GetValue()).IsEmpty());
		TestFalse(TEXT("a home piece isn't sold in a run"),
				  WoRVendorServices::Run(World(), EMerchantType::Blacksmith, EVendorService::Sell, Key(), Row.GetValue(), 1).IsEmpty());
		TestNotNull(TEXT("still owned"), Pool->FindWeaponInPool(DaggerID));
		Pool->EndRunLayer();
		TestTrue(TEXT("back home it may be dismantled"),
				 WoRVendorServices::GetRefusal(World(), EMerchantType::Blacksmith, EVendorService::Dismantle, Key(), Row.GetValue()).IsEmpty());
	});

	It("sells one of a pouch stack and leaves the rest carried", [this]()
	{
		if (!Inventory) return;
		const FCrystalId Gem(ECrystalType::Garnet, EItemTier::F_Tier);
		const int32 Carried = 3;
		Pool->CarryOrStore(Key(), Gem, Carried);
		TOptional<WoRVendorServices::FVendorRow> Stack;
		for (const WoRVendorServices::FVendorRow &Row : WoRVendorServices::GatherRows(World(), EMerchantType::Jeweler, EVendorService::Sell, Key()))
		{
			if (!Row.IsGear() && Row.Item == Gem)
			{
				Stack = Row;
			}
		}
		if (!TestTrue(TEXT("the pouch stack is listed"), Stack.IsSet())) return;
		const int32 Before = Wallet()->GetBalance(ECurrencyType::Prisms);
		TestTrue(TEXT("one sold"), WoRVendorServices::Run(World(), EMerchantType::Jeweler, EVendorService::Sell, Key(), Stack.GetValue(), 1).IsEmpty());
		TestEqual(TEXT("the rest is still carried"), Pool->GetCarriedCount(Key(), Gem), Carried - 1);
		TestEqual(TEXT("paid for one"), Wallet()->GetBalance(ECurrencyType::Prisms), Before + UEconomyService::GetSellPriceForTier(Gem.Tier));
	});

	It("dismantles an evolution at the Spiritualist, and only there", [this]()
	{
		if (!Inventory) return;
		UEvolutionInventoryComponent *Evolutions = Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>();
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->ItemName = TEXT("Vendor evolution");
		if (!TestTrue(TEXT("evolution owned"), Evolutions && Evolutions->AddInstance(Evolution))) return;
		TestEqual(TEXT("the Blacksmith lists no evolution"),
				  WoRVendorServices::GatherRows(World(), EMerchantType::Blacksmith, EVendorService::Dismantle, Key())
					  .FilterByPredicate([](const WoRVendorServices::FVendorRow &Row) { return Row.IsEvolution(); }).Num(), 0);
		const TArray<WoRVendorServices::FVendorRow> Rows = WoRVendorServices::GatherRows(World(), EMerchantType::Spiritualist, EVendorService::Dismantle, Key());
		if (!TestTrue(TEXT("the Spiritualist lists it, and nothing else"), Rows.Num() == 1 && Rows[0].IsEvolution())) return;
		TestTrue(TEXT("dismantled"), WoRVendorServices::Run(World(), EMerchantType::Spiritualist, EVendorService::Dismantle, Key(), Rows[0], 1).IsEmpty());
		TestEqual(TEXT("gone"), Evolutions->Num(), 0);
	});

	It("refuses a loadout's primary for both services, with the reason", [this]()
	{
		if (!Inventory) return;
		const FGuid SwordID = IdOf(Sword);
		const TOptional<WoRVendorServices::FVendorRow> Row = RowOf(EMerchantType::Blacksmith, EVendorService::Sell, SwordID);
		if (!TestTrue(TEXT("the sword is listed"), Row.IsSet())) return;
		TestFalse(TEXT("not sold"), WoRVendorServices::Run(World(), EMerchantType::Blacksmith, EVendorService::Sell, Key(), Row.GetValue(), 1).IsEmpty());
		TestFalse(TEXT("not dismantled"), WoRVendorServices::Run(World(), EMerchantType::Blacksmith, EVendorService::Dismantle, Key(), Row.GetValue(), 1).IsEmpty());
		TestNotNull(TEXT("still owned"), Pool->FindWeaponInPool(SwordID));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
