// JournalScreenWidget.cpp

#include "UI/Menus/JournalScreenWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "Story/StoryLinesData.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRJournalScreenWidget"

namespace JournalScreenStyle
{
	// This screen's own layout (screen_mockups/menus/journal, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float HeaderGap = 24.0f;
	constexpr float ListWidth = 768.0f;
	constexpr float RowPadding = 15.0f;
	constexpr float KindStripWidth = 6.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float StepMarkWidth = 36.0f;
	constexpr float ReaderScrollStep = 120.0f;
	// The quest kinds' colours (UIScreens #6: Main gold, Side grey, Secret violet). TODO: UI_Theme.md's shades.
	inline const FLinearColor MainQuestColor = FLinearColor::FromSRGBColor(FColor(232, 200, 96));
	inline const FLinearColor SecretQuestColor = FLinearColor::FromSRGBColor(FColor(168, 77, 245));

	const FLinearColor &GetKindColor(EQuestKind Kind)
	{
		switch (Kind)
		{
		case EQuestKind::Main:
			return MainQuestColor;
		case EQuestKind::Hidden:
			return SecretQuestColor;
		default:
			return DimColor;
		}
	}

	/** The reader's text scrolls on the right stick and Page Up / Down (the list has the left stick). */
	bool IsReaderUpKey(const FKey &Key) { return Key == EKeys::PageUp || Key == EKeys::Gamepad_RightStick_Up; }
	bool IsReaderDownKey(const FKey &Key) { return Key == EKeys::PageDown || Key == EKeys::Gamepad_RightStick_Down; }
}

UJournalScreenWidget::UJournalScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UJournalScreenWidget *UJournalScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Journal] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UJournalScreenWidget>(Root->PushToMenu(UJournalScreenWidget::StaticClass()));
}

void UJournalScreenWidget::SetStory(const TArray<UStoryLinesData *> &InStory)
{
	Story = InStory;
	bStoryGiven = true;
}

TSharedRef<SWidget> UJournalScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UJournalScreenWidget::BuildTree()
{
	using namespace JournalScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	UTextBlock *Title = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	Title->SetText(LOCTEXT("Title", "Journal"));
	AddToColumn(Main, Title);

	UHorizontalBox *TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "LB")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < WoRJournalScreen::PAGE_COUNT; ++Index)
	{
		UTextBlock *Label = nullptr;
		PageTabs.Add(MakeTab(WidgetTree, Label));
		PageTabLabels.Add(Label);
		Label->SetText(WoRJournalScreen::GetPageName(static_cast<EJournalPage>(Index)));
		AddToRow(TabRow, PageTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabNext", "RB")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, TabRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Left: the sub-tabs, the list, the line under it.
	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(FilterRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "LT")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	FilterTabBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(FilterRow, FilterTabBox);
	AddToRow(FilterRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "RT")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(ListColumn, FilterRow, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	RowScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(ListColumn, RowScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListNote = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	AddToColumn(ListColumn, ListNote, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);

	// Right: the reader for the focused row.
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	ReaderTitle = MakeWrappedText(WidgetTree, NameFontSize, TextColor, true);
	ReaderKicker = MakeWrappedText(WidgetTree, LineFontSize, DimColor, true);
	StepBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ReaderScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ReaderBody = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	ReaderScroll->AddChild(ReaderBody);
	ReaderNote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, ReaderTitle);
	AddToColumn(PanelColumn, ReaderKicker, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, StepBox, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, ReaderScroll, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToColumn(PanelColumn, ReaderNote, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, Panel, FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	HintText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Main, HintText)->SetHorizontalAlignment(HAlign_Right);
}

UJournalScreenWidget::FRowWidgets UJournalScreenWidget::BuildRow()
{
	using namespace JournalScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding));
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);

	UBorder *KindStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AddToRow(Line, MakeSized(WidgetTree, KindStripWidth, 0.0f, KindStrip), FMargin(0.0f, 0.0f, RowPadding, 0.0f), VAlign_Fill);
	UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Title = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *Subtitle = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Words, Title);
	AddToColumn(Words, Subtitle);
	AddToRow(Line, Words)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Tag = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	AddToRow(Line, Tag, FMargin(RowPadding, 0.0f, 0.0f, 0.0f), VAlign_Top);

	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(RowScroll->AddChild(Frame)))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, LineGap));
	}
	Widgets.Frame = Frame;
	Widgets.KindStrip = KindStrip;
	Widgets.Title = Title;
	Widgets.Subtitle = Subtitle;
	Widgets.Tag = Tag;
	return Widgets;
}

