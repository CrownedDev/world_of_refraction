// ResistanceLinesSpec.cpp
// B-389 (D-168, Crown 2026-11-08): defence is resistance by damage type. Defence stays the All line (its own step); a
// Physical line cuts only hits that are not spells, a Spell line only spells; the matching scope line and the element
// line are one resistance under the ±50% cap; old data (no scope lines) gives the same numbers as before. Q-389 (Crown,
// 2026-11-09): the class table's Physical column joins the Physical line, so a Generic takes 15% less from a physical
// hit, a Resonator 15% more, a Refractor the same; spells are unaffected.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	constexpr int32 RESIST_LINES_TEST_BASE = 100;
	/** A gear line, percent (the struct's unit), and the same as a fraction. */
	constexpr float RESIST_LINES_TEST_PERCENT = 10.0f;
	constexpr float RESIST_LINES_TEST_FRACTION = 0.1f;
	constexpr float RESIST_LINES_TEST_ELEMENT = 0.45f;
	constexpr float RESIST_LINES_TEST_LINE = 0.2f;
	/** The class table's Physical column, as a fraction (ClassInnateResistanceTable::R_STRONG percent). */
	constexpr float RESIST_LINES_TEST_CLASS_PHYSICAL = ClassInnateResistanceTable::R_STRONG / ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;

	FDamageMathInputs PlainInputs()
	{
		FDamageMathInputs Inputs;
		Inputs.BaseDamage = RESIST_LINES_TEST_BASE;
		Inputs.AttackerStatMultiplier = 1.0f;
		Inputs.bCanCrit = false;
		return Inputs;
	}
}

BEGIN_DEFINE_SPEC(FResistanceLinesSpec, "WoR.Resistance.Lines", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	/** The defender's active loadout, or null (reported) when the test setup has none. */
	FCombatLoadout *ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			AddError(TEXT("no active loadout"));
			return nullptr;
		}
		return &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	/** One hit's final damage from Attacker on Defender: a spell (Fire) or an ability (no element), no crit. */
	int32 Hit(AActor *Attacker, AActor *Defender, bool bSpell)
	{
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!Calculator || !Attacker)
		{
			AddError(TEXT("no calculator or attacker"));
			return INDEX_NONE;
		}
		FDamageCalculationInput Input;
		Input.BaseDamage = RESIST_LINES_TEST_BASE;
		Input.ActionType = bSpell ? EActionType::Spell : EActionType::Ability;
		Input.Element = bSpell ? ESpellElement::Fire : ESpellElement::None;
		Input.OverrideCritChance = 0.0f;
		return Calculator->CalculateDamage(Attacker, Defender, Input).FinalDamage;
	}
END_DEFINE_SPEC(FResistanceLinesSpec)

