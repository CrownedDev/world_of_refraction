// StatusBuildupSpec.cpp
// B-135: the status bar as it works today. A hit fills the bar by its amount less the target's
// total status resistance; an element-matched resistance buff lowers only its own element; drains
// take a fraction or a flat amount; the bar decays each quiet turn and resets after
// STATUS_DECAY_FULL_RESET_TURNS; at STATUS_EFFECT_THRESHOLD it fires the element's status and
// empties; an immunity blocks the hit. Source-side amplification and the infused path are
// WoR.Status.InfusedBuildup; bar-cap break lengths are WoR.Effects.StatusBreaks.
// B-042o: every case runs on the enum bar (wor.Effects.UseGAS 0) and on GAS (1: the target carries
// an ASC with synced attributes and an effect mirror, so the bar is its Buildup attribute).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatConstants.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoREffectMirrorComponent.h"
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
	constexpr float BUILDUP_TEST_OVERWHELMING_FACTOR = 100.0f;
	constexpr float BUILDUP_TEST_OVERWHELMING_HIT = CombatConstants::STATUS_EFFECT_THRESHOLD * BUILDUP_TEST_OVERWHELMING_FACTOR;
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

	/** What a GAS bar needs: an ASC, the attribute sync and an effect mirror (tag reads). */
	static void PutOnGAS(AActor *Actor)
	{
		FWoRTestWorld::AddEffectMirror(Actor);
		FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Actor)->SyncAttributes();
	}

	void DefineCases();
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
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		Target = nullptr;
		Buildup = nullptr;
		Effects = nullptr;
		TestWorld.Reset();
	});

	for (const bool bOnGAS : {false, true})
	{
		Describe(bOnGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0"), [this, bOnGAS]()
		{
			BeforeEach([this, bOnGAS]()
			{
				UWoREffectMirrorComponent::SetReadsFromGAS(bOnGAS);
				if (bOnGAS && Target)
				{
					PutOnGAS(Target);
				}
			});
			DefineCases();
		});
	}
}

void FStatusBuildupSpec::DefineCases()
{
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
		TestNearlyEqual(TEXT("Hits add up"), Buildup->GetStatusBarBuildup(Target), Expected + Expected, BUILDUP_TEST_TOLERANCE);
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
		Hit(BUILDUP_TEST_HIT, ESpellElement::Lightning);
		TestEqual(TEXT("Lightning caps into Stun"), Buildup->GetPendingTrigger(Target), ESkillEffectType::Stun);
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
		AActor *Other = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("Second target"), Other)) return;
		if (UWoREffectMirrorComponent::ReadsFromGAS())
		{
			PutOnGAS(Other);
		}
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		Buildup->AddStatusBuildup(nullptr, Other, BUILDUP_TEST_HIT, ESpellElement::Fire, EPhysicalDamageType::None);
		Buildup->ResetForCombat();
		TestNearlyEqual(TEXT("Bar empty"), Buildup->GetStatusBarBuildup(Target), 0.0f, BUILDUP_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("The other bar empty"), Buildup->GetStatusBarBuildup(Other), 0.0f, BUILDUP_TEST_TOLERANCE);
	});

	It("keeps the bar in the Buildup attribute on GAS, and only there", [this]()
	{
		if (!Buildup) return;
		Hit(BUILDUP_TEST_HIT, ESpellElement::Fire);
		const UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!UWoREffectMirrorComponent::ReadsFromGAS())
		{
			TestTrue(TEXT("Enum bar"), Buildup->GetBarDebugString(Target).Contains(TEXT("the enum bar")));
			return;
		}
		if (!TestNotNull(TEXT("ASC"), ASC)) return;
		const FGameplayAttribute Attribute = UWoRAttributeSet_Vitals::GetBuildupAttribute();
		TestNearlyEqual(TEXT("The attribute is the bar"), ASC->GetNumericAttribute(Attribute), Buildup->GetStatusBarBuildup(Target),
						BUILDUP_TEST_TOLERANCE);
		TestTrue(TEXT("On GAS"), Buildup->GetBarDebugString(Target).Contains(TEXT("on GAS")));
		Buildup->ProcessStatusBarDecay(Target);
		TestNearlyEqual(TEXT("Decay writes the attribute"), ASC->GetNumericAttribute(Attribute), Buildup->GetStatusBarBuildup(Target),
						BUILDUP_TEST_TOLERANCE);
		Buildup->ResetStatusBar(Target);
		TestNearlyEqual(TEXT("Reset empties the attribute"), ASC->GetNumericAttribute(Attribute), 0.0f, BUILDUP_TEST_TOLERANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
