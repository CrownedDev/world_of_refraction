// BattleHUDWidget.cpp

#include "UI/Combat/Battle/BattleHUDWidget.h"

#include "Save/SaveSubsystem.h"
#include "Character/PlayerBuild.h"
#include "WoRGameNames.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "EngineUtils.h"
#include "Combat/CombatPresentationComponent.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/StatusBarStateComponent.h"
#include "Combat/Actions/ActionPipsText.h"
#include "Combat/TurnManager.h"
#include "Combat/WoRFightSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Player/TurnStateComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "UI/Combat/Battle/BattleCommandMenuWidget.h"
#include "UI/Combat/Battle/BattleHUDButton.h"
#include "UI/Combat/Battle/BattleIntelWidget.h"
#include "Trial/TrialRunSubsystem.h"
#include "Widgets/Layout/Anchors.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "BattleHUD"

namespace BattleHUDStyle
{
	// TODO: style pass (placeholder until the CommonUI screens, D-024). Slate units, DPI-scaled.
	using namespace BattleCardStyle; // B-503j: the card's look is shared with the hub HUD
	constexpr float ScreenMargin = 24.0f;
	constexpr float StatusPanelWidth = 640.0f;
	constexpr int32 TurnFontSize = 14;

	const FLinearColor PanelColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FLinearColor EnemyColor(0.85f, 0.55f, 0.5f);   // the other side, in the turn strip
	const FLinearColor BonusColor(0.45f, 0.9f, 0.55f);   // a bonus turn (Emerald), as the old strip's green

	// B-436a layout (docs/Design/BattleHUDRefinePlan.md piece 1, D-254: refine_final/1_main_menu.png).
	const FVector2D RightColumnAnchor(1.0f, 0.0f);  // top-right: the turn order, the enemy cards under it
	const FVector2D PartyAnchor(0.0f, 1.0f);        // bottom-left: the featured card, the compact cards in a row to its right
	const FVector2D StatusButtonAnchor(0.5f, 0.0f); // top centre
	const FVector2D StatusPanelAnchor(0.5f, 0.5f);  // centre
	constexpr float CardGap = 8.0f;
	constexpr float RightColumnGap = 16.0f; // between the turn order and the enemy cards
	// shortcut: the command menu sits a fixed height above the bottom margin, not the featured card's real height,
	// upgrade to the card's measured height if its lines outgrow this. Today's full card with two identity lines is
	// about 225 by its fonts and bars; the rest is room for one wrapped line. TODO: tune in PIE.
	// OBSOLETE (B-436g): the menu stacks on the card in the party column (MenuCardGap); used only by GetCommandMenuOffset.
	constexpr float FeaturedCardReserve = 250.0f;
	/** B-436g (D-260, B-574 item 22): the menu sits this far above the featured card, whatever the card's height. */
	constexpr float MenuCardGap = 10.0f;
	const FVector2D MenuHintsAnchor(1.0f, 1.0f); // B-436g (D-258): the corner hints, bottom right

	// B-391b: a damage number's text is centred on its point and sits above it.
	const FVector2D DamageNumberAlignment(0.5f, 1.0f);
}

namespace BattleHUDBuild
{
	using BattleHUDCard::AddText;

	UCanvasPanelSlot *Place(UCanvasPanel *Canvas, UWidget *Widget, const FVector2D &Anchor, const FVector2D &Offset)
	{
		UCanvasPanelSlot *CanvasSlot = Canvas->AddChildToCanvas(Widget);
		if (CanvasSlot)
		{
			CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			CanvasSlot->SetAlignment(Anchor);
			CanvasSlot->SetPosition(Offset);
			CanvasSlot->SetAutoSize(true);
		}
		return CanvasSlot;
	}

	/** Offset from an anchored corner inward by the screen margin. */
	FVector2D Inset(const FVector2D &Anchor)
	{
		const float X = Anchor.X < 0.5f ? BattleHUDStyle::ScreenMargin : (Anchor.X > 0.5f ? -BattleHUDStyle::ScreenMargin : 0.0f);
		const float Y = Anchor.Y < 0.5f ? BattleHUDStyle::ScreenMargin : (Anchor.Y > 0.5f ? -BattleHUDStyle::ScreenMargin : 0.0f);
		return FVector2D(X, Y);
	}

	void SetTextSafe(UTextBlock *Text, const FText &Value)
	{
		if (Text)
		{
			Text->SetText(Value);
		}
	}
}

// ========================================
// LIFECYCLE
// ========================================

void UBattleHUDWidget::Initialise(const TArray<AActor *> &Party, const TArray<AActor *> &Enemies)
{
	if (bBound)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleHUD] Initialise called twice; ignoring"));
		return;
	}
	ClearFlags(RF_Transactional); // TransBuffer rule, as the old panels
	EnsureDamageNumberPool();
	BoundBuildup = WoRFight::Get<UStatusBuildupManager>(this); // AddCard seeds each bar from it

	for (AActor *Actor : Party)
	{
		AddCard(Actor, /*bParty=*/true);
	}
	for (AActor *Actor : Enemies)
	{
		AddCard(Actor, /*bParty=*/false);
	}

	if (USkillEffectManager *Effects = WoRFight::Get<USkillEffectManager>(this))
	{
		BoundEffects = Effects;
		Effects->OnEffectApplied.AddDynamic(this, &UBattleHUDWidget::HandleEffectChanged);
		Effects->OnEffectRemoved.AddDynamic(this, &UBattleHUDWidget::HandleEffectChanged);
		Effects->OnEffectStacksChanged.AddDynamic(this, &UBattleHUDWidget::HandleEffectCountChanged);
		Effects->OnEffectDurationChanged.AddDynamic(this, &UBattleHUDWidget::HandleEffectCountChanged);
	}
	// B-391c: the status bars, as the old panel (filtered per card in the handler). On a guest the replicated bars
	// drive them instead (AddCard); this manager's events don't fire there.
	if (UStatusBuildupManager *Buildup = BoundBuildup.Get())
	{
		Buildup->OnStatusBuildupChanged.AddDynamic(this, &UBattleHUDWidget::HandleBuildupChanged);
	}

	// Turn changes: the replicated turn state when there is one (B-228, clients), else the server's TurnManager, as
	// the old strip. The turn number is seeded from the same source; the handlers keep it after.
	if (UTurnStateComponent *TurnState = UTurnStateComponent::Find(this))
	{
		BoundTurnState = TurnState;
		TurnState->OnTurnStateChanged.AddDynamic(this, &UBattleHUDWidget::HandleTurnStateChanged);
	}
	else if (UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(this))
	{
		BoundTurnManager = TurnManager;
		TurnManager->OnTurnStarted.AddDynamic(this, &UBattleHUDWidget::HandleTurnStarted);
	}
	CurrentTurnNumber = BattleHUDView::ReadTurnNumber(this);

	// B-587 QA: Intel closes by itself when the fight moves on without the player (a guard prompt, the fight's end).
	if (UDefenseSystem *Defense = WoRFight::Get<UDefenseSystem>(this))
	{
		BoundDefense = Defense;
		Defense->OnDefenseCueTriggered.AddDynamic(this, &UBattleHUDWidget::HandleDefenseCue);
		Defense->OnDefenseWindowOpened.AddDynamic(this, &UBattleHUDWidget::HandleDefenseWindowOpened);
	}
	for (TActorIterator<ACombatOrchestrator> It(GetWorld()); It; ++It)
	{
		BoundOrchestrator = *It;
		It->OnCombatStateChanged.AddDynamic(this, &UBattleHUDWidget::HandleCombatStateChanged);
		break; // one fight at a time, as FindBattleHUD
	}

	bBound = true;
	RefreshPools();
	RefreshStatuses();
	RefreshTurnStrip();
	RefreshGear();
	UE_LOG(LogWoRUI, Log, TEXT("[BattleHUD] Initialised: %d party, %d enemy card(s)"), Party.Num(), Enemies.Num());
}

