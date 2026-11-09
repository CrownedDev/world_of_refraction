// CommandMenuKeysSpec.cpp
// B-093: the Defend and infusion keys press the menu's own buttons, so they work exactly where
// the buttons are: Defend from the action menu on your turn, the infusion keys only in a target
// picker that offers them. Driven through a real orchestrator on two player actors.
// B-170e: the End Turn key presses the End Turn button, which shows only with wor.Turns.ActionPips 1.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Loadout/LoadoutComponent.h"
#include "Character/CharacterDataComponent.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"

namespace
{
	constexpr int32 MENU_KEYS_SETTLE_FRAMES = 3;
	/** Infusion levels 0, 1, 2 (UInfusionVFXComponent::CycleInfusionLevel). */
	constexpr int32 MENU_KEYS_INFUSION_LEVELS = 3;
	/** B-170e: the action pips switch, as MultiActionTurnSpec. */
	const TCHAR *const MENU_KEYS_PIPS_SWITCH = TEXT("wor.Turns.ActionPips");
	constexpr int32 MENU_KEYS_PIPS_ON = 1;
	constexpr int32 MENU_KEYS_PIPS_OFF = 0;

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
	IConsoleVariable *PipsSwitch = nullptr;
	int32 PipsSwitchBefore = MENU_KEYS_PIPS_OFF;

	/** B-170e: set the action pips switch before StartDuel (the fight reads it at start); AfterEach restores it. */
	void SetPipsSwitch(int32 Value)
	{
		if (PipsSwitch)
		{
			PipsSwitch->Set(Value, ECVF_SetByConsole);
		}
	}

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
		PipsSwitch = IConsoleManager::Get().FindConsoleVariable(MENU_KEYS_PIPS_SWITCH);
		PipsSwitchBefore = PipsSwitch ? PipsSwitch->GetInt() : MENU_KEYS_PIPS_OFF;
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
		SetPipsSwitch(PipsSwitchBefore);
		PipsSwitch = nullptr;
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
		TestFalse(TEXT("In a submenu"), Menu->IsAtMainMenu());
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

