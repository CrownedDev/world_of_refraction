// TitleScreenWidget.cpp

#include "UI/Menus/TitleScreenWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/WoRHubController.h"
#include "Save/SaveSubsystem.h"
#include "UI/Menus/CreditsScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/NewGameScreenWidget.h"
#include "UI/Menus/SettingsScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRTitleScreenWidget"

namespace TitleScreenStyle
{
	// This screen's own layout (screen_mockups/front/title, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr int32 GameTitleFontSize = 60;
	constexpr int32 GameSubtitleFontSize = 24;
	/** The subtitle's spread-out letters (1/1000 em). */
	constexpr int32 SubtitleLetterSpacing = 600;
	/** The mockup's left margin (72 px at 720p). */
	constexpr float ScreenPaddingX = 108.0f;
	constexpr float TitleGap = 36.0f;
	constexpr float RowsTopGap = 96.0f;
	constexpr float ListWidth = 612.0f;
	constexpr float RowHeight = 72.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowPaddingX = 24.0f;
	constexpr float PanelWidth = 900.0f;
	constexpr float PanelPadding = 36.0f;
	constexpr float HoldBarHeight = 12.0f;
	constexpr int32 MaxRows = 6;
	/** UIScreens #9: New Game over a save needs A held this long (Destiny 2's dismantle, Elden Ring's delete). */
	constexpr float HOLD_TO_START_OVER_SECONDS = 2.0f;
	constexpr float MaxHoldStepSeconds = 0.1f;
	inline const FLinearColor DimLayerColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
	/** "Your save was reset": the mockup's amber title. */
	inline const FLinearColor NoticeTitleColor = WarnColor;
}

namespace
{
	/** Set when the player leaves the title (Continue, or New Game with no save): it doesn't open again this launch. */
	bool GTitleLeftThisLaunch = false;
	/** B-469: the hold deleted the save this launch (the title says so until the new game begins). */
	bool GTitleSaveDeletedThisLaunch = false;
	/** "Your save was reset" is said once a launch. */
	bool GSaveResetNoticeShown = false;
}

UTitleScreenWidget::UTitleScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UTitleScreenWidget *UTitleScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UTitleScreenWidget>(Root->PushToMenu(UTitleScreenWidget::StaticClass()));
}

void UTitleScreenWidget::OpenAtLaunch(APlayerController *PC)
{
	if (!GTitleLeftThisLaunch && PC && PC->IsLocalController())
	{
		Open(PC);
	}
}

void UTitleScreenWidget::MarkLeftThisLaunch()
{
	GTitleLeftThisLaunch = true;
}

