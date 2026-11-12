// BattleHUDSpec.cpp
// B-391: the new placeholder battle HUD. Its text helpers (turn strip, pools, status chips, menu key hints), then the
// two widgets built headless (not added to a viewport, so their plain-data state is what's checked) on a real duel:
// a card's HP line follows damage, the turn strip leads with the acting character, the command menu lists the
// subsystem's buttons with their keys and a row press acts. Layout and looks are PIE (docs/PIE/B-391.md).
// B-170e: with wor.Turns.ActionPips 1 the party cards show the pips line live and the menu an End Turn row; at 0 neither.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "HAL/IConsoleManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsText.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "UI/Combat/Battle/BattleCommandMenuWidget.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "UI/Combat/Battle/BattleHUDWidget.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "WoRGameNames.h"
#include "Save/SaveSubsystem.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr int32 BATTLE_HUD_SETTLE_FRAMES = 3;
	constexpr int32 BATTLE_HUD_HIT = 1;
	constexpr int32 BATTLE_HUD_PIPS_OFF = 0;
	constexpr int32 BATTLE_HUD_PIPS_ON = 1;
	constexpr int32 BATTLE_HUD_ONE_ACTION = 1;

	/** B-170e: sets wor.Turns.ActionPips for one test (before the fight starts, which latches it) and restores it. */
	struct FBattleHUDPipsSwitchScope
	{
		IConsoleVariable *Switch = nullptr;
		int32 Before = BATTLE_HUD_PIPS_OFF;

		explicit FBattleHUDPipsSwitchScope(int32 Value)
			: Switch(IConsoleManager::Get().FindConsoleVariable(TEXT("wor.Turns.ActionPips")))
		{
			if (Switch)
			{
				Before = Switch->GetInt();
				Switch->Set(Value, ECVF_SetByConsole);
			}
		}

		~FBattleHUDPipsSwitchScope()
		{
			if (Switch)
			{
				Switch->Set(Before, ECVF_SetByConsole);
			}
		}
	};

	void BattleHUDAddLoadout(AActor *Actor)
	{
		ULoadoutComponent *Loadout = NewObject<ULoadoutComponent>(Actor);
		Actor->AddInstanceComponent(Loadout);
		Loadout->RegisterComponent();
	}

	int32 FindLabel(const TArray<FText> &Labels, const FString &Part)
	{
		return Labels.IndexOfByPredicate([&Part](const FText &Label) { return Label.ToString().Contains(Part); });
	}
}

BEGIN_DEFINE_SPEC(FBattleHUDSpec, "WoR.UI.BattleHUD",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UCombatCommandMenuSubsystem *Menu = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Both sides player-controlled, so each turn opens the menu. False if setup failed. */
	bool StartDuel()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestTrue(TEXT("Test world created"), TestWorld->Create()))
		{
			return false;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Orchestrator, menu and combatants"), Orchestrator && Menu && Hero && Foe))
		{
			return false;
		}
		BattleHUDAddLoadout(Hero);
		BattleHUDAddLoadout(Foe);
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe});
		TestWorld->Tick(BATTLE_HUD_SETTLE_FRAMES);
		return TestTrue(TEXT("Hero's menu is open on turn 1"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero);
	}
END_DEFINE_SPEC(FBattleHUDSpec)

