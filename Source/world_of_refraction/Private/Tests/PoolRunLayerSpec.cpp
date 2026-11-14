// PoolRunLayerSpec.cpp
// B-314 (D-111, D-097, Q-314): the run layer over the gear store. Going on a run marks the real home piece (no copy);
// at run end its wear and anything attached in the run come off; gear found in the run is gone after it, and its tier
// is remembered for its definition, so the next copy found arrives at that tier. One instance per item throughout.
// Review fixes: a reloaded layer is discarded, not applied; a home piece on the run can't be dismantled (Q-314b); a
// run-only piece dismantled before run end is still remembered (Q-314c).
// B-347 (D-137): the run takes the worn loadout, free; unworn home pieces stay home.
// B-347b (Q-347): a member drawn during a run sees only what the run brought and found; a piece left home is not its
// to equip, hearing of it doesn't remove it from the store, and a home loadout naming it is kept as stored.

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
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Currency/CurrencyWallet.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 RUN_LAYER_FULL = 10;
	constexpr int32 RUN_LAYER_WORN = 2;
	constexpr int32 RUN_LAYER_WALLET_PRISMS = 500;
	constexpr EItemTier RUN_LAYER_FOUND_TIER = EItemTier::F_Tier;
	constexpr EItemTier RUN_LAYER_UPGRADED_TIER = EItemTier::C_Tier;
	constexpr EItemTier RUN_LAYER_LOWER_TIER = EItemTier::E_Tier;
	// B-439b: items carried and stored before the run, found in it, and used in it.
	constexpr int32 RUN_LAYER_ITEMS_CARRIED = 2;
	constexpr int32 RUN_LAYER_ITEMS_STORED = 4;
	constexpr int32 RUN_LAYER_ITEMS_FOUND = 3;
	constexpr int32 RUN_LAYER_ITEMS_USED = 1;
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

	TSet<FName> MemberKeys() const { return {UPoolSubsystem::GetCharacterKey(Member)}; }

	/** A home piece in the store (owned and worn by the member), its sword carrying a gem at Durability (none when 0). */
	FGuid AddHomeSword(int32 Durability)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = Durability > 0 ? RunLayerGem(Durability) : FRuntimeAttachedItem();
		Pool->SetGearOwner(Entry.PersistentID, UPoolSubsystem::GetCharacterKey(Member));
		Pool->SetWearer(Entry.PersistentID, UPoolSubsystem::GetCharacterKey(Member));
		Pool->UpdateWeapon(Entry);
		return Entry.PersistentID;
	}

	FGuid AddHomeAdapter()
	{
		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Adapter);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = FRuntimeAttachedItem();
		Pool->SetGearOwner(Entry.PersistentID, UPoolSubsystem::GetCharacterKey(Member));
		Pool->SetWearer(Entry.PersistentID, UPoolSubsystem::GetCharacterKey(Member));
		Pool->UpdateRing(Entry);
		return Entry.PersistentID;
	}

	/** The party member as each level spawns it: the sword, then the draw. bLore: its sword loadout is a lore one, so
	 *  the drawn custom loadouts line up with the store's (B-347b). */
	UInventoryComponent *SpawnMember(bool bLore = false)
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
		Loadout.bLore = bLore;
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
		TestEqual(TEXT("the home piece goes along"), Pool->TakeHomeGearOnRun(MemberKeys()), 1);
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
		Pool->TakeHomeGearOnRun(MemberKeys());

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
		Pool->TakeHomeGearOnRun(MemberKeys());
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
		Pool->TakeHomeGearOnRun(MemberKeys());
		TestTrue(TEXT("loot given again"), Inventory->AddWeapon(Dagger));
		const FGuid SecondDagger = OwnedID(Inventory, Dagger);
		const FWeaponInventoryEntry *InStore = Pool->FindWeaponInPool(SecondDagger);
		const FWeaponInventoryEntry *MemberCopy = Inventory->FindOwnedWeapon(SecondDagger);
		TestTrue(TEXT("the store's copy arrives upgraded"), InStore && InStore->Tier == RUN_LAYER_UPGRADED_TIER);
		TestTrue(TEXT("the member's copy agrees"), MemberCopy && MemberCopy->Tier == RUN_LAYER_UPGRADED_TIER);
		TestEqual(TEXT("one instance"), CountInStore(*Pool, SecondDagger), 1);
		Pool->EndRunLayer();
	});

	It("takes back what a quit run found, from the pouches then storage, and keeps what was held before (B-439b, D-176)", [this]()
	{
		if (!Pool) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Member);
		const FCrystalId Owned(ECrystalType::Emerald, EItemTier::D_Tier);
		const FCrystalId FoundOnly(ECrystalType::Emerald, EItemTier::F_Tier);
		Pool->CarryOrStore(Key, Owned, RUN_LAYER_ITEMS_CARRIED);
		Pool->AddCrystalToPool(Owned, RUN_LAYER_ITEMS_STORED);
		TestEqual(TEXT("no run: nothing to take"), Pool->TakeRunFoundItems(), 0);

		Pool->BeginRunLayer();
		Pool->CarryOrStore(Key, Owned, RUN_LAYER_ITEMS_FOUND); // a find of a kind already owned
		const int32 Overflow = Pool->CarryOrStore(Key, FoundOnly, InventoryConstants::ITEM_STACK_MAX + RUN_LAYER_ITEMS_FOUND);
		TestEqual(TEXT("a find that didn't fit went to storage"), Overflow, RUN_LAYER_ITEMS_FOUND);
		Pool->TakeCarried(Key, Owned, RUN_LAYER_ITEMS_USED); // used in the run
		TestTrue(TEXT("the print names the finds"), Pool->GetRunLayerString().Contains(TEXT("found in the run")));

		const int32 Taken = Pool->TakeRunFoundItems();
		TestEqual(TEXT("the finds left, less the one used"), Taken,
				  RUN_LAYER_ITEMS_FOUND - RUN_LAYER_ITEMS_USED + InventoryConstants::ITEM_STACK_MAX + RUN_LAYER_ITEMS_FOUND);
		TestEqual(TEXT("the owned kind is carried as before the run"), Pool->GetCarriedCount(Key, Owned), RUN_LAYER_ITEMS_CARRIED);
		TestEqual(TEXT("and stored as before (the pouch pays first)"), Pool->GetStoredCount(Owned), RUN_LAYER_ITEMS_STORED);
		TestEqual(TEXT("the found kind left the pouch"), Pool->GetCarriedCount(Key, FoundOnly), 0);
		TestEqual(TEXT("and storage"), Pool->GetStoredCount(FoundOnly), 0);
		TestEqual(TEXT("a second quit takes nothing more"), Pool->TakeRunFoundItems(), 0);
		Pool->EndRunLayer();
	});

	It("sends a finished run's pouch finds to home storage and keeps what was carried before (B-238b, D-097)", [this]()
	{
		if (!Pool) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Member);
		const FCrystalId Owned(ECrystalType::Emerald, EItemTier::D_Tier);
		const FCrystalId FoundOnly(ECrystalType::Emerald, EItemTier::F_Tier);
		Pool->CarryOrStore(Key, Owned, RUN_LAYER_ITEMS_CARRIED);
		TestEqual(TEXT("no run: nothing to store"), Pool->StoreRunFoundItems(), 0);

		Pool->BeginRunLayer();
		Pool->CarryOrStore(Key, Owned, RUN_LAYER_ITEMS_FOUND);
		Pool->CarryOrStore(Key, FoundOnly, RUN_LAYER_ITEMS_FOUND);
		TestEqual(TEXT("both finds moved"), Pool->StoreRunFoundItems(), RUN_LAYER_ITEMS_FOUND + RUN_LAYER_ITEMS_FOUND);
		TestEqual(TEXT("the owned kind is carried as before the run"), Pool->GetCarriedCount(Key, Owned), RUN_LAYER_ITEMS_CARRIED);
		TestEqual(TEXT("its find is stored"), Pool->GetStoredCount(Owned), RUN_LAYER_ITEMS_FOUND);
		TestEqual(TEXT("the found kind left the pouch"), Pool->GetCarriedCount(Key, FoundOnly), 0);
		TestEqual(TEXT("and is stored"), Pool->GetStoredCount(FoundOnly), RUN_LAYER_ITEMS_FOUND);
		Pool->EndRunLayer();
	});

	It("keeps a finished run's finds, and starting items banked mid-run survive a quit (B-439b, D-097)", [this]()
	{
		if (!Pool) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Member);
		const FCrystalId Find(ECrystalType::Emerald, EItemTier::D_Tier);
		Pool->BeginRunLayer();
		Pool->CarryOrStore(Key, Find, RUN_LAYER_ITEMS_FOUND);
		Pool->EndRunLayer(); // extracted, wiped or won: nothing is taken
		TestEqual(TEXT("the find is kept"), Pool->GetCarriedCount(Key, Find), RUN_LAYER_ITEMS_FOUND);
		TestEqual(TEXT("the run is over: nothing to take"), Pool->TakeRunFoundItems(), 0);

		Pool->BeginRunLayer();
		const FName Recruit(TEXT("RunLayerRecruit"));
		TMap<FCrystalId, int32> Starting;
		Starting.Add(Find, RUN_LAYER_ITEMS_CARRIED);
		TestTrue(TEXT("a recruit's starting items banked in the run"), Pool->BankStartingItems(Recruit, Starting));
		TestEqual(TEXT("a quit takes none of them"), Pool->TakeRunFoundItems(), 0);
		TestEqual(TEXT("the recruit still carries them"), Pool->GetCarriedCount(Recruit, Find), RUN_LAYER_ITEMS_CARRIED);
		Pool->EndRunLayer();
	});

	It("discards a layer without putting anything back", [this]()
	{
		if (!Pool) return;
		const FGuid SwordID = AddHomeSword(RUN_LAYER_FULL);
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun(MemberKeys());
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
		Pool->SetWearer(DaggerID, UPoolSubsystem::GetCharacterKey(Member)); // brought along

		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun(MemberKeys());
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

	It("starts a run with exactly the worn loadout, free, and the run's starting Gold (B-347)", [this]()
	{
		if (!Pool) return;
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Save"), Save) || !TestNotNull(TEXT("Trial run"), TrialRun) || !TestNotNull(TEXT("Member"), Inventory)) return;
		UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Inventory->GetOwner());
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Wallet->Add(ECurrencyType::Prisms, RUN_LAYER_WALLET_PRISMS);
		const FGuid SwordID = OwnedID(Inventory, Sword); // the active loadout's primary: worn since the draw
		TestTrue(TEXT("an unworn home dagger"), Inventory->AddWeapon(Dagger));
		const FGuid DaggerID = OwnedID(Inventory, Dagger);
		TestTrue(TEXT("the sword is worn by the member"), Pool->GetWearer(SwordID) == UPoolSubsystem::GetCharacterKey(Member));
		TestTrue(TEXT("the dagger is worn by nobody"), Pool->GetWearer(DaggerID).IsNone());
		const int32 WeaponsBefore = Pool->GetOwnedWeapons().Num();

		TrialRun->StartRunGearAndGold(); // what EnterTrial does, without the level travel
		TestTrue(TEXT("the run layer is open"), Pool->IsRunLayerActive());
		TestTrue(TEXT("the worn sword is on the run"), Pool->IsOnRun(SwordID));
		TestFalse(TEXT("the unworn dagger stays home"), Pool->IsOnRun(DaggerID));
		TestEqual(TEXT("exactly the worn loadout: nothing more to take"), Pool->TakeHomeGearOnRun(MemberKeys()), 0);
		TestEqual(TEXT("nothing bought or copied"), Pool->GetOwnedWeapons().Num(), WeaponsBefore);
		TestEqual(TEXT("free: no Prisms charged"), Wallet->GetBalance(ECurrencyType::Prisms), RUN_LAYER_WALLET_PRISMS);
		TestEqual(TEXT("the carried run Gold is at its start (none, B-178)"), Save->GetCarriedGold(), 0);
		Pool->EndRunLayer();
	});

	It("refuses to dismantle an unworn home piece during a run (B-347, Q-314b)", [this]()
	{
		if (!Pool) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Economy"), Economy) || !TestNotNull(TEXT("Trial run"), TrialRun) || !TestNotNull(TEXT("Member"), Inventory)) return;
		TestTrue(TEXT("an unworn home dagger"), Inventory->AddWeapon(Dagger));
		const FGuid DaggerID = OwnedID(Inventory, Dagger);
		TrialRun->StartRunGearAndGold();
		TestFalse(TEXT("left home, not on the run"), Pool->IsOnRun(DaggerID));
		TestFalse(TEXT("still refused during the run"), Economy->DismantleWeapon(Inventory->GetOwner(), DaggerID));
		TestEqual(TEXT("the dagger stays"), CountInStore(*Pool, DaggerID), 1);
		Pool->EndRunLayer();
		TestTrue(TEXT("after the run it can go"), Economy->DismantleWeapon(Inventory->GetOwner(), DaggerID));
	});

	It("takes an unworn home piece onto the run the first time it changes mid-run, and puts it back (B-347)", [this]()
	{
		if (!Pool) return;
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Trial run"), TrialRun) || !TestNotNull(TEXT("Member"), Inventory)) return;
		TestTrue(TEXT("an unworn home dagger"), Inventory->AddWeapon(Dagger));
		const FGuid DaggerID = OwnedID(Inventory, Dagger);
		TrialRun->StartRunGearAndGold();
		TestFalse(TEXT("left home at the start"), Pool->IsOnRun(DaggerID));

		FWeaponInventoryEntry *Owned = Inventory->Weapons.FindByPredicate([&DaggerID](const FWeaponInventoryEntry &E) { return E.PersistentID == DaggerID; });
		if (!TestNotNull(TEXT("the member's own dagger entry"), Owned)) { Pool->EndRunLayer(); return; }
		Owned->AttachedItem = RunLayerGem(RUN_LAYER_WORN); // a gem socketed at a rest spot
		Inventory->RefreshLoadoutsFromOwned(DaggerID);       // the member's change reaches the store (PushGearToPool)
		TestTrue(TEXT("it joins the run the first time it changes"), Pool->IsOnRun(DaggerID));
		const FWeaponInventoryEntry *InStore = Pool->FindWeaponInPool(DaggerID);
		TestTrue(TEXT("the store carries the run's gem"), InStore && !InStore->AttachedItem.IsEmpty());

		Pool->EndRunLayer();
		const FWeaponInventoryEntry *Home = Pool->FindWeaponInPool(DaggerID);
		TestTrue(TEXT("back to its home state: no gem"), Home && Home->AttachedItem.IsEmpty());
		TestEqual(TEXT("one dagger"), CountInStore(*Pool, DaggerID), 1);
	});

	It("draws only what the run brought, and the home piece left home again after it (B-347b)", [this]()
	{
		if (!Pool) return;
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UInventoryComponent *Home = SpawnMember();
		if (!TestNotNull(TEXT("Trial run"), TrialRun) || !TestNotNull(TEXT("Member"), Home)) return;
		const FGuid SwordID = OwnedID(Home, Sword);
		TestTrue(TEXT("an unworn home dagger"), Home->AddWeapon(Dagger));
		const FGuid DaggerID = OwnedID(Home, Dagger);
		TrialRun->StartRunGearAndGold();
		Home->GetOwner()->Destroy(); // the trial level: the member is spawned and draws again

		UInventoryComponent *OnRun = SpawnMember();
		if (!TestNotNull(TEXT("Member on the run"), OnRun)) { Pool->EndRunLayer(); return; }
		TestEqual(TEXT("the worn sword is in the run armoury"), OwnedID(OnRun, Sword), SwordID);
		TestFalse(TEXT("the dagger left home is not"), OwnedID(OnRun, Dagger).IsValid());
		TestFalse(TEXT("nor the member's to equip"), OnRun->OwnsWeaponAsset(Dagger));
		OnRun->RefreshLoadoutsFromOwned(DaggerID); // hearing of a piece it doesn't hold
		TestEqual(TEXT("the store keeps the dagger"), CountInStore(*Pool, DaggerID), 1);
		TestFalse(TEXT("and it doesn't join the run"), Pool->IsOnRun(DaggerID));

		Pool->EndRunLayer();
		OnRun->GetOwner()->Destroy(); // back in the hub
		UInventoryComponent *Back = SpawnMember();
		if (!TestNotNull(TEXT("Member back home"), Back)) return;
		TestEqual(TEXT("the dagger is in the armoury again"), OwnedID(Back, Dagger), DaggerID);
		TestTrue(TEXT("and the member's to equip"), Back->OwnsWeaponAsset(Dagger));
	});

	It("keeps a home loadout naming a piece left home as stored through the run (B-347b)", [this]()
	{
		if (!Pool) return;
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun)) return;
		const FName Key = UPoolSubsystem::GetCharacterKey(Member);
		FWeaponInventoryEntry HomeDagger = FWeaponInventoryEntry::CreateFromWeapon(Dagger);
		HomeDagger.PersistentID = FGuid::NewGuid();
		const FGuid DaggerID = HomeDagger.PersistentID;
		Pool->SetGearOwner(DaggerID, Key); // an unworn home dagger
		Pool->UpdateWeapon(HomeDagger);
		FSavedLoadout DaggerLoadout;
		DaggerLoadout.LoadoutName = TEXT("Dagger loadout");
		DaggerLoadout.PrimarySlotType = EPrimarySlotType::Weapon;
		DaggerLoadout.PrimaryWeapon = Dagger;
		DaggerLoadout.PrimaryWeaponInstance = DaggerID;
		Pool->SetCustomLoadouts(Key, {DaggerLoadout}); // the player's own loadout, made at home
		UInventoryComponent *Home = SpawnMember(/*bLore=*/true);
		if (!TestNotNull(TEXT("Member"), Home)) return;
		TrialRun->StartRunGearAndGold();
		TestFalse(TEXT("the dagger stays home"), Pool->IsOnRun(DaggerID));
		Home->GetOwner()->Destroy();

		UInventoryComponent *OnRun = SpawnMember(/*bLore=*/true);
		if (!TestNotNull(TEXT("Member on the run"), OnRun)) { Pool->EndRunLayer(); return; }
		OnRun->PushCustomLoadoutsToPool(); // what a mid-run save does
		const TArray<FSavedLoadout> *Stored = Pool->GetCustomLoadouts(Key);
		if (TestTrue(TEXT("one custom loadout stored"), Stored && Stored->Num() == 1))
		{
			TestEqual(TEXT("it still names the dagger"), (*Stored)[0].PrimaryWeaponInstance, DaggerID);
		}
		Pool->EndRunLayer();
		OnRun->PushCustomLoadoutsToPool(); // ExitTrial's save, after the layer closed, with the member still there
		Stored = Pool->GetCustomLoadouts(Key);
		if (TestTrue(TEXT("still one custom loadout after the run"), Stored && Stored->Num() == 1))
		{
			TestEqual(TEXT("it still names the dagger after the run"), (*Stored)[0].PrimaryWeaponInstance, DaggerID);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