TSharedRef<SWidget> UTitleScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UTitleScreenWidget::BuildTree()
{
	using namespace TitleScreenStyle;

	UOverlay *Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(FrontBackdropColor);
	Screen->SetPadding(FMargin(ScreenPaddingX, ScreenPadding));
	UOverlaySlot *ScreenSlot = Root->AddChildToOverlay(Screen);
	ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
	ScreenSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	UTextBlock *GameTitle = MakeText(WidgetTree, GameTitleFontSize, TextColor);
	GameTitle->SetText(LOCTEXT("GameTitle", "WORLD OF REFRACTION"));
	UTextBlock *GameSubtitle = MakeText(WidgetTree, GameSubtitleFontSize, DimColor);
	FSlateFontInfo SubtitleFont = GameSubtitle->GetFont();
	SubtitleFont.LetterSpacing = SubtitleLetterSpacing;
	GameSubtitle->SetFont(SubtitleFont);
	GameSubtitle->SetText(LOCTEXT("GameSubtitle", "TALES OF THE VOID"));
	UVerticalBox *TitleBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(TitleBlock, GameTitle);
	AddToColumn(TitleBlock, GameSubtitle, FMargin(0.0f, TitleGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
	AddToColumn(Main, TitleBlock)->SetHorizontalAlignment(HAlign_Left);

	UVerticalBox *List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (int32 Index = 0; Index < MaxRows; ++Index)
	{
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetPadding(FMargin(RowPaddingX, 0.0f));
		Row->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(Columns);
		UTextBlock *Label = MakeText(WidgetTree, NameFontSize, TextColor, true);
		UTextBlock *Line = MakeText(WidgetTree, SmallFontSize, TextColor);
		AddToRow(Columns, Label);
		UHorizontalBoxSlot *LineSlot = AddToRow(Columns, Line, FMargin(SectionGap, 0.0f, 0.0f, 0.0f));
		LineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LineSlot->SetHorizontalAlignment(HAlign_Right);
		RowBoxes.Add(Row);
		RowLabels.Add(Label);
		RowLines.Add(Line);
		AddToColumn(List, MakeSized(WidgetTree, ListWidth, RowHeight, Row), FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	AddToColumn(Main, List, FMargin(0.0f, RowsTopGap, 0.0f, 0.0f));
	StatusText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(Main, MakeSized(WidgetTree, ListWidth, 0.0f, StatusText), FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UVerticalBox *Spacer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Main, Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HintsText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Main, HintsText)->SetHorizontalAlignment(HAlign_Right);

	// Over everything: the dimmed screen and one centred panel (the reset notice, or New Game's confirm).
	UBorder *Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelLayer"));
	Dim->SetBrushColor(DimLayerColor);
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot *DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);
	PanelLayer = Dim;

	UBorder *PanelBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PanelBox->SetBrushColor(PanelColor);
	PanelBox->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelBox->SetContent(PanelColumn);
	PanelTitle = MakeText(WidgetTree, NameFontSize, TextColor, true);
	PanelBody = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(PanelColumn, PanelTitle);
	AddToColumn(PanelColumn, PanelBody, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	UVerticalBox *Versions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	auto AddVersionLine = [this, Versions](const FText &Label, TObjectPtr<UTextBlock> &OutValue)
	{
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, LineFontSize, DimColor);
		LabelText->SetText(Label);
		OutValue = MakeText(WidgetTree, LineFontSize, TextColor, true);
		AddToRow(Line, LabelText);
		UHorizontalBoxSlot *ValueSlot = AddToRow(Line, OutValue);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		AddToColumn(Versions, Line, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	};
	AddVersionLine(LOCTEXT("OldSave", "Old save"), OldVersionText);
	AddVersionLine(LOCTEXT("BuildNeeds", "This build needs"), NeededVersionText);
	AddToColumn(PanelColumn, Versions, FMargin(0.0f, SectionGap * 2.0f, 0.0f, 0.0f));
	VersionLines = Versions;

	UVerticalBox *Hold = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *CantUndo = MakeText(WidgetTree, LineFontSize, WarnColor, true);
	CantUndo->SetText(LOCTEXT("CantUndo", "This can't be undone."));
	UTextBlock *HoldHint = MakeText(WidgetTree, SmallFontSize, DimColor);
	HoldHint->SetText(FText::Format(LOCTEXT("HoldHint", "Hold A for {0} seconds to confirm."), FMath::RoundToInt32(HOLD_TO_START_OVER_SECONDS)));
	HoldBar = MakeBar(WidgetTree, WarnColor); // as the game menu's quit hold
	AddToColumn(Hold, CantUndo);
	AddToColumn(Hold, HoldHint, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(Hold, MakeSized(WidgetTree, PanelWidth - PanelPadding * 2.0f, HoldBarHeight, HoldBar), FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, Hold, FMargin(0.0f, SectionGap * 2.0f, 0.0f, 0.0f));
	HoldLines = Hold;

	Dim->SetContent(MakeSized(WidgetTree, PanelWidth, 0.0f, PanelBox));
}

void UTitleScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	SelectedRow = 0;
	Status = GTitleSaveDeletedThisLaunch ? LOCTEXT("SaveDeleted", "Your save is deleted. Choose New Game to begin.") : FText::GetEmpty();
	StopHold();
	const USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	const bool bShowReset = Saves && Saves->WasWiped() && !GSaveResetNoticeShown;
	GSaveResetNoticeShown |= bShowReset;
	Panel = bShowReset ? ETitlePanel::SaveReset : ETitlePanel::None;
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[Title] Opened%s"), bShowReset ? TEXT(" (the save was reset: notice shown)") : TEXT(""));
}

void UTitleScreenWidget::NativeOnDeactivated()
{
	StopHold();
	Super::NativeOnDeactivated();
}

UWidget *UTitleScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UTitleScreenWidget *>(this);
}

void UTitleScreenWidget::Refresh()
{
	using namespace TitleScreenStyle;

	Save = WoRTitleScreen::SummarizeSave(GetGameInstance());
	Rows = WoRTitleScreen::BuildRows(Save);
	SelectedRow = FMath::Clamp(SelectedRow, 0, Rows.Num() - 1);

	for (int32 Index = 0; Index < RowBoxes.Num(); ++Index)
	{
		const bool bShown = Rows.IsValidIndex(Index);
		SetShown(RowBoxes[Index]->GetParent(), bShown);
		if (!bShown)
		{
			continue;
		}
		const FTitleScreenRow &Row = Rows[Index];
		RowBoxes[Index]->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		RowLabels[Index]->SetText(Row.Label);
		RowLabels[Index]->SetColorAndOpacity(FSlateColor(Row.CanUse() ? TextColor : OffColor));
		RowLines[Index]->SetText(Row.Line);
	}
	if (!StatusText)
	{
		return; // headless: no tree
	}
	StatusText->SetText(Status);
	SetShown(StatusText, !Status.IsEmpty());

	SetShown(PanelLayer, Panel != ETitlePanel::None);
	SetShown(VersionLines, Panel == ETitlePanel::SaveReset);
	SetShown(HoldLines, Panel == ETitlePanel::NewGameConfirm);
	if (Panel == ETitlePanel::SaveReset)
	{
		const USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
		PanelTitle->SetText(LOCTEXT("ResetTitle", "Your save was reset"));
		PanelTitle->SetColorAndOpacity(FSlateColor(NoticeTitleColor));
		PanelBody->SetText(LOCTEXT("ResetBody", "This build can't read saves from the older version, so the game starts fresh. This can happen until the first public release; after that, saves are always carried forward."));
		OldVersionText->SetText(WoRTitleScreen::GetOldVersionText(Saves ? Saves->GetWipedFromVersion() : 0));
		NeededVersionText->SetText(WoRTitleScreen::GetNeededVersionText());
		HintsText->SetText(LOCTEXT("ResetHints", "A  Start fresh"));
	}
	else if (Panel == ETitlePanel::NewGameConfirm)
	{
		PanelTitle->SetText(LOCTEXT("NewGameTitle", "Start a new game?"));
		PanelTitle->SetColorAndOpacity(FSlateColor(TextColor));
		PanelBody->SetText(WoRTitleScreen::GetNewGameWarning(Save));
		HoldBar->SetPercent(HeldSeconds / HOLD_TO_START_OVER_SECONDS);
		HintsText->SetText(LOCTEXT("ConfirmHints", "A  Hold: start over        B  Cancel"));
	}
	else
	{
		HintsText->SetText(LOCTEXT("Hints", "Up / Down  Choose        A  Select"));
	}
}

void UTitleScreenWidget::ShowPanel(ETitlePanel InPanel)
{
	StopHold();
	Panel = InPanel;
	Refresh();
}

void UTitleScreenWidget::MoveSelection(int32 Delta)
{
	if (Panel == ETitlePanel::None && Rows.Num() > 0)
	{
		SelectRow(((SelectedRow + Delta) % Rows.Num() + Rows.Num()) % Rows.Num());
	}
}

void UTitleScreenWidget::SelectRow(int32 Index)
{
	if (Rows.IsValidIndex(Index) && Index != SelectedRow)
	{
		SelectedRow = Index;
		Status = FText::GetEmpty();
		Refresh();
	}
}

void UTitleScreenWidget::Confirm()
{
	if (Panel == ETitlePanel::SaveReset)
	{
		ShowPanel(ETitlePanel::None);
		return;
	}
	if (Panel != ETitlePanel::None || !Rows.IsValidIndex(SelectedRow))
	{
		return; // New Game's confirm is a hold, not a press
	}
	const FTitleScreenRow &Row = Rows[SelectedRow];
	if (!Row.CanUse())
	{
		Status = Row.Refusal;
		Refresh();
		return;
	}
	switch (Row.Id)
	{
	case ETitleScreenRow::Continue:
		Leave();
		break;
	case ETitleScreenRow::NewGame:
		if (Save.bHasSave)
		{
			ShowPanel(ETitlePanel::NewGameConfirm);
		}
		else
		{
			UNewGameScreenWidget::Open(GetOwningPlayer()); // B-469: nothing to lose; Begin starts the game
		}
		break;
	case ETitleScreenRow::Settings:
		USettingsScreenWidget::Open(GetOwningPlayer());
		break;
	case ETitleScreenRow::Credits:
		UCreditsScreenWidget::Open(GetOwningPlayer());
		break;
	case ETitleScreenRow::Quit:
		UE_LOG(LogWoRUI, Log, TEXT("[Title] Quit"));
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	default:
		break;
	}
}

void UTitleScreenWidget::Leave()
{
	GTitleLeftThisLaunch = true;
	UE_LOG(LogWoRUI, Log, TEXT("[Title] Left: %s"), Save.bHasSave ? TEXT("continuing the save") : TEXT("a new game"));
	DeactivateWidget();
}

void UTitleScreenWidget::StartHold()
{
	if (Panel != ETitlePanel::NewGameConfirm || HoldTicker.IsValid())
	{
		return;
	}
	HeldSeconds = 0.0f;
	// A ticker, not the world's timers: the hold counts even while the game under the title is paused.
	HoldTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		UpdateHold(DeltaSeconds);
		return HoldTicker.IsValid();
	}));
}

