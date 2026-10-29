// GearInstanceSpec.cpp
// B-301 (D-110 unified loadout, step 1): FGearInstance holds a weapon or an adapter once. Converting today's weapon or
// ring entry to it and back keeps every field; the kind follows the asset; and the element rule is one for both kinds:
// the attachment's element, None when it is broken (CL-24).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/FGearInstance.h"

namespace
{
	const FCrystalId GEAR_INSTANCE_TEST_GEM(ECrystalType::Garnet, EItemTier::C_Tier);
	constexpr int32 GEAR_INSTANCE_TEST_DURABILITY = 7;
	constexpr int32 GEAR_INSTANCE_TEST_BROKEN = 0;
	constexpr int32 GEAR_INSTANCE_TEST_INSTANCE_ID = 4301;
	constexpr int32 GEAR_INSTANCE_TEST_POOL = 3;

	FRuntimeAttachedItem GearInstanceTestGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(GEAR_INSTANCE_TEST_GEM, Durability);
		return Attached;
	}

	template <typename TEntry>
	void FillSharedGearTestFields(TEntry &Entry)
	{
		Entry.InstanceID = GEAR_INSTANCE_TEST_INSTANCE_ID;
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Tier = EItemTier::B_Tier;
		Entry.Quality = EItemQuality::A_Quality;
		Entry.AttachedItem = GearInstanceTestGem(GEAR_INSTANCE_TEST_DURABILITY);
		Entry.AssignedSpells.Add(NewObject<USpellData>(GetTransientPackage()));
		Entry.StatPool = GEAR_INSTANCE_TEST_POOL;
		Entry.ResistanceMaxPool = GEAR_INSTANCE_TEST_POOL;
	}
}

BEGIN_DEFINE_SPEC(FGearInstanceSpec, "WoR.Loadout.GearInstance", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	template <typename TEntry>
	void TestSameGear(const FString &What, const TEntry &Before, const TEntry &After)
	{
		TestEqual(What + TEXT(" instance id"), After.InstanceID, Before.InstanceID);
		TestTrue(What + TEXT(" persistent id"), After.PersistentID == Before.PersistentID);
		TestTrue(What + TEXT(" tier"), After.Tier == Before.Tier);
		TestTrue(What + TEXT(" quality"), After.Quality == Before.Quality);
		TestEqual(What + TEXT(" durability"), After.AttachedItem.GetCurrentDurability(), Before.AttachedItem.GetCurrentDurability());
		TestTrue(What + TEXT(" spells"), After.AssignedSpells == Before.AssignedSpells);
		TestEqual(What + TEXT(" stat pool"), After.StatPool, Before.StatPool);
		TestEqual(What + TEXT(" resistance max pool"), After.ResistanceMaxPool, Before.ResistanceMaxPool);
	}
END_DEFINE_SPEC(FGearInstanceSpec)

void FGearInstanceSpec::Define()
{
	It("round-trips a weapon entry as a Weapon", [this]()
	{
		FWeaponInventoryEntry Entry;
		Entry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		FillSharedGearTestFields(Entry);
		const FGearInstance Gear = FGearInstance::FromWeaponEntry(Entry);
		TestTrue(TEXT("kind"), Gear.IsWeapon());
		TestTrue(TEXT("asset"), Gear.GetWeapon() == Entry.Weapon && Gear.GetAdapter() == nullptr);
		const FWeaponInventoryEntry Back = Gear.ToWeaponEntry();
		TestTrue(TEXT("weapon back"), Back.Weapon == Entry.Weapon);
		TestSameGear(TEXT("weapon"), Entry, Back);
		TestTrue(TEXT("not a ring"), Gear.ToRingEntry().Ring == nullptr);
	});

	It("round-trips a ring entry as an Adapter", [this]()
	{
		FRingInventoryEntry Entry;
		Entry.Ring = NewObject<URingData>(GetTransientPackage());
		FillSharedGearTestFields(Entry);
		const FGearInstance Gear = FGearInstance::FromRingEntry(Entry);
		TestTrue(TEXT("kind"), Gear.IsAdapter());
		TestTrue(TEXT("asset"), Gear.GetAdapter() == Entry.Ring && Gear.GetWeapon() == nullptr);
		const FRingInventoryEntry Back = Gear.ToRingEntry();
		TestTrue(TEXT("ring back"), Back.Ring == Entry.Ring);
		TestSameGear(TEXT("adapter"), Entry, Back);
		TestTrue(TEXT("not a weapon"), Gear.ToWeaponEntry().Weapon == nullptr);
	});

	It("gives both kinds the gem's element, and None once it breaks (CL-24)", [this]()
	{
		const ESpellElement GemElement = ItemIdentity::GetElement(GEAR_INSTANCE_TEST_GEM);
		for (const EGearKind Kind : {EGearKind::Weapon, EGearKind::Adapter})
		{
			const FString Name = UEnum::GetValueAsString(Kind);
			FGearInstance Gear;
			Gear.Kind = Kind;
			TestTrue(Name + TEXT(": no attachment, no element"), Gear.GetElement() == ESpellElement::None);
			Gear.AttachedItem = GearInstanceTestGem(GEAR_INSTANCE_TEST_DURABILITY);
			TestTrue(Name + TEXT(": the gem's element"), Gear.GetElement() == GemElement);
			Gear.AttachedItem = GearInstanceTestGem(GEAR_INSTANCE_TEST_BROKEN);
			TestTrue(Name + TEXT(": broken, no element"), Gear.GetElement() == ESpellElement::None);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
