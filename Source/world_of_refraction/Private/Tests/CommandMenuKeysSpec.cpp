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
#include "GameFramework/Pawn.h"
#include "Infusion/ClassColors.h"
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

	/** B-592: a Generic with a Sword and a Hammer (the Sword in hand), owned, with an infusion VFX component. */
	bool StartTwoWeaponDuel()
	{
		if (!Orchestrator || !Menu)
		{
			return false;
		}
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Sword");
		UWeaponData *Hammer = NewObject<UWeaponData>(GetTransientPackage());
		Hammer->Name = TEXT("Hammer");
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Sword);
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
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.bShowPrimary = true;
		Active.PrimaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		Active.SecondarySlotType = ESecondarySlotType::Weapon;
		Active.SecondaryWeapon.WeaponEntry.Weapon = Hammer;
		Active.SecondaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		Inventory->Weapons.Add(Active.PrimaryWeapon.WeaponEntry); // the fight keeps only owned gear
		Inventory->Weapons.Add(Active.SecondaryWeapon.WeaponEntry);
		if (!Hero->FindComponentByClass<UInfusionVFXComponent>())
		{
			UInfusionVFXComponent *VFX = NewObject<UInfusionVFXComponent>(Hero);
			Hero->AddInstanceComponent(VFX);
			VFX->RegisterComponent();
		}
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe});
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);
		return TestTrue(TEXT("Hero's menu is open on turn 1"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero);
	}

	/** B-592: whether the view's corner hints hold Label on Key (the keyboard's, headless). */
	static bool HasHint(const UBattleCommandMenuWidget *View, const TCHAR *Label, const FKey &Key)
	{
		return View->GetHints().ContainsByPredicate([Label, &Key](const FBattleMenuHint &Hint)
													{ return Hint.Label.ToString() == Label && Hint.KeyId == Key; });
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
		TestEqual(TEXT("the header"), Menu->GetViewTitle().ToString(), FString(TEXT("Attack · choose a target"))); // B-592: refine_v4's heading
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

	It("attacks the highlighted target on 1 / A again, and opens the next Attack on it while it lives (B-582, D-266)", [this]()
	{
		if (!Orchestrator || !Menu)
		{
			return;
		}
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *SecondFoe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe && SecondFoe))
		{
			return;
		}
		MenuKeysAddLoadout(Hero);
		MenuKeysAddLoadout(Foe);
		MenuKeysAddLoadout(SecondFoe);
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe, SecondFoe});
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);
		if (!TestTrue(TEXT("Hero's menu is open on turn 1"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero))
		{
			return;
		}
		const auto OpenAttack = [this]()
		{
			FPieMenuButtonData Attack;
			Attack.Category = EPieMenuCategory::Attack;
			Menu->HandleSelection(Attack);
			return Menu->GetHighlightedTarget();
		};

		AActor *Default = OpenAttack();
		TestEqual(TEXT("two targets"), Menu->GetHighlightTargetCount(), 2);
		TestNull(TEXT("nothing remembered yet"), Menu->GetLastAttackTarget(Hero));
		AActor *Other = Default == Foe ? SecondFoe : Foe;
		if (!TestTrue(TEXT("the first target by default"), Default == Foe || Default == SecondFoe))
		{
			return;
		}
		Menu->HighlightTarget(Other);
		TestTrue(TEXT("1 again is taken in the Attack picker"), Menu->PressMenuKey(EPieMenuCategory::Attack));
		TestNull(TEXT("and leaves it: the attack is sent"), Menu->GetHighlightedTarget());
		TestTrue(TEXT("on the highlighted target, which is remembered"), Menu->GetLastAttackTarget(Hero) == Other);
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);

		UCharacterDataComponent *OtherData = Other->FindComponentByClass<UCharacterDataComponent>();
		if (!TestTrue(TEXT("the target survived the hit"), OtherData && OtherData->bIsAlive))
		{
			return;
		}
		Menu->OpenForActor(Hero); // the hero's next menu
		TestTrue(TEXT("the hero's menu"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero);
		TestTrue(TEXT("the next Attack opens on the last target"), OpenAttack() == Other);
		Menu->HandleBack();
		OtherData->bIsAlive = false;
		TestTrue(TEXT("a dead last target: the default again"), OpenAttack() == Default);
		Menu->HandleBack();

		Menu->SetCombatOrchestrator(Orchestrator); // a new fight
		TestNull(TEXT("forgotten with the fight"), Menu->GetLastAttackTarget(Hero));
	});

	It("confirms the highlighted target on Enter / Space in a target picker, and nowhere else (B-582b, D-267)", [this]()
	{
		const FString Keys = ACombatPlayerController::GetMenuKeysString();
		TestTrue(TEXT("Enter, its own action"), Keys.Contains(TEXT("IA_Combat_MenuConfirm (")) && Keys.Contains(EKeys::Enter.GetDisplayName().ToString()));
		TestTrue(TEXT("Space, its own action"), Keys.Contains(TEXT("IA_Combat_MenuConfirmSpace (")) && Keys.Contains(EKeys::SpaceBar.GetDisplayName().ToString()));
		if (!StartSkilledDuel(/*bTwoKinds=*/false))
		{
			return;
		}
		TestFalse(TEXT("nothing on the main menu"), Menu->PressMenuKey(EPieMenuCategory::Target));
		TestTrue(TEXT("still the main menu"), Menu->IsAtMainMenu());

		FPieMenuButtonData Position;
		Position.Category = EPieMenuCategory::MoveRow;
		Menu->HandleSelection(Position);
		TestFalse(TEXT("nothing in the Position picker"), Menu->PressMenuKey(EPieMenuCategory::Target));
		TestTrue(TEXT("still in it"), Menu->IsPositionPickerOpen());
		Menu->HandleBack();

		FPieMenuButtonData Ability;
		Ability.Category = EPieMenuCategory::Ability;
		Ability.DataReference = NewObject<UAbilityData>(GetTransientPackage());
		Menu->HandleSelection(Ability);
		TestTrue(TEXT("an ability's target picker, on the foe"), Menu->GetHighlightedTarget() == Foe);
		TestFalse(TEXT("1 is the Attack picker's only"), Menu->PressMenuKey(EPieMenuCategory::Attack));
		TestTrue(TEXT("Enter / Space picks the highlighted foe there too"), Menu->PressMenuKey(EPieMenuCategory::Target));
		TestNull(TEXT("and leaves the picker"), Menu->GetHighlightedTarget());
	});

	It("opens the Attack step small, and 2 / 3 open the weapon and infusion lines, not Skills / Items (B-592, D-268)", [this]()
	{
		if (!StartTwoWeaponDuel())
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
		TestTrue(TEXT("two weapons"), Menu->GetCurrentCapabilities().bCanSwitchWeapon);
		FPieMenuButtonData Attack;
		Attack.Category = EPieMenuCategory::Attack;
		Attack.DisplayName = FText::FromString(TEXT("Attack"));
		Menu->HandleSelection(Attack);

		FTargetPanelView Panel = View->GetPanel();
		TestTrue(TEXT("the panel shows"), Panel.bShown && Panel.Open == ETargetPanelLine::None);
		TestTrue(TEXT("small: no weapon, element or level chips"), Panel.Weapons.IsEmpty() && Panel.Elements.IsEmpty() && Panel.Levels.IsEmpty());
		TestTrue(TEXT("the summary names the weapon"), Panel.Summary.ToString().StartsWith(TEXT("Sword")));
		TestTrue(TEXT("and the level"), Panel.Summary.ToString().Contains(TEXT("No infusion")));
		TestTrue(TEXT("hint 2 Weapon"), HasHint(View, TEXT("Weapon"), EKeys::Two));
		TestTrue(TEXT("hint 3 Infusion"), HasHint(View, TEXT("Infusion"), EKeys::Three) || HasHint(View, TEXT("Infusion level"), EKeys::Three));
		TestFalse(TEXT("no infusion key hints while it is closed"), HasHint(View, TEXT("Level"), EKeys::T));

		TestTrue(TEXT("2 / X: the weapon line"), Menu->PressMenuKey(EPieMenuCategory::Skills));
		Panel = View->GetPanel();
		TestTrue(TEXT("not Skills: still the Attack step"), Panel.bShown && Menu->GetHighlightedTarget() == Foe);
		TestEqual(TEXT("its heading"), View->GetTitle().ToString(), FString(TEXT("Attack · weapon")));
		if (TestEqual(TEXT("both weapons"), Panel.Weapons.Num(), 2))
		{
			TestEqual(TEXT("Sword first"), Panel.Weapons[0].ToString(), FString(TEXT("Sword")));
			TestEqual(TEXT("in hand"), Panel.ActiveWeapon, 0);
		}
		TestTrue(TEXT("only the weapon line"), Panel.Elements.IsEmpty() && Panel.Levels.IsEmpty());
		TestTrue(TEXT("hint 2 Next weapon"), HasHint(View, TEXT("Next weapon"), EKeys::Two));

		TestTrue(TEXT("2 / X again: the next weapon"), Menu->PressMenuKey(EPieMenuCategory::Skills));
		Panel = View->GetPanel();
		TestTrue(TEXT("the Hammer in hand, the line still open"), Panel.Open == ETargetPanelLine::Weapon && Panel.ActiveWeapon == 1);
		TestTrue(TEXT("the summary follows"), Panel.Summary.ToString().StartsWith(TEXT("Hammer")));
		// A switch is free any number of times (B-170c), so 2 / X keeps cycling, wrapping.
		TestTrue(TEXT("2 / X a third time"), Menu->PressMenuKey(EPieMenuCategory::Skills));
		TestEqual(TEXT("wraps back to the Sword"), View->GetPanel().ActiveWeapon, 0);
		Menu->PressMenuKey(EPieMenuCategory::Skills);
		TestEqual(TEXT("and on to the Hammer"), View->GetPanel().ActiveWeapon, 1);

		TestTrue(TEXT("3 / Y: the infusion lines"), Menu->PressMenuKey(EPieMenuCategory::Items));
		Panel = View->GetPanel();
		TestTrue(TEXT("not Items: the infusion line, the weapon line hidden"), Panel.Open == ETargetPanelLine::Infusion && Panel.Weapons.IsEmpty());
		TestEqual(TEXT("its heading"), View->GetTitle().ToString(), FString(TEXT("Attack · infusion")));
		TestEqual(TEXT("levels none, I, II"), Panel.Levels.Num(), MENU_KEYS_INFUSION_LEVELS);
		TestTrue(TEXT("hint T Level while it is open"), HasHint(View, TEXT("Level"), EKeys::T));
		TestTrue(TEXT("T steps the level, as now"), Menu->PressMenuKey(EPieMenuCategory::CycleLevel));
		TestEqual(TEXT("the level chip follows"), View->GetPanel().ActiveLevel, 1);

		TestTrue(TEXT("3 / Y again closes it"), Menu->PressMenuKey(EPieMenuCategory::Items));
		Panel = View->GetPanel();
		TestTrue(TEXT("back to the small panel"), Panel.Open == ETargetPanelLine::None && Panel.Levels.IsEmpty());
		TestTrue(TEXT("its summary has the level"), Panel.Summary.ToString().Contains(TEXT("Lv I")));
		Menu->PressMenuKey(EPieMenuCategory::Skills);
		Menu->PressMenuKey(EPieMenuCategory::Back);
		TestTrue(TEXT("Back closes an open line first"), View->GetPanel().bShown && View->GetPanel().Open == ETargetPanelLine::None);
		for (const FText &Label : View->GetRowLabels())
		{
			TestFalse(TEXT("no size line"), Label.ToString().Contains(TEXT("Size")));
			TestFalse(TEXT("no backlash line"), Label.ToString().Contains(TEXT("backlash")));
		}
	});

	It("shows no element for a spell of one fixed element, only its level (B-592, D-268)", [this]()
	{
		if (!StartTwoWeaponDuel())
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
		FPieMenuButtonData Spell;
		Spell.Category = EPieMenuCategory::Spell;
		Spell.DisplayName = FText::FromString(TEXT("Shade Lance"));
		Spell.DataReference = NewObject<USpellData>(GetTransientPackage());
		Menu->HandleSelection(Spell);
		if (!TestTrue(TEXT("the spell's target step"), View->GetPanel().bShown))
		{
			return;
		}
		TestFalse(TEXT("no weapon hint for a spell"), HasHint(View, TEXT("Weapon"), EKeys::Two));
		TestTrue(TEXT("hint 3 Infusion level"), HasHint(View, TEXT("Infusion level"), EKeys::Three));
		TestTrue(TEXT("3 / Y"), Menu->PressMenuKey(EPieMenuCategory::Items));
		Menu->PickTargetPanelChoice(ETargetPanelChoice::Level, 2);
		const FTargetPanelView Panel = View->GetPanel();
		TestTrue(TEXT("no element line"), Panel.Elements.IsEmpty());
		TestEqual(TEXT("only the level line"), Panel.Levels.Num(), MENU_KEYS_INFUSION_LEVELS);
		TestEqual(TEXT("a level chip click picks it"), Panel.ActiveLevel, 2);
		TestFalse(TEXT("no element key hint"), HasHint(View, TEXT("Element"), EKeys::R));
		Menu->PressMenuKey(EPieMenuCategory::Items);
		TestEqual(TEXT("the summary is the level alone"), View->GetPanel().Summary.ToString(), FString(TEXT("Lv II")));
	});

	It("moves the lit row with W / S (wrapping), opens it with D, picks a target with D, backs out with A (B-582c, D-269)", [this]()
	{
		if (!StartSkilledDuel(true)) // Attack and Skills open; Items greyed (W / S skip it)
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
		const auto LitLabel = [View]()
		{
			const int32 Lit = View->GetLitRow();
			return View->GetRowLabels().IsValidIndex(Lit) ? View->GetRowLabels()[Lit].ToString() : FString();
		};
		TestTrue(TEXT("the main menu opens lit on its first row, Attack"), LitLabel().Contains(TEXT("Attack")));
		View->MoveLitRow(-1);
		const FString Last = LitLabel();
		TestTrue(TEXT("W from the first wraps to the last row"), Last.Contains(TEXT("Skills")));
		View->MoveLitRow(1);
		TestTrue(TEXT("S from the last wraps to the first"), LitLabel().Contains(TEXT("Attack")));
		View->MoveLitRow(1);
		TestTrue(TEXT("S skips greyed Items and wraps to Skills"), LitLabel().Contains(TEXT("Skills")));
		View->MoveLitRow(1);
		TestTrue(TEXT("wor.UI.Print shows the lit row"), View->GetCommandMenuString().Contains(FString::Printf(TEXT("lit=%d"), View->GetLitRow())));

		TestFalse(TEXT("A (Back) on the main menu does nothing"), Menu->PressMenuKey(EPieMenuCategory::Back));
		TestTrue(TEXT("still the main menu"), Menu->IsOpen() && Menu->IsAtMainMenu());

		TestTrue(TEXT("D opens the lit row, as its key"), View->PressLitRow());
		TestEqual(TEXT("the Attack picker, on the foe"), Menu->GetHighlightedTarget(), Foe);
		TestEqual(TEXT("the foe's row is lit"), View->GetRowTarget(View->GetLitRow()), Foe);
		TestTrue(TEXT("A goes back one step"), Menu->PressMenuKey(EPieMenuCategory::Back));
		TestTrue(TEXT("to the main menu"), Menu->IsAtMainMenu());
		TestTrue(TEXT("Back re-lights the row you came from, Attack"), LitLabel().Contains(TEXT("Attack")));

		// QA: Skills, a kind, then Back twice re-lights the kind, then Skills.
		View->MoveLitRow(1);
		TestTrue(TEXT("D opens Skills"), View->PressLitRow() && !Menu->IsAtMainMenu());
		const FString Kind = LitLabel();
		TestTrue(TEXT("the kinds list opens lit on its first, Abilities"), Kind.Contains(TEXT("Abilities")));
		const FText KindsTitle = View->GetTitle();
		TestTrue(TEXT("D opens the kind"), View->PressLitRow() && !View->GetTitle().EqualTo(KindsTitle));
		TestFalse(TEXT("its list opens lit"), LitLabel().IsEmpty());
		TestTrue(TEXT("Back to the kinds"), Menu->PressMenuKey(EPieMenuCategory::Back) && View->GetTitle().EqualTo(KindsTitle));
		TestEqual(TEXT("on the kind you opened"), LitLabel(), Kind);
		TestTrue(TEXT("Back to the main menu"), Menu->PressMenuKey(EPieMenuCategory::Back) && Menu->IsAtMainMenu());
		TestTrue(TEXT("on Skills"), LitLabel().Contains(TEXT("Skills")));

		// A new turn starts fresh: both Defend round to the hero, whose menu opens on Attack, not the Skills it was left on.
		TestTrue(TEXT("the hero defends"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);
		TestTrue(TEXT("the foe defends"), Menu->GetCurrentActor() == Foe && Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);
		TestTrue(TEXT("the hero's next turn"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero && Menu->IsAtMainMenu());
		TestTrue(TEXT("opens lit on its first row, Attack"), LitLabel().Contains(TEXT("Attack")));

		View->PressLitRow();
		TestTrue(TEXT("D on a target row picks it, as Enter"), View->PressLitRow());
		TestNull(TEXT("and leaves the picker"), Menu->GetHighlightedTarget());

		const FString Keys = ACombatPlayerController::GetMenuKeysString();
		TestTrue(TEXT("W / S / D are bound"), Keys.Contains(TEXT("IA_Combat_MenuRowUp")) && Keys.Contains(TEXT("IA_Combat_MenuRowDown")) &&
												  Keys.Contains(TEXT("IA_Combat_MenuForward")));
		const FCombatMenuKey *BackA = ACombatPlayerController::GetMenuKeys().FindByPredicate([](const FCombatMenuKey &Key) { return Key.Keyboard == EKeys::A; });
		TestTrue(TEXT("A is a Back key"), BackA && BackA->Category == EPieMenuCategory::Back);
		TestFalse(TEXT("Q / E stay Intel's: no menu key on them"),
				  ACombatPlayerController::GetMenuKeys().ContainsByPredicate([](const FCombatMenuKey &Key) { return Key.Keyboard == EKeys::Q || Key.Keyboard == EKeys::E; }));
	});

	It("colours the selector with the acting character's element or class, readable with white text (B-582c, Crown)", [this]()
	{
		if (!StartSkilledDuel(false))
		{
			return;
		}
		UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
		UCharacterDataComponent *FoeData = Foe->FindComponentByClass<UCharacterDataComponent>();
		if (!TestTrue(TEXT("both have character data"), HeroData && HeroData->CharacterData && FoeData && FoeData->CharacterData))
		{
			return;
		}
		HeroData->CharacterData->InnateElement = ESpellElement::Water; // a Water Caster
		FoeData->CharacterData->CharacterClass = ECharacterClass::Generic;
		UBattleCommandMenuWidget *View = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
		if (!TestNotNull(TEXT("menu widget"), View))
		{
			return;
		}
		View->Initialise();
		ON_SCOPE_EXIT { View->Teardown(); };
		Menu->RefreshMenu();
		FLinearColor Fill, Outline;
		UBattleCommandMenuWidget::MakeSelectorColours(ElementColors::Water, Fill, Outline);
		TestEqual(TEXT("the Caster's selector is its element, Water"), View->GetSelectorFill(), Fill);
		TestEqual(TEXT("outlined in Water"), View->GetSelectorOutline(), ElementColors::Water);
		const FLinearColor HeroFill = View->GetSelectorFill();

		TestTrue(TEXT("the hero defends"), Menu->PressMenuKey(EPieMenuCategory::Defend));
		TestWorld->Tick(MENU_KEYS_SETTLE_FRAMES);
		TestTrue(TEXT("the foe's turn"), Menu->IsOpen() && Menu->GetCurrentActor() == Foe);
		UBattleCommandMenuWidget::MakeSelectorColours(ElementColors::Generic, Fill, Outline);
		TestEqual(TEXT("a Generic has no element: its class colour, the brown"), View->GetSelectorFill(), Fill);
		TestNotEqual(TEXT("the colour changed with the turn"), View->GetSelectorFill(), HeroFill);
		TestTrue(TEXT("wor.UI.Print shows it"), View->GetCommandMenuString().Contains(TEXT("selector=")));

		// Every element and class colour: white text on the fill at >= 4.5:1, and an outline that shows on the dark panel.
		constexpr float MIN_TEXT_CONTRAST = 4.5f;
		constexpr float WCAG_FLARE = 0.05f;
		TArray<FLinearColor> Accents = {ClassColors::ResonatorSilver, ElementColors::Generic, ElementColors::Ice};
		for (const ESpellElement Element : {ESpellElement::Fire, ESpellElement::Lightning, ESpellElement::Earth, ESpellElement::Wind,
											ESpellElement::Water, ESpellElement::Reality, ESpellElement::Void, ESpellElement::Light,
											ESpellElement::Darkness})
		{
			Accents.Add(ElementColors::GetColorForElement(Element));
		}
		for (const FLinearColor &Accent : Accents)
		{
			UBattleCommandMenuWidget::MakeSelectorColours(Accent, Fill, Outline);
			const float Contrast = (1.0f + WCAG_FLARE) / (UBattleCommandMenuWidget::GetRelativeLuminance(Fill) + WCAG_FLARE);
			TestTrue(FString::Printf(TEXT("white text on %s's fill reads (%.1f:1)"), *Accent.ToFColor(true).ToHex(), Contrast), Contrast >= MIN_TEXT_CONTRAST);
			TestTrue(FString::Printf(TEXT("%s's outline shows"), *Accent.ToFColor(true).ToHex()), UBattleCommandMenuWidget::GetRelativeLuminance(Outline) >= WCAG_FLARE);
		}
		UBattleCommandMenuWidget::MakeSelectorColours(ElementColors::Darkness, Fill, Outline);
		TestEqual(TEXT("Darkness keeps its black fill with the pale outline"), Outline, ElementColors::TextOutline);
	});

	It("never moves the possessed fighter on move input in a fight (B-582c, D-269)", [this]()
	{
		// No movement component: AddMovementInput then queues input unless the controller ignores move input.
		APawn *Pawn = TestWorld->GetWorld()->SpawnActor<APawn>();
		ACombatPlayerController *Controller = TestWorld->GetWorld()->SpawnActor<ACombatPlayerController>();
		if (!TestNotNull(TEXT("a fighter pawn"), Pawn) || !TestNotNull(TEXT("the fight's player controller"), Controller))
		{
			return;
		}
		Controller->Possess(Pawn);
		TestTrue(TEXT("move input is ignored"), Controller->IsMoveInputIgnored());
		Pawn->AddMovementInput(FVector::ForwardVector, 1.0f); // what a W mapped to walking would do
		TestTrue(TEXT("no movement queued"), Pawn->GetPendingMovementInputVector().IsNearlyZero());
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