void UBattleHUDWidget::AddCard(AActor *Actor, bool bParty)
{
	if (!Actor)
	{
		return;
	}
	FBattleHUDCard &Card = Cards.AddDefaulted_GetRef();
	Card.Actor = Actor;
	Card.bParty = bParty;
	Card.Name = BattleHUDView::GetCombatantName(Actor);

	UCharacterDataComponent *CharData = Actor->FindComponentByClass<UCharacterDataComponent>();
	Card.CharData = CharData;
	// D-254: no Spirit of ... / Body of ... on a fight's card (Card.DetailLine stays empty); InspectCard shows them.
	if (CharData)
	{
		CharData->OnHPChanged.AddDynamic(this, &UBattleHUDWidget::HandlePoolChanged);
		CharData->OnEPChanged.AddDynamic(this, &UBattleHUDWidget::HandlePoolChanged);
		CharData->OnDied.AddDynamic(this, &UBattleHUDWidget::HandleLifeChanged);
		CharData->OnResurrected.AddDynamic(this, &UBattleHUDWidget::HandleLifeChanged);
		// A Resonator with nothing to spend EP on has no EP line, as the old panel (init-only there too). B-391b: a Broken
		// Darkness character always shows its absorption pool.
		Card.bShowEP = bParty && (CharData->HasUsableEPTarget() || CharData->IsBrokenDarkness());
		if (bParty)
		{
			CharData->OnActionsChanged.AddDynamic(this, &UBattleHUDWidget::HandlePoolChanged); // B-170e; same (int32, int32) shape
		}
	}
	else
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleHUD] %s has no CharacterDataComponent: its card shows a name only"), *Actor->GetName());
		Card.bShowEP = false;
	}
	// B-318: on a guest the statuses follow the server's snapshot (the effect manager's events don't fire there).
	if (UWoREffectMirrorComponent *Mirror = Actor->FindComponentByClass<UWoREffectMirrorComponent>())
	{
		BoundMirrors.Add(Mirror);
		Mirror->OnHeldSnapshotChanged.AddDynamic(this, &UBattleHUDWidget::HandleHeldSnapshotChanged);
	}
	// B-391b: the server's hits and heals on this character, on every machine.
	if (UCombatPresentationComponent *Presentation = UCombatPresentationComponent::Find(Actor))
	{
		BoundPresentations.Add(Presentation);
		Presentation->OnHitDetailShown.AddDynamic(this, &UBattleHUDWidget::HandleHitShown);
	}

	// B-391c: the status bar. The server's manager is the source; a guest reads the replicated bar (B-230), as the
	// old panel. Seeded now: the manager reports only on change.
	const UWorld *World = GetWorld();
	const bool bGuest = World && World->GetNetMode() == NM_Client;
	UStatusBarStateComponent *BarState = Actor->FindComponentByClass<UStatusBarStateComponent>();
	const UStatusBuildupManager *Buildup = BoundBuildup.Get();
	if (!bGuest && Buildup)
	{
		SetCardBuildup(Card, Buildup->GetStatusBarBuildup(Actor), CombatConstants::STATUS_EFFECT_THRESHOLD,
					   Buildup->GetPendingElement(Actor), /*Source=*/nullptr);
	}
	else if (BarState && BarState->GetBar().Max > 0.0f)
	{
		const FReplicatedStatusBar &Bar = BarState->GetBar();
		SetCardBuildup(Card, Bar.Current, Bar.Max, Bar.PendingElement, Bar.Source);
	}
	else
	{
		SetCardBuildup(Card, 0.0f, CombatConstants::STATUS_EFFECT_THRESHOLD, ESpellElement::None, /*Source=*/nullptr);
	}
	if (bGuest && BarState)
	{
		BoundBarStates.Add(BarState);
		BarState->OnBarChanged.AddDynamic(this, &UBattleHUDWidget::HandleBuildupChanged);
	}

	// B-391c: a party card names its gear and follows loadout and weapon changes, as the old panel.
	if (ULoadoutComponent *Loadout = bParty ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr)
	{
		Card.Loadout = Loadout;
		Loadout->OnLoadoutChanged.AddDynamic(this, &UBattleHUDWidget::HandleLoadoutChanged);
		Loadout->OnActiveWeaponChanged.AddDynamic(this, &UBattleHUDWidget::HandleActiveWeaponChanged);
	}
}

void UBattleHUDWidget::Teardown()
{
	if (!bBound)
	{
		return;
	}
	IntelCameraSubject.Reset(); // B-587: the next fight's director starts with no subject
	for (const FBattleHUDCard &Card : Cards)
	{
		if (UCharacterDataComponent *CharData = Card.CharData.Get())
		{
			CharData->OnHPChanged.RemoveDynamic(this, &UBattleHUDWidget::HandlePoolChanged);
			CharData->OnEPChanged.RemoveDynamic(this, &UBattleHUDWidget::HandlePoolChanged);
			CharData->OnActionsChanged.RemoveDynamic(this, &UBattleHUDWidget::HandlePoolChanged); // B-170e; a no-op when unbound
			CharData->OnDied.RemoveDynamic(this, &UBattleHUDWidget::HandleLifeChanged);
			CharData->OnResurrected.RemoveDynamic(this, &UBattleHUDWidget::HandleLifeChanged);
		}
		if (ULoadoutComponent *Loadout = Card.Loadout.Get())
		{
			Loadout->OnLoadoutChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleLoadoutChanged);
			Loadout->OnActiveWeaponChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleActiveWeaponChanged);
		}
	}
	for (const TWeakObjectPtr<UStatusBarStateComponent> &BarStateRef : BoundBarStates)
	{
		if (UStatusBarStateComponent *BarState = BarStateRef.Get())
		{
			BarState->OnBarChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleBuildupChanged);
		}
	}
	BoundBarStates.Reset();
	if (UStatusBuildupManager *Buildup = BoundBuildup.Get())
	{
		Buildup->OnStatusBuildupChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleBuildupChanged);
	}
	BoundBuildup.Reset();
	for (const TWeakObjectPtr<UWoREffectMirrorComponent> &MirrorRef : BoundMirrors)
	{
		if (UWoREffectMirrorComponent *Mirror = MirrorRef.Get())
		{
			Mirror->OnHeldSnapshotChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleHeldSnapshotChanged);
		}
	}
	BoundMirrors.Reset();
	for (const TWeakObjectPtr<UCombatPresentationComponent> &PresentationRef : BoundPresentations)
	{
		if (UCombatPresentationComponent *Presentation = PresentationRef.Get())
		{
			Presentation->OnHitDetailShown.RemoveDynamic(this, &UBattleHUDWidget::HandleHitShown);
		}
	}
	BoundPresentations.Reset();
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageNumberTimer);
	}
	for (FBattleDamageNumber &Number : DamageNumbers)
	{
		HideDamageNumber(Number);
	}
	if (USkillEffectManager *Effects = BoundEffects.Get())
	{
		Effects->OnEffectApplied.RemoveDynamic(this, &UBattleHUDWidget::HandleEffectChanged);
		Effects->OnEffectRemoved.RemoveDynamic(this, &UBattleHUDWidget::HandleEffectChanged);
		Effects->OnEffectStacksChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleEffectCountChanged);
		Effects->OnEffectDurationChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleEffectCountChanged);
	}
	BoundEffects.Reset();
	if (UTurnStateComponent *TurnState = BoundTurnState.Get())
	{
		TurnState->OnTurnStateChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleTurnStateChanged);
	}
	BoundTurnState.Reset();
	if (UTurnManager *TurnManager = BoundTurnManager.Get())
	{
		TurnManager->OnTurnStarted.RemoveDynamic(this, &UBattleHUDWidget::HandleTurnStarted);
	}
	BoundTurnManager.Reset();
	if (UDefenseSystem *Defense = BoundDefense.Get())
	{
		Defense->OnDefenseCueTriggered.RemoveDynamic(this, &UBattleHUDWidget::HandleDefenseCue);
		Defense->OnDefenseWindowOpened.RemoveDynamic(this, &UBattleHUDWidget::HandleDefenseWindowOpened);
	}
	BoundDefense.Reset();
	if (ACombatOrchestrator *Orchestrator = BoundOrchestrator.Get())
	{
		Orchestrator->OnCombatStateChanged.RemoveDynamic(this, &UBattleHUDWidget::HandleCombatStateChanged);
	}
	BoundOrchestrator.Reset();
	bBound = false;
}

