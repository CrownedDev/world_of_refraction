// BattleIntelSpec.cpp
// B-586 (D-261, Q-586): the fight's Intel panel, headless. A stat reads before -> now only when an effect changed it;
// every effect family has an amount and its turns left; a hit teaches one Weak to / Resists line for that enemy type,
// kept for the run (another fight still shows it); your own character's tab reads "You"; the HUD opens Intel on a
// card, steps its tabs and never shows it with the status panel. The look is PIE (docs/PIE/B-586.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/PlayerBuild.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPresentationComponent.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/WoRFightSubsystem.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Combat/Battle/BattleCommandMenuWidget.h"
#include "UI/Combat/Battle/BattleHUDWidget.h"
#include "UI/Combat/Battle/BattleIntelView.h"
#include "UI/Combat/Battle/BattleIntelWidget.h"

namespace
{
	constexpr int32 INTEL_STAT_COUNT = 11; // the Character sheet's eleven
	constexpr int32 INTEL_BUFF_ID = 5861;
	constexpr int32 INTEL_DEBUFF_ID = 5862;
	constexpr int32 INTEL_RESIST_ID = 5863;
	constexpr int32 INTEL_DOT_ID = 5864;
	constexpr int32 INTEL_STUN_ID = 5865;
	constexpr int32 INTEL_RESTORE_ID = 5866;
	constexpr int32 INTEL_PERSIST_ID = 5867;
	constexpr int32 INTEL_DEFEND_ID = 5868;
	constexpr int32 INTEL_NEGATIVE_TURNS = -1;
	constexpr float INTEL_BUFF_PERCENT = 50.0f;
	constexpr float INTEL_DEBUFF_PERCENT = 30.0f;
	constexpr float INTEL_DOT_DAMAGE = 7.0f;
	constexpr float INTEL_RESTORE_AMOUNT = 10.0f;
	constexpr int32 INTEL_ONE_TURN = 1;
	constexpr int32 INTEL_TWO_TURNS = 2;
	constexpr int32 INTEL_HIT_AMOUNT = 12;
	constexpr int32 INTEL_PILLAR_COUNT = 3; // Mind, Body, Spirit
	constexpr int32 INTEL_YOU_SPEC_TABS = 3; // you, a party member, an enemy
	constexpr float INTEL_CUE_SECONDS = 0.5f; // any time: the HUD only reads who the prompt is for
	constexpr int32 HERO_CARD = 0;
	constexpr int32 FOE_CARD = 1;

	int32 CountChanged(const TArray<FBattleIntelRow> &Stats)
	{
		return Stats.FilterByPredicate([](const FBattleIntelRow &Row) { return !Row.Before.IsEmpty(); }).Num();
	}

	/** Weak to and Resists lines a hit has taught (not the "?" line, not "None"). */
	int32 CountLearned(const FBattleIntelPage &Page)
	{
		int32 Count = 0;
		for (const TArray<FBattleIntelRow> *Lines : {&Page.WeakTo, &Page.Resists})
		{
			Count += Lines->FilterByPredicate([](const FBattleIntelRow &Row)
			{ return Row.Tone == EBattleIntelTone::Better || Row.Tone == EBattleIntelTone::Worse; }).Num();
		}
		return Count;
	}

	UCharacterData *NewNamedData(const FString &Name, ECharacterClass Class, ESpellElement Element)
	{
		UPackage *Package = GetTransientPackage();
		UCharacterData *Data = NewObject<UCharacterData>(Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(), *Name));
		Data->CharacterClass = Class;
		Data->InnateElement = Element;
		return Data;
	}
}

