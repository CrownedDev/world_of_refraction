// AIBuildupEstimateSpec.cpp
// B-114: the AI's status-buildup estimate reads the same authored number the live hit uses.
// A live hit spreads the authored buildup over its hits (shares sum to it) and AddStatusBuildup
// scales it by the attacker's Status Multiplier, so before resistance an action builds
// authored × Status Multiplier — not 5 per hit (abilities) or authored per hit (spells).
// B-200: the multiplier is the live one (gear, StatusStone, buffs), read by
// UAIDecisionManager::EstimateBaseStatusBuildup.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr int32 AI_BUILDUP_TEST_AUTHORED = 20;
	constexpr int32 AI_BUILDUP_TEST_HITS = 3;
	constexpr float AI_BUILDUP_TEST_TOLERANCE = 1.0f; // the estimate rounds to whole points
	constexpr int32 AI_BUILDUP_TEST_STATUS_MULT_POINTS = 10; // a Status Multiplier above 1, so it is tested
	constexpr int32 AI_BUILDUP_TEST_BUFF_ID = 200000;
	constexpr int32 AI_BUILDUP_TEST_BUFF_TURNS = 3;
	constexpr float AI_BUILDUP_TEST_BUFF_PERCENT = 50.0f;
}

BEGIN_DEFINE_SPEC(FAIBuildupEstimateSpec, "WoR.AI.BuildupEstimate",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UStatusBuildupManager *Buildup = nullptr;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	}

	/** An attacker whose Status Multiplier is above 1. */
	AActor *SpawnSource()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->StatusMultiplier = AI_BUILDUP_TEST_STATUS_MULT_POINTS;
		return TestWorld->SpawnCombatant(Data);
	}

	/** What the live path adds for an action authored at AI_BUILDUP_TEST_AUTHORED, before the target's resistance. */
	float LiveBuildupBeforeResistance(AActor *Source)
	{
		AActor *Target = Spawn();
		Buildup->AddStatusBuildup(Source, Target, AI_BUILDUP_TEST_AUTHORED, ESpellElement::None, EPhysicalDamageType::None);
		const float Kept = 1.0f - Buildup->GetTotalStatusResistance(Target, ESpellElement::None, EPhysicalDamageType::None);
		return Kept > KINDA_SMALL_NUMBER ? Buildup->GetStatusBarBuildup(Target) / Kept : 0.0f;
	}

	template <class TSkill>
	TSkill *NewSkill()
	{
		TSkill *Skill = NewObject<TSkill>(GetTransientPackage());
		Skill->StatusBuildup = AI_BUILDUP_TEST_AUTHORED;
		Skill->HitCount = AI_BUILDUP_TEST_HITS;
		return Skill;
	}
END_DEFINE_SPEC(FAIBuildupEstimateSpec)

void FAIBuildupEstimateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
		TestNotNull(TEXT("StatusBuildupManager"), Buildup);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Buildup = nullptr;
	});

	It("for a multi-hit ability equals the buildup the live hit adds", [this]()
	{
		if (!Buildup) return;
		AActor *Source = SpawnSource();
		UCharacterDataComponent *Comp = Source->FindComponentByClass<UCharacterDataComponent>();
		if (!TestTrue(TEXT("Source has character data"), Comp && Comp->CharacterData)) return;
		TestTrue(TEXT("Status Multiplier above 1"), Comp->CharacterData->CalculateStatusMultiplier() > 1.0f);
		TestEqual(TEXT("Ability estimate"), UAIDecisionManager::EstimateBaseStatusBuildup(Comp, NewSkill<UAbilityData>()),
				  LiveBuildupBeforeResistance(Source), AI_BUILDUP_TEST_TOLERANCE);
	});

	It("for a buffed attacker reads the live Status Multiplier (B-200)", [this]()
	{
		if (!Buildup) return;
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("SkillEffectManager"), Effects)) return;
		AActor *Source = SpawnSource();
		UCharacterDataComponent *Comp = Source->FindComponentByClass<UCharacterDataComponent>();
		if (!TestTrue(TEXT("Source has character data"), Comp && Comp->CharacterData)) return;
		Effects->ApplyEffect(Source, FActiveSkillEffect::CreateBuff(TEXT("AI buildup estimate test"), AI_BUILDUP_TEST_BUFF_ID,
			ESkillEffectType::StatusMultiplierBuff, AI_BUILDUP_TEST_BUFF_PERCENT, AI_BUILDUP_TEST_BUFF_TURNS));
		TestTrue(TEXT("The buff raises the live multiplier"),
				 Comp->GetEffectiveStatusMultiplier() > Comp->CharacterData->CalculateStatusMultiplier());
		TestEqual(TEXT("Spell estimate"), UAIDecisionManager::EstimateBaseStatusBuildup(Comp, NewSkill<USpellData>()),
				  LiveBuildupBeforeResistance(Source), AI_BUILDUP_TEST_TOLERANCE);
		TestEqual(TEXT("Ability estimate"), UAIDecisionManager::EstimateBaseStatusBuildup(Comp, NewSkill<UAbilityData>()),
				  LiveBuildupBeforeResistance(Source), AI_BUILDUP_TEST_TOLERANCE);
	});

	It("for a multi-hit spell equals the buildup the live hit adds", [this]()
	{
		if (!Buildup) return;
		AActor *Source = SpawnSource();
		UCharacterDataComponent *Comp = Source->FindComponentByClass<UCharacterDataComponent>();
		if (!TestTrue(TEXT("Source has character data"), Comp && Comp->CharacterData)) return;
		TestTrue(TEXT("Status Multiplier above 1"), Comp->CharacterData->CalculateStatusMultiplier() > 1.0f);
		TestEqual(TEXT("Spell estimate"), UAIDecisionManager::EstimateBaseStatusBuildup(Comp, NewSkill<USpellData>()),
				  LiveBuildupBeforeResistance(Source), AI_BUILDUP_TEST_TOLERANCE);
	});
}

#endif
