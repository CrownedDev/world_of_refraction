// PoolRunLayerSpec.cpp
// B-314 (D-111, D-097, Q-314): the run layer over the gear store. Going on a run marks the real home piece (no copy);
// at run end its wear and anything attached in the run come off; gear found in the run is gone after it, and its tier
// is remembered for its definition, so the next copy found arrives at that tier. One instance per item throughout.
// Review fixes: a reloaded layer is discarded, not applied; a home piece on the run can't be dismantled (Q-314b); a
// run-only piece dismantled before run end is still remembered (Q-314c).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	constexpr int32 RUN_LAYER_FULL = 10;
	constexpr int32 RUN_LAYER_WORN = 2;
	constexpr EItemTier RUN_LAYER_FOUND_TIER = EItemTier::F_Tier;
	constexpr EItemTier RUN_LAYER_UPGRADED_TIER = EItemTier::C_Tier;
	constexpr EItemTier RUN_LAYER_LOWER_TIER = EItemTier::E_Tier;
	const FCrystalId RUN_LAYER_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	FRuntimeAttachedItem RunLayerGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(RUN_LAYER_GEM, Durability);
		return Attached;
	}

	int32 CountInStore(const UPoolSubsystem &Pool, const FGuid &ID)
	{
		return Pool.GetOwnedWeapons().FilterByPredicate([&ID](const FWeaponInventoryEntry &E) { return E.PersistentID == ID; }).Num() +
			   Pool.GetOwnedRings().FilterByPredicate([&ID](const FRingInventoryEntry &E) { return E.PersistentID == ID; }).Num();
	}
}