void UBattleHUDWidget::CloseIntelForFight(const TCHAR *Why)
{
	if (!bIntelOpen)
	{
		return;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[BattleHUD] Intel closes: %s"), Why);
	CloseIntel(); // the camera and the hidden pieces come back with it
}

void UBattleHUDWidget::HandleDefenseCue(AActor *Defender, float TimeUntilWindow)
{
	if (Cards.ContainsByPredicate([Defender](const FBattleHUDCard &Card) { return Card.bParty && Card.Actor.Get() == Defender; }))
	{
		CloseIntelForFight(TEXT("a guard prompt"));
	}
}

void UBattleHUDWidget::HandleDefenseWindowOpened(AActor *Defender, float AttackSize, float WindowDuration)
{
	HandleDefenseCue(Defender, 0.0f);
}

void UBattleHUDWidget::HandleCombatStateChanged(ECombatState NewState)
{
	if (NewState != ECombatState::InProgress && NewState != ECombatState::Initializing)
	{
		CloseIntelForFight(TEXT("the fight ended"));
	}
}

void UBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible); // clicks pass through except on the Statuses button
	PushToWidgets();
}

void UBattleHUDWidget::NativeDestruct()
{
	Teardown();
	Super::NativeDestruct();
}

// ========================================
// LAYOUT (built once)
// ========================================

TSharedRef<SWidget> UBattleHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel *Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Canvas;

		// B-436a: the right-hand column, top: the turn order, the enemy cards under it. B-586: only the enemy cards take a
		// click (Intel).
		UVerticalBox *RightColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightColumn"));
		RightColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UVerticalBox *TurnColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TurnOrder"));
		TurnColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
		BattleHUDBuild::AddText(WidgetTree, TurnColumn, BattleHUDStyle::NameFontSize)->SetText(LOCTEXT("TurnOrderTitle", "Turn order"));
		for (int32 Index = 0; Index <= BattleHUDView::UPCOMING_TURNS_SHOWN; ++Index)
		{
			TurnTexts.Add(BattleHUDBuild::AddText(WidgetTree, TurnColumn, BattleHUDStyle::TurnFontSize));
		}
		UVerticalBox *Enemies = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Enemies"));
		Enemies->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		EnemyColumn = Enemies;
		if (UVerticalBoxSlot *TurnSlot = RightColumn->AddChildToVerticalBox(TurnColumn))
		{
			TurnSlot->SetHorizontalAlignment(HAlign_Right);
			TurnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BattleHUDStyle::RightColumnGap));
		}
		if (UVerticalBoxSlot *EnemySlot = RightColumn->AddChildToVerticalBox(Enemies))
		{
			EnemySlot->SetHorizontalAlignment(HAlign_Right);
		}
		BattleHUDBuild::Place(Canvas, RightColumn, BattleHUDStyle::RightColumnAnchor, BattleHUDBuild::Inset(BattleHUDStyle::RightColumnAnchor));

		// B-436g (D-260): one bottom-left column, the command menu just above the party cards, so the menu follows the
		// featured card's real height with no measuring. The party cards share one overlay, bottom-aligned
		// (LayoutPartyCards sets each one's left padding).
		UVerticalBox *PartyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PartyColumn"));
		PartyColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (CommandMenu)
		{
			// B-587: in a holder the HUD hides under Intel; the menu keeps its own visibility.
			UOverlay *Holder = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CommandMenuHolder"));
			Holder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Holder->AddChild(CommandMenu);
			MenuHolder = Holder;
			if (UVerticalBoxSlot *MenuSlot = PartyColumn->AddChildToVerticalBox(Holder))
			{
				MenuSlot->SetHorizontalAlignment(HAlign_Left);
				MenuSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BattleHUDStyle::MenuCardGap));
			}
		}
		UOverlay *PartyRow = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PartyCards"));
		PartyRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UVerticalBoxSlot *RowSlot = PartyColumn->AddChildToVerticalBox(PartyRow))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Left);
		}
		BattleHUDBuild::Place(Canvas, PartyColumn, BattleHUDStyle::PartyAnchor, BattleHUDBuild::Inset(BattleHUDStyle::PartyAnchor));
		if (UWidget *Hints = CommandMenu ? CommandMenu->GetHintsWidget() : nullptr)
		{
			// B-587 QA: in a holder the HUD can hide under Intel; the hints widget keeps its own visibility.
			UOverlay *Holder = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CornerHints"));
			Holder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Holder->AddChild(Hints);
			BattleHUDBuild::Place(Canvas, Holder, BattleHUDStyle::MenuHintsAnchor, BattleHUDBuild::Inset(BattleHUDStyle::MenuHintsAnchor));
			CornerHints = Holder;
		}

		// One card per combatant, made here once. A party card sits in the overlay (B-391c: it takes a hover, its
		// tooltip), so LayoutPartyCards can move it to the corner or along the row without re-parenting anything.
		for (FBattleHUDCard &Card : Cards)
		{
			UBorder *Frame = BattleHUDCard::Build(WidgetTree, Card, EBattleCardLevel::Battle);
			Frame->SetVisibility(ESlateVisibility::Visible); // B-586 (D-261): every card takes a click (Intel)
			Frame->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UBattleHUDWidget, HandleCardMouseDown));
			if (Card.bParty)
			{
				if (UOverlaySlot *CardSlot = PartyRow->AddChildToOverlay(Frame))
				{
					CardSlot->SetHorizontalAlignment(HAlign_Left);
					CardSlot->SetVerticalAlignment(VAlign_Bottom);
				}
			}
			else if (UVerticalBoxSlot *ColumnSlot = Enemies->AddChildToVerticalBox(Frame))
			{
				ColumnSlot->SetPadding(FMargin(BattleHUDStyle::RowPadding));
			}
		}

		// The one button that shows every status (FeelGuide 5).
		UButton *StatusButton = WidgetTree->ConstructWidget<UBattleHUDButton>(UBattleHUDButton::StaticClass(), TEXT("StatusesButton")); // not focusable: keeps A / Enter for the command menu
		UTextBlock *StatusButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		StatusButtonLabel->SetText(LOCTEXT("StatusesButton", "Status"));
		StatusButton->AddChild(StatusButtonLabel);
		StatusButton->OnClicked.AddDynamic(this, &UBattleHUDWidget::HandleStatusButtonClicked);
		BattleHUDBuild::Place(Canvas, StatusButton, BattleHUDStyle::StatusButtonAnchor, BattleHUDBuild::Inset(BattleHUDStyle::StatusButtonAnchor));

		StatusPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusPanel"));
		StatusPanel->SetBrushColor(BattleHUDStyle::PanelColor);
		StatusPanel->SetPadding(FMargin(BattleHUDStyle::CardPadding));
		USizeBox *PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PanelSize->SetWidthOverride(BattleHUDStyle::StatusPanelWidth);
		UVerticalBox *PanelRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PanelSize->AddChild(PanelRows);
		StatusPanel->SetContent(PanelSize);
		StatusPanelBody = BattleHUDBuild::AddText(WidgetTree, PanelRows, BattleHUDStyle::LineFontSize);
		BattleHUDBuild::Place(Canvas, StatusPanel, BattleHUDStyle::StatusPanelAnchor, FVector2D::ZeroVector);

		// B-586 (D-261): the Intel panel; the status panel and it never show together. B-587 (Crown, Expedition 33): on
		// the right under the turn order, in the enemy cards' place (they hide while it is open), so nothing is covered.
		IntelPanel = WidgetTree->ConstructWidget<UBattleIntelWidget>(UBattleIntelWidget::StaticClass(), TEXT("Intel"));
		IntelPanel->SetVisibility(ESlateVisibility::Collapsed); // closed until the first push says otherwise
		IntelPanel->OnStepClicked.BindUObject(this, &UBattleHUDWidget::HandleIntelStepClicked);
		if (UVerticalBoxSlot *IntelSlot = RightColumn->AddChildToVerticalBox(IntelPanel))
		{
			IntelSlot->SetHorizontalAlignment(HAlign_Right);
		}

		BuildDamageNumberWidgets(Canvas); // last, so the numbers draw over the cards
	}
	return Super::RebuildWidget();
}

