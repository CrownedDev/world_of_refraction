// GearChangeDropsSpec.cpp
// B-171 (D-081 = A): a change to an owned weapon's socket goes through, and every loadout holding
// that weapon drops the parts it broke at once, with a message naming them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "PartsDroppedRecorder.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FCrystalId GEAR_DROPS_FIRE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId GEAR_DROPS_WATER_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	constexpr int32 GEAR_DROPS_LOADOUTS = 2;
	constexpr int32 GEAR_DROPS_FIRE_GEMS = 2;
}

BEGIN_DEFINE_SPEC(FGearChangeDropsSpec, "WoR.Loadout.GearChangeDrops",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadouts = nullptr;
	UPartsDroppedRecorder *Recorder = nullptr;
	USpellData *FireBolt = nullptr;
	FGuid SwordId;

	/** Both loadouts hold the owned sword with Fire Bolt assigned to it. */
	void AssignFireBoltInBothLoadouts()
	{
		for (FCombatLoadout &Preset : Inventory->SavedLoadouts)
		{
			Preset.PrimaryWeapon.AssignedSpells = {FireBolt};
		}
	}

	bool HoldsFireBolt(int32 LoadoutIndex) const
	{
		return Inventory->SavedLoadouts[LoadoutIndex].PrimaryWeapon.AssignedSpells.Contains(FireBolt);
	}
END_DEFINE_SPEC(FGearChangeDropsSpec)

void FGearChangeDropsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Owner)
		{
			return;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		Loadouts = FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);

		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		TestTrue(TEXT("Sword owned"), Inventory->AddWeapon(Sword));
		SwordId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();
		// Two: a gem taken off is not returned, and one case puts a Fire gem back on.
		TestTrue(TEXT("Fire gems owned"), Inventory->AddCrystal(GEAR_DROPS_FIRE_GEM, GEAR_DROPS_FIRE_GEMS));
		TestTrue(TEXT("Water gem owned"), Inventory->AddCrystal(GEAR_DROPS_WATER_GEM));

		FireBolt = NewObject<USpellData>(GetTransientPackage());
		FireBolt->Name = TEXT("Fire Bolt");
		FireBolt->Element = ESpellElement::Fire;
		TestTrue(TEXT("Fire Bolt learned"), Inventory->LearnSpell(FireBolt));

		Inventory->SavedLoadouts.Empty();
		for (int32 Index = 0; Index < GEAR_DROPS_LOADOUTS; ++Index)
		{
			FCombatLoadout Preset;
			Preset.PrimarySlotType = EPrimarySlotType::Weapon;
			Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
			Inventory->SavedLoadouts.Add(Preset);
		}
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		TestTrue(TEXT("Fire gem on the sword"), Inventory->AttachCrystalToWeapon(SwordId, GEAR_DROPS_FIRE_GEM));

		Recorder = NewObject<UPartsDroppedRecorder>();
		Loadouts->OnPartsDropped.AddDynamic(Recorder, &UPartsDroppedRecorder::Record);
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Loadouts = nullptr;
		Recorder = nullptr;
		FireBolt = nullptr;
		TestWorld.Reset();
	});

	It("keeps a spell the new crystal still powers", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		AssignFireBoltInBothLoadouts();
		TestTrue(TEXT("Fire gem off"), Inventory->RemoveCrystalFromWeapon(0));
		Recorder->LoadoutIndices.Empty();
		Recorder->Messages.Empty();
		AssignFireBoltInBothLoadouts();

		TestTrue(TEXT("Fire gem back on"), Inventory->AttachCrystalToWeapon(SwordId, GEAR_DROPS_FIRE_GEM));
		TestTrue(TEXT("Loadout 0 keeps Fire Bolt"), HoldsFireBolt(0));
		TestTrue(TEXT("Loadout 1 keeps Fire Bolt"), HoldsFireBolt(1));
		TestEqual(TEXT("Nothing announced"), Recorder->Messages.Num(), 0);
	});

	It("drops a spell from every loadout when its crystal comes off, with a message", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		AssignFireBoltInBothLoadouts();

		TestTrue(TEXT("Fire gem off"), Inventory->RemoveCrystalFromWeapon(0));
		TestFalse(TEXT("Loadout 0 dropped Fire Bolt"), HoldsFireBolt(0));
		TestFalse(TEXT("Loadout 1 dropped Fire Bolt"), HoldsFireBolt(1));
		TestEqual(TEXT("One announcement per loadout"), Recorder->LoadoutIndices.Num(), GEAR_DROPS_LOADOUTS);
		TestTrue(TEXT("The message names the spell"),
				 Recorder->Messages.Num() > 0 && Recorder->Messages[0].ToString().Contains(TEXT("Fire Bolt")));
	});

	It("drops a spell the swapped-in crystal's element doesn't power", [this]()
	{
		if (!Inventory || !SwordId.IsValid()) return;
		TestTrue(TEXT("Fire gem off"), Inventory->RemoveCrystalFromWeapon(0));
		AssignFireBoltInBothLoadouts();
		Recorder->LoadoutIndices.Empty();
		Recorder->Messages.Empty();

		TestTrue(TEXT("Water gem on"), Inventory->AttachCrystalToWeapon(SwordId, GEAR_DROPS_WATER_GEM));
		TestFalse(TEXT("Loadout 0 dropped Fire Bolt"), HoldsFireBolt(0));
		TestFalse(TEXT("Loadout 1 dropped Fire Bolt"), HoldsFireBolt(1));
		TestTrue(TEXT("The message names the spell"),
				 Recorder->Messages.Num() > 0 && Recorder->Messages[0].ToString().Contains(TEXT("Fire Bolt")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
