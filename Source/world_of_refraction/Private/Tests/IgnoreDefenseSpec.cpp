// IgnoreDefenseSpec.cpp
// B-042m (D-086, Q-146b A): Ignore Defense is an amount. A skill's IgnoreDefensePercent plus the
// attacker's IgnoreDefense effects cut the defender's Defense and positive element resistance by
// that share; weaknesses stay; 100% is the old full ignore.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/DamageMath.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 IGNORE_BASE_DAMAGE = 100;
	/** Far past max investment: Defense sits at its UNIVERSAL_STAT_CAP reduction. */
	constexpr int32 IGNORE_DEFENSE_POINTS = 500;
	constexpr float IGNORE_HALF_PERCENT = 50.0f;
	constexpr float IGNORE_FULL_PERCENT = 100.0f;
	constexpr float IGNORE_EFFECT_PERCENT = 30.0f;
	constexpr float IGNORE_LARGE_EFFECT_PERCENT = 60.0f;
	constexpr int32 IGNORE_EFFECT_ID = 4701;
	constexpr int32 IGNORE_SECOND_EFFECT_ID = 4702;
	constexpr int32 IGNORE_EFFECT_TURNS = 2;
	constexpr float IGNORE_HALF = 0.5f;
	constexpr float IGNORE_RESIST = 0.4f;
	constexpr float IGNORE_WEAKNESS = -0.3f;
	constexpr float IGNORE_TOLERANCE = 1.0e-4f;
}

BEGIN_DEFINE_SPEC(FIgnoreDefenseSpec, "WoR.Damage.IgnoreDefense",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UDamageCalculator *Calculator = nullptr;
	USkillEffectManager *Effects = nullptr;
	AActor *Attacker = nullptr;
	AActor *Defender = nullptr;

	static FDamageCalculationInput Hit(float IgnorePercent)
	{
		FDamageCalculationInput Input;
		Input.BaseDamage = IGNORE_BASE_DAMAGE;
		Input.ActionType = EActionType::Ability;
		Input.bCanCrit = false;
		Input.IgnoreDefensePercent = IgnorePercent;
		return Input;
	}

	void GiveIgnoreEffect(int32 EffectID, float Percent) const
	{
		Effects->ApplyEffect(Attacker, FActiveSkillEffect::CreateBuff(TEXT("Ignore defense test"), EffectID,
																	  ESkillEffectType::IgnoreDefense, Percent, IGNORE_EFFECT_TURNS),
							 Attacker);
	}
END_DEFINE_SPEC(FIgnoreDefenseSpec)

void FIgnoreDefenseSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		UCharacterData *DefenderData = FWoRTestWorld::NewCharacterData();
		DefenderData->Defense = IGNORE_DEFENSE_POINTS;
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Defender = TestWorld->SpawnCombatant(DefenderData, true);
		if (!TestNotNull(TEXT("DamageCalculator"), Calculator) || !TestNotNull(TEXT("SkillEffectManager"), Effects) ||
			!TestNotNull(TEXT("Attacker"), Attacker) || !TestNotNull(TEXT("Defender"), Defender))
		{
			Calculator = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Calculator = nullptr;
		Effects = nullptr;
		Attacker = nullptr;
		Defender = nullptr;
		TestWorld.Reset();
	});

	It("cuts Defense by the skill's amount", [this]()
	{
		if (!Calculator) return;
		const float Reduction = Calculator->GetDefenderFlatDefense(Defender);
		TestEqual(TEXT("Defense at its cap"), Reduction, CombatConstants::UNIVERSAL_STAT_CAP, IGNORE_TOLERANCE);
		TestEqual(TEXT("0%: full Defense"), Calculator->CalculateDamage(Attacker, Defender, Hit(0.0f)).FinalDamage,
				  FMath::RoundToInt(IGNORE_BASE_DAMAGE * (1.0f - Reduction)));
		TestEqual(TEXT("50%: half the Defense"), Calculator->CalculateDamage(Attacker, Defender, Hit(IGNORE_HALF_PERCENT)).FinalDamage,
				  FMath::RoundToInt(IGNORE_BASE_DAMAGE * (1.0f - Reduction * IGNORE_HALF)));
		const FDamageCalculationResult Full = Calculator->CalculateDamage(Attacker, Defender, Hit(IGNORE_FULL_PERCENT));
		TestEqual(TEXT("100%: no Defense"), Full.FinalDamage, IGNORE_BASE_DAMAGE);
		TestEqual(TEXT("100%: nothing blocked"), Full.DamageBlockedByDefense, 0);
		FDamageCalculationInput Flag = Hit(0.0f);
		Flag.bIgnoreDefense = true;
		TestEqual(TEXT("bIgnoreDefense is 100%"), Calculator->CalculateDamage(Attacker, Defender, Flag).FinalDamage, Full.FinalDamage);
	});

	It("cuts positive element resistance by the same share and keeps weaknesses", [this]()
	{
		TestEqual(TEXT("Resistance halved"), WoRDamageMath::GetElementMultiplier(IGNORE_RESIST, IGNORE_HALF),
				  1.0f - IGNORE_RESIST * IGNORE_HALF, IGNORE_TOLERANCE);
		TestEqual(TEXT("Resistance ignored at 100%"), WoRDamageMath::GetElementMultiplier(IGNORE_RESIST, 1.0f), 1.0f);
		TestEqual(TEXT("Weakness kept"), WoRDamageMath::GetElementMultiplier(IGNORE_WEAKNESS, IGNORE_HALF),
				  1.0f - IGNORE_WEAKNESS, IGNORE_TOLERANCE);
		TestEqual(TEXT("Defense halved"), WoRDamageMath::GetIgnoredDefense(CombatConstants::UNIVERSAL_STAT_CAP, IGNORE_HALF),
				  CombatConstants::UNIVERSAL_STAT_CAP * IGNORE_HALF, IGNORE_TOLERANCE);
	});

	It("adds the attacker's IgnoreDefense effects at their value", [this]()
	{
		if (!Calculator) return;
		GiveIgnoreEffect(IGNORE_EFFECT_ID, IGNORE_EFFECT_PERCENT);
		TestEqual(TEXT("Effect alone"), Calculator->GetIgnoreDefenseFraction(Attacker, Hit(0.0f)),
				  IGNORE_EFFECT_PERCENT / CombatConstants::STAT_PERCENT_DIVISOR, IGNORE_TOLERANCE);
		TestEqual(TEXT("Skill + effect"), Calculator->GetIgnoreDefenseFraction(Attacker, Hit(IGNORE_HALF_PERCENT)),
				  (IGNORE_HALF_PERCENT + IGNORE_EFFECT_PERCENT) / CombatConstants::STAT_PERCENT_DIVISOR, IGNORE_TOLERANCE);
		GiveIgnoreEffect(IGNORE_SECOND_EFFECT_ID, IGNORE_LARGE_EFFECT_PERCENT);
		TestEqual(TEXT("Capped at 100%"), Calculator->GetIgnoreDefenseFraction(Attacker, Hit(IGNORE_HALF_PERCENT)), 1.0f);
	});

	It("treats an IgnoreDefense effect with no value as a full ignore", [this]()
	{
		if (!Calculator) return;
		GiveIgnoreEffect(IGNORE_EFFECT_ID, 0.0f);
		TestEqual(TEXT("No value = 100%"), Calculator->GetIgnoreDefenseFraction(Attacker, Hit(0.0f)), 1.0f);
		TestEqual(TEXT("No Defense"), Calculator->CalculateDamage(Attacker, Defender, Hit(0.0f)).FinalDamage, IGNORE_BASE_DAMAGE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
