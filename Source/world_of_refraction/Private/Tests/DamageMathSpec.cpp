// DamageMathSpec.cpp
// B-219: the damage maths core (WoRDamageMath::Compute) on hand-made inputs, and
// UDamageCalculator::CalculateDamage equal to Compute over its gathered inputs.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/DamageMath.h"
#include "Debug/Combat/DamageMathDebug.h"

namespace
{
	constexpr int32 DAMAGE_MATH_TEST_BASE = 100;
	constexpr float DAMAGE_MATH_TEST_STAT = 1.2f;
	constexpr float DAMAGE_MATH_TEST_DEFENSE = 0.25f;
	constexpr float DAMAGE_MATH_TEST_CRIT_MULT = 1.5f;
	constexpr float DAMAGE_MATH_TEST_CRIT_CHANCE = 0.3f;
	constexpr float DAMAGE_MATH_TEST_ROLL_BELOW = 0.1f;
	constexpr float DAMAGE_MATH_TEST_ROLL_ABOVE = 0.9f;
	constexpr float DAMAGE_MATH_TEST_OVER_CAP_STAT = 2.0f;
	constexpr int32 DAMAGE_MATH_TEST_DEFENSE_POINTS = 50;

	FDamageMathInputs PlainInputs()
	{
		FDamageMathInputs Inputs;
		Inputs.BaseDamage = DAMAGE_MATH_TEST_BASE;
		Inputs.AttackerStatMultiplier = DAMAGE_MATH_TEST_STAT;
		Inputs.bCanCrit = false;
		return Inputs;
	}
}

BEGIN_DEFINE_SPEC(FDamageMathSpec, "WoR.Damage.MathCore",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FDamageMathSpec)

