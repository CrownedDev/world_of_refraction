// FightRecordsSpec.cpp
// GAS stage 3 (B-042z, D-064): the fires-once uses and the armed gear conditionals are kept on the holder's
// turn tracker (B-042g2: only there; USkillEffectManager's own sets and the UseGAS 0 pass are gone). The tracker
// refuses a second use, arms and disarms the gear, and a new fight clears it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 RECORDS_TEST_EFFECT_ID = 9411;
	constexpr int32 RECORDS_TEST_DEF_ID = 94;
	constexpr int32 RECORDS_TEST_ITEM = 7101;
	constexpr float RECORDS_TEST_HEAL_VALUE = 10.0f;

	FActiveSkillEffect OnceHeal()
	{
		FActiveSkillEffect Heal = FActiveSkillEffect::CreateBuff(TEXT("Fight records heal"), RECORDS_TEST_EFFECT_ID,
																 ESkillEffectType::HealthRestore, RECORDS_TEST_HEAL_VALUE, 0);
		Heal.bFiresOncePerMatch = true;
		return Heal;
	}

	FGatheredEffect ArmedGear()
	{
		FSkillEffect Effect;
		Effect.EffectName = TEXT("Fight records gear");
		return FGatheredEffect(RECORDS_TEST_DEF_ID, 0, Effect, RECORDS_TEST_ITEM);
	}
}

BEGIN_DEFINE_SPEC(FFightRecordsSpec, "WoR.GAS.FightRecords",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	AActor *Holder = nullptr;
	AActor *Source = nullptr;
	UWoRTurnEffectTracker *Tracker = nullptr;

	bool ApplyHeal()
	{
		return EffectManager->ApplyEffect(Holder, OnceHeal(), Source, TEXT("Test")) == EEffectApplicationResult::Applied;
	}
END_DEFINE_SPEC(FFightRecordsSpec)

void FFightRecordsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Holder = TestWorld->SpawnCombatant(Data);
		Source = TestWorld->SpawnCombatant(Data);
		Tracker = Holder ? FWoRTestWorld::AddComponent<UWoRTurnEffectTracker>(Holder) : nullptr;
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Tracker"), Tracker))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		EffectManager = nullptr;
		Holder = nullptr;
		Source = nullptr;
		Tracker = nullptr;
		TestWorld.Reset();
	});

	It("refuses a second fires-once use, and the tracker records it", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ResetForNewCombat();
		TestTrue(TEXT("the first use lands"), ApplyHeal());
		TestFalse(TEXT("the second use is refused"), ApplyHeal());
		TestEqual(TEXT("the tracker holds the use"), Tracker->NumFiredOnce(), 1);
	});

	It("gives a doesn't-stack effect one use per holder, whichever source brings it", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ResetForNewCombat();
		FActiveSkillEffect Heal = OnceHeal();
		Heal.bDoesNotStack = true;
		TestTrue(TEXT("the first source's use lands"),
				 EffectManager->ApplyEffect(Holder, Heal, Source, TEXT("Test")) == EEffectApplicationResult::Applied);
		TestTrue(TEXT("another source's use is refused"),
				 EffectManager->ApplyEffect(Holder, Heal, Holder, TEXT("Other")) == EEffectApplicationResult::Rejected);
	});

	It("clears the tracker's records for a new fight", [this]()
	{
		if (!EffectManager) return;
		ApplyHeal();
		EffectManager->ArmConditionalEffects(Holder, {ArmedGear()});
		EffectManager->ResetForNewCombat();
		TestEqual(TEXT("No fires-once use left"), Tracker->NumFiredOnce(), 0);
		TestNull(TEXT("Gear no longer armed"), Tracker->GetArmedConditionals());
		TestTrue(TEXT("the use is back"), ApplyHeal());
	});

	It("arms and disarms the gear", [this]()
	{
		if (!EffectManager) return;
		TestFalse(TEXT("not armed before battle prep"), EffectManager->IsGearArmed(Holder));
		EffectManager->ArmConditionalEffects(Holder, {ArmedGear()});
		TestTrue(TEXT("armed"), EffectManager->IsGearArmed(Holder));
		const TArray<FGatheredEffect> *Armed = Tracker->GetArmedConditionals();
		TestTrue(TEXT("the tracker holds the armed set"), Armed && Armed->Num() == 1);
		EffectManager->ClearAllEffects();
		TestFalse(TEXT("disarmed at fight end"), EffectManager->IsGearArmed(Holder));
	});
}

#endif
