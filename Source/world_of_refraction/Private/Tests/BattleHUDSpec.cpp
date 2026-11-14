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
#include "Combat/CombatConstants.h"
#include "HAL/IConsoleManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsText.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
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

		It("shows a Broken Darkness absorb line and its overload (B-391b)", [this]()
		{
			TestEqual(TEXT("absorb line"), BattleHUDView::FormatAbsorbLine(80, 100).ToString(), FString(TEXT("Absorb 80 / 100")));
			TestFalse(TEXT("at max: not overloaded"), BattleHUDView::IsOverloaded(100, 100));
			TestTrue(TEXT("past max: overloaded"), BattleHUDView::IsOverloaded(115, 100));
			const FString Over = BattleHUDView::FormatAbsorbLine(130, 100).ToString();
			TestTrue(TEXT("overload line"), Over.StartsWith(TEXT("Absorb 130 / 100, overload")) && Over.Contains(TEXT("130")));
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

			TestEqual(TEXT("line"), BattleHUDView::FormatBuildupLine(40.7f, 100.0f).ToString(), FString(TEXT("Status 40 / 100")));
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
														  Tooltip.Contains(TEXT("Ring: Ember Ring")));
			Gear.RingName.Reset();
			TestEqual(TEXT("weapon alone"), BattleHUDView::FormatGearLine(Gear).ToString(), FString(TEXT("Iron Sword")));
			TestTrue(TEXT("an empty slot reads none"), BattleHUDView::FormatGearTooltip(Gear).ToString().Contains(TEXT("Ring: none")));
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
				return ActionPipsText::MakeActionsLine(HeroData->CurrentActions, HeroData->GetMaxActions(), HeroData->GetActionPointProgress())
					.ToString();
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
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