// ========================================
// STATE (plain data; runs unbuilt)
// ========================================

void UBattleHUDWidget::RefreshPools()
{
	for (FBattleHUDCard &Card : Cards)
	{
		const UCharacterDataComponent *CharData = Card.CharData.Get();
		if (!CharData)
		{
			continue;
		}
		Card.bAlive = CharData->bIsAlive;
		Card.HPFraction = BattleHUDView::PoolFraction(CharData->CurrentHP, CharData->MaxHP);
		Card.HPLine = BattleHUDView::FormatPool(BattleHUDView::GetHPLabel(), CharData->CurrentHP, CharData->MaxHP);
		// B-391b: a Broken Darkness character's EP is its absorption pool, which can overload past max (the old "Abs").
		Card.bBrokenDarkness = CharData->IsBrokenDarkness();
		Card.EPFraction = BattleHUDView::PoolFraction(CharData->CurrentEP, CharData->MaxEP); // B-503m
		Card.EPLine = Card.bBrokenDarkness ? BattleHUDView::FormatAbsorbLine(CharData->CurrentEP, CharData->MaxEP)
										   : BattleHUDView::FormatPool(BattleHUDView::GetEPLabel(), CharData->CurrentEP, CharData->MaxEP);
		Card.EPColour = Card.bBrokenDarkness ? BattleHUDView::GetOverloadColour(CharData->CurrentEP, CharData->MaxEP)
											 : BattleHUDStyle::TextColor;
		if (Card.bParty) // B-170e
		{
			// D-254 / D-259: "Act: ●●○"; the number, Max and the action-point progress are in inspect (BuildInspectText).
			Card.ActionsLine = ActionPipsText::MakeCardActionsLine(CharData->CurrentActions, CharData->GetMaxActions(),
																   CharData->SkillsUsedThisTurn); // Q-170j
		}
	}
	if (Cards.IsValidIndex(InspectedCard))
	{
		StatusPanelText = BuildInspectText(Cards[InspectedCard]); // its actions line follows the bank
	}
	if (CommandMenu)
	{
		CommandMenu->RefreshActionsLine(); // B-436g (D-260): the pips are on the menu's tab, live from the same events
	}
	PushToWidgets();
}

TArray<FActiveSkillEffect> UBattleHUDWidget::GetStatusesOf(AActor *Actor) const
{
	if (!Actor)
	{
		return {};
	}
	if (const USkillEffectManager *Effects = BoundEffects.Get())
	{
		return Effects->GetHeldEffects(Actor); // the mirror on a guest (B-318), GAS at UseGAS 1 (B-042ye)
	}
	const UWoREffectMirrorComponent *Mirror = Actor->FindComponentByClass<UWoREffectMirrorComponent>();
	return Mirror ? Mirror->GetHeldSnapshot() : TArray<FActiveSkillEffect>();
}

void UBattleHUDWidget::RefreshStatuses()
{
	TArray<FText> PanelLines;
	for (FBattleHUDCard &Card : Cards)
	{
		const TArray<FActiveSkillEffect> Statuses = GetStatusesOf(Card.Actor.Get());
		Card.StatusLine = BattleHUDView::JoinStatusChips(Statuses);

		PanelLines.Add(FText::Format(LOCTEXT("StatusPanelName", "{0}:"), Card.Name));
		if (Statuses.IsEmpty())
		{
			PanelLines.Add(LOCTEXT("StatusPanelNone", "  no statuses"));
		}
		for (const FActiveSkillEffect &Status : Statuses)
		{
			PanelLines.Add(FText::Format(LOCTEXT("StatusPanelLine", "  {0}"), BattleHUDView::DescribeStatus(Status)));
		}
	}
	StatusPanelText = Cards.IsValidIndex(InspectedCard) ? BuildInspectText(Cards[InspectedCard])
														: FText::Join(FText::FromString(TEXT("\n")), PanelLines);
	PushToWidgets();
}