UJournalScreenWidget::FStepWidgets UJournalScreenWidget::BuildStep()
{
	using namespace JournalScreenStyle;

	FStepWidgets Widgets;
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Mark = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	UTextBlock *Text = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToRow(Line, MakeSized(WidgetTree, StepMarkWidth, 0.0f, Mark), FMargin(0.0f), VAlign_Top);
	AddToRow(Line, Text, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToColumn(StepBox, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	Widgets.Line = Line;
	Widgets.Mark = Mark;
	Widgets.Text = Text;
	return Widgets;
}

void UJournalScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// The stack may hand back a pooled instance: every open starts on Quests with every secret quest hidden.
	Page = EJournalPage::Quests;
	Filter = 0;
	SelectedRow = 0;
	State = FJournalState();
	if (!bStoryGiven)
	{
		Story = StoryUnlocks::GatherStoryData();
	}
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[Journal] Opened"));
}

void UJournalScreenWidget::NativeOnDeactivated()
{
	State = FJournalState(); // Q-397b: a shown secret quest is hidden again once the journal closes
	Super::NativeOnDeactivated();
}

UWidget *UJournalScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UJournalScreenWidget *>(this);
}

void UJournalScreenWidget::Refresh(bool bScrollIntoView)
{
	using namespace JournalScreenStyle;

	View = WoRJournalScreen::BuildView(GetGameInstance(), Page, Filter, State, ObjectPtrDecay(Story));
	Filter = FMath::Clamp(Filter, 0, FMath::Max(View.Filters.Num() - 1, 0));
	SelectedRow = View.Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, View.Rows.Num() - 1) : 0;
	if (!RowScroll)
	{
		return; // headless: no tree
	}

	for (int32 Index = 0; Index < PageTabs.Num(); ++Index)
	{
		const bool bShownPage = Index == static_cast<int32>(Page);
		PageTabs[Index]->SetBrushColor(bShownPage ? FocusColor : PlateColor);
		PageTabLabels[Index]->SetColorAndOpacity(FSlateColor(bShownPage ? TextColor : DimColor));
	}

	// Sub-tabs and rows are made as a page first needs them and refilled after (never rebuilt).
	while (FilterTabs.Num() < View.Filters.Num())
	{
		UTextBlock *Label = nullptr;
		FilterTabs.Add(MakeTab(WidgetTree, Label));
		FilterTabLabels.Add(Label);
		AddToRow(FilterTabBox, FilterTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	SetShown(FilterRow, View.Filters.Num() > 0);
	for (int32 Index = 0; Index < FilterTabs.Num(); ++Index)
	{
		const bool bShown = View.Filters.IsValidIndex(Index);
		SetShown(FilterTabs[Index], bShown);
		if (bShown)
		{
			FilterTabLabels[Index]->SetText(View.Filters[Index]);
			FilterTabs[Index]->SetBrushColor(Index == Filter ? FocusColor : PlateColor);
			FilterTabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == Filter ? TextColor : DimColor));
		}
	}

	while (RowWidgets.Num() < View.Rows.Num())
	{
		RowWidgets.Add(BuildRow());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = RowWidgets[Index];
		UBorder *Frame = Widgets.Frame.Get();
		const bool bShown = View.Rows.IsValidIndex(Index);
		SetShown(Frame, bShown);
		if (!bShown || !Frame)
		{
			continue;
		}
		const FJournalRow &Row = View.Rows[Index];
		Frame->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		Widgets.Title->SetText(Row.Title);
		Widgets.Title->SetColorAndOpacity(FSlateColor(Row.bDone ? OffColor : TextColor));
		Widgets.Subtitle->SetText(Row.Subtitle);
		SetShown(Widgets.Subtitle.Get(), !Row.Subtitle.IsEmpty());
		Widgets.Tag->SetText(Row.Tag);
		Widgets.Tag->SetColorAndOpacity(FSlateColor(Row.bQuest ? GetKindColor(Row.Kind) : CyanColor));
		// Collapsing the strip's sized box keeps a row without a kind flush left.
		SetShown(Widgets.KindStrip->GetParent(), Row.bQuest);
		Widgets.KindStrip->SetBrushColor(GetKindColor(Row.Kind));
	}
	ListNote->SetText(View.ListNote);
	SetShown(ListNote, !View.ListNote.IsEmpty());

	RefreshReader();
	HintText->SetText(GetHints());

	if (bScrollIntoView && RowWidgets.IsValidIndex(SelectedRow))
	{
		if (UWidget *RowFrame = RowWidgets[SelectedRow].Frame.Get())
		{
			RowScroll->ScrollWidgetIntoView(RowFrame, /*AnimateScroll=*/false);
		}
	}
}

