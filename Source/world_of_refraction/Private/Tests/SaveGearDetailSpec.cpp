// SaveGearDetailSpec.cpp
// B-124 (D-059 = B): a saved weapon or ring comes back whole, through the explicit save struct and
// save bytes: the same asset and GUID, its attached crystal (with its wear), its spells and its rolls.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FCrystalId GEAR_DETAIL_CRYSTAL(ECrystalType::Sapphire, EItemTier::E_Tier);
	constexpr int32 GEAR_DETAIL_WEAR = 7;
	constexpr int32 GEAR_DETAIL_RAW_DAMAGE = 12;
	constexpr float GEAR_DETAIL_FIRE_RESIST = 0.15f;
	constexpr int32 GEAR_DETAIL_STAT_POOL = 3;
	constexpr int32 GEAR_DETAIL_STAT_MAX_POOL = 8;

	/** CapturePool, then through save bytes, then RestorePool into a fresh pool. */
	UPoolSubsystem *GearDetailRoundTrip(FAutomationTestBase &Test, const UPoolSubsystem &Pool, FWoRTestWorld &After)
	{
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(Pool);
		TArray<uint8> Bytes;
		if (!Test.TestTrue(TEXT("Saved to bytes"), UGameplayStatics::SaveGameToMemory(Save, Bytes)))
		{
			return nullptr;
		}
		const UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!Test.TestNotNull(TEXT("Loaded"), Loaded) || !Test.TestNotNull(TEXT("Pool after"), Restored))
		{
			return nullptr;
		}
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *Restored);
		return Restored;
	}
}

BEGIN_DEFINE_SPEC(FSaveGearDetailSpec, "WoR.Save.GearDetail",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSaveGearDetailSpec)

