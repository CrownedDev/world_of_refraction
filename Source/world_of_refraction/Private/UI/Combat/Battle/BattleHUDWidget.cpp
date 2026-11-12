// BattleHUDWidget.cpp

#include "UI/Combat/Battle/BattleHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/CombatPresentationComponent.h"
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
#include "UI/Combat/Battle/BattleHUDButton.h"
#include "Widgets/Layout/Anchors.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "BattleHUD"

namespace BattleHUDStyle
{
	// TODO: style pass (placeholder until the CommonUI screens, D-024). Slate units, DPI-scaled.
	constexpr float ScreenMargin = 24.0f;
	constexpr float CardWidth = 260.0f;
	constexpr float BarHeight = 10.0f;
	constexpr float CardPadding = 6.0f;
	constexpr float RowPadding = 2.0f;
	constexpr float StatusPanelWidth = 640.0f;
	constexpr int32 NameFontSize = 16;
	constexpr int32 LineFontSize = 13;
	constexpr int32 TurnFontSize = 14;
	constexpr float DownedCardOpacity = 0.4f;

	const FLinearColor CardColor(0.0f, 0.0f, 0.0f, 0.55f);
	const FLinearColor PanelColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FLinearColor TextColor = FLinearColor::White;
	const FLinearColor ActingColor(1.0f, 0.78f, 0.25f);  // warm gold: whose turn it is
	const FLinearColor EnemyColor(0.85f, 0.55f, 0.5f);   // the other side, in the turn strip
	const FLinearColor BonusColor(0.45f, 0.9f, 0.55f);   // a bonus turn (Emerald), as the old strip's green
	const FLinearColor PartyHPColor(0.75f, 0.1f, 0.1f);
	const FLinearColor EnemyHPColor(0.55f, 0.1f, 0.1f);

	const FVector2D TurnStripAnchor(0.0f, 0.0f);    // left edge, top (Expedition 33's turn column)
	const FVector2D EnemyAnchor(1.0f, 0.0f);        // top-right
	const FVector2D PartyAnchor(0.0f, 1.0f);        // bottom-left (the command menu is bottom-right)
	const FVector2D StatusButtonAnchor(0.5f, 0.0f); // top centre
	const FVector2D StatusPanelAnchor(0.5f, 0.5f);  // centre
	constexpr float TurnStripTopOffset = 24.0f;

	// B-391b: a damage number's text is centred on its point and sits above it.
	const FVector2D DamageNumberAlignment(0.5f, 1.0f);

	// B-391c: the status buildup bar under the HP bar, and its cracks (Q-111 A).
	constexpr float BuildupBarHeight = 6.0f;
	constexpr float CrackWidth = 2.0f;
	const FLinearColor CrackColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FLinearColor GearColor(0.8f, 0.8f, 0.8f);
}

