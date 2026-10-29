// ClassLimitsSpec.cpp
// B-303 (D-110 unified loadout, step 3): one cost for every class and one class table. Refractor 3 / one item;
// Generic 6 / two weapons + one adapter; Resonator 5 / one weapon / any adapters. Checks Crown's worked examples:
// a Refractor's evolved weapon fits, a Generic's two evolved weapons fit (3 + 3), a Resonator over 5 is refused.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"

namespace
{
	const FCrystalId CLASS_LIMITS_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 CLASS_LIMITS_TEST_DURABILITY = 5;

	FRuntimeAttachedItem ClassLimitsTestGem()
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(CLASS_LIMITS_TEST_GEM, CLASS_LIMITS_TEST_DURABILITY);
		return Attached;
	}

	FRuntimeAttachedItem ClassLimitsTestEvolution()
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Evolution;
		Attached.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		return Attached;
	}

	FWeaponLoadoutEntry ClassLimitsTestWeapon(const FRuntimeAttachedItem &Attached = FRuntimeAttachedItem())
	{
		FWeaponLoadoutEntry Entry;
		Entry.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Entry.WeaponEntry.AttachedItem = Attached;
		return Entry;
	}

	FRingLoadoutEntry ClassLimitsTestAdapter(const FRuntimeAttachedItem &Attached = FRuntimeAttachedItem())
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.AttachedItem = Attached;
		return Entry;
	}

	bool ClassLimitsHasBudgetFinding(const TArray<FInvalidSlotFinding> &Findings)
	{
		return Findings.ContainsByPredicate([](const FInvalidSlotFinding &F) { return F.Reason.Contains(TEXT("exceeds budget")); });
	}
}

BEGIN_DEFINE_SPEC(FClassLimitsSpec, "WoR.Loadout.ClassLimits", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FClassLimitsSpec)

void FClassLimitsSpec::Define()
{
	It("holds Crown's table (D-110)", [this]()
	{
		const FLoadoutClassLimits Refractor = LoadoutClassTable::Get(ECharacterClass::Caster);
		const FLoadoutClassLimits Generic = LoadoutClassTable::Get(ECharacterClass::Generic);
		const FLoadoutClassLimits Resonator = LoadoutClassTable::Get(ECharacterClass::Resonator);
		TestTrue(TEXT("Refractor 3 / one item"), Refractor.Budget == 3 && Refractor.MaxItems == 1);
		TestTrue(TEXT("Generic 6 / two weapons / one adapter"), Generic.Budget == 6 && Generic.MaxWeapons == 2 && Generic.MaxAdapters == 1);
		TestTrue(TEXT("Resonator 5 / one weapon / any adapters"),
				 Resonator.Budget == LoadoutConstants::LOADOUT_TOTAL_BUDGET && Resonator.MaxWeapons == 1 &&
					 Resonator.MaxAdapters == LoadoutClassTable::NO_LIMIT);
	});

	It("costs each slot by state, the same for every class", [this]()
	{
		const auto CostOfWeapon = [](const FRuntimeAttachedItem &Attached)
		{ return FLoadoutRules::SlotCost(FLoadoutSlot::FromWeaponLoadoutEntry(ClassLimitsTestWeapon(Attached))); };
		const auto CostOfAdapter = [](const FRuntimeAttachedItem &Attached)
		{ return FLoadoutRules::SlotCost(FLoadoutSlot::FromRingLoadoutEntry(ClassLimitsTestAdapter(Attached))); };
		TestEqual(TEXT("bare weapon"), CostOfWeapon(FRuntimeAttachedItem()), InventoryConstants::WEAPON_BASE_SLOT_COST);
		TestEqual(TEXT("weapon with a gem"), CostOfWeapon(ClassLimitsTestGem()), InventoryConstants::WEAPON_CRYSTAL_SLOT_COST);
		TestEqual(TEXT("weapon with an evolution"), CostOfWeapon(ClassLimitsTestEvolution()), InventoryConstants::WEAPON_EVOLUTION_SLOT_COST);
		TestEqual(TEXT("adapter"), CostOfAdapter(ClassLimitsTestGem()), InventoryConstants::RING_BASE_SLOT_COST);
		TestEqual(TEXT("evolved adapter"), CostOfAdapter(ClassLimitsTestEvolution()), InventoryConstants::RING_EVOLUTION_SLOT_COST);
		TestEqual(TEXT("empty slot"), FLoadoutRules::SlotCost(FLoadoutSlot()), 0);
	});

	It("lets a Refractor carry an evolved weapon, and a Generic two (3 + 3)", [this]()
	{
		FCombatLoadout Refractor;
		Refractor.PrimarySlotType = EPrimarySlotType::Weapon;
		Refractor.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		TestEqual(TEXT("Refractor: no finding"), FLoadoutRules::CollectClassLimitFindings(Refractor, ECharacterClass::Caster).Num(), 0);

		FCombatLoadout Generic = Refractor;
		Generic.SecondarySlotType = ESecondarySlotType::Weapon;
		Generic.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		TestEqual(TEXT("Generic: no finding"), FLoadoutRules::CollectClassLimitFindings(Generic, ECharacterClass::Generic).Num(), 0);
	});

	It("refuses a Resonator over 5 with the clearable adapter-budget finding", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::Weapon;
		Resonator.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution()); // 3
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 5, fits
		TestFalse(TEXT("5 fits"), ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator)));
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestGem())); // 1: 6
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator);
		TestTrue(TEXT("6 is refused"), ClassLimitsHasBudgetFinding(Findings));
		TestTrue(TEXT("as RingOverflow, clearable"), Findings.ContainsByPredicate([](const FInvalidSlotFinding &F)
																				 { return F.SlotType == ELoadoutSlotType::RingOverflow && F.bClearable; }));
	});

	It("counts a Refractor's evolution seat as its one item", [this]()
	{
		FCombatLoadout Refractor;
		Refractor.PrimarySlotType = EPrimarySlotType::Evolution;
		Refractor.RingLoadout.Add(ClassLimitsTestAdapter()); // a shape a Refractor can't author today
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(Refractor, ECharacterClass::Caster);
		TestTrue(TEXT("two items refused"), Findings.ContainsByPredicate([](const FInvalidSlotFinding &F) { return F.Reason.Contains(TEXT("Too many items")); }));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
