// RunWearByGuidSpec.cpp
// B-310 (D-111): run wear follows the owned item, not its asset. With two identical adapters on a Resonator, a cast's
// wear lands on the active one, and the carried state goes back to the same item even when the two swap places.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/CarriedGearState.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	constexpr int32 RUN_WEAR_TEST_FULL = 10;
	constexpr int32 RUN_WEAR_TEST_WORN = 3;
	constexpr int32 RUN_WEAR_TEST_ACTIVE = 1;
	const FCrystalId RUN_WEAR_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FRuntimeAttachedItem RunWearTestGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(RUN_WEAR_TEST_GEM, Durability);
		return Attached;
	}

	FRingLoadoutEntry RunWearTestAdapter(URingData *Ring, const FGuid &GearID)
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = Ring;
		Entry.RingEntry.PersistentID = GearID;
		Entry.RingEntry.AttachedItem = RunWearTestGem(RUN_WEAR_TEST_FULL);
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FRunWearByGuidSpec, "WoR.Party.RunWearByGuid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	URingData *Ring = nullptr;
	FGuid FirstID;
	FGuid SecondID;

	/** A Resonator wearing two copies of one adapter, in the given order, the second (index 1) active. */
	ULoadoutComponent *SpawnWith(const FGuid &InFirst, const FGuid &InSecond)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data);
		UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return nullptr;
		}
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.RingLoadout = {RunWearTestAdapter(Ring, InFirst), RunWearTestAdapter(Ring, InSecond)};
		Active.ActiveRingIndex = RUN_WEAR_TEST_ACTIVE;
		return Loadout;
	}

	static const FCombatLoadout &ActiveOf(const ULoadoutComponent *Loadout)
	{
		const UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}
END_DEFINE_SPEC(FRunWearByGuidSpec)

void FRunWearByGuidSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Ring = NewObject<URingData>(GetTransientPackage());
		FirstID = FGuid::NewGuid();
		SecondID = FGuid::NewGuid();
	});

	AfterEach([this]()
	{
		Ring = nullptr;
		TestWorld.Reset();
	});

	It("wears the active copy of two identical adapters", [this]()
	{
		ULoadoutComponent *Loadout = SpawnWith(FirstID, SecondID);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		FRuntimeAttachedItem *Worn = Loadout->FindAttachedItemByHolder(Ring);
		TestTrue(TEXT("the active adapter's attachment"),
				 Worn == &ActiveOf(Loadout).RingLoadout[RUN_WEAR_TEST_ACTIVE].RingEntry.AttachedItem);
	});

	It("carries each copy's wear to the same item, even when the two swap places", [this]()
	{
		ULoadoutComponent *Before = SpawnWith(FirstID, SecondID);
		if (!TestNotNull(TEXT("Loadout"), Before)) return;
		if (FRuntimeAttachedItem *Worn = Before->FindAttachedItemByHolder(Ring))
		{
			*Worn = RunWearTestGem(RUN_WEAR_TEST_WORN); // the second (active) copy wore down
		}
		const FCarriedGearState Carried = Before->SnapshotCarriedGear();

		ULoadoutComponent *After = SpawnWith(SecondID, FirstID); // the next fight lists them the other way round
		if (!TestNotNull(TEXT("Loadout"), After)) return;
		After->ApplyCarriedGear(Carried);
		const FCombatLoadout &Active = ActiveOf(After);
		TestEqual(TEXT("the worn copy (now first) kept its wear"), Active.RingLoadout[0].RingEntry.AttachedItem.GetCurrentDurability(), RUN_WEAR_TEST_WORN);
		TestEqual(TEXT("the other copy stayed full"), Active.RingLoadout[1].RingEntry.AttachedItem.GetCurrentDurability(), RUN_WEAR_TEST_FULL);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