namespace BattleHUDBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box, int32 FontSize)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(BattleHUDStyle::TextColor));
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(BattleHUDStyle::RowPadding));
		}
		return Text;
	}

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
	// B-170e: the fight's mode is latched at its start (InitializeCombat runs before the HUD is built).
	bShowActions = ActionPipsText::IsActionPipsFight(this);
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
	if (CharData)
	{
		CharData->OnHPChanged.AddDynamic(this, &UBattleHUDWidget::HandlePoolChanged);
		CharData->OnEPChanged.AddDynamic(this, &UBattleHUDWidget::HandlePoolChanged);
		CharData->OnDied.AddDynamic(this, &UBattleHUDWidget::HandleLifeChanged);
		CharData->OnResurrected.AddDynamic(this, &UBattleHUDWidget::HandleLifeChanged);
		// A Resonator with nothing to spend EP on has no EP line, as the old panel (init-only there too). B-391b: a Broken
		// Darkness character always shows its absorption pool.
		Card.bShowEP = bParty && (CharData->HasUsableEPTarget() || CharData->IsBrokenDarkness());
		if (bParty && bShowActions)
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
	bBound = false;
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

		// Turn order: the left edge, top.
		UVerticalBox *TurnColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TurnOrder"));
		TurnColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
		BattleHUDBuild::AddText(WidgetTree, TurnColumn, BattleHUDStyle::NameFontSize)->SetText(LOCTEXT("TurnOrderTitle", "Turn order"));
		for (int32 Index = 0; Index <= BattleHUDView::UPCOMING_TURNS_SHOWN; ++Index)
		{
			TurnTexts.Add(BattleHUDBuild::AddText(WidgetTree, TurnColumn, BattleHUDStyle::TurnFontSize));
		}
		BattleHUDBuild::Place(Canvas, TurnColumn, BattleHUDStyle::TurnStripAnchor,
							  BattleHUDBuild::Inset(BattleHUDStyle::TurnStripAnchor) + FVector2D(0.0f, BattleHUDStyle::TurnStripTopOffset));

		// One column per side; one card per combatant, made here once.
		UVerticalBox *PartyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Party"));
		UVerticalBox *EnemyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Enemies"));
		PartyColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // B-391c: a party card takes a hover (its tooltip)
		EnemyColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
		for (FBattleHUDCard &Card : Cards)
		{
			BuildCardWidgets(Card, Card.bParty ? PartyColumn : EnemyColumn);
		}
		BattleHUDBuild::Place(Canvas, PartyColumn, BattleHUDStyle::PartyAnchor, BattleHUDBuild::Inset(BattleHUDStyle::PartyAnchor));
		BattleHUDBuild::Place(Canvas, EnemyColumn, BattleHUDStyle::EnemyAnchor, BattleHUDBuild::Inset(BattleHUDStyle::EnemyAnchor));

		// The one button that shows every status (FeelGuide 5).
		UButton *StatusButton = WidgetTree->ConstructWidget<UBattleHUDButton>(UBattleHUDButton::StaticClass(), TEXT("StatusesButton")); // not focusable: keeps A / Enter for the command menu
		UTextBlock *StatusButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		StatusButtonLabel->SetText(LOCTEXT("StatusesButton", "Statuses"));
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

		BuildDamageNumberWidgets(Canvas); // last, so the numbers draw over the cards
	}
	return Super::RebuildWidget();
}

void UBattleHUDWidget::BuildCardWidgets(FBattleHUDCard &Card, UVerticalBox *Column)
{
	Card.Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card.Frame->SetBrushColor(BattleHUDStyle::CardColor);
	Card.Frame->SetPadding(FMargin(BattleHUDStyle::CardPadding));
	// B-391c: a party card is hit-testable so its gear tooltip shows on hover; an enemy card lets clicks through.
	Card.Frame->SetVisibility(Card.bParty ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	if (Card.bParty)
	{
		Card.Frame->SetToolTipText(Card.GearTooltip); // Initialise's RefreshGear ran before the widgets existed
	}
	if (UVerticalBoxSlot *ColumnSlot = Column->AddChildToVerticalBox(Card.Frame))
	{
		ColumnSlot->SetPadding(FMargin(BattleHUDStyle::RowPadding));
	}
	USizeBox *Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(BattleHUDStyle::CardWidth);
	Card.Frame->SetContent(Width);
	UVerticalBox *Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Width->AddChild(Rows);

	Card.NameText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::NameFontSize);

	USizeBox *BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarSize->SetHeightOverride(BattleHUDStyle::BarHeight);
	Card.HPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	Card.HPBar->SetFillColorAndOpacity(Card.bParty ? BattleHUDStyle::PartyHPColor : BattleHUDStyle::EnemyHPColor);
	BarSize->AddChild(Card.HPBar);
	if (UVerticalBoxSlot *BarSlot = Rows->AddChildToVerticalBox(BarSize))
	{
		BarSlot->SetPadding(FMargin(BattleHUDStyle::RowPadding));
	}

	BuildBuildupWidgets(Card, Rows);

	if (Card.bParty) // Q-391: enemies show a bar, no numbers (Expedition 33)
	{
		Card.HPText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::LineFontSize);
		Card.EPText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::LineFontSize);
		Card.EPText->SetVisibility(Card.bShowEP ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Card.BuildupText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::LineFontSize);
		Card.GearText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::LineFontSize);
		Card.GearText->SetColorAndOpacity(FSlateColor(BattleHUDStyle::GearColor));
		Card.ActionsText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::LineFontSize); // B-170e
		Card.ActionsText->SetVisibility(bShowActions ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	Card.StatusText = BattleHUDBuild::AddText(WidgetTree, Rows, BattleHUDStyle::LineFontSize);
}

