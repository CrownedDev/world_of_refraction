// SilenceSealSpec.cpp
// B-148 / D-038: Silenced no longer blocks spells; it seals SILENCE_SEALED_EP_FRACTION of max EP
// while it lasts, and every EP spend validates and pays against the rest. 0-cost actions
// (basic attack, Defend) still work; the seal lifts when Silenced ends.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 SILENCE_TEST_MAX_EP = 100;
	constexpr int32 SILENCE_TEST_CURRENT_EP = 80;
	constexpr int32 SILENCE_TEST_SPENDABLE_EP = 30; // 80 − 50% of 100
}

BEGIN_DEFINE_SPEC(FSilenceSealSpec, "WoR.Effects.SilenceSeal",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UActionExecutor *Executor = nullptr;

	/** A combatant at SILENCE_TEST_CURRENT_EP of SILENCE_TEST_MAX_EP, Silenced by a Darkness break. */
	AActor *SpawnSilenced()
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
		{
			Comp->MaxEP = SILENCE_TEST_MAX_EP;
			Comp->CurrentEP = SILENCE_TEST_CURRENT_EP;
		}
		AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		EffectManager->ApplyTriggeredSkillEffect(Attacker, Actor, ESkillEffectType::Silenced, ESpellElement::Darkness);
		return Actor;
	}
END_DEFINE_SPEC(FSilenceSealSpec)

void FSilenceSealSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("ActionExecutor"), Executor);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		EffectManager = nullptr;
		Executor = nullptr;
	});

	It("seals half of max EP: 80 of 100 leaves 30 to spend, not 31", [this]()
	{
		if (!EffectManager || !Executor) return;
		AActor *Actor = SpawnSilenced();
		if (!TestTrue(TEXT("Silenced"), EffectManager->IsSilenced(Actor))) return;
		TestEqual(TEXT("Sealed EP"), EffectManager->GetSealedEP(Actor),
				  FMath::RoundToInt(SILENCE_TEST_MAX_EP * CombatConstants::SILENCE_SEALED_EP_FRACTION));
		TestEqual(TEXT("Spendable EP"), EffectManager->GetSpendableEP(Actor), SILENCE_TEST_SPENDABLE_EP);
		TestEqual(TEXT("The executor validates against the same number"), Executor->GetSpendableEP(Actor), SILENCE_TEST_SPENDABLE_EP);
	});

	It("never goes below 0 spendable", [this]()
	{
		if (!EffectManager) return;
		AActor *Actor = SpawnSilenced();
		Actor->FindComponentByClass<UCharacterDataComponent>()->CurrentEP = SILENCE_TEST_SPENDABLE_EP;
		TestEqual(TEXT("All of it sealed"), EffectManager->GetSpendableEP(Actor), 0);
	});

	It("still allows Defend and a 0-cost ability with every point sealed", [this]()
	{
		if (!EffectManager || !Executor) return;
		AActor *Actor = SpawnSilenced();
		Actor->FindComponentByClass<UCharacterDataComponent>()->CurrentEP = 0;

		FAction Defend;
		Defend.ActionType = EActionType::Defend;
		TestTrue(TEXT("Defend allowed"), Executor->ValidateAction(Actor, Defend).bIsValid);

		FAction FreeAbility;
		FreeAbility.ActionType = EActionType::Ability;
		FreeAbility.SkillData = NewObject<UAbilityData>(GetTransientPackage());
		FreeAbility.Targets.Add(Actor);
		const FActionValidationResult Result = Executor->ValidateAction(Actor, FreeAbility);
		TestTrue(FString::Printf(TEXT("0-cost ability allowed (%s)"), *Result.ErrorMessage), Result.bIsValid);
	});

	It("lifts the seal when Silenced ends", [this]()
	{
		if (!EffectManager) return;
		AActor *Actor = SpawnSilenced();
		EffectManager->ProcessStartOfTurnEffects(Actor);
		EffectManager->ProcessEndOfTurnEffects(Actor);
		EffectManager->ProcessStartOfTurnEffects(Actor);
		TestFalse(TEXT("Silenced ended"), EffectManager->IsSilenced(Actor));
		TestEqual(TEXT("All EP spendable again"), EffectManager->GetSpendableEP(Actor), SILENCE_TEST_CURRENT_EP);
	});
}

#endif