FText UBattleHUDWidget::BuildInspectText(const FBattleHUDCard &Card) const
{
	TArray<FText> Lines;
	Lines.Add(Card.Name);
	const UCharacterDataComponent *CharData = Card.CharData.Get();
	if (CharData)
	{
		// D-254: Spirit of ... / Body of ... left the card for here (empty for anyone but a player character).
		const FText Identity = WoRPlayerBuild::GetDetailLine(CharData->CharacterData, CharData->PartyMemberId, GetWorld());
		if (!Identity.IsEmpty())
		{
			Lines.Add(Identity);
		}
		Lines.Add(ActionPipsText::MakeActionsLine(CharData->CurrentActions, CharData->GetMaxActions(), CharData->GetActionPointProgress(),
												  CharData->SkillsUsedThisTurn));
	}
	const TArray<FActiveSkillEffect> Statuses = GetStatusesOf(Card.Actor.Get());
	if (Statuses.IsEmpty())
	{
		Lines.Add(LOCTEXT("InspectNoStatuses", "No statuses"));
	}
	for (const FActiveSkillEffect &Status : Statuses)
	{
		Lines.Add(BattleHUDView::DescribeStatus(Status));
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

void UBattleHUDWidget::InspectCard(int32 CardIndex)
{
	OpenIntel(CardIndex); // D-261: was the party-only inspect in the status panel (D-254)
}

// ========================================
// INTEL (B-586, D-261)
// ========================================

TArray<int32> UBattleHUDWidget::GetIntelCardOrder() const
{
	TArray<int32> Order;
	for (const bool bParty : {true, false})
	{
		for (int32 Index = 0; Index < Cards.Num(); ++Index)
		{
			if (Cards[Index].bParty == bParty)
			{
				Order.Add(Index);
			}
		}
	}
	return Order;
}

void UBattleHUDWidget::OpenIntel(int32 CardIndex)
{
	const int32 Tab = GetIntelCardOrder().IndexOfByKey(CardIndex);
	if (Tab == INDEX_NONE)
	{
		return;
	}
	if (bIntelOpen && IntelTab == Tab)
	{
		CloseIntel(); // the same card again closes it
		return;
	}
	if (bStatusPanelOpen)
	{
		bStatusPanelOpen = false;
		InspectedCard = INDEX_NONE;
	}
	bIntelOpen = true;
	IntelTab = Tab;
	PushToWidgets();
}

void UBattleHUDWidget::ToggleIntel()
{
	if (bIntelOpen)
	{
		CloseIntel();
		return;
	}
	const int32 Featured = GetFeaturedCardIndex();
	const TArray<int32> Order = GetIntelCardOrder();
	OpenIntel(Cards.IsValidIndex(Featured) ? Featured : (Order.IsEmpty() ? INDEX_NONE : Order[0]));
}

void UBattleHUDWidget::StepIntelTab(int32 Delta)
{
	const int32 TabCount = GetIntelCardOrder().Num();
	if (!bIntelOpen || TabCount == 0)
	{
		return;
	}
	SelectIntelTab((IntelTab + Delta % TabCount + TabCount) % TabCount);
}

void UBattleHUDWidget::SelectIntelTab(int32 Tab)
{
	if (!bIntelOpen || !GetIntelCardOrder().IsValidIndex(Tab))
	{
		return;
	}
	IntelTab = Tab;
	PushToWidgets();
}

void UBattleHUDWidget::CloseIntel()
{
	bIntelOpen = false;
	PushToWidgets();
}

void UBattleHUDWidget::CycleIntel()
{
	if (!bIntelOpen)
	{
		ToggleIntel(); // the featured party card
		return;
	}
	const TArray<int32> Order = GetIntelCardOrder();
	const bool bOnParty = Order.IsValidIndex(IntelTab) && Cards[Order[IntelTab]].bParty;
	const int32 FirstEnemy = Order.IndexOfByPredicate([this](int32 CardIndex) { return !Cards[CardIndex].bParty; });
	if (bOnParty && FirstEnemy != INDEX_NONE)
	{
		SelectIntelTab(FirstEnemy);
		return;
	}
	CloseIntel();
}

void UBattleHUDWidget::PressIntelKey()
{
	if (IsIntelKeyCycle())
	{
		CycleIntel();
		return;
	}
	ToggleIntel();
}

bool UBattleHUDWidget::IsIntelKeyCycle() const
{
	const UWorld *World = GetWorld();
	const USaveSubsystem *Save = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	return !Save || Save->IsIntelKeyCycle();
}

void UBattleHUDWidget::HandleIntelStepClicked(int32 Delta)
{
	StepIntelTab(Delta);
}

void UBattleHUDWidget::RefreshIntelPages()
{
	IntelPages.Reset();
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const TArray<int32> Order = GetIntelCardOrder();
	for (int32 Tab = 0; Tab < Order.Num(); ++Tab)
	{
		const FBattleHUDCard &Card = Cards[Order[Tab]];
		if (Tab == IntelTab)
		{
			IntelPages.Add(BattleIntelView::BuildPage(Card, GetStatusesOf(Card.Actor.Get()), Run));
			continue;
		}
		// Review: pushes come on every HP or build-up change, so a tab not shown carries only what its tab needs.
		FBattleIntelPage &Page = IntelPages.AddDefaulted_GetRef();
		Page.Actor = Card.Actor;
		Page.bEnemy = !Card.bParty;
		Page.Heading = Card.Name;
		Page.bYou = Card.bParty && BattleIntelView::IsYou(Card.Actor.Get());
		Page.Tab = Page.bYou ? LOCTEXT("IntelYouTab", "You") : Card.Name;
	}
}

FEventReply UBattleHUDWidget::HandleCardMouseDown(FGeometry MyGeometry, const FPointerEvent &MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FEventReply(false);
	}
	// One handler for every card: the card under the pointer is the one clicked.
	const int32 Clicked = Cards.IndexOfByPredicate([&MouseEvent](const FBattleHUDCard &Card)
	{
		return Card.Frame && Card.Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	});
	if (Clicked == INDEX_NONE)
	{
		return FEventReply(false);
	}
	InspectCard(Clicked);
	return FEventReply(true);
}

void UBattleHUDWidget::RefreshTurnStrip()
{
	const TArray<FBattleTurnSlot> Strip = BattleHUDView::ReadTurnStrip(this, CurrentTurnNumber, BattleHUDView::UPCOMING_TURNS_SHOWN);
	const AActor *Acting = Strip.IsEmpty() ? nullptr : Strip[0].Actor.Get();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		FBattleHUDCard &Card = Cards[Index];
		Card.bActing = Acting && Card.Actor.Get() == Acting;
		if (Card.bActing && Card.bParty)
		{
			LastActingPartyCard = Index; // B-436a: stays featured through the enemies' turns
		}
		if (Card.bParty && Card.Actor.IsValid())
		{
			Card.Name = BattleHUDView::GetCombatantName(Card.Actor.Get()); // D-229: a claim or tag can arrive after the card was built
		}
	}

	TurnLines.Reset();
	TurnLineColors.Reset();
	for (const FBattleTurnSlot &TurnSlot : Strip)
	{
		const AActor *Actor = TurnSlot.Actor.Get();
		if (!Actor)
		{
			continue; // an empty slot shows nothing
		}
		const FBattleHUDCard *Card = Cards.FindByPredicate([Actor](const FBattleHUDCard &Candidate) { return Candidate.Actor.Get() == Actor; });
		const FText Name = Card ? Card->Name : BattleHUDView::GetCombatantName(Actor);
		const FText Bonus = TurnSlot.bBonus ? LOCTEXT("BonusTurn", " (bonus)") : FText::GetEmpty();
		TurnLines.Add(TurnSlot.bCurrent ? FText::Format(LOCTEXT("TurnNow", "> {0}  {1}{2}"), FText::AsNumber(TurnSlot.TurnNumber), Name, Bonus)
									: FText::Format(LOCTEXT("TurnNext", "   {0}  {1}{2}"), FText::AsNumber(TurnSlot.TurnNumber), Name, Bonus));
		TurnLineColors.Add(TurnSlot.bCurrent		  ? BattleHUDStyle::ActingColor
						   : TurnSlot.bBonus		  ? BattleHUDStyle::BonusColor
						   : Card && !Card->bParty ? BattleHUDStyle::EnemyColor
												  : BattleHUDStyle::TextColor);
	}
	PushToWidgets();
}

int32 UBattleHUDWidget::GetFeaturedCardIndex() const
{
	if (Cards.IsValidIndex(LastActingPartyCard))
	{
		return LastActingPartyCard;
	}
	return Cards.IndexOfByPredicate([](const FBattleHUDCard &Card) { return Card.bParty; });
}

void UBattleHUDWidget::SetCommandMenu(UBattleCommandMenuWidget *Menu)
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleHUD] SetCommandMenu after the HUD was built: the menu is not placed"));
		return;
	}
	CommandMenu = Menu;
}

FVector2D UBattleHUDWidget::GetCommandMenuOffset()
{
	return FVector2D(BattleHUDStyle::ScreenMargin,
					 -(BattleHUDStyle::ScreenMargin + BattleHUDStyle::FeaturedCardReserve + BattleHUDStyle::CardGap));
}