void FDamageMathSpec::Define()
{
	Describe("Compute", [this]()
	{
		It("scales the base by the attacker multiplier", [this]()
		{
			const FDamageCalculationResult Result = WoRDamageMath::Compute(PlainInputs());
			TestEqual(TEXT("attacker multiplier"), Result.AttackerDamageMultiplier, DAMAGE_MATH_TEST_STAT);
			TestEqual(TEXT("final"), Result.FinalDamage, FMath::RoundToInt(DAMAGE_MATH_TEST_BASE * DAMAGE_MATH_TEST_STAT));
		});

		It("applies defense as a percent reduction and records what it blocked", [this]()
		{
			FDamageMathInputs Inputs = PlainInputs();
			Inputs.bApplyDefense = true;
			Inputs.DefenderDefense = DAMAGE_MATH_TEST_DEFENSE;
			const FDamageCalculationResult Result = WoRDamageMath::Compute(Inputs);
			const int32 Before = FMath::RoundToInt(DAMAGE_MATH_TEST_BASE * DAMAGE_MATH_TEST_STAT);
			const int32 After = FMath::RoundToInt(DAMAGE_MATH_TEST_BASE * DAMAGE_MATH_TEST_STAT * (1.0f - DAMAGE_MATH_TEST_DEFENSE));
			TestEqual(TEXT("before defense"), Result.DamageBeforeDefense, Before);
			TestEqual(TEXT("final"), Result.FinalDamage, After);
			TestEqual(TEXT("blocked"), Result.DamageBlockedByDefense, Before - After);
		});

		It("crits when the roll is under the chance or the crit is forced", [this]()
		{
			FDamageMathInputs Inputs = PlainInputs();
			Inputs.bCanCrit = true;
			Inputs.CritChance = DAMAGE_MATH_TEST_CRIT_CHANCE;
			Inputs.CritDamageMultiplier = DAMAGE_MATH_TEST_CRIT_MULT;

			Inputs.CritRoll = DAMAGE_MATH_TEST_ROLL_ABOVE;
			TestFalse(TEXT("roll above: no crit"), WoRDamageMath::Compute(Inputs).bWasCritical);

			Inputs.CritRoll = DAMAGE_MATH_TEST_ROLL_BELOW;
			const FDamageCalculationResult Crit = WoRDamageMath::Compute(Inputs);
			TestTrue(TEXT("roll below: crit"), Crit.bWasCritical);
			TestEqual(TEXT("crit damage"), Crit.FinalDamage,
					  FMath::RoundToInt(DAMAGE_MATH_TEST_BASE * DAMAGE_MATH_TEST_STAT * DAMAGE_MATH_TEST_CRIT_MULT));

			Inputs.CritRoll = DAMAGE_MATH_TEST_ROLL_ABOVE;
			Inputs.bForceCrit = true;
			TestTrue(TEXT("forced: crit whatever the roll"), WoRDamageMath::Compute(Inputs).bWasCritical);
		});

		It("caps the stat alone at STAT_MULT_CAP (Pattern P)", [this]()
		{
			FDamageMathInputs Inputs = PlainInputs();
			Inputs.AttackerStatMultiplier = DAMAGE_MATH_TEST_OVER_CAP_STAT;
			Inputs.bHasAttackerStats = true;
			Inputs.EvolutionModifiedStat = DAMAGE_MATH_TEST_OVER_CAP_STAT;
			TestEqual(TEXT("capped at the stat cap"), WoRDamageMath::Compute(Inputs).FinalDamage,
					  FMath::RoundToInt(DAMAGE_MATH_TEST_BASE * CombatConstants::STAT_MULT_CAP));
		});

		It("deals nothing for a base of 0 and at least MIN_DAMAGE otherwise", [this]()
		{
			FDamageMathInputs Inputs = PlainInputs();
			Inputs.BaseDamage = 0;
			TestEqual(TEXT("base 0"), WoRDamageMath::Compute(Inputs).FinalDamage, 0);
			Inputs.BaseDamage = DamageConstants::MIN_DAMAGE;
			Inputs.AttackerStatMultiplier = 0.0f;
			TestEqual(TEXT("minimum"), WoRDamageMath::Compute(Inputs).FinalDamage, DamageConstants::MIN_DAMAGE);
		});
	});

	Describe("UDamageCalculator", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
		});

		AfterEach([this]()
		{
			TestWorld.Reset();
		});

		It("CalculateDamage equals Compute over the gathered inputs", [this]()
		{
			UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			UCharacterData *DefenderData = FWoRTestWorld::NewCharacterData();
			DefenderData->Defense = DAMAGE_MATH_TEST_DEFENSE_POINTS;
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Defender = TestWorld->SpawnCombatant(DefenderData);
			if (!TestTrue(TEXT("setup"), Calculator && Attacker && Defender))
			{
				return;
			}
			for (const EActionType ActionType : {EActionType::Ability, EActionType::Spell})
			{
				FDamageCalculationInput Input;
				Input.BaseDamage = DAMAGE_MATH_TEST_BASE;
				Input.ActionType = ActionType;
				Input.bCanCrit = false;
				const FDamageCalculationResult Direct = Calculator->CalculateDamage(Attacker, Defender, Input);
				const FDamageCalculationResult Core =
					WoRDamageMath::Compute(Calculator->GatherDamageMathInputs(Attacker, Defender, Input));
				TestEqual(TEXT("final"), Direct.FinalDamage, Core.FinalDamage);
				TestEqual(TEXT("before defense"), Direct.DamageBeforeDefense, Core.DamageBeforeDefense);
				TestEqual(TEXT("blocked"), Direct.DamageBlockedByDefense, Core.DamageBlockedByDefense);
				TestTrue(TEXT("defense applied"), Direct.DamageBlockedByDefense > 0);
			}
		});

		It("the debug readout shows the final damage", [this]()
		{
			UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestTrue(TEXT("setup"), Calculator && Attacker && Defender))
			{
				return;
			}
			FDamageCalculationInput Input;
			Input.BaseDamage = DAMAGE_MATH_TEST_BASE;
			Input.ActionType = EActionType::Ability;
			Input.bCanCrit = false;
			const int32 Final = Calculator->CalculateDamage(Attacker, Defender, Input).FinalDamage;
			const FString Readout = UDamageMathDebug::GetDamageMathString(Attacker, Attacker, Defender, DAMAGE_MATH_TEST_BASE, false);
			TestTrue(TEXT("final in the readout"), Readout.Contains(FString::Printf(TEXT("final %d"), Final)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
