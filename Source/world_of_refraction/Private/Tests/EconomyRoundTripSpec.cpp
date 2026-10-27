// EconomyRoundTripSpec.cpp
// B-137: what the economy charges and pays back today. A cart costs the sum of its Prisms prices
// (a crystal per unit, bare gear at its tier's base); a purchase spends exactly that or nothing.
// Levelling a weapon costs the tier-up price in Gear essence plus half that in Reality; reverting
// the step refunds half the Gear essence only, and never below the weapon's authored tier.
// Dismantling pays by the instance tier. Every entry point's owner checks are WoR.Economy.Preamble;
// dismantling equipped gear is WoR.Economy.DismantleEquipped.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Merchant/MerchantData.h"

namespace
{
	const FCrystalId ECONOMY_TRIP_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 ECONOMY_TRIP_GEM_COUNT = 3;
	constexpr int32 ECONOMY_TRIP_SHORT = 1;
	constexpr EItemTier ECONOMY_TRIP_WEAPON_TIER = EItemTier::F_Tier;
	constexpr EItemTier ECONOMY_TRIP_LEVELLED_TIER = EItemTier::E_Tier;
	/** Today's prices, as literals so a changed ladder fails here on purpose. */
	constexpr int32 ECONOMY_TRIP_CART_PRISMS = 100;  // 3 F gems x 25 + an F weapon 25
	constexpr int32 ECONOMY_TRIP_STEP_GEAR = 10;     // F to E
	constexpr int32 ECONOMY_TRIP_STEP_REALITY = 5;   // half the step
	constexpr int32 ECONOMY_TRIP_REVERT_REFUND = 5;  // half the step, Gear only
	constexpr int32 ECONOMY_TRIP_E_WEAPON_YIELD = 15; // dismantling an E weapon
	constexpr int32 ECONOMY_TRIP_GEMS_YIELD = 15;    // 3 F gems x 5
}

BEGIN_DEFINE_SPEC(FEconomyRoundTripSpec, "WoR.Economy.RoundTrip",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;
	AActor *Owner = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UCurrencyComponent *Wallet = nullptr;

	static UWeaponData *NewWeapon()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Tier = ECONOMY_TRIP_WEAPON_TIER;
		return Weapon;
	}

	/** Adds a weapon and returns its inventory entry (null with a failed check). */
	FWeaponInventoryEntry *GiveWeapon()
	{
		if (!TestTrue(TEXT("Weapon added"), Inventory->AddWeapon(NewWeapon())) || Inventory->Weapons.Num() == 0)
		{
			return nullptr;
		}
		return &Inventory->Weapons.Last();
	}

	TArray<FMerchantStockEntry> Cart() const
	{
		FMerchantStockEntry Gems;
		Gems.Crystal = ECONOMY_TRIP_GEM;
		Gems.Count = ECONOMY_TRIP_GEM_COUNT;
		FMerchantStockEntry Weapon;
		Weapon.Asset = NewWeapon();
		return {Gems, Weapon};
	}
END_DEFINE_SPEC(FEconomyRoundTripSpec)

void FEconomyRoundTripSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Owner)
		{
			FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
			FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
			Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
			Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
			FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);
		}
		if (!TestNotNull(TEXT("EconomyService"), Economy) || !TestNotNull(TEXT("Inventory"), Inventory) ||
			!TestNotNull(TEXT("Wallet"), Wallet))
		{
			Economy = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		Owner = nullptr;
		Inventory = nullptr;
		Wallet = nullptr;
		TestWorld.Reset();
	});

	It("prices a cart as the sum of its Prisms prices", [this]()
	{
		if (!Economy) return;
		const FPurchaseCost Cost = Economy->PreviewCartCost(Cart());
		TestEqual(TEXT("Prisms"), Cost.Prisms, ECONOMY_TRIP_CART_PRISMS);
		TestEqual(TEXT("No skill essence for crystals and bare gear"), Cost.SkillEssence, 0);
		TestEqual(TEXT("No typed essence"), Cost.Typed.Num(), 0);
	});

	It("spends exactly the cart's price, or nothing when short", [this]()
	{
		if (!Economy) return;
		Wallet->Add(ECurrencyType::Prisms, ECONOMY_TRIP_CART_PRISMS - ECONOMY_TRIP_SHORT);
		TestFalse(TEXT("Short by one: refused"), Economy->Purchase(Owner, Cart()));
		TestEqual(TEXT("Nothing spent"), Wallet->GetBalance(ECurrencyType::Prisms), ECONOMY_TRIP_CART_PRISMS - ECONOMY_TRIP_SHORT);
		TestEqual(TEXT("Nothing given"), Inventory->Weapons.Num(), 0);

		Wallet->Add(ECurrencyType::Prisms, ECONOMY_TRIP_SHORT);
		TestTrue(TEXT("Exact: bought"), Economy->Purchase(Owner, Cart()));
		TestEqual(TEXT("All spent"), Wallet->GetBalance(ECurrencyType::Prisms), 0);
		TestEqual(TEXT("The weapon arrived"), Inventory->Weapons.Num(), 1);
		const UCrystalInventoryComponent *Crystals = Owner->FindComponentByClass<UCrystalInventoryComponent>();
		TestEqual(TEXT("The gems arrived"), Crystals->GetCount(ECONOMY_TRIP_GEM), ECONOMY_TRIP_GEM_COUNT);
	});

	It("levels a weapon for Gear plus half in Reality, and refunds half the Gear on the way back", [this]()
	{
		if (!Economy) return;
		FWeaponInventoryEntry *Entry = GiveWeapon();
		if (!Entry) return;
		const FGuid Id = Entry->PersistentID;
		Wallet->AddGearEssence(ECONOMY_TRIP_STEP_GEAR);
		Wallet->AddEssenceType(EEssenceType::Reality, ECONOMY_TRIP_STEP_REALITY);

		TestTrue(TEXT("Levelled"), Economy->LevelUpWeapon(Owner, Id));
		TestEqual(TEXT("One tier up"), Inventory->Weapons.Last().Tier, ECONOMY_TRIP_LEVELLED_TIER);
		TestEqual(TEXT("Gear essence spent"), Wallet->GetGearEssence(), 0);
		TestEqual(TEXT("Reality spent"), Wallet->GetEssenceType(EEssenceType::Reality), 0);

		TestTrue(TEXT("Reverted"), Economy->DowngradeWeapon(Owner, Id));
		TestEqual(TEXT("Back to its authored tier"), Inventory->Weapons.Last().Tier, ECONOMY_TRIP_WEAPON_TIER);
		TestEqual(TEXT("Half the Gear essence back"), Wallet->GetGearEssence(), ECONOMY_TRIP_REVERT_REFUND);
		TestEqual(TEXT("No Reality back"), Wallet->GetEssenceType(EEssenceType::Reality), 0);
		TestFalse(TEXT("Never below the authored tier"), Economy->DowngradeWeapon(Owner, Id));
	});

	It("refuses a level-up it can't fully pay for, and spends nothing", [this]()
	{
		if (!Economy) return;
		FWeaponInventoryEntry *Entry = GiveWeapon();
		if (!Entry) return;
		const FGuid Id = Entry->PersistentID;
		Wallet->AddGearEssence(ECONOMY_TRIP_STEP_GEAR);
		TestFalse(TEXT("No Reality: refused"), Economy->LevelUpWeapon(Owner, Id));
		TestEqual(TEXT("Gear essence kept"), Wallet->GetGearEssence(), ECONOMY_TRIP_STEP_GEAR);
		TestEqual(TEXT("Tier unchanged"), Inventory->Weapons.Last().Tier, ECONOMY_TRIP_WEAPON_TIER);
	});

	It("pays a weapon's dismantle by its levelled tier, and a crystal's by type and count", [this]()
	{
		if (!Economy) return;
		FWeaponInventoryEntry *Entry = GiveWeapon();
		if (!Entry) return;
		Entry->Tier = ECONOMY_TRIP_LEVELLED_TIER;
		TestTrue(TEXT("Weapon dismantled"), Economy->DismantleWeapon(Owner, Entry->PersistentID));
		TestEqual(TEXT("Paid by the levelled tier"), Wallet->GetGearEssence(), ECONOMY_TRIP_E_WEAPON_YIELD);
		TestEqual(TEXT("Weapon gone"), Inventory->Weapons.Num(), 0);

		Inventory->AddCrystal(ECONOMY_TRIP_GEM, ECONOMY_TRIP_GEM_COUNT);
		TestTrue(TEXT("Gems dismantled"), Economy->DismantleCrystal(Owner, ECONOMY_TRIP_GEM, ECONOMY_TRIP_GEM_COUNT));
		TestEqual(TEXT("Typed essence per gem"),
				  Wallet->GetEssenceType(EconomyYield::ResolveEssenceType(ECONOMY_TRIP_GEM)),
				  ECONOMY_TRIP_GEMS_YIELD);
		TestFalse(TEXT("None left to dismantle"), Economy->DismantleCrystal(Owner, ECONOMY_TRIP_GEM, ECONOMY_TRIP_GEM_COUNT));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
