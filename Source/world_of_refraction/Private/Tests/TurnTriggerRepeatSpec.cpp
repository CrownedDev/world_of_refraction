// TurnTriggerRepeatSpec.cpp
// B-300 (ET-17): an "On Turn Start" / "On Turn End" triggered effect fires at every such event, not once for its
// whole life. Before the fix the trigger latched on its first fire and only re-armed when its condition was
// unmet, which never happens for an event trigger. A threshold trigger (HP below %) still fires once per crossing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 TURN_TRIGGER_TEST_EFFECT_ID = 9300;
	constexpr int32 TURN_TRIGGER_TEST_CHARGES = 5;
	constexpr float TURN_TRIGGER_TEST_HEAL = 1.0f;
	/** HP threshold (percent) for the latch case; a fresh combatant is at full HP, so "above" is met. */
	constexpr float TURN_TRIGGER_TEST_HP_THRESHOLD = 50.0f;
}

BEGIN_DEFINE_SPEC(FTurnTriggerRepeatSpec, "WoR.Effects.TurnTriggerRepeat",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	AActor *Holder = nullptr;

	/** A charged, permanent triggered effect: each fire spends one charge, so the charge count shows the fires. */
	void ApplyTriggered(ESkillTrigger Trigger, float Threshold = 0.0f)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreatePersistent(TEXT("Turn trigger"), TURN_TRIGGER_TEST_EFFECT_ID,
																		  ESkillEffectType::HealthRestore, TURN_TRIGGER_TEST_HEAL);
		Effect.ProcessTiming = ESkillEffectTiming::OnTrigger;
		Effect.TriggerCondition = Trigger;
		Effect.TriggerThreshold = Threshold;
		Effect.Charges = TURN_TRIGGER_TEST_CHARGES;
		Effects->ApplyEffect(Holder, Effect, Holder, TEXT("TurnTriggerTest"));
	}

	int32 ChargesLeft() const
	{
		const TArray<FActiveSkillEffect> Held = Effects->GetEffectsByType(Holder, ESkillEffectType::HealthRestore);
		return Held.Num() > 0 ? Held[0].Charges : 0;
	}
END_DEFINE_SPEC(FTurnTriggerRepeatSpec)

void FTurnTriggerRepeatSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		Effects = nullptr;
		Holder = nullptr;
		TestWorld.Reset();
	});

	for (const ESkillTrigger Trigger : {ESkillTrigger::OnTurnStart, ESkillTrigger::OnTurnEnd})
	{
		It(UEnum::GetValueAsString(Trigger) + TEXT(" fires on every event, spending a charge each time"), [this, Trigger]()
		{
			if (!TestTrue(TEXT("setup"), Effects && Holder)) return;
			ApplyTriggered(Trigger);
			const int32 Before = ChargesLeft();
			if (!TestTrue(TEXT("held with charges"), Before > 1)) return;
			Effects->ProcessTriggerEffects(Holder, Trigger);
			TestEqual(TEXT("first event fires"), ChargesLeft(), Before - 1);
			Effects->ProcessTriggerEffects(Holder, Trigger);
			TestEqual(TEXT("second event fires too"), ChargesLeft(), Before - 2);
		});
	}

	It("still fires a threshold trigger once per crossing", [this]()
	{
		if (!TestTrue(TEXT("setup"), Effects && Holder)) return;
		ApplyTriggered(ESkillTrigger::OnHPAboveThreshold, TURN_TRIGGER_TEST_HP_THRESHOLD);
		const int32 Before = ChargesLeft();
		if (!TestTrue(TEXT("held with charges"), Before > 1)) return;
		Effects->ProcessTriggerEffects(Holder, ESkillTrigger::OnHPAboveThreshold);
		Effects->ProcessTriggerEffects(Holder, ESkillTrigger::OnHPAboveThreshold);
		TestEqual(TEXT("one fire while the condition stays met"), ChargesLeft(), Before - 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