void UTitleScreenWidget::StopHold()
{
	if (HoldTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HoldTicker);
		HoldTicker.Reset();
	}
	HeldSeconds = 0.0f;
	if (HoldBar)
	{
		HoldBar->SetPercent(0.0f);
	}
}

void UTitleScreenWidget::UpdateHold(float DeltaSeconds)
{
	using namespace TitleScreenStyle;
	if (Panel != ETitlePanel::NewGameConfirm)
	{
		return;
	}
	HeldSeconds += FMath::Min(DeltaSeconds, MaxHoldStepSeconds); // a hitch can't finish the hold by itself
	if (HoldBar)
	{
		HoldBar->SetPercent(FMath::Min(HeldSeconds / HOLD_TO_START_OVER_SECONDS, 1.0f));
	}
	if (HeldSeconds >= HOLD_TO_START_OVER_SECONDS)
	{
		StartOver();
	}
}

void UTitleScreenWidget::StartOver()
{
	USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	if (Saves && Saves->IsBoundToPlayer())
	{
		// B-469: the new game screen's Begin resets the old game still in memory and starts the new one.
		Saves->DeleteSave();
		// Shown if the player backs out of the new game screen: the old game can't be continued now.
		GTitleSaveDeletedThisLaunch = true;
		Status = LOCTEXT("SaveDeleted", "Your save is deleted. Choose New Game to begin.");
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] New Game held: the save is deleted; the new game screen opens"));
	}
	SelectedRow = 0;
	ShowPanel(ETitlePanel::None);
	UNewGameScreenWidget::Open(GetOwningPlayer());
}

