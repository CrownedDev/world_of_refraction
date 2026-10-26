// StunRuleSpec.cpp
// D-001 stun rule (B-061): UActionExecutor::IsActionAllowedWhileStunned — only Defend and
// the loadout's current basic attack get through.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

BEGIN_DEFINE_SPEC(FStunRuleSpec, "WoR.Combat.StunRule",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UAbilityData *CurrentAttack = nullptr;
	UAbilityData *OtherAttack = nullptr;
	UAbilityData *PlainAbility = nullptr;

	static FAction MakeAction(EActionType Type, USkillDataBase *Skill = nullptr)
	{
		FAction Action;
		Action.ActionType = Type;
		Action.SkillData = Skill;
		return Action;
	}

	static UAbilityData *NewAbility(bool bIsAttack)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->bIsAttack = bIsAttack;
		return Ability;
	}
END_DEFINE_SPEC(FStunRuleSpec)

void FStunRuleSpec::Define()
{
	BeforeEach([this]()
	{
		CurrentAttack = NewAbility(true);
		OtherAttack = NewAbility(true);
		PlainAbility = NewAbility(false);
	});

	It("allows Defend, with or without a weapon", [this]()
	{
		TestTrue(TEXT("Defend, armed"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Defend), CurrentAttack));
		TestTrue(TEXT("Defend, unarmed"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Defend), nullptr));
	});

	It("allows the loadout's current basic attack", [this]()
	{
		TestTrue(TEXT("Current attack"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Ability, CurrentAttack), CurrentAttack));
	});

	It("refuses any other attack-flagged skill", [this]()
	{
		TestFalse(TEXT("Other attack"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Ability, OtherAttack), CurrentAttack));
		TestFalse(TEXT("Attack while unarmed"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Ability, CurrentAttack), nullptr));
	});

	It("refuses an infused basic attack (D-023 A)", [this]()
	{
		for (int32 Level : {1, 2})
		{
			FAction Infused = MakeAction(EActionType::Ability, CurrentAttack);
			Infused.AbilityInfusionLevel = Level;
			TestFalse(FString::Printf(TEXT("Current attack at infusion level %d"), Level),
					  UActionExecutor::IsActionAllowedWhileStunned(Infused, CurrentAttack));
		}
	});

	It("refuses abilities, spells, items, weapon switch and flee", [this]()
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		FAction SpellAction = MakeAction(EActionType::Spell);
		SpellAction.SpellData = Spell;

		TestFalse(TEXT("Ability"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Ability, PlainAbility), PlainAbility));
		TestFalse(TEXT("Spell"), UActionExecutor::IsActionAllowedWhileStunned(SpellAction, CurrentAttack));
		TestFalse(TEXT("Item"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Item), CurrentAttack));
		TestFalse(TEXT("Switch weapon"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::SwitchWeapon), CurrentAttack));
		TestFalse(TEXT("Flee"), UActionExecutor::IsActionAllowedWhileStunned(MakeAction(EActionType::Flee), CurrentAttack));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
