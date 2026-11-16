// LoadoutV2InventorySpec.cpp
// B-531 (D-230): the loadout screen's Inventory tab lists what is owned and not worn, grouped Weapons, Adapters,
// Evolutions, Crystals; exactly-same pieces show as one line with a count (five empty adapters: one line of 5; socket
// one and it leaves the stack: 4 and its own line); a built piece keeps its own line.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "UI/Menus/LoadoutV2InventoryModel.h"
#include "UI/Menus/LoadoutV2Model.h"

namespace
{
	constexpr int32 InventoryTestSpareAdapters = 5;

	FWeaponInventoryEntry InventoryTestWeapon(const TCHAR *Name)
	{
		UWeaponData *Data = NewObject<UWeaponData>(GetTransientPackage());
		Data->Name = Name;
		Data->WeaponType = EWeaponType::Sword;
		FWeaponInventoryEntry Entry;
		Entry.Weapon = Data;
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	TArray<FRingInventoryEntry> InventoryTestSpares(int32 Count)
	{
		URingData *Ring = NewObject<URingData>(GetTransientPackage());
		Ring->Name = TEXT("Adapter Ring");
		TArray<FRingInventoryEntry> Spares;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FRingInventoryEntry &Entry = Spares.AddDefaulted_GetRef();
			Entry.Ring = Ring;
			Entry.PersistentID = FGuid::NewGuid();
		}
		return Spares;
	}

	TArray<const FLoadoutV2InventoryRow *> InventoryTestRowsOf(const FLoadoutV2InventoryPage &Page, ELoadoutV2InventoryRowKind Kind)
	{
		TArray<const FLoadoutV2InventoryRow *> Rows;
		for (const FLoadoutV2InventoryRow &Row : Page.Rows)
		{
			if (Row.Kind == Kind)
			{
				Rows.Add(&Row);
			}
		}
		return Rows;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutV2InventorySpec, "WoR.UI.LoadoutV2.Inventory", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutV2InventorySpec)

void FLoadoutV2InventorySpec::Define()
{
	It("lists what is owned and not worn, grouped under headers", [this]()
	{
		FCombatLoadout Worn;
		const FWeaponInventoryEntry Sword = InventoryTestWeapon(TEXT("Arming Sword"));
		const FWeaponInventoryEntry Axe = InventoryTestWeapon(TEXT("Hand Axe"));
		Worn.PrimaryWeapon.WeaponEntry = Sword;

		FEvolutionInventoryEntry Free;
		Free.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		Free.InstanceID = FGuid::NewGuid();
		FEvolutionInventoryEntry InUse = Free;
		InUse.InstanceID = FGuid::NewGuid();

		TMap<FCrystalId, int32> Pouch;
		Pouch.Add(FCrystalId(ECrystalType::Garnet, EItemTier::C_Tier), 3);
		Pouch.Add(FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier), 0);

		const FLoadoutV2InventoryPage Page = WoRLoadoutV2::BuildInventoryPage(Worn, {Sword, Axe}, InventoryTestSpares(1), {Free, InUse}, {InUse.InstanceID}, Pouch);
		AddInfo(WoRLoadoutV2::GetInventoryPageString(Page));

		TestEqual(TEXT("four groups, each under a header"), InventoryTestRowsOf(Page, ELoadoutV2InventoryRowKind::Header).Num(), 4);
		const TArray<const FLoadoutV2InventoryRow *> Weapons = InventoryTestRowsOf(Page, ELoadoutV2InventoryRowKind::Weapon);
		if (TestEqual(TEXT("the worn sword is left out"), Weapons.Num(), 1))
		{
			TestEqual(TEXT("the axe is listed"), Weapons[0]->Weapon.PersistentID, Axe.PersistentID);
		}
		const TArray<const FLoadoutV2InventoryRow *> Evolutions = InventoryTestRowsOf(Page, ELoadoutV2InventoryRowKind::Evolution);
		if (TestEqual(TEXT("the evolution in use is left out"), Evolutions.Num(), 1))
		{
			TestEqual(TEXT("the free one is listed"), Evolutions[0]->Evolution.InstanceID, Free.InstanceID);
		}
		const TArray<const FLoadoutV2InventoryRow *> Crystals = InventoryTestRowsOf(Page, ELoadoutV2InventoryRowKind::Crystal);
		if (TestEqual(TEXT("an empty pouch stack is left out"), Crystals.Num(), 1))
		{
			TestEqual(TEXT("the garnets as one line of 3"), Crystals[0]->Count, 3);
			TestTrue(TEXT("a crystal sits in its socket form"), Crystals[0]->Crystal.IsCrystal());
		}
		TestEqual(TEXT("the first line is the Weapons header"), Page.Rows[0].Kind, ELoadoutV2InventoryRowKind::Header);
	});