void FResistanceLinesSpec::Define()
{
	Describe("the damage step", [this]()
	{
		It("adds the scope line to the element line under the one ±50% cap; none gives today's number", [this]()
		{
			FDamageMathInputs Line = PlainInputs();
			Line.ResistanceScopeLine = RESIST_LINES_TEST_LINE;
			TestEqual(TEXT("a scope line cuts"), WoRDamageMath::Compute(Line).ElementMultiplier, 1.0f - RESIST_LINES_TEST_LINE, KINDA_SMALL_NUMBER);
			FDamageMathInputs Both = Line;
			Both.ElementResistance = RESIST_LINES_TEST_ELEMENT;
			TestEqual(TEXT("summed with the element, capped at 50%"), WoRDamageMath::Compute(Both).ElementMultiplier,
					  1.0f - WoRDamageMath::ELEMENT_DAMAGE_RESIST_CAP, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("no scope line: no element cut"), WoRDamageMath::Compute(PlainInputs()).ElementMultiplier, 1.0f, KINDA_SMALL_NUMBER);
			TestTrue(TEXT("the breakdown names the lines"), WoRDamageMath::GetInputsString(Both).Contains(TEXT("Resistance All (Defence)")));
		});
	});

	Describe("through gear", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("Q-389: a Generic takes 15% less from a physical hit, a Resonator 15% more, a Refractor the same; spells unaffected", [this]()
		{
			auto SpawnOfClass = [this](ECharacterClass Class)
			{
				UCharacterData *Data = FWoRTestWorld::NewCharacterData();
				Data->CharacterClass = Class;
				return TestWorld->SpawnCombatant(Data);
			};
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Refractor = SpawnOfClass(ECharacterClass::Caster);
			AActor *Generic = SpawnOfClass(ECharacterClass::Generic);
			AActor *Resonator = SpawnOfClass(ECharacterClass::Resonator);
			TestEqual(TEXT("Refractor: no class Physical line"), WoRResistance::GetScopeLineResistance(Refractor, false), 0.0f, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("Generic: +15%"), WoRResistance::GetScopeLineResistance(Generic, false), RESIST_LINES_TEST_CLASS_PHYSICAL, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("Resonator: -15%"), WoRResistance::GetScopeLineResistance(Resonator, false), -RESIST_LINES_TEST_CLASS_PHYSICAL, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("a spell reads no class Physical line"), WoRResistance::GetScopeLineResistance(Generic, true), 0.0f, KINDA_SMALL_NUMBER);
			const int32 OnRefractor = Hit(Attacker, Refractor, /*bSpell=*/false);
			TestTrue(TEXT("a Generic takes less from a physical hit"), Hit(Attacker, Generic, false) < OnRefractor);
			TestTrue(TEXT("a Resonator takes more"), Hit(Attacker, Resonator, false) > OnRefractor);
		});

		It("a Physical line cuts an ability hit and leaves a spell alone; a Spell line the other way round", [this]()
		{
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData());
			if (!TestNotNull(TEXT("Loadout"), Loadout))
			{
				return;
			}
			AActor *Defender = Loadout->GetOwner();
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData()); // one attacker for every hit
			FCombatLoadout *Active = ActiveOf(Loadout);
			if (!Active)
			{
				return;
			}
			TestEqual(TEXT("old data: no Physical line"), WoRResistance::GetScopeLineResistance(Defender, false), 0.0f, KINDA_SMALL_NUMBER);
			const int32 AbilityBefore = Hit(Attacker, Defender, /*bSpell=*/false);
			const int32 SpellBefore = Hit(Attacker, Defender, /*bSpell=*/true);

			UApparelData *Coat = NewObject<UApparelData>(GetTransientPackage());
			Coat->Tier = EItemTier::F_Tier;
			Coat->BaseResistance.PhysicalDamage = RESIST_LINES_TEST_PERCENT;
			TestTrue(TEXT("worn"), ApparelRules::AddApparel(*Active, FApparelEntry::CreateFromApparel(Coat)));
			TestEqual(TEXT("the Physical line, as a fraction"), WoRResistance::GetScopeLineResistance(Defender, false),
					  RESIST_LINES_TEST_FRACTION, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("no Spell line"), WoRResistance::GetScopeLineResistance(Defender, true), 0.0f, KINDA_SMALL_NUMBER);
			TestTrue(TEXT("an ability hit is cut"), Hit(Attacker, Defender, false) < AbilityBefore);
			TestEqual(TEXT("a spell is not"), Hit(Attacker, Defender, true), SpellBefore);

			Active->Apparel.Reset();
			Coat->BaseResistance.PhysicalDamage = 0.0f;
			Coat->BaseResistance.SpellDamage = RESIST_LINES_TEST_PERCENT;
			ApparelRules::AddApparel(*Active, FApparelEntry::CreateFromApparel(Coat));
			TestTrue(TEXT("a Spell line cuts a spell"), Hit(Attacker, Defender, true) < SpellBefore);
			TestEqual(TEXT("and not an ability hit"), Hit(Attacker, Defender, false), AbilityBefore);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
