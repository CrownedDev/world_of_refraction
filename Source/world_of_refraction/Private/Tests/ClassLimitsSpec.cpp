// ClassLimitsSpec.cpp
// B-303 (D-110 unified loadout, step 3): one cost for every class and one class table. Refractor 3 / one item;
// Generic 5 (Crown 2026-11-08, was 6) / two weapons + one adapter; Resonator 5 / one weapon / any adapters. Checks
// Crown's worked examples: a Refractor's evolved weapon fits; a Generic's evolved weapon fits with an evolved adapter
// (3 + 2) or a crystal weapon (3 + 2) but two evolved weapons (3 + 3) don't; a Resonator over 5 is refused.

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
		TestTrue(TEXT("Generic 5 / two weapons / one adapter"), Generic.Budget == 5 && Generic.MaxWeapons == 2 && Generic.MaxAdapters == 1);
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

	It("lets a Refractor carry an evolved weapon; a Generic one evolved weapon, not two (Crown 2026-11-08)", [this]()
	{
		FCombatLoadout Refractor;
		Refractor.PrimarySlotType = EPrimarySlotType::Weapon;
		Refractor.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		TestEqual(TEXT("Refractor: no finding"), FLoadoutRules::CollectClassLimitFindings(Refractor, ECharacterClass::Caster).Num(), 0);

		FCombatLoadout TwoEvolved = Refractor;
		TwoEvolved.SecondarySlotType = ESecondarySlotType::Weapon;
		TwoEvolved.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		TestTrue(TEXT("Generic: two evolved weapons (3 + 3) over 5"),
				 ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(TwoEvolved, ECharacterClass::Generic)));

		FCombatLoadout WithCrystalWeapon = Refractor;
		WithCrystalWeapon.SecondarySlotType = ESecondarySlotType::Weapon;
		WithCrystalWeapon.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestGem());
		TestEqual(TEXT("Generic: evolved weapon + crystal weapon (3 + 2) fits"),
				  FLoadoutRules::CollectClassLimitFindings(WithCrystalWeapon, ECharacterClass::Generic).Num(), 0);

		FCombatLoadout WithEvolvedAdapter;
		WithEvolvedAdapter.PrimarySlotType = EPrimarySlotType::Ring;
		WithEvolvedAdapter.PrimaryRing = ClassLimitsTestAdapter(ClassLimitsTestEvolution());
		WithEvolvedAdapter.SecondarySlotType = ESecondarySlotType::Weapon;
		WithEvolvedAdapter.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		TestEqual(TEXT("Generic: evolved adapter + evolved weapon (2 + 3) fits"),
				  FLoadoutRules::CollectClassLimitFindings(WithEvolvedAdapter, ECharacterClass::Generic).Num(), 0);
	});

	It("costs by frame + attachment (Q-357, Crown 2026-11-08)", [this]()
	{
		TestEqual(TEXT("bare weapon"), InventoryConstants::WEAPON_BASE_SLOT_COST, 1);
		TestEqual(TEXT("weapon + crystal"), InventoryConstants::WEAPON_CRYSTAL_SLOT_COST, 2);
		TestEqual(TEXT("weapon + evolution"), InventoryConstants::WEAPON_EVOLUTION_SLOT_COST, 3);
		TestEqual(TEXT("adapter + crystal"), InventoryConstants::RING_BASE_SLOT_COST, 1);
		TestEqual(TEXT("adapter + evolution"), InventoryConstants::RING_EVOLUTION_SLOT_COST, 2);
		TestEqual(TEXT("evolution alone"), LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST, 2);
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

	It("charges a primary weapon left with a None slot type, as the fight reads it (Q-303b)", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::None; // as AutoPopulateLoadout leaves it
		Resonator.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestGem()); // 2
		TestEqual(TEXT("the weapon is charged"), FLoadoutRules::NonAdapterCost(Resonator, ECharacterClass::Resonator),
				  InventoryConstants::WEAPON_CRYSTAL_SLOT_COST);
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 4
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 6
		TestTrue(TEXT("6 is refused"), ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator)));
	});

	It("counts only the gear the fight reads for the class", [this]()
	{
		// A loadout authored for a Generic, worn by a Resonator: its secondary weapon isn't read, so it costs nothing.
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution()); // 3
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution()); // ignored for a Resonator
		Loadout.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 5
		TestEqual(TEXT("weapons and seat"), FLoadoutRules::NonAdapterCost(Loadout, ECharacterClass::Resonator),
				  InventoryConstants::WEAPON_EVOLUTION_SLOT_COST);
		TestFalse(TEXT("no budget finding"), ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Loadout, ECharacterClass::Resonator)));
	});

	It("words a Resonator's budget finding as the old check did", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::Evolution; // seat 2
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 6
		const FString Expected = FString::Printf(TEXT("Loadout exceeds budget: primary (cost %d) + adapters (cost %d) = %d, max is %d"),
												 LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST, 2 * InventoryConstants::RING_EVOLUTION_SLOT_COST,
												 LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST + 2 * InventoryConstants::RING_EVOLUTION_SLOT_COST,
												 LoadoutConstants::LOADOUT_TOTAL_BUDGET);
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator);
		TestTrue(TEXT("same words: ") + Expected, Findings.ContainsByPredicate([&Expected](const FInvalidSlotFinding &F) { return F.Reason == Expected; }));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
