// RingWeaponQuerySpec.cpp
// The ring and weapon queries URingManager / UWeaponManager answer, and the ring auto-switch
// when a ring's crystal breaks (B-024). Written against the two subsystems first, so the
// loadout component must give the same results once it owns them; the component's own
// GetActiveRingElement / SwitchToNextUsableRing are checked alongside.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Rings/RingManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponManager.h"
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
URingManager *RingManager = nullptr;
UWeaponManager *WeaponManager = nullptr;
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
		RingManager = TestWorld->GetSubsystem<URingManager>();
		WeaponManager = TestWorld->GetSubsystem<UWeaponManager>();
		CrystalManager = TestWorld->GetSubsystem<UCrystalManager>();
		TestNotNull(TEXT("ring manager"), RingManager);
		TestNotNull(TEXT("weapon manager"), WeaponManager);
		TestNotNull(TEXT("crystal manager"), CrystalManager);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Inventory = nullptr;
	});

	Describe("ring queries", [this]()
	{
		It("give the active ring, the primary ring and the active ring's element", [this]()
		{
			const TArray<FRingLoadoutEntry> Rings = {RingTestEntry(ECrystalType::Garnet, RING_TEST_INTACT_DURABILITY),
													 RingTestEntry(ECrystalType::Sapphire, RING_TEST_INTACT_DURABILITY)};
			ULoadoutComponent *Loadout = SpawnResonator(Rings);
			if (!Loadout)
			{
				return;
			}
			AActor *Actor = Loadout->GetOwner();
			TestEqual(TEXT("active ring"), RingManager->GetActiveRing(Actor), Rings[0].RingEntry.Ring);
			TestEqual(TEXT("active ring matches the loadout"), RingManager->GetActiveRing(Actor), Loadout->GetActiveRing());
			TestEqual(TEXT("primary ring matches the loadout"), RingManager->GetPrimaryRing(Actor), Loadout->GetPrimaryRing());
			TestEqual(TEXT("active element"), RingManager->GetActiveElement(Actor),
					  CrystalTypeHelpers::GetElement(ECrystalType::Garnet));
			TestEqual(TEXT("loadout's active element"), Loadout->GetActiveRingElement(),
					  CrystalTypeHelpers::GetElement(ECrystalType::Garnet));
		});

		It("give nothing for an actor without a loadout", [this]()
		{
			AActor *Bare = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestNull(TEXT("active ring"), RingManager->GetActiveRing(Bare));
			TestNull(TEXT("primary ring"), RingManager->GetPrimaryRing(Bare));
			TestEqual(TEXT("active element"), RingManager->GetActiveElement(Bare), ESpellElement::None);
			TestFalse(TEXT("switch"), RingManager->SwitchToNextRing(Bare));
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
			TestTrue(TEXT("switched"), RingManager->SwitchToNextRing(Loadout->GetOwner()));
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
			TestFalse(TEXT("no switch"), RingManager->SwitchToNextRing(Loadout->GetOwner()));
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

	Describe("weapon queries", [this]()
	{
		It("match the loadout's active weapon and attack", [this]()
		{
			ULoadoutComponent *Loadout = SpawnResonator({});
			if (!Loadout)
			{
				return;
			}
			AActor *Actor = Loadout->GetOwner();
			TestEqual(TEXT("active weapon"), WeaponManager->GetActiveWeapon(Actor), Loadout->GetActiveWeapon());
			TestEqual(TEXT("active attack"), WeaponManager->GetActiveAttack(Actor), Loadout->GetCurrentAttack());
			AActor *Bare = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestNull(TEXT("no loadout: weapon"), WeaponManager->GetActiveWeapon(Bare));
			TestNull(TEXT("no loadout: attack"), WeaponManager->GetActiveAttack(Bare));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