void UBattleHUDWidget::LayoutPartyCards()
{
	const int32 Featured = GetFeaturedCardIndex();
	const float FullCardOuterWidth = BattleHUDStyle::CardWidth + 2.0f * BattleHUDStyle::CardPadding;
	const float CompactCardOuterWidth = BattleHUDStyle::CompactCardWidth + 2.0f * BattleHUDStyle::CardPadding;
	// B-436g: left offsets inside the party overlay, which sits at the bottom-left margin.
	float CompactX = FullCardOuterWidth + BattleHUDStyle::CardGap;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const FBattleHUDCard &Card = Cards[Index];
		if (!Card.bParty)
		{
			continue;
		}
		const bool bFeatured = Index == Featured;
		BattleHUDCard::SetCompact(Card, !bFeatured);
		if (UOverlaySlot *CardSlot = Card.Frame ? Cast<UOverlaySlot>(Card.Frame->Slot) : nullptr)
		{
			CardSlot->SetPadding(FMargin(bFeatured ? 0.0f : CompactX, 0.0f, 0.0f, 0.0f));
		}
		if (!bFeatured)
		{
			CompactX += CompactCardOuterWidth + BattleHUDStyle::CardGap;
		}
	}
}

void UBattleHUDWidget::RefreshGear()
{
	for (FBattleHUDCard &Card : Cards)
	{
		if (!Card.bParty)
		{
			continue;
		}
		const FBattleGearNames Gear = BattleHUDView::ReadGearNames(Card.Actor.Get());
		Card.GearLine = BattleHUDView::FormatGearLine(Gear);
		const FText Tooltip = BattleHUDView::FormatGearTooltip(Gear);
		// Review fix: the tooltip is set only when it changes (setting it rebuilds the Slate tooltip and can close an
		// open one), not on every push.
		if (!Tooltip.EqualTo(Card.GearTooltip))
		{
			Card.GearTooltip = Tooltip;
			if (Card.Frame)
			{
				Card.Frame->SetToolTipText(Card.GearTooltip);
			}
		}
	}
	PushToWidgets();
}

void UBattleHUDWidget::SetCardBuildup(FBattleHUDCard &Card, float Current, float Max, ESpellElement PendingElement, const AActor *Source)
{
	Card.BuildupFraction = BattleHUDView::BuildupFraction(Current, Max);
	Card.CracksShown = BattleHUDView::CountCracksShown(Card.BuildupFraction);
	Card.BuildupColour = BattleHUDView::GetBuildupColour(PendingElement, Source && UBrokenDarknessManager::IsInBDForm(Source));
	Card.BuildupLine = BattleHUDView::FormatBuildupLine(Current, Max);
	// B-453f (D-190): the bar's colour says the pending element; with element letters on, the line says it too.
	const UWorld *World = Card.Actor.IsValid() ? Card.Actor->GetWorld() : nullptr; // static: the card's actor gives the world
	const USaveSubsystem *Save = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	// Not in Broken Darkness form: its bar is the hybrid colour, which one element's letters would misname.
	const bool bHybridBar = Source && UBrokenDarknessManager::IsInBDForm(Source);
	const FText Letter = PendingElement == ESpellElement::Generic || bHybridBar ? FText::GetEmpty() : WoRGameNames::GetElementLetter(PendingElement);
	if (Save && Save->IsElementLettersOn() && !Letter.IsEmpty())
	{
		Card.BuildupLine = FText::Format(LOCTEXT("BuildupWithLetter", "{0} {1}"), Card.BuildupLine, Letter);
	}
}

void UBattleHUDWidget::SetStatusPanelOpen(bool bOpen)
{
	bStatusPanelOpen = bOpen;
	if (bOpen)
	{
		bIntelOpen = false; // B-586: never both

		RefreshStatuses(); // pushes too
		return;
	}
	InspectedCard = INDEX_NONE; // closed: the next open is the view of everyone unless a card asks
	PushToWidgets();
}

void UBattleHUDWidget::PushToWidgets()
{
	for (const FBattleHUDCard &Card : Cards)
	{
		BattleHUDCard::Push(Card);
	}
	LayoutPartyCards(); // B-436a: after Push, so a compact card's hidden lines stay hidden
	for (int32 Index = 0; Index < TurnTexts.Num(); ++Index)
	{
		UTextBlock *Text = TurnTexts[Index];
		if (!Text)
		{
			continue;
		}
		const bool bUsed = TurnLines.IsValidIndex(Index);
		Text->SetText(bUsed ? TurnLines[Index] : FText::GetEmpty());
		Text->SetColorAndOpacity(FSlateColor(bUsed ? TurnLineColors[Index] : BattleHUDStyle::TextColor));
		// Hidden, not Collapsed: the enemy cards sit under the strip (B-436a) and must not jump as it shortens.
		Text->SetVisibility(bUsed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	BattleHUDBuild::SetTextSafe(StatusPanelBody, StatusPanelText);
	if (StatusPanel)
	{
		StatusPanel->SetVisibility(bStatusPanelOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bIntelOpen) // B-586: built from the live cards on every push while open
	{
		RefreshIntelPages();
	}
	if (IntelPanel)
	{
		IntelPanel->SetVisibility(bIntelOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		IntelPanel->Push(IntelPages, IntelTab, IsIntelKeyCycle());
	}
	// B-587 (Crown, as Expedition 33's close-up hides its action UI): while Intel is open the enemy cards (Intel takes
	// their place under the turn order), the command menu and the corner hints step aside; no menu key acts meanwhile.
	const ESlateVisibility UnderIntel = bIntelOpen ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
	for (UWidget *Piece : {EnemyColumn.Get(), MenuHolder.Get(), CornerHints.Get()})
	{
		if (Piece)
		{
			Piece->SetVisibility(UnderIntel);
		}
	}
	UpdateIntelCamera();
}

void UBattleHUDWidget::UpdateIntelCamera()
{
	const TArray<int32> Order = GetIntelCardOrder();
	AActor *Subject = bIntelOpen && Order.IsValidIndex(IntelTab) ? Cards[Order[IntelTab]].Actor.Get() : nullptr;
	if (Subject == IntelCameraSubject.Get())
	{
		return;
	}
	IntelCameraSubject = Subject;
	for (TActorIterator<ACombatOrchestrator> It(GetWorld()); It; ++It)
	{
		if (UCombatCameraDirector *Director = It->GetCameraDirector())
		{
			Director->SetIntelSubject(Subject); // a cut, or nothing with movement off (D-262 b, d)
			return;
		}
	}
}

// ========================================
// HANDLERS (each refreshes from the source; few combatants, so no per-actor filtering)
// ========================================

void UBattleHUDWidget::HandlePoolChanged(int32 Current, int32 Max)
{
	RefreshPools();
}

void UBattleHUDWidget::HandleLifeChanged(AActor *Actor)
{
	RefreshPools();
}

void UBattleHUDWidget::HandleEffectChanged(AActor *Target, const FActiveSkillEffect &Effect)
{
	RefreshStatuses();
}

void UBattleHUDWidget::HandleEffectCountChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 Count)
{
	RefreshStatuses();
}

void UBattleHUDWidget::HandleHeldSnapshotChanged(AActor *Holder)
{
	RefreshStatuses();
}

void UBattleHUDWidget::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
{
	CurrentTurnNumber = TurnNumber;
	RefreshTurnStrip();
	RefreshStatuses(); // durations are shown per turn
	RefreshPools();	   // B-170e: Max Actions can move with buffs that don't change the bank
}

void UBattleHUDWidget::HandleTurnStateChanged()
{
	if (const UTurnStateComponent *TurnState = BoundTurnState.Get())
	{
		CurrentTurnNumber = FMath::Max(1, TurnState->GetTurnNumber());
	}
	RefreshTurnStrip();
	RefreshStatuses();
	RefreshPools(); // B-170e, as HandleTurnStarted
}

void UBattleHUDWidget::ToggleAllStatuses()
{
	const bool bWasInspecting = Cards.IsValidIndex(InspectedCard);
	InspectedCard = INDEX_NONE;
	SetStatusPanelOpen(bWasInspecting || !bStatusPanelOpen);
}

void UBattleHUDWidget::HandleStatusButtonClicked()
{
	// B-587 QA (D-261: Intel replaced the status list): open Intel on the party, or close it.
	if (bIntelOpen)
	{
		CloseIntel();
		return;
	}
	ToggleIntel();
}

void UBattleHUDWidget::HandleHitShown(AActor *Target, const FCombatHitShown &Hit)
{
	ShowDamageNumber(Target, Hit);
	// B-586 (Q-586): a hit of a type teaches that type's line for this enemy type, for the run (every machine sees the hit).
	const FBattleHUDCard *Card = Cards.FindByPredicate([Target](const FBattleHUDCard &Candidate) { return Candidate.Actor.Get() == Target; });
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const FName TypeKey = BattleIntelView::GetEnemyTypeKey(Target);
	if (Card && !Card->bParty && Run && !Hit.bHeal && Hit.DamageType != EWoRDamageType::None && !TypeKey.IsNone()
		&& !Run->IsIntelLearned(TypeKey, Hit.DamageType))
	{
		Run->MarkIntelLearned(TypeKey, Hit.DamageType);
		UE_LOG(LogWoRUI, Log, TEXT("[BattleHUD] Intel learned: %s %s"), *TypeKey.ToString(), *UEnum::GetValueAsString(Hit.DamageType));
		PushToWidgets();
	}
}

void UBattleHUDWidget::HandleBuildupChanged(AActor *Target, float Current, float Max, ESpellElement PendingElement, AActor *Source)
{
	FBattleHUDCard *Card = Cards.FindByPredicate([Target](const FBattleHUDCard &Candidate) { return Candidate.Actor.Get() == Target; });
	if (!Card)
	{
		return; // not a carded combatant
	}
	SetCardBuildup(*Card, Current, Max, PendingElement, Source);
	PushToWidgets();
}

void UBattleHUDWidget::HandleLoadoutChanged(int32 NewLoadoutIndex)
{
	RefreshGear();
}

void UBattleHUDWidget::HandleActiveWeaponChanged(UWeaponData *NewWeapon)
{
	RefreshGear();
}

// ========================================
// DAMAGE NUMBERS (B-391b: a pool made once; one timer while any is live)
// ========================================

void UBattleHUDWidget::EnsureDamageNumberPool()
{
	if (DamageNumbers.IsEmpty())
	{
		DamageNumbers.SetNum(DamageNumberView::MAX_NUMBERS);
	}
}

void UBattleHUDWidget::BuildDamageNumberWidgets(UCanvasPanel *Canvas)
{
	EnsureDamageNumberPool();
	for (FBattleDamageNumber &Number : DamageNumbers)
	{
		Number.Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Number.Text->SetJustification(ETextJustify::Center);
		Number.Text->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot *CanvasSlot = Canvas->AddChildToCanvas(Number.Text))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f)); // positioned in viewport space each step
			CanvasSlot->SetAlignment(BattleHUDStyle::DamageNumberAlignment);
			CanvasSlot->SetAutoSize(true);
		}
	}
}

