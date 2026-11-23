// BattleHUDSpec.cpp
// B-391: the new placeholder battle HUD. Its text helpers (turn strip, pools, status chips, menu key hints), then the
// two widgets built headless (not added to a viewport, so their plain-data state is what's checked) on a real duel:
// a card's HP line follows damage, the turn strip leads with the acting character, the command menu lists the
// subsystem's buttons with their keys and a row press acts. Layout and looks are PIE (docs/PIE/B-391.md).
// B-391b: the Broken Darkness absorb line and overload tint; damage numbers pool, stack and drop a 0.
// B-391c: the status buildup bar (fill, cracks, line, tint), the party gear line and tooltip, the durability header
// and the "Negated" cue; on the duel, a card's buildup follows a hit's buildup.
// B-170e: the party cards show the pips line live and the menu an End Turn row (B-170h2: every fight; the switch is gone).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Combat/CombatConstants.h"
#include "HAL/IConsoleManager.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/PlayerBuild.h"
#include "Combat/Actions/ActionPipsText.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/Tier/CombatTier.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Infusion/ElementColors.h"
#include "Inventory/InventoryComponent.h"
#include "Infusion/HybridSpellColors.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "UI/Combat/Battle/BattleCommandMenuWidget.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "UI/Combat/Battle/BattleHUDWidget.h"
#include "UI/Combat/Battle/TurnHexWidget.h"
#include "Engine/Texture2D.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "WoRGameNames.h"
#include "Save/SaveSubsystem.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr int32 BATTLE_HUD_SETTLE_FRAMES = 3;
	constexpr int32 BATTLE_HUD_HIT = 1;
	constexpr int32 BATTLE_HUD_NUMBER = 45;
	constexpr float BATTLE_HUD_WEAK = 1.5f;
	constexpr float BATTLE_HUD_BUILDUP = 20.0f;  // well under the cap: no break
	constexpr float BATTLE_HUD_CRACKED = 80.0f;  // past three of the four default cracks
	constexpr int32 BATTLE_HUD_RING_NOW = 3;
	constexpr int32 BATTLE_HUD_RING_MAX = 5;
	constexpr int32 BATTLE_HUD_WEAPON_NOW = 2;
	constexpr int32 BATTLE_HUD_WEAPON_MAX = 4;
	constexpr int32 BATTLE_HUD_GEM_DURABILITY = 7; // the hero's weapon gem in the durability header case
	const FCrystalId BATTLE_HUD_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 BATTLE_HUD_ONE_ACTION = 1;
	// B-592b: pools and an ability EP cost that give a Raw L2 infusion a visible, non-lethal HP cost.
	constexpr int32 BATTLE_HUD_GHOST_POOL = 200;
	constexpr int32 BATTLE_HUD_GHOST_ABILITY_EP = 40;
	constexpr int32 BATTLE_HUD_GHOST_LEVEL = 2;
	constexpr int32 BATTLE_HUD_MAX_SOURCE_PRESSES = 8; // more than any character's source count

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

	/** The test world, made once per case, so a case can set the save before StartDuel. False if setup failed. */
	bool CreateTestWorld()
	{
		if (TestWorld)
		{
			return true;
		}
		TestWorld = MakeUnique<FWoRTestWorld>();
		return TestTrue(TEXT("Test world created"), TestWorld->Create());
	}

	/** Both sides player-controlled, so each turn opens the menu. False if setup failed. */
	bool StartDuel()
	{
		if (!CreateTestWorld())
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

	Describe("the party card's show levels (B-503j, D-216)", [this]()
	{
		It("shows the name only in the hub", [this]()
		{
			const FBattleCardParts Parts = BattleHUDCard::GetParts(EBattleCardLevel::Hub, /*bParty=*/true);
			TestFalse(TEXT("no HP"), Parts.bHPBar || Parts.bHPLine);
			TestFalse(TEXT("no EP"), Parts.bEPLine);
			TestFalse(TEXT("no build-up"), Parts.bBuildupBar || Parts.bBuildupLine);
			TestFalse(TEXT("no fight parts"), Parts.bGear || Parts.bActions || Parts.bStatus);
			TestFalse(TEXT("fits the name (B-503k)"), Parts.bFixedWidth);
		});

		It("shows HP and EP in a trial, no build-up bar (D-216)", [this]()
		{
			const FBattleCardParts Parts = BattleHUDCard::GetParts(EBattleCardLevel::Trial, /*bParty=*/true);
			TestTrue(TEXT("HP bar and line"), Parts.bHPBar && Parts.bHPLine);
			TestTrue(TEXT("EP line"), Parts.bEPLine);
			TestFalse(TEXT("no build-up bar: battle only (it is always empty after a fight)"), Parts.bBuildupBar);
			TestFalse(TEXT("no fight parts"), Parts.bBuildupLine || Parts.bGear || Parts.bActions || Parts.bStatus);
			TestTrue(TEXT("the battle card's width"), Parts.bFixedWidth);
		});

		It("shows everything in a battle, and an enemy its bars and statuses (Q-391)", [this]()
		{
			const FBattleCardParts Party = BattleHUDCard::GetParts(EBattleCardLevel::Battle, /*bParty=*/true);
			TestTrue(TEXT("a party card has every part"), Party.bHPBar && Party.bHPLine && Party.bEPLine && Party.bBuildupBar &&
															  Party.bBuildupLine && Party.bGear);
			TestFalse(TEXT("but no action pips: they are on the command menu's tab (D-260)"), Party.bActions);
			TestFalse(TEXT("and no statuses: they are in Intel (D-261)"), Party.bStatus);
			const FBattleCardParts Enemy = BattleHUDCard::GetParts(EBattleCardLevel::Battle, /*bParty=*/false);
			TestTrue(TEXT("an enemy: the bars and statuses"), Enemy.bHPBar && Enemy.bBuildupBar && Enemy.bStatus);
			TestFalse(TEXT("an enemy: no numbers, gear or actions"), Enemy.bHPLine || Enemy.bEPLine || Enemy.bBuildupLine || Enemy.bGear || Enemy.bActions);
		});

		It("builds only its level's parts", [this]()
		{
			UWidgetTree *Tree = NewObject<UWidgetTree>(GetTransientPackage());
			for (const EBattleCardLevel Level : {EBattleCardLevel::Hub, EBattleCardLevel::Trial, EBattleCardLevel::Battle})
			{
				FBattleHUDCard Card;
				Card.bParty = true;
				TestNotNull(TEXT("a frame"), BattleHUDCard::Build(Tree, Card, Level));
				const FBattleCardParts Parts = BattleHUDCard::GetParts(Level, Card.bParty);
				TestNotNull(TEXT("the name"), Card.NameText.Get());
				TestEqual(TEXT("HP bar"), Card.HPBar != nullptr, Parts.bHPBar);
				TestEqual(TEXT("HP number (on the HP bar, B-503m)"), Card.HPText != nullptr, Parts.bHPLine);
				TestEqual(TEXT("EP bar (B-503m)"), Card.EPBar != nullptr, Parts.bEPLine);
				TestEqual(TEXT("EP number (on the EP bar)"), Card.EPText != nullptr, Parts.bEPLine);
				TestEqual(TEXT("build-up bar"), Card.BuildupBar != nullptr, Parts.bBuildupBar);
				TestEqual(TEXT("build-up line"), Card.BuildupText != nullptr, Parts.bBuildupLine);
				TestEqual(TEXT("the gear tooltip on hover, no gear line (B-503k)"), Card.Frame->GetVisibility() == ESlateVisibility::Visible, Parts.bGear);
				TestEqual(TEXT("actions"), Card.ActionsText != nullptr, Parts.bActions);
				TestEqual(TEXT("statuses"), Card.StatusText != nullptr, Parts.bStatus);
				BattleHUDCard::Push(Card); // the parts it didn't build are skipped
			}
		});

		It("shows a fight's card compact or full, and leaves the hub and trial cards as built (B-436a)", [this]()
		{
			UWidgetTree *Tree = NewObject<UWidgetTree>(GetTransientPackage());
			FBattleHUDCard Card;
			Card.bParty = true;
			Card.DetailLine = FText::FromString(TEXT("Fire Refractor"));
			BattleHUDCard::Build(Tree, Card, EBattleCardLevel::Battle);
			if (!TestTrue(TEXT("a battle card keeps its width box and its EP row"), Card.WidthBox && Card.EPRow && Card.BuildupBar))
			{
				return;
			}
			TestNull(TEXT("no status line on a party card: its effects are in Intel (D-261)"), Card.StatusText.Get());
			BattleHUDCard::Push(Card);
			BattleHUDCard::SetCompact(Card, true);
			TestEqual(TEXT("compact: the narrow width"), Card.WidthBox->GetWidthOverride(), BattleCardStyle::CompactCardWidth);
			TestTrue(TEXT("compact: the name, the HP bar and the build-up bar stay"),
					 Card.NameText->GetVisibility() != ESlateVisibility::Collapsed && Card.HPBar->GetVisibility() != ESlateVisibility::Collapsed &&
						 Card.BuildupBar->GetVisibility() != ESlateVisibility::Collapsed);
			TestTrue(TEXT("compact: the build-up number stays on its bar (D-257)"),
					 Card.BuildupText && Card.BuildupText->GetVisibility() != ESlateVisibility::Collapsed &&
						 Card.BuildupText->GetParent() && Card.BuildupText->GetParent() == Card.BuildupBar->GetParent());
			const UWidget *HiddenWhenCompact[] = {Card.DetailText.Get(), Card.EPRow.Get()};
			for (const UWidget *Hidden : HiddenWhenCompact)
			{
				TestTrue(TEXT("compact: the kind and EP are collapsed"),
						 Hidden && Hidden->GetVisibility() == ESlateVisibility::Collapsed);
			}
			TestNull(TEXT("no actions line on a fight card at all: the pips are on the menu's tab (D-260)"), Card.ActionsText.Get());

			BattleHUDCard::SetCompact(Card, false);
			TestEqual(TEXT("full: the card's width"), Card.WidthBox->GetWidthOverride(), BattleCardStyle::CardWidth);
			TestTrue(TEXT("full: the kind and EP are back"),
					 Card.DetailText->GetVisibility() == ESlateVisibility::HitTestInvisible && Card.EPRow->GetVisibility() == ESlateVisibility::HitTestInvisible);

			FBattleHUDCard NoEP;
			NoEP.bParty = true;
			NoEP.bShowEP = false; // a Resonator with no EP use
			BattleHUDCard::Build(Tree, NoEP, EBattleCardLevel::Battle);
			BattleHUDCard::SetCompact(NoEP, false);
			TestTrue(TEXT("full: a card with no EP keeps its EP row collapsed"), NoEP.EPRow && NoEP.EPRow->GetVisibility() == ESlateVisibility::Collapsed);

			FBattleHUDCard HubCard;
			HubCard.bParty = true;
			BattleHUDCard::Build(Tree, HubCard, EBattleCardLevel::Hub);
			BattleHUDCard::Push(HubCard);
			TestTrue(TEXT("the hub's name-only card has no EP row and no fixed width"),
					 !HubCard.EPRow && HubCard.WidthBox && HubCard.WidthBox->GetWidthOverride() == 0.0f);
		});
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

		It("makes the turn order's hexagons: the current one large and violet, party cyan, enemies rose (B-590)", [this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			if (!TestTrue(TEXT("Test world created"), TestWorld->Create()))
			{
				return;
			}
			AActor *Ally = TestWorld->GetWorld()->SpawnActor<AActor>();
			AActor *Enemy = TestWorld->GetWorld()->SpawnActor<AActor>();
			const TArray<FBattleUpcomingTurn> Upcoming = {{Enemy, false}, {Ally, false}, {nullptr, false}, {Enemy, false}};
			const TArray<FBattleTurnSlot> Strip = BattleHUDView::MakeTurnStrip(Ally, false, 1, Upcoming, Upcoming.Num());
			const auto IsParty = [Ally](const AActor *Actor) { return Actor == Ally; };
			UTexture *EnemyFace = NewObject<UTexture2D>(GetTransientPackage()); // stands in for a captured face
			const auto FindFace = [Enemy, EnemyFace](const AActor *Actor) { return Actor == Enemy ? EnemyFace : nullptr; };

			const TArray<FBattleTurnHex> All = BattleHUDView::MakeTurnHexes(Strip, BattleHUDView::MAX_TURNS_SHOWN, IsParty, FindFace);
			if (!TestEqual(TEXT("the empty slot is skipped"), All.Num(), Strip.Num() - 1))
			{
				return;
			}
			TestTrue(TEXT("the current turn first"), All[0].bCurrent && All[0].Actor.Get() == Ally);
			TestEqual(TEXT("the current one is large"), All[0].Size, BattleHUDView::CURRENT_TURN_HEX_SIZE);
			TestTrue(TEXT("with the violet edge, even for the party"), All[0].Border.Equals(BattleHUDView::CURRENT_TURN_HEX_COLOUR));
			TestTrue(TEXT("an enemy next: small and rose"), !All[1].bCurrent && !All[1].bParty && All[1].Size == BattleHUDView::NEXT_TURN_HEX_SIZE &&
															   All[1].Border.Equals(BattleHUDView::ENEMY_TURN_HEX_COLOUR));
			TestTrue(TEXT("then the ally: cyan"), All[2].bParty && All[2].Border.Equals(BattleHUDView::PARTY_TURN_HEX_COLOUR));
			TestTrue(TEXT("a captured face fills its hexagon"), All[1].Portrait.Get() == EnemyFace);
			TestNull(TEXT("no capture: the placeholder"), All[2].Portrait.Get());
			TestEqual(TEXT("Turns shown cuts the strip, the current turn included"), BattleHUDView::MakeTurnHexes(Strip, 2, IsParty, FindFace).Num(), 2);
			TestEqual(TEXT("1: only the current turn"), BattleHUDView::MakeTurnHexes(Strip, BattleHUDView::MIN_TURNS_SHOWN, IsParty, FindFace).Num(), 1);
			TestNull(TEXT("no 3D model: no capture, so the placeholder"), TurnHexFace::Capture(Ally, GetTransientPackage(), FVector::ZeroVector, {Enemy}));
		});

		It("reads a saved Turns shown and draws a flat-topped hexagon in its box (B-590)", [this]()
		{
			TestEqual(TEXT("never saved: 6"), BattleHUDView::ReadTurnsShown(0), BattleHUDView::DEFAULT_TURNS_SHOWN);
			TestEqual(TEXT("3 stays"), BattleHUDView::ReadTurnsShown(3), 3);
			TestEqual(TEXT("above 6: 6"), BattleHUDView::ReadTurnsShown(BattleHUDView::MAX_TURNS_SHOWN + 1), BattleHUDView::MAX_TURNS_SHOWN);
			const float Size = BattleHUDView::NEXT_TURN_HEX_SIZE;
			const TArray<FVector2f> Corners = BattleHUDView::GetTurnHexCorners(Size);
			if (!TestEqual(TEXT("six corners"), Corners.Num(), 6))
			{
				return;
			}
			FBox2f Bounds(ForceInit);
			for (const FVector2f &Corner : Corners)
			{
				Bounds += Corner;
			}
			TestTrue(TEXT("it fills its box"), Bounds.Min.Equals(FVector2f::ZeroVector) && Bounds.Max.Equals(FVector2f(Size, Size)));
			TestTrue(TEXT("flat top: two corners on the top edge"), Corners[1].Y == 0.0f && Corners[2].Y == 0.0f);
			TestTrue(TEXT("points left and right at mid-height"), Corners[0].Equals(FVector2f(0.0f, Size * 0.5f)) && Corners[3].Equals(FVector2f(Size, Size * 0.5f)));
		});

		It("formats a pool and clamps its fraction", [this]()
		{
			TestEqual(TEXT("HP line"), BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), 12, 40).ToString(), FString(TEXT("HP 12 / 40")));
			TestEqual(TEXT("fraction"), BattleHUDView::PoolFraction(10, 40), 0.25f);
			TestEqual(TEXT("no max"), BattleHUDView::PoolFraction(10, 0), 0.0f);
			TestEqual(TEXT("over max"), BattleHUDView::PoolFraction(50, 40), 1.0f);
		});

		It("pulses only a living card under 30% HP (B-436b)", [this]()
		{
			TestTrue(TEXT("under 30%: low"), BattleHUDView::IsLowHP(true, BattleHUDView::LOW_HP_FRACTION - KINDA_SMALL_NUMBER));
			TestFalse(TEXT("at 30%: not low"), BattleHUDView::IsLowHP(true, BattleHUDView::LOW_HP_FRACTION));
			TestFalse(TEXT("downed: never low"), BattleHUDView::IsLowHP(false, 0.0f));

			const double Period = BattleHUDView::LOW_HP_PULSE_PERIOD_SECONDS;
			TestEqual(TEXT("full at the start"), BattleHUDView::GetLowHPPulseOpacity(0.0), 1.0f, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("dimmest half-way"), BattleHUDView::GetLowHPPulseOpacity(Period * 0.5), BattleHUDView::LOW_HP_PULSE_MIN_OPACITY, KINDA_SMALL_NUMBER);
			TestEqual(TEXT("full again each period"), BattleHUDView::GetLowHPPulseOpacity(Period * 3.0), 1.0f, KINDA_SMALL_NUMBER);
			const float Between = BattleHUDView::GetLowHPPulseOpacity(Period * 0.25);
			TestTrue(TEXT("in between, between"), Between > BattleHUDView::LOW_HP_PULSE_MIN_OPACITY && Between < 1.0f);
		});

		It("shows a Broken Darkness absorb line and its overload (B-391b)", [this]()
		{
			TestEqual(TEXT("absorb line"), BattleHUDView::FormatAbsorbLine(80, 100).ToString(), FString(TEXT("Barrier 80 / 100")));
			TestFalse(TEXT("at max: not overloaded"), BattleHUDView::IsOverloaded(100, 100));
			TestTrue(TEXT("past max: overloaded"), BattleHUDView::IsOverloaded(115, 100));
			const FString Over = BattleHUDView::FormatAbsorbLine(130, 100).ToString();
			TestTrue(TEXT("overload line"), Over.StartsWith(TEXT("Barrier 130 / 100 (+")) && Over.Contains(TEXT("130")));
			TestTrue(TEXT("white below"), BattleHUDView::GetOverloadColour(80, 100).Equals(FLinearColor::White));
			TestTrue(TEXT("yellow just past"), BattleHUDView::GetOverloadColour(105, 100).Equals(FLinearColor::Yellow));
			TestTrue(TEXT("red near the cap"), BattleHUDView::GetOverloadColour(125, 100).Equals(FLinearColor::Red));
			TestFalse(TEXT("no max: never overloaded"), BattleHUDView::IsOverloaded(5, 0));
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

		It("fills the buildup bar, cracks it past half and tints it as the old panel (B-391c)", [this]()
		{
			TestEqual(TEXT("fraction"), BattleHUDView::BuildupFraction(40.0f, 100.0f), 0.4f);
			TestEqual(TEXT("over the cap"), BattleHUDView::BuildupFraction(150.0f, 100.0f), 1.0f);
			TestEqual(TEXT("no cap"), BattleHUDView::BuildupFraction(5.0f, 0.0f), 0.0f);

			const TArray<float> Cracks = BattleHUDView::GetCrackPositions();
			if (!TestEqual(TEXT("one position per crack"), Cracks.Num(), BattleHUDView::CRACK_COUNT))
			{
				return;
			}
			TestEqual(TEXT("the first crack at the start"), Cracks[0], BattleHUDView::CRACK_START_FRACTION);
			for (int32 Index = 1; Index < Cracks.Num(); ++Index)
			{
				TestTrue(TEXT("cracks run toward the cap"), Cracks[Index] > Cracks[Index - 1] && Cracks[Index] < 1.0f);
			}
			TestEqual(TEXT("clean below half"), BattleHUDView::CountCracksShown(0.49f), 0);
			TestEqual(TEXT("the first at half"), BattleHUDView::CountCracksShown(0.5f), 1);
			TestEqual(TEXT("three at 80%"), BattleHUDView::CountCracksShown(0.8f), 3);
			TestEqual(TEXT("all at the cap"), BattleHUDView::CountCracksShown(1.0f), BattleHUDView::CRACK_COUNT);

			TestEqual(TEXT("line"), BattleHUDView::FormatBuildupLine(40.7f, 100.0f).ToString(), FString(TEXT("40/100")));
			TestTrue(TEXT("element colour"), BattleHUDView::GetBuildupColour(ESpellElement::Fire, false)
												 .Equals(ElementColors::GetColorForElement(ESpellElement::Fire)));
			TestTrue(TEXT("a Broken Darkness attacker darkens it"), BattleHUDView::GetBuildupColour(ESpellElement::Fire, true)
																		.Equals(UHybridSpellColors::GetHybridSpellColors(ESpellElement::Fire).BlendedColor));
			TestTrue(TEXT("physical stays neutral, even from Broken Darkness"), BattleHUDView::GetBuildupColour(ESpellElement::None, true)
																					.Equals(ElementColors::GetColorForElement(ESpellElement::None)));
		});

		It("names a party card's gear and lists it in the tooltip (B-391c)", [this]()
		{
			FBattleGearNames Gear;
			Gear.LoadoutName = TEXT("Main");
			Gear.WeaponName = TEXT("Iron Sword");
			Gear.RingName = TEXT("Ember Ring");
			TestEqual(TEXT("weapon and ring"), BattleHUDView::FormatGearLine(Gear).ToString(), FString(TEXT("Iron Sword · Ember Ring")));
			const FString Tooltip = BattleHUDView::FormatGearTooltip(Gear).ToString();
			TestTrue(TEXT("tooltip lists all three"), Tooltip.Contains(TEXT("Loadout: Main")) && Tooltip.Contains(TEXT("Weapon: Iron Sword")) &&
														  Tooltip.Contains(TEXT("Adapter: Ember Ring")));
			Gear.RingName.Reset();
			TestEqual(TEXT("weapon alone"), BattleHUDView::FormatGearLine(Gear).ToString(), FString(TEXT("Iron Sword")));
			TestTrue(TEXT("an empty slot reads none"), BattleHUDView::FormatGearTooltip(Gear).ToString().Contains(TEXT("Adapter: none")));
			TestTrue(TEXT("no gear, no line"), BattleHUDView::FormatGearLine(FBattleGearNames()).IsEmpty());
			TestTrue(TEXT("no actor, no gear"), BattleHUDView::ReadGearNames(nullptr).WeaponName.IsEmpty());
		});

		It("writes the durability header by the old header's slot rule (B-391c)", [this]()
		{
			TestEqual(TEXT("nothing to wear: no lines"), BattleHUDView::MakeDurabilityLines(FBattleDurability()).Num(), 0);
			TestEqual(TEXT("no actor: no lines"), BattleHUDView::MakeDurabilityLines(BattleHUDView::ReadDurability(nullptr)).Num(), 0);

			FBattleDurability Both;
			Both.bHasRing = true;
			Both.RingCurrent = BATTLE_HUD_RING_NOW;
			Both.RingMax = BATTLE_HUD_RING_MAX;
			Both.bHasWeapon = true;
			Both.WeaponCurrent = BATTLE_HUD_WEAPON_NOW;
			Both.WeaponMax = BATTLE_HUD_WEAPON_MAX;
			const TArray<FText> Lines = BattleHUDView::MakeDurabilityLines(Both);
			if (TestEqual(TEXT("ring and weapon: two lines"), Lines.Num(), 2))
			{
				TestEqual(TEXT("the ring first"), Lines[0].ToString(), FString(TEXT("AD 3 / 5")));
				TestEqual(TEXT("then the weapon"), Lines[1].ToString(), FString(TEXT("WD 2 / 4")));
			}
			FBattleDurability WeaponOnly = Both;
			WeaponOnly.bHasRing = false;
			const TArray<FText> Weapon = BattleHUDView::MakeDurabilityLines(WeaponOnly);
			TestTrue(TEXT("weapon alone takes the first line"), Weapon.Num() == 1 && Weapon[0].ToString() == TEXT("WD 2 / 4"));
		});

		It("reads Negated only for a hit Broken Darkness took fully (B-391c, Q-391c)", [this]()
		{
			FCombatHitShown Nothing;
			Nothing.DamageType = EWoRDamageType::Fire;
			TestTrue(TEXT("0 on Broken Darkness"), DamageNumberView::IsNegatedHit(Nothing, /*bTargetBrokenDarkness=*/true));
			TestFalse(TEXT("0 on anyone else"), DamageNumberView::IsNegatedHit(Nothing, false));
			FCombatHitShown Landed = Nothing;
			Landed.Amount = BATTLE_HUD_NUMBER;
			TestFalse(TEXT("a hit that took HP"), DamageNumberView::IsNegatedHit(Landed, true));
			FCombatHitShown Heal = Nothing;
			Heal.bHeal = true;
			TestFalse(TEXT("a heal"), DamageNumberView::IsNegatedHit(Heal, true));
			FCombatHitShown Immune = Nothing;
			Immune.ElementMultiplier = DamageNumberView::IMMUNE_MULTIPLIER;
			TestFalse(TEXT("immune keeps its grey number"), DamageNumberView::IsNegatedHit(Immune, true));

			const FDamageNumberStyle Style = DamageNumberView::MakeNegatedStyle(Nothing);
			TestTrue(TEXT("the Negated tier"), Style.Tier == EDamageNumberTier::Negated);
			TestEqual(TEXT("the word"), Style.Text.ToString(), DamageNumberView::GetNegatedLabel().ToString());
			TestTrue(TEXT("in the hit's colour"), Style.Colour.Equals(DamageNumberView::GetDamageTypeColour(EWoRDamageType::Fire)));
		});

		It("gives each menu key to the row it presses, as PressMenuKey does", [this]()
		{
			// B-436g (D-258): the main menu is Attack, Skills, Items, then the corner hints; Back is a key past it.
			const TArray<EPieMenuCategory> Rows = {EPieMenuCategory::Attack, EPieMenuCategory::Skills, EPieMenuCategory::Items,
												   EPieMenuCategory::Defend, EPieMenuCategory::Back};
			const TArray<FCombatMenuKey> &Keys = ACombatPlayerController::GetMenuKeys();
			const auto KeyCategory = [&Keys](int32 KeyIndex)
			{ return Keys.IsValidIndex(KeyIndex) ? Keys[KeyIndex].Category : EPieMenuCategory::None; };

			const TArray<int32> Main = BattleHUDView::AssignMenuKeys(Rows, {true, true, false, true, true}, /*bAtMainMenu=*/true);
			TestTrue(TEXT("Attack row: Attack key"), KeyCategory(Main[0]) == EPieMenuCategory::Attack);
			TestTrue(TEXT("Skills row: Skills key"), KeyCategory(Main[1]) == EPieMenuCategory::Skills);
			TestEqual(TEXT("greyed Items row: no key"), Main[2], INDEX_NONE);
			TestTrue(TEXT("Defend row: Defend key"), KeyCategory(Main[3]) == EPieMenuCategory::Defend);

			const TArray<int32> Sub = BattleHUDView::AssignMenuKeys(Rows, {true, true, true, true, true}, /*bAtMainMenu=*/false);
			TestEqual(TEXT("off the main menu: no Attack key"), Sub[0], INDEX_NONE);
			TestEqual(TEXT("off the main menu: no Items key"), Sub[2], INDEX_NONE);
			TestEqual(TEXT("off the main menu: no Defend key (D-258c: RB is the infusion level there)"), Sub[3], INDEX_NONE);
			if (TestTrue(TEXT("Back row: a Back key"), Keys.IsValidIndex(Sub[4]) && KeyCategory(Sub[4]) == EPieMenuCategory::Back))
			{
				TestTrue(TEXT("the first Back key shows: Backspace, not Esc"), Keys[Sub[4]].Keyboard == EKeys::BackSpace);
			}
		});
	});

	Describe("the widgets on a duel", [this]()
	{
		It("names a party card without its tier and marks it low under 30% HP (B-436b)", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
			if (!TestTrue(TEXT("battle HUD widget and the hero's data"), HUD && HeroData))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe});
			if (!TestTrue(TEXT("two cards, the party's first"), HUD->GetCards().Num() == 2 && HUD->GetCards()[0].bParty))
			{
				HUD->Teardown();
				return;
			}
			// Crown, 2026-11-23: "We don't need the tier on names".
			const FString Name = HUD->GetCards()[0].Name.ToString();
			const FString WithTier = FString::Printf(TEXT("%s (%s)"), *Name, *WoRGameNames::GetTierLetter(WoRCombatTier::GetCombatTier(Hero)).ToString());
			TestTrue(TEXT("wor.UI.Print names the hero"), HUD->GetBattleHUDString().Contains(TEXT("party ") + Name));
			TestFalse(TEXT("no tier letter beside the name"), HUD->GetBattleHUDString().Contains(WithTier));
			TestFalse(TEXT("full HP: not low"), HUD->GetCards()[0].bLowHP);

			int32 LowHP = FMath::FloorToInt(HeroData->MaxHP * BattleHUDView::LOW_HP_FRACTION); // the most HP the HUD's own rule calls low
			while (LowHP > 0 && !BattleHUDView::IsLowHP(true, BattleHUDView::PoolFraction(LowHP, HeroData->MaxHP)))
			{
				--LowHP;
			}
			if (TestTrue(TEXT("the hero has HP to be under 30% and alive"), LowHP >= 1))
			{
				HeroData->ServerSetHP(LowHP);
				TestTrue(TEXT("the card is low"), HeroData->bIsAlive && HUD->GetCards()[0].bLowHP);
				TestTrue(TEXT("wor.UI.Print says it pulses"), HUD->GetBattleHUDString().Contains(TEXT("low HP, pulsing")));
				HeroData->ServerSetHP(HeroData->MaxHP);
				TestFalse(TEXT("healed: not low"), HUD->GetCards()[0].bLowHP);
				HeroData->ServerSetHP(0);
				TestFalse(TEXT("downed: dimmed, never low"), !HeroData->bIsAlive && HUD->GetCards()[0].bLowHP);
				TestFalse(TEXT("downed: no pulse in wor.UI.Print"), HUD->GetBattleHUDString().Contains(TEXT("low HP, pulsing")));
			}
			HUD->Teardown();
		});

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
			TestEqual(TEXT("B-503m: the EP bar's fill"), HUD->GetCards()[0].EPFraction,
					  BattleHUDView::PoolFraction(HeroData->CurrentEP, HeroData->MaxEP));

			Hero->FindComponentByClass<UCharacterDataComponent>()->ServerTakeDamage(BATTLE_HUD_HIT);
			TestEqual(TEXT("HP line after the hit"), HUD->GetCards()[0].HPLine.ToString(),
					  BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), HeroData->CurrentHP, HeroData->MaxHP).ToString());

			const AActor *Acting = Orchestrator->GetCurrentActor();
			TestTrue(TEXT("the turn strip leads with the acting character"),
					 !HUD->GetTurnLines().IsEmpty() && HUD->GetTurnLines()[0].ToString().Contains(BattleHUDView::GetCombatantName(Acting).ToString()));
			TestTrue(TEXT("the acting card is marked"), HUD->GetCards()[0].bActing == (Acting == Hero));
			// B-590: the hexagons follow the turn order and the settings, read on each refresh.
			TestTrue(TEXT("the hexagons lead with the acting character"),
					 !HUD->GetTurnHexes().IsEmpty() && HUD->GetTurnHexes()[0].bCurrent && HUD->GetTurnHexes()[0].Actor.Get() == Acting);
			TestFalse(TEXT("a row by default"), HUD->IsTurnOrderColumn());
			if (USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>(); TestNotNull(TEXT("save"), Save))
			{
				Save->SetTurnsShown(BattleHUDView::MIN_TURNS_SHOWN);
				Save->SetTurnOrderColumn(true);
				if (UBattleHUDWidget *Set = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass()); TestNotNull(TEXT("a second HUD"), Set))
				{
					Set->Initialise({Hero}, {Foe});
					TestEqual(TEXT("Turns shown 1: the current turn alone"), Set->GetTurnHexes().Num(), 1);
					TestTrue(TEXT("the column layout"), Set->IsTurnOrderColumn());
					TestTrue(TEXT("the print names the layout"), Set->GetBattleHUDString().Contains(TEXT("turn order: column, 1 hexagons")));
					Set->Teardown();
				}
				Save->SetTurnsShown(BattleHUDView::DEFAULT_TURNS_SHOWN);
				Save->SetTurnOrderColumn(false);
			}

			HUD->SetStatusPanelOpen(true);
			TestTrue(TEXT("the status panel names every combatant"),
					 HUD->GetStatusPanelText().ToString().Contains(HUD->GetCards()[1].Name.ToString()));
			HUD->Teardown();
		});

		It("keeps Spirit and Body off a fight's card and shows them in Intel on a card click (D-254, D-261)", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UPackage *Package = GetTransientPackage();
			UCharacterData *PlayerData = NewObject<UCharacterData>(
				Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(), *(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_InspectSpec"))));
			PlayerData->CharacterClass = ECharacterClass::Caster;
			PlayerData->InnateElement = ESpellElement::Fire;
			AActor *Player = TestWorld->SpawnCombatant(PlayerData);
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			UCharacterDataComponent *PlayerComp = Player ? Player->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestNotNull(TEXT("a player character"), PlayerComp) || !TestNotNull(TEXT("battle HUD widget"), HUD))
			{
				return;
			}
			constexpr int32 PLAYER_CARD = 0;
			constexpr int32 HERO_CARD = 1;
			constexpr int32 FOE_CARD = 2;
			HUD->Initialise({Player, Hero}, {Foe});
			const FString Identity = WoRPlayerBuild::GetDetailLine(PlayerData, PlayerComp->PartyMemberId, TestWorld->GetWorld()).ToString();
			TestTrue(TEXT("the character has identity lines to show"), Identity.StartsWith(TEXT("Spirit of")));
			TestTrue(TEXT("no identity lines on the fight's card"), HUD->GetCards()[PLAYER_CARD].DetailLine.IsEmpty());
			TestFalse(TEXT("closed to start"), HUD->IsIntelOpen() || HUD->IsStatusPanelOpen());

			// D-261 (B-586): the card click opens Intel on that character's tab, not the status panel (D-254's inspect).
			HUD->InspectCard(PLAYER_CARD);
			const auto OpenPage = [HUD]() -> const FBattleIntelPage & { return HUD->GetIntelPages()[HUD->GetIntelTab()]; };
			if (!TestTrue(TEXT("a card click opens Intel on that character"), HUD->IsIntelOpen() && !HUD->IsStatusPanelOpen() &&
																				   HUD->GetIntelPages().IsValidIndex(HUD->GetIntelTab()) &&
																				   OpenPage().Actor.Get() == Player))
			{
				return;
			}
			TestTrue(TEXT("Intel names the character and holds its Spirit / Body lines"),
					 OpenPage().Heading.EqualTo(HUD->GetCards()[PLAYER_CARD].Name) && OpenPage().Subline.ToString() == Identity);
			TestEqual(TEXT("Intel holds the full actions line: pips, Max and the action-point progress (D-259)"), OpenPage().ActionsLine.ToString(),
					  ActionPipsText::MakeActionsLine(PlayerComp->CurrentActions, PlayerComp->GetMaxActions(),
													  PlayerComp->GetActionPointProgress(), PlayerComp->SkillsUsedThisTurn).ToString());
			TestTrue(TEXT("Intel shows no effects"), OpenPage().Effects.IsEmpty());
			TestTrue(TEXT("wor.UI.Print shows the open Intel page"), HUD->GetBattleHUDString().Contains(TEXT("intel tab")));

			PlayerComp->GainActions(PlayerComp->GetMaxActions() + BATTLE_HUD_ONE_ACTION, /*bMayPassMax=*/true);
			if (TestTrue(TEXT("the bank is above Max"), PlayerComp->CurrentActions > PlayerComp->GetMaxActions()))
			{
				TestEqual(TEXT("the open Intel page follows the bank"), OpenPage().ActionsLine.ToString(),
						  ActionPipsText::MakeActionsLine(PlayerComp->CurrentActions, PlayerComp->GetMaxActions(), PlayerComp->GetActionPointProgress(),
														  PlayerComp->SkillsUsedThisTurn).ToString());
				TestTrue(TEXT("and warns that bonus actions are lost at turn end"), OpenPage().ActionsLine.ToString().Contains(TEXT("lost at turn end")));
				TestEqual(TEXT("the short line has the bonus as ◆ pips, with no warning text (D-259; drawn on the menu's tab since D-260)"), HUD->GetCards()[PLAYER_CARD].ActionsLine.ToString(),
						  ActionPipsText::MakeCardActionsLine(PlayerComp->CurrentActions, PlayerComp->GetMaxActions(), PlayerComp->SkillsUsedThisTurn).ToString());
				TestTrue(TEXT("a ◆ on the card"), HUD->GetCards()[PLAYER_CARD].ActionsLine.ToString().Contains(TEXT("◆")));
			}

			HUD->InspectCard(HERO_CARD);
			TestTrue(TEXT("another card switches to its tab"), HUD->IsIntelOpen() && OpenPage().Actor.Get() == Hero);
			HUD->InspectCard(FOE_CARD);
			TestTrue(TEXT("an enemy card opens its own tab now (D-261)"), HUD->IsIntelOpen() && OpenPage().bEnemy && OpenPage().Actor.Get() == Foe);
			HUD->InspectCard(HERO_CARD);
			HUD->InspectCard(HERO_CARD);
			TestFalse(TEXT("the same card again closes Intel"), HUD->IsIntelOpen() || HUD->IsStatusPanelOpen());
			HUD->Teardown();
		});

		It("features the party member acting, keeps it through an enemy's turn, and never an enemy card (B-436a)", [this]()
		{
			USaveSubsystem *Save = CreateTestWorld() ? TestWorld->GetSubsystem<USaveSubsystem>() : nullptr;
			if (!TestNotNull(TEXT("the save"), Save))
			{
				return;
			}
			const bool bHiddenBefore = Save->IsEndTurnButtonHidden();
			ON_SCOPE_EXIT { Save->SetEndTurnButtonHidden(bHiddenBefore); };
			Save->SetEndTurnButtonHidden(false); // the End Turn row hands the turn to Foe below
			if (!StartDuel())
			{
				return;
			}
			AActor *Bystander = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData()); // a party card that never acts
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (!TestNotNull(TEXT("a second party member"), Bystander) || !TestNotNull(TEXT("battle HUD widget"), HUD))
			{
				return;
			}
			constexpr int32 HERO_CARD = 1;
			HUD->Initialise({Bystander, Hero}, {Foe});
			TestEqual(TEXT("Hero is acting: Hero's card is featured, not the first card"), HUD->GetFeaturedCardIndex(), HERO_CARD);
			TestTrue(TEXT("wor.UI.Print marks the featured card and the compact one"),
					 HUD->GetBattleHUDString().Contains(TEXT("featured (full card)")) && HUD->GetBattleHUDString().Contains(TEXT("| compact")));

			UBattleCommandMenuWidget *MenuWidget = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
			if (TestNotNull(TEXT("command menu widget"), MenuWidget))
			{
				MenuWidget->Initialise();
				const int32 EndTurnRow = FindLabel(MenuWidget->GetRowLabels(), WoRGameNames::GetEndTurnVerb().ToString());
				if (TestTrue(TEXT("an End Turn row"), EndTurnRow != INDEX_NONE))
				{
					MenuWidget->PressRow(EndTurnRow);
					TestTrue(TEXT("it is Foe's turn"), Orchestrator->GetCurrentActor() == Foe);
					TestFalse(TEXT("Hero's card is no longer marked acting"), HUD->GetCards()[HERO_CARD].bActing);
					TestEqual(TEXT("Hero's card stays featured through the enemy's turn"), HUD->GetFeaturedCardIndex(), HERO_CARD);
				}
				MenuWidget->Teardown();
			}
			HUD->Teardown();

			UBattleHUDWidget *NoActorYet = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (TestNotNull(TEXT("a HUD whose party has not acted"), NoActorYet))
			{
				NoActorYet->Initialise({Bystander}, {Foe});
				TestEqual(TEXT("no party member has acted: the first party card is featured"), NoActorYet->GetFeaturedCardIndex(), 0);
				NoActorYet->Teardown();
			}
			UBattleHUDWidget *EnemiesOnly = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (TestNotNull(TEXT("a HUD with no party"), EnemiesOnly))
			{
				EnemiesOnly->Initialise({}, {Foe});
				TestEqual(TEXT("no party card: nothing featured"), EnemiesOnly->GetFeaturedCardIndex(), static_cast<int32>(INDEX_NONE));
				EnemiesOnly->Teardown();
			}
			const FVector2D MenuOffset = UBattleHUDWidget::GetCommandMenuOffset();
			TestTrue(TEXT("the command menu sits in from the left edge and above the bottom"), MenuOffset.X > 0.0f && MenuOffset.Y < 0.0f);
		});

		It("floats a damage number over a hit character, stacks a second, and drops a hit that took nothing (B-391b)", [this]()
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
			FCombatHitShown Weak;
			Weak.Amount = BATTLE_HUD_NUMBER;
			Weak.ElementMultiplier = BATTLE_HUD_WEAK;
			Weak.DamageType = EWoRDamageType::Fire;
			HUD->ShowDamageNumber(Foe, Weak);
			HUD->ShowDamageNumber(Foe, Weak);
			FCombatHitShown Nothing = Weak;
			Nothing.Amount = 0;
			HUD->ShowDamageNumber(Foe, Nothing);
			TestEqual(TEXT("two live numbers, the 0 dropped"), HUD->GetLiveDamageNumberCount(), 2);
			const FBattleDamageNumber *Second = HUD->GetDamageNumbers().FindByPredicate(
				[](const FBattleDamageNumber &Number) { return Number.bLive && Number.StackIndex == 1; });
			TestTrue(TEXT("the second stacks above the first"), Second != nullptr);
			TestTrue(TEXT("styled weak"), Second && Second->Style.Tier == EDamageNumberTier::Weak);
			TestTrue(TEXT("wor.UI.Print shows it"), HUD->GetBattleHUDString().Contains(TEXT("2 live")));
			HUD->Teardown();
			TestEqual(TEXT("teardown clears them"), HUD->GetLiveDamageNumberCount(), 0);
		});

		It("follows a hit's buildup on the card's bar and line, and cracks it (B-391c)", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UStatusBuildupManager *Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (!TestTrue(TEXT("buildup manager and battle HUD widget"), Buildup && HUD))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe});
			if (!TestEqual(TEXT("two cards"), HUD->GetCards().Num(), 2))
			{
				HUD->Teardown();
				return;
			}
			const FBattleHUDCard &FoeCard = HUD->GetCards()[1];
			TestEqual(TEXT("seeded empty"), FoeCard.BuildupFraction, 0.0f);

			Buildup->AddStatusBuildup(Hero, Foe, BATTLE_HUD_BUILDUP, ESpellElement::Fire, EPhysicalDamageType::None);
			const float Built = Buildup->GetStatusBarBuildup(Foe);
			TestTrue(TEXT("the hit built some"), Built > 0.0f);
			TestEqual(TEXT("the line follows the bar"), FoeCard.BuildupLine.ToString(),
					  BattleHUDView::FormatBuildupLine(Built, CombatConstants::STATUS_EFFECT_THRESHOLD).ToString());
			TestEqual(TEXT("the fill follows the bar"), FoeCard.BuildupFraction,
					  BattleHUDView::BuildupFraction(Built, CombatConstants::STATUS_EFFECT_THRESHOLD));

			Buildup->OnStatusBuildupChanged.Broadcast(Foe, BATTLE_HUD_CRACKED, CombatConstants::STATUS_EFFECT_THRESHOLD, ESpellElement::Fire, nullptr);
			TestEqual(TEXT("cracked past half"), FoeCard.CracksShown,
					  BattleHUDView::CountCracksShown(BATTLE_HUD_CRACKED / CombatConstants::STATUS_EFFECT_THRESHOLD));
			TestTrue(TEXT("wor.UI.Print shows it"), HUD->GetBattleHUDString().Contains(TEXT("crack(s)")));
			HUD->Teardown();
		});

		It("names a party card's gear, and floats Negated over a Broken Darkness hit that took nothing (B-391c)", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			UCharacterDataComponent *FoeData = Foe->FindComponentByClass<UCharacterDataComponent>();
			if (!TestTrue(TEXT("battle HUD widget and the foe's data"), HUD && FoeData))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe});
			if (!TestEqual(TEXT("two cards"), HUD->GetCards().Num(), 2))
			{
				HUD->Teardown();
				return;
			}
			const FBattleHUDCard &HeroCard = HUD->GetCards()[0];
			TestEqual(TEXT("the gear line reads the loadout"), HeroCard.GearLine.ToString(),
					  BattleHUDView::FormatGearLine(BattleHUDView::ReadGearNames(Hero)).ToString());
			TestTrue(TEXT("the tooltip names the loadout"), HeroCard.GearTooltip.ToString().Contains(TEXT("Loadout:")));
			TestTrue(TEXT("an enemy card has no gear"), HUD->GetCards()[1].GearTooltip.IsEmpty());

			FCombatHitShown Nothing;
			Nothing.DamageType = EWoRDamageType::Fire;
			HUD->ShowDamageNumber(Foe, Nothing);
			TestEqual(TEXT("a 0 on a normal foe: nothing"), HUD->GetLiveDamageNumberCount(), 0);
			FoeData->ServerSetBrokenDarkness(true);
			if (!TestTrue(TEXT("the foe is in Broken Darkness"), FoeData->IsBrokenDarkness()))
			{
				HUD->Teardown();
				return;
			}
			HUD->ShowDamageNumber(Foe, Nothing);
			const FBattleDamageNumber *Negated = HUD->GetDamageNumbers().FindByPredicate(
				[](const FBattleDamageNumber &Number) { return Number.bLive; });
			TestTrue(TEXT("Negated floats"), DamageNumberView::SHOW_NEGATED_CUE == (Negated != nullptr));
			TestTrue(TEXT("styled Negated"), !Negated || Negated->Style.Tier == EDamageNumberTier::Negated);
			HUD->Teardown();
		});

		It("shows the acting character's durability header as the old header (B-391c)", [this]()
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
			// Review fix: give the acting hero a gemmed weapon, so the header has a line to check (as CrystalBreakSpec).
			UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
			ULoadoutComponent *HeroLoadout = Hero->FindComponentByClass<ULoadoutComponent>();
			UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Hero);
			if (!TestTrue(TEXT("hero data, loadout and inventory"), HeroData && HeroLoadout && Inventory))
			{
				return;
			}
			HeroLoadout->InitializeFromCharacterData(HeroData->CharacterData, Inventory);
			if (!TestTrue(TEXT("an active saved loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
			{
				return;
			}
			FRuntimeAttachedItem Gem;
			Gem.Kind = EAttachedItemKind::Crystal;
			Gem.Crystal = FCrystalAttachment(BATTLE_HUD_GEM, BATTLE_HUD_GEM_DURABILITY);
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimarySlotType = EPrimarySlotType::Weapon;
			Active.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Active.PrimaryWeapon.WeaponEntry.AttachedItem = Gem;
			HeroLoadout->NotifyActiveWeaponChanged(); // a direct edit, so announce it (B-138)

			MenuWidget->Initialise(); // replays the open menu, which reads the header
			const TArray<FText> &Lines = MenuWidget->GetDurabilityLines();
			if (TestTrue(TEXT("Hero's menu, with a header line"), Menu->GetCurrentActor() == Hero && Lines.Num() > 0))
			{
				const FString Expected = FString::Printf(TEXT("WD %d / "), BATTLE_HUD_GEM_DURABILITY);
				TestTrue(TEXT("the weapon gem's durability"), Lines.Last().ToString().StartsWith(Expected));
			}
			TestTrue(TEXT("wor.UI.Print shows it"), MenuWidget->GetCommandMenuString().Contains(TEXT("WD ")));
			MenuWidget->Teardown();
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
			// D-260: no "Your turn" title; the header is empty on the main menu.
			TestTrue(TEXT("showing Hero's menu, with no title on the main menu"), MenuWidget->IsShowing() && MenuWidget->GetTitle().IsEmpty());
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
				TestTrue(TEXT("the header follows the menu (D-260)"), MenuWidget->GetTitle().EqualTo(Menu->GetViewTitle()));
			}
			MenuWidget->Teardown();
		});

		It("draws the menu as one frame: the Act tab, Attack / Skills / Items, the rest as corner hints (B-436g, D-258, D-260)", [this]()
		{
			if (!StartDuel())
			{
				return;
			}
			UBattleCommandMenuWidget *MenuWidget = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
			UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
			if (!TestNotNull(TEXT("command menu widget"), MenuWidget) || !TestNotNull(TEXT("Hero data"), HeroData))
			{
				return;
			}
			MenuWidget->Initialise();
			TestEqual(TEXT("the tab is Hero's short actions line (D-259 rules)"), MenuWidget->GetActionsLine().ToString(),
					  ActionPipsText::MakeCardActionsLine(HeroData->CurrentActions, HeroData->GetMaxActions(), HeroData->SkillsUsedThisTurn).ToString());

			const TArray<FText> &Labels = MenuWidget->GetRowLabels();
			TArray<FString> HintLabels;
			for (const FBattleMenuHint &Hint : MenuWidget->GetHints())
			{
				HintLabels.Add(Hint.Label.ToString());
				TestTrue(TEXT("a hint presses a real row"), Labels.IsValidIndex(Hint.RowIndex));
				TestEqual(TEXT("a hint with a key carries the key, drawn as the rows' cap (B-436g QA)"), Hint.KeyId.IsValid(), !Hint.Key.IsEmpty());
			}
			TestTrue(TEXT("Defend is a corner hint"), HintLabels.ContainsByPredicate([](const FString &Label) { return Label.Contains(TEXT("Defend")); }));
			TestFalse(TEXT("Attack is a frame row, not a hint"), HintLabels.ContainsByPredicate([](const FString &Label) { return Label.Contains(TEXT("Attack")); }));
			TestTrue(TEXT("main menu: the three frame rows"), !UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Attack, true) &&
																 !UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Skills, true) &&
																 !UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Items, true));
			TestTrue(TEXT("main menu: Flee, End Turn and the steps are hints"), UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Flee, true) &&
																				UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::EndTurn, true) &&
																				UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::MoveRow, true));
			TestTrue(TEXT("past it only Back is a hint"), UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Back, false) &&
															  !UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Target, false) &&
															  !UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory::Item, false));

			FPieMenuButtonData Attack;
			Attack.Category = EPieMenuCategory::Attack;
			Menu->HandleSelection(Attack); // the target picker
			TestFalse(TEXT("the target screen has a header"), MenuWidget->GetTitle().IsEmpty());
			const int32 BackRow = FindLabel(MenuWidget->GetRowLabels(), TEXT("Back"));
			if (TestTrue(TEXT("Back is there"), BackRow != INDEX_NONE))
			{
				TestTrue(TEXT("with its key (Backspace headless, D-258)"), MenuWidget->GetRowLabels()[BackRow].ToString().StartsWith(TEXT("[")));
				TestTrue(TEXT("as the one corner hint"), MenuWidget->GetHints().Num() == 1 && MenuWidget->GetHints()[0].RowIndex == BackRow);
				MenuWidget->PressRow(BackRow);
				TestTrue(TEXT("Back goes to the main menu"), Menu->IsAtMainMenu() && MenuWidget->GetTitle().IsEmpty());
			}
			TestTrue(TEXT("wor.UI.Print shows the tab and the hints"), MenuWidget->GetCommandMenuString().Contains(TEXT("tab=")) &&
																		   MenuWidget->GetCommandMenuString().Contains(TEXT(" hint ")));
			MenuWidget->Teardown();
		});

		It("shows the party's action pips live and an End Turn row (B-170e)", [this]()
		{
			USaveSubsystem *Save = CreateTestWorld() ? TestWorld->GetSubsystem<USaveSubsystem>() : nullptr;
			if (!TestNotNull(TEXT("the save"), Save))
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
				// D-254, D-259 (B-581): the card's line is the short one ("Act: ●●○"); the full line moved to inspect.
				return ActionPipsText::MakeCardActionsLine(HeroData->CurrentActions, HeroData->GetMaxActions()).ToString();
			};
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

		It("highlights a hovered target row's enemy in the target picker (B-400b)", [this]()
		{
			// StartDuel's one foe, plus a second, so the picker offers two targets (B-400's target view).
			TestWorld = MakeUnique<FWoRTestWorld>();
			if (!TestTrue(TEXT("Test world created"), TestWorld->Create()))
			{
				return;
			}
			Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
			Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *SecondFoe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestTrue(TEXT("Orchestrator, menu and combatants"), Orchestrator && Menu && Hero && Foe && SecondFoe))
			{
				return;
			}
			for (AActor *Actor : {Hero, Foe, SecondFoe})
			{
				BattleHUDAddLoadout(Actor);
			}
			Menu->SetCombatOrchestrator(Orchestrator);
			Orchestrator->StartCombat({Hero}, {Foe, SecondFoe});
			TestWorld->Tick(BATTLE_HUD_SETTLE_FRAMES);
			if (!TestTrue(TEXT("Hero's menu is open on turn 1"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero))
			{
				return;
			}
			UBattleCommandMenuWidget *MenuWidget = CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass());
			if (!TestNotNull(TEXT("command menu widget"), MenuWidget))
			{
				return;
			}
			MenuWidget->Initialise();

			const int32 DefendRow = FindLabel(MenuWidget->GetRowLabels(), TEXT("Defend"));
			if (TestTrue(TEXT("a Defend row to hover"), DefendRow != INDEX_NONE))
			{
				MenuWidget->HoverRow(DefendRow);
				TestNull(TEXT("hovering a non-target row highlights nothing"), Menu->GetHighlightedTarget());
			}

			FPieMenuButtonData Attack;
			Attack.Category = EPieMenuCategory::Attack;
			Menu->HandleSelection(Attack); // the widget follows the menu's rebroadcast
			int32 FoeRow = INDEX_NONE;
			int32 SecondFoeRow = INDEX_NONE;
			for (int32 Row = 0; Row < MenuWidget->GetRowLabels().Num(); ++Row)
			{
				FoeRow = MenuWidget->GetRowTarget(Row) == Foe ? Row : FoeRow;
				SecondFoeRow = MenuWidget->GetRowTarget(Row) == SecondFoe ? Row : SecondFoeRow;
			}
			if (TestTrue(TEXT("a target row per enemy"), FoeRow != INDEX_NONE && SecondFoeRow != INDEX_NONE))
			{
				MenuWidget->HoverRow(SecondFoeRow);
				TestTrue(TEXT("hover highlights the second enemy"), Menu->GetHighlightedTarget() == SecondFoe);
				MenuWidget->HoverRow(FoeRow);
				TestTrue(TEXT("and back to the first"), Menu->GetHighlightedTarget() == Foe);
			}
			MenuWidget->Teardown();
		});

		It("shows an enemy's card only as the marker over the highlighted target, with its tier (B-436c, D-263)", [this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			if (!TestTrue(TEXT("Test world created"), TestWorld->Create()))
			{
				return;
			}
			Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
			Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *SecondFoe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestTrue(TEXT("Orchestrator, menu and combatants"), Orchestrator && Menu && Hero && Foe && SecondFoe))
			{
				return;
			}
			for (AActor *Actor : {Hero, Foe, SecondFoe})
			{
				BattleHUDAddLoadout(Actor);
			}
			Menu->SetCombatOrchestrator(Orchestrator);
			Orchestrator->StartCombat({Hero}, {Foe, SecondFoe});
			TestWorld->Tick(BATTLE_HUD_SETTLE_FRAMES);
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (!TestTrue(TEXT("Hero's menu is open"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero) || !TestNotNull(TEXT("battle HUD"), HUD))
			{
				return;
			}
			HUD->Initialise({Hero}, {Foe, SecondFoe});
			const TSharedRef<SWidget> Built = HUD->TakeWidget(); // builds the cards; held, or its release tears the HUD down
			const TArray<FBattleHUDCard> &Cards = HUD->GetCards();
			const auto CardOf = [&Cards](const AActor *Actor) { return Cards.IndexOfByPredicate([Actor](const FBattleHUDCard &Card) { return Card.Actor.Get() == Actor; }); };
			const int32 FoeCard = CardOf(Foe);
			const int32 SecondCard = CardOf(SecondFoe);
			if (!TestTrue(TEXT("both enemy cards built"), Cards.IsValidIndex(FoeCard) && Cards.IsValidIndex(SecondCard) && Cards[FoeCard].Frame && Cards[SecondCard].Frame))
			{
				return;
			}
			const auto Shown = [&Cards](int32 Card) { return Cards[Card].Frame->GetVisibility() == ESlateVisibility::Visible; };
			TestEqual(TEXT("the command state: no marker"), HUD->GetMarkedCardIndex(), INDEX_NONE);
			TestFalse(TEXT("and no enemy card"), Shown(FoeCard) || Shown(SecondCard));
			TestNotNull(TEXT("the marker layer"), HUD->GetWidgetFromName(TEXT("TargetMarkers")));
			TestNull(TEXT("the old column is gone"), HUD->GetWidgetFromName(TEXT("Enemies")));
			TestEqual(TEXT("an enemy card reads its tier"), Cards[FoeCard].TierLine.ToString(),
					  WoRCombatTier::GetCombatTierText(WoRCombatTier::GetCombatTier(Foe)).ToString());
			TestTrue(TEXT("the tier names no rank"), Cards[FoeCard].TierLine.ToString().StartsWith(TEXT("Tier ")));
			TestTrue(TEXT("still no HP number on it (Q-391)"), Cards[FoeCard].HPBar && !Cards[FoeCard].HPText);
			// B-582c (Crown, 2026-11-23): the floating card keeps its HP bar, buildup bar and status line, not the tier line.
			TestTrue(TEXT("no tier line drawn on any card"), !Cards[FoeCard].TierText && !Cards[CardOf(Hero)].TierText);
			TestTrue(TEXT("the enemy card keeps its HP bar"), Cards[FoeCard].HPBar != nullptr);

			FPieMenuButtonData Attack;
			Attack.Category = EPieMenuCategory::Attack;
			Menu->HandleSelection(Attack);
			const int32 First = CardOf(Menu->GetHighlightedTarget());
			const int32 Other = First == FoeCard ? SecondCard : FoeCard;
			TestTrue(TEXT("the picker marks its highlighted enemy"), First != INDEX_NONE && HUD->GetMarkedCardIndex() == First);
			TestTrue(TEXT("its card shows, the other's not"), First != INDEX_NONE && Shown(First) && !Shown(Other));
			Menu->CycleHighlightedTarget(1);
			TestEqual(TEXT("the marker follows the highlight"), HUD->GetMarkedCardIndex(), Other);
			TestTrue(TEXT("on the other card now"), Shown(Other) && !Shown(First));
			TestTrue(TEXT("wor.UI.Print names it"), HUD->GetBattleHUDString().Contains(TEXT("follows the picker): ") + Cards[Other].Name.ToString()));
			Menu->HandleBack();
			TestEqual(TEXT("Back to the main menu: no marker"), HUD->GetMarkedCardIndex(), INDEX_NONE);
			TestFalse(TEXT("and no enemy card"), Shown(FoeCard) || Shown(SecondCard));
			HUD->Teardown();
		});

		It("ghosts the picked infusion's HP cost on the caster's HP bar, KILL red when lethal, gone when the step closes (B-592b)", [this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			if (!TestTrue(TEXT("Test world created"), TestWorld->Create()))
			{
				return;
			}
			Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
			Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			if (!TestTrue(TEXT("Orchestrator, menu and combatants"), Orchestrator && Menu && Hero && Foe))
			{
				return;
			}
			BattleHUDAddLoadout(Hero);
			BattleHUDAddLoadout(Foe);
			UInfusionVFXComponent *VFX = NewObject<UInfusionVFXComponent>(Hero);
			Hero->AddInstanceComponent(VFX);
			VFX->RegisterComponent();
			Menu->SetCombatOrchestrator(Orchestrator);
			Orchestrator->StartCombat({Hero}, {Foe});
			TestWorld->Tick(BATTLE_HUD_SETTLE_FRAMES);
			UCharacterDataComponent *Stats = Hero->FindComponentByClass<UCharacterDataComponent>();
			const UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
			UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
			if (!TestTrue(TEXT("Hero's menu is open"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero) || !TestNotNull(TEXT("battle HUD"), HUD) ||
				!TestNotNull(TEXT("Stats"), Stats) || !TestNotNull(TEXT("ActionExecutor"), Executor))
			{
				return;
			}
			Stats->MaxEP = Stats->CurrentEP = BATTLE_HUD_GHOST_POOL;
			Stats->MaxHP = Stats->CurrentHP = BATTLE_HUD_GHOST_POOL;
			HUD->Initialise({Hero}, {Foe});
			const TSharedRef<SWidget> Built = HUD->TakeWidget(); // builds the cards; held, or its release tears the HUD down
			ON_SCOPE_EXIT { HUD->Teardown(); };
			const TArray<FBattleHUDCard> &Cards = HUD->GetCards();
			const int32 HeroCard = Cards.IndexOfByPredicate([this](const FBattleHUDCard &Card) { return Card.Actor.Get() == Hero; });
			if (!TestTrue(TEXT("the hero's card with its ghost built"), Cards.IsValidIndex(HeroCard) && Cards[HeroCard].HPGhostRow && Cards[HeroCard].HPGhost))
			{
				return;
			}
			const FBattleHUDCard &Card = Cards[HeroCard];
			const auto GhostShown = [&Card]() { return Card.HPGhostRow->GetVisibility() != ESlateVisibility::Collapsed; };
			TestFalse(TEXT("the command state: no ghost"), Card.HPGhostFraction > 0.0f || GhostShown());

			UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
			Ability->BaseEnergyCost = BATTLE_HUD_GHOST_ABILITY_EP;
			FPieMenuButtonData Use;
			Use.Category = EPieMenuCategory::Ability;
			Use.DataReference = Ability;
			const auto PickedRaw = [VFX]()
			{
				return VFX->GetCachedSources().IsValidIndex(VFX->GetCurrentSourceIndex()) &&
					   VFX->GetCachedSources()[VFX->GetCurrentSourceIndex()] == EInfusionSourceOption::Raw;
			};
			// The ability's target step with Raw at L2, through the menu's own keys (each rebroadcasts the picker).
			const auto OpenRawL2 = [this, &Use, &PickedRaw]()
			{
				Menu->HandleSelection(Use);
				for (int32 Press = 0; Press < BATTLE_HUD_MAX_SOURCE_PRESSES && !PickedRaw(); ++Press)
				{
					Menu->PressMenuKey(EPieMenuCategory::CycleSource);
				}
				for (int32 Press = 0; Press < BATTLE_HUD_GHOST_LEVEL; ++Press)
				{
					Menu->PressMenuKey(EPieMenuCategory::CycleLevel);
				}
				return TestTrue(TEXT("Raw at L2 picked"), Menu->GetPendingInfusionLevel() == BATTLE_HUD_GHOST_LEVEL && PickedRaw());
			};
			Menu->HandleSelection(Use);
			TestFalse(TEXT("the target step at L0: no ghost"), Card.HPGhostFraction > 0.0f);
			Menu->HandleBack();
			if (!OpenRawL2())
			{
				return;
			}
			const FInfusionCostPreview Cost = Menu->GetTargetStepCostPreview();
			TestTrue(TEXT("Raw L2 costs HP"), Cost.HP > 0 && !Cost.bWouldKill);
			TestNearlyEqual(TEXT("the ghost is that share of max HP"), Card.HPGhostFraction, BattleHUDView::PoolFraction(Cost.HP, BATTLE_HUD_GHOST_POOL));
			TestTrue(TEXT("shown, not KILL red"), GhostShown() && !Card.bHPGhostKills);
			TestTrue(TEXT("wor.UI.Print shows it"), HUD->GetBattleHUDString().Contains(TEXT("HP ghost")));

			Menu->PressMenuKey(EPieMenuCategory::CycleLevel);
			TestFalse(TEXT("the level wraps to L0: the ghost follows, gone"), Card.HPGhostFraction > 0.0f || GhostShown());
			Stats->CurrentHP = Cost.HP; // the same cost now takes the hero to 0
			Menu->PressMenuKey(EPieMenuCategory::CycleLevel);
			Menu->PressMenuKey(EPieMenuCategory::CycleLevel);
			TestTrue(TEXT("back at L2, lethal: KILL red"), Card.bHPGhostKills && GhostShown());
			TestTrue(TEXT("wor.UI.Print says so"), HUD->GetBattleHUDString().Contains(TEXT("(would kill)")));

			Menu->HandleBack();
			TestFalse(TEXT("Back closes the step: no ghost"), Card.HPGhostFraction > 0.0f || Card.bHPGhostKills || GhostShown());
			TestEqual(TEXT("and the menu previews nothing"), Menu->GetTargetStepCostPreview().HP, 0);

			Stats->CurrentHP = BATTLE_HUD_GHOST_POOL;
			if (!OpenRawL2())
			{
				return;
			}
			TestTrue(TEXT("the ghost is back"), Card.HPGhostFraction > 0.0f);
			TestTrue(TEXT("Enter commits the action"), Menu->PressMenuKey(EPieMenuCategory::Target));
			TestFalse(TEXT("committed: no ghost"), Card.HPGhostFraction > 0.0f || GhostShown());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