BEGIN_DEFINE_SPEC(FPoolRunLayerSpec, "WoR.Pool.RunLayer", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	UWeaponData *Axe = nullptr;
	URingData *Adapter = nullptr;
	UCharacterData *Member = nullptr;

	/** A home piece in the store (owned by the member), its sword carrying a gem at Durability (none when 0). */
	FGuid AddHomeSword(int32 Durability)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = Durability > 0 ? RunLayerGem(Durability) : FRuntimeAttachedItem();
		Pool->SetGearOwner(Entry.PersistentID, UPoolSubsystem::GetCharacterKey(Member));
		Pool->UpdateWeapon(Entry);
		return Entry.PersistentID;
	}

	FGuid AddHomeAdapter()
	{
		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Adapter);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = FRuntimeAttachedItem();
		Pool->SetGearOwner(Entry.PersistentID, UPoolSubsystem::GetCharacterKey(Member));
		Pool->UpdateRing(Entry);
		return Entry.PersistentID;
	}

	/** The party member as each level spawns it: the sword, then the draw. */
	UInventoryComponent *SpawnMember()
	{
		AActor *Actor = TestWorld->SpawnCombatant(Member);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	static FGuid OwnedID(const UInventoryComponent *Inventory, const UWeaponData *Asset)
	{
		const FWeaponInventoryEntry *Entry = Inventory->Weapons.FindByPredicate([Asset](const FWeaponInventoryEntry &E) { return E.Weapon == Asset; });
		return Entry ? Entry->PersistentID : FGuid();
	}
END_DEFINE_SPEC(FPoolRunLayerSpec)

void FPoolRunLayerSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Run layer sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Run layer dagger");
		Axe = NewObject<UWeaponData>(GetTransientPackage());
		Axe->Name = TEXT("Run layer axe");
		Adapter = NewObject<URingData>(GetTransientPackage());
		Member = FWoRTestWorld::NewCharacterData();
		Member->Name = TEXT("Run layer member");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = Member;
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
		Axe = nullptr;
		Adapter = nullptr;
		Member = nullptr;
		TestWorld.Reset();
	});

	It("marks the real home piece when it goes on the run, without a copy", [this]()
	{
		if (!Pool) return;
		const FGuid SwordID = AddHomeSword(RUN_LAYER_FULL);
		const int32 WeaponsBefore = Pool->GetOwnedWeapons().Num();
		TestFalse(TEXT("nothing is taken with no run open"), Pool->TakeOnRun(SwordID));

		Pool->BeginRunLayer();
		TestTrue(TEXT("the run layer is open"), Pool->IsRunLayerActive());
		TestEqual(TEXT("the home piece goes along"), Pool->TakeHomeGearOnRun(), 1);
		TestTrue(TEXT("it is marked on the run"), Pool->IsOnRun(SwordID));
		TestFalse(TEXT("a home piece is not run-only"), Pool->IsRunOnly(SwordID));
		TestFalse(TEXT("taken once only"), Pool->TakeOnRun(SwordID));
		TestFalse(TEXT("a home piece can't be made run-only"), Pool->MarkRunOnly(SwordID));
		TestFalse(TEXT("an unknown piece can't go on the run"), Pool->TakeOnRun(FGuid::NewGuid()));
		TestEqual(TEXT("still one instance in the store"), CountInStore(*Pool, SwordID), 1);
		TestEqual(TEXT("no copy was added"), Pool->GetOwnedWeapons().Num(), WeaponsBefore);

		Pool->EndRunLayer();
		TestFalse(TEXT("the run layer is closed"), Pool->IsRunLayerActive());
		TestFalse(TEXT("the mark goes with the run"), Pool->IsOnRun(SwordID));
		TestEqual(TEXT("the home piece stays"), CountInStore(*Pool, SwordID), 1);
	});

	It("puts the home piece's wear back and takes off what the run attached", [this]()
	{
		if (!Pool) return;
		const FGuid SwordID = AddHomeSword(RUN_LAYER_FULL);
		const FGuid AdapterID = AddHomeAdapter();
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun();

		FWeaponInventoryEntry Worn = *Pool->FindWeaponInPool(SwordID);
		Worn.AttachedItem = RunLayerGem(RUN_LAYER_WORN); // the run wore the gem down
		Pool->UpdateWeapon(Worn);
		FRingInventoryEntry Socketed = *Pool->FindRingInPool(AdapterID);
		Socketed.AttachedItem = RunLayerGem(RUN_LAYER_FULL); // a gem attached at a rest spot
		Pool->UpdateRing(Socketed);

		const FPoolRunEndResult Result = Pool->EndRunLayer();
		TestEqual(TEXT("both home pieces restored"), Result.Restored, 2);
		const FWeaponInventoryEntry *HomeSword = Pool->FindWeaponInPool(SwordID);
		TestTrue(TEXT("the sword's gem is back to its home durability"),
				 HomeSword && HomeSword->AttachedItem.Kind == EAttachedItemKind::Crystal && HomeSword->AttachedItem.Crystal.CurrentDurability == RUN_LAYER_FULL);
		const FRingInventoryEntry *Restored = Pool->FindRingInPool(AdapterID);
		TestTrue(TEXT("the gem attached in the run came off"), Restored && Restored->AttachedItem.IsEmpty());
		TestEqual(TEXT("one sword"), CountInStore(*Pool, SwordID), 1);
		TestEqual(TEXT("one adapter"), CountInStore(*Pool, AdapterID), 1);
	});

	It("drops found gear at run end and remembers its tier for its definition", [this]()
	{
		if (!Pool) return;
		Pool->BeginRunLayer();
		FWeaponInventoryEntry Found = FWeaponInventoryEntry::CreateFromWeapon(Dagger);
		Found.PersistentID = FGuid::NewGuid();
		Found.Tier = RUN_LAYER_FOUND_TIER;
		Pool->UpdateWeapon(Found);
		TestTrue(TEXT("found in the run: run-only"), Pool->MarkRunOnly(Found.PersistentID));
		Found.Tier = RUN_LAYER_UPGRADED_TIER; // upgraded during the run
		Pool->UpdateWeapon(Found);

		const FPoolRunEndResult Result = Pool->EndRunLayer();
		TestEqual(TEXT("one piece dropped"), Result.Dropped, 1);
		TestNull(TEXT("the found dagger is gone"), Pool->FindWeaponInPool(Found.PersistentID));
		EItemTier Remembered = EItemTier::F_Tier;
		TestTrue(TEXT("its definition is remembered"), Pool->FindRememberedTier(Dagger, Remembered));
		TestTrue(TEXT("at the tier the run left it"), Remembered == RUN_LAYER_UPGRADED_TIER);

		FWeaponInventoryEntry Again = FWeaponInventoryEntry::CreateFromWeapon(Dagger);
		Again.Tier = RUN_LAYER_FOUND_TIER;
		TestTrue(TEXT("the next copy found is raised"), Pool->ApplyUpgradeMemory(Again));
		TestTrue(TEXT("to the remembered tier"), Again.Tier == RUN_LAYER_UPGRADED_TIER);

		// A later run that leaves a copy lower doesn't lose the progress.
		Pool->BeginRunLayer();
		FWeaponInventoryEntry Lower = FWeaponInventoryEntry::CreateFromWeapon(Dagger);
		Lower.PersistentID = FGuid::NewGuid();
		Lower.Tier = RUN_LAYER_LOWER_TIER;
		Pool->UpdateWeapon(Lower);
		Pool->MarkRunOnly(Lower.PersistentID);
		Pool->EndRunLayer();
		TestTrue(TEXT("the higher tier stays remembered"), Pool->FindRememberedTier(Dagger, Remembered) && Remembered == RUN_LAYER_UPGRADED_TIER);
	});

	It("marks loot a member gets in a run, raises it to the remembered tier, and takes it from the member at run end", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Member"), Inventory)) return;
		const FGuid SwordID = OwnedID(Inventory, Sword);
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun();
		TestTrue(TEXT("the member's own sword goes on the run"), Pool->IsOnRun(SwordID));

		// Run 1: the dagger is found and upgraded.
		TestTrue(TEXT("loot given"), Inventory->AddWeapon(Dagger));
		const FGuid FirstDagger = OwnedID(Inventory, Dagger);
		TestTrue(TEXT("found loot is run-only"), Pool->IsRunOnly(FirstDagger));
		TestEqual(TEXT("one instance of the loot"), CountInStore(*Pool, FirstDagger), 1);
		FWeaponInventoryEntry Upgraded = *Pool->FindWeaponInPool(FirstDagger);
		Upgraded.Tier = RUN_LAYER_UPGRADED_TIER;
		Pool->UpdateWeapon(Upgraded);
		Pool->EndRunLayer();
		TestFalse(TEXT("the member no longer has the found dagger"), OwnedID(Inventory, Dagger).IsValid());
		TestEqual(TEXT("the store no longer has it"), CountInStore(*Pool, FirstDagger), 0);
		TestEqual(TEXT("the member keeps its home sword"), CountInStore(*Pool, SwordID), 1);
		TestTrue(TEXT("the member's sword is still its own"), OwnedID(Inventory, Sword) == SwordID);

		// Run 2: the next dagger found arrives at the tier run 1 left it.
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun();
		TestTrue(TEXT("loot given again"), Inventory->AddWeapon(Dagger));
		const FGuid SecondDagger = OwnedID(Inventory, Dagger);
		const FWeaponInventoryEntry *InStore = Pool->FindWeaponInPool(SecondDagger);
		const FWeaponInventoryEntry *MemberCopy = Inventory->FindOwnedWeapon(SecondDagger);
		TestTrue(TEXT("the store's copy arrives upgraded"), InStore && InStore->Tier == RUN_LAYER_UPGRADED_TIER);
		TestTrue(TEXT("the member's copy agrees"), MemberCopy && MemberCopy->Tier == RUN_LAYER_UPGRADED_TIER);
		TestEqual(TEXT("one instance"), CountInStore(*Pool, SecondDagger), 1);
		Pool->EndRunLayer();
	});

	It("discards a layer without putting anything back", [this]()
	{
		if (!Pool) return;
		const FGuid SwordID = AddHomeSword(RUN_LAYER_FULL);
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun();
		FWeaponInventoryEntry Worn = *Pool->FindWeaponInPool(SwordID);
		Worn.AttachedItem = RunLayerGem(RUN_LAYER_WORN); // as a reloaded save would hold it
		Pool->UpdateWeapon(Worn);

		Pool->BeginRunLayer(); // a still-open layer is discarded, not applied
		TestTrue(TEXT("a new layer is open"), Pool->IsRunLayerActive());
		TestFalse(TEXT("the old mark is gone"), Pool->IsOnRun(SwordID));
		const FWeaponInventoryEntry *Kept = Pool->FindWeaponInPool(SwordID);
		TestTrue(TEXT("the gear is left as loaded"), Kept && Kept->AttachedItem.GetCurrentDurability() == RUN_LAYER_WORN);

		Pool->DiscardRunLayer();
		TestFalse(TEXT("discarded: closed"), Pool->IsRunLayerActive());
		TestEqual(TEXT("still one sword"), CountInStore(*Pool, SwordID), 1);
	});

	It("refuses to dismantle a home piece on the run, and remembers a run-only piece dismantled before run end", [this]()
	{
		if (!Pool) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Economy"), Economy) || !TestNotNull(TEXT("Member"), Inventory)) return;
		AActor *Owner = Inventory->GetOwner();
		TestTrue(TEXT("a home dagger"), Inventory->AddWeapon(Dagger));
		const FGuid DaggerID = OwnedID(Inventory, Dagger);

		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun();
		TestFalse(TEXT("the home dagger on the run is refused"), Economy->DismantleWeapon(Owner, DaggerID));
		TestEqual(TEXT("the home dagger stays"), CountInStore(*Pool, DaggerID), 1);

		TestTrue(TEXT("an axe found"), Inventory->AddWeapon(Axe));
		const FGuid AxeID = OwnedID(Inventory, Axe);
		TestTrue(TEXT("the axe is run-only"), Pool->IsRunOnly(AxeID));
		FWeaponInventoryEntry Upgraded = *Pool->FindWeaponInPool(AxeID);
		Upgraded.Tier = RUN_LAYER_UPGRADED_TIER;
		Pool->UpdateWeapon(Upgraded);
		TestTrue(TEXT("a run-only axe can be dismantled"), Economy->DismantleWeapon(Owner, AxeID));
		EItemTier Remembered = EItemTier::F_Tier;
		TestTrue(TEXT("its tier is remembered at the removal"), Pool->FindRememberedTier(Axe, Remembered) && Remembered == RUN_LAYER_UPGRADED_TIER);
		TestFalse(TEXT("its mark is gone"), Pool->IsRunOnly(AxeID));
		Pool->EndRunLayer();
		TestEqual(TEXT("the home dagger is still home"), CountInStore(*Pool, DaggerID), 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
