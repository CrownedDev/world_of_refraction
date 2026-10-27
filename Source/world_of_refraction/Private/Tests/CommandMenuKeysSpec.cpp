// CommandMenuKeysSpec.cpp
// B-093: the Defend and infusion keys press the menu's own buttons, so they work exactly where
// the buttons are: Defend from the action menu on your turn, the infusion keys only in a target
// picker that offers them. Driven through a real orchestrator on two player actors.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Loadout/LoadoutComponent.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"

namespace
{
	constexpr int32 MENU_KEYS_SETTLE_FRAMES = 3;

	void MenuKeysAddLoadout(AActor *Actor)
	{
		ULoadoutComponent *Loadout = NewObject<ULoadoutComponent>(Actor);
		Actor->AddInstanceComponent(Loadout);
		Loadout->RegisterComponent();
	}
}

BEGIN_DEFINE_SPEC(FCommandMenuKeysSpec, "WoR.UI.CommandMenu.Keys",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UCombatCommandMenuSubsystem *Menu = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Both sides player-controlled, so each turn opens the menu. False if setup failed. */
	bool StartDuel()
	{
		if (!Orchestrator || !Menu)
		{
			return false;
		}
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		MenuKeysAddLoadout(Hero);
		MenuKeysAddLoadout(Foe);
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe});
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);
		return TestTrue(TEXT("Hero's menu is open on turn 1"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero);
	}
END_DEFINE_SPEC(FCommandMenuKeysSpec)

void FCommandMenuKeysSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
		TestNotNull(TEXT("Orchestrator"), Orchestrator);
		TestNotNull(TEXT("CommandMenu"), Menu);
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
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	It("defends from the main menu and ends the turn", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		TestTrue(TEXT("Defend key pressed the button"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestTrue(TEXT("The turn moved on"), Orchestrator->GetCurrentActor() == Foe);
	});

	It("defends from a submenu too", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Abilities;
		Abilities.Category = EPieMenuCategory::Abilities;
		Menu->HandleSelection(Abilities);
		TestTrue(TEXT("Defend key pressed"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestTrue(TEXT("The turn moved on"), Orchestrator->GetCurrentActor() == Foe);
	});

	It("cycles the infusion level in a target picker that offers it", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		UInfusionVFXComponent *VFX = Hero->FindComponentByClass<UInfusionVFXComponent>();
		if (!VFX)
		{
			VFX = NewObject<UInfusionVFXComponent>(Hero);
			Hero->AddInstanceComponent(VFX);
			VFX->RegisterComponent();
		}
		FPieMenuButtonData Ability;
		Ability.Category = EPieMenuCategory::Ability;
		Ability.DataReference = NewObject<UAbilityData>(GetTransientPackage());
		Menu->HandleSelection(Ability);

		const int32 LevelBefore = VFX->GetCurrentInfusionLevel();
		TestTrue(TEXT("Infusion level key pressed"), Menu->PressMenuKey(EPieMenuCategory::CycleLevel));
		TestNotEqual(TEXT("Level changed"), VFX->GetCurrentInfusionLevel(), LevelBefore);
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("ignores the infusion keys outside a target picker", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		TestFalse(TEXT("Infusion source key"), Menu->PressMenuKey(EPieMenuCategory::CycleSource));
		TestFalse(TEXT("Infusion level key"), Menu->PressMenuKey(EPieMenuCategory::CycleLevel));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("Menu still open"), Menu->IsOpen());
	});

	It("ignores the Defend key while picking a target", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Ability;
		Ability.Category = EPieMenuCategory::Ability;
		Ability.DataReference = NewObject<UAbilityData>(GetTransientPackage());
		Menu->HandleSelection(Ability);

		TestFalse(TEXT("Defend key"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("Still picking"), Menu->IsOpen());
	});

	It("does nothing with the menu closed", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		Menu->Close();
		TestFalse(TEXT("Defend key"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("lists the three menu keys", [this]()
	{
		const FString Keys = ACombatPlayerController::GetMenuKeysString();
		TestTrue(TEXT("Defend"), Keys.Contains(TEXT("IA_Combat_MenuDefend")));
		TestTrue(TEXT("Source"), Keys.Contains(TEXT("IA_Combat_MenuInfusionSource")));
		TestTrue(TEXT("Level"), Keys.Contains(TEXT("IA_Combat_MenuInfusionLevel")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
