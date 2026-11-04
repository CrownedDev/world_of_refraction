// EffectRemovalSpec.cpp
// B-350 (D-140): the removal paths only the manual ASkillEffectManagerTestActor exercised, as a headless spec:
// remove by type, remove every buff or every debuff (the counts), and remove everything a source applied.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 REMOVAL_TEST_FIRST_ID = 9801;
	constexpr int32 REMOVAL_TEST_DURATION_TURNS = 3;
	constexpr float REMOVAL_TEST_BUFF_PERCENT = 10.0f;
	constexpr float REMOVAL_TEST_DEBUFF_PERCENT = -10.0f;
}

BEGIN_DEFINE_SPEC(FEffectRemovalSpec, "WoR.Effects.Removal", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	AActor *Holder = nullptr;
	int32 NextID = REMOVAL_TEST_FIRST_ID;

	void Buff(ESkillEffectType Type, AActor *Source = nullptr)
	{
		Effects->ApplyEffect(Holder, FActiveSkillEffect::CreateBuff(TEXT("Removal test buff"), NextID++, Type, REMOVAL_TEST_BUFF_PERCENT,
																	REMOVAL_TEST_DURATION_TURNS), Source);
	}

	void Debuff(ESkillEffectType Type, AActor *Source = nullptr)
	{
		FActiveSkillEffect Effect;
		Effect.EffectName = TEXT("Removal test debuff");
		Effect.EffectID = NextID++;
		Effect.EffectType = Type;
		Effect.EffectValue = REMOVAL_TEST_DEBUFF_PERCENT;
		Effect.RemainingTurns = REMOVAL_TEST_DURATION_TURNS;
		Effect.ProcessTiming = ESkillEffectTiming::Persistent;
		Effects->ApplyEffect(Holder, Effect, Source);
	}
END_DEFINE_SPEC(FEffectRemovalSpec)

void FEffectRemovalSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Holder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		NextID = REMOVAL_TEST_FIRST_ID;
		if (!TestNotNull(TEXT("SkillEffectManager"), Effects) || !TestNotNull(TEXT("Holder"), Holder))
		{
			Effects = nullptr;
		}
	});

	AfterEach([this]()
	{
		Effects = nullptr;
		Holder = nullptr;
		TestWorld.Reset();
	});

	It("removes every effect of one type and keeps the rest", [this]()
	{
		if (!Effects) return;
		Buff(ESkillEffectType::DamageBuff);
		Buff(ESkillEffectType::DamageBuff);
		Buff(ESkillEffectType::SpeedBuff);
		TestEqual(TEXT("three effects"), Effects->GetEffectCount(Holder), 3);
		TestEqual(TEXT("two damage buffs removed"), Effects->RemoveEffectsByType(Holder, ESkillEffectType::DamageBuff), 2);
		TestEqual(TEXT("one left"), Effects->GetEffectCount(Holder), 1);
		TestTrue(TEXT("the speed buff stays"), Effects->HasEffectOfType(Holder, ESkillEffectType::SpeedBuff));
	});

	It("removes every buff and keeps the debuffs", [this]()
	{
		if (!Effects) return;
		Buff(ESkillEffectType::DamageBuff);
		Buff(ESkillEffectType::SpeedBuff);
		Debuff(ESkillEffectType::SpeedDebuff);
		TestEqual(TEXT("two buffs"), Effects->GetBuffCount(Holder), 2);
		TestEqual(TEXT("one debuff"), Effects->GetDebuffCount(Holder), 1);
		TestEqual(TEXT("two removed"), Effects->RemoveAllBuffs(Holder), 2);
		TestEqual(TEXT("no buffs"), Effects->GetBuffCount(Holder), 0);
		TestEqual(TEXT("the debuff stays"), Effects->GetDebuffCount(Holder), 1);
	});

	It("removes every debuff and keeps the buffs", [this]()
	{
		if (!Effects) return;
		Buff(ESkillEffectType::DamageBuff);
		Debuff(ESkillEffectType::SpeedDebuff);
		TestEqual(TEXT("one removed"), Effects->RemoveAllDebuffs(Holder), 1);
		TestEqual(TEXT("no debuffs"), Effects->GetDebuffCount(Holder), 0);
		TestEqual(TEXT("the buff stays"), Effects->GetBuffCount(Holder), 1);
	});

	It("removes everything one source applied", [this]()
	{
		if (!Effects) return;
		AActor *Caster = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Other = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("sources"), Caster && Other)) return;
		Buff(ESkillEffectType::DamageBuff, Caster);
		Debuff(ESkillEffectType::SpeedDebuff, Caster);
		Buff(ESkillEffectType::SpeedBuff, Other);
		TestEqual(TEXT("the caster's two removed"), Effects->RemoveEffectsBySource(Caster), 2);
		TestEqual(TEXT("one left"), Effects->GetEffectCount(Holder), 1);
		TestTrue(TEXT("the other source's buff stays"), Effects->HasEffectOfType(Holder, ESkillEffectType::SpeedBuff));
	});
}

#endif