void UBattleHUDWidget::BuildBuildupWidgets(FBattleHUDCard &Card, UVerticalBox *Rows)
{
	// The bar under the HP bar, with a row of crack marks laid over it: spacers weighted by the gaps between
	// BattleHUDView::GetCrackPositions, a thin mark at each. Marks are Hidden (not Collapsed) so the row keeps its spacing.
	USizeBox *BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarSize->SetHeightOverride(BattleHUDStyle::BuildupBarHeight);
	UOverlay *BarLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	BarSize->AddChild(BarLayers);

	Card.BuildupBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	if (UOverlaySlot *BarSlot = BarLayers->AddChildToOverlay(Card.BuildupBar))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UHorizontalBox *CrackRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CrackRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot *CrackRowSlot = BarLayers->AddChildToOverlay(CrackRow))
	{
		CrackRowSlot->SetHorizontalAlignment(HAlign_Fill);
		CrackRowSlot->SetVerticalAlignment(VAlign_Fill);
	}

	const auto AddGap = [this, CrackRow](float Weight)
	{
		USpacer *Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		if (UHorizontalBoxSlot *GapSlot = CrackRow->AddChildToHorizontalBox(Gap))
		{
			FSlateChildSize GapSize(ESlateSizeRule::Fill);
			GapSize.Value = Weight;
			GapSlot->SetSize(GapSize);
		}
	};
	float PreviousPosition = 0.0f;
	for (const float Position : BattleHUDView::GetCrackPositions())
	{
		AddGap(Position - PreviousPosition);
		USizeBox *MarkSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MarkSize->SetWidthOverride(BattleHUDStyle::CrackWidth);
		UBorder *Mark = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Mark->SetBrushColor(BattleHUDStyle::CrackColor);
		MarkSize->AddChild(Mark);
		MarkSize->SetVisibility(ESlateVisibility::Hidden);
		if (UHorizontalBoxSlot *MarkSlot = CrackRow->AddChildToHorizontalBox(MarkSize))
		{
			MarkSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			MarkSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Card.CrackMarks.Add(MarkSize);
		PreviousPosition = Position;
	}
	AddGap(1.0f - PreviousPosition);

	if (UVerticalBoxSlot *BuildupSlot = Rows->AddChildToVerticalBox(BarSize))
	{
		BuildupSlot->SetPadding(FMargin(BattleHUDStyle::RowPadding));
	}
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
		Card.EPLine = Card.bBrokenDarkness ? BattleHUDView::FormatAbsorbLine(CharData->CurrentEP, CharData->MaxEP)
										   : BattleHUDView::FormatPool(BattleHUDView::GetEPLabel(), CharData->CurrentEP, CharData->MaxEP);
		Card.EPColour = Card.bBrokenDarkness ? BattleHUDView::GetOverloadColour(CharData->CurrentEP, CharData->MaxEP)
											 : BattleHUDStyle::TextColor;
		if (Card.bParty && bShowActions) // B-170e
		{
			Card.ActionsLine = ActionPipsText::MakeActionsLine(CharData->CurrentActions, CharData->GetMaxActions(),
															   CharData->GetActionPointProgress(), CharData->SkillsUsedThisTurn); // Q-170j
		}
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
	StatusPanelText = FText::Join(FText::FromString(TEXT("\n")), PanelLines);
	PushToWidgets();
}

void UBattleHUDWidget::RefreshTurnStrip()
{
	const TArray<FBattleTurnSlot> Strip = BattleHUDView::ReadTurnStrip(this, CurrentTurnNumber, BattleHUDView::UPCOMING_TURNS_SHOWN);
	const AActor *Acting = Strip.IsEmpty() ? nullptr : Strip[0].Actor.Get();

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
	for (FBattleHUDCard &Card : Cards)
	{
		Card.bActing = Acting && Card.Actor.Get() == Acting;
	}
	PushToWidgets();
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
}

