// DuplicateGearSpec.cpp
// B-319 (D-115): a character keeps one copy of a weapon or adapter. The weaker copy converts to Gold or is dismantled, and its loadout slots move to the kept copy.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	template <class TComponent>
	TComponent *DuplicateGearAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FDuplicateGearSpec, "WoR.Economy.DuplicateGear",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;
	AActor *Owner = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UCurrencyComponent *Currency = nullptr;

	/** Two copies of one weapon; the second at Tier. The first is the active loadout's primary. */
	void OwnTwoSwords(EItemTier SecondTier)
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		Inventory->AddWeapon(Sword);
		Inventory->AddWeapon(Sword);
		if (Inventory->Weapons.Num() == 2)
		{
			Inventory->Weapons[1].Tier = SecondTier;
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimarySlotType = EPrimarySlotType::Weapon;
			Active.PrimaryWeapon.WeaponEntry = Inventory->Weapons[0];
		}
	}
END_DEFINE_SPEC(FDuplicateGearSpec)

void FDuplicateGearSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		Owner = TestWorld->SpawnCombatant(Data);
		Currency = DuplicateGearAddComponent<UCurrencyComponent>(Owner);
		Inventory = DuplicateGearAddComponent<UInventoryComponent>(Owner);
		DuplicateGearAddComponent<ULoadoutComponent>(Owner)->InitializeFromCharacterData(Data, Inventory);
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		Owner = nullptr;
		Inventory = nullptr;
		Currency = nullptr;
		TestWorld.Reset();
	});

	It("pairs the weaker copy with the one kept", [this]()
	{
		OwnTwoSwords(EItemTier::C_Tier);
		if (!TestEqual(TEXT("two swords"), Inventory->Weapons.Num(), 2)) return;

		const TArray<FDuplicateGearPair> Pairs = Inventory->FindDuplicateGear();
		if (!TestEqual(TEXT("one pair"), Pairs.Num(), 1)) return;
		TestEqual(TEXT("the higher tier is kept"), Pairs[0].KeptID, Inventory->Weapons[1].PersistentID);
		TestEqual(TEXT("the first is the weaker"), Pairs[0].WeakerID, Inventory->Weapons[0].PersistentID);
		TestFalse(TEXT("a weapon pair"), Pairs[0].bRing);
	});

	It("keeps the copy owned first on a tie", [this]()
	{
		OwnTwoSwords(EItemTier::E_Tier); // the asset's default tier, so both copies match
		const TArray<FDuplicateGearPair> Pairs = Inventory->FindDuplicateGear();
		if (!TestEqual(TEXT("one pair"), Pairs.Num(), 1)) return;
		TestEqual(TEXT("the first is kept"), Pairs[0].KeptID, Inventory->Weapons[0].PersistentID);
	});

	It("converts the weaker primary to Gold and puts the kept copy in its slot", [this]()
	{
		OwnTwoSwords(EItemTier::C_Tier);
		if (!TestEqual(TEXT("two swords"), Inventory->Weapons.Num(), 2)) return;
		const FGuid WeakerID = Inventory->Weapons[0].PersistentID;
		const FGuid KeptID = Inventory->Weapons[1].PersistentID;
		const EItemTier WeakerTier = Inventory->Weapons[0].Tier;
		const int32 GoldBefore = Currency->GetBalance(ECurrencyType::Gold);

		TestTrue(TEXT("converted"), Economy->ResolveDuplicate(Owner, WeakerID, /*bConvertToGold=*/true));
		TestEqual(TEXT("one sword left"), Inventory->Weapons.Num(), 1);
		TestNull(TEXT("the weaker copy is gone"), Inventory->FindOwnedWeapon(WeakerID));
		TestEqual(TEXT("Gold for the weaker copy's tier"), Currency->GetBalance(ECurrencyType::Gold) - GoldBefore, UEconomyService::GetDuplicateGoldForTier(WeakerTier));
		const FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TestEqual(TEXT("the kept copy is the primary"), Active.PrimaryWeapon.WeaponEntry.PersistentID, KeptID);
		TestTrue(TEXT("no duplicates left"), Inventory->FindDuplicateGear().IsEmpty());
	});

	It("dismantles the weaker adapter for Gear essence", [this]()
	{
		URingData *Ring = NewObject<URingData>(GetTransientPackage());
		Inventory->AddRing(Ring);
		Inventory->AddRing(Ring);
		const TArray<FDuplicateGearPair> Pairs = Inventory->FindDuplicateGear();
		if (!TestEqual(TEXT("one pair"), Pairs.Num(), 1)) return;
		TestTrue(TEXT("an adapter pair"), Pairs[0].bRing);
		const int32 EssenceBefore = Currency->GetGearEssence();

		TestTrue(TEXT("dismantled"), Economy->ResolveDuplicate(Owner, Pairs[0].WeakerID, /*bConvertToGold=*/false));
		TestEqual(TEXT("one adapter left"), Inventory->Rings.Num(), 1);
		TestTrue(TEXT("Gear essence granted"), Currency->GetGearEssence() > EssenceBefore);
	});

	It("refuses a piece that is no weaker copy", [this]()
	{
		OwnTwoSwords(EItemTier::C_Tier);
		if (!TestEqual(TEXT("two swords"), Inventory->Weapons.Num(), 2)) return;
		TestFalse(TEXT("the kept copy is refused"), Economy->ResolveDuplicate(Owner, Inventory->Weapons[1].PersistentID, true));
		TestEqual(TEXT("both still owned"), Inventory->Weapons.Num(), 2);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
