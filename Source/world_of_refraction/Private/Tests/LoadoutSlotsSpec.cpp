// LoadoutSlotsSpec.cpp
// B-302 (D-110 unified loadout, step 2): FCombatLoadout::Slots mirrors the old weapon / ring fields. Order: primary,
// secondary weapon, then the Resonator's adapters. The active weapon and adapter are the ones today's getters pick,
// and on a live character the mirror agrees with them (wor.Loadout.CompareSlots says "match").

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Debug/Inventory/LoadoutRulesDebug.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	constexpr int32 LOADOUT_SLOTS_TEST_RINGS = 3;
	constexpr int32 LOADOUT_SLOTS_TEST_ACTIVE_RING = 1;
	constexpr int32 LOADOUT_SLOTS_TEST_STALE_RING = 7;

	FWeaponLoadoutEntry LoadoutSlotsTestWeapon()
	{
		FWeaponLoadoutEntry Entry;
		Entry.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		return Entry;
	}

	FRingLoadoutEntry LoadoutSlotsTestRing()
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		return Entry;
	}

	FCombatLoadout LoadoutSlotsTestDualWeapons(bool bShowPrimary)
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = LoadoutSlotsTestWeapon();
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = LoadoutSlotsTestWeapon();
		Loadout.bShowPrimary = bShowPrimary;
		return Loadout;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutSlotsSpec, "WoR.Loadout.Slots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutSlotsSpec)

void FLoadoutSlotsSpec::Define()
{
	It("mirrors a Generic's two weapons, with bShowPrimary picking the one in hand", [this]()
	{
		FCombatLoadout Shown = LoadoutSlotsTestDualWeapons(true);
		Shown.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestEqual(TEXT("two slots"), Shown.Slots.Num(), 2);
		TestTrue(TEXT("both weapons"), Shown.Slots[0].Kind() == EGearKind::Weapon && Shown.Slots[1].Kind() == EGearKind::Weapon);
		TestEqual(TEXT("primary in hand"), Shown.ActiveWeaponSlot, 0);
		TestEqual(TEXT("no adapter"), Shown.ActiveAdapterSlot, static_cast<int32>(INDEX_NONE));

		FCombatLoadout Hidden = LoadoutSlotsTestDualWeapons(false);
		Hidden.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestEqual(TEXT("secondary in hand"), Hidden.ActiveWeaponSlot, 1);
	});

	It("ignores bShowPrimary for a class that isn't Generic", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = LoadoutSlotsTestWeapon();
		Loadout.bShowPrimary = false;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Caster);
		TestEqual(TEXT("primary in hand"), Loadout.ActiveWeaponSlot, 0);
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestEqual(TEXT("a Generic showing an empty secondary holds nothing"), Loadout.ActiveWeaponSlot, static_cast<int32>(INDEX_NONE));
	});

	It("mirrors a Generic's adapter primary with its secondary weapon in hand", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Ring;
		Loadout.PrimaryRing = LoadoutSlotsTestRing();
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = LoadoutSlotsTestWeapon();
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestTrue(TEXT("adapter then weapon"), Loadout.Slots.Num() == 2 && Loadout.Slots[0].Kind() == EGearKind::Adapter &&
												   Loadout.Slots[1].Kind() == EGearKind::Weapon);
		TestEqual(TEXT("active adapter"), Loadout.ActiveAdapterSlot, 0);
		TestEqual(TEXT("secondary in hand"), Loadout.ActiveWeaponSlot, 1);
	});

	It("mirrors a Resonator's adapters with its active ring, and none for a stale index", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = LoadoutSlotsTestWeapon();
		for (int32 Ring = 0; Ring < LOADOUT_SLOTS_TEST_RINGS; ++Ring)
		{
			Loadout.RingLoadout.Add(LoadoutSlotsTestRing());
		}
		Loadout.ActiveRingIndex = LOADOUT_SLOTS_TEST_ACTIVE_RING;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Resonator);
		TestEqual(TEXT("weapon plus three adapters"), Loadout.Slots.Num(), 1 + LOADOUT_SLOTS_TEST_RINGS);
		TestEqual(TEXT("weapon in hand"), Loadout.ActiveWeaponSlot, 0);
		TestEqual(TEXT("active adapter after the weapon"), Loadout.ActiveAdapterSlot, 1 + LOADOUT_SLOTS_TEST_ACTIVE_RING);
		TestTrue(TEXT("the same adapter"),
				 Loadout.Slots[Loadout.ActiveAdapterSlot].Gear.GetAdapter() == Loadout.RingLoadout[LOADOUT_SLOTS_TEST_ACTIVE_RING].RingEntry.Ring);

		Loadout.ActiveRingIndex = LOADOUT_SLOTS_TEST_STALE_RING;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Resonator);
		TestEqual(TEXT("stale index: no active adapter"), Loadout.ActiveAdapterSlot, static_cast<int32>(INDEX_NONE));
	});

	It("follows a Generic's empty primary as GetActiveWeapon does (a sold primary weapon)", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::None;
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = LoadoutSlotsTestWeapon();
		Loadout.bShowPrimary = true;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestEqual(TEXT("showing the empty primary: nothing in hand"), Loadout.ActiveWeaponSlot, static_cast<int32>(INDEX_NONE));
		Loadout.bShowPrimary = false;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Generic);
		TestEqual(TEXT("showing the secondary"), Loadout.ActiveWeaponSlot, 0);
	});

	It("gives an evolution primary no weapon or adapter slot", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Caster);
		TestEqual(TEXT("no slots"), Loadout.Slots.Num(), 0);
		TestEqual(TEXT("no weapon"), Loadout.ActiveWeaponSlot, static_cast<int32>(INDEX_NONE));
	});

	It("agrees with the old getters on a live character after a gear change", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("Test world created"), TestWorld.Create())) return;
		ULoadoutComponent *Loadout = TestWorld.SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(),
																		  NewObject<UWeaponData>(GetTransientPackage()));
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		Loadout->NotifyActiveWeaponChanged();
		const FString Compare = ULoadoutRulesDebug::GetCompareSlotsString(Loadout);
		TestTrue(TEXT("mirror matches: ") + Compare, Compare.EndsWith(TEXT("match")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