	It("shows five empty adapters as one line of 5; a socketed one leaves the stack (D-230)", [this]()
	{
		TArray<FRingInventoryEntry> Spares = InventoryTestSpares(InventoryTestSpareAdapters);
		const FLoadoutV2InventoryPage Five = WoRLoadoutV2::BuildInventoryPage(FCombatLoadout(), {}, Spares, {}, {}, {});
		TArray<const FLoadoutV2InventoryRow *> Adapters = InventoryTestRowsOf(Five, ELoadoutV2InventoryRowKind::Adapter);
		if (TestEqual(TEXT("one line"), Adapters.Num(), 1))
		{
			TestEqual(TEXT("of 5"), Adapters[0]->Count, InventoryTestSpareAdapters);
			TestTrue(TEXT("named with its count"), Adapters[0]->Name.ToString().EndsWith(TEXT(" x5")));
		}

		Spares[2].AttachedItem.Kind = EAttachedItemKind::Crystal;
		Spares[2].AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier);
		const FLoadoutV2InventoryPage Built = WoRLoadoutV2::BuildInventoryPage(FCombatLoadout(), {}, Spares, {}, {}, {});
		Adapters = InventoryTestRowsOf(Built, ELoadoutV2InventoryRowKind::Adapter);
		if (TestEqual(TEXT("two lines"), Adapters.Num(), 2))
		{
			TestEqual(TEXT("the stack of 4"), Adapters[0]->Count, InventoryTestSpareAdapters - 1);
			TestEqual(TEXT("the socketed one on its own"), Adapters[1]->Count, 1);
			TestEqual(TEXT("its own line is that piece"), Adapters[1]->Adapter.PersistentID, Spares[2].PersistentID);
		}
	});

	It("keeps a different tier, or a piece with picks, on its own line", [this]()
	{
		TArray<FRingInventoryEntry> Spares = InventoryTestSpares(3);
		Spares[1].Tier = EItemTier::C_Tier;
		FWeaponInventoryEntry Plain = InventoryTestWeapon(TEXT("Hand Axe"));
		FWeaponInventoryEntry Same = Plain;
		Same.PersistentID = FGuid::NewGuid();
		FWeaponInventoryEntry Picked = Plain;
		Picked.PersistentID = FGuid::NewGuid();
		Picked.SkillPicks.Add(nullptr);

		TestEqual(TEXT("the C adapter apart"), WoRLoadoutV2::GroupStacks({WoRLoadoutV2::GetStackKey(Spares[0]), WoRLoadoutV2::GetStackKey(Spares[1]),
																		 WoRLoadoutV2::GetStackKey(Spares[2])}).Num(), 2);
		TestEqual(TEXT("two plain axes stack, the picked one apart"),
				  WoRLoadoutV2::GroupStacks({WoRLoadoutV2::GetStackKey(Plain), WoRLoadoutV2::GetStackKey(Same), WoRLoadoutV2::GetStackKey(Picked)}).Num(), 2);
		TestTrue(TEXT("a picked piece has no stack key"), WoRLoadoutV2::GetStackKey(Picked).IsEmpty());
	});
}

#endif
