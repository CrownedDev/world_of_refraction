// RunEndGearSpec.cpp
// B-509 (D-219, D-221 stage 1b): what each ending does to the gear itself. Every ending but a quit keeps a home piece's
// socket as the run left it (at full wear); an extract or a boss win keeps worn found gear as hub gear (saved, never
// sold); out of wipes keeps no found gear; a quit puts the home state back. A home gem taken out of its socket in the
// run is not taken back as a find.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	const FName RUN_END_GEAR_KEY(TEXT("RunEndGearTester"));
	const FCrystalId RUN_END_HOME_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId RUN_END_RUN_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	constexpr int32 RUN_END_WORN_DURABILITY = 1;

	FRuntimeAttachedItem RunEndGem(const FCrystalId &Id, int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(Id, Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FRunEndGearSpec, "WoR.Run.EndGear", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	FGuid HomeID;
	FGuid WornFindID;
	FGuid LeftFindID;

	FGuid AddWeapon(const FRuntimeAttachedItem &Attached)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = Attached;
		Pool->UpdateWeapon(Entry);
		return Entry.PersistentID;
	}

	void SetSocket(const FGuid &ID, const FRuntimeAttachedItem &Attached)
	{
		FWeaponInventoryEntry Entry = *Pool->FindWeaponInPool(ID);
		Entry.AttachedItem = Attached;
		Pool->UpdateWeapon(Entry);
	}

	/** A home sword with the home gem goes on a run; the run swaps in another (worn) gem, finds two swords and wears
	 *  one of them. */
	void PlayRun()
	{
		HomeID = AddWeapon(RunEndGem(RUN_END_HOME_GEM, ItemIdentity::GetMaxDurability(RUN_END_HOME_GEM)));
		Pool->SetGearOwner(HomeID, RUN_END_GEAR_KEY);
		Pool->BeginRunLayer();
		Pool->TakeOnRun(HomeID);
		SetSocket(HomeID, RunEndGem(RUN_END_RUN_GEM, RUN_END_WORN_DURABILITY));
		WornFindID = AddWeapon(FRuntimeAttachedItem());
		LeftFindID = AddWeapon(FRuntimeAttachedItem());
		Pool->MarkRunOnly(WornFindID);
		Pool->MarkRunOnly(LeftFindID);
		FWornGear Worn;
		Worn.Equipment.AddDefaulted_GetRef().GearID = HomeID;
		Worn.Equipment.AddDefaulted_GetRef().GearID = WornFindID;
		Worn.ActiveGearID = HomeID;
		Pool->SetWornGear(RUN_END_GEAR_KEY, Worn);
	}

	bool HomeHolds(const FCrystalId &Id) const
	{
		const FWeaponInventoryEntry *Home = Pool->FindWeaponInPool(HomeID);
		return Home && Home->AttachedItem.IsCrystal() && Home->AttachedItem.Crystal.Id == Id;
	}
END_DEFINE_SPEC(FRunEndGearSpec)

void FRunEndGearSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		if (Pool)
		{
			PlayRun();
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		TestWorld.Reset();
	});

	It("keeps the run's socket at full wear and the worn find as hub gear on an extract or a boss win", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		Pool->CloseRun(EPoolRunEnding::Kept);
		TestTrue(TEXT("the run's gem stays"), HomeHolds(RUN_END_RUN_GEM));
		const FWeaponInventoryEntry *Home = Pool->FindWeaponInPool(HomeID);
		TestTrue(TEXT("at full wear"), Home && Home->AttachedItem.GetCurrentDurability() == Home->AttachedItem.GetMaxDurability());
		TestNotNull(TEXT("the worn find stays"), Pool->FindWeaponInPool(WornFindID));
		TestTrue(TEXT("as hub gear"), Pool->IsHubGear(WornFindID));
		TestNull(TEXT("the unworn find goes"), Pool->FindWeaponInPool(LeftFindID));
		TestFalse(TEXT("a home piece is not hub gear"), Pool->IsHubGear(HomeID));
	});

	It("keeps the run's socket but no found gear when the party runs out of wipes", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		Pool->CloseRun(EPoolRunEnding::Lost);
		TestTrue(TEXT("the run's gem stays"), HomeHolds(RUN_END_RUN_GEM));
		TestNull(TEXT("the worn find goes"), Pool->FindWeaponInPool(WornFindID));
		TestNull(TEXT("the unworn find goes"), Pool->FindWeaponInPool(LeftFindID));
		TestFalse(TEXT("no hub gear"), Pool->IsHubGear(WornFindID));
	});

	It("puts the home gem back and keeps no found gear on a quit", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		Pool->CloseRun(EPoolRunEnding::Quit);
		TestTrue(TEXT("the home gem is back"), HomeHolds(RUN_END_HOME_GEM));
		TestNull(TEXT("the worn find goes"), Pool->FindWeaponInPool(WornFindID));
	});

	It("doesn't take back a home gem the run moved from its socket to the pouch when the party runs out of wipes", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		SetSocket(HomeID, FRuntimeAttachedItem()); // the run's gem came off, and the home gem went into the pouch
		Pool->CarryOrStore(RUN_END_GEAR_KEY, RUN_END_HOME_GEM, 1);
		Pool->CloseRun(EPoolRunEnding::Lost);
		TestEqual(TEXT("the home gem is still held"),
				  Pool->GetCarriedCount(RUN_END_GEAR_KEY, RUN_END_HOME_GEM) + Pool->GetStoredCount(RUN_END_HOME_GEM), 1);
	});

	It("saves hub gear and drops the mark when the piece leaves the store", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		Pool->CloseRun(EPoolRunEnding::Kept);
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		TestTrue(TEXT("saved"), Saved.HubGear.Contains(WornFindID));
		Pool->RestoreHubGear({});
		USaveSubsystem::RestorePool(Saved, *Pool);
		TestTrue(TEXT("back after a load"), Pool->IsHubGear(WornFindID));
		Pool->RemoveGear(WornFindID);
		TestFalse(TEXT("gone with the piece"), Pool->IsHubGear(WornFindID));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
