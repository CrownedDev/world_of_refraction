// FightSubsystemsSpec.cpp
// B-336 (D-129, D-064): the per-fight subsystems are world subsystems. Every one exists on a game world,
// WoRFight::Get finds the same one from the world and from its game instance, and two worlds (two PIE
// clients, two fights) never share one, so a fight in one world leaves the other untouched.
// (Nothing from a forced-ended fight reaching the next fight in the same world is WoR.Combat.ResetBetweenFights.)

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/WoRFightSubsystem.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Combat/Mechanics/WeatherStateManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Infusion/InfusionChargeManager.h"
#include "AI/AIDecisionManager.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"

namespace
{
	constexpr int32 FIGHT_SUBSYSTEMS_TEST_EFFECT_ID = 9601;
	constexpr float FIGHT_SUBSYSTEMS_TEST_BUFF = 10.0f;
	constexpr int32 FIGHT_SUBSYSTEMS_TEST_TURNS = 2;
	constexpr int32 FIGHT_SUBSYSTEMS_COUNT = 10;
}

BEGIN_DEFINE_SPEC(FFightSubsystemsSpec, "WoR.Combat.FightSubsystems",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> First;
TUniquePtr<FWoRTestWorld> Second;

/** Lives on World as a fight subsystem, and the game instance finds the same one. */
template <class TSubsystem>
void ExpectOnWorld(const FWoRTestWorld &TestWorld, const TCHAR *Name)
{
	UWorld *World = TestWorld.GetWorld();
	TSubsystem *FromWorld = WoRFight::Get<TSubsystem>(World);
	if (!TestNotNull(*FString::Printf(TEXT("%s on the world"), Name), FromWorld))
	{
		return;
	}
	TestTrue(*FString::Printf(TEXT("%s is a fight subsystem"), Name), FromWorld->IsA(UWoRFightSubsystem::StaticClass()));
	TestTrue(*FString::Printf(TEXT("%s from the game instance"), Name),
			 WoRFight::Get<TSubsystem>(World->GetGameInstance()) == FromWorld);
}
END_DEFINE_SPEC(FFightSubsystemsSpec)

void FFightSubsystemsSpec::Define()
{
	BeforeEach([this]()
	{
		First = MakeUnique<FWoRTestWorld>();
		Second = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("first world created"), First->Create());
		TestTrue(TEXT("second world created"), Second->Create());
	});

	AfterEach([this]()
	{
		Second.Reset();
		First.Reset();
	});

	It("puts every per-fight subsystem on the world", [this]()
	{
		ExpectOnWorld<UTurnManager>(*First, TEXT("TurnManager"));
		ExpectOnWorld<UActionExecutor>(*First, TEXT("ActionExecutor"));
		ExpectOnWorld<UDefenseSystem>(*First, TEXT("DefenseSystem"));
		ExpectOnWorld<UCombatGridSubsystem>(*First, TEXT("CombatGridSubsystem"));
		ExpectOnWorld<UWeatherStateManager>(*First, TEXT("WeatherStateManager"));
		ExpectOnWorld<USkillEffectManager>(*First, TEXT("SkillEffectManager"));
		ExpectOnWorld<UStatusBuildupManager>(*First, TEXT("StatusBuildupManager"));
		ExpectOnWorld<UInfusionChargeManager>(*First, TEXT("InfusionChargeManager"));
		ExpectOnWorld<UAIDecisionManager>(*First, TEXT("AIDecisionManager"));
		ExpectOnWorld<UCombatCommandMenuSubsystem>(*First, TEXT("CombatCommandMenuSubsystem"));
		TestEqual(TEXT("fight subsystem count"),
				  First->GetWorld()->GetSubsystemArrayCopy<UWoRFightSubsystem>().Num(), FIGHT_SUBSYSTEMS_COUNT);
	});

	It("gives each world its own", [this]()
	{
		TestTrue(TEXT("two turn managers"), First->GetSubsystem<UTurnManager>() != Second->GetSubsystem<UTurnManager>());
		TestTrue(TEXT("two effect managers"),
				 First->GetSubsystem<USkillEffectManager>() != Second->GetSubsystem<USkillEffectManager>());
		TestTrue(TEXT("two action executors"), First->GetSubsystem<UActionExecutor>() != Second->GetSubsystem<UActionExecutor>());
	});

	It("leaves the other world untouched by a fight", [this]()
	{
		ACombatOrchestrator *Orchestrator = First->GetWorld()->SpawnActor<ACombatOrchestrator>();
		AActor *Hero = First->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = First->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UTurnManager *FirstTurns = First->GetSubsystem<UTurnManager>();
		UTurnManager *SecondTurns = Second->GetSubsystem<UTurnManager>();
		USkillEffectManager *FirstEffects = First->GetSubsystem<USkillEffectManager>();
		USkillEffectManager *SecondEffects = Second->GetSubsystem<USkillEffectManager>();
		if (!TestTrue(TEXT("setup"), Orchestrator && Hero && Foe && FirstTurns && SecondTurns && FirstEffects && SecondEffects))
		{
			return;
		}

		Orchestrator->StartCombat({Hero}, {Foe});
		FirstEffects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Fight subsystems buff"), FIGHT_SUBSYSTEMS_TEST_EFFECT_ID,
																	   ESkillEffectType::HealingReceivedBuff, FIGHT_SUBSYSTEMS_TEST_BUFF,
																	   FIGHT_SUBSYSTEMS_TEST_TURNS),
								  Hero);

		TestTrue(TEXT("fight running in the first world"), FirstTurns->IsCombatActive());
		TestFalse(TEXT("no fight in the second world"), SecondTurns->IsCombatActive());
		TestEqual(TEXT("effect stored in the first world"), FirstEffects->GetEffectCount(Hero), 1);
		TestEqual(TEXT("second world holds no effect for it"), SecondEffects->GetEffectCount(Hero), 0);

		Orchestrator->ForceEndCombat(ECombatState::Victory);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
