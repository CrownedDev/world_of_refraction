// AlwaysPrimarySpec.cpp
// B-320 (D-116): a character always has one valid primary. Dismantling a primary is refused; a
// active loadout left with none (an old save, a strip) gets the first usable owned piece.

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
	TComponent *AlwaysPrimaryAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	/** Empties the primary the way an old save or a strip leaves it. */
	void AlwaysPrimaryEmpty(FCombatLoadout &Loadout)
	{
		Loadout.PrimarySlotType = EPrimarySlotType::None;
		Loadout.PrimaryWeapon.Clear();
		Loadout.PrimaryRing.Clear();
	}
}

BEGIN_DEFINE_SPEC(FAlwaysPrimarySpec, "WoR.Loadout.AlwaysPrimary",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;

	/** A character of Class with currency, inventory and loadout, one saved loadout. */
	UInventoryComponent *SpawnInventory(ECharacterClass Class, AActor *&OutOwner)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		Data->Name = TEXT("Tester");
		OutOwner = TestWorld->SpawnCombatant(Data);
		AlwaysPrimaryAddComponent<UCurrencyComponent>(OutOwner);
		UInventoryComponent *Inventory = AlwaysPrimaryAddComponent<UInventoryComponent>(OutOwner);
		ULoadoutComponent *Loadout = AlwaysPrimaryAddComponent<ULoadoutComponent>(OutOwner);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		return Inventory;
	}
END_DEFINE_SPEC(FAlwaysPrimarySpec)

void FAlwaysPrimarySpec::Define()
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

	It("fills an empty primary with the first usable weapon, skipping the excluded one and the secondary", [this]()
	{
		AActor *Owner = nullptr;
		UInventoryComponent *Inventory = SpawnInventory(ECharacterClass::Generic, Owner);
		const bool bAdded = Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())) &&
							Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())) &&
							Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		if (!TestTrue(TEXT("three weapons added"), bAdded) ||
			!TestTrue(TEXT("a saved loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return;
		}
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		AlwaysPrimaryEmpty(Active);
		Active.SecondarySlotType = ESecondarySlotType::Weapon;
		Active.SecondaryWeapon.WeaponEntry = Inventory->Weapons[0];
		TestFalse(TEXT("no primary before"), UInventoryComponent::LoadoutHasPrimary(Active));

		TestTrue(TEXT("filled"), Inventory->RefillActivePrimary(Inventory->Weapons[1].PersistentID));
		const FCombatLoadout &After = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TestTrue(TEXT("a weapon primary"), After.PrimarySlotType == EPrimarySlotType::Weapon);
		TestEqual(TEXT("the first usable weapon"), After.PrimaryWeapon.WeaponEntry.PersistentID, Inventory->Weapons[2].PersistentID);
		TestTrue(TEXT("it is now a primary"), Inventory->IsPrimaryInAnyLoadout(Inventory->Weapons[2].PersistentID));
		TestFalse(TEXT("a full loadout is left alone"), Inventory->RefillActivePrimary());
	});

	It("gives a Generic an adapter primary when no weapon is owned, but never a Resonator", [this]()
	{
		AActor *GenericOwner = nullptr;
		UInventoryComponent *Generic = SpawnInventory(ECharacterClass::Generic, GenericOwner);
		AActor *ResonatorOwner = nullptr;
		UInventoryComponent *Resonator = SpawnInventory(ECharacterClass::Resonator, ResonatorOwner);
		const bool bAdded = Generic->AddRing(NewObject<URingData>(GetTransientPackage())) &&
							Resonator->AddRing(NewObject<URingData>(GetTransientPackage()));
		if (!TestTrue(TEXT("rings added"), bAdded) ||
			!TestTrue(TEXT("saved loadouts"), Generic->SavedLoadouts.Num() > 0 && Resonator->SavedLoadouts.Num() > 0))
		{
			return;
		}
		FCombatLoadout &GenericActive = Generic->SavedLoadouts[Generic->ActiveLoadoutIndex];
		FCombatLoadout &ResonatorActive = Resonator->SavedLoadouts[Resonator->ActiveLoadoutIndex];
		AlwaysPrimaryEmpty(GenericActive);
		AlwaysPrimaryEmpty(ResonatorActive);

		TestTrue(TEXT("Generic: filled"), Generic->RefillActivePrimary());
		TestTrue(TEXT("Generic: adapter primary"), GenericActive.PrimarySlotType == EPrimarySlotType::Ring);
		TestEqual(TEXT("Generic: the owned adapter"), GenericActive.PrimaryRing.RingEntry.PersistentID, Generic->Rings[0].PersistentID);

		TestFalse(TEXT("Resonator: nothing filled"), Resonator->RefillActivePrimary());
		TestFalse(TEXT("Resonator: still no primary (the guard fallback)"), UInventoryComponent::LoadoutHasPrimary(ResonatorActive));
	});

	It("refuses to dismantle an adapter that is a primary", [this]()
	{
		AActor *Owner = nullptr;
		UInventoryComponent *Inventory = SpawnInventory(ECharacterClass::Generic, Owner);
		if (!TestTrue(TEXT("ring added"), Inventory->AddRing(NewObject<URingData>(GetTransientPackage()))) ||
			!TestTrue(TEXT("a saved loadout"), Inventory->SavedLoadouts.Num() > 0))
		{
			return;
		}
		FCombatLoadout &Loadout = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Loadout.PrimarySlotType = EPrimarySlotType::Ring;
		Loadout.PrimaryRing.RingEntry = Inventory->Rings[0];
		const FGuid RingId = Inventory->Rings[0].PersistentID;

		TestFalse(TEXT("refused"), Economy->DismantleRing(Owner, RingId));
		TestEqual(TEXT("still owned"), Inventory->Rings.Num(), 1);
		TestEqual(TEXT("still the primary"), Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryRing.RingEntry.PersistentID, RingId);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
