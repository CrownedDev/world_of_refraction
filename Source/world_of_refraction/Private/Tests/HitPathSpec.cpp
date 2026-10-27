// HitPathSpec.cpp
// B-032 (Q-031): one hit path. ApplyDamage is a damage-only ApplyHit, so it deals the same damage and
// gets the same reflect; the typed reflects (physical / spell) follow the hit's action type.
// B-032c (Q-032): reflect returns a share of the HP actually lost; DoubleHit skips a killed target.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/DamageCalculator.h"
#include "ElementDamageRecorder.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 HIT_PATH_TEST_BASE = 100;
	constexpr float HIT_PATH_TEST_REFLECT_PERCENT = 50.0f;
	constexpr int32 HIT_PATH_TEST_REFLECT_TURNS = 3;
	constexpr int32 HIT_PATH_TEST_TOUGH_HP = 10000; // no test hit overkills
	constexpr int32 HIT_PATH_TEST_FRAIL_HP = 10;	   // any test hit overkills
}

BEGIN_DEFINE_SPEC(FHitPathSpec, "WoR.Combat.HitPath",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UActionExecutor *Executor = nullptr;
AActor *Attacker = nullptr;

FActionHitInput DamageOnlyHit(AActor *Target, EActionType ActionType) const
{
	FActionHitInput Hit;
	Hit.Attacker = Attacker;
	Hit.Target = Target;
	Hit.ActionType = ActionType;
	Hit.BaseDamage = HIT_PATH_TEST_BASE;
	Hit.bCanCrit = false;
	return Hit;
}

AActor *SpawnWithHP(int32 HP)
{
	AActor *Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	UCharacterDataComponent *Comp = Target->FindComponentByClass<UCharacterDataComponent>();
	Comp->MaxHP = HP;
	Comp->CurrentHP = HP;
	return Target;
}

AActor *SpawnTough()
{
	return SpawnWithHP(HIT_PATH_TEST_TOUGH_HP);
}

AActor *SpawnWithReflect(ESkillEffectType ReflectType, int32 HP = HIT_PATH_TEST_TOUGH_HP)
{
	AActor *Target = SpawnWithHP(HP);
	TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(
		Target,
		FActiveSkillEffect::CreateBuff(TEXT("Test reflect"), 0, ReflectType, HIT_PATH_TEST_REFLECT_PERCENT,
									   HIT_PATH_TEST_REFLECT_TURNS),
		Target);
	return Target;
}

int32 AttackerHP() const
{
	return Attacker->FindComponentByClass<UCharacterDataComponent>()->CurrentHP;
}

int32 ReflectedShare(int32 Dealt) const
{
	return FMath::RoundToInt(Dealt * HIT_PATH_TEST_REFLECT_PERCENT / 100.0f);
}
END_DEFINE_SPEC(FHitPathSpec)

void FHitPathSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Executor = nullptr;
		Attacker = nullptr;
	});

	It("ApplyDamage deals the damage calculator's number, as before the merge", [this]()
	{
		UDamageCalculator *Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!TestTrue(TEXT("setup"), Executor && Attacker && Calculator))
		{
			return;
		}
		AActor *Target = SpawnTough();
		FDamageCalculationInput Input;
		Input.BaseDamage = HIT_PATH_TEST_BASE;
		Input.Element = ESpellElement::Fire;
		Input.bCanCrit = false;
		const int32 Expected = Calculator->CalculateDamage(Attacker, Target, Input).FinalDamage;
		TestTrue(TEXT("a number to compare"), Expected > 0);
		TestEqual(TEXT("ApplyDamage"),
				  Executor->ApplyDamage(Attacker, Target, HIT_PATH_TEST_BASE, ESpellElement::Fire, false).DamageDealt, Expected);
	});

	It("ApplyDamage is reflected like any hit", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && Attacker))
		{
			return;
		}
		AActor *Target = SpawnWithReflect(ESkillEffectType::DamageReflect);
		const int32 HPBefore = AttackerHP();
		const int32 Dealt = Executor->ApplyDamage(Attacker, Target, HIT_PATH_TEST_BASE, ESpellElement::None, false).DamageDealt;
		TestTrue(TEXT("a reflect to measure"), ReflectedShare(Dealt) > 0);
		TestEqual(TEXT("attacker lost the reflected share"), HPBefore - AttackerHP(), ReflectedShare(Dealt));
	});

	It("spell reflect hits spells only, physical reflect abilities only", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && Attacker))
		{
			return;
		}
		// A fresh target per hit, so one mirror's reflect never counts twice.
		int32 HPBefore = AttackerHP();
		Executor->ApplyHit(DamageOnlyHit(SpawnWithReflect(ESkillEffectType::ReflectSpellDamage), EActionType::Ability));
		TestEqual(TEXT("an ability isn't spell-reflected"), AttackerHP(), HPBefore);
		const int32 SpellDealt =
			Executor->ApplyHit(DamageOnlyHit(SpawnWithReflect(ESkillEffectType::ReflectSpellDamage), EActionType::Spell)).DamageDealt;
		TestEqual(TEXT("a spell is"), HPBefore - AttackerHP(), ReflectedShare(SpellDealt));

		HPBefore = AttackerHP();
		Executor->ApplyHit(DamageOnlyHit(SpawnWithReflect(ESkillEffectType::ReflectPhysicalDamage), EActionType::Spell));
		TestEqual(TEXT("a spell isn't physical-reflected"), AttackerHP(), HPBefore);
		const int32 AbilityDealt =
			Executor->ApplyHit(DamageOnlyHit(SpawnWithReflect(ESkillEffectType::ReflectPhysicalDamage), EActionType::Ability)).DamageDealt;
		TestEqual(TEXT("an ability is"), HPBefore - AttackerHP(), ReflectedShare(AbilityDealt));
	});

	It("an overkilling hit reflects a share of the HP the target lost (Q-032)", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && Attacker))
		{
			return;
		}
		const int32 HPBefore = AttackerHP();
		const FCombatHitResult Hit =
			Executor->ApplyHit(DamageOnlyHit(SpawnWithReflect(ESkillEffectType::DamageReflect, HIT_PATH_TEST_FRAIL_HP), EActionType::None));
		TestTrue(TEXT("the hit killed"), Hit.bTargetDied);
		TestEqual(TEXT("lost only its HP"), Hit.DamageDealt, HIT_PATH_TEST_FRAIL_HP);
		TestEqual(TEXT("reflect is a share of that"), HPBefore - AttackerHP(), ReflectedShare(HIT_PATH_TEST_FRAIL_HP));
	});

	It("DoubleHit skips its second hit when the first one killed (Q-032)", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && Attacker))
		{
			return;
		}
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Effects->ApplyEffect(Attacker,
							 FActiveSkillEffect::CreateBuff(TEXT("Test double hit"), 0, ESkillEffectType::DoubleHit, 0.0f,
															HIT_PATH_TEST_REFLECT_TURNS),
							 Attacker);
		UElementDamageRecorder *Recorder = NewObject<UElementDamageRecorder>();
		Recorder->Listen(Executor);
		Executor->ApplyHit(DamageOnlyHit(SpawnWithHP(HIT_PATH_TEST_FRAIL_HP), EActionType::None));
		TestEqual(TEXT("one hit on the killed target"), Recorder->Entries.Num(), 1);
		Executor->ApplyHit(DamageOnlyHit(SpawnTough(), EActionType::None));
		TestEqual(TEXT("a survivor still takes the second hit"), Recorder->Entries.Num(), 3);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