void UJournalScreenWidget::RefreshReader()
{
	using namespace JournalScreenStyle;

	const FJournalRow *Row = GetFocusedRow();
	const FJournalRow Nothing;
	const FJournalRow &Shown = Row ? *Row : Nothing;

	ReaderTitle->SetText(Shown.Title);
	ReaderKicker->SetText(Shown.Kicker);
	ReaderKicker->SetColorAndOpacity(FSlateColor(Shown.bQuest ? GetKindColor(Shown.Kind) : DimColor));
	SetShown(ReaderKicker, !Shown.Kicker.IsEmpty());

	while (StepWidgets.Num() < Shown.Steps.Num())
	{
		StepWidgets.Add(BuildStep());
	}
	for (int32 Index = 0; Index < StepWidgets.Num(); ++Index)
	{
		const FStepWidgets &Widgets = StepWidgets[Index];
		const bool bShown = Shown.Steps.IsValidIndex(Index);
		SetShown(Widgets.Line.Get(), bShown);
		if (!bShown || !Widgets.Text.IsValid())
		{
			continue;
		}
		// A done step is dim; the one in progress is lit. TODO: the tick and arrow icons come with the art pass.
		const FJournalStep &Step = Shown.Steps[Index];
		Widgets.Mark->SetText(Step.bDone ? LOCTEXT("StepDoneMark", "·") : LOCTEXT("StepCurrentMark", "›"));
		Widgets.Mark->SetColorAndOpacity(FSlateColor(Step.bDone ? GoodColor : CyanColor));
		Widgets.Text->SetText(Step.Text);
		Widgets.Text->SetColorAndOpacity(FSlateColor(Step.bDone ? OffColor : TextColor));
	}
	SetShown(StepBox, Shown.Steps.Num() > 0);

	ReaderBody->SetText(Shown.Body);
	// Visible, not SelfHitTestInvisible: the scroll box itself has to take the mouse wheel.
	ReaderScroll->SetVisibility(Shown.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ReaderScroll->ScrollToStart();
	ReaderNote->SetText(Shown.Note);
	SetShown(ReaderNote, !Shown.Note.IsEmpty());
}

FText UJournalScreenWidget::GetHints() const
{
	TArray<FText> Parts;
	Parts.Add(LOCTEXT("HintPage", "LB / RB  Page"));
	if (View.Filters.Num() > 1)
	{
		Parts.Add(LOCTEXT("HintFilter", "LT / RT  Filter"));
	}
	if (const FJournalRow *Row = GetFocusedRow())
	{
		if (!Row->SelectVerb.IsEmpty())
		{
			Parts.Add(FText::Format(LOCTEXT("HintSelect", "A  {0}"), Row->SelectVerb));
		}
		if (Row->IsSecret())
		{
			Parts.Add(FText::Format(LOCTEXT("HintToggle", "X  {0}"), Row->ToggleVerb)); // D-198: Show or Hide
		}
	}
	if (Page == EJournalPage::Quests)
	{
		Parts.Add(LOCTEXT("HintHideAll", "Y  Hide all"));
	}
	Parts.Add(LOCTEXT("HintBack", "B / Esc  Back"));
	return FText::Join(FText::AsCultureInvariant(TEXT("        ")), Parts);
}

void UJournalScreenWidget::MovePage(int32 Delta)
{
	const int32 Count = WoRJournalScreen::PAGE_COUNT;
	Page = static_cast<EJournalPage>(((static_cast<int32>(Page) + Delta) % Count + Count) % Count);
	Filter = 0;
	SelectedRow = 0;
	State.TalkId = NAME_None; // a talk left half way starts again
	State.TalkLines = 0;
	Refresh();
}

void UJournalScreenWidget::MoveFilter(int32 Delta)
{
	const int32 Count = View.Filters.Num();
	if (Count > 1)
	{
		Filter = ((Filter + Delta) % Count + Count) % Count;
		SelectedRow = 0;
		Refresh();
	}
}

void UJournalScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = View.Rows.Num();
	if (Count > 0)
	{
		SelectRow(((SelectedRow + Delta) % Count + Count) % Count);
	}
}

