// OneSwitchSpec.cpp
// B-305 (D-110 unified loadout, step 5): one switch for weapons and adapters. TrySwitchActiveSlot puts a slot in use
// with the turn's one free switch (ET-19); SwitchToNextUsableSlot is the crystal-break auto-switch; CountUsableSlots is
// what the command menu reads to offer Switch Weapon / Switch Adapter.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	const FCrystalId ONE_SWITCH_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 ONE_SWITCH_TEST_INTACT = 5;
	constexpr int32 ONE_SWITCH_TEST_BROKEN = 0;

	FRingLoadoutEntry OneSwitchTestAdapter(int32 Durability)
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Entry.RingEntry.AttachedItem.Crystal = FCrystalAttachment(ONE_SWITCH_TEST_GEM, Durability);
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FOneSwitchSpec, "WoR.Loadout.OneSwitch", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}
END_DEFINE_SPEC(FOneSwitchSpec)

void FOneSwitchSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("switches a Generic to its other weapon once per turn", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		UWeaponData *Primary = NewObject<UWeaponData>(GetTransientPackage());
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Primary);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		UWeaponData *Secondary = NewObject<UWeaponData>(GetTransientPackage());
		ActiveOf(Loadout).SecondarySlotType = ESecondarySlotType::Weapon;
		ActiveOf(Loadout).SecondaryWeapon.WeaponEntry.Weapon = Secondary;
		ActiveOf(Loadout).bShowPrimary = true;

		TestEqual(TEXT("two weapons to switch between"), Loadout->CountUsableSlots(EGearKind::Weapon), 2);
		FText Reason;
		TestFalse(TEXT("switching to the slot in use changes nothing"), Loadout->TrySwitchActiveSlot(EGearKind::Weapon, 0, Reason));
		TestTrue(TEXT("switch to the secondary"), Loadout->TrySwitchActiveSlot(EGearKind::Weapon, 1, Reason));
		TestTrue(TEXT("secondary in hand"), Loadout->GetActiveWeapon() == Secondary);
		TestFalse(TEXT("a second switch this turn is refused"), Loadout->TrySwitchActiveSlot(EGearKind::Weapon, 0, Reason));
		TestFalse(TEXT("with a reason"), Reason.IsEmpty());
	});

	It("moves a Resonator off a broken adapter to the next intact one", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		ActiveOf(Loadout).RingLoadout = {OneSwitchTestAdapter(ONE_SWITCH_TEST_BROKEN), OneSwitchTestAdapter(ONE_SWITCH_TEST_BROKEN),
										 OneSwitchTestAdapter(ONE_SWITCH_TEST_INTACT)};
		ActiveOf(Loadout).ActiveRingIndex = 0;
		TestEqual(TEXT("three adapters to switch between"), Loadout->CountUsableSlots(EGearKind::Adapter), 3);
		TestTrue(TEXT("auto-switched"), Loadout->SwitchToNextUsableSlot(EGearKind::Adapter));
		TestEqual(TEXT("past the broken one to the intact one"), ActiveOf(Loadout).ActiveRingIndex, 2);
		TestTrue(TEXT("the ring forward agrees"), !Loadout->SwitchToNextUsableRing()); // only one intact adapter: nothing else to move to
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
