// BossRulesSpec.cpp
// B-355 (D-144): a boss's one rule list (when -> condition -> result, "next phase" a result). One
// authored list drives both a phase change (a state-change rule on the boss's HP) and a trigger (a
// fight-start rule on who is in the party); old phases and triggers read across as rules.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/Boss/BossRules.h"
#include "Combat/Boss/BossTriggers.h"

namespace
{
	constexpr int32 BOSS_RULES_TEST_HP_LINE = 50;
	constexpr int32 BOSS_RULES_TEST_MARGIN = 10;
	constexpr float BOSS_RULES_TEST_OLD_HP_LINE = 50.0f;
	constexpr float BOSS_RULES_TEST_PERCENT_SCALE = 100.0f;
	const FName BOSS_RULES_TEST_PHASE(TEXT("RulesSpecPhase"));
}

BEGIN_DEFINE_SPEC(FBossRulesSpec, "WoR.Boss.Rules",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UBossData *Data = nullptr;
	UCharacterData *Lord = nullptr;
	AActor *BossActor = nullptr;
	UCharacterDataComponent *BossHP = nullptr;

	/** A phase that only says what changes (when is the rules' job). */
	static FBossPhase ChangesOnly()
	{
		FBossPhase Phase;
		Phase.PhaseId = BOSS_RULES_TEST_PHASE;
		Phase.AggressionSteps = 1;
		return Phase;
	}

	static FWoRCondition Condition(EWoRConditionSubject Subject, int32 Value = 0)
	{
		FWoRCondition Out;
		Out.Subject = Subject;
		Out.Value = Value;
		return Out;
	}

	static FBossRule Rule(EBossRuleWhen When, const FWoRCondition &If, EBossRuleResult Then)
	{
		FBossRule Out;
		Out.When = When;
		Out.Conditions.Conditions.Add(If);
		Out.Results.Add(Then);
		return Out;
	}

	UBossPhaseComponent *SpawnBoss()
	{
		BossActor = TestWorld->SpawnCombatant(Data->Boss, /*bAIControlled=*/true);
		BossHP = BossActor->FindComponentByClass<UCharacterDataComponent>();
		UBossPhaseComponent *Boss = NewObject<UBossPhaseComponent>(BossActor);
		BossActor->AddInstanceComponent(Boss);
		Boss->RegisterComponent();
		Boss->Setup(Data);
		return Boss;
	}

	void SetBossHPPercent(int32 Percent)
	{
		BossHP->ServerSetHP(FMath::FloorToInt32(BossHP->MaxHP * Percent / BOSS_RULES_TEST_PERCENT_SCALE));
	}
END_DEFINE_SPEC(FBossRulesSpec)

void FBossRulesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Data = NewObject<UBossData>(GetTransientPackage());
		Data->Boss = FWoRTestWorld::NewCharacterData();
		Lord = FWoRTestWorld::NewCharacterData();
	});

	AfterEach([this]()
	{
		Data = nullptr;
		Lord = nullptr;
		BossActor = nullptr;
		BossHP = nullptr;
		TestWorld.Reset();
	});

	It("fires a phase change and a trigger from the one list", [this]()
	{
		FWoRCondition InParty = Condition(EWoRConditionSubject::CharacterInParty);
		InParty.Character = Lord;
		Data->Phases.Add(ChangesOnly());
		Data->Rules.Add(Rule(EBossRuleWhen::FightStart, InParty, EBossRuleResult::Strengthen));
		Data->Rules.Add(Rule(EBossRuleWhen::StateChange, Condition(EWoRConditionSubject::BossHPAtMostPercent, BOSS_RULES_TEST_HP_LINE),
							 EBossRuleResult::NextPhase));

		UBossPhaseComponent *Boss = SpawnBoss();
		const int32 BaseMaxHP = BossHP->MaxHP;
		AActor *LordActor = TestWorld->SpawnCombatant(Lord);
		TestEqual(TEXT("the fight-start rule fires"), BossTriggers::ApplyTo(BossActor, *Data, {LordActor}), 1);
		TestTrue(TEXT("the boss is stronger"), BossHP->MaxHP > BaseMaxHP);

		SetBossHPPercent(BOSS_RULES_TEST_HP_LINE + BOSS_RULES_TEST_MARGIN);
		TestEqual(TEXT("above the line: no phase"), Boss->GetReachedPhaseCount(), 0);
		SetBossHPPercent(BOSS_RULES_TEST_HP_LINE - BOSS_RULES_TEST_MARGIN);
		TestEqual(TEXT("the state-change rule enters the next phase"), Boss->GetReachedPhaseCount(), 1);
		TestEqual(TEXT("with that phase's changes"), Boss->GetAggressionSteps(), 1);
	});

	It("reads old phases and triggers across as rules", [this]()
	{
		FBossPhase OldPhase = ChangesOnly();
		OldPhase.Conditions.Add({EBossPhaseConditionKind::HPBelowPercent, BOSS_RULES_TEST_OLD_HP_LINE});
		Data->Phases.Add(OldPhase);
		FBossTriggerCondition OldCondition;
		OldCondition.Kind = EBossTriggerConditionKind::PartyMember;
		OldCondition.Character = Lord;
		FBossTrigger OldTrigger;
		OldTrigger.Conditions.Add(OldCondition);
		OldTrigger.Results.Add(EBossTriggerResult::Weaken);
		Data->Triggers.Add(OldTrigger);

		const TArray<FBossRule> Rules = BossRules::GetRules(*Data);
		if (!TestEqual(TEXT("one rule each"), Rules.Num(), 2)) return;
		TestTrue(TEXT("the phase is a state-change NextPhase rule"),
				 Rules[0].When == EBossRuleWhen::StateChange && Rules[0].Results == TArray<EBossRuleResult>{EBossRuleResult::NextPhase});
		TestTrue(TEXT("the trigger is a fight-start Weaken rule"),
				 Rules[1].When == EBossRuleWhen::FightStart && Rules[1].Results == TArray<EBossRuleResult>{EBossRuleResult::Weaken});

		UBossPhaseComponent *Boss = SpawnBoss();
		SetBossHPPercent(BOSS_RULES_TEST_HP_LINE - BOSS_RULES_TEST_MARGIN);
		TestEqual(TEXT("the old phase is reached through its rule"), Boss->GetReachedPhaseCount(), 1);
	});

	It("fires an authored state-change rule once per fight", [this]()
	{
		Data->Phases.Add(ChangesOnly());
		Data->Phases.Add(ChangesOnly());
		Data->Rules.Add(Rule(EBossRuleWhen::StateChange, Condition(EWoRConditionSubject::BossHPAtMostPercent, BOSS_RULES_TEST_HP_LINE),
							 EBossRuleResult::NextPhase));
		UBossPhaseComponent *Boss = SpawnBoss();
		SetBossHPPercent(BOSS_RULES_TEST_HP_LINE - BOSS_RULES_TEST_MARGIN);
		SetBossHPPercent(BOSS_RULES_TEST_HP_LINE - BOSS_RULES_TEST_MARGIN - BOSS_RULES_TEST_MARGIN);
		TestEqual(TEXT("one phase, not two"), Boss->GetReachedPhaseCount(), 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
