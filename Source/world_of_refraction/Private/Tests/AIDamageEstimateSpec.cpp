// AIDamageEstimateSpec.cpp
// The AI's damage estimate equals what the executor's hit takes off the target (B-029): the hit is
// built from UActionExecutor::PreviewActionDamage (what execution sends into the defense window) and
// applied through ApplyHit with no defense roll and no crit; the estimate's expected-crit factor is
// folded onto that HP loss. Single-hit skills only (a multi-hit skill splits and rounds per hit).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	/** Small, odd base so rounding before vs after defense gives different numbers. */
	constexpr int32 AI_ESTIMATE_TEST_BASE_DAMAGE = 7;
	constexpr int32 AI_ESTIMATE_TEST_DEFENSE_POINTS = 50;
	constexpr int32 AI_ESTIMATE_TEST_INFUSION_LEVEL = 1;
}

BEGIN_DEFINE_SPEC(FAIDamageEstimateSpec, "WoR.AI.DamageEstimate",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UAIDecisionManager *AI = nullptr;
UActionExecutor *Executor = nullptr;
UDamageCalculator *Calculator = nullptr;
AActor *Attacker = nullptr;
AActor *Target = nullptr;

/** HP the executor's single hit for Action takes off Target, crit off. */
int32 ExecutorHitDamage(const FAction &Action)
{
	const FActionStatModifiers ActionMods = Executor->ComputeActionStatModifiers(Action, Attacker);
	const FActionDamagePreview Preview = Executor->PreviewActionDamage(Attacker, Action, ActionMods);

	FActionHitInput Hit;
	Hit.Attacker = Attacker;
	Hit.Target = Target;
	Hit.ActionType = Action.ActionType;
	Hit.BaseDamage = Preview.FinalDamage;
	Hit.Element = Preview.Element;
	Hit.bCanCrit = false;
	Hit.InfusionLevel = Action.ActionType == EActionType::Spell ? Action.SpellInfusionLevel : Action.AbilityInfusionLevel;
	Hit.SelectedSource = Action.SelectedSource;
	Hit.ActionMods = ActionMods;

	UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	const int32 HPBefore = TargetComp->CurrentHP;
	Executor->ApplyHit(Hit);
	return HPBefore - TargetComp->CurrentHP;
}

/** The estimate's expected-crit factor for Attacker (no GuaranteedCrit window in these tests). */
int32 WithExpectedCrit(int32 Damage) const
{
	const float CritChance = Calculator->GetCriticalChance(Attacker);
	const float CritMult = Calculator->GetCritDamageMultiplier(Attacker);
	return FMath::RoundToInt(Damage * (1.0f + CritChance * (CritMult - 1.0f)));
}
END_DEFINE_SPEC(FAIDamageEstimateSpec)

void FAIDamageEstimateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Calculator = TestWorld->GetSubsystem<UDamageCalculator>();
		UCharacterData *DefenderData = FWoRTestWorld::NewCharacterData();
		DefenderData->Defense = AI_ESTIMATE_TEST_DEFENSE_POINTS;
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Target = TestWorld->SpawnCombatant(DefenderData);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("estimates a spell at what the executor's hit deals", [this]()
	{
		if (!TestTrue(TEXT("setup"), AI && Executor && Calculator && Attacker && Target))
		{
			return;
		}
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseDamage = AI_ESTIMATE_TEST_BASE_DAMAGE;
		Spell->Tier = EItemTier::S_Tier;
		Spell->Element = ESpellElement::Generic;

		const int32 Estimate = AI->EstimateSpellDamage(Attacker, Target, Spell);

		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.Targets.Add(Target);
		TestEqual(TEXT("estimate equals the hit"), Estimate, WithExpectedCrit(ExecutorHitDamage(Action)));
	});

	It("estimates an infused ability at what the executor's hit deals", [this]()
	{
		if (!TestTrue(TEXT("setup"), AI && Executor && Calculator && Attacker && Target))
		{
			return;
		}
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseDamage = AI_ESTIMATE_TEST_BASE_DAMAGE;
		Ability->Tier = EItemTier::S_Tier;

		const int32 Estimate = AI->EstimateAbilityDamage(Attacker, Target, Ability, AI_ESTIMATE_TEST_INFUSION_LEVEL,
														 EInfusionSourceOption::Innate);

		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.AbilityInfusionLevel = AI_ESTIMATE_TEST_INFUSION_LEVEL;
		Action.SelectedSource = EInfusionSourceOption::Innate;
		Action.Targets.Add(Target);
		TestEqual(TEXT("estimate equals the hit"), Estimate, WithExpectedCrit(ExecutorHitDamage(Action)));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