int32 UBattleHUDWidget::GetLiveDamageNumberCount() const
{
	int32 Live = 0;
	for (const FBattleDamageNumber &Number : DamageNumbers)
	{
		Live += Number.bLive ? 1 : 0;
	}
	return Live;
}

void UBattleHUDWidget::ShowDamageNumber(AActor *Target, const FCombatHitShown &Hit)
{
	UWorld *World = GetWorld();
	// B-391c (Q-391c): a hit Broken Darkness took fully reads "Negated" rather than nothing (Crown, 2026-11-12).
	const UCharacterDataComponent *TargetData = Target ? Target->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const bool bNegated = DamageNumberView::SHOW_NEGATED_CUE &&
						   DamageNumberView::IsNegatedHit(Hit, TargetData && TargetData->IsBrokenDarkness());
	if (!Target || !World || (!bNegated && !DamageNumberView::ShouldShow(Hit)))
	{
		return;
	}
	EnsureDamageNumberPool();

	// A free slot, else the oldest live one.
	int32 SlotIndex = INDEX_NONE;
	for (int32 Index = 0; Index < DamageNumbers.Num(); ++Index)
	{
		const FBattleDamageNumber &Candidate = DamageNumbers[Index];
		const bool bBetter = SlotIndex == INDEX_NONE || (DamageNumbers[SlotIndex].bLive &&
														 (!Candidate.bLive || Candidate.SpawnTime < DamageNumbers[SlotIndex].SpawnTime));
		if (bBetter)
		{
			SlotIndex = Index;
		}
	}
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}
	// Review fix: one row above this target's highest live number (not a count, which reused a row still up).
	TArray<int32> LiveStackIndices;
	for (int32 Index = 0; Index < DamageNumbers.Num(); ++Index)
	{
		const FBattleDamageNumber &Other = DamageNumbers[Index];
		if (Index != SlotIndex && Other.bLive && Other.Target.Get() == Target)
		{
			LiveStackIndices.Add(Other.StackIndex);
		}
	}

	FBattleDamageNumber &Number = DamageNumbers[SlotIndex];
	Number.Target = Target;
	Number.Hit = Hit;
	// B-453f (D-190): the player's "Element letters on colours" setting tags elemental numbers.
	const USaveSubsystem *Save = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	const bool bLetters = Save && Save->IsElementLettersOn();
	Number.Style = bNegated ? DamageNumberView::MakeNegatedStyle(Hit, bLetters) : DamageNumberView::MakeStyle(Hit, bLetters);
	Number.WorldPoint = Target->GetActorLocation() + FVector(0.0f, 0.0f, Target->GetSimpleCollisionHalfHeight());
	Number.SpawnTime = World->GetTimeSeconds();
	Number.StackIndex = DamageNumberView::NextStackIndex(LiveStackIndices);
	Number.bLive = true;
	if (UTextBlock *Text = Number.Text)
	{
		Text->SetText(Number.Style.Text);
		Text->SetColorAndOpacity(FSlateColor(Number.Style.Colour));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Number.Style.FontSize;
		Font.OutlineSettings.OutlineSize = Number.Style.OutlineSize;
		Font.OutlineSettings.OutlineColor = Number.Style.OutlineColour;
		Text->SetFont(Font);
		Text->SetRenderOpacity(1.0f);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	PlaceDamageNumber(Number, Number.SpawnTime);

	++DamageNumbersShown;
	LastDamageNumber = FString::Printf(TEXT("%s on %s"), *DamageNumberView::DescribeStyle(Hit, Number.Style), *Target->GetName());
	UE_LOG(LogWoRUI, Verbose, TEXT("[BattleHUD] Damage number: %s"), *LastDamageNumber);

	if (!World->GetTimerManager().IsTimerActive(DamageNumberTimer))
	{
		World->GetTimerManager().SetTimer(DamageNumberTimer, this, &UBattleHUDWidget::StepDamageNumbers, DamageNumberView::STEP_SECONDS,
										  /*bLoop=*/true);
	}
}

