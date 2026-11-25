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
	// B-630 (mockup menus/journal/v2, 1280x720 shown at 1080p): the loadout screen's three columns (v5); the shared look
	// is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float HeaderGap = 24.0f;
	constexpr float LeftWidth = 600.0f;
	constexpr float MiddleWidth = 618.0f;
	constexpr float RightWidth = 504.0f;
	constexpr float PanelGap = 27.0f;
	constexpr float PanelPadding = 24.0f;
	constexpr float RowPadding = 15.0f;
	constexpr float KindStripWidth = 6.0f;
	constexpr float StepMarkWidth = 36.0f;
	constexpr float ReaderScrollStep = 120.0f;
	/** The kind plate's lit word: a line under it (as the loadout screen's selectors). */
	constexpr float KindUnderlineHeight = 3.0f;
	/** Between two hints on the bottom line (as the settings screen's). */
	const TCHAR *const HintGap = TEXT("        ");
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

UBorder *UJournalScreenWidget::MakePanel(UVerticalBox *&OutColumn) const
{
	using namespace JournalScreenStyle;
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	OutColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(OutColumn);
	return Panel;
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
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "{TabPrev}")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < WoRJournalScreen::PAGE_COUNT; ++Index)
	{
		UTextBlock *Label = nullptr;
		PageTabs.Add(MakeTab(WidgetTree, Label));
		PageTabLabels.Add(Label);
		Label->SetText(WoRJournalScreen::GetPageName(static_cast<EJournalPage>(Index)));
		AddToRow(TabRow, PageTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabNext", "{TabNext}")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, TabRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox *Left = nullptr;
	UVerticalBox *Middle = nullptr;
	UVerticalBox *Right = nullptr;
	AddToRow(Body, MakeSized(WidgetTree, LeftWidth, 0.0f, MakePanel(Left)), FMargin(0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, MiddleWidth, 0.0f, MakePanel(Middle)), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, RightWidth, 0.0f, MakePanel(Right)), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// Left: the page's name and count, the ‹ kind › plate, the list, the line under it.
	UHorizontalBox *ListHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ListTitle = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToRow(ListHead, ListTitle, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CountText = MakeText(WidgetTree, SmallFontSize, DimColor, true);
	AddToRow(ListHead, CountText, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
	AddToColumn(Left, ListHead, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	FilterRow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FilterRow->SetBrushColor(PlateColor);
	FilterRow->SetPadding(FMargin(TabPaddingX, TabPaddingY));
	UHorizontalBox *Plate = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FilterRow->SetContent(Plate);
	const auto AddArrow = [this, Plate](const FText &Arrow, const FMargin &ArrowPadding)
	{
		UTextBlock *Text = MakeText(WidgetTree, RowFontSize, CyanColor, true);
		Text->SetText(Arrow);
		return AddToRow(Plate, Text, ArrowPadding, VAlign_Center);
	};
	AddArrow(LOCTEXT("KindPrev", "‹"), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	FilterTabBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	// Many kinds (a memory per character, lore per place) scroll sideways; the lit word is kept in view (Refresh).
	FilterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	FilterScroll->SetOrientation(Orient_Horizontal);
	FilterScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	FilterScroll->AddChild(FilterTabBox);
	AddToRow(Plate, FilterScroll, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddArrow(LOCTEXT("KindNext", "›"), FMargin(SectionGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Left, FilterRow, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	RowScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(Left, RowScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListNote = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	AddToColumn(Left, ListNote, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	// Middle: the focused entry, always open.
	ReaderTitle = MakeWrappedText(WidgetTree, NameFontSize, TextColor, true);
	ReaderKicker = MakeWrappedText(WidgetTree, LineFontSize, DimColor, true);
	StepBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ReaderScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ReaderBody = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	ReaderScroll->AddChild(ReaderBody);
	AddToColumn(Middle, ReaderTitle);
	AddToColumn(Middle, ReaderKicker, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(Middle, StepBox, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(Middle, ReaderScroll, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Right: Details, the facts, the buttons, the note.
	UTextBlock *DetailsHeading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	DetailsHeading->SetText(LOCTEXT("Details", "Details"));
	AddToColumn(Right, DetailsHeading);
	DetailsName = MakeText(WidgetTree, SmallFontSize, DimColor);
	DetailsName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	AddToColumn(Right, DetailsName, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	FactBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Right, FactBox, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	ButtonBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Right, ButtonBox);
	ReaderNote = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Right, ReaderNote, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	HintText = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	AddToColumn(Main, HintText)->SetHorizontalAlignment(HAlign_Right);
}

void UJournalScreenWidget::AddFilterWord()
{
	using namespace JournalScreenStyle;

	UBorder *Word = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Word->SetBrushColor(FLinearColor::Transparent);
	Word->SetPadding(FMargin(0.0f));
	UVerticalBox *Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Label = MakeText(WidgetTree, RowFontSize, DimColor, true);
	AddToColumn(Stack, Label);
	UBorder *Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Line->SetBrushColor(CyanColor);
	USizeBox *Under = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Under->SetHeightOverride(KindUnderlineHeight);
	Under->AddChild(Line);
	AddToColumn(Stack, Under)->SetHorizontalAlignment(HAlign_Fill);
	Word->SetContent(Stack);
	AddToRow(FilterTabBox, Word, FMargin(0.0f, 0.0f, SectionGap, 0.0f), VAlign_Center);
	FilterTabs.Add(Word);
	FilterTabLabels.Add(Label);
	FilterUnderlines.Add(Under);
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
	Title->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	UTextBlock *Subtitle = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Words, Title);
	AddToColumn(Words, Subtitle);
	AddToRow(Line, Words)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox *Marks = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Tag = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	Tag->SetJustification(ETextJustify::Right);
	UTextBlock *Tracked = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	Tracked->SetJustification(ETextJustify::Right);
	Tracked->SetText(LOCTEXT("RowTracked", "tracked"));
	AddToColumn(Marks, Tag)->SetHorizontalAlignment(HAlign_Right);
	AddToColumn(Marks, Tracked, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
	AddToRow(Line, Marks, FMargin(RowPadding, 0.0f, 0.0f, 0.0f), VAlign_Top);

	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(RowScroll->AddChild(Frame)))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, LineGap));
	}
	Widgets.Frame = Frame;
	Widgets.KindStrip = KindStrip;
	Widgets.Title = Title;
	Widgets.Subtitle = Subtitle;
	Widgets.Tag = Tag;
	Widgets.Tracked = Tracked;
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
	// Q-397b: a shown secret quest is hidden again once the journal closes; the rows are refilled so no text block
	// keeps its name while the screen leaves.
	State = FJournalState();
	Refresh(/*bScrollIntoView=*/false);
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
	if (View.Rows.IsValidIndex(SelectedRow))
	{
		WoRJournalScreen::MarkOpened(GetGameInstance(), View.Rows[SelectedRow]); // B-463c: in the reader now, so opened
	}
	if (!RowScroll)
	{
		ShownButtons = GetButtons();
		return; // headless: no tree
	}

	for (int32 Index = 0; Index < PageTabs.Num(); ++Index)
	{
		const bool bShownPage = Index == static_cast<int32>(Page);
		PageTabs[Index]->SetBrushColor(bShownPage ? FocusColor : PlateColor);
		PageTabLabels[Index]->SetColorAndOpacity(FSlateColor(bShownPage ? TextColor : DimColor));
	}
	ListTitle->SetText(WoRJournalScreen::GetPageName(Page));
	CountText->SetText(View.CountLine);

	// The kind words and the rows are made as a page first needs them and refilled after (never rebuilt).
	while (FilterTabs.Num() < View.Filters.Num())
	{
		AddFilterWord();
	}
	SetShown(FilterRow, View.Filters.Num() > 0);
	for (int32 Index = 0; Index < FilterTabs.Num(); ++Index)
	{
		const bool bShown = View.Filters.IsValidIndex(Index);
		const bool bLit = bShown && Index == Filter;
		SetShown(FilterTabs[Index], bShown);
		FilterTabLabels[Index]->SetText(bShown ? View.Filters[Index] : FText::GetEmpty());
		FilterTabLabels[Index]->SetColorAndOpacity(FSlateColor(bLit ? TextColor : DimColor));
		FilterUnderlines[Index]->SetVisibility(bLit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (FilterTabs.IsValidIndex(Filter))
	{
		FilterScroll->ScrollWidgetIntoView(FilterTabs[Filter], /*AnimateScroll=*/false);
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
		Widgets.Tag->SetColorAndOpacity(FSlateColor(Row.bDone ? OffColor : Row.bQuest ? GetKindColor(Row.Kind) : CyanColor));
		SetShown(Widgets.Tracked.Get(), Row.bTracked);
		// Collapsing the strip's sized box keeps a row without a kind flush left.
		SetShown(Widgets.KindStrip->GetParent(), Row.bQuest);
		Widgets.KindStrip->SetBrushColor(GetKindColor(Row.Kind));
	}
	ListNote->SetText(View.ListNote);
	// Memories and Lore count in the note too: it shows once, beside the title.
	SetShown(ListNote, !View.ListNote.IsEmpty() && !View.ListNote.EqualTo(View.CountLine));

	RefreshReader();
	RefreshDetails();
	WoRKeyHints::SetHint(HintText, GetHints());

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
		// B-630 (mockup v2): a done step is ticked and dim; the one in progress has the arrow and is lit.
		const FJournalStep &Step = Shown.Steps[Index];
		Widgets.Mark->SetText(Step.bDone ? LOCTEXT("StepDoneMark", "✓") : LOCTEXT("StepCurrentMark", "►"));
		Widgets.Mark->SetColorAndOpacity(FSlateColor(Step.bDone ? GoodColor : CyanColor));
		Widgets.Text->SetText(Step.Text);
		Widgets.Text->SetColorAndOpacity(FSlateColor(Step.bDone ? OffColor : TextColor));
	}
	SetShown(StepBox, Shown.Steps.Num() > 0);

	ReaderBody->SetText(Shown.Body);
	// Visible, not SelfHitTestInvisible: the scroll box itself has to take the mouse wheel.
	ReaderScroll->SetVisibility(Shown.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ReaderScroll->ScrollToStart();
}

TArray<UJournalScreenWidget::FJournalButton> UJournalScreenWidget::GetButtons() const
{
	TArray<FJournalButton> Buttons;
	const FJournalRow *Row = GetFocusedRow();
	if (Row && !Row->SelectVerb.IsEmpty())
	{
		Buttons.Add({EJournalButton::Select, FText::FormatNamed(LOCTEXT("ButtonSelect", "{Confirm}  {Verb}"), TEXT("Verb"), Row->SelectVerb)});
	}
	if (Row && Row->IsSecret())
	{
		Buttons.Add({EJournalButton::Toggle, FText::FormatNamed(LOCTEXT("ButtonToggle", "{Toggle}  {Verb}"), TEXT("Verb"), Row->ToggleVerb)});
	}
	if (Page == EJournalPage::Quests)
	{
		Buttons.Add({EJournalButton::HideAll, LOCTEXT("ButtonHideAll", "{Second}  Hide all secret quests"),
					 State.ShownSecrets.IsEmpty() ? LOCTEXT("NoneShown", "No secret quest is shown.") : FText::GetEmpty()});
	}
	return Buttons;
}

void UJournalScreenWidget::RefreshDetails()
{
	using namespace JournalScreenStyle;

	const FJournalRow *Row = GetFocusedRow();
	DetailsName->SetText(Row ? Row->Title : FText::GetEmpty());
	const TArray<FJournalFact> NoFacts;
	const TArray<FJournalFact> &Facts = Row ? Row->Facts : NoFacts;
	while (FactWidgets.Num() < Facts.Num())
	{
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *Label = MakeText(WidgetTree, LineFontSize, DimColor);
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, TextColor, true);
		Value->SetJustification(ETextJustify::Right);
		Value->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddToRow(Line, Label, FMargin(0.0f), VAlign_Center);
		AddToRow(Line, Value, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToColumn(FactBox, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		FactWidgets.Add({Line, Label, Value});
	}
	for (int32 Index = 0; Index < FactWidgets.Num(); ++Index)
	{
		const FFactWidgets &Widgets = FactWidgets[Index];
		const bool bShown = Facts.IsValidIndex(Index);
		SetShown(Widgets.Line.Get(), bShown);
		if (bShown && Widgets.Label.IsValid() && Widgets.Value.IsValid())
		{
			Widgets.Label->SetText(Facts[Index].Label);
			Widgets.Value->SetText(Facts[Index].Value);
			Widgets.Value->SetColorAndOpacity(FSlateColor(Facts[Index].bLit ? CyanColor : TextColor));
		}
	}

	ShownButtons = GetButtons();
	while (ButtonWidgets.Num() < ShownButtons.Num())
	{
		UBorder *Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Plate->SetBrushColor(PlateColor);
		Plate->SetPadding(FMargin(TabPaddingX, TabPaddingY));
		UVerticalBox *Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Plate->SetContent(Lines);
		UTextBlock *Verb = MakeHintText(WidgetTree, LineFontSize, TextColor, true);
		UTextBlock *Reason = MakeWrappedText(WidgetTree, SmallFontSize, WarnColor);
		AddToColumn(Lines, Verb);
		AddToColumn(Lines, Reason);
		AddToColumn(ButtonBox, Plate, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		ButtonWidgets.Add({Plate, Verb, Reason});
	}
	for (int32 Index = 0; Index < ButtonWidgets.Num(); ++Index)
	{
		const FButtonWidgets &Widgets = ButtonWidgets[Index];
		const bool bShown = ShownButtons.IsValidIndex(Index);
		SetShown(Widgets.Plate.Get(), bShown);
		if (!bShown || !Widgets.Verb.IsValid() || !Widgets.Reason.IsValid())
		{
			continue;
		}
		const FJournalButton &Button = ShownButtons[Index];
		WoRKeyHints::SetHint(Widgets.Verb.Get(), Button.Label);
		Widgets.Verb->SetColorAndOpacity(FSlateColor(Button.Refusal.IsEmpty() ? TextColor : OffColor));
		Widgets.Reason->SetText(Button.Refusal);
		SetShown(Widgets.Reason.Get(), !Button.Refusal.IsEmpty());
	}

	ReaderNote->SetText(Row ? Row->Note : FText::GetEmpty());
	SetShown(ReaderNote, Row && !Row->Note.IsEmpty());
}

FText UJournalScreenWidget::GetHints() const
{
	TArray<FText> Parts;
	Parts.Add(LOCTEXT("HintPage", "{Tabs}  Page"));
	if (View.Filters.Num() > 1)
	{
		Parts.Add(LOCTEXT("HintKind", "‹ ›  Kind"));
	}
	if (const FJournalRow *Row = GetFocusedRow())
	{
		if (!Row->SelectVerb.IsEmpty())
		{
			Parts.Add(FText::FormatNamed(LOCTEXT("HintSelect", "{Confirm}  {Verb}"), TEXT("Verb"), Row->SelectVerb));
		}
		if (Row->IsSecret())
		{
			Parts.Add(FText::FormatNamed(LOCTEXT("HintToggle", "{Toggle}  {Verb}"), TEXT("Verb"), Row->ToggleVerb)); // D-198: Show or Hide
		}
	}
	if (Page == EJournalPage::Quests)
	{
		Parts.Add(LOCTEXT("HintHideAll", "{Second}  Hide all"));
	}
	Parts.Add(LOCTEXT("HintBack", "{Back}  Back"));
	return FText::Join(FText::AsCultureInvariant(JournalScreenStyle::HintGap), Parts);
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
	if (Row && Row->bQuest)
	{
		if (WoRJournalScreen::ToggleTrack(GetGameInstance(), *Row))
		{
			Refresh(); // the row keeps its place: tracking doesn't reorder the list
		}
		return;
	}
	const FName TalkId = Row ? Row->Id : NAME_None;
	if (Row && WoRJournalScreen::PlayNextLine(GetGameInstance(), *Row, State, ObjectPtrDecay(Story)))
	{
		Refresh();
		// Starting a talk lets the one read before it leave Ready, so the rows may have moved up: stay on this talk.
		const int32 NewIndex = View.Rows.IndexOfByPredicate([TalkId](const FJournalRow &Other) { return Other.Id == TalkId; });
		if (NewIndex != INDEX_NONE && NewIndex != SelectedRow)
		{
			SelectedRow = NewIndex;
			Refresh();
		}
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

void UJournalScreenWidget::PressButton(EJournalButton Button)
{
	UE_LOG(LogWoRUI, Verbose, TEXT("[Journal] Button %d"), static_cast<int32>(Button));
	switch (Button)
	{
	case EJournalButton::Select:
		PressSelect();
		break;
	case EJournalButton::Toggle:
		PressToggle();
		break;
	case EJournalButton::HideAll:
		PressHideAll();
		break;
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
	// B-630: Left / Right turn the kind plate; LT / RT still do.
	const bool bFilterKey = IsLeftKey(Key) || IsRightKey(Key) || IsSubTabPrevKey(Key) || IsSubTabNextKey(Key);
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
			MoveFilter(IsRightKey(Key) || IsSubTabNextKey(Key) ? 1 : -1);
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
	// B-602: the rows' places as scrolled now (a row scrolled out of view keeps its last drawn place).
	return WoRMenuStyle::FindScrolledRowUnder(RowScroll, FMath::Min(RowWidgets.Num(), View.Rows.Num()),
											  [this](int32 Index) -> const UWidget * { return RowWidgets[Index].Frame.Get(); },
											  MouseEvent.GetScreenSpacePosition());
}

int32 UJournalScreenWidget::FindButtonUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < FMath::Min(ButtonWidgets.Num(), ShownButtons.Num()); ++Index)
	{
		const UBorder *Plate = ButtonWidgets[Index].Plate.Get();
		if (Plate && Plate->IsVisible() && Plate->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
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
		if (Tab != static_cast<int32>(Page)) // a click on the lit tab changes nothing
		{
			MovePage(Tab - static_cast<int32>(Page));
		}
		return FReply::Handled();
	}
	const int32 FilterTab = FindUnder(FilterTabs, InMouseEvent);
	if (View.Filters.IsValidIndex(FilterTab))
	{
		if (FilterTab != Filter)
		{
			MoveFilter(FilterTab - Filter);
		}
		return FReply::Handled();
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (View.Rows.IsValidIndex(Row))
	{
		// B-630: a click only lights a row; the Details buttons act.
		if (Row != SelectedRow)
		{
			SelectedRow = Row;
			Refresh(/*bScrollIntoView=*/false);
		}
		return FReply::Handled();
	}
	const int32 Button = FindButtonUnder(InMouseEvent);
	if (ShownButtons.IsValidIndex(Button))
	{
		if (ShownButtons[Button].Refusal.IsEmpty()) // a greyed button only shows why
		{
			PressButton(ShownButtons[Button].Kind);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GJournalScreenOpenCommand(
	TEXT("wor.UI.Journal"),
	TEXT("B-463 / B-630: open the journal screen, as the game menu's Journal page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UJournalScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Journal] wor.UI.Journal: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
