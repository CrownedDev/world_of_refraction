// RunEndLoadoutSpec.cpp
// B-503t (D-219, audit finding 4): what a run ends with is what the hub has. Finish, extract and out of wipes carry the
// run's loadout home (sockets as they stand, wear reset; the loadout in use); a quit and a game closed mid-run put the
// run-start loadout back. Found gear equipped at the end, and a found crystal in a socket, stay until the next run
// starts. The loadout in use is kept across draws and saves.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 RUN_END_FULL = 10;
	constexpr int32 RUN_END_WORN = 3;
	constexpr int32 RUN_END_SEED = 503;
	constexpr int32 RUN_END_FIELD_PICK_BEFORE = 0;
	constexpr int32 RUN_END_FIELD_PICK_IN_RUN = 2;
	constexpr int32 RUN_END_CUSTOM_INDEX = 1; // the lore loadout, then the custom one
	const FCrystalId RUN_END_HOME_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId RUN_END_FOUND_GEM(ECrystalType::Emerald, EItemTier::F_Tier);
	/** A carrying end with no run-only piece equipped. */
	const TSet<FGuid> NothingEquipped;

	FRuntimeAttachedItem RunEndGem(const FCrystalId &Id, int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(Id, Durability);
		return Attached;
	}

	FSavedLoadout RunEndLoadout(const TCHAR *Name, UWeaponData *Weapon, const FGuid &WeaponID)
	{
		FSavedLoadout Loadout;
		Loadout.LoadoutName = Name;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Weapon;
		Loadout.PrimaryWeaponInstance = WeaponID;
		return Loadout;
	}
}

BEGIN_DEFINE_SPEC(FRunEndLoadoutSpec, "WoR.Trial.RunEndLoadout", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	UCharacterData *Member = nullptr;

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Member); }

	/** A home weapon owned and worn by the member, carrying Gem (none when Durability is 0). */
	FGuid AddHomeWeapon(UWeaponData *Weapon, const FCrystalId &Gem, int32 Durability)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = Durability > 0 ? RunEndGem(Gem, Durability) : FRuntimeAttachedItem();
		Pool->SetGearOwner(Entry.PersistentID, Key());
		Pool->SetWearer(Entry.PersistentID, Key());
		Pool->UpdateWeapon(Entry);
		return Entry.PersistentID;
	}

	/** A weapon found in the run (run-only), owned by the member. */
	FGuid AddFoundWeapon(UWeaponData *Weapon)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		Entry.PersistentID = FGuid::NewGuid();
		Pool->SetGearOwner(Entry.PersistentID, Key());
		Pool->UpdateWeapon(Entry);
		Pool->MarkRunOnly(Entry.PersistentID);
		return Entry.PersistentID;
	}

	void SetSocket(const FGuid &WeaponID, const FRuntimeAttachedItem &Attached)
	{
		FWeaponInventoryEntry Entry = *Pool->FindWeaponInPool(WeaponID);
		Entry.AttachedItem = Attached;
		Pool->UpdateWeapon(Entry);
	}

	/** The member as a level spawns it: one lore sword loadout, then the draw (custom loadouts after it). */
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
		Loadout.bLore = true;
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	FPartyMember *PartyMember() const
	{
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		return Party && Party->Members.Num() > 0 ? &Party->Members[0] : nullptr;
	}
END_DEFINE_SPEC(FRunEndLoadoutSpec)

void FRunEndLoadoutSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Run end sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Run end dagger");
		Member = FWoRTestWorld::NewCharacterData();
		Member->Name = TEXT("Run end member");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		FPartyMember &Added = Party->Members.AddDefaulted_GetRef();
		Added.PawnClass = ACombatCharacter::StaticClass();
		Added.CharacterData = Member;
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
		Member = nullptr;
		TestWorld.Reset();
	});

	It("carries a home piece's run socket home at full wear, and a quit puts the home one back", [this]()
	{
		if (!Pool) return;
		const FGuid Carried = AddHomeWeapon(Sword, RUN_END_HOME_GEM, RUN_END_FULL);
		const FGuid Quit = AddHomeWeapon(Dagger, RUN_END_HOME_GEM, RUN_END_FULL);
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun({Key()});
		SetSocket(Carried, RunEndGem(RUN_END_FOUND_GEM, RUN_END_WORN)); // swapped at a rest spot, then worn
		Pool->EndRunLayer(/*bKeepUpgrades=*/true, &NothingEquipped);
		const FWeaponInventoryEntry *After = Pool->FindWeaponInPool(Carried);
		TestTrue(TEXT("the run's gem stays in the socket"), After && After->AttachedItem.Crystal.Id == RUN_END_FOUND_GEM);
		TestTrue(TEXT("its wear reset at run end (D-093)"), After && After->AttachedItem.GetCurrentDurability() == After->AttachedItem.GetMaxDurability());

		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun({Key()});
		SetSocket(Quit, RunEndGem(RUN_END_FOUND_GEM, RUN_END_WORN));
		Pool->EndRunLayer(/*bKeepUpgrades=*/false, nullptr);
		const FWeaponInventoryEntry *Home = Pool->FindWeaponInPool(Quit);
		TestTrue(TEXT("a quit puts the home gem back"), Home && Home->AttachedItem.Crystal.Id == RUN_END_HOME_GEM &&
															  Home->AttachedItem.GetCurrentDurability() == RUN_END_FULL);
	});

	It("keeps found gear equipped at a carrying end until the next run starts, and drops the rest", [this]()
	{
		if (!Pool) return;
		Pool->BeginRunLayer();
		const FGuid Equipped = AddFoundWeapon(Sword);
		const FGuid Unequipped = AddFoundWeapon(Dagger);
		const TSet<FGuid> Worn = {Equipped};
		const FPoolRunEndResult Result = Pool->EndRunLayer(/*bKeepUpgrades=*/false, &Worn); // out of wipes carries too
		TestEqual(TEXT("one kept"), Result.Kept, 1);
		TestEqual(TEXT("one dropped"), Result.Dropped, 1);
		TestNotNull(TEXT("the equipped find is worn home"), Pool->FindWeaponInPool(Equipped));
		TestNull(TEXT("the unequipped find goes back as before"), Pool->FindWeaponInPool(Unequipped));
		TestEqual(TEXT("it is listed as kept"), Pool->GetKeptRunGear().Num(), 1);

		Pool->BeginRunLayer();
		TestNull(TEXT("the next run starts without it"), Pool->FindWeaponInPool(Equipped));
		TestEqual(TEXT("nothing kept any more"), Pool->GetKeptRunGear().Num(), 0);
		Pool->EndRunLayer();
	});

	It("keeps a found crystal in its socket until the next run, and the gem the socket gave up is the player's own", [this]()
	{
		if (!Pool) return;
		const FGuid SwordID = AddHomeWeapon(Sword, RUN_END_HOME_GEM, RUN_END_FULL);
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun({Key()});
		Pool->CarryOrStore(Key(), RUN_END_FOUND_GEM, 1); // found
		Pool->TakeCarried(Key(), RUN_END_FOUND_GEM, 1); // socketed: the home gem goes to the pouch
		SetSocket(SwordID, RunEndGem(RUN_END_FOUND_GEM, RUN_END_FULL));
		Pool->CarryOrStore(Key(), RUN_END_HOME_GEM, 1);

		TestEqual(TEXT("one found crystal stays in its socket"), Pool->KeepFoundCrystalsInSockets(), 1);
		TestEqual(TEXT("out of wipes takes no item: the pouch's gem was the player's"), Pool->TakeRunFoundItems(), 0);
		Pool->EndRunLayer(/*bKeepUpgrades=*/false, &NothingEquipped);
		TestEqual(TEXT("the home gem is still carried"), Pool->GetCarriedCount(Key(), RUN_END_HOME_GEM), 1);
		TestTrue(TEXT("the found gem is in the socket"), Pool->FindWeaponInPool(SwordID)->AttachedItem.Crystal.Id == RUN_END_FOUND_GEM);

		Pool->BeginRunLayer();
		TestTrue(TEXT("the next run starts with the socket empty"), Pool->FindWeaponInPool(SwordID)->AttachedItem.IsEmpty());
		TestEqual(TEXT("the home gem stays"), Pool->GetCarriedCount(Key(), RUN_END_HOME_GEM), 1);
		Pool->EndRunLayer();
	});

	It("takes the kept found crystal from the pouch when the hub moved it out of its socket", [this]()
	{
		if (!Pool) return;
		const FGuid SwordID = AddHomeWeapon(Sword, RUN_END_HOME_GEM, 0);
		Pool->BeginRunLayer();
		Pool->TakeHomeGearOnRun({Key()});
		SetSocket(SwordID, RunEndGem(RUN_END_FOUND_GEM, RUN_END_FULL)); // found straight into the socket
		TestEqual(TEXT("kept in its socket"), Pool->KeepFoundCrystalsInSockets(), 1);
		Pool->StoreRunFoundItems();
		Pool->EndRunLayer(/*bKeepUpgrades=*/true, &NothingEquipped);

		SetSocket(SwordID, FRuntimeAttachedItem()); // changed in the hub
		Pool->CarryOrStore(Key(), RUN_END_FOUND_GEM, 1);
		Pool->BeginRunLayer();
		TestEqual(TEXT("the next run takes it from the pouch"), Pool->GetCarriedCount(Key(), RUN_END_FOUND_GEM), 0);
		Pool->EndRunLayer();
	});

	It("keeps the loadout in use across draws (audit finding 4)", [this]()
	{
		if (!Pool) return;
		const FGuid DaggerID = AddHomeWeapon(Dagger, RUN_END_HOME_GEM, 0);
		Pool->SetCustomLoadouts(Key(), {RunEndLoadout(TEXT("Dagger loadout"), Dagger, DaggerID)});
		UInventoryComponent *First = SpawnMember();
		if (!TestNotNull(TEXT("Member"), First)) return;
		TestEqual(TEXT("the asset's default at first"), First->ActiveLoadoutIndex, 0);
		ULoadoutComponent *Loadout = First->GetOwner()->FindComponentByClass<ULoadoutComponent>();
		TestTrue(TEXT("the custom loadout put in use"), Loadout && Loadout->SetActiveLoadoutIndex(RUN_END_CUSTOM_INDEX));
		First->GetOwner()->Destroy(); // EndPlay pushes

		UInventoryComponent *Next = SpawnMember();
		if (!TestNotNull(TEXT("Member on the next map"), Next)) return;
		TestEqual(TEXT("still in use on the next map"), Next->ActiveLoadoutIndex, RUN_END_CUSTOM_INDEX);
	});

	It("puts the run-start loadouts, loadout in use and field pick back when the run is quit", [this]()
	{
		if (!Pool) return;
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		FPartyMember *PartyMember = this->PartyMember();
		if (!TestNotNull(TEXT("Trial run"), TrialRun) || !TestNotNull(TEXT("Party member"), PartyMember)) return;
		const FGuid DaggerID = AddHomeWeapon(Dagger, RUN_END_HOME_GEM, 0);
		Pool->SetCustomLoadouts(Key(), {RunEndLoadout(TEXT("Home loadout"), Dagger, DaggerID)});
		Pool->SetActiveLoadoutIndex(Key(), 0);
		PartyMember->FieldAttackIndex = RUN_END_FIELD_PICK_BEFORE;
		TrialRun->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_END_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		TestTrue(TEXT("the run is open"), Pool->IsRunLayerActive());

		UInventoryComponent *OnRun = SpawnMember();
		if (!TestNotNull(TEXT("Member on the run"), OnRun)) return;
		OnRun->SavedLoadouts[RUN_END_CUSTOM_INDEX].LoadoutName = TEXT("Edited in the run");
		OnRun->ActiveLoadoutIndex = RUN_END_CUSTOM_INDEX;
		OnRun->PushCustomLoadoutsToPool(); // a mid-run save
		PartyMember->FieldAttackIndex = RUN_END_FIELD_PICK_IN_RUN;

		TrialRun->SettleRunAsQuit(/*bTravelIfAway=*/false);
		OnRun->PushCustomLoadoutsToPool(); // the member's EndPlay, after the quit
		const TArray<FSavedLoadout> *Back = Pool->GetCustomLoadouts(Key());
		TestTrue(TEXT("the run-start custom loadout is back"), Back && Back->Num() == 1 && (*Back)[0].LoadoutName == TEXT("Home loadout"));
		TestEqual(TEXT("the run-start loadout in use is back"), Pool->GetActiveLoadoutIndex(Key()), 0);
		TestEqual(TEXT("the run-start field pick is back"), PartyMember->FieldAttackIndex, RUN_END_FIELD_PICK_BEFORE);
	});

	It("saves the loadout in use, the kept gear and the run-start snapshot", [this]()
	{
		if (!Pool) return;
		const FGuid DaggerID = AddHomeWeapon(Dagger, RUN_END_HOME_GEM, 0);
		Pool->SetCustomLoadouts(Key(), {RunEndLoadout(TEXT("Home loadout"), Dagger, DaggerID)});
		Pool->SetActiveLoadoutIndex(Key(), RUN_END_CUSTOM_INDEX);
		Pool->RestoreKeptRunGear({DaggerID}, {});
		const FSavedPool SavedPool = USaveSubsystem::CapturePool(*Pool);

		Pool->BeginRunLayer(); // drops the kept dagger, snapshots the loadouts
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun)) return;
		TrialRun->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_END_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		const FSavedRun SavedRun = USaveSubsystem::CaptureRun(*TrialRun, 0);
		TestTrue(TEXT("the run saves its start loadouts"), SavedRun.bHasRunStartLoadouts && SavedRun.RunStartLoadouts.Num() == 1);
		Pool->EndRunLayer();

		Pool->ResetStore();
		USaveSubsystem::RestorePool(SavedPool, *Pool);
		TestEqual(TEXT("the loadout in use is loaded"), Pool->GetActiveLoadoutIndex(Key()), RUN_END_CUSTOM_INDEX);
		TestEqual(TEXT("the kept gear is loaded"), Pool->GetKeptRunGear().Num(), 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
