// CombatResetSpec.cpp
// Nothing from one fight survives into the next (B-030): leave a defense window open, an infusion
// charging, a status bar part-filled and a turn-order preview built, force the fight to end, and every per-fight subsystem is
// clean before the second fight starts.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Infusion/InfusionChargeManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr int32 COMBAT_RESET_TEST_PREVIEW_TURNS = 3;
	constexpr float COMBAT_RESET_TEST_ATTACK_SIZE = 1.0f;
	constexpr int32 COMBAT_RESET_TEST_DAMAGE = 10;
	/** Small enough not to fill the bar and trigger a status. */
	constexpr float COMBAT_RESET_TEST_BUILDUP = 1.0f;
}

BEGIN_DEFINE_SPEC(FCombatResetSpec, "WoR.Combat.ResetBetweenFights",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
ACombatOrchestrator *Orchestrator = nullptr;
UTurnManager *TurnManager = nullptr;
UDefenseSystem *Defense = nullptr;
UInfusionChargeManager *Infusion = nullptr;
USkillEffectManager *Effects = nullptr;
UStatusBuildupManager *Buildup = nullptr;
AActor *Hero = nullptr;
AActor *Foe = nullptr;

/** No trace of a fight left on any per-fight subsystem. */
void ExpectClean(const FString &When)
{
	TestFalse(*(When + TEXT(": defense window closed")), Defense->IsDefenseWindowOpen(Foe));
	TestFalse(*(When + TEXT(": infusion not charging")), Infusion->IsCharging());
	TestEqual(*(When + TEXT(": no effects on the hero")), Effects->GetEffectCount(Hero), 0);
	TestEqual(*(When + TEXT(": no effects on the foe")), Effects->GetEffectCount(Foe), 0);
	TestEqual(*(When + TEXT(": hero status bar empty")), Buildup->GetStatusBarBuildup(Hero), 0.0f);
}
END_DEFINE_SPEC(FCombatResetSpec)

void FCombatResetSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		Defense = TestWorld->GetSubsystem<UDefenseSystem>();
		Infusion = TestWorld->GetSubsystem<UInfusionChargeManager>();
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("leaves nothing from a forced-ended fight for the next one", [this]()
	{
		if (!TestTrue(TEXT("setup"), Orchestrator && TurnManager && Defense && Infusion && Effects && Buildup && Hero && Foe))
		{
			return;
		}

		Orchestrator->StartCombat({Hero}, {Foe});
		TestTrue(TEXT("first fight running"), TurnManager->IsCombatActive());
		TestTrue(TEXT("turn preview built"), TurnManager->PreviewTurnOrder(COMBAT_RESET_TEST_PREVIEW_TURNS).Num() > 0);
		Defense->OpenDefenseWindow(Hero, Foe, COMBAT_RESET_TEST_ATTACK_SIZE, COMBAT_RESET_TEST_DAMAGE);
		TestTrue(TEXT("defense window open"), Defense->IsDefenseWindowOpen(Foe));
		TestTrue(TEXT("infusion charging"), Infusion->BeginCharge(Hero, EChargeInfusionType::Spell));
		Buildup->AddStatusBuildup(Foe, Hero, COMBAT_RESET_TEST_BUILDUP, ESpellElement::Fire, EPhysicalDamageType::None);
		TestTrue(TEXT("hero status bar part-filled"), Buildup->GetStatusBarBuildup(Hero) > 0.0f);

		Orchestrator->ForceEndCombat(ECombatState::Victory);
		TestFalse(TEXT("first fight over"), TurnManager->IsCombatActive());
		TestEqual(TEXT("no turn preview after the fight"), TurnManager->PreviewTurnOrder(COMBAT_RESET_TEST_PREVIEW_TURNS).Num(), 0);
		ExpectClean(TEXT("between fights"));

		Orchestrator->StartCombat({Hero}, {Foe});
		TestTrue(TEXT("second fight running"), TurnManager->IsCombatActive());
		ExpectClean(TEXT("second fight start"));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
