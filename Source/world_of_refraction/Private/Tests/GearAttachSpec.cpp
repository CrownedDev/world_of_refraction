// GearAttachSpec.cpp
// UInventoryComponent's weapon and ring socket operations: attach a crystal, stone, fusion or
// evolution, then remove it (B-026). Written against the hand-copied weapon/ring pairs first, so
// the shared template must give the same results. Not covered: the "no authority" refusal (a
// standalone test world always has authority) and the turn-order notify for a speed stone (no
// combat is running).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	template <class TComponent>
	TComponent *GearTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	const FCrystalId GEAR_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId GEAR_TEST_OTHER_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	const FCrystalId GEAR_TEST_STAT_STONE(ECrystalType::DamageStone, EItemTier::F_Tier);
	const FCrystalId GEAR_TEST_ABILITY_STONE(ECrystalType::AbilityStone, EItemTier::F_Tier);
	constexpr int32 GEAR_TEST_STACK = 3;
	constexpr int32 GEAR_TEST_EVO_MAX_DURABILITY = 100;
	constexpr int32 GEAR_TEST_EVO_AFTER_REMOVAL = 90; // 10% removal wear
	constexpr int32 GEAR_TEST_EVO_LOW_DURABILITY = 5; // under 10%, so removal breaks it

	FFusionId GearTestFusion(const FCrystalId &HalfA, const FCrystalId &HalfB)
	{
		FFusionId Fusion;
		Fusion.HalfA = HalfA;
		Fusion.HalfB = HalfB;
		return Fusion;
	}
}

