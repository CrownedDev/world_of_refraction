// PerInstanceRollsRetiredSpec.cpp
// B-335 (D-127): per-instance random rolls retire (flagged OBSOLETE; deleted in the D-124 sweep). Gear keeps tier
// and quality. The roll path is already off, so a fresh weapon, ring or evolution ships its authored stats unchanged:
// no roll, no MaxPool seed. Every roll call (RerollSubstats / RerollPillars / RerollResistance) sits inside the
// OBSOLETE path behind bRandomGenerateOnPickup or the editor preview buttons; this spec guards the default.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"

namespace
{
	constexpr int32 ROLL_TEST_RAW_DAMAGE = 5;
	constexpr int32 ROLL_TEST_SPELL_DAMAGE = 0;
	constexpr float ROLL_TEST_FIRE_RESIST = 3.0f;
	constexpr float ROLL_TEST_WATER_RESIST = 0.0f;
	constexpr int32 ROLL_TEST_NO_POOL = 0;
	constexpr float ROLL_TEST_TOLERANCE = 0.001f;
}

BEGIN_DEFINE_SPEC(FPerInstanceRollsRetiredSpec, "WoR.Equipment.PerInstanceRollsRetired",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	UEvolutionInventoryComponent *Evolutions = nullptr;

	template <class TAsset>
	TAsset *NewAuthoredGear() const
	{
		TAsset *Asset = NewObject<TAsset>(GetTransientPackage());
		Asset->BaseStatBonus.BonusRawDamage = ROLL_TEST_RAW_DAMAGE;
		Asset->BaseResistance.Fire = ROLL_TEST_FIRE_RESIST;
		return Asset;
	}

	void TestAuthoredOnly(const TCHAR *What, const FEquipmentStatBonus &Stats, const FResistanceBonus &Resistance,
						  int32 StatMaxPool, int32 ResistanceMaxPool)
	{
		TestEqual(FString::Printf(TEXT("%s: authored Raw Damage kept"), What), Stats.BonusRawDamage, ROLL_TEST_RAW_DAMAGE);
		TestEqual(FString::Printf(TEXT("%s: no rolled Spell Damage"), What), Stats.BonusSpellDamage, ROLL_TEST_SPELL_DAMAGE);
		TestNearlyEqual(FString::Printf(TEXT("%s: authored Fire resistance kept"), What), Resistance.Fire, ROLL_TEST_FIRE_RESIST, ROLL_TEST_TOLERANCE);
		TestNearlyEqual(FString::Printf(TEXT("%s: no rolled Water resistance"), What), Resistance.Water, ROLL_TEST_WATER_RESIST, ROLL_TEST_TOLERANCE);
		TestEqual(FString::Printf(TEXT("%s: no stat roll budget seeded"), What), StatMaxPool, ROLL_TEST_NO_POOL);
		TestEqual(FString::Printf(TEXT("%s: no resistance roll budget seeded"), What), ResistanceMaxPool, ROLL_TEST_NO_POOL);
	}
END_DEFINE_SPEC(FPerInstanceRollsRetiredSpec)

void FPerInstanceRollsRetiredSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Combatant"), Actor))
		{
			return;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		Evolutions = FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Evolutions = nullptr;
		TestWorld.Reset();
	});

	It("keeps the pickup roll off by default on every gear asset", [this]()
	{
		TestFalse(TEXT("Weapon"), NewObject<UWeaponData>(GetTransientPackage())->bRandomGenerateOnPickup);
		TestFalse(TEXT("Adapter (ring)"), NewObject<URingData>(GetTransientPackage())->bRandomGenerateOnPickup);
		TestFalse(TEXT("Evolution"), NewObject<UEvolutionItemData>(GetTransientPackage())->bRandomGenerateOnPickup);
	});

	It("gives a fresh weapon its authored stats, unrolled", [this]()
	{
		if (!Inventory) return;
		TestTrue(TEXT("Weapon added"), Inventory->AddWeapon(NewAuthoredGear<UWeaponData>()));
		if (!TestTrue(TEXT("One weapon owned"), Inventory->Weapons.Num() > 0)) return;
		const FWeaponInventoryEntry &Entry = Inventory->Weapons.Last();
		TestAuthoredOnly(TEXT("Weapon"), Entry.StatBonus, Entry.ResistanceBonus, Entry.StatMaxPool, Entry.ResistanceMaxPool);
	});

	It("gives a fresh adapter its authored stats, unrolled", [this]()
	{
		if (!Inventory) return;
		TestTrue(TEXT("Adapter added"), Inventory->AddRing(NewAuthoredGear<URingData>()));
		if (!TestTrue(TEXT("One adapter owned"), Inventory->Rings.Num() > 0)) return;
		const FRingInventoryEntry &Entry = Inventory->Rings.Last();
		TestAuthoredOnly(TEXT("Adapter"), Entry.StatBonus, Entry.ResistanceBonus, Entry.StatMaxPool, Entry.ResistanceMaxPool);
	});

	It("rolls nothing into a fresh evolution", [this]()
	{
		if (!Evolutions) return;
		TestTrue(TEXT("Evolution added"), Evolutions->AddInstance(NewObject<UEvolutionItemData>(GetTransientPackage())));
		if (!TestTrue(TEXT("One evolution owned"), Evolutions->Entries.Num() > 0)) return;
		const FEvolutionInventoryEntry &Entry = Evolutions->Entries.Last();
		TestEqual(TEXT("No rolled Raw Damage"), Entry.GeneratedStatBonus.BonusRawDamage, 0);
		TestNearlyEqual(TEXT("No rolled Fire resistance"), Entry.GeneratedResistance.Fire, 0.0f, ROLL_TEST_TOLERANCE);
		TestEqual(TEXT("No stat roll budget seeded"), Entry.StatMaxPool, ROLL_TEST_NO_POOL);
		TestEqual(TEXT("No resistance roll budget seeded"), Entry.ResistanceMaxPool, ROLL_TEST_NO_POOL);
	});
}

#endif
