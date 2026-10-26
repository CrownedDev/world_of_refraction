// DamageCalculatorSpec.cpp
// UDamageCalculator::CalculateDamage on bare combatants (no loadout, no grid slot,
// no effects): Pattern-P stat cap, defense reduction + clamp, crit damage.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/CombatConstants.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"

namespace
{
	constexpr int32 TEST_BASE_DAMAGE = 100;
	/** Below max investment — the stat formula stays under its cap. */
	constexpr int32 PARTIAL_STAT_POINTS = 50;
	/** Far past max investment — the raw stat formula overshoots its cap. */
	constexpr int32 OVERCAP_STAT_POINTS = 500;
	constexpr float ALWAYS_CRIT = 1.0f;
	constexpr float NEVER_CRIT = 0.0f;
	constexpr int32 TEST_DEFENSE_BUFF_EFFECT_ID = 9101;
	constexpr float DOUBLING_BUFF_PERCENT = 100.0f;
	constexpr int32 DEFENSE_BUFF_TURNS = 1;
}

BEGIN_DEFINE_SPEC(FDamageCalculatorSpec, "WoR.Damage",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UDamageCalculator *Calculator = nullptr;
	UCharacterData *AttackerData = nullptr;
	UCharacterData *DefenderData = nullptr;

	/** Spawn after the stat points are set — the component seeds from the asset at BeginPlay. */
	FDamageCalculationResult Calculate(const FDamageCalculationInput &Input)
	{
		if (!Calculator)
		{
			return FDamageCalculationResult(); // setup failure already recorded in BeforeEach
		}
		AActor *Attacker = TestWorld->SpawnCombatant(AttackerData);
		AActor *Defender = TestWorld->SpawnCombatant(DefenderData);
		return Calculator->CalculateDamage(Attacker, Defender, Input);
	}

	static FDamageCalculationInput MakeInput(EActionType ActionType, float CritChance = NEVER_CRIT)
	{
		FDamageCalculationInput Input;
		Input.BaseDamage = TEST_BASE_DAMAGE;
		Input.ActionType = ActionType;
		Input.OverrideCritChance = CritChance;
		return Input;
	}
END_DEFINE_SPEC(FDamageCalculatorSpec)

void FDamageCalculatorSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		TestNotNull(TEXT("DamageCalculator subsystem"), Calculator);
		AttackerData = FWoRTestWorld::NewCharacterData();
		DefenderData = FWoRTestWorld::NewCharacterData();
	});

	AfterEach([this]()
	{
		Calculator = nullptr;
		TestWorld.Reset();
	});

	Describe("Pattern-P stat cap", [this]()
	{
		It("leaves physical damage uncapped below the stat ceiling", [this]()
		{
			AttackerData->RawDamage = PARTIAL_STAT_POINTS;
			const float Multiplier = 1.0f + PARTIAL_STAT_POINTS * CombatConstants::RAW_DAMAGE_PER_POINT;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability));
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage, FMath::RoundToInt(TEST_BASE_DAMAGE * Multiplier));
		});

		It("caps physical stat damage at STAT_MULT_CAP", [this]()
		{
			AttackerData->RawDamage = OVERCAP_STAT_POINTS;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability));
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage,
					  FMath::RoundToInt(TEST_BASE_DAMAGE * CombatConstants::STAT_MULT_CAP));
		});

		It("caps spell stat damage at STAT_MULT_CAP", [this]()
		{
			AttackerData->SpellDamage = OVERCAP_STAT_POINTS;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Spell));
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage,
					  FMath::RoundToInt(TEST_BASE_DAMAGE * CombatConstants::STAT_MULT_CAP));
		});
	});

	Describe("Defense", [this]()
	{
		It("reduces damage by the defender's stat fraction below the cap", [this]()
		{
			DefenderData->Defense = PARTIAL_STAT_POINTS;
			const float Reduction = PARTIAL_STAT_POINTS * CombatConstants::DEFENSE_PER_POINT;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability));
			TestEqual(TEXT("Reduction"), Result.DefenderFlatDefense, Reduction, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage, FMath::RoundToInt(TEST_BASE_DAMAGE * (1.0f - Reduction)));
		});

		It("clamps stat defense at UNIVERSAL_STAT_CAP", [this]()
		{
			DefenderData->Defense = OVERCAP_STAT_POINTS;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability));
			TestEqual(TEXT("Reduction"), Result.DefenderFlatDefense, CombatConstants::UNIVERSAL_STAT_CAP, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage,
					  FMath::RoundToInt(TEST_BASE_DAMAGE * (1.0f - CombatConstants::UNIVERSAL_STAT_CAP)));
		});

		It("is skipped when the input ignores defense", [this]()
		{
			DefenderData->Defense = OVERCAP_STAT_POINTS;
			FDamageCalculationInput Input = MakeInput(EActionType::Ability);
			Input.bIgnoreDefense = true;

			const FDamageCalculationResult Result = Calculate(Input);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage, TEST_BASE_DAMAGE);
		});

		It("clamps buffed defense at DEFENSE_REDUCTION_CEILING and never drops a hit below MIN_DAMAGE", [this]()
		{
			USkillEffectManager *EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !Calculator)
			{
				return;
			}
			DefenderData->Defense = OVERCAP_STAT_POINTS;
			AActor *Attacker = TestWorld->SpawnCombatant(AttackerData);
			AActor *Defender = TestWorld->SpawnCombatant(DefenderData);
			// The buff multiplies the capped 0.5 stat past the ceiling, so 1 damage lands well
			// under 0.5 — it would round to 0 without the MIN_DAMAGE clamp.
			EffectManager->ApplyEffect(Defender, FActiveSkillEffect::CreateBuff(
				TEXT("Defense Up"), TEST_DEFENSE_BUFF_EFFECT_ID, ESkillEffectType::DefenseBuff, DOUBLING_BUFF_PERCENT, DEFENSE_BUFF_TURNS));
			FDamageCalculationInput Input = MakeInput(EActionType::Ability);
			Input.BaseDamage = DamageConstants::MIN_DAMAGE;

			const FDamageCalculationResult Result = Calculator->CalculateDamage(Attacker, Defender, Input);
			TestEqual(TEXT("Reduction"), Result.DefenderFlatDefense, CombatConstants::DEFENSE_REDUCTION_CEILING, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage, DamageConstants::MIN_DAMAGE);
		});
	});

	Describe("Critical hits", [this]()
	{
		It("deal normal damage without crit-damage investment", [this]()
		{
			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability, ALWAYS_CRIT));
			TestTrue(TEXT("Was critical"), Result.bWasCritical);
			TestEqual(TEXT("CritMultiplier"), Result.CritMultiplier, CombatConstants::CRIT_DMG_BASE, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage, TEST_BASE_DAMAGE);
		});

		It("cap stat crit damage at CRIT_DAMAGE_STAT_CAP", [this]()
		{
			AttackerData->CritDamage = OVERCAP_STAT_POINTS;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability, ALWAYS_CRIT));
			TestTrue(TEXT("Was critical"), Result.bWasCritical);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage,
					  FMath::RoundToInt(TEST_BASE_DAMAGE * CombatConstants::CRIT_DAMAGE_STAT_CAP));
		});

		It("never happen at zero crit chance", [this]()
		{
			AttackerData->CritDamage = OVERCAP_STAT_POINTS;

			const FDamageCalculationResult Result = Calculate(MakeInput(EActionType::Ability, NEVER_CRIT));
			TestFalse(TEXT("Was critical"), Result.bWasCritical);
			TestEqual(TEXT("FinalDamage"), Result.FinalDamage, TEST_BASE_DAMAGE);
		});

		It("never happen when the input can't crit", [this]()
		{
			FDamageCalculationInput Input = MakeInput(EActionType::Ability, ALWAYS_CRIT);
			Input.bCanCrit = false;

			const FDamageCalculationResult Result = Calculate(Input);
			TestFalse(TEXT("Was critical"), Result.bWasCritical);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
