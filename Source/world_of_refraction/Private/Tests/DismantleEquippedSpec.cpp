// DismantleEquippedSpec.cpp
// B-084 (D-016): dismantling an equipped weapon or ring warns first, then takes it out of every
// saved loadout; a Generic left with no gear is kept out of the fight.

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
#include "Trial/BattleGameMode.h"

namespace
{
	constexpr int32 DISMANTLE_FIRST_RING = 0;
	constexpr int32 DISMANTLE_SECOND_RING = 1;

	template <class TComponent>
	TComponent *DismantleEquippedAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FDismantleEquippedSpec, "WoR.Economy.DismantleEquipped",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;

	/** A character of Class with the components dismantle and the loadout read, one saved loadout. */
	AActor *SpawnOwner(ECharacterClass Class)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		Data->Name = TEXT("Tester");
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		DismantleEquippedAddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = DismantleEquippedAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = DismantleEquippedAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		return Actor;
	}
END_DEFINE_SPEC(FDismantleEquippedSpec)

void FDismantleEquippedSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		TestNotNull(TEXT("economy service"), Economy);
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		TestWorld.Reset();
	});

	It("warns, then leaves no ghost copy in any loadout and keeps the gearless Generic out", [this]()
	{
		AActor *Owner = SpawnOwner(ECharacterClass::Generic);
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		if (!TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage()))) ||
			!TestTrue(TEXT("a saved loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return;
		}
		const FGuid WeaponId = Inventory->Weapons[0].PersistentID;
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry = Inventory->Weapons[0];
		Inventory->SavedLoadouts.Add(Active); // equipped in a second loadout too
		TestTrue(TEXT("armed: may fight"), ABattleGameMode::CanEnterCombat(Owner));

		const FString Warning = UEconomyService::GetDismantleWarning(Owner, WeaponId).ToString();
		TestTrue(TEXT("names both loadouts"), Warning.Contains(TEXT("loadouts 1, 2")));
		TestTrue(TEXT("says upgrades are lost"), Warning.Contains(TEXT("upgrades")));
		TestTrue(TEXT("says Tester can't fight"), Warning.Contains(TEXT("Tester will have no gear")));

		TestTrue(TEXT("dismantled"), Economy->DismantleWeapon(Owner, WeaponId));
		TestEqual(TEXT("owned copy gone"), Inventory->Weapons.Num(), 0);
		for (const FCombatLoadout &Saved : Inventory->SavedLoadouts)
		{
			TestFalse(TEXT("no ghost copy in a loadout"), Saved.PrimaryWeapon.IsValid());
			TestTrue(TEXT("primary slot emptied"), Saved.PrimarySlotType == EPrimarySlotType::None);
		}
		TestNull(TEXT("no primary weapon"), Owner->FindComponentByClass<ULoadoutComponent>()->GetPrimaryWeapon());
		TestFalse(TEXT("gearless Generic refused at fight start"), ABattleGameMode::CanEnterCombat(Owner));
	});

	It("has no warning for gear no loadout holds", [this]()
	{
		AActor *Owner = SpawnOwner(ECharacterClass::Generic);
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		if (!TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage()))))
		{
			return;
		}
		TestTrue(TEXT("no warning"), UEconomyService::GetDismantleWarning(Owner, Inventory->Weapons[0].PersistentID).IsEmpty());
	});

	It("removes a Resonator's ring slot, keeps its other ring active and lets it fight", [this]()
	{
		AActor *Owner = SpawnOwner(ECharacterClass::Resonator);
		UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
		const bool bAdded = Inventory->AddRing(NewObject<URingData>(GetTransientPackage())) &&
							Inventory->AddRing(NewObject<URingData>(GetTransientPackage()));
		if (!TestTrue(TEXT("two rings added"), bAdded) ||
			!TestTrue(TEXT("a saved loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return;
		}
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.RingLoadout.SetNum(2);
		Active.RingLoadout[DISMANTLE_FIRST_RING].RingEntry = Inventory->Rings[DISMANTLE_FIRST_RING];
		Active.RingLoadout[DISMANTLE_SECOND_RING].RingEntry = Inventory->Rings[DISMANTLE_SECOND_RING];
		Active.ActiveRingIndex = DISMANTLE_SECOND_RING;
		const FGuid KeptId = Inventory->Rings[DISMANTLE_SECOND_RING].PersistentID;
		const FGuid GoneId = Inventory->Rings[DISMANTLE_FIRST_RING].PersistentID;

		const FString Warning = UEconomyService::GetDismantleWarning(Owner, GoneId).ToString();
		TestTrue(TEXT("names the loadout"), Warning.Contains(TEXT("loadout 1")));
		TestFalse(TEXT("still has a ring: no can't-fight line"), Warning.Contains(TEXT("no gear")));

		TestTrue(TEXT("dismantled"), Economy->DismantleRing(Owner, GoneId));
		const FCombatLoadout &After = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		if (!TestEqual(TEXT("one ring slot left"), After.RingLoadout.Num(), 1))
		{
			return;
		}
		TestEqual(TEXT("the other ring stays active"), After.RingLoadout[After.ActiveRingIndex].RingEntry.PersistentID, KeptId);
		TestTrue(TEXT("a Resonator with only rings may fight"), ABattleGameMode::CanEnterCombat(Owner));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
