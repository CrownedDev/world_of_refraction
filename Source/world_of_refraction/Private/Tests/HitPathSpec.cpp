// HitPathSpec.cpp
// B-032 (Q-031): one hit path. ApplyDamage is a damage-only ApplyHit, so it deals the same damage and
// gets the same reflect; the typed reflects (physical / spell) follow the hit's action type.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 HIT_PATH_TEST_BASE = 100;
	constexpr float HIT_PATH_TEST_REFLECT_PERCENT = 50.0f;
	constexpr int32 HIT_PATH_TEST_REFLECT_TURNS = 3;
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

AActor *SpawnWithReflect(ESkillEffectType ReflectType)
{
	AActor *Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
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

	It("ApplyDamage deals what a damage-only ApplyHit deals", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && Attacker))
		{
			return;
		}
		AActor *First = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Second = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const int32 ViaApplyDamage = Executor->ApplyDamage(Attacker, First, HIT_PATH_TEST_BASE, ESpellElement::None, false).DamageDealt;
		const int32 ViaApplyHit = Executor->ApplyHit(DamageOnlyHit(Second, EActionType::None)).DamageDealt;
		TestTrue(TEXT("dealt damage"), ViaApplyDamage > 0);
		TestEqual(TEXT("same damage"), ViaApplyDamage, ViaApplyHit);
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
		// A fresh target per hit: one test hit can take a default character's whole HP.
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