void UJournalScreenWidget::SelectRow(int32 Index)
{
	if (View.Rows.IsValidIndex(Index) && Index != SelectedRow)
	{
		SelectedRow = Index;
		Refresh();
	}
}

void UJournalScreenWidget::PressSelect()
{
	const FJournalRow *Row = GetFocusedRow();
	if (Row && WoRJournalScreen::PlayNextLine(GetGameInstance(), *Row, State, ObjectPtrDecay(Story)))
	{
		Refresh();
		if (ReaderScroll)
		{
			ReaderScroll->ScrollToEnd(); // the new line is the last one
		}
	}
}

void UJournalScreenWidget::PressToggle()
{
	const FJournalRow *Row = GetFocusedRow();
	if (Row && WoRJournalScreen::ToggleSecret(*Row, State))
	{
		Refresh();
	}
}

void UJournalScreenWidget::PressHideAll()
{
	if (Page == EJournalPage::Quests && WoRJournalScreen::HideAllSecrets(State))
	{
		Refresh();
	}
}

FReply UJournalScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace JournalScreenStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsReaderUpKey(Key) || IsReaderDownKey(Key))
	{
		if (ReaderScroll)
		{
			const float Step = IsReaderDownKey(Key) ? ReaderScrollStep : -ReaderScrollStep;
			ReaderScroll->SetScrollOffset(FMath::Clamp(ReaderScroll->GetScrollOffset() + Step, 0.0f, ReaderScroll->GetScrollOffsetOfEnd()));
		}
		return FReply::Handled();
	}
	const bool bPageKey = IsTabPrevKey(Key) || IsTabNextKey(Key);
	const bool bFilterKey = IsSubTabPrevKey(Key) || IsSubTabNextKey(Key);
	if (bPageKey || bFilterKey || IsConfirmKey(Key) || IsToggleKey(Key) || IsSecondActionKey(Key) || IsBackKey(Key))
	{
		if (InKeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}
		if (bPageKey)
		{
			MovePage(IsTabNextKey(Key) ? 1 : -1);
		}
		else if (bFilterKey)
		{
			MoveFilter(IsSubTabNextKey(Key) ? 1 : -1);
		}
		else if (IsConfirmKey(Key))
		{
			PressSelect();
		}
		else if (IsToggleKey(Key))
		{
			PressToggle();
		}
		else if (IsSecondActionKey(Key))
		{
			PressHideAll();
		}
		else
		{
			// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
			HandleBackAction();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UJournalScreenWidget::FindUnder(const TArray<TObjectPtr<UBorder>> &Boxes, const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < Boxes.Num(); ++Index)
	{
		if (Boxes[Index] && Boxes[Index]->IsVisible() && Boxes[Index]->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UJournalScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	// A row scrolled out of view keeps its geometry past the list's edge: only the list's own area counts.
	if (!RowScroll || !RowScroll->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < RowWidgets.Num() && Index < View.Rows.Num(); ++Index)
	{
		const UBorder *Frame = RowWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UJournalScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	const int32 Row = InMouseEvent.GetCursorDelta().IsZero() ? INDEX_NONE : FindRowUnder(InMouseEvent);
	if (View.Rows.IsValidIndex(Row) && Row != SelectedRow)
	{
		SelectedRow = Row;
		Refresh(/*bScrollIntoView=*/false); // the list mustn't move under the cursor
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UJournalScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	const int32 Tab = FindUnder(PageTabs, InMouseEvent);
	if (Tab != INDEX_NONE)
	{
		MovePage(Tab - static_cast<int32>(Page));
		return FReply::Handled();
	}
	const int32 FilterTab = FindUnder(FilterTabs, InMouseEvent);
	if (View.Filters.IsValidIndex(FilterTab))
	{
		MoveFilter(FilterTab - Filter);
		return FReply::Handled();
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (View.Rows.IsValidIndex(Row))
	{
		if (Row != SelectedRow)
		{
			SelectedRow = Row;
			Refresh(/*bScrollIntoView=*/false);
		}
		PressSelect();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GJournalScreenOpenCommand(
	TEXT("wor.UI.Journal"),
	TEXT("B-463: open the journal screen, as the game menu's Journal page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UJournalScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Journal] wor.UI.Journal: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