void UBattleHUDWidget::SetStatusPanelOpen(bool bOpen)
{
	bStatusPanelOpen = bOpen;
	if (bOpen)
	{
		RefreshStatuses(); // pushes too
		return;
	}
	PushToWidgets();
}

void UBattleHUDWidget::PushToWidgets()
{
	for (const FBattleHUDCard &Card : Cards)
	{
		const FText Name = Card.bActing ? FText::Format(LOCTEXT("ActingName", "> {0}"), Card.Name) : Card.Name;
		BattleHUDBuild::SetTextSafe(Card.NameText, Name);
		if (Card.NameText)
		{
			Card.NameText->SetColorAndOpacity(FSlateColor(Card.bActing ? BattleHUDStyle::ActingColor : BattleHUDStyle::TextColor));
		}
		if (Card.HPBar)
		{
			Card.HPBar->SetPercent(Card.HPFraction);
		}
		BattleHUDBuild::SetTextSafe(Card.HPText, Card.HPLine);
		BattleHUDBuild::SetTextSafe(Card.EPText, Card.EPLine);
		if (Card.EPText)
		{
			Card.EPText->SetColorAndOpacity(FSlateColor(Card.EPColour));
		}
		BattleHUDBuild::SetTextSafe(Card.ActionsText, Card.ActionsLine);
		BattleHUDBuild::SetTextSafe(Card.StatusText, Card.StatusLine);
		if (Card.Frame)
		{
			Card.Frame->SetRenderOpacity(Card.bAlive ? 1.0f : BattleHUDStyle::DownedCardOpacity);
		}
		if (Card.BuildupBar)
		{
			Card.BuildupBar->SetPercent(Card.BuildupFraction);
			Card.BuildupBar->SetFillColorAndOpacity(Card.BuildupColour);
		}
		for (int32 Index = 0; Index < Card.CrackMarks.Num(); ++Index)
		{
			if (UWidget *Mark = Card.CrackMarks[Index])
			{
				Mark->SetVisibility(Index < Card.CracksShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			}
		}
		BattleHUDBuild::SetTextSafe(Card.BuildupText, Card.BuildupLine);
		BattleHUDBuild::SetTextSafe(Card.GearText, Card.GearLine);
		if (Card.GearText)
		{
			Card.GearText->SetVisibility(Card.GearLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
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
		Text->SetVisibility(bUsed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	BattleHUDBuild::SetTextSafe(StatusPanelBody, StatusPanelText);
	if (StatusPanel)
	{
		StatusPanel->SetVisibility(bStatusPanelOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
	if (bShowActions)
	{
		RefreshPools(); // B-170e: Max Actions can move with buffs that don't change the bank
	}
}

void UBattleHUDWidget::HandleTurnStateChanged()
{
	if (const UTurnStateComponent *TurnState = BoundTurnState.Get())
	{
		CurrentTurnNumber = FMath::Max(1, TurnState->GetTurnNumber());
	}
	RefreshTurnStrip();
	RefreshStatuses();
	if (bShowActions)
	{
		RefreshPools(); // B-170e, as HandleTurnStarted
	}
}

void UBattleHUDWidget::HandleStatusButtonClicked()
{
	SetStatusPanelOpen(!bStatusPanelOpen);
}

void UBattleHUDWidget::HandleHitShown(AActor *Target, const FCombatHitShown &Hit)
{
	ShowDamageNumber(Target, Hit);
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
	Number.Style = bNegated ? DamageNumberView::MakeNegatedStyle(Hit) : DamageNumberView::MakeStyle(Hit);
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
	FString Out = FString::Printf(TEXT("[BattleHUD] bound=%s cards=%d turn=%d statusPanel=%s built=%s actionPips=%s"), bBound ? TEXT("yes") : TEXT("no"),
								  Cards.Num(), CurrentTurnNumber, bStatusPanelOpen ? TEXT("open") : TEXT("closed"),
								  WidgetTree && WidgetTree->RootWidget ? TEXT("yes") : TEXT("no"), bShowActions ? TEXT("on") : TEXT("off"));
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
		}
		if (Card.bParty && bShowActions)
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
