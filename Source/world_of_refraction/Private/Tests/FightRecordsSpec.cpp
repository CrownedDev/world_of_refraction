// FightRecordsSpec.cpp
// GAS stage 3 (B-042z, D-064): the fires-once uses and the armed gear conditionals are kept on the holder's
// turn tracker as well as on USkillEffectManager, and read from the tracker with `wor.Effects.UseGAS 1`.
// Both paths refuse a second use, arm and disarm the gear the same way, and a new fight clears both.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "GAS/WoREffectMirrorComponent.h"
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
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		Holder = nullptr;
		Source = nullptr;
		Tracker = nullptr;
		TestWorld.Reset();
	});

	It("refuses a second fires-once use on both paths, and the tracker records it", [this]()
	{
		if (!EffectManager) return;
		for (const bool bUseGAS : {false, true})
		{
			EffectManager->ResetForNewCombat();
			UWoREffectMirrorComponent::SetReadsFromGAS(bUseGAS);
			const TCHAR *Path = bUseGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0");
			TestTrue(FString::Printf(TEXT("%s: the first use lands"), Path), ApplyHeal());
			TestFalse(FString::Printf(TEXT("%s: the second use is refused"), Path), ApplyHeal());
			TestEqual(FString::Printf(TEXT("%s: the tracker holds the use"), Path), Tracker->NumFiredOnce(), 1);
		}
	});

	It("gives a doesn't-stack effect one use per holder, whichever source brings it, on both paths", [this]()
	{
		if (!EffectManager) return;
		for (const bool bUseGAS : {false, true})
		{
			EffectManager->ResetForNewCombat();
			UWoREffectMirrorComponent::SetReadsFromGAS(bUseGAS);
			const TCHAR *Path = bUseGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0");
			FActiveSkillEffect Heal = OnceHeal();
			Heal.bDoesNotStack = true;
			TestTrue(FString::Printf(TEXT("%s: the first source's use lands"), Path),
					 EffectManager->ApplyEffect(Holder, Heal, Source, TEXT("Test")) == EEffectApplicationResult::Applied);
			TestTrue(FString::Printf(TEXT("%s: another source's use is refused"), Path),
					 EffectManager->ApplyEffect(Holder, Heal, Holder, TEXT("Other")) == EEffectApplicationResult::Rejected);
		}
	});

	It("keeps a use spent before the switch is turned on", [this]()
	{
		if (!EffectManager) return;
		TestTrue(TEXT("UseGAS 0: the first use lands"), ApplyHeal());
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestFalse(TEXT("UseGAS 1: the tracker refuses the second"), ApplyHeal());
	});

	It("clears the tracker's records for a new fight", [this]()
	{
		if (!EffectManager) return;
		ApplyHeal();
		EffectManager->ArmConditionalEffects(Holder, {ArmedGear()});
		EffectManager->ResetForNewCombat();
		TestEqual(TEXT("No fires-once use left"), Tracker->NumFiredOnce(), 0);
		TestNull(TEXT("Gear no longer armed"), Tracker->GetArmedConditionals());
		UWoREffectMirrorComponent::SetReadsFromGAS(true);
		TestTrue(TEXT("UseGAS 1: the use is back"), ApplyHeal());
	});

	It("arms and disarms the gear the same way on both paths", [this]()
	{
		if (!EffectManager) return;
		for (const bool bUseGAS : {false, true})
		{
			UWoREffectMirrorComponent::SetReadsFromGAS(bUseGAS);
			const TCHAR *Path = bUseGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0");
			TestFalse(FString::Printf(TEXT("%s: not armed before battle prep"), Path), EffectManager->IsGearArmed(Holder));
			EffectManager->ArmConditionalEffects(Holder, {ArmedGear()});
			TestTrue(FString::Printf(TEXT("%s: armed"), Path), EffectManager->IsGearArmed(Holder));
			const TArray<FGatheredEffect> *Armed = Tracker->GetArmedConditionals();
			TestTrue(FString::Printf(TEXT("%s: the tracker holds the armed set"), Path), Armed && Armed->Num() == 1);
			EffectManager->ClearAllEffects();
			TestFalse(FString::Printf(TEXT("%s: disarmed at fight end"), Path), EffectManager->IsGearArmed(Holder));
		}
	});
}

#endif