BEGIN_DEFINE_SPEC(FGearAttachSpec, "WoR.Inventory.GearAttach",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
AActor *Owner = nullptr;
UInventoryComponent *Inventory = nullptr;
UCrystalInventoryComponent *Crystals = nullptr;
UEvolutionInventoryComponent *Evolutions = nullptr;
FGuid WeaponId;
FGuid RingId;

/** Adds an owned evolution at full durability and returns its instance id. */
FGuid AddEvolution()
{
	UEvolutionItemData *Item = NewObject<UEvolutionItemData>(GetTransientPackage());
	Item->MaxDurability = GEAR_TEST_EVO_MAX_DURABILITY;
	TestTrue(TEXT("evolution added"), Evolutions->AddInstance(Item));
	return Evolutions->Entries.Num() > 0 ? Evolutions->Entries.Last().InstanceID : FGuid();
}

const FEvolutionInventoryEntry *FindEvolution(const FGuid &Id) const
{
	return Evolutions->Entries.FindByPredicate([&Id](const FEvolutionInventoryEntry &E) { return E.InstanceID == Id; });
}
END_DEFINE_SPEC(FGearAttachSpec)

void FGearAttachSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Crystals = GearTestAddComponent<UCrystalInventoryComponent>(Owner);
		Evolutions = GearTestAddComponent<UEvolutionInventoryComponent>(Owner);
		GearTestAddComponent<UCurrencyComponent>(Owner);
		Inventory = GearTestAddComponent<UInventoryComponent>(Owner);
		GearTestAddComponent<ULoadoutComponent>(Owner);

		TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())));
		TestTrue(TEXT("ring added"), Inventory->AddRing(NewObject<URingData>(GetTransientPackage())));
		WeaponId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();
		RingId = Inventory->Rings.Num() > 0 ? Inventory->Rings[0].PersistentID : FGuid();
		for (const FCrystalId &Id : {GEAR_TEST_GEM, GEAR_TEST_OTHER_GEM, GEAR_TEST_STAT_STONE, GEAR_TEST_ABILITY_STONE})
		{
			TestTrue(TEXT("crystals added"), Inventory->AddCrystal(Id, GEAR_TEST_STACK));
		}
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	Describe("crystal attach", [this]()
	{
		It("sockets a gem on a weapon and a ring, debiting one each", [this]()
		{
			TestTrue(TEXT("weapon attach"), Inventory->AttachCrystalToWeapon(WeaponId, GEAR_TEST_GEM));
			TestTrue(TEXT("ring attach"), Inventory->AttachCrystalToRing(RingId, GEAR_TEST_GEM));
			TestTrue(TEXT("weapon holds a crystal"), Inventory->Weapons[0].AttachedItem.IsCrystal());
			TestTrue(TEXT("ring holds a crystal"), Inventory->Rings[0].AttachedItem.IsCrystal());
			TestTrue(TEXT("ring crystal id"), Inventory->Rings[0].AttachedItem.Crystal.Id == GEAR_TEST_GEM);
			TestTrue(TEXT("gem starts with durability"), Inventory->Weapons[0].AttachedItem.Crystal.CurrentDurability > 0);
			TestEqual(TEXT("two gems debited"), Crystals->GetCount(GEAR_TEST_GEM), GEAR_TEST_STACK - 2);
		});

		It("sockets a stone on a weapon as an augment stone", [this]()
		{
			TestTrue(TEXT("weapon attach"), Inventory->AttachCrystalToWeapon(WeaponId, GEAR_TEST_STAT_STONE));
			TestTrue(TEXT("augment stone"), Inventory->Weapons[0].AttachedItem.IsAugmentStone());
			TestEqual(TEXT("stone never wears"), Inventory->Weapons[0].AttachedItem.Crystal.CurrentDurability, 0);
			TestEqual(TEXT("stone debited"), Crystals->GetCount(GEAR_TEST_STAT_STONE), GEAR_TEST_STACK - 1);
		});

		It("refuses a stone on a ring without debiting it", [this]()
		{
			TestFalse(TEXT("ring attach refused"), Inventory->AttachCrystalToRing(RingId, GEAR_TEST_STAT_STONE));
			TestTrue(TEXT("ring still empty"), Inventory->Rings[0].AttachedItem.IsEmpty());
			TestEqual(TEXT("nothing debited"), Crystals->GetCount(GEAR_TEST_STAT_STONE), GEAR_TEST_STACK);
		});

		It("refuses an occupied slot, an unknown gear id and an unowned crystal", [this]()
		{
			const FCrystalId Unowned(ECrystalType::Citrine, EItemTier::F_Tier);
			TestTrue(TEXT("first weapon attach"), Inventory->AttachCrystalToWeapon(WeaponId, GEAR_TEST_GEM));
			TestTrue(TEXT("first ring attach"), Inventory->AttachCrystalToRing(RingId, GEAR_TEST_GEM));
			TestFalse(TEXT("occupied weapon"), Inventory->AttachCrystalToWeapon(WeaponId, GEAR_TEST_GEM));
			TestFalse(TEXT("occupied ring"), Inventory->AttachCrystalToRing(RingId, GEAR_TEST_GEM));
			TestEqual(TEXT("only the first two debited"), Crystals->GetCount(GEAR_TEST_GEM), GEAR_TEST_STACK - 2);
			TestFalse(TEXT("unknown weapon"), Inventory->AttachCrystalToWeapon(FGuid::NewGuid(), GEAR_TEST_GEM));
			TestFalse(TEXT("unknown ring"), Inventory->AttachCrystalToRing(FGuid::NewGuid(), GEAR_TEST_GEM));

			TestTrue(TEXT("weapon emptied"), Inventory->RemoveCrystalFromWeapon(0));
			TestTrue(TEXT("ring emptied"), Inventory->RemoveCrystalFromRing(0));
			TestFalse(TEXT("unowned on weapon"), Inventory->AttachCrystalToWeapon(WeaponId, Unowned));
			TestFalse(TEXT("unowned on ring"), Inventory->AttachCrystalToRing(RingId, Unowned));
			TestTrue(TEXT("weapon stays empty"), Inventory->Weapons[0].AttachedItem.IsEmpty());
			TestTrue(TEXT("ring stays empty"), Inventory->Rings[0].AttachedItem.IsEmpty());
		});
	});

	Describe("fusion attach", [this]()
	{
		It("fuses an elemental pair on a weapon and a ring, debiting both halves", [this]()
		{
			const FFusionId Elemental = GearTestFusion(GEAR_TEST_STAT_STONE, GEAR_TEST_GEM);
			TestTrue(TEXT("weapon fuse"), Inventory->AttachFusionToWeapon(WeaponId, Elemental));
			TestTrue(TEXT("ring fuse"), Inventory->AttachFusionToRing(RingId, Elemental));
			TestTrue(TEXT("weapon holds a fusion"), Inventory->Weapons[0].AttachedItem.IsFusion());
			TestTrue(TEXT("ring holds a fusion"), Inventory->Rings[0].AttachedItem.IsFusion());
			TestEqual(TEXT("stones debited"), Crystals->GetCount(GEAR_TEST_STAT_STONE), GEAR_TEST_STACK - 2);
			TestEqual(TEXT("gems debited"), Crystals->GetCount(GEAR_TEST_GEM), GEAR_TEST_STACK - 2);
		});

		It("fuses an augmented pair on a weapon but refuses it on a ring", [this]()
		{
			const FFusionId Augmented = GearTestFusion(GEAR_TEST_STAT_STONE, GEAR_TEST_ABILITY_STONE);
			TestFalse(TEXT("ring refuses"), Inventory->AttachFusionToRing(RingId, Augmented));
			TestTrue(TEXT("ring still empty"), Inventory->Rings[0].AttachedItem.IsEmpty());
			TestEqual(TEXT("nothing debited by the ring"), Crystals->GetCount(GEAR_TEST_ABILITY_STONE), GEAR_TEST_STACK);
			TestTrue(TEXT("weapon accepts"), Inventory->AttachFusionToWeapon(WeaponId, Augmented));
			TestEqual(TEXT("ability stone debited"), Crystals->GetCount(GEAR_TEST_ABILITY_STONE), GEAR_TEST_STACK - 1);
		});

		It("refuses an invalid pair and an occupied slot", [this]()
		{
			const FFusionId TwoGems = GearTestFusion(GEAR_TEST_GEM, GEAR_TEST_OTHER_GEM);
			TestFalse(TEXT("weapon invalid pair"), Inventory->AttachFusionToWeapon(WeaponId, TwoGems));
			TestFalse(TEXT("ring invalid pair"), Inventory->AttachFusionToRing(RingId, TwoGems));
			TestEqual(TEXT("nothing debited"), Crystals->GetCount(GEAR_TEST_GEM), GEAR_TEST_STACK);

			const FFusionId Elemental = GearTestFusion(GEAR_TEST_STAT_STONE, GEAR_TEST_GEM);
			TestTrue(TEXT("weapon gem"), Inventory->AttachCrystalToWeapon(WeaponId, GEAR_TEST_OTHER_GEM));
			TestTrue(TEXT("ring gem"), Inventory->AttachCrystalToRing(RingId, GEAR_TEST_OTHER_GEM));
			TestFalse(TEXT("occupied weapon"), Inventory->AttachFusionToWeapon(WeaponId, Elemental));
			TestFalse(TEXT("occupied ring"), Inventory->AttachFusionToRing(RingId, Elemental));
			TestEqual(TEXT("halves kept"), Crystals->GetCount(GEAR_TEST_STAT_STONE), GEAR_TEST_STACK);
		});
	});

	Describe("evolution attach and remove", [this]()
	{
		It("references an owned evolution on a weapon and a ring, one slot each", [this]()
		{
			const FGuid WeaponEvo = AddEvolution();
			const FGuid RingEvo = AddEvolution();
			TestTrue(TEXT("weapon attach"), Inventory->AttachEvolutionToWeapon(WeaponId, WeaponEvo));
			TestTrue(TEXT("ring attach"), Inventory->AttachEvolutionToRing(RingId, RingEvo));
			TestTrue(TEXT("weapon references the entry"), Inventory->Weapons[0].AttachedItem.Evolution.InstanceID == WeaponEvo);
			TestTrue(TEXT("ring references the entry"), Inventory->Rings[0].AttachedItem.Evolution.InstanceID == RingEvo);
			TestEqual(TEXT("entries stay owned"), Evolutions->Entries.Num(), 2);
		});

		It("refuses an evolution already slotted, an unknown one and an occupied slot", [this]()
		{
			const FGuid Evo = AddEvolution();
			TestTrue(TEXT("weapon attach"), Inventory->AttachEvolutionToWeapon(WeaponId, Evo));
			TestFalse(TEXT("same evo on the ring"), Inventory->AttachEvolutionToRing(RingId, Evo));
			TestFalse(TEXT("unknown evo"), Inventory->AttachEvolutionToRing(RingId, FGuid::NewGuid()));
			TestTrue(TEXT("ring still empty"), Inventory->Rings[0].AttachedItem.IsEmpty());

			const FGuid Second = AddEvolution();
			TestTrue(TEXT("ring gem"), Inventory->AttachCrystalToRing(RingId, GEAR_TEST_GEM));
			TestFalse(TEXT("occupied weapon"), Inventory->AttachEvolutionToWeapon(WeaponId, Second));
			TestFalse(TEXT("occupied ring"), Inventory->AttachEvolutionToRing(RingId, Second));
			TestFalse(TEXT("unknown weapon"), Inventory->AttachEvolutionToWeapon(FGuid::NewGuid(), Second));
		});

		It("returns a removed evolution to inventory with 10% wear", [this]()
		{
			const FGuid WeaponEvo = AddEvolution();
			const FGuid RingEvo = AddEvolution();
			Inventory->AttachEvolutionToWeapon(WeaponId, WeaponEvo);
			Inventory->AttachEvolutionToRing(RingId, RingEvo);
			TestTrue(TEXT("weapon remove"), Inventory->RemoveEvolutionFromWeapon(WeaponId));
			TestTrue(TEXT("ring remove"), Inventory->RemoveEvolutionFromRing(RingId));
			TestTrue(TEXT("weapon emptied"), Inventory->Weapons[0].AttachedItem.IsEmpty());
			TestTrue(TEXT("ring emptied"), Inventory->Rings[0].AttachedItem.IsEmpty());
			const FEvolutionInventoryEntry *WeaponEntry = FindEvolution(WeaponEvo);
			const FEvolutionInventoryEntry *RingEntry = FindEvolution(RingEvo);
			if (TestNotNull(TEXT("weapon evo kept"), WeaponEntry) && TestNotNull(TEXT("ring evo kept"), RingEntry))
			{
				TestEqual(TEXT("weapon evo worn"), WeaponEntry->CurrentDurability, GEAR_TEST_EVO_AFTER_REMOVAL);
				TestEqual(TEXT("ring evo worn"), RingEntry->CurrentDurability, GEAR_TEST_EVO_AFTER_REMOVAL);
			}
			TestFalse(TEXT("weapon has nothing left to remove"), Inventory->RemoveEvolutionFromWeapon(WeaponId));
			TestFalse(TEXT("ring has nothing left to remove"), Inventory->RemoveEvolutionFromRing(RingId));
		});

		It("dismantles an evolution the removal wear breaks", [this]()
		{
			const FGuid WeaponEvo = AddEvolution();
			const FGuid RingEvo = AddEvolution();
			Inventory->AttachEvolutionToWeapon(WeaponId, WeaponEvo);
			Inventory->AttachEvolutionToRing(RingId, RingEvo);
			Inventory->Weapons[0].AttachedItem.Evolution.CurrentDurability = GEAR_TEST_EVO_LOW_DURABILITY;
			Inventory->Rings[0].AttachedItem.Evolution.CurrentDurability = GEAR_TEST_EVO_LOW_DURABILITY;
			TestTrue(TEXT("weapon remove"), Inventory->RemoveEvolutionFromWeapon(WeaponId));
			TestTrue(TEXT("ring remove"), Inventory->RemoveEvolutionFromRing(RingId));
			TestNull(TEXT("weapon evo dismantled"), FindEvolution(WeaponEvo));
			TestNull(TEXT("ring evo dismantled"), FindEvolution(RingEvo));
		});

		It("refuses removal from an unknown gear id", [this]()
		{
			TestFalse(TEXT("unknown weapon"), Inventory->RemoveEvolutionFromWeapon(FGuid::NewGuid()));
			TestFalse(TEXT("unknown ring"), Inventory->RemoveEvolutionFromRing(FGuid::NewGuid()));
		});
	});

	Describe("crystal remove by index", [this]()
	{
		It("empties a filled slot and reports an empty or invalid one", [this]()
		{
			TestFalse(TEXT("empty weapon"), Inventory->RemoveCrystalFromWeapon(0));
			TestFalse(TEXT("empty ring"), Inventory->RemoveCrystalFromRing(0));
			TestFalse(TEXT("bad weapon index"), Inventory->RemoveCrystalFromWeapon(1));
			TestFalse(TEXT("bad ring index"), Inventory->RemoveCrystalFromRing(-1));

			Inventory->AttachFusionToWeapon(WeaponId, GearTestFusion(GEAR_TEST_STAT_STONE, GEAR_TEST_GEM));
			Inventory->AttachCrystalToRing(RingId, GEAR_TEST_GEM);
			TestTrue(TEXT("weapon removed"), Inventory->RemoveCrystalFromWeapon(0));
			TestTrue(TEXT("ring removed"), Inventory->RemoveCrystalFromRing(0));
			TestTrue(TEXT("weapon empty"), Inventory->Weapons[0].AttachedItem.IsEmpty());
			TestTrue(TEXT("ring empty"), Inventory->Rings[0].AttachedItem.IsEmpty());
			TestEqual(TEXT("socketed crystals are destroyed, not returned"), Crystals->GetCount(GEAR_TEST_GEM), GEAR_TEST_STACK - 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
