// GuardSpec.cpp
// D-084 / D-087 (Crown, 2026-10-26; B-168): Block and Parry are one defense, Guard. A normal Guard
// takes half, a Perfect Guard takes less, Dodge takes nothing, and no defense reflects; old block /
// parry triggers map onto On Guard / On Perfect Guard.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/GuardRules.h"
#include "Skills/Effects/SkillTriggerUtils.h"

namespace
{
	constexpr int32 GUARD_TEST_DAMAGE = 100;
	constexpr float GUARD_TEST_WINDOW_SECONDS = 0.5f;
	constexpr float GUARD_TEST_ATTACK_SIZE = 1.0f;
	constexpr bool GUARD_TEST_PERFECT = true;
	constexpr bool GUARD_TEST_NORMAL = false;

	TArray<ESkillTrigger> GuardTestTriggers(EDefenseType Type, bool bPerfect)
	{
		TArray<ESkillTrigger> Triggers;
		SkillTriggerUtils::DefenseOutcomeToTriggers(Type, bPerfect, Triggers);
		return Triggers;
	}
}

BEGIN_DEFINE_SPEC(FGuardSpec, "WoR.Defense.Guard",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UDefenseSystem *DefenseSystem = nullptr;

	int32 Taken(EDefenseType Type, bool bPerfect)
	{
		return DefenseSystem->CalculateDefenseResult(GUARD_TEST_DAMAGE, Type, /*bDefenseSuccessful*/ true, bPerfect).FinalDamage;
	}

	int32 ExpectedTaken(float Reduction) const
	{
		return FMath::RoundToInt(GUARD_TEST_DAMAGE * (1.0f - Reduction));
	}
END_DEFINE_SPEC(FGuardSpec)

void FGuardSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		DefenseSystem = TestWorld->GetSubsystem<UDefenseSystem>();
		TestNotNull(TEXT("DefenseSystem subsystem"), DefenseSystem);
	});

	AfterEach([this]()
	{
		DefenseSystem = nullptr;
		TestWorld.Reset();
	});

	Describe("damage", [this]()
	{
		It("a Guard takes half, a Perfect Guard less, a Dodge nothing, a miss everything", [this]()
		{
			TestEqual(TEXT("Guard"), Taken(EDefenseType::Block, GUARD_TEST_NORMAL), ExpectedTaken(DefenseSystem->BlockReduction));
			TestEqual(TEXT("Perfect Guard"), Taken(EDefenseType::Block, GUARD_TEST_PERFECT), ExpectedTaken(DefenseSystem->PerfectGuardReduction));
			TestTrue(TEXT("Perfect Guard takes less"), Taken(EDefenseType::Block, GUARD_TEST_PERFECT) < Taken(EDefenseType::Block, GUARD_TEST_NORMAL));
			TestEqual(TEXT("Dodge"), Taken(EDefenseType::Dodge, GUARD_TEST_PERFECT), 0);
			TestEqual(TEXT("missed timing"),
					  DefenseSystem->CalculateDefenseResult(GUARD_TEST_DAMAGE, EDefenseType::Block, false, false).FinalDamage, GUARD_TEST_DAMAGE);
		});

		It("treats a Parry as a Guard and never reflects", [this]()
		{
			TestEqual(TEXT("Parry = Guard"), Taken(EDefenseType::Parry, GUARD_TEST_NORMAL), Taken(EDefenseType::Block, GUARD_TEST_NORMAL));
			TestEqual(TEXT("Perfect Parry = Perfect Guard"), Taken(EDefenseType::Parry, GUARD_TEST_PERFECT), Taken(EDefenseType::Block, GUARD_TEST_PERFECT));
			for (const EDefenseType Type : {EDefenseType::Block, EDefenseType::Parry})
			{
				for (const bool bPerfect : {GUARD_TEST_NORMAL, GUARD_TEST_PERFECT})
				{
					TestEqual(TEXT("no reflect"), DefenseSystem->CalculateDefenseResult(GUARD_TEST_DAMAGE, Type, true, bPerfect).ReflectedDamage, 0);
				}
			}
		});

		It("buffers a Parry press as a Guard press", [this]()
		{
			AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			DefenseSystem->OpenDefenseWindow(Attacker, Defender, GUARD_TEST_ATTACK_SIZE, GUARD_TEST_DAMAGE, GUARD_TEST_WINDOW_SECONDS, false);
			DefenseSystem->SubmitDefenseInput(Defender, EDefenseType::Parry);
			const FDefenseState State = DefenseSystem->GetDefenseState(Defender);
			if (TestEqual(TEXT("one press"), State.InputBuffer.Num(), 1))
			{
				TestTrue(TEXT("stored as Guard"), State.InputBuffer[0].Type == EDefenseType::Block);
			}
			DefenseSystem->CloseDefenseWindow(Defender);
		});
	});

	Describe("what a Guard stops and lets through", [this]()
	{
		It("only a Perfect Guard or a Dodge stops an interruptable hit", [this]()
		{
			TestTrue(TEXT("Perfect Guard"), GuardRules::StopsInterruptableHit(EDefenseType::Block, true, GUARD_TEST_PERFECT));
			TestFalse(TEXT("Guard"), GuardRules::StopsInterruptableHit(EDefenseType::Block, true, GUARD_TEST_NORMAL));
			TestTrue(TEXT("Dodge"), GuardRules::StopsInterruptableHit(EDefenseType::Dodge, true, GUARD_TEST_NORMAL));
			TestFalse(TEXT("a failed Perfect Guard"), GuardRules::StopsInterruptableHit(EDefenseType::Block, false, GUARD_TEST_PERFECT));
		});

		It("lets less status through a Perfect Guard", [this]()
		{
			TestEqual(TEXT("Guard"), GuardRules::GuardBuildupMultiplier(GUARD_TEST_NORMAL), CombatConstants::BLOCK_BUILDUP_MULTIPLIER);
			TestEqual(TEXT("Perfect Guard"), GuardRules::GuardBuildupMultiplier(GUARD_TEST_PERFECT), CombatConstants::PARRY_BUILDUP_MULTIPLIER);
		});
	});

	Describe("triggers", [this]()
	{
		It("a Guard fires On Guard and legacy On Block, not the perfect ones", [this]()
		{
			const TArray<ESkillTrigger> Triggers = GuardTestTriggers(EDefenseType::Block, GUARD_TEST_NORMAL);
			TestTrue(TEXT("On Guard"), Triggers.Contains(ESkillTrigger::OnGuard));
			TestTrue(TEXT("On Block"), Triggers.Contains(ESkillTrigger::OnBlock));
			TestFalse(TEXT("no On Perfect Guard"), Triggers.Contains(ESkillTrigger::OnPerfectGuard));
			TestFalse(TEXT("no On Parry"), Triggers.Contains(ESkillTrigger::OnParry));
		});

		It("a Perfect Guard also fires On Perfect Guard and every legacy perfect / parry trigger", [this]()
		{
			const TArray<ESkillTrigger> Triggers = GuardTestTriggers(EDefenseType::Block, GUARD_TEST_PERFECT);
			for (const ESkillTrigger Trigger : {ESkillTrigger::OnGuard, ESkillTrigger::OnPerfectGuard, ESkillTrigger::OnBlock,
												ESkillTrigger::OnPerfectBlock, ESkillTrigger::OnParry, ESkillTrigger::OnPerfectParry})
			{
				TestTrue(*FString::Printf(TEXT("fires %s"), *UEnum::GetValueAsString(Trigger)), Triggers.Contains(Trigger));
				TestTrue(TEXT("is a defense-outcome trigger"), SkillTriggerUtils::IsDefenseOutcomeTrigger(Trigger));
			}
			TestEqual(TEXT("a Perfect Parry press maps the same"), GuardTestTriggers(EDefenseType::Parry, GUARD_TEST_PERFECT), Triggers);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