void FSaveGearDetailSpec::Define()
{
	It("brings a rolled weapon back with the same asset, crystal, spells, rolls and GUID", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		UPoolSubsystem *Pool = Before.Create() ? Before.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool"), Pool)) return;

		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Tier = EItemTier::D_Tier;
		Entry.Quality = EItemQuality::B_Quality;
		Entry.AttachedItem = FRuntimeAttachedItem();
		Entry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Entry.AttachedItem.Crystal.Id = GEAR_DETAIL_CRYSTAL;
		Entry.AttachedItem.Crystal.CurrentDurability = GEAR_DETAIL_WEAR;
		Entry.AssignedSpells = {Spell};
		Entry.StatBonus.BonusRawDamage = GEAR_DETAIL_RAW_DAMAGE;
		Entry.ResistanceBonus.Fire = GEAR_DETAIL_FIRE_RESIST;
		Entry.StatPool = GEAR_DETAIL_STAT_POOL;
		Entry.StatMaxPool = GEAR_DETAIL_STAT_MAX_POOL;
		Pool->AddWeaponToPool(Entry);

		const UPoolSubsystem *Restored = GearDetailRoundTrip(*this, *Pool, After);
		if (!Restored || !TestEqual(TEXT("One weapon"), Restored->GetOwnedWeapons().Num(), 1)) return;
		const FWeaponInventoryEntry &Back = Restored->GetOwnedWeapons()[0];
		TestTrue(TEXT("Same asset"), Back.Weapon == Sword);
		TestEqual(TEXT("Same GUID"), Back.PersistentID, Entry.PersistentID);
		TestTrue(TEXT("Same tier and quality"), Back.Tier == EItemTier::D_Tier && Back.Quality == EItemQuality::B_Quality);
		TestTrue(TEXT("A crystal is attached"), Back.AttachedItem.Kind == EAttachedItemKind::Crystal);
		TestTrue(TEXT("The same crystal"), Back.AttachedItem.Crystal.Id == GEAR_DETAIL_CRYSTAL);
		TestEqual(TEXT("With its wear"), Back.AttachedItem.Crystal.CurrentDurability, GEAR_DETAIL_WEAR);
		TestTrue(TEXT("The same spell on it"), Back.AssignedSpells.Num() == 1 && Back.AssignedSpells[0] == Spell);
		TestEqual(TEXT("The same stat roll"), Back.StatBonus.BonusRawDamage, GEAR_DETAIL_RAW_DAMAGE);
		TestEqual(TEXT("The same resistance roll"), Back.ResistanceBonus.Fire, GEAR_DETAIL_FIRE_RESIST);
		TestTrue(TEXT("The same roll pools"), Back.StatPool == GEAR_DETAIL_STAT_POOL && Back.StatMaxPool == GEAR_DETAIL_STAT_MAX_POOL);
	});

	It("brings a ring back with its crystal and rolls", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		UPoolSubsystem *Pool = Before.Create() ? Before.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool"), Pool)) return;

		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(NewObject<URingData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = FRuntimeAttachedItem();
		Entry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Entry.AttachedItem.Crystal.Id = GEAR_DETAIL_CRYSTAL;
		Entry.StatBonus.BonusRawDamage = GEAR_DETAIL_RAW_DAMAGE;
		Pool->AddRingToPool(Entry);

		const UPoolSubsystem *Restored = GearDetailRoundTrip(*this, *Pool, After);
		if (!Restored || !TestEqual(TEXT("One ring"), Restored->GetOwnedRings().Num(), 1)) return;
		const FRingInventoryEntry &Back = Restored->GetOwnedRings()[0];
		TestEqual(TEXT("Same GUID"), Back.PersistentID, Entry.PersistentID);
		TestTrue(TEXT("The same crystal"), Back.AttachedItem.Crystal.Id == GEAR_DETAIL_CRYSTAL);
		TestEqual(TEXT("The same roll"), Back.StatBonus.BonusRawDamage, GEAR_DETAIL_RAW_DAMAGE);
	});

	It("brings back an attached evolution and keeps empty spell slots in place", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		UPoolSubsystem *Pool = Before.Create() ? Before.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool"), Pool)) return;

		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		Entry.AttachedItem = FRuntimeAttachedItem();
		Entry.AttachedItem.Kind = EAttachedItemKind::Evolution;
		Entry.AttachedItem.Evolution.Item = Evolution;
		Entry.AttachedItem.Evolution.InstanceID = FGuid::NewGuid();
		Entry.AttachedItem.Evolution.CurrentDurability = GEAR_DETAIL_WEAR;
		Entry.AssignedSpells = {nullptr, Spell};
		Pool->AddWeaponToPool(Entry);

		const UPoolSubsystem *Restored = GearDetailRoundTrip(*this, *Pool, After);
		if (!Restored || !TestEqual(TEXT("One weapon"), Restored->GetOwnedWeapons().Num(), 1)) return;
		const FWeaponInventoryEntry &Back = Restored->GetOwnedWeapons()[0];
		TestTrue(TEXT("The same evolution"), Back.AttachedItem.IsEvolution() && Back.AttachedItem.Evolution.Item == Evolution);
		TestEqual(TEXT("Its instance"), Back.AttachedItem.Evolution.InstanceID, Entry.AttachedItem.Evolution.InstanceID);
		TestEqual(TEXT("Its wear"), Back.AttachedItem.Evolution.CurrentDurability, GEAR_DETAIL_WEAR);
		TestTrue(TEXT("The empty first slot stays empty"),
				 Back.AssignedSpells.Num() == 2 && Back.AssignedSpells[0] == nullptr && Back.AssignedSpells[1] == Spell);
	});

	It("keeps the asset's defaults for a weapon saved before B-124", [this]()
	{
		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool"), Restored)) return;
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		FSavedPool Old;
		FSavedOwnedItem &Item = Old.Weapons.AddDefaulted_GetRef();
		Item.Def = FSavedAssetRef::Make(Sword);
		Item.InstanceID = FGuid::NewGuid();
		USaveSubsystem::RestorePool(Old, *Restored);
		if (!TestEqual(TEXT("One weapon"), Restored->GetOwnedWeapons().Num(), 1)) return;
		const FWeaponInventoryEntry Fresh = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		TestTrue(TEXT("The asset's default attachment"), Restored->GetOwnedWeapons()[0].AttachedItem == Fresh.AttachedItem);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
