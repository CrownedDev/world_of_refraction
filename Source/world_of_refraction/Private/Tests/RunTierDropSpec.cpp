// RunTierDropSpec.cpp
// B-510 (D-219, D-221 stage 1b): a lost run costs tier. Running out of wipes, a quit and a game closed mid-run lower
// every piece the party wears at that moment by GearTierLoss::RANKS_LOST_ON_LOST_RUN, never below F, once; an extract
// and a boss win cost nothing. A home piece taken off before a quit keeps its tier.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 TIER_DROP_TEST_SEED = 510;
	const FName TIER_DROP_KEY(TEXT("TierDropTester"));
	constexpr EItemTier TIER_DROP_START = EItemTier::S_Tier;
	constexpr EItemTier TIER_DROP_AFTER = EItemTier::C_Tier; // S less 3
	constexpr EItemTier TIER_DROP_LOW_START = EItemTier::D_Tier; // D less 3 is below F
	constexpr EItemTier TIER_DROP_APPAREL_START = EItemTier::A_Tier;
	constexpr EItemTier TIER_DROP_APPAREL_AFTER = EItemTier::D_Tier;
}

BEGIN_DEFINE_SPEC(FRunTierDropSpec, "WoR.Run.TierDrop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	FGuid WornID;
	FGuid SpareID;
	FGuid ApparelID;

	FGuid AddWeapon(EItemTier Tier)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Tier = Tier;
		Pool->UpdateWeapon(Entry);
		Pool->SetGearOwner(Entry.PersistentID, TIER_DROP_KEY);
		return Entry.PersistentID;
	}

	void Wear(const FGuid &Weapon)
	{
		FWornGear Worn;
		Worn.Equipment.AddDefaulted_GetRef().GearID = Weapon;
		Worn.ActiveGearID = Weapon;
		Worn.Apparel.Add(ApparelID);
		Pool->SetWornGear(TIER_DROP_KEY, Worn);
	}

	/** The hub wears an S-tier sword and A-tier apparel, with an S-tier spare at home; then the run opens. */
	void StartRun(EItemTier WornTier = TIER_DROP_START)
	{
		WornID = AddWeapon(WornTier);
		SpareID = AddWeapon(TIER_DROP_START);
		FApparelEntry Apparel;
		Apparel.Tier = TIER_DROP_APPAREL_START;
		ApparelID = Pool->AddApparelToPool(Apparel, TIER_DROP_KEY);
		Wear(WornID);
		if (!Pool->IsRunLayerActive())
		{
			Pool->BeginRunLayer();
		}
		Pool->SnapshotRunStartGear({TIER_DROP_KEY});
		Pool->TakeOnRun(WornID);
	}

	EItemTier TierOf(const FGuid &ID) const
	{
		const FWeaponInventoryEntry *Weapon = Pool->FindWeaponInPool(ID);
		const FApparelEntry *Apparel = Weapon ? nullptr : Pool->FindApparelInPool(ID);
		return Weapon ? Weapon->Tier : Apparel ? Apparel->Tier : EItemTier::F_Tier;
	}
END_DEFINE_SPEC(FRunTierDropSpec)

void FRunTierDropSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		TestWorld.Reset();
	});

	It("drops each worn piece 3 tiers when the party runs out of wipes, and leaves unworn pieces as they are", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		Pool->CloseRun(EPoolRunEnding::Lost);
		TestEqual(TEXT("the worn sword: S to C"), TierOf(WornID), TIER_DROP_AFTER);
		TestEqual(TEXT("the apparel: A to D"), TierOf(ApparelID), TIER_DROP_APPAREL_AFTER);
		TestEqual(TEXT("the spare at home keeps S"), TierOf(SpareID), TIER_DROP_START);
	});

	It("never drops a piece below F", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun(TIER_DROP_LOW_START);
		Pool->CloseRun(EPoolRunEnding::Lost);
		TestEqual(TEXT("D stops at F"), TierOf(WornID), EItemTier::F_Tier);
	});

	It("costs nothing on an extract or a boss win", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		Pool->CloseRun(EPoolRunEnding::Kept);
		TestEqual(TEXT("the worn sword keeps S"), TierOf(WornID), TIER_DROP_START);
		TestEqual(TEXT("the apparel keeps A"), TierOf(ApparelID), TIER_DROP_APPAREL_START);
	});

	It("on a quit drops what is worn at the quit, then puts the hub gear back", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		Pool->TakeOnRun(SpareID);
		Wear(SpareID); // the run swapped the hub's sword for the spare
		Pool->CloseRun(EPoolRunEnding::Quit);
		TestEqual(TEXT("the sword worn at the quit drops"), TierOf(SpareID), TIER_DROP_AFTER);
		TestEqual(TEXT("the sword taken off before the quit keeps S"), TierOf(WornID), TIER_DROP_START);
		const FWornGear *After = Pool->GetWornGear(TIER_DROP_KEY);
		TestTrue(TEXT("the hub record is back"), After && After->ActiveGearID == WornID);
	});

	It("drops tier once when a game closed mid-run is settled", [this]()
	{
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Run"), Run)) return;
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), TIER_DROP_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		StartRun();
		Run->SettleRunAsQuit(/*bTravelIfAway=*/false);
		TestEqual(TEXT("S to C, once"), TierOf(WornID), TIER_DROP_AFTER);
		Run->SettleRunAsQuit(/*bTravelIfAway=*/false); // no run left to settle
		TestEqual(TEXT("no second drop"), TierOf(WornID), TIER_DROP_AFTER);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