BEGIN_DEFINE_SPEC(FBattleIntelSpec, "WoR.UI.Intel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;

	void Apply(AActor *Target, const FActiveSkillEffect &Effect) const
	{
		if (Effects)
		{
			Effects->ApplyEffect(Target, Effect, Target, TEXT("BattleIntelSpec"));
		}
	}

	UBattleHUDWidget *MakeHUD(const TArray<AActor *> &Party, const TArray<AActor *> &Enemies) const
	{
		UBattleHUDWidget *HUD = CreateWidget<UBattleHUDWidget>(TestWorld->GetWorld(), UBattleHUDWidget::StaticClass());
		if (HUD)
		{
			HUD->Initialise(Party, Enemies);
		}
		return HUD;
	}
END_DEFINE_SPEC(FBattleIntelSpec)

void FBattleIntelSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("effect manager"), Effects);
	});

	AfterEach([this]()
	{
		Effects = nullptr;
		TestWorld.Reset();
	});

	It("reads before -> now only for a stat an effect changed (D-261)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("hero"), Hero) || !Effects)
		{
			return;
		}
		TArray<FBattleIntelRow> Stats = BattleIntelView::BuildStats(Hero);
		TestEqual(TEXT("the sheet's eleven stats"), Stats.Num(), INTEL_STAT_COUNT);
		TestEqual(TEXT("no effects: no stat reads before -> now"), CountChanged(Stats), 0);

		Apply(Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel up"), INTEL_BUFF_ID, ESkillEffectType::SpellDamageBuff, INTEL_BUFF_PERCENT, INTEL_TWO_TURNS));
		Stats = BattleIntelView::BuildStats(Hero);
		const FBattleIntelRow *Better = Stats.FindByPredicate([](const FBattleIntelRow &Row) { return !Row.Before.IsEmpty(); });
		if (TestNotNull(TEXT("a buff: its stat reads before -> now"), Better))
		{
			TestTrue(TEXT("green (better), and the two values differ"),
					 Better->Tone == EBattleIntelTone::Better && !Better->Before.EqualTo(Better->Now));
		}

		Apply(Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel slow"), INTEL_DEBUFF_ID, ESkillEffectType::ActionSpeedDebuff, INTEL_DEBUFF_PERCENT, INTEL_ONE_TURN));
		Stats = BattleIntelView::BuildStats(Hero);
		TestTrue(TEXT("a debuff: its stat is red (worse)"), Stats.ContainsByPredicate([](const FBattleIntelRow &Row)
		{ return !Row.Before.IsEmpty() && Row.Tone == EBattleIntelTone::Worse; }));

		const auto FindResistance = [](const TArray<FBattleIntelRow> &Rows)
		{ return Rows.FindByPredicate([](const FBattleIntelRow &Row) { return Row.Label.ToString().Contains(TEXT("Resist")); }); };
		const FBattleIntelRow *Resistance = FindResistance(Stats);
		if (TestNotNull(TEXT("a Resistance row"), Resistance))
		{
			TestTrue(TEXT("Resistance unchanged so far"), Resistance->Before.IsEmpty());
		}
		// The sheet's Resistance is general status resistance: a fight moves it with ModifyStatusResist (ResistanceBuff is
		// typed damage resistance, not this stat).
		Apply(Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel resist"), INTEL_RESIST_ID, ESkillEffectType::ModifyStatusResist, INTEL_BUFF_PERCENT, INTEL_TWO_TURNS));
		Stats = BattleIntelView::BuildStats(Hero);
		Resistance = FindResistance(Stats);
		TestTrue(TEXT("Resistance has its own before (its effect bonus left out)"), Resistance && !Resistance->Before.IsEmpty() &&
																					   Resistance->Tone == EBattleIntelTone::Better);
	});

	It("gives every effect family an amount and its turns left (D-261)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("hero"), Hero))
		{
			return;
		}
		const FBattleIntelRow Up = BattleIntelView::DescribeEffect(
			Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel up"), INTEL_BUFF_ID, ESkillEffectType::SpellDamageBuff, INTEL_BUFF_PERCENT, INTEL_TWO_TURNS));
		TestEqual(TEXT("a stat buff: its percent"), Up.Now.ToString(), FString(TEXT("+50%")));
		TestEqual(TEXT("and its turns left"), Up.Turns.ToString(), FString(TEXT("2 turns")));
		TestTrue(TEXT("green, and named"), Up.Tone == EBattleIntelTone::Better && !Up.Label.IsEmpty());

		const FBattleIntelRow Down = BattleIntelView::DescribeEffect(
			Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel slow"), INTEL_DEBUFF_ID, ESkillEffectType::ActionSpeedDebuff, INTEL_DEBUFF_PERCENT, INTEL_ONE_TURN));
		TestEqual(TEXT("a stat debuff: minus its percent"), Down.Now.ToString(), FString(TEXT("-30%")));
		TestEqual(TEXT("one turn"), Down.Turns.ToString(), FString(TEXT("1 turn")));
		TestTrue(TEXT("red"), Down.Tone == EBattleIntelTone::Worse);

		const FBattleIntelRow Burn = BattleIntelView::DescribeEffect(
			Hero, FActiveSkillEffect::CreateDOT(TEXT("Intel burn"), INTEL_DOT_ID, INTEL_DOT_DAMAGE, INTEL_TWO_TURNS, ESpellElement::Fire));
		TestTrue(TEXT("damage over time: HP lost a turn"), Burn.Now.ToString().StartsWith(TEXT("-")) && Burn.Now.ToString().EndsWith(TEXT("HP a turn")));
		TestTrue(TEXT("red"), Burn.Tone == EBattleIntelTone::Worse);

		const FBattleIntelRow Stun = BattleIntelView::DescribeEffect(
			Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel stun"), INTEL_STUN_ID, ESkillEffectType::SkipTurn, 0.0f, INTEL_ONE_TURN));
		TestEqual(TEXT("an effect with no number: what it does"), Stun.Now.ToString(), FString(TEXT("can't act")));

		const FBattleIntelRow Regen = BattleIntelView::DescribeEffect(
			Hero, FActiveSkillEffect::CreateBuff(TEXT("Intel regen"), INTEL_RESTORE_ID, ESkillEffectType::HealthRestore, INTEL_RESTORE_AMOUNT, INTEL_TWO_TURNS));
		TestTrue(TEXT("healing over time: HP gained a turn"), Regen.Now.ToString().StartsWith(TEXT("+")) && Regen.Now.ToString().Contains(TEXT("HP a turn")));

		const FBattleIntelRow Kept = BattleIntelView::DescribeEffect(
			Hero, FActiveSkillEffect::CreatePersistent(TEXT("Intel kept"), INTEL_PERSIST_ID, ESkillEffectType::SpellDamageBuff, INTEL_BUFF_PERCENT));
		TestEqual(TEXT("a lasting effect"), Kept.Turns.ToString(), FString(TEXT("until removed")));
	});

	It("reads a held effect with no turns left as until next turn, as Defend's after its turn (B-586 QA)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("hero"), Hero))
		{
			return;
		}
		// Defend's buff: one holder turn (ActionExecutor::ExecuteDefend), counted down as the Defend turn ends.
		FActiveSkillEffect Defend = FActiveSkillEffect::CreateBuff(TEXT("Defending"), INTEL_DEFEND_ID, ESkillEffectType::DefenseBuff,
																	 INTEL_BUFF_PERCENT, INTEL_ONE_TURN);
		Defend.StartHolderTurnSpan(INTEL_ONE_TURN);
		TestEqual(TEXT("before its turn ends: 1 turn"), BattleIntelView::DescribeEffect(Hero, Defend).Turns.ToString(), FString(TEXT("1 turn")));
		Defend.HolderTurns.CountHolderTurn();
		Defend.RemainingTurns = Defend.HolderTurns.TurnsLeft;
		TestEqual(TEXT("after the Defend turn: 0 left, removed only as the next turn starts (QA's case)"), Defend.RemainingTurns, 0);
		TestEqual(TEXT("none left: until next turn, never 0 turns"), BattleIntelView::DescribeEffect(Hero, Defend).Turns.ToString(),
				  FString(TEXT("until next turn")));
		Defend.RemainingTurns = INTEL_NEGATIVE_TURNS;
		TestEqual(TEXT("a negative count reads the same, never a minus number"), BattleIntelView::DescribeEffect(Hero, Defend).Turns.ToString(),
				  FString(TEXT("until next turn")));
	});

	It("learns one line per hit type for that enemy type and keeps it for the run (Q-586)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(NewNamedData(TEXT("IntelSpecFoe"), ECharacterClass::Caster, ESpellElement::Fire));
		AActor *OtherFoe = TestWorld->SpawnCombatant(NewNamedData(TEXT("IntelSpecOtherFoe"), ECharacterClass::Caster, ESpellElement::Fire));
		UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("combatants"), Hero && Foe && OtherFoe ? Hero : nullptr) || !TestNotNull(TEXT("the run"), Run))
		{
			return;
		}
		UCombatPresentationComponent *FoeHits = FWoRTestWorld::AddComponent<UCombatPresentationComponent>(Foe);
		UCombatPresentationComponent *HeroHits = FWoRTestWorld::AddComponent<UCombatPresentationComponent>(Hero);
		const FName FoeType = BattleIntelView::GetEnemyTypeKey(Foe);
		if (!TestNotNull(TEXT("presentation"), FoeHits && HeroHits ? FoeHits : nullptr) || !TestFalse(TEXT("an enemy type key"), FoeType.IsNone()))
		{
			return;
		}
		TestNotEqual(TEXT("another enemy is another type"), BattleIntelView::GetEnemyTypeKey(OtherFoe), FoeType);

		// A damage type this enemy is weak to or resists, so learning it shows a line.
		EWoRDamageType Teach = EWoRDamageType::None;
		const UEnum *Types = StaticEnum<EWoRDamageType>();
		for (int32 Index = 0; Index < Types->NumEnums() - 1 && Teach == EWoRDamageType::None; ++Index)
		{
			const EWoRDamageType Type = static_cast<EWoRDamageType>(Types->GetValueByIndex(Index));
			TArray<FBattleIntelRow> Weak, Resists;
			BattleIntelView::BuildElementLines(Foe, [Type](EWoRDamageType Learned) { return Learned == Type; }, Weak, Resists);
			FBattleIntelPage Probe;
			Probe.WeakTo = Weak;
			Probe.Resists = Resists;
			Teach = Type != EWoRDamageType::None && CountLearned(Probe) == 1 ? Type : EWoRDamageType::None;
		}
		if (!TestNotEqual(TEXT("a Fire Caster is weak to or resists some element"), Teach, EWoRDamageType::None))
		{
			return;
		}

		UBattleHUDWidget *HUD = MakeHUD({Hero}, {Foe});
		if (!TestNotNull(TEXT("battle HUD"), HUD))
		{
			return;
		}
		HUD->OpenIntel(FOE_CARD);
		TestTrue(TEXT("the enemy's tab"), HUD->IsIntelOpen() && HUD->GetIntelPages().IsValidIndex(HUD->GetIntelTab()) &&
											  HUD->GetIntelPages()[HUD->GetIntelTab()].bEnemy);
		const auto Learned = [HUD]() { return CountLearned(HUD->GetIntelPages()[HUD->GetIntelTab()]); };
		TestEqual(TEXT("nothing learned yet: only ? lines"), Learned(), 0);
		TestTrue(TEXT("an enemy's HP number stays hidden (Q-391)"), HUD->GetIntelPages()[HUD->GetIntelTab()].HPLine.IsEmpty());

		FCombatHitShown Hit;
		Hit.Amount = INTEL_HIT_AMOUNT;
		Hit.DamageType = Teach;
		Hit.bHeal = true;
		FoeHits->OnHitDetailShown.Broadcast(Foe, Hit);
		TestEqual(TEXT("a heal teaches nothing"), Learned(), 0);
		Hit.bHeal = false;
		HeroHits->OnHitDetailShown.Broadcast(Hero, Hit);
		TestFalse(TEXT("a hit on the party teaches nothing"), Run->IsIntelLearned(BattleIntelView::GetEnemyTypeKey(Hero), Teach));
		FoeHits->OnHitDetailShown.Broadcast(Foe, Hit);
		TestTrue(TEXT("a hit teaches its type for that enemy type"), Run->IsIntelLearned(FoeType, Teach));
		TestEqual(TEXT("one line learned"), Learned(), 1);
		FoeHits->OnHitDetailShown.Broadcast(Foe, Hit);
		TestEqual(TEXT("the same type again: still one line"), Learned(), 1);
		TestFalse(TEXT("another enemy type learns nothing from it"), Run->IsIntelLearned(BattleIntelView::GetEnemyTypeKey(OtherFoe), Teach));
		HUD->Teardown();

		UBattleHUDWidget *NextFight = MakeHUD({Hero}, {Foe});
		if (TestNotNull(TEXT("the next fight's HUD"), NextFight))
		{
			NextFight->OpenIntel(FOE_CARD);
			TestEqual(TEXT("kept for the run: the next fight still shows it"), CountLearned(NextFight->GetIntelPages()[NextFight->GetIntelTab()]), 1);
			NextFight->Teardown();
		}
	});

	It("names your own character's tab You and keeps its heading (D-261)", [this]()
	{
		UCharacterData *PlayerData = NewNamedData(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_IntelSpec"), ECharacterClass::Caster, ESpellElement::Fire);
		AActor *Player = TestWorld->SpawnCombatant(PlayerData);
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Player && Hero && Foe ? MakeHUD({Player, Hero}, {Foe}) : nullptr;
		if (!TestNotNull(TEXT("battle HUD"), HUD))
		{
			return;
		}
		HUD->OpenIntel(0);
		const TArray<FBattleIntelPage> &Pages = HUD->GetIntelPages();
		if (!TestEqual(TEXT("a tab each: party first, then enemies"), Pages.Num(), INTEL_YOU_SPEC_TABS))
		{
			return;
		}
		TestEqual(TEXT("your tab reads You"), Pages[0].Tab.ToString(), FString(TEXT("You")));
		TestEqual(TEXT("its heading keeps the name"), Pages[0].Heading.ToString(), HUD->GetCards()[0].Name.ToString());
		TestTrue(TEXT("Spirit / Body under it"), Pages[0].Subline.ToString().StartsWith(TEXT("Spirit of")));
		TestFalse(TEXT("and the full actions line (D-259)"), Pages[0].ActionsLine.IsEmpty());
		TestEqual(TEXT("Pillars"), Pages[0].Pillars.Num(), INTEL_PILLAR_COUNT);
		TestEqual(TEXT("another party member's tab keeps its name"), Pages[1].Tab.ToString(), HUD->GetCards()[1].Name.ToString());
		TestTrue(TEXT("the enemy last"), Pages[2].bEnemy && !Pages[0].bEnemy && !Pages[1].bEnemy);
		HUD->Teardown();
	});

	It("opens on a card, steps its tabs, closes, and never shows with the status panel (D-261)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Hero && Foe ? MakeHUD({Hero}, {Foe}) : nullptr;
		if (!TestNotNull(TEXT("battle HUD"), HUD))
		{
			return;
		}
		TestFalse(TEXT("closed to start"), HUD->IsIntelOpen());
		HUD->StepIntelTab(1);
		TestFalse(TEXT("Q / E do nothing while closed"), HUD->IsIntelOpen());
		HUD->InspectCard(FOE_CARD);
		TestTrue(TEXT("an enemy card click opens it on that enemy's tab"), HUD->IsIntelOpen() && HUD->GetIntelTab() == FOE_CARD);
		HUD->InspectCard(FOE_CARD);
		TestFalse(TEXT("the same card again closes it"), HUD->IsIntelOpen());
		HUD->ToggleIntel();
		TestTrue(TEXT("V opens it on the featured party card"), HUD->IsIntelOpen() && HUD->GetIntelTab() == HERO_CARD);
		HUD->StepIntelTab(-1);
		TestEqual(TEXT("Q from the first tab wraps to the last"), HUD->GetIntelTab(), FOE_CARD);
		HUD->StepIntelTab(1);
		TestEqual(TEXT("E wraps back"), HUD->GetIntelTab(), HERO_CARD);
		TestTrue(TEXT("wor.UI.Print shows the open page"), HUD->GetBattleHUDString().Contains(TEXT("intel tab")));
		HUD->SetStatusPanelOpen(true);
		TestTrue(TEXT("the status panel closes Intel"), HUD->IsStatusPanelOpen() && !HUD->IsIntelOpen());
		HUD->OpenIntel(HERO_CARD);
		TestTrue(TEXT("and Intel closes the status panel"), HUD->IsIntelOpen() && !HUD->IsStatusPanelOpen());
		HUD->ToggleIntel();
		TestFalse(TEXT("V closes it"), HUD->IsIntelOpen());
		HUD->Teardown();
	});

	It("hides the command menu, the corner hints and the enemy cards while open; the turn order stays (B-587, D-262)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Hero && Foe ? MakeHUD({Hero}, {Foe}) : nullptr;
		if (HUD)
		{
			HUD->SetCommandMenu(CreateWidget<UBattleCommandMenuWidget>(TestWorld->GetWorld(), UBattleCommandMenuWidget::StaticClass()));
			HUD->TakeWidget(); // builds the tree
		}
		const UWidget *TurnOrder = HUD ? HUD->GetWidgetFromName(TEXT("TurnOrder")) : nullptr;
		TArray<const UWidget *> Hidden;
		for (const TCHAR *Name : {TEXT("TargetMarkers"), TEXT("CommandMenuHolder"), TEXT("CornerHints")})
		{
			const UWidget *Piece = HUD ? HUD->GetWidgetFromName(Name) : nullptr;
			TestNotNull(*FString::Printf(TEXT("%s built"), Name), Piece);
			Hidden.Add(Piece);
		}
		if (!TestNotNull(TEXT("the turn order"), TurnOrder) || Hidden.Contains(nullptr))
		{
			return;
		}
		HUD->OpenIntel(FOE_CARD);
		for (const UWidget *Piece : Hidden)
		{
			TestEqual(*FString::Printf(TEXT("open: %s hidden"), *Piece->GetName()), Piece->GetVisibility(), ESlateVisibility::Collapsed);
		}
		TestEqual(TEXT("open: the turn order stays"), TurnOrder->GetVisibility(), ESlateVisibility::HitTestInvisible);
		HUD->CloseIntel();
		for (const UWidget *Piece : Hidden)
		{
			TestEqual(*FString::Printf(TEXT("closed: %s back"), *Piece->GetName()), Piece->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		}
		HUD->Teardown();
	});

	It("closes by itself on a party member's guard prompt and when the fight ends (B-587 QA)", [this]()
	{
		ACombatOrchestrator *Fight = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Fight && Hero && Foe ? MakeHUD({Hero}, {Foe}) : nullptr;
		UDefenseSystem *Defense = WoRFight::Get<UDefenseSystem>(TestWorld->GetWorld());
		if (!TestNotNull(TEXT("battle HUD"), HUD) || !TestNotNull(TEXT("defense system"), Defense))
		{
			return;
		}
		HUD->OpenIntel(HERO_CARD);
		Defense->OnDefenseCueTriggered.Broadcast(Foe, INTEL_CUE_SECONDS);
		TestTrue(TEXT("an enemy's cue leaves it open"), HUD->IsIntelOpen());
		Defense->OnDefenseCueTriggered.Broadcast(Hero, INTEL_CUE_SECONDS);
		TestFalse(TEXT("the party's guard cue closes it"), HUD->IsIntelOpen());
		HUD->OpenIntel(HERO_CARD);
		Defense->OnDefenseWindowOpened.Broadcast(Hero, INTEL_CUE_SECONDS, INTEL_CUE_SECONDS);
		TestFalse(TEXT("the guard window closes it"), HUD->IsIntelOpen());
		for (const ECombatState End : {ECombatState::Victory, ECombatState::Defeat, ECombatState::Fled})
		{
			HUD->OpenIntel(HERO_CARD);
			Fight->OnCombatStateChanged.Broadcast(End);
			TestFalse(*FString::Printf(TEXT("%s closes it"), *UEnum::GetValueAsString(End)), HUD->IsIntelOpen());
		}
		HUD->OpenIntel(HERO_CARD);
		Fight->OnCombatStateChanged.Broadcast(ECombatState::InProgress);
		TestTrue(TEXT("a fight still going leaves it open"), HUD->IsIntelOpen());
		HUD->Teardown();
	});

	It("opens on the party from the Status button, and a second click closes it (B-587 QA)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Hero && Foe ? MakeHUD({Hero}, {Foe}) : nullptr;
		if (!TestNotNull(TEXT("battle HUD"), HUD))
		{
			return;
		}
		HUD->HandleStatusButtonClicked();
		TestTrue(TEXT("Intel on the party, not the old list"), HUD->IsIntelOpen() && HUD->GetIntelTab() == HERO_CARD && !HUD->IsStatusPanelOpen());
		HUD->HandleStatusButtonClicked();
		TestFalse(TEXT("closed"), HUD->IsIntelOpen() || HUD->IsStatusPanelOpen());
		HUD->InspectCard(FOE_CARD);
		HUD->HandleStatusButtonClicked();
		TestFalse(TEXT("from an enemy's page it closes too"), HUD->IsIntelOpen());
		HUD->Teardown();
	});

	It("shows one title with the side and arrows, not a row of tabs (B-587, D-262)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Hero && Foe ? MakeHUD({Hero}, {Foe}) : nullptr;
		if (HUD)
		{
			HUD->TakeWidget();
		}
		const UBattleIntelWidget *Panel = HUD ? Cast<UBattleIntelWidget>(HUD->GetWidgetFromName(TEXT("Intel"))) : nullptr;
		if (!TestNotNull(TEXT("the Intel panel"), Panel))
		{
			return;
		}
		HUD->OpenIntel(FOE_CARD);
		TestEqual(TEXT("the enemy's name"), Panel->GetTitleText().ToString(), HUD->GetCards()[FOE_CARD].Name.ToString());
		TestEqual(TEXT("marked Enemy"), Panel->GetSideText().ToString(), FString(TEXT("Enemy")));
		TestTrue(TEXT("arrows: there is more"), Panel->AreArrowsShown());
		HUD->StepIntelTab(1);
		TestEqual(TEXT("E: the party member's name"), Panel->GetTitleText().ToString(), HUD->GetCards()[HERO_CARD].Name.ToString());
		TestEqual(TEXT("marked Party"), Panel->GetSideText().ToString(), FString(TEXT("Party")));
		HUD->Teardown();
	});

	It("cycles V / R3: party, then the first enemy, then closed; Open and close toggles (B-587, D-262)", [this]()
	{
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UBattleHUDWidget *HUD = Hero && Foe ? MakeHUD({Hero}, {Foe}) : nullptr;
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestNotNull(TEXT("battle HUD"), HUD) || !TestNotNull(TEXT("save"), Save))
		{
			return;
		}
		Save->SetIntelKeyCycle(true);
		HUD->PressIntelKey();
		TestTrue(TEXT("first tap: the party"), HUD->IsIntelOpen() && HUD->GetIntelTab() == HERO_CARD);
		HUD->PressIntelKey();
		TestTrue(TEXT("second tap: the first enemy"), HUD->IsIntelOpen() && HUD->GetIntelTab() == FOE_CARD);
		HUD->PressIntelKey();
		TestFalse(TEXT("third tap: closed"), HUD->IsIntelOpen());
		Save->SetIntelKeyCycle(false);
		HUD->PressIntelKey();
		TestTrue(TEXT("open and close: opens on the party"), HUD->IsIntelOpen() && HUD->GetIntelTab() == HERO_CARD);
		HUD->PressIntelKey();
		TestFalse(TEXT("and the next press closes"), HUD->IsIntelOpen());
		Save->SetIntelKeyCycle(true);
		HUD->Teardown();
	});
}

#endif
