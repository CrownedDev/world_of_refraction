// LoadoutBudgetSpec.cpp
// B-285: pins the loadout budgets to the rules page (docs/Rules/01-classes-and-loadouts.md).
// CL-10: a Resonator's primary cost + ring costs must be <= 5; bare weapon 1, weapon with a crystal 2,
//        weapon with an evolution 3, evolution primary 2, ring 1, evolved ring 2.
// CL-11: no ring-count cap; the budget is the only limit.
// CL-12: the budget is a Resonator rule only (Generic and Refractor are never checked).
// CL-34: innate spell cost = tier (F 1 ... S 7), total <= 24, 6 per school, -1 per pillar at
//        Mind 4 / Body 7 / Spirit 5, never below 1.
// CL-35: a Broken Darkness Refractor's budget is 48.
// Each constant is checked against the page's literal, so a changed constant fails here.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/SpellPoolConstants.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Inventory/InventoryConstants.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	// Rules-page numbers (CL-10, CL-34, CL-35).
	constexpr int32 LOADOUT_BUDGET_RULE_TOTAL = 5;
	constexpr int32 LOADOUT_BUDGET_RULE_BARE_WEAPON = 1;
	constexpr int32 LOADOUT_BUDGET_RULE_CRYSTAL_WEAPON = 2;
	constexpr int32 LOADOUT_BUDGET_RULE_EVOLVED_WEAPON = 3;
	constexpr int32 LOADOUT_BUDGET_RULE_EVOLUTION_PRIMARY = 2;
	constexpr int32 LOADOUT_BUDGET_RULE_RING = 1;
	constexpr int32 LOADOUT_BUDGET_RULE_EVOLVED_RING = 2;
	constexpr int32 LOADOUT_BUDGET_RULE_INNATE = 24;
	constexpr int32 LOADOUT_BUDGET_RULE_BROKEN_DARKNESS = 48;
	constexpr int32 LOADOUT_BUDGET_RULE_PER_SCHOOL = 6;
	constexpr int32 LOADOUT_BUDGET_RULE_MIND_THRESHOLD = 4;
	constexpr int32 LOADOUT_BUDGET_RULE_BODY_THRESHOLD = 7;
	constexpr int32 LOADOUT_BUDGET_RULE_SPIRIT_THRESHOLD = 5;
	constexpr int32 LOADOUT_BUDGET_RULE_COST_FLOOR = 1;
	constexpr int32 LOADOUT_BUDGET_RULE_TIER_COSTS[] = {1, 2, 3, 4, 5, 6, 7}; // F, E, D, C, B, A, S

	/** A bare weapon (1) fills the budget with this many plain rings (1 each). */
	constexpr int32 LOADOUT_BUDGET_TEST_RINGS_TO_FILL = 4;

	/** Far more rings than any count cap would allow, all on a Generic / Refractor (CL-12). */
	constexpr int32 LOADOUT_BUDGET_TEST_MANY_RINGS = 8;

	/** A-tier spells (6 points each) that exactly fill the innate budget of 24. */
	constexpr int32 LOADOUT_BUDGET_TEST_A_TIER_SPELLS = 4;

	enum class ELoadoutBudgetTestWeapon : uint8
	{
		Bare,
		Crystal,
		Evolution
	};

	FCombatLoadout LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon Kind)
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		FRuntimeAttachedItem Attached;
		if (Kind == ELoadoutBudgetTestWeapon::Crystal)
		{
			Attached.Kind = EAttachedItemKind::Crystal;
		}
		else if (Kind == ELoadoutBudgetTestWeapon::Evolution)
		{
			Attached.Kind = EAttachedItemKind::Evolution;
			Attached.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		}
		Loadout.PrimaryWeapon.WeaponEntry.AttachedItem = Attached;
		return Loadout;
	}

	FRingLoadoutEntry LoadoutBudgetRing(bool bEvolved)
	{
		FRingLoadoutEntry Ring;
		Ring.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		if (bEvolved)
		{
			Ring.RingEntry.AttachedItem.Kind = EAttachedItemKind::Evolution;
			Ring.RingEntry.AttachedItem.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		}
		return Ring;
	}

	void LoadoutBudgetAddRings(FCombatLoadout &Loadout, int32 Count, bool bEvolved = false)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			Loadout.RingLoadout.Add(LoadoutBudgetRing(bEvolved));
		}
	}

	bool LoadoutBudgetOverBudget(const FCombatLoadout &Loadout, ECharacterClass CharacterClass)
	{
		FLoadoutRuleContext Context;
		Context.CharacterClass = CharacterClass;
		return FLoadoutRules::CollectFindings(Loadout, Context).ContainsByPredicate(
			[](const FInvalidSlotFinding &Finding) { return Finding.SlotType == ELoadoutSlotType::RingOverflow; });
	}

	USpellData *LoadoutBudgetSpell(EItemTier Tier)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Budget Test Spell");
		Spell->Element = ESpellElement::Fire;
		Spell->Tier = Tier;
		return Spell;
	}

	bool LoadoutBudgetInnateOverBudget(const FCombatLoadout &Loadout, int32 Discount)
	{
		FLoadoutRuleContext Context;
		Context.CharacterClass = ECharacterClass::Caster;
		Context.SpellDiscount = Discount;
		return FLoadoutRules::CollectFindings(Loadout, Context).ContainsByPredicate(
			[](const FInvalidSlotFinding &Finding) { return Finding.Reason.Contains(TEXT("Innate spell budget exceeded")); });
	}
}