FReply UTitleScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			if (Panel == ETitlePanel::NewGameConfirm)
			{
				StartHold();
			}
			else
			{
				Confirm();
			}
		}
		return FReply::Handled();
	}
	if (IsBackKey(Key))
	{
		// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
		if (!InKeyEvent.IsRepeat())
		{
			HandleBackAction();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UTitleScreenWidget::NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (WoRMenuStyle::IsConfirmKey(InKeyEvent.GetKey()) && HoldTicker.IsValid())
	{
		StopHold(); // let go before the end: nothing happens
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UTitleScreenWidget::NativeOnFocusLost(const FFocusEvent &InFocusEvent)
{
	StopHold(); // no key-up arrives once the keys go elsewhere: the hold must not finish by itself
	Super::NativeOnFocusLost(InFocusEvent);
}

bool UTitleScreenWidget::NativeOnHandleBackAction()
{
	// The title never closes on Back; Back only takes a panel away.
	if (Panel != ETitlePanel::None)
	{
		ShowPanel(ETitlePanel::None);
	}
	return true;
}

int32 UTitleScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < RowBoxes.Num(); ++Index)
	{
		if (Rows.IsValidIndex(Index) && RowBoxes[Index]->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UTitleScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (Panel == ETitlePanel::None && !InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTitleScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (Panel == ETitlePanel::NewGameConfirm)
	{
		StartHold(); // held anywhere, as A is
		return FReply::Handled();
	}
	if (Panel == ETitlePanel::SaveReset)
	{
		Confirm();
		return FReply::Handled();
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (Row != INDEX_NONE)
	{
		SelectRow(Row);
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UTitleScreenWidget::NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HoldTicker.IsValid())
	{
		StopHold();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTitleOpenCommand(
	TEXT("wor.UI.Title"),
	TEXT("B-467: open the title screen (outside the editor it opens by itself once a launch)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UTitleScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Title] wor.UI.Title: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
