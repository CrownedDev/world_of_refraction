// StatusBuildupSpec.cpp
// B-135: the status bar as it works today. A hit fills the bar by its amount less the target's
// total status resistance; an element-matched resistance buff lowers only its own element; drains
// take a fraction or a flat amount; the bar decays each quiet turn and resets after
// STATUS_DECAY_FULL_RESET_TURNS; at STATUS_EFFECT_THRESHOLD it fires the element's status and
// empties; an immunity blocks the hit. Source-side amplification and the infused path are
// WoR.Status.InfusedBuildup; bar-cap break lengths are WoR.Effects.StatusBreaks.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatConstants.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr float BUILDUP_TEST_HIT = 30.0f;
	constexpr float BUILDUP_TEST_RESIST_PERCENT = 50.0f;
	constexpr float BUILDUP_TEST_DRAIN_FRACTION = 0.5f;
	constexpr float BUILDUP_TEST_TOLERANCE = 0.01f;
	constexpr int32 BUILDUP_TEST_RESIST_ID = 9501;
	constexpr int32 BUILDUP_TEST_IMMUNITY_ID = 9502;
	constexpr int32 BUILDUP_TEST_DURATION_TURNS = 3;
	/** Far past the threshold, whatever the target's resistance (below RESISTANCE_MAX). */
	constexpr float BUILDUP_TEST_OVERWHELMING_HIT = CombatConstants::STATUS_EFFECT_THRESHOLD * 100.0f;
}

BEGIN_DEFINE_SPEC(FStatusBuildupSpec, "WoR.Status.Buildup",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UStatusBuildupManager *Buildup = nullptr;
	USkillEffectManager *Effects = nullptr;
	AActor *Target = nullptr;

	/** What a source-less hit of Amount adds to Target today. */
	float Landed(float Amount, ESpellElement Element) const
	{
		return Amount * (1.0f - Buildup->GetTotalStatusResistance(Target, Element, EPhysicalDamageType::None));
	}

	bool Hit(float Amount, ESpellElement Element)
	{
		return Buildup->AddStatusBuildup(nullptr, Target, Amount, Element, EPhysicalDamageType::None);
	}

	void GiveEffect(int32 ID, ESkillEffectType Type, float Value, ESpellElement Element)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Buildup test"), ID, Type, Value, BUILDUP_TEST_DURATION_TURNS);
		Effect.Element = Element;
		Effect.bDelayFirstExecution = true;
		Effects->ApplyEffect(Target, Effect, Target);
	}
END_DEFINE_SPEC(FStatusBuildupSpec)

void FStatusBuildupSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("StatusBuildupManager"), Buildup) || !TestNotNull(TEXT("SkillEffectManager"), Effects) ||
			!TestNotNull(TEXT("Target"), Target))
		{
			Buildup = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Target = nullptr;
		Buildup = nullptr;
		Effects = nullptr;
		TestWorld.Reset();
	});

	It("fills by the hit less the target's total status resistance", [this]()
	{
		if (!Buildup) return;
		const float Expected = Landed(BUILDUP_TEST_HIT, ESpellElement::Fire);
		TestFalse(TEXT("Below the threshold, nothing fires"), Hit(BUILDUP_TEST_HIT, ESpellElement::Fire));
		TestNearlyEqual(TEXT("Buildup"), Buildup->GetStatusBarBuildup(Target), Expected, BUILDUP_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Percent of the threshold"), Buildup->GetStatusBarPercent(Target),
						Expected / CombatConstants::STATUS_EFFECT_THRESHOLD, BUILDUP_TEST_TOLERANCE);
		TestEqual(TEXT("Remembers the element"), Buildup->GetPendingElement(Target), ESpellElement::Fire);
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		TestNearlyEqual(TEXT("Hits add up"), Buildup->GetStatusBarBuildup(Target), 2.0f * Expected, BUILDUP_TEST_TOLERANCE);
	});

	It("lowers buildup with an element-matched resistance buff, and only for that element", [this]()
	{
		if (!Buildup) return;
		const float FireBefore = Buildup->GetTotalStatusResistance(Target, ESpellElement::Fire, EPhysicalDamageType::None);
		const float LightningBefore = Buildup->GetTotalStatusResistance(Target, ESpellElement::Lightning, EPhysicalDamageType::None);
		GiveEffect(BUILDUP_TEST_RESIST_ID, ESkillEffectType::ResistanceBuff, BUILDUP_TEST_RESIST_PERCENT, ESpellElement::Fire);
		TestNearlyEqual(TEXT("Fire resistance rises by the buff"),
						Buildup->GetTotalStatusResistance(Target, ESpellElement::Fire, EPhysicalDamageType::None),
						FMath::Min(FireBefore + BUILDUP_TEST_RESIST_PERCENT / CombatConstants::STAT_PERCENT_DIVISOR, CombatConstants::RESISTANCE_MAX),
						BUILDUP_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Lightning resistance unchanged"),
						Buildup->GetTotalStatusResistance(Target, ESpellElement::Lightning, EPhysicalDamageType::None),
						LightningBefore, BUILDUP_TEST_TOLERANCE);
		const float Expected = Landed(BUILDUP_TEST_HIT, ESpellElement::Fire);
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		TestNearlyEqual(TEXT("The Fire hit is reduced"), Buildup->GetStatusBarBuildup(Target), Expected, BUILDUP_TEST_TOLERANCE);
	});

	It("drains by a fraction or a flat amount, never below zero", [this]()
	{
		if (!Buildup) return;
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		const float Full = Buildup->GetStatusBarBuildup(Target);
		Buildup->ReduceStatusBuildup(Target, BUILDUP_TEST_DRAIN_FRACTION);
		TestNearlyEqual(TEXT("Fraction"), Buildup->GetStatusBarBuildup(Target), Full * (1.0f - BUILDUP_TEST_DRAIN_FRACTION),
						BUILDUP_TEST_TOLERANCE);
		Buildup->ReduceStatusBuildupByAmount(Target, BUILDUP_TEST_OVERWHELMING_HIT);
		TestNearlyEqual(TEXT("Flat, clamped at zero"), Buildup->GetStatusBarBuildup(Target), 0.0f, BUILDUP_TEST_TOLERANCE);
	});

	It("decays each quiet turn and resets after the full-reset count; a hit restarts the count", [this]()
	{
		if (!Buildup) return;
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		const float Full = Buildup->GetStatusBarBuildup(Target);
		const float Kept = 1.0f - CombatConstants::STATUS_DECAY_RATE;
		Buildup->ProcessStatusBarDecay(Target);
		TestNearlyEqual(TEXT("One quiet turn"), Buildup->GetStatusBarBuildup(Target), Full * Kept, BUILDUP_TEST_TOLERANCE);

		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		for (int32 Turn = 1; Turn < CombatConstants::STATUS_DECAY_FULL_RESET_TURNS; ++Turn)
		{
			Buildup->ProcessStatusBarDecay(Target);
		}
		TestTrue(TEXT("The hit restarted the count"), Buildup->GetStatusBarBuildup(Target) > 0.0f);
		Buildup->ProcessStatusBarDecay(Target);
		TestNearlyEqual(TEXT("Reset after the full-reset count"), Buildup->GetStatusBarBuildup(Target), 0.0f, BUILDUP_TEST_TOLERANCE);
	});

	It("fires the element's status at the threshold and empties the bar", [this]()
	{
		if (!Buildup) return;
		TestEqual(TEXT("Lightning caps into Stun"), Buildup->GetPendingTrigger(Target), ESkillEffectType::None);
		TestTrue(TEXT("Fires"), Hit(BUILDUP_TEST_OVERWHELMING_HIT, ESpellElement::Lightning));
		TestNearlyEqual(TEXT("Bar empty"), Buildup->GetStatusBarBuildup(Target), 0.0f, BUILDUP_TEST_TOLERANCE);
		TestTrue(TEXT("Target stunned"), Effects->IsStunned(Target));
	});

	It("blocks every hit while the target holds all-status immunity", [this]()
	{
		if (!Buildup) return;
		GiveEffect(BUILDUP_TEST_IMMUNITY_ID, ESkillEffectType::GrantAllStatusImmunity, 0.0f, ESpellElement::None);
		TestFalse(TEXT("Nothing fires"), Hit(BUILDUP_TEST_OVERWHELMING_HIT, ESpellElement::Lightning));
		TestNearlyEqual(TEXT("Bar untouched"), Buildup->GetStatusBarBuildup(Target), 0.0f, BUILDUP_TEST_TOLERANCE);
		TestFalse(TEXT("Not stunned"), Effects->IsStunned(Target));
	});

	It("forgets every bar between fights", [this]()
	{
		if (!Buildup) return;
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		Buildup->ResetForCombat();
		TestNearlyEqual(TEXT("Bar empty"), Buildup->GetStatusBarBuildup(Target), 0.0f, BUILDUP_TEST_TOLERANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