		const int32 LevelBefore = Menu->GetPendingInfusionLevel();
		TestEqual(TEXT("The picker opens at L0"), LevelBefore, 0);
		TestTrue(TEXT("Infusion level key pressed"), Menu->PressMenuKey(EPieMenuCategory::CycleLevel));
		TestEqual(TEXT("Next level"), Menu->GetPendingInfusionLevel(), (LevelBefore + 1) % MENU_KEYS_INFUSION_LEVELS);
		TestEqual(TEXT("The VFX shows the pending level"), VFX->GetCurrentInfusionLevel(), Menu->GetPendingInfusionLevel());
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("wraps the pending infusion level and resets it when the picker closes (B-034)", [this]()
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

		for (int32 Press = 0; Press < MENU_KEYS_INFUSION_LEVELS; ++Press)
		{
			Menu->PressMenuKey(EPieMenuCategory::CycleLevel);
		}
		TestEqual(TEXT("A full cycle wraps back to L0"), Menu->GetPendingInfusionLevel(), 0);

		Menu->PressMenuKey(EPieMenuCategory::CycleLevel);
		TestEqual(TEXT("Back to L1"), Menu->GetPendingInfusionLevel(), 1);
		Menu->HandleBack();
		TestEqual(TEXT("Leaving the picker resets the level"), Menu->GetPendingInfusionLevel(), 0);
		TestEqual(TEXT("The VFX follows"), VFX->GetCurrentInfusionLevel(), 0);
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

	It("matches each category key to its class's buttons (B-391)", [this]()
	{
		TestTrue(TEXT("Skills key presses Abilities"),
				 CommandMenuRules::KeyPressesButton(EPieMenuCategory::Abilities, EPieMenuCategory::Abilities));
		TestTrue(TEXT("Skills key presses a weapon resonance"),
				 CommandMenuRules::KeyPressesButton(EPieMenuCategory::Abilities, EPieMenuCategory::ResonateWeapon));
		TestTrue(TEXT("Spells key presses a ring resonance"),
				 CommandMenuRules::KeyPressesButton(EPieMenuCategory::Refractions, EPieMenuCategory::ResonateRing));
		TestTrue(TEXT("Spells key presses a breakthrough"),
				 CommandMenuRules::KeyPressesButton(EPieMenuCategory::Refractions, EPieMenuCategory::Breakthrough));
		TestFalse(TEXT("Attack key never presses Items"),
				  CommandMenuRules::KeyPressesButton(EPieMenuCategory::Attack, EPieMenuCategory::Items));
		TestTrue(TEXT("Attack is a main-menu key"), CommandMenuRules::IsMainMenuKey(EPieMenuCategory::Attack));
		TestTrue(TEXT("End Turn is a main-menu key (B-170e)"), CommandMenuRules::IsMainMenuKey(EPieMenuCategory::EndTurn));
		TestFalse(TEXT("Defend keeps its own rule"), CommandMenuRules::IsMainMenuKey(EPieMenuCategory::Defend));
	});

	It("ignores a category key whose button the menu doesn't offer (B-391)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		// The duel's bare loadout has no weapon, so no Attack button and no abilities.
		TestFalse(TEXT("Attack key"), Menu->PressMenuKey(EPieMenuCategory::Attack));
		TestFalse(TEXT("Skills key"), Menu->PressMenuKey(EPieMenuCategory::Abilities));
		TestTrue(TEXT("Still on the main menu"), Menu->IsAtMainMenu());
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("ignores the category keys outside the main menu (B-391)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Ability;
		Ability.Category = EPieMenuCategory::Ability;
		Ability.DataReference = NewObject<UAbilityData>(GetTransientPackage());
		Menu->HandleSelection(Ability);

		TestFalse(TEXT("Items key"), Menu->PressMenuKey(EPieMenuCategory::Items));
		TestFalse(TEXT("Flee key"), Menu->PressMenuKey(EPieMenuCategory::Flee));
		TestTrue(TEXT("Still picking"), Menu->IsOpen() && !Menu->IsAtMainMenu());
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("lists the menu keys", [this]()
	{
		const FString Keys = ACombatPlayerController::GetMenuKeysString();
		TestTrue(TEXT("Defend"), Keys.Contains(TEXT("IA_Combat_MenuDefend")));
		TestTrue(TEXT("Source"), Keys.Contains(TEXT("IA_Combat_MenuInfusionSource")));
		TestTrue(TEXT("Level"), Keys.Contains(TEXT("IA_Combat_MenuInfusionLevel")));
		TestTrue(TEXT("Attack (B-391)"), Keys.Contains(TEXT("IA_Combat_MenuAttack")));
		TestTrue(TEXT("Skills (B-391)"), Keys.Contains(TEXT("IA_Combat_MenuSkills")));
		TestTrue(TEXT("End Turn (B-170e)"), Keys.Contains(TEXT("IA_Combat_MenuEndTurn")));
	});

	It("has no End Turn button with the action pips switch off (B-170e)", [this]()
	{
		if (!TestNotNull(TEXT("the switch exists"), PipsSwitch))
		{
			return;
		}
		SetPipsSwitch(MENU_KEYS_PIPS_OFF);
		if (!StartDuel())
		{
			return;
		}
		TestFalse(TEXT("End Turn key finds no button"), Menu->PressMenuKey(EPieMenuCategory::EndTurn));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("ends the turn from the End Turn button and banks the actions (B-170e)", [this]()
	{
		if (!TestNotNull(TEXT("the switch exists"), PipsSwitch))
		{
			return;
		}
		SetPipsSwitch(MENU_KEYS_PIPS_ON);
		if (!StartDuel() || !TestTrue(TEXT("an action-pips fight"), Orchestrator->IsActionPipsFight()))
		{
			return;
		}
		const UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
		const int32 ActionsBefore = HeroData ? HeroData->CurrentActions : INDEX_NONE;
		TestTrue(TEXT("End Turn key pressed the button"), Menu->PressMenuKey(EPieMenuCategory::EndTurn));
		TestTrue(TEXT("The turn moved on"), Orchestrator->GetCurrentActor() == Foe);
		TestEqual(TEXT("Nothing was spent: the actions stay banked"), HeroData ? HeroData->CurrentActions : INDEX_NONE, ActionsBefore);
	});

	It("ignores the End Turn key outside the main menu (B-170e)", [this]()
	{
		if (!TestNotNull(TEXT("the switch exists"), PipsSwitch))
		{
			return;
		}
		SetPipsSwitch(MENU_KEYS_PIPS_ON);
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Abilities;
		Abilities.Category = EPieMenuCategory::Abilities;
		Menu->HandleSelection(Abilities);
		TestFalse(TEXT("End Turn key in a submenu"), Menu->PressMenuKey(EPieMenuCategory::EndTurn));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("gives every menu key a unique action name and a keyboard key (D-063, B-391)", [this]()
	{
		TSet<FName> Names;
		for (const FCombatMenuKey &Key : ACombatPlayerController::GetMenuKeys())
		{
			TestFalse(FString::Printf(TEXT("%s is unique"), *Key.ActionName.ToString()), Names.Contains(Key.ActionName));
			Names.Add(Key.ActionName);
			TestTrue(FString::Printf(TEXT("%s has a keyboard key"), *Key.ActionName.ToString()), Key.Keyboard.IsValid());
			const bool bKnownRule = Key.Category == EPieMenuCategory::Defend || Key.Category == EPieMenuCategory::CycleSource ||
									Key.Category == EPieMenuCategory::CycleLevel || CommandMenuRules::IsMainMenuKey(Key.Category);
			TestTrue(FString::Printf(TEXT("%s presses a category PressMenuKey knows"), *Key.ActionName.ToString()), bKnownRule);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
