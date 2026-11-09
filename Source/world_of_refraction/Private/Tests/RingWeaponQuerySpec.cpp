// RingWeaponQuerySpec.cpp
// The ring queries and the ring switch on ULoadoutComponent, and the ring auto-switch when a
// ring's crystal breaks (B-024). First written against URingManager / UWeaponManager, which
// forwarded to the component; their checks went with them (B-154).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	constexpr int32 RING_TEST_INTACT_DURABILITY = 10;
	constexpr int32 RING_TEST_BROKEN_DURABILITY = 0;

	template <class TComponent>
	TComponent *RingTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	FRingLoadoutEntry RingTestEntry(ECrystalType Gem, int32 Durability)
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Entry.RingEntry.AttachedItem.Crystal = FCrystalAttachment(FCrystalId(Gem, EItemTier::F_Tier), Durability);
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FRingWeaponQuerySpec, "WoR.Loadout.RingWeaponQueries",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UCrystalManager *CrystalManager = nullptr;
UInventoryComponent *Inventory = nullptr;

/** A Resonator wearing the given rings, ring 0 active. */
ULoadoutComponent *SpawnResonator(const TArray<FRingLoadoutEntry> &Rings)
{
	UCharacterData *Data = FWoRTestWorld::NewCharacterData();
	Data->CharacterClass = ECharacterClass::Resonator;
	AActor *Actor = TestWorld->SpawnCombatant(Data);
	Inventory = RingTestAddComponent<UInventoryComponent>(Actor);
	ULoadoutComponent *Loadout = RingTestAddComponent<ULoadoutComponent>(Actor);
	Loadout->InitializeFromCharacterData(Data, Inventory);
	if (!TestTrue(TEXT("active loadout exists"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
	{
		return nullptr;
	}
	FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	Active.RingLoadout = Rings;
	Active.ActiveRingIndex = 0;
	return Loadout;
}

int32 ActiveRingIndex() const
{
	return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].ActiveRingIndex;
}
END_DEFINE_SPEC(FRingWeaponQuerySpec)

void FRingWeaponQuerySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		CrystalManager = TestWorld->GetSubsystem<UCrystalManager>();
		TestNotNull(TEXT("crystal manager"), CrystalManager);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Inventory = nullptr;
	});

	Describe("ring queries", [this]()
	{
		It("give the active ring and the active ring's element", [this]()
		{
			const TArray<FRingLoadoutEntry> Rings = {RingTestEntry(ECrystalType::Garnet, RING_TEST_INTACT_DURABILITY),
													 RingTestEntry(ECrystalType::Sapphire, RING_TEST_INTACT_DURABILITY)};
			ULoadoutComponent *Loadout = SpawnResonator(Rings);
			if (!Loadout)
			{
				return;
			}
			TestEqual(TEXT("active ring"), Loadout->GetActiveRing(), Rings[0].RingEntry.Ring);
			TestEqual(TEXT("loadout's active element"), Loadout->GetActiveRingElement(),
					  CrystalTypeHelpers::GetElement(ECrystalType::Garnet));
		});
	});

	Describe("switching rings", [this]()
	{
		It("skips a broken ring", [this]()
		{
			ULoadoutComponent *Loadout = SpawnResonator({RingTestEntry(ECrystalType::Garnet, RING_TEST_INTACT_DURABILITY),
														 RingTestEntry(ECrystalType::Citrine, RING_TEST_BROKEN_DURABILITY),
														 RingTestEntry(ECrystalType::Sapphire, RING_TEST_INTACT_DURABILITY)});
			if (!Loadout)
			{
				return;
			}
			TestTrue(TEXT("switched"), Loadout->SwitchToNextUsableRing());
			TestEqual(TEXT("landed past the broken ring"), ActiveRingIndex(), 2);
			TestTrue(TEXT("loadout switch wraps"), Loadout->SwitchToNextUsableRing());
			TestEqual(TEXT("wrapped past the broken ring to the first"), ActiveRingIndex(), 0);
		});

		It("fails when no other ring is usable", [this]()
		{
			ULoadoutComponent *Loadout = SpawnResonator({RingTestEntry(ECrystalType::Garnet, RING_TEST_BROKEN_DURABILITY),
														 RingTestEntry(ECrystalType::Citrine, RING_TEST_BROKEN_DURABILITY)});
			if (!Loadout)
			{
				return;
			}
			TestFalse(TEXT("no loadout switch"), Loadout->SwitchToNextUsableRing());
			TestEqual(TEXT("stayed"), ActiveRingIndex(), 0);
		});

		It("moves to the next ring exactly once when a ring crystal breaks", [this]()
		{
			const TArray<FRingLoadoutEntry> Rings = {RingTestEntry(ECrystalType::Garnet, RING_TEST_INTACT_DURABILITY),
													 RingTestEntry(ECrystalType::Citrine, RING_TEST_INTACT_DURABILITY),
													 RingTestEntry(ECrystalType::Sapphire, RING_TEST_INTACT_DURABILITY)};
			ULoadoutComponent *Loadout = SpawnResonator(Rings);
			if (!Loadout)
			{
				return;
			}
			CrystalManager->NotifyCrystalBroken(Loadout->GetOwner(), Rings[0].RingEntry.Ring, FBrokenCrystalPayload());
			TestEqual(TEXT("one step forward"), ActiveRingIndex(), 1);
		});

		It("ignores a broken crystal on a weapon", [this]()
		{
			ULoadoutComponent *Loadout = SpawnResonator({RingTestEntry(ECrystalType::Garnet, RING_TEST_INTACT_DURABILITY),
														 RingTestEntry(ECrystalType::Citrine, RING_TEST_INTACT_DURABILITY)});
			if (!Loadout)
			{
				return;
			}
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			CrystalManager->NotifyCrystalBroken(Loadout->GetOwner(), Weapon, FBrokenCrystalPayload());
			TestEqual(TEXT("stayed"), ActiveRingIndex(), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