BEGIN_DEFINE_SPEC(FLoadoutBudgetSpec, "WoR.Loadout.Budget",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutBudgetSpec)

void FLoadoutBudgetSpec::Define()
{
	Describe("the Resonator budget", [this]()
	{
		It("is 5, with the slot costs on the rules page (CL-10)", [this]()
		{
			TestEqual(TEXT("LOADOUT_TOTAL_BUDGET"), LoadoutConstants::LOADOUT_TOTAL_BUDGET, LOADOUT_BUDGET_RULE_TOTAL);
			TestEqual(TEXT("WEAPON_BASE_SLOT_COST"), InventoryConstants::WEAPON_BASE_SLOT_COST, LOADOUT_BUDGET_RULE_BARE_WEAPON);
			TestEqual(TEXT("WEAPON_CRYSTAL_SLOT_COST"), InventoryConstants::WEAPON_CRYSTAL_SLOT_COST, LOADOUT_BUDGET_RULE_CRYSTAL_WEAPON);
			TestEqual(TEXT("WEAPON_EVOLUTION_SLOT_COST"), InventoryConstants::WEAPON_EVOLUTION_SLOT_COST, LOADOUT_BUDGET_RULE_EVOLVED_WEAPON);
			TestEqual(TEXT("EVOLUTION_PRIMARY_SLOT_COST"), LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST, LOADOUT_BUDGET_RULE_EVOLUTION_PRIMARY);
			TestEqual(TEXT("RING_BASE_SLOT_COST"), InventoryConstants::RING_BASE_SLOT_COST, LOADOUT_BUDGET_RULE_RING);
			TestEqual(TEXT("RING_EVOLUTION_SLOT_COST"), InventoryConstants::RING_EVOLUTION_SLOT_COST, LOADOUT_BUDGET_RULE_EVOLVED_RING);
		});

		It("costs each gear state as the rules page says (CL-10)", [this]()
		{
			TestEqual(TEXT("bare weapon"), LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Bare).PrimaryWeapon.WeaponEntry.GetSlotCost(),
					  LOADOUT_BUDGET_RULE_BARE_WEAPON);
			TestEqual(TEXT("weapon with a crystal"), LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Crystal).PrimaryWeapon.WeaponEntry.GetSlotCost(),
					  LOADOUT_BUDGET_RULE_CRYSTAL_WEAPON);
			TestEqual(TEXT("weapon with an evolution"), LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Evolution).PrimaryWeapon.WeaponEntry.GetSlotCost(),
					  LOADOUT_BUDGET_RULE_EVOLVED_WEAPON);
			TestEqual(TEXT("ring"), InventoryConstants::GetRingSlotCost(LoadoutBudgetRing(false).IsEvolved()), LOADOUT_BUDGET_RULE_RING);
			TestEqual(TEXT("evolved ring"), InventoryConstants::GetRingSlotCost(LoadoutBudgetRing(true).IsEvolved()), LOADOUT_BUDGET_RULE_EVOLVED_RING);
		});

		It("passes a bare weapon and 4 rings (1 + 4 = 5) (CL-10)", [this]()
		{
			FCombatLoadout Loadout = LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Bare);
			LoadoutBudgetAddRings(Loadout, LOADOUT_BUDGET_TEST_RINGS_TO_FILL);
			TestFalse(TEXT("at the budget: passes"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
		});

		It("refuses a weapon with a crystal and 4 rings (2 + 4 = 6) (CL-10)", [this]()
		{
			FCombatLoadout Loadout = LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Crystal);
			LoadoutBudgetAddRings(Loadout, LOADOUT_BUDGET_TEST_RINGS_TO_FILL);
			TestTrue(TEXT("one over: refused"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
		});

		It("costs an evolution primary 2 (CL-10)", [this]()
		{
			FCombatLoadout Loadout;
			Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
			LoadoutBudgetAddRings(Loadout, LOADOUT_BUDGET_RULE_TOTAL - LOADOUT_BUDGET_RULE_EVOLUTION_PRIMARY);
			TestFalse(TEXT("2 + 3 rings: passes"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
			LoadoutBudgetAddRings(Loadout, 1);
			TestTrue(TEXT("2 + 4 rings: refused"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
		});

		It("costs an evolved ring 2 (CL-10)", [this]()
		{
			// Bare weapon 1 + two evolved rings 4 = 5.
			FCombatLoadout Loadout = LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Bare);
			LoadoutBudgetAddRings(Loadout, 2, /*bEvolved=*/true);
			TestFalse(TEXT("1 + 2 + 2: passes"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
			LoadoutBudgetAddRings(Loadout, 1);
			TestTrue(TEXT("1 + 2 + 2 + 1: refused"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
		});

		It("has no ring-count cap: 5 plain rings with no primary fit (CL-11)", [this]()
		{
			FCombatLoadout Loadout;
			LoadoutBudgetAddRings(Loadout, LOADOUT_BUDGET_RULE_TOTAL);
			TestFalse(TEXT("5 rings at 1 each: passes"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
			LoadoutBudgetAddRings(Loadout, 1);
			TestTrue(TEXT("a 6th ring is refused by the budget, not a count"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
		});

		It("is never checked for a Generic or a Refractor (CL-12)", [this]()
		{
			FCombatLoadout Loadout = LoadoutBudgetWeaponPrimary(ELoadoutBudgetTestWeapon::Evolution);
			LoadoutBudgetAddRings(Loadout, LOADOUT_BUDGET_TEST_MANY_RINGS, /*bEvolved=*/true);
			TestTrue(TEXT("the same loadout is over budget for a Resonator"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Resonator));
			TestFalse(TEXT("Generic: not checked"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Generic));
			TestFalse(TEXT("Refractor: not checked"), LoadoutBudgetOverBudget(Loadout, ECharacterClass::Caster));
		});
	});

	Describe("the innate spell budget", [this]()
	{
		It("is 24, and 48 for Broken Darkness (CL-34, CL-35)", [this]()
		{
			TestEqual(TEXT("INNATE_SPELL_BUDGET"), SpellPoolConstants::INNATE_SPELL_BUDGET, LOADOUT_BUDGET_RULE_INNATE);
			TestEqual(TEXT("BD_SPELL_BUDGET"), SpellPoolConstants::BD_SPELL_BUDGET, LOADOUT_BUDGET_RULE_BROKEN_DARKNESS);
			TestEqual(TEXT("MAX_EQUIPPED_SLOT_POOL"), SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL, LOADOUT_BUDGET_RULE_PER_SCHOOL);
		});

		It("costs a spell its tier, F 1 to S 7 (CL-34)", [this]()
		{
			const EItemTier Tiers[] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier, EItemTier::C_Tier,
									   EItemTier::B_Tier, EItemTier::A_Tier, EItemTier::S_Tier};
			static_assert(UE_ARRAY_COUNT(Tiers) == UE_ARRAY_COUNT(LOADOUT_BUDGET_RULE_TIER_COSTS), "one rule cost per tier");
			for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(Tiers)); ++i)
			{
				TestEqual(FString::Printf(TEXT("tier %d cost"), i), SpellPoolConstants::SpellSlotBaseCost(Tiers[i]),
						  LOADOUT_BUDGET_RULE_TIER_COSTS[i]);
			}
		});

		It("cuts 1 per pillar at Mind 4 / Body 7 / Spirit 5, never below 1 (CL-34)", [this]()
		{
			TestEqual(TEXT("Mind threshold"), SpellPoolConstants::SPELL_DISCOUNT_MIND_THRESHOLD, LOADOUT_BUDGET_RULE_MIND_THRESHOLD);
			TestEqual(TEXT("Body threshold"), SpellPoolConstants::SPELL_DISCOUNT_BODY_THRESHOLD, LOADOUT_BUDGET_RULE_BODY_THRESHOLD);
			TestEqual(TEXT("Spirit threshold"), SpellPoolConstants::SPELL_DISCOUNT_SPIRIT_THRESHOLD, LOADOUT_BUDGET_RULE_SPIRIT_THRESHOLD);
			TestEqual(TEXT("cost floor"), SpellPoolConstants::SPELL_SLOT_COST_FLOOR, LOADOUT_BUDGET_RULE_COST_FLOOR);

			TestEqual(TEXT("each pillar one below its bar: no discount"),
					  SpellPoolConstants::SpellSlotDiscount(LOADOUT_BUDGET_RULE_MIND_THRESHOLD - 1, LOADOUT_BUDGET_RULE_BODY_THRESHOLD - 1,
															LOADOUT_BUDGET_RULE_SPIRIT_THRESHOLD - 1),
					  0);
			TestEqual(TEXT("Mind at its bar only: -1"), SpellPoolConstants::SpellSlotDiscount(LOADOUT_BUDGET_RULE_MIND_THRESHOLD, 0, 0), 1);
			TestEqual(TEXT("Body at its bar only: -1"), SpellPoolConstants::SpellSlotDiscount(0, LOADOUT_BUDGET_RULE_BODY_THRESHOLD, 0), 1);
			TestEqual(TEXT("Spirit at its bar only: -1"), SpellPoolConstants::SpellSlotDiscount(0, 0, LOADOUT_BUDGET_RULE_SPIRIT_THRESHOLD), 1);
			const int32 FullDiscount = SpellPoolConstants::SpellSlotDiscount(
				LOADOUT_BUDGET_RULE_MIND_THRESHOLD, LOADOUT_BUDGET_RULE_BODY_THRESHOLD, LOADOUT_BUDGET_RULE_SPIRIT_THRESHOLD);
			TestEqual(TEXT("all three at their bars: -3"), FullDiscount, 3);
			TestEqual(TEXT("an F spell at -3 stays at the floor"), SpellPoolConstants::SpellSlotEffectiveCost(EItemTier::F_Tier, FullDiscount),
					  LOADOUT_BUDGET_RULE_COST_FLOOR);
			TestEqual(TEXT("an S spell at -3 costs 4"), SpellPoolConstants::SpellSlotEffectiveCost(EItemTier::S_Tier, FullDiscount), 4);
		});

		It("passes a Refractor at 24 points and refuses 25 (CL-34)", [this]()
		{
			FCombatLoadout Loadout;
			for (int32 i = 0; i < LOADOUT_BUDGET_TEST_A_TIER_SPELLS; ++i)
			{
				Loadout.InnateSpells.Add(LoadoutBudgetSpell(EItemTier::A_Tier));
			}
			TestFalse(TEXT("4 A-tier = 24: passes"), LoadoutBudgetInnateOverBudget(Loadout, 0));
			Loadout.InnateSpells.Add(LoadoutBudgetSpell(EItemTier::F_Tier));
			TestTrue(TEXT("+ 1 F-tier = 25: refused"), LoadoutBudgetInnateOverBudget(Loadout, 0));
			TestFalse(TEXT("the same 25 with a -1 discount (5 + 5 + 5 + 5 + 1 = 21): passes"), LoadoutBudgetInnateOverBudget(Loadout, 1));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
