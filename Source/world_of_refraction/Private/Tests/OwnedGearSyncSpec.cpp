// OwnedGearSyncSpec.cpp
// B-085 (Q-085): the owned weapon, ring or evolution is the truth and every loadout holding it
// stays in step: authored slots link to the owned gear, an owned change reaches every loadout, the
// active loadout's fight wear is written back, and a dismantled evolution leaves no loadout.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	const FCrystalId OWNED_SYNC_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 OWNED_SYNC_WEAR = 3;
	constexpr int32 OWNED_SYNC_LOADOUTS = 2;
	constexpr int32 OWNED_SYNC_EVO_MAX_DURABILITY = 100;
	constexpr EItemTier OWNED_SYNC_LEVELLED_TIER = EItemTier::E_Tier;
	constexpr int32 OWNED_SYNC_STEP_GEAR = 10;   // F to E, as WoR.Economy.RoundTrip
	constexpr int32 OWNED_SYNC_STEP_REALITY = 5;
}

BEGIN_DEFINE_SPEC(FOwnedGearSyncSpec, "WoR.Inventory.OwnedGearSync",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Owner = nullptr;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	UEvolutionInventoryComponent *Evolutions = nullptr;
	UWeaponData *Sword = nullptr;
	FGuid SwordId;

	/** Two loadouts holding the sword the way an authored loadout arrives: an asset copy, unlinked. */
	void AddTwoSwordLoadouts()
	{
		Inventory->SavedLoadouts.Empty();
		for (int32 Index = 0; Index < OWNED_SYNC_LOADOUTS; ++Index)
		{
			FCombatLoadout Preset;
			Preset.PrimarySlotType = EPrimarySlotType::Weapon;
			Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
			Inventory->SavedLoadouts.Add(Preset);
		}
		Inventory->ActiveLoadoutIndex = 0;
	}

	const FWeaponInventoryEntry &SlotIn(int32 LoadoutIndex) const
	{
		return Inventory->SavedLoadouts[LoadoutIndex].PrimaryWeapon.WeaponEntry;
	}

	const FWeaponInventoryEntry &OwnedSword() const { return Inventory->Weapons[0]; }
END_DEFINE_SPEC(FOwnedGearSyncSpec)

void FOwnedGearSyncSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Owner)
		{
			return;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		Evolutions = FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		Loadout = FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);

		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Tier = EItemTier::F_Tier;
		TestTrue(TEXT("Sword owned"), Inventory->AddWeapon(Sword));
		TestTrue(TEXT("A gem owned"), Inventory->AddCrystal(OWNED_SYNC_GEM));
		SwordId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();
		AddTwoSwordLoadouts();
	});

	AfterEach([this]()
	{
		Owner = nullptr;
		Inventory = nullptr;
		Loadout = nullptr;
		Evolutions = nullptr;
		Sword = nullptr;
		TestWorld.Reset();
	});

	It("links authored loadout slots to the owned sword, once", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		TestFalse(TEXT("Unlinked before"), SlotIn(0).PersistentID == SwordId);
		TestEqual(TEXT("Both loadouts linked"), Inventory->LinkLoadoutsToOwned(), OWNED_SYNC_LOADOUTS);
		TestTrue(TEXT("Loadout 0 points at the owned sword"), SlotIn(0).PersistentID == SwordId);
		TestTrue(TEXT("Loadout 1 points at the owned sword"), SlotIn(1).PersistentID == SwordId);
		TestEqual(TEXT("Linking again changes nothing"), Inventory->LinkLoadoutsToOwned(), 0);
		TestTrue(TEXT("Debug string says in sync"), Inventory->GetOwnedGearSyncString().Contains(TEXT("in sync")));
	});

	It("shows a gem attached to the owned sword in every loadout", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		Inventory->LinkLoadoutsToOwned();
		TestTrue(TEXT("Gem attached to the owned sword"), Inventory->AttachCrystalToWeapon(SwordId, OWNED_SYNC_GEM));
		TestTrue(TEXT("Loadout 0 has the gem"), SlotIn(0).AttachedItem.IsCrystal());
		TestTrue(TEXT("Loadout 1 has the gem"), SlotIn(1).AttachedItem.IsCrystal());

		TestTrue(TEXT("Gem taken off the owned sword"), Inventory->RemoveCrystalFromWeapon(0));
		TestTrue(TEXT("Loadout 0 empty again"), SlotIn(0).AttachedItem.IsEmpty());
		TestTrue(TEXT("Loadout 1 empty again"), SlotIn(1).AttachedItem.IsEmpty());
	});

	It("writes the active loadout's fight wear back to the owned sword and the other loadout", [this]()
	{
		if (!Inventory || !Loadout || !SwordId.IsValid()) return;
		Inventory->LinkLoadoutsToOwned();
		Inventory->AttachCrystalToWeapon(SwordId, OWNED_SYNC_GEM);
		const int32 Fresh = OwnedSword().AttachedItem.GetCurrentDurability();
		if (!TestTrue(TEXT("The gem can wear"), Fresh > OWNED_SYNC_WEAR)) return;

		Loadout->ApplyWearToCrystalEntryByHolder(Sword, OWNED_SYNC_WEAR);
		TestEqual(TEXT("Mid-fight the active loadout leads"), SlotIn(0).AttachedItem.GetCurrentDurability(), Fresh - OWNED_SYNC_WEAR);
		TestEqual(TEXT("Owned not yet written"), OwnedSword().AttachedItem.GetCurrentDurability(), Fresh);
		TestTrue(TEXT("Debug string shows the difference"), Inventory->GetOwnedGearSyncString().Contains(TEXT("DIFFERS")));

		TestEqual(TEXT("One owned entry written"), Inventory->WriteLoadoutToOwned(0), 1);
		TestEqual(TEXT("Owned sword worn"), OwnedSword().AttachedItem.GetCurrentDurability(), Fresh - OWNED_SYNC_WEAR);
		TestEqual(TEXT("Other loadout worn too"), SlotIn(1).AttachedItem.GetCurrentDurability(), Fresh - OWNED_SYNC_WEAR);
	});

	It("writes the outgoing loadout back when the active loadout is switched", [this]()
	{
		if (!Inventory || !Loadout || !SwordId.IsValid()) return;
		Inventory->LinkLoadoutsToOwned();
		Inventory->AttachCrystalToWeapon(SwordId, OWNED_SYNC_GEM);
		const int32 Fresh = OwnedSword().AttachedItem.GetCurrentDurability();
		Loadout->ApplyWearToCrystalEntryByHolder(Sword, OWNED_SYNC_WEAR);

		TestTrue(TEXT("Switched to loadout 1"), Loadout->SetActiveLoadoutIndex(1));
		TestEqual(TEXT("The incoming loadout's sword is the worn one"), SlotIn(1).AttachedItem.GetCurrentDurability(),
				  Fresh - OWNED_SYNC_WEAR);
	});

	It("shows a level-up in every loadout", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		UCurrencyComponent *Wallet = Owner->FindComponentByClass<UCurrencyComponent>();
		if (!TestNotNull(TEXT("EconomyService"), Economy) || !TestNotNull(TEXT("Wallet"), Wallet)) return;
		Inventory->LinkLoadoutsToOwned();
		Wallet->AddGearEssence(OWNED_SYNC_STEP_GEAR);
		Wallet->AddEssenceType(EEssenceType::Reality, OWNED_SYNC_STEP_REALITY);

		TestTrue(TEXT("Levelled"), Economy->LevelUpWeapon(Owner, SwordId));
		TestEqual(TEXT("Loadout 0 shows the new tier"), SlotIn(0).Tier, OWNED_SYNC_LEVELLED_TIER);
		TestEqual(TEXT("Loadout 1 shows the new tier"), SlotIn(1).Tier, OWNED_SYNC_LEVELLED_TIER);
	});

	It("links a primary evolution and takes it out of the loadout when dismantled", [this]()
	{
		if (!Inventory || !Evolutions) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		if (!TestNotNull(TEXT("EconomyService"), Economy)) return;
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->MaxDurability = OWNED_SYNC_EVO_MAX_DURABILITY;
		TestTrue(TEXT("Evolution owned"), Evolutions->AddInstance(Evolution));
		const FGuid EvolutionId = Evolutions->Entries.Num() > 0 ? Evolutions->Entries.Last().InstanceID : FGuid();

		FCombatLoadout &Preset = Inventory->SavedLoadouts[1];
		Preset.PrimarySlotType = EPrimarySlotType::Evolution;
		Preset.PrimaryEvolution.Item = Evolution;
		Preset.PrimaryEvolution.CurrentDurability = OWNED_SYNC_EVO_MAX_DURABILITY;

		Inventory->LinkLoadoutsToOwned();
		TestTrue(TEXT("Primary slot points at the owned evolution"), Inventory->SavedLoadouts[1].PrimaryEvolutionInstance == EvolutionId);

		TestTrue(TEXT("Dismantled"), Economy->DismantleEvolution(Owner, EvolutionId));
		TestNull(TEXT("No ghost evolution in the loadout"), Inventory->SavedLoadouts[1].PrimaryEvolution.Item);
		TestFalse(TEXT("No stale instance id"), Inventory->SavedLoadouts[1].PrimaryEvolutionInstance.IsValid());
	});

	It("takes a dismantled evolution off the sword it was attached to, in every loadout", [this]()
	{
		if (!Inventory || !Evolutions || !SwordId.IsValid()) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		if (!TestNotNull(TEXT("EconomyService"), Economy)) return;
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->MaxDurability = OWNED_SYNC_EVO_MAX_DURABILITY;
		TestTrue(TEXT("Evolution owned"), Evolutions->AddInstance(Evolution));
		const FGuid EvolutionId = Evolutions->Entries.Num() > 0 ? Evolutions->Entries.Last().InstanceID : FGuid();
		Inventory->LinkLoadoutsToOwned();
		TestTrue(TEXT("Attached to the owned sword"), Inventory->AttachEvolutionToWeapon(SwordId, EvolutionId));
		TestTrue(TEXT("Loadout 1 shows it"), SlotIn(1).AttachedItem.IsEvolution());

		TestTrue(TEXT("Dismantled"), Economy->DismantleEvolution(Owner, EvolutionId));
		TestTrue(TEXT("Owned sword's socket empty"), OwnedSword().AttachedItem.IsEmpty());
		TestTrue(TEXT("Loadout 0 socket empty"), SlotIn(0).AttachedItem.IsEmpty());
		TestTrue(TEXT("Loadout 1 socket empty"), SlotIn(1).AttachedItem.IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
