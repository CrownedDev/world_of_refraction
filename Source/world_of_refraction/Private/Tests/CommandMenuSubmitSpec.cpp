// CommandMenuSubmitSpec.cpp
// B-110: confirming a target must not shut a menu the player still needs. A refused pick
// re-requests the same turn (the menu reopens), while an action still running closes it.
// Driven through a real orchestrator on two player actors.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Definitions/AbilityData.h"
#include "Loadout/LoadoutComponent.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"

namespace
{
	constexpr int32 MENU_SUBMIT_STUN_ID = 9201;
	constexpr int32 MENU_SUBMIT_STUN_TURNS = 1;
	constexpr int32 MENU_SUBMIT_SETTLE_FRAMES = 3;

	/** The menu only opens for an actor with a loadout (OpenForActor). */
	void MenuSubmitAddLoadout(AActor *Actor)
	{
		ULoadoutComponent *Loadout = NewObject<ULoadoutComponent>(Actor);
		Actor->AddInstanceComponent(Loadout);
		Loadout->RegisterComponent();
	}
}

BEGIN_DEFINE_SPEC(FCommandMenuSubmitSpec, "WoR.UI.CommandMenu.Submit",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UCombatCommandMenuSubsystem *Menu = nullptr;
	USkillEffectManager *EffectManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Both sides player-controlled, so each turn opens the menu. False if setup failed. */
	bool StartDuel()
	{
		if (!Orchestrator || !Menu || !EffectManager)
		{
			return false;
		}
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		MenuSubmitAddLoadout(Hero);
		MenuSubmitAddLoadout(Foe);
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe});
		TestWorld->Tick(MENU_SUBMIT_SETTLE_FRAMES);
		return TestTrue(TEXT("Hero's menu is open on turn 1"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero);
	}

	/** Pick an instant (montage-less) ability, then confirm it on Target. */
	void PickAbilityOn(AActor *Target)
	{
		FPieMenuButtonData Ability;
		Ability.Category = EPieMenuCategory::Ability;
		Ability.DataReference = NewObject<UAbilityData>(GetTransientPackage());
		Menu->HandleSelection(Ability);

		FPieMenuButtonData Pick;
		Pick.Category = EPieMenuCategory::Target;
		Pick.DataReference = Target;
		Menu->HandleSelection(Pick);
	}
END_DEFINE_SPEC(FCommandMenuSubmitSpec)

void FCommandMenuSubmitSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("CommandMenu"), Menu);
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
	});

	AfterEach([this]()
	{
		if (Menu)
		{
			Menu->ClearCombatOrchestrator();
			Menu->Close();
		}
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		Menu = nullptr;
		EffectManager = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	It("keeps the reopened menu after a refused pick", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		// Stunned: an ability is refused (D-001 B), so the orchestrator re-requests Hero's turn.
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Stun"), MENU_SUBMIT_STUN_ID, ESkillEffectType::Stun, 0.0f, MENU_SUBMIT_STUN_TURNS), Foe);
		PickAbilityOn(Foe);

		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("Menu open again"), Menu->IsOpen());
		TestTrue(TEXT("For Hero"), Menu->GetCurrentActor() == Hero);
	});

	It("closes the menu while the confirmed action is still running", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		PickAbilityOn(Foe);

		TestTrue(TEXT("The ability is running (async)"), Orchestrator->IsWaitingForAsyncAction());
		TestTrue(TEXT("Still Hero's turn until it completes"), Orchestrator->GetCurrentActor() == Hero);
		TestFalse(TEXT("Menu closed"), Menu->IsOpen());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
