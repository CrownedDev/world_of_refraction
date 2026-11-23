// CommandMenuKeysSpec.cpp
// B-093: the Defend and infusion keys press the menu's own buttons, so they work exactly where
// the buttons are: Defend from the main menu (D-258c, B-436g), the infusion keys only in a target
// picker that offers them. Driven through a real orchestrator on two player actors.
// B-170e: a main-menu End Turn press reaches the End Turn button, which shows unless the player hid it (Crown,
// 2026-11-12); End Turn has no bound key yet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Loadout/LoadoutComponent.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "InputCoreTypes.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Combat/Battle/BattleCommandMenuWidget.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Save/SaveSubsystem.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr int32 MENU_KEYS_SETTLE_FRAMES = 3;
	/** Infusion levels 0, 1, 2 (UInfusionVFXComponent::CycleInfusionLevel). */
	constexpr int32 MENU_KEYS_INFUSION_LEVELS = 3;

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

	/** B-436g: the duel with a Caster who has a weapon with an ability, and (bTwoKinds) an innate spell too. */
	bool StartSkilledDuel(bool bTwoKinds)
	{
		if (!Orchestrator || !Menu)
		{
			return false;
		}
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = NewObject<UAbilityData>(GetTransientPackage());
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Weapon);
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Loadout && Foe))
		{
			return false;
		}
		Hero = Loadout->GetOwner();
		MenuKeysAddLoadout(Foe);
		UInventoryComponent *Inventory = Hero->FindComponentByClass<UInventoryComponent>();
		if (!TestTrue(TEXT("a saved loadout"), Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return false;
		}
		FWeaponInventoryEntry Owned; // the fight keeps only gear the inventory owns (as AIActionPipsSpec)
		Owned.Weapon = Weapon;
		Inventory->Weapons.Add(Owned);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.bShowPrimary = true;
		Active.PrimaryWeapon.ReplaceSkill(0, NewObject<UAbilityData>(GetTransientPackage()));
		if (bTwoKinds)
		{
			Active.InnateSpells.Add(NewObject<USpellData>(GetTransientPackage()));
		}
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

	// D-258c (Crown) reverses B-093's "from a submenu too": RB is Defend on the main menu only.
	It("does not defend from a submenu (D-258c)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Abilities;
		Abilities.Category = EPieMenuCategory::Abilities;
		Menu->HandleSelection(Abilities);
		TestFalse(TEXT("In a submenu"), Menu->IsAtMainMenu());
		TestFalse(TEXT("Defend key does nothing"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
	});

	It("never defends when RB is pressed on a target screen (D-258c)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		if (!Hero->FindComponentByClass<UInfusionVFXComponent>())
		{
			FWoRTestWorld::AddComponent<UInfusionVFXComponent>(Hero);
		}
		FPieMenuButtonData Ability;
		Ability.Category = EPieMenuCategory::Ability;
		Ability.DataReference = NewObject<UAbilityData>(GetTransientPackage());
		Menu->HandleSelection(Ability);

		// One press of RB runs every action on it, in an order Enhanced Input picks: try both.
		TArray<EPieMenuCategory> OnRB;
		for (const FCombatMenuKey &Key : ACombatPlayerController::GetMenuKeys())
		{
			if (Key.Gamepad == EKeys::Gamepad_RightShoulder)
			{
				OnRB.Add(Key.Category);
			}
		}
		if (!TestEqual(TEXT("RB is two actions"), OnRB.Num(), 2))
		{
			return;
		}
		Menu->PressMenuKey(OnRB[0]);
		Menu->PressMenuKey(OnRB[1]);
		TestEqual(TEXT("one press: one level up"), Menu->GetPendingInfusionLevel(), 1);
		Menu->PressMenuKey(OnRB[1]);
		Menu->PressMenuKey(OnRB[0]);
		TestEqual(TEXT("the other order: one more"), Menu->GetPendingInfusionLevel(), 2);
		TestTrue(TEXT("no Defend: still Hero's turn, still picking"),
				 Orchestrator->GetCurrentActor() == Hero && Menu->IsOpen() && !Menu->IsAtMainMenu());
	});

	It("opens the only kind of skill at once, and Back returns to the main menu (D-258a)", [this]()
	{
		if (!StartSkilledDuel(/*bTwoKinds=*/false))
		{
			return;
		}
		TestTrue(TEXT("Skills key"), Menu->PressMenuKey(EPieMenuCategory::Skills));
		TestEqual(TEXT("straight into Abilities"), Menu->GetViewTitle().ToString(), FString(TEXT("Skills > Abilities")));
		TestTrue(TEXT("Back key"), Menu->PressMenuKey(EPieMenuCategory::Back));
		TestTrue(TEXT("the main menu"), Menu->IsAtMainMenu() && Menu->GetViewTitle().IsEmpty());
	});

	It("lists the kinds of skill by name only, and Back steps out one list at a time (D-258a, D-260)", [this]()
	{
		if (!StartSkilledDuel(/*bTwoKinds=*/true))
		{
			return;
		}
		UBattleCommandMenuWidget *View = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
		if (!TestNotNull(TEXT("menu widget"), View))
		{
			return;
		}
		View->Initialise();
		TestTrue(TEXT("Skills key"), Menu->PressMenuKey(EPieMenuCategory::Skills));
		TestEqual(TEXT("the kinds"), Menu->GetViewTitle().ToString(), FString(TEXT("Skills")));
		for (const FText &Label : View->GetRowLabels())
		{
			TestFalse(FString::Printf(TEXT("no count in %s"), *Label.ToString()),
					  Label.ToString().Contains(TEXT("spells")) || Label.ToString().Contains(TEXT(" abilities")));
		}
		View->Teardown();

		FPieMenuButtonData Kind;
		Kind.Category = EPieMenuCategory::Refractions;
		Menu->HandleSelection(Kind);
		TestEqual(TEXT("its schools"), Menu->GetViewTitle().ToString(), FString(TEXT("Skills > Refractions")));
		FPieMenuButtonData School;
		School.Category = EPieMenuCategory::School;
		Menu->HandleSelection(School); // a school's spells
		Menu->PressMenuKey(EPieMenuCategory::Back);
		TestEqual(TEXT("back to the schools"), Menu->GetViewTitle().ToString(), FString(TEXT("Skills > Refractions")));
		Menu->PressMenuKey(EPieMenuCategory::Back);
		TestEqual(TEXT("back to the kinds"), Menu->GetViewTitle().ToString(), FString(TEXT("Skills")));
		Menu->PressMenuKey(EPieMenuCategory::Back);
		TestTrue(TEXT("back to the main menu"), Menu->IsAtMainMenu());
	});

	It("ignores Back on the main menu, so the turn keeps its menu (D-258)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		TestFalse(TEXT("Back key"), Menu->PressMenuKey(EPieMenuCategory::Back));
		TestTrue(TEXT("still open, on the main menu"), Menu->IsOpen() && Menu->IsAtMainMenu());
	});

	It("steps back out of a target screen, named for its action (D-260)", [this]()
	{
		if (!StartSkilledDuel(/*bTwoKinds=*/false))
		{
			return;
		}
		TestTrue(TEXT("Attack key"), Menu->PressMenuKey(EPieMenuCategory::Attack));
		TestEqual(TEXT("the header"), Menu->GetViewTitle().ToString(), FString(TEXT("Attack > choose a target")));
		TestTrue(TEXT("Back key"), Menu->PressMenuKey(EPieMenuCategory::Back));
		TestTrue(TEXT("the main menu"), Menu->IsAtMainMenu());
	});

	It("maps the pad as D-258 says: A, X, Y, B Back, RB Defend", [this]()
	{
		const auto PadOf = [](EPieMenuCategory Category)
		{
			const FCombatMenuKey *Key = ACombatPlayerController::GetMenuKeys().FindByPredicate(
				[Category](const FCombatMenuKey &Entry) { return Entry.Category == Category; });
			return Key ? Key->Gamepad : EKeys::Invalid;
		};
		TestTrue(TEXT("A Attack"), PadOf(EPieMenuCategory::Attack) == EKeys::Gamepad_FaceButton_Bottom);
		TestTrue(TEXT("X Skills"), PadOf(EPieMenuCategory::Skills) == EKeys::Gamepad_FaceButton_Left);
		TestTrue(TEXT("Y Items"), PadOf(EPieMenuCategory::Items) == EKeys::Gamepad_FaceButton_Top);
		TestTrue(TEXT("B Back"), PadOf(EPieMenuCategory::Back) == EKeys::Gamepad_FaceButton_Right);
		TestTrue(TEXT("RB Defend"), PadOf(EPieMenuCategory::Defend) == EKeys::Gamepad_RightShoulder);
		TestFalse(TEXT("no Spells key"), ACombatPlayerController::GetMenuKeysString().Contains(TEXT("IA_Combat_MenuSpells")));
	});

	It("opens Position on 4 / D-pad Up and picks a row on 1-3 / A X Y, the other hints hidden (B-588, D-263)", [this]()
	{
		const FCombatMenuKey *PositionKey = ACombatPlayerController::GetMenuKeys().FindByPredicate(
			[](const FCombatMenuKey &Key) { return Key.Category == EPieMenuCategory::MoveRow; });
		TestTrue(TEXT("4 / D-pad Up"), PositionKey && PositionKey->Keyboard == EKeys::Four && PositionKey->Gamepad == EKeys::Gamepad_DPad_Up);
		if (!StartDuel())
		{
			return;
		}
		UBattleCommandMenuWidget *View = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
		if (!TestNotNull(TEXT("menu widget"), View))
		{
			return;
		}
		View->Initialise();
		ON_SCOPE_EXIT { View->Teardown(); };
		Menu->RefreshMenu();
		const auto CountHints = [View]()
		{
			TArray<FString> Lines;
			View->GetCommandMenuString().ParseIntoArrayLines(Lines);
			return Lines.FilterByPredicate([](const FString &Line) { return Line.Contains(TEXT("] hint ")); }).Num();
		};
		const FString Main = View->GetCommandMenuString();
		TestTrue(TEXT("one Position hint with its key"), Main.Contains(TEXT("hint [4] Position")));
		TestFalse(TEXT("no Step Forward / Back"), Main.Contains(TEXT("Step Forward")) || Main.Contains(TEXT("Step Back")));

		TestTrue(TEXT("Position key"), Menu->PressMenuKey(EPieMenuCategory::MoveRow));
		TestTrue(TEXT("the picker"), Menu->IsPositionPickerOpen());
		TestEqual(TEXT("its header"), Menu->GetViewTitle().ToString(), FString(TEXT("Position")));
		const FString Picker = View->GetCommandMenuString();
		TestTrue(TEXT("Front on 1"), Picker.Contains(TEXT("[1] Front Row")));
		TestTrue(TEXT("Middle marked, no key"), Picker.Contains(TEXT("Middle Row  (You are here.) (greyed)")));
		TestTrue(TEXT("Back on 3"), Picker.Contains(TEXT("[3] Back Row")));
		TestEqual(TEXT("only Back stays a hint"), CountHints(), 1);
		TestFalse(TEXT("the marked row's key does nothing"), Menu->PressMenuKey(EPieMenuCategory::Skills));

		TestTrue(TEXT("3 picks the back row"), Menu->PressMenuKey(EPieMenuCategory::Items));
		FCombatGridPosition Position;
		const UCombatGridSubsystem *Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
		TestTrue(TEXT("moved to the back"), Grid && Grid->GetActorPosition(Hero, Position) && Position.Row == ECombatRow::Back);
		TestTrue(TEXT("the main menu again, still the hero's turn"), Menu->IsAtMainMenu() && Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("Position greyed with why"), View->GetCommandMenuString().Contains(TEXT("Position  (Already moved this turn.) (greyed)")));
		TestFalse(TEXT("its key does nothing now"), Menu->PressMenuKey(EPieMenuCategory::MoveRow));
	});

	It("steps back out of the Position picker without moving (B-588)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Position;
		Position.Category = EPieMenuCategory::MoveRow; // DataIndex -1: the Position button
		Menu->HandleSelection(Position);
		TestTrue(TEXT("the picker"), Menu->IsPositionPickerOpen());
		TestTrue(TEXT("Back key"), Menu->PressMenuKey(EPieMenuCategory::Back));
		TestTrue(TEXT("the main menu"), Menu->IsAtMainMenu());
		FCombatGridPosition Where;
		const UCombatGridSubsystem *Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
		TestTrue(TEXT("not moved"), Grid && Grid->GetActorPosition(Hero, Where) && Where.Row == ECombatRow::Middle);
		TestTrue(TEXT("the move is still free"), Orchestrator->GetMoveToRowRefusedReason(Hero, ECombatRow::Front).IsEmpty());
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
		TestTrue(TEXT("Defend is a main-menu key (D-258c)"), CommandMenuRules::IsMainMenuKey(EPieMenuCategory::Defend));
		TestTrue(TEXT("Skills is a main-menu key (D-258a)"), CommandMenuRules::IsMainMenuKey(EPieMenuCategory::Skills));
	});

	It("ignores a category key whose button the menu doesn't offer (B-391)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		// The duel's bare loadout has no weapon and no skills, so both rows are greyed (D-260).
		TestFalse(TEXT("Attack key"), Menu->PressMenuKey(EPieMenuCategory::Attack));
		TestFalse(TEXT("Skills key"), Menu->PressMenuKey(EPieMenuCategory::Skills));
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
		TestFalse(TEXT("End Turn has no key yet (Crown, 2026-11-12)"), Keys.Contains(TEXT("IA_Combat_MenuEndTurn")));
	});

	It("has no End Turn button when the player hid it, and the turn goes on (B-170e, Crown 2026-11-12)", [this]()
	{
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestNotNull(TEXT("the save"), Save))
		{
			return;
		}
		const bool bHiddenBefore = Save->IsEndTurnButtonHidden();
		ON_SCOPE_EXIT { Save->SetEndTurnButtonHidden(bHiddenBefore); };
		Save->SetEndTurnButtonHidden(true);
		if (!StartDuel())
		{
			return;
		}
		TestFalse(TEXT("End Turn press finds no button"), Menu->PressMenuKey(EPieMenuCategory::EndTurn));
		TestTrue(TEXT("Still Hero's turn"), Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("Defend is still there"), Menu->PressMenuKey(EPieMenuCategory::Defend));
	});

	It("ends the turn from the End Turn button and banks the actions (B-170e)", [this]()
	{
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestNotNull(TEXT("the save"), Save))
		{
			return;
		}
		const bool bHiddenBefore = Save->IsEndTurnButtonHidden();
		ON_SCOPE_EXIT { Save->SetEndTurnButtonHidden(bHiddenBefore); };
		Save->SetEndTurnButtonHidden(false); // shown, the default, whatever a loaded slot held
		if (!StartDuel())
		{
			return;
		}
		const UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
		const int32 ActionsBefore = HeroData ? HeroData->CurrentActions : INDEX_NONE;
		TestTrue(TEXT("End Turn press reached the button"), Menu->PressMenuKey(EPieMenuCategory::EndTurn));
		TestTrue(TEXT("The turn moved on"), Orchestrator->GetCurrentActor() == Foe);
		TestEqual(TEXT("Nothing was spent: the actions stay banked"), HeroData ? HeroData->CurrentActions : INDEX_NONE, ActionsBefore);
	});

	It("ignores an End Turn press outside the main menu (B-170e)", [this]()
	{
		if (!StartDuel())
		{
			return;
		}
		FPieMenuButtonData Abilities;
		Abilities.Category = EPieMenuCategory::Abilities;
		Menu->HandleSelection(Abilities);
		TestFalse(TEXT("End Turn press in a submenu"), Menu->PressMenuKey(EPieMenuCategory::EndTurn));
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
									Key.Category == EPieMenuCategory::CycleLevel || Key.Category == EPieMenuCategory::Back ||
									CommandMenuRules::IsMainMenuKey(Key.Category);
			TestTrue(FString::Printf(TEXT("%s presses a category PressMenuKey knows"), *Key.ActionName.ToString()), bKnownRule);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
