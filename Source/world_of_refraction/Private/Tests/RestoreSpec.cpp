// RestoreSpec.cpp
// B-534 (D-231 "Restore amounts", "Breaking, restoring and the word", Q-534): a worn gem, or a locked one at 0, is topped
// up with crystals of its own type. One crystal restores value(its tier) / value(the gem's tier) (the merge values): the same tier fills
// it, one tier down a half, an A a third of an S; cheapest first, overshoot lost, no money. An unlocked gem at 0 is gone at fight end, so not offered.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Durability/GearRepair.h"
#include "Equipment/Rings/RingData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "UI/Menus/LoadoutScreenModel.h"

namespace
{
	const FCrystalId RESTORE_TEST_GEM(ECrystalType::Garnet, EItemTier::C_Tier);
	const FCrystalId RESTORE_TEST_SAME(ECrystalType::Garnet, EItemTier::C_Tier);
	const FCrystalId RESTORE_TEST_ONE_DOWN(ECrystalType::Garnet, EItemTier::D_Tier);
	const FCrystalId RESTORE_TEST_FLOOR(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId RESTORE_TEST_TOP(ECrystalType::Garnet, EItemTier::S_Tier);
	const FCrystalId RESTORE_TEST_OTHER_TYPE(ECrystalType::Sapphire, EItemTier::C_Tier);
	const FCrystalId RESTORE_TEST_S_GEM(ECrystalType::Garnet, EItemTier::S_Tier);
	const FCrystalId RESTORE_TEST_A(ECrystalType::Garnet, EItemTier::A_Tier);
	constexpr int32 RESTORE_TEST_HALF_DIVISOR = 2;
	constexpr int32 RESTORE_TEST_THIRD_DIVISOR = 3;
	constexpr int32 RESTORE_TEST_FLOORS = 2;

	FRuntimeAttachedItem RestoreTestBrokenGem(const FCrystalId &Id)
	{
		FRuntimeAttachedItem Gem;
		Gem.Kind = EAttachedItemKind::Crystal;
		Gem.Crystal = FCrystalAttachment(Id, 0);
		return Gem;
	}
}

BEGIN_DEFINE_SPEC(FRestoreSpec, "WoR.Durability.Restore", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	UCrystalInventoryComponent *Crystals = nullptr;

	/** An owned adapter holding Gem at Wear (0: broken), locked (prebuilt, not removable) unless bLocked is false. */
	FGuid AddRing(const FCrystalId &Gem, bool bLocked, int32 Wear = 0)
	{
		Inventory->AddRing(NewObject<URingData>(GetTransientPackage()));
		FRingInventoryEntry &Ring = Inventory->Rings.Last();
		Ring.AttachedItem = RestoreTestBrokenGem(Gem);
		Ring.AttachedItem.Crystal.CurrentDurability = Wear;
		Ring.bPrebuilt = bLocked;
		Ring.bCrystalRemovable = !bLocked;
		return Ring.PersistentID;
	}
END_DEFINE_SPEC(FRestoreSpec)

void FRestoreSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Owner"), Owner)) return;
		Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Crystals = nullptr;
		TestWorld.Reset();
	});

	It("restores by the merge values: one tier down a half, the same tier fills it", [this]()
	{
		if (!Inventory) return;
		const FRuntimeAttachedItem Gem = RestoreTestBrokenGem(RESTORE_TEST_GEM);
		const int32 Max = ItemIdentity::GetMaxDurability(RESTORE_TEST_GEM);
		TestTrue(TEXT("Nothing held: nothing to spend"), GearRepair::PlanRestore(Gem, *Crystals).Uses.IsEmpty());

		Inventory->AddCrystal(RESTORE_TEST_OTHER_TYPE);
		TestTrue(TEXT("Another type doesn't count"), GearRepair::PlanRestore(Gem, *Crystals).Uses.IsEmpty());

		Inventory->AddCrystal(RESTORE_TEST_ONE_DOWN);
		GearRepair::FRestorePlan Plan = GearRepair::PlanRestore(Gem, *Crystals);
		TestTrue(TEXT("One tier down: one crystal"), Plan.Uses.Num() == 1 && Plan.Uses[0] == RESTORE_TEST_ONE_DOWN);
		TestEqual(TEXT("... restores half"), Plan.NewDurability, Max / RESTORE_TEST_HALF_DIVISOR);

		Inventory->AddCrystal(RESTORE_TEST_SAME);
		Plan = GearRepair::PlanRestore(Gem, *Crystals);
		TestEqual(TEXT("Cheapest first: the D, then the C"), Plan.Uses.Num(), 2);
		TestEqual(TEXT("Full, the extra lost"), Plan.NewDurability, Max);
	});

	It("an A restores a third of an S, and a higher tier fills a lower gem", [this]()
	{
		if (!Inventory) return;
		Inventory->AddCrystal(RESTORE_TEST_A);
		const GearRepair::FRestorePlan Third = GearRepair::PlanRestore(RestoreTestBrokenGem(RESTORE_TEST_S_GEM), *Crystals);
		TestEqual(TEXT("A third of an S"), Third.NewDurability, ItemIdentity::GetMaxDurability(RESTORE_TEST_S_GEM) / RESTORE_TEST_THIRD_DIVISOR);

		Inventory->AddCrystal(RESTORE_TEST_FLOOR, RESTORE_TEST_FLOORS);
		Inventory->AddCrystal(RESTORE_TEST_TOP);
		const GearRepair::FRestorePlan Filled = GearRepair::PlanRestore(RestoreTestBrokenGem(RESTORE_TEST_GEM), *Crystals);
		TestEqual(TEXT("The two F first, then the A fills it"), Filled.Uses.Num(), RESTORE_TEST_FLOORS + 1);
		TestFalse(TEXT("The S isn't needed"), Filled.Uses.Contains(RESTORE_TEST_TOP));
		TestEqual(TEXT("Full"), Filled.NewDurability, ItemIdentity::GetMaxDurability(RESTORE_TEST_GEM));
	});

	It("restores a locked broken gem, spending its crystals; refuses a full one or an unlocked one at 0", [this]()
	{
		if (!Inventory) return;
		const FGuid Locked = AddRing(RESTORE_TEST_GEM, true);
		TestFalse(TEXT("No crystals: refused"), Inventory->RestoreCrystal(Locked));
		Inventory->AddCrystal(RESTORE_TEST_SAME);
		TestTrue(TEXT("Restored"), Inventory->RestoreCrystal(Locked));
		const FRingInventoryEntry *Ring = Inventory->FindOwnedRing(Locked);
		TestTrue(TEXT("Full again"), Ring && Ring->AttachedItem.GetCurrentDurability() == ItemIdentity::GetMaxDurability(RESTORE_TEST_GEM));
		TestEqual(TEXT("The crystal spent"), Crystals->GetCount(RESTORE_TEST_SAME), 0);
		Inventory->AddCrystal(RESTORE_TEST_SAME);
		TestFalse(TEXT("Full now: refused"), Inventory->RestoreCrystal(Locked));

		const FGuid Unlocked = AddRing(RESTORE_TEST_GEM, false);
		TestFalse(TEXT("Unlocked at 0: refused"), Inventory->RestoreCrystal(Unlocked));
		TestEqual(TEXT("Nothing spent"), Crystals->GetCount(RESTORE_TEST_SAME), 1);
	});

	It("tops up any worn gem, locked or not: half worn, one tier down fills it", [this]()
	{
		if (!Inventory) return;
		const int32 Max = ItemIdentity::GetMaxDurability(RESTORE_TEST_GEM);
		const FGuid Unlocked = AddRing(RESTORE_TEST_GEM, false, Max / RESTORE_TEST_HALF_DIVISOR);
		Inventory->AddCrystal(RESTORE_TEST_ONE_DOWN);
		TestTrue(TEXT("Unlocked, half worn: restored"), Inventory->RestoreCrystal(Unlocked));
		const FRingInventoryEntry *Ring = Inventory->FindOwnedRing(Unlocked);
		TestTrue(TEXT("Full"), Ring && Ring->AttachedItem.GetCurrentDurability() == Max);
		TestEqual(TEXT("The D spent"), Crystals->GetCount(RESTORE_TEST_ONE_DOWN), 0);
	});

	It("is offered on the Equipment screen on a worn gem, and at 0 only when locked", [this]()
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::Adapter;
		Row.GearID = FGuid::NewGuid();
		Row.Sockets.SetNum(2);
		FLoadoutScreenSocket &Gem = Row.Sockets[LoadoutScreenSockets::CRYSTAL];
		Gem.bEmpty = false;
		Gem.bWorn = true;
		TestTrue(TEXT("Worn, unlocked: offered"), WoRLoadoutScreen::IsRestoreOffered(Row, LoadoutScreenSockets::CRYSTAL));
		TestFalse(TEXT("Not on the stone socket"), WoRLoadoutScreen::IsRestoreOffered(Row, LoadoutScreenSockets::STONE));
		Gem.bBroken = true;
		TestFalse(TEXT("At 0, unlocked: not offered"), WoRLoadoutScreen::IsRestoreOffered(Row, LoadoutScreenSockets::CRYSTAL));
		Row.bCrystalFixed = true;
		TestTrue(TEXT("At 0, locked: offered"), WoRLoadoutScreen::IsRestoreOffered(Row, LoadoutScreenSockets::CRYSTAL));
		Gem.bBroken = false;
		Gem.bWorn = false;
		TestFalse(TEXT("Full: not offered"), WoRLoadoutScreen::IsRestoreOffered(Row, LoadoutScreenSockets::CRYSTAL));
	});
}

#endif