void UBattleHUDWidget::StepDamageNumbers()
{
	UWorld *World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	int32 Live = 0;
	for (FBattleDamageNumber &Number : DamageNumbers)
	{
		if (!Number.bLive)
		{
			continue;
		}
		if (Now - Number.SpawnTime >= DamageNumberView::LIFETIME_SECONDS)
		{
			HideDamageNumber(Number); // its time is up: removed
			continue;
		}
		PlaceDamageNumber(Number, Now);
		++Live;
	}
	if (Live == 0)
	{
		World->GetTimerManager().ClearTimer(DamageNumberTimer);
	}
}

void UBattleHUDWidget::PlaceDamageNumber(FBattleDamageNumber &Number, double Now)
{
	UTextBlock *Text = Number.Text;
	if (!Text)
	{
		return; // unbuilt (headless): the state alone
	}
	const float Age = static_cast<float>(Now - Number.SpawnTime);
	FVector2D Point;
	APlayerController *PC = GetOwningPlayer();
	const bool bOnScreen = PC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Number.WorldPoint, Point,
																							 /*bPlayerViewportRelative=*/false);
	if (!bOnScreen)
	{
		Text->SetVisibility(ESlateVisibility::Collapsed); // behind the camera: hidden until it comes back or expires
		return;
	}
	const float Lift = DamageNumberView::GetRiseOffset(Age) + Number.StackIndex * DamageNumberView::STACK_SPACING;
	if (UCanvasPanelSlot *CanvasSlot = Cast<UCanvasPanelSlot>(Text->Slot))
	{
		CanvasSlot->SetPosition(Point - FVector2D(0.0f, Lift));
	}
	Text->SetRenderOpacity(DamageNumberView::GetOpacity(Age));
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBattleHUDWidget::HideDamageNumber(FBattleDamageNumber &Number)
{
	Number.bLive = false;
	Number.Target.Reset();
	if (Number.Text)
	{
		Number.Text->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ========================================
// DEBUG
// ========================================

FString UBattleHUDWidget::GetBattleHUDString() const
{
	FString Out = FString::Printf(TEXT("[BattleHUD] bound=%s cards=%d turn=%d statusPanel=%s built=%s"), bBound ? TEXT("yes") : TEXT("no"),
								  Cards.Num(), CurrentTurnNumber, bStatusPanelOpen ? TEXT("open") : TEXT("closed"),
								  WidgetTree && WidgetTree->RootWidget ? TEXT("yes") : TEXT("no"));
	if (Cards.IsValidIndex(InspectedCard)) // D-254
	{
		Out += FString::Printf(TEXT("\n  inspecting %s:\n    %s"), *Cards[InspectedCard].Name.ToString(),
							   *StatusPanelText.ToString().Replace(TEXT("\n"), TEXT("\n    ")));
	}
	if (bIntelOpen && IntelPages.IsValidIndex(IntelTab)) // B-586
	{
		Out += FString::Printf(TEXT("\n  intel tab %d/%d: %s"), IntelTab + 1, IntelPages.Num(),
							   *BattleIntelView::GetPageString(IntelPages[IntelTab]).Replace(TEXT("\n"), TEXT("\n    ")));
	}
	const int32 Featured = GetFeaturedCardIndex();
	const FBattleHUDCard *FeaturedCard = Cards.IsValidIndex(Featured) ? &Cards[Featured] : nullptr;
	for (const FBattleHUDCard &Card : Cards)
	{
		Out += FString::Printf(TEXT("\n  %s %s%s%s | %s | %s | %s | %s, %d crack(s)"), Card.bParty ? TEXT("party") : TEXT("enemy"),
							   *Card.Name.ToString(), Card.bActing ? TEXT(" (acting)") : TEXT(""), Card.bAlive ? TEXT("") : TEXT(" (down)"),
							   *Card.HPLine.ToString(), Card.bShowEP ? *Card.EPLine.ToString() : TEXT("no EP"),
							   Card.StatusLine.IsEmpty() ? TEXT("no statuses") : *Card.StatusLine.ToString(), *Card.BuildupLine.ToString(),
							   Card.CracksShown);
		if (Card.bParty) // B-391c
		{
			Out += FString::Printf(TEXT(" | gear: %s"), Card.GearLine.IsEmpty() ? TEXT("none") : *Card.GearLine.ToString());
			Out += &Card == FeaturedCard ? TEXT(" | featured (full card)") : TEXT(" | compact"); // B-436a
		}
		if (Card.bParty)
		{
			Out += FString::Printf(TEXT(" | %s"), *Card.ActionsLine.ToString()); // B-170e
		}
	}
	for (const FText &Line : TurnLines)
	{
		Out += FString::Printf(TEXT("\n  turn: %s"), *Line.ToString());
	}
	Out += FString::Printf(TEXT("\n  damage numbers: %d live, %d shown, last %s"), GetLiveDamageNumberCount(), DamageNumbersShown,
						   LastDamageNumber.IsEmpty() ? TEXT("none") : *LastDamageNumber);
	return Out;
}

// wor.UI.DamageNumber <amount> [damage type] [multiplier]: a local preview over the first enemy card (else the first card).
namespace BattleHUDDamageNumberArgs
{
	constexpr int32 Amount = 0;
	constexpr int32 DamageType = 1;
	constexpr int32 Multiplier = 2;
}

static FAutoConsoleCommandWithWorldAndArgs GBattleHUDDamageNumberCommand(
	TEXT("wor.UI.DamageNumber"),
	TEXT("B-391b: preview a damage number on the new battle HUD (this machine only). Args: <amount> [Fire|Slash|Ice|...|Heal] [multiplier, 1 = neutral, 0 = immune]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		UBattleHUDWidget *HUD = nullptr;
		for (TObjectIterator<UBattleHUDWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && !It->GetCards().IsEmpty())
			{
				HUD = *It;
				break;
			}
		}
		if (!HUD)
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[BattleHUD] wor.UI.DamageNumber: no new battle HUD in this world (wor.UI.NewHUD 1, in a fight)"));
			return;
		}
		FCombatHitShown Hit;
		Hit.Amount = Args.IsValidIndex(BattleHUDDamageNumberArgs::Amount) ? FCString::Atoi(*Args[BattleHUDDamageNumberArgs::Amount]) : 0;
		if (Args.IsValidIndex(BattleHUDDamageNumberArgs::DamageType))
		{
			const FString &TypeName = Args[BattleHUDDamageNumberArgs::DamageType];
			Hit.bHeal = TypeName.Equals(TEXT("Heal"), ESearchCase::IgnoreCase);
			const int64 Type = StaticEnum<EWoRDamageType>()->GetValueByNameString(TypeName);
			if (!Hit.bHeal && Type == INDEX_NONE)
			{
				UE_LOG(LogWoRUI, Warning, TEXT("[BattleHUD] wor.UI.DamageNumber: unknown damage type '%s'; using None"), *TypeName);
			}
			Hit.DamageType = Type == INDEX_NONE ? EWoRDamageType::None : static_cast<EWoRDamageType>(Type);
		}
		Hit.ElementMultiplier = Args.IsValidIndex(BattleHUDDamageNumberArgs::Multiplier) ? FCString::Atof(*Args[BattleHUDDamageNumberArgs::Multiplier]) : 1.0f;
		const FBattleHUDCard *Card = HUD->GetCards().FindByPredicate([](const FBattleHUDCard &Candidate) { return !Candidate.bParty; });
		AActor *Target = Card ? Card->Actor.Get() : HUD->GetCards()[0].Actor.Get();
		HUD->ShowDamageNumber(Target, Hit);
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *HUD->GetBattleHUDString());
	}));

#undef LOCTEXT_NAMESPACE
