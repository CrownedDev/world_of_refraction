// ClassLimitsSpec.cpp
// B-303 (D-110 unified loadout): one cost for every class and one class table. B-472 (D-206, Crown 2026-11-14): the
// gear is free, only what is in it costs (crystal 1, stone 1, evolution 2, an evolution alone 2). Refractor 2 / one
// item, Generic 5 / three items (two weapons + one adapter, no "one evolved weapon" limit), Resonator 5 / one weapon /
// at most 5 adapters. Checks Crown's worked examples: a Refractor's evolved weapon, evolved adapter, evolution alone or
// weapon with a crystal and a stone fit, evolved gear plus a stone does not (cleared by dropping the stone); a
// Generic's two evolved weapons (2 + 2) fit; a Resonator over 5 is refused.

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

	FRuntimeAttachedItem ClassLimitsTestStone()
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::AugmentStone;
		Attached.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::DamageStone, EItemTier::F_Tier), 0);
		return Attached;
	}

	FRuntimeAttachedItem ClassLimitsTestEvolution()
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Evolution;
		Attached.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		return Attached;
	}

	FWeaponLoadoutEntry ClassLimitsTestWeapon(const FRuntimeAttachedItem &Attached = FRuntimeAttachedItem(),
											  const FRuntimeAttachedItem &Stone = FRuntimeAttachedItem())
	{
		FWeaponLoadoutEntry Entry;
		Entry.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Entry.WeaponEntry.AttachedItem = Attached;
		Entry.WeaponEntry.StoneSocket = Stone;
		return Entry;
	}

	FRingLoadoutEntry ClassLimitsTestAdapter(const FRuntimeAttachedItem &Attached = FRuntimeAttachedItem(),
											 const FRuntimeAttachedItem &Stone = FRuntimeAttachedItem())
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.AttachedItem = Attached;
		Entry.RingEntry.StoneSocket = Stone;
		return Entry;
	}

	FCombatLoadout ClassLimitsRefractorWeapon(const FRuntimeAttachedItem &Attached, const FRuntimeAttachedItem &Stone = FRuntimeAttachedItem())
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = ClassLimitsTestWeapon(Attached, Stone);
		return Loadout;
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
	It("holds Crown's table (D-206)", [this]()
	{
		const FLoadoutClassLimits Refractor = LoadoutClassTable::Get(ECharacterClass::Caster);
		const FLoadoutClassLimits Generic = LoadoutClassTable::Get(ECharacterClass::Generic);
		const FLoadoutClassLimits Resonator = LoadoutClassTable::Get(ECharacterClass::Resonator);
		TestTrue(TEXT("Refractor 2 / one item"), Refractor.Budget == 2 && Refractor.MaxItems == 1);
		TestTrue(TEXT("Generic 5 / three items / two weapons / one adapter"),
				 Generic.Budget == 5 && Generic.MaxItems == 3 && Generic.MaxWeapons == 2 && Generic.MaxAdapters == 1);
		TestTrue(TEXT("Resonator 5 / one weapon / at most 5 adapters"),
				 Resonator.Budget == LoadoutConstants::LOADOUT_TOTAL_BUDGET && Resonator.MaxWeapons == 1 && Resonator.MaxAdapters == 5);
	});

	It("costs only what is inside, the same for every class (D-206)", [this]()
	{
		TestEqual(TEXT("the gear itself"), LoadoutConstants::LOADOUT_GEAR_COST, 0);
		TestEqual(TEXT("a crystal"), LoadoutConstants::LOADOUT_CRYSTAL_COST, 1);
		TestEqual(TEXT("a stone"), LoadoutConstants::LOADOUT_STONE_COST, 1);
		TestEqual(TEXT("an evolution on gear"), LoadoutConstants::LOADOUT_EVOLUTION_COST, 2);
		TestEqual(TEXT("an evolution alone"), LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST, 2);

		const auto CostOfWeapon = [](const FRuntimeAttachedItem &Attached, const FRuntimeAttachedItem &Stone)
		{ return FLoadoutRules::SlotCost(FLoadoutSlot::FromWeaponLoadoutEntry(ClassLimitsTestWeapon(Attached, Stone))); };
		const auto CostOfAdapter = [](const FRuntimeAttachedItem &Attached, const FRuntimeAttachedItem &Stone)
		{ return FLoadoutRules::SlotCost(FLoadoutSlot::FromRingLoadoutEntry(ClassLimitsTestAdapter(Attached, Stone))); };
		const FRuntimeAttachedItem None;
		TestEqual(TEXT("bare weapon: free"), CostOfWeapon(None, None), 0);
		TestEqual(TEXT("weapon with a crystal"), CostOfWeapon(ClassLimitsTestGem(), None), 1);
		TestEqual(TEXT("weapon with a crystal and a stone"), CostOfWeapon(ClassLimitsTestGem(), ClassLimitsTestStone()), 2);
		TestEqual(TEXT("weapon with an evolution"), CostOfWeapon(ClassLimitsTestEvolution(), None), 2);
		TestEqual(TEXT("weapon with an evolution and a stone"), CostOfWeapon(ClassLimitsTestEvolution(), ClassLimitsTestStone()), 3);
		TestEqual(TEXT("bare adapter: free"), CostOfAdapter(None, None), 0);
		TestEqual(TEXT("adapter with a crystal: as a weapon with one"), CostOfAdapter(ClassLimitsTestGem(), None), CostOfWeapon(ClassLimitsTestGem(), None));
		TestEqual(TEXT("evolved adapter"), CostOfAdapter(ClassLimitsTestEvolution(), None), 2);
		TestEqual(TEXT("empty slot"), FLoadoutRules::SlotCost(FLoadoutSlot()), 0);
	});

	It("keeps the pouch capacity costs as they were (D-206 notes)", [this]()
	{
		TestEqual(TEXT("bare weapon"), InventoryConstants::WEAPON_BASE_SLOT_COST, 1);
		TestEqual(TEXT("weapon + crystal"), InventoryConstants::WEAPON_CRYSTAL_SLOT_COST, 2);
		TestEqual(TEXT("weapon + evolution"), InventoryConstants::WEAPON_EVOLUTION_SLOT_COST, 3);
		TestEqual(TEXT("adapter"), InventoryConstants::RING_BASE_SLOT_COST, 1);
		TestEqual(TEXT("evolved adapter"), InventoryConstants::RING_EVOLUTION_SLOT_COST, 2);
	});

	It("fits a Refractor's one item at 2, refuses evolved gear plus a stone and clears it by the stone (D-206)", [this]()
	{
		const auto Findings = [](const FCombatLoadout &Loadout) { return FLoadoutRules::CollectClassLimitFindings(Loadout, ECharacterClass::Caster); };
		TestEqual(TEXT("an evolved weapon fits"), Findings(ClassLimitsRefractorWeapon(ClassLimitsTestEvolution())).Num(), 0);
		TestEqual(TEXT("a weapon with a crystal and a stone fits"), Findings(ClassLimitsRefractorWeapon(ClassLimitsTestGem(), ClassLimitsTestStone())).Num(), 0);

		FCombatLoadout EvolvedAdapter;
		EvolvedAdapter.PrimarySlotType = EPrimarySlotType::Ring;
		EvolvedAdapter.PrimaryRing = ClassLimitsTestAdapter(ClassLimitsTestEvolution());
		TestEqual(TEXT("an evolved adapter fits"), Findings(EvolvedAdapter).Num(), 0);

		FCombatLoadout EvolutionAlone;
		EvolutionAlone.PrimarySlotType = EPrimarySlotType::Evolution;
		TestEqual(TEXT("an evolution alone fits"), Findings(EvolutionAlone).Num(), 0);

		const TArray<FInvalidSlotFinding> Over = Findings(ClassLimitsRefractorWeapon(ClassLimitsTestEvolution(), ClassLimitsTestStone()));
		TestTrue(TEXT("evolved weapon + stone (3) is over 2"), ClassLimitsHasBudgetFinding(Over));
		TestTrue(TEXT("... cleared by dropping the stone"), Over.ContainsByPredicate([](const FInvalidSlotFinding &F)
																					  { return F.bClearable && F.SlotType == ELoadoutSlotType::PrimaryStone; }));

		FCombatLoadout EvolvedAdapterAndStone = EvolvedAdapter;
		EvolvedAdapterAndStone.PrimaryRing.RingEntry.StoneSocket = ClassLimitsTestStone();
		TestTrue(TEXT("evolved adapter + stone is over 2, cleared by the stone"),
				 Findings(EvolvedAdapterAndStone).ContainsByPredicate([](const FInvalidSlotFinding &F)
																	  { return F.bClearable && F.SlotType == ELoadoutSlotType::PrimaryStone; }));
	});

	It("lets a Generic carry two evolved weapons (2 + 2, D-206: no one-evolved-weapon limit)", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution());
		// The third item, an adapter beside two weapons, is a shape B-477 adds (the fight doesn't read one yet).
		TestFalse(TEXT("no budget finding for the two evolved weapons"),
				  ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Loadout, ECharacterClass::Generic)));

		FCombatLoadout OverFive = Loadout;
		OverFive.PrimaryWeapon.WeaponEntry.StoneSocket = ClassLimitsTestStone();
		OverFive.SecondaryWeapon.WeaponEntry.StoneSocket = ClassLimitsTestStone(); // 3 + 3 = 6
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(OverFive, ECharacterClass::Generic);
		TestTrue(TEXT("two evolved weapons with stones (3 + 3) are over 5"), ClassLimitsHasBudgetFinding(Findings));
		TestTrue(TEXT("... cleared by dropping the secondary weapon"), Findings.ContainsByPredicate([](const FInvalidSlotFinding &F)
																									 { return F.bClearable && F.SlotType == ELoadoutSlotType::SecondaryWeapon; }));
	});

	It("refuses a Resonator over 5 with the clearable adapter-budget finding", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::Weapon;
		Resonator.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution(), ClassLimitsTestStone()); // 3
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 5, fits
		TestFalse(TEXT("5 fits"), ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator)));
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestGem())); // 1: 6
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator);
		TestTrue(TEXT("6 is refused"), ClassLimitsHasBudgetFinding(Findings));
		TestTrue(TEXT("as RingOverflow, clearable"), Findings.ContainsByPredicate([](const FInvalidSlotFinding &F)
																				 { return F.SlotType == ELoadoutSlotType::RingOverflow && F.bClearable; }));
	});

	It("caps a Resonator at 5 adapters even when they cost nothing (D-206)", [this]()
	{
		FCombatLoadout Resonator;
		for (int32 Index = 0; Index < LoadoutConstants::MAX_RESONATOR_ADAPTERS; ++Index)
		{
			Resonator.RingLoadout.Add(ClassLimitsTestAdapter());
		}
		TestEqual(TEXT("5 bare adapters: no finding"), FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator).Num(), 0);
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter());
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator);
		TestFalse(TEXT("a 6th is not a budget finding"), ClassLimitsHasBudgetFinding(Findings));
		TestTrue(TEXT("a 6th is too many, cleared like the budget (RingOverflow)"), Findings.ContainsByPredicate([](const FInvalidSlotFinding &F)
																												 { return F.SlotType == ELoadoutSlotType::RingOverflow && F.bClearable && F.Reason.Contains(TEXT("Too many adapters")); }));
	});

	It("charges a primary weapon left with a None slot type, as the fight reads it (Q-303b)", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::None; // as AutoPopulateLoadout leaves it
		Resonator.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestGem()); // 1
		TestEqual(TEXT("the weapon is charged"), FLoadoutRules::NonAdapterCost(Resonator, ECharacterClass::Resonator), LoadoutConstants::LOADOUT_CRYSTAL_COST);
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 3
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 5
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestGem())); // 1: 6
		TestTrue(TEXT("6 is refused"), ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator)));
	});

	It("counts only the gear the fight reads for the class", [this]()
	{
		// A loadout authored for a Generic, worn by a Resonator: its secondary weapon isn't read, so it costs nothing.
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution(), ClassLimitsTestStone()); // 3
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = ClassLimitsTestWeapon(ClassLimitsTestEvolution()); // ignored for a Resonator
		Loadout.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 5
		TestEqual(TEXT("weapons and seat"), FLoadoutRules::NonAdapterCost(Loadout, ECharacterClass::Resonator),
				  LoadoutConstants::LOADOUT_EVOLUTION_COST + LoadoutConstants::LOADOUT_STONE_COST);
		TestFalse(TEXT("no budget finding"), ClassLimitsHasBudgetFinding(FLoadoutRules::CollectClassLimitFindings(Loadout, ECharacterClass::Resonator)));
	});

	It("words a Resonator's budget finding as the old check did", [this]()
	{
		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::Evolution; // seat 2
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2
		Resonator.RingLoadout.Add(ClassLimitsTestAdapter(ClassLimitsTestEvolution())); // 2: 6
		const int32 AdapterCost = 2 * LoadoutConstants::LOADOUT_EVOLUTION_COST;
		const FString Expected = FString::Printf(TEXT("Loadout exceeds budget: primary (cost %d) + adapters (cost %d) = %d, max is %d"),
												 LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST, AdapterCost,
												 LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST + AdapterCost, LoadoutConstants::LOADOUT_TOTAL_BUDGET);
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectClassLimitFindings(Resonator, ECharacterClass::Resonator);
		TestTrue(TEXT("same words: ") + Expected, Findings.ContainsByPredicate([&Expected](const FInvalidSlotFinding &F) { return F.Reason == Expected; }));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