void FBattleHUDSpec::Define()
{
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

	Describe("the text helpers", [this]()
	{
		It("leads the turn strip with the current actor and numbers the rest on", [this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			if (!TestTrue(TEXT("Test world created"), TestWorld->Create()))
			{
				return;
			}
			AActor *Current = TestWorld->GetWorld()->SpawnActor<AActor>();
			AActor *Next = TestWorld->GetWorld()->SpawnActor<AActor>();
			const TArray<FBattleUpcomingTurn> Upcoming = {{Next, false}, {Current, true}, {Next, false}};
			const TArray<FBattleTurnSlot> Strip = BattleHUDView::MakeTurnStrip(Current, false, 4, Upcoming, 2);
			if (!TestEqual(TEXT("current + two upcoming"), Strip.Num(), 3))
			{
				return;
			}
			TestTrue(TEXT("slot 0 is the current actor"), Strip[0].bCurrent && Strip[0].Actor.Get() == Current);
			TestEqual(TEXT("slot 0 is turn 4"), Strip[0].TurnNumber, 4);
			TestTrue(TEXT("slot 1 is next, turn 5"), Strip[1].Actor.Get() == Next && Strip[1].TurnNumber == 5 && !Strip[1].bCurrent);
			TestTrue(TEXT("slot 2 keeps its bonus flag"), Strip[2].bBonus && Strip[2].TurnNumber == 6);
		});

		It("formats a pool and clamps its fraction", [this]()
		{
			TestEqual(TEXT("HP line"), BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), 12, 40).ToString(), FString(TEXT("HP 12 / 40")));
			TestEqual(TEXT("fraction"), BattleHUDView::PoolFraction(10, 40), 0.25f);
			TestEqual(TEXT("no max"), BattleHUDView::PoolFraction(10, 0), 0.0f);
			TestEqual(TEXT("over max"), BattleHUDView::PoolFraction(50, 40), 1.0f);
		});

		It("shows a status chip's turns and stacks, and none for a permanent single", [this]()
		{
			FActiveSkillEffect Timed;
			Timed.EffectType = ESkillEffectType::Stun;
			Timed.RemainingTurns = 2;
			const FString Name = WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::Stun).ToString();
			TestEqual(TEXT("turns"), BattleHUDView::MakeStatusChip(Timed).ToString(), Name + TEXT(" 2"));

			FActiveSkillEffect Stacked = Timed;
			Stacked.bPermanent = true;
			Stacked.bCanStack = true;
			Stacked.CurrentStacks = 3;
			TestEqual(TEXT("stacks"), BattleHUDView::MakeStatusChip(Stacked).ToString(), Name + TEXT(" x3"));

			FActiveSkillEffect Plain = Stacked;
			Plain.CurrentStacks = 1;
			TestEqual(TEXT("name only"), BattleHUDView::MakeStatusChip(Plain).ToString(), Name);
			TestTrue(TEXT("no statuses, no chips"), BattleHUDView::JoinStatusChips({}).IsEmpty());
		});

		It("gives each menu key to the row it presses, as PressMenuKey does", [this]()
		{
			const TArray<EPieMenuCategory> Rows = {EPieMenuCategory::Attack, EPieMenuCategory::Abilities, EPieMenuCategory::ResonateWeapon,
												   EPieMenuCategory::Items, EPieMenuCategory::Defend};
			const TArray<FCombatMenuKey> &Keys = ACombatPlayerController::GetMenuKeys();
			const auto KeyCategory = [&Keys](int32 KeyIndex)
			{ return Keys.IsValidIndex(KeyIndex) ? Keys[KeyIndex].Category : EPieMenuCategory::None; };

			const TArray<int32> Main = BattleHUDView::AssignMenuKeys(Rows, {true, true, true, false, true}, /*bAtMainMenu=*/true);
			TestTrue(TEXT("Attack row: Attack key"), KeyCategory(Main[0]) == EPieMenuCategory::Attack);
			TestTrue(TEXT("Abilities row: Skills key"), KeyCategory(Main[1]) == EPieMenuCategory::Abilities);
			TestEqual(TEXT("the weapon resonance after it: no key (Skills presses Abilities)"), Main[2], INDEX_NONE);
			TestEqual(TEXT("greyed Items row: no key"), Main[3], INDEX_NONE);
			TestTrue(TEXT("Defend row: Defend key"), KeyCategory(Main[4]) == EPieMenuCategory::Defend);

			const TArray<int32> Sub = BattleHUDView::AssignMenuKeys(Rows, {true, true, true, true, true}, /*bAtMainMenu=*/false);
			TestEqual(TEXT("off the main menu: no Attack key"), Sub[0], INDEX_NONE);
			TestEqual(TEXT("off the main menu: no Items key"), Sub[3], INDEX_NONE);
		});
	});

	Describe("the widgets on a duel", [this]()
	{
		It("updates a card's HP line when its character is hit", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (!TestNotNull(TEXT("battle HUD widget"), HUD))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe});
			const UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
			if (!TestTrue(TEXT("two cards, a party card first, with HP to lose"), HUD->GetCards().Num() == 2 && HUD->GetCards()[0].bParty &&
																					  HeroData && HeroData->CurrentHP > BATTLE_HUD_HIT))
			{
				HUD->Teardown();
				return;
			}
			TestEqual(TEXT("seeded HP line"), HUD->GetCards()[0].HPLine.ToString(),
					  BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), HeroData->CurrentHP, HeroData->MaxHP).ToString());

			Hero->FindComponentByClass<UCharacterDataComponent>()->ServerTakeDamage(BATTLE_HUD_HIT);
			TestEqual(TEXT("HP line after the hit"), HUD->GetCards()[0].HPLine.ToString(),
					  BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), HeroData->CurrentHP, HeroData->MaxHP).ToString());

			const AActor *Acting = Orchestrator->GetCurrentActor();
			TestTrue(TEXT("the turn strip leads with the acting character"),
					 !HUD->GetTurnLines().IsEmpty() && HUD->GetTurnLines()[0].ToString().Contains(BattleHUDView::GetCombatantName(Acting).ToString()));
			TestTrue(TEXT("the acting card is marked"), HUD->GetCards()[0].bActing == (Acting == Hero));

			HUD->SetStatusPanelOpen(true);
			TestTrue(TEXT("the status panel names every combatant"),
					 HUD->GetStatusPanelText().ToString().Contains(HUD->GetCards()[1].Name.ToString()));
			HUD->Teardown();
		});

		It("lists the menu's buttons with their keys, and a row press acts", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UBattleCommandMenuWidget *MenuWidget = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
			if (!TestNotNull(TEXT("command menu widget"), MenuWidget))
			{
				return;
			}
			MenuWidget->Initialise(); // replays the open menu
			TestTrue(TEXT("showing Hero's menu"), MenuWidget->IsShowing() &&
													  MenuWidget->GetTitle().ToString().Contains(BattleHUDView::GetCombatantName(Hero).ToString()));
			const int32 DefendRow = FindLabel(MenuWidget->GetRowLabels(), TEXT("Defend"));
			if (!TestTrue(TEXT("a Defend row"), DefendRow != INDEX_NONE))
			{
				MenuWidget->Teardown();
				return;
			}
			TestTrue(TEXT("its key hint"), MenuWidget->GetRowLabels()[DefendRow].ToString().StartsWith(TEXT("[")));

			MenuWidget->PressRow(DefendRow);
			TestTrue(TEXT("the turn moved on"), Orchestrator->GetCurrentActor() == Foe);
			TestTrue(TEXT("the widget shows the menu exactly while it is open"), MenuWidget->IsShowing() == Menu->IsOpen());
			if (Menu->IsOpen() && Menu->GetCurrentActor())
			{
				TestTrue(TEXT("titled for whoever the menu is open for"),
						 MenuWidget->GetTitle().ToString().Contains(BattleHUDView::GetCombatantName(Menu->GetCurrentActor()).ToString()));
			}
			MenuWidget->Teardown();
		});

		It("shows the party's action pips live and an End Turn row in an action-pips fight (B-170e)", [this]()
		{
			const FBattleHUDPipsSwitchScope Pips(BATTLE_HUD_PIPS_ON);
			USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
			if (!TestNotNull(TEXT("the switch exists"), Pips.Switch) || !TestNotNull(TEXT("the save"), Save))
			{
				return;
			}
			const bool bHiddenBefore = Save->IsEndTurnButtonHidden();
			ON_SCOPE_EXIT { Save->SetEndTurnButtonHidden(bHiddenBefore); };
			Save->SetEndTurnButtonHidden(false); // shown, the default (Crown, 2026-11-12), whatever a loaded slot held
			if (!StartDuel())
			{
				return;
			}
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("battle HUD widget"), HUD) || !TestNotNull(TEXT("Hero data"), HeroData))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe});
			const auto ExpectedLine = [HeroData]()
			{
				return ActionPipsText::MakeActionsLine(HeroData->CurrentActions, HeroData->GetMaxActions(), HeroData->GetActionPointProgress())
					.ToString();
			};
			TestTrue(TEXT("the HUD shows actions"), HUD->IsShowingActions());
			if (TestTrue(TEXT("two cards, the party's first"), HUD->GetCards().Num() == 2 && HUD->GetCards()[0].bParty))
			{
				TestEqual(TEXT("the party card's pips line"), HUD->GetCards()[0].ActionsLine.ToString(), ExpectedLine());
				TestTrue(TEXT("no pips on the enemy card"), HUD->GetCards()[1].ActionsLine.IsEmpty());
				HeroData->GainActions(BATTLE_HUD_ONE_ACTION, /*bMayPassMax=*/true);
				TestEqual(TEXT("the line follows OnActionsChanged"), HUD->GetCards()[0].ActionsLine.ToString(), ExpectedLine());
			}
			HUD->Teardown();

			UBattleCommandMenuWidget *MenuWidget = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
			if (!TestNotNull(TEXT("command menu widget"), MenuWidget))
			{
				return;
			}
			MenuWidget->Initialise();
			const int32 EndTurnRow = FindLabel(MenuWidget->GetRowLabels(), WoRGameNames::GetEndTurnVerb().ToString());
			if (TestTrue(TEXT("an End Turn row"), EndTurnRow != INDEX_NONE))
			{
				TestFalse(TEXT("with no key hint: End Turn has no key yet (Crown, 2026-11-12)"),
						  MenuWidget->GetRowLabels()[EndTurnRow].ToString().StartsWith(TEXT("[")));
				MenuWidget->PressRow(EndTurnRow);
				TestTrue(TEXT("End Turn ended Hero's turn"), Orchestrator->GetCurrentActor() == Foe);
			}
			MenuWidget->Teardown();
		});

		It("shows no action pips and no End Turn row with the switch off (B-170e)", [this]()
		{
			const FBattleHUDPipsSwitchScope Pips(BATTLE_HUD_PIPS_OFF);
			if (!StartDuel())
			{
				return;
			}
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (!TestNotNull(TEXT("battle HUD widget"), HUD))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe});
			TestFalse(TEXT("the HUD shows no actions"), HUD->IsShowingActions());
			TestTrue(TEXT("no pips line"), !HUD->GetCards().IsEmpty() && HUD->GetCards()[0].ActionsLine.IsEmpty());
			HUD->Teardown();

			UBattleCommandMenuWidget *MenuWidget = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
			if (!TestNotNull(TEXT("command menu widget"), MenuWidget))
			{
				return;
			}
			MenuWidget->Initialise();
			TestEqual(TEXT("no End Turn row"), FindLabel(MenuWidget->GetRowLabels(), WoRGameNames::GetEndTurnVerb().ToString()), INDEX_NONE);
			MenuWidget->Teardown();
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
