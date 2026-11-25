// SettingsScreenWidget.cpp

#include "UI/Menus/SettingsScreenWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRSettingsScreenWidget"

namespace SettingsScreenStyle
{
	// B-631 (mockup menus/settings/v2, 1280x720 shown at 1080p): the loadout screen's three columns (v5); the shared look
	// is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float HeaderGap = 24.0f;
	constexpr float LeftWidth = 600.0f;
	constexpr float MiddleWidth = 618.0f;
	constexpr float RightWidth = 504.0f;
	constexpr float PanelGap = 27.0f;
	constexpr float PanelPadding = 24.0f;
	constexpr float RowHeight = 66.0f;
	constexpr float RowPaddingX = 21.0f;
	/** The On / Off switch on a row (mockup: 44x20 at 720p). */
	constexpr float SwitchWidth = 66.0f;
	constexpr float SwitchHeight = 30.0f;
	constexpr float SwitchPadding = 4.0f;
	constexpr float SwitchKnobSize = 22.0f;
	/** Controls' key caps in the middle (mockup: 68 square at 720p), grown by a long key name. */
	constexpr int32 KeyCapCount = 2;
	constexpr float KeyCapSize = 102.0f;
	constexpr float KeyCapGap = 90.0f;
	constexpr float KeyPictureHeight = 300.0f;
	/** The kind plate's lit word: a line under it (as the loadout screen's selectors). */
	constexpr float KindUnderlineHeight = 3.0f;
	/** Between two hints on the bottom line. */
	const TCHAR *const HintGap = TEXT("        ");
}

USettingsScreenWidget::USettingsScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

USettingsScreenWidget *USettingsScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Settings] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<USettingsScreenWidget>(Root->PushToMenu(USettingsScreenWidget::StaticClass()));
}

TSharedRef<SWidget> USettingsScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UBorder *USettingsScreenWidget::MakePanel(UVerticalBox *&OutColumn) const
{
	using namespace SettingsScreenStyle;
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	OutColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(OutColumn);
	return Panel;
}

void USettingsScreenWidget::BuildTree()
{
	using namespace SettingsScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	UTextBlock *Title = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	Title->SetText(LOCTEXT("Title", "Settings"));
	AddToColumn(Main, Title);

	UHorizontalBox *TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "{TabPrev}")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < WoRSettingsScreen::PAGE_COUNT; ++Index)
	{
		UTextBlock *Label = nullptr;
		PageTabs.Add(MakeTab(WidgetTree, Label));
		PageTabLabels.Add(Label);
		Label->SetText(WoRSettingsScreen::GetPageName(static_cast<ESettingsPage>(Index)));
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

	// Left: the page's name, Controls' ‹ kind › plate, the rows, and what an empty page says.
	ListTitle = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToColumn(Left, ListTitle, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	KindPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	KindPlate->SetBrushColor(PlateColor);
	KindPlate->SetPadding(FMargin(TabPaddingX, TabPaddingY));
	UHorizontalBox *Plate = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	KindPlate->SetContent(Plate);
	const auto AddArrow = [this, Plate](const FText &Arrow, const FMargin &ArrowPadding)
	{
		UTextBlock *Text = MakeText(WidgetTree, RowFontSize, CyanColor, true);
		Text->SetText(Arrow);
		AddToRow(Plate, Text, ArrowPadding, VAlign_Center);
	};
	AddArrow(LOCTEXT("KindPrev", "‹"), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (const FText &Section : WoRSettingsScreen::GetKeySections())
	{
		UBorder *Word = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Word->SetBrushColor(FLinearColor::Transparent);
		Word->SetPadding(FMargin(0.0f));
		UVerticalBox *Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock *Label = MakeText(WidgetTree, RowFontSize, DimColor, true);
		Label->SetText(Section);
		AddToColumn(Stack, Label);
		UBorder *Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Line->SetBrushColor(CyanColor);
		USizeBox *Under = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Under->SetHeightOverride(KindUnderlineHeight);
		Under->AddChild(Line);
		AddToColumn(Stack, Under)->SetHorizontalAlignment(HAlign_Fill);
		Word->SetContent(Stack);
		AddToRow(Plate, Word, FMargin(0.0f, 0.0f, SectionGap, 0.0f), VAlign_Center);
		KindWords.Add(Word);
		KindLabels.Add(Label);
		KindUnderlines.Add(Under);
	}
	AddArrow(LOCTEXT("KindNext", "›"), FMargin(0.0f));
	AddToColumn(Left, KindPlate, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RowList"));
	AddToColumn(Left, RowList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListNote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Left, ListNote);

	// Middle: the lit row, always shown (Controls: its key caps).
	PreviewName = MakeWrappedText(WidgetTree, NameFontSize, TextColor, true);
	PreviewLine = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Middle, PreviewName);
	AddToColumn(Middle, PreviewLine, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	KeyPicture = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	KeyPicture->SetBrushColor(ScreenColor);
	KeyPicture->SetHorizontalAlignment(HAlign_Center);
	KeyPicture->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Caps = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	KeyPicture->SetContent(Caps);
	for (int32 Index = 0; Index < KeyCapCount; ++Index)
	{
		UVerticalBox *Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UBorder *Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Cap->SetBrushColor(PlateColor);
		Cap->SetPadding(FMargin(TabPaddingX, TabPaddingY));
		Cap->SetHorizontalAlignment(HAlign_Center);
		Cap->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Key = MakeText(WidgetTree, NameFontSize, TextColor, true);
		Cap->SetContent(Key);
		USizeBox *CapSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CapSize->SetMinDesiredWidth(KeyCapSize); // a long key ("Left Shift (hold)") widens its cap
		CapSize->SetMinDesiredHeight(KeyCapSize);
		CapSize->AddChild(Cap);
		AddToColumn(Stack, CapSize)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock *Device = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Stack, Device, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
		AddToRow(Caps, Stack, FMargin(Index > 0 ? KeyCapGap : 0.0f, 0.0f, 0.0f, 0.0f));
		KeyCaps.Add({Key, Device});
	}
	AddToColumn(Middle, MakeSized(WidgetTree, MiddleWidth - 2.0f * PanelPadding, KeyPictureHeight, KeyPicture), FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	PreviewBlurb = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Middle, PreviewBlurb, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	// Right: Details, every choice, the note.
	UTextBlock *DetailsHeading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	DetailsHeading->SetText(LOCTEXT("Details", "Details"));
	AddToColumn(Right, DetailsHeading);
	DetailsSub = MakeText(WidgetTree, SmallFontSize, DimColor);
	DetailsSub->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	AddToColumn(Right, DetailsSub, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	ChoiceBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Right, ChoiceBox);
	DetailsNote = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Right, DetailsNote, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	HintText = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	AddToColumn(Main, HintText)->SetHorizontalAlignment(HAlign_Right);
}

USettingsScreenWidget::FRowWidgets USettingsScreenWidget::BuildRow()
{
	using namespace SettingsScreenStyle;

	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPaddingX, 0.0f));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	UTextBlock *Value = MakeText(WidgetTree, LineFontSize, DimColor, true);
	AddToRow(Line, Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Line, Value, FMargin(WordGap, 0.0f, 0.0f, 0.0f));
	UBorder *Track = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Track->SetPadding(FMargin(SwitchPadding));
	Track->SetVerticalAlignment(VAlign_Center);
	UBorder *Knob = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Knob->SetBrushColor(TextColor);
	Track->SetContent(MakeSized(WidgetTree, SwitchKnobSize, SwitchKnobSize, Knob));
	AddToRow(Line, MakeSized(WidgetTree, SwitchWidth, SwitchHeight, Track), FMargin(WordGap, 0.0f, 0.0f, 0.0f));

	// The frame sits in a box of the row's height; hiding that box keeps the list from holding empty rows.
	USizeBox *Height = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetHeightOverride(RowHeight);
	Height->AddChild(Frame);
	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(RowList->AddChild(Height)))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, LineGap));
	}
	return {Frame, Label, Value, Track};
}

USettingsScreenWidget::FChoiceWidgets USettingsScreenWidget::BuildChoice()
{
	using namespace SettingsScreenStyle;

	UBorder *Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetPadding(FMargin(TabPaddingX, TabPaddingY));
	UVerticalBox *Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Lines);
	UHorizontalBox *Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, LineFontSize, TextColor, true);
	UTextBlock *DefaultTag = MakeText(WidgetTree, SmallFontSize, OffColor, true);
	DefaultTag->SetText(LOCTEXT("ChoiceDefault", "default"));
	AddToRow(Top, Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Top, DefaultTag, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Top);
	AddToColumn(Lines, Top);
	UTextBlock *What = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Lines, What, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(ChoiceBox, Card, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	return {Card, Name, DefaultTag, What};
}

void USettingsScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// The stack may hand back a pooled instance: every open starts on the first page.
	Page = ESettingsPage::Gameplay;
	Kind = 0;
	SelectedRow = 0;
	bChanged = false;
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[Settings] Opened"));
}

void USettingsScreenWidget::NativeOnDeactivated()
{
	// A change is live already; the file gets it once, when the screen closes.
	if (bChanged)
	{
		WoRSettingsScreen::SaveChanges(GetWorld());
		bChanged = false;
	}
	Super::NativeOnDeactivated();
}

UWidget *USettingsScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<USettingsScreenWidget *>(this);
}

void USettingsScreenWidget::Refresh(bool bScrollIntoView)
{
	using namespace SettingsScreenStyle;

	Rows = Page == ESettingsPage::Controls ? WoRSettingsScreen::BuildKeyPage(Kind) : WoRSettingsScreen::BuildRows(GetWorld(), Page);
	SelectedRow = Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Rows.Num() - 1) : 0;
	if (!RowList)
	{
		return; // headless: no tree
	}

	for (int32 Index = 0; Index < PageTabs.Num(); ++Index)
	{
		const bool bShownPage = Index == static_cast<int32>(Page);
		PageTabs[Index]->SetBrushColor(bShownPage ? FocusColor : PlateColor);
		PageTabLabels[Index]->SetColorAndOpacity(FSlateColor(bShownPage ? TextColor : DimColor));
	}
	const bool bControls = Page == ESettingsPage::Controls;
	const TArray<FText> Sections = WoRSettingsScreen::GetKeySections();
	ListTitle->SetText(bControls && Sections.IsValidIndex(Kind)
						   ? FText::Format(LOCTEXT("ControlsTitle", "{0}  ·  {1}"), WoRSettingsScreen::GetPageName(Page), Sections[Kind])
						   : WoRSettingsScreen::GetPageName(Page));
	SetShown(KindPlate, bControls);
	for (int32 Index = 0; Index < KindWords.Num(); ++Index)
	{
		const bool bLit = Index == Kind;
		KindLabels[Index]->SetColorAndOpacity(FSlateColor(bLit ? TextColor : DimColor));
		KindUnderlines[Index]->SetVisibility(bLit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// The rows are made as a page first needs them and refilled after (never rebuilt).
	while (RowWidgets.Num() < Rows.Num())
	{
		RowWidgets.Add(BuildRow());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = RowWidgets[Index];
		UBorder *Frame = Widgets.Frame.Get();
		const bool bShown = Rows.IsValidIndex(Index);
		SetShown(Frame ? Frame->GetParent() : nullptr, bShown);
		if (!bShown || !Frame || !Widgets.Label.IsValid() || !Widgets.Value.IsValid() || !Widgets.SwitchTrack.IsValid())
		{
			continue;
		}
		const FSettingsRow &Row = Rows[Index];
		const bool bFocused = Index == SelectedRow;
		Frame->SetBrushColor(bFocused ? FocusColor : PlateColor);
		Widgets.Label->SetText(Row.Label);
		Widgets.Label->SetColorAndOpacity(FSlateColor(Row.CanChange() || Row.Id == ESettingId::FixedKey ? TextColor : OffColor));
		SetShown(Widgets.Value.Get(), !Row.bSwitch);
		Widgets.Value->SetText(bFocused && Row.CanChange() ? FText::Format(LOCTEXT("FocusedValue", "‹  {0}  ›"), Row.Value) : Row.Value);
		Widgets.Value->SetColorAndOpacity(FSlateColor(bFocused ? CyanColor : DimColor));
		// On / Off: the knob at the right on a green track, or at the left on a grey one.
		SetShown(Widgets.SwitchTrack->GetParent(), Row.bSwitch);
		const bool bOn = Row.Current == 0;
		Widgets.SwitchTrack->SetBrushColor(bOn ? GoodColor : OffColor);
		Widgets.SwitchTrack->SetHorizontalAlignment(bOn ? HAlign_Right : HAlign_Left);
	}
	// Visible, not SelfHitTestInvisible: the scroll box itself has to take the mouse wheel.
	RowList->SetVisibility(Rows.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ListNote->SetText(WoRSettingsScreen::GetPageNote(Page));
	SetShown(ListNote, Rows.IsEmpty());

	RefreshPreview();
	RefreshDetails();
	WoRKeyHints::SetHint(HintText, GetHints());

	if (bScrollIntoView && RowWidgets.IsValidIndex(SelectedRow) && Rows.IsValidIndex(SelectedRow))
	{
		if (UBorder *Frame = RowWidgets[SelectedRow].Frame.Get())
		{
			RowList->ScrollWidgetIntoView(Frame->GetParent(), /*AnimateScroll=*/false);
		}
	}
}

void USettingsScreenWidget::RefreshPreview()
{
	using namespace SettingsScreenStyle;

	const FSettingsRow *Row = Rows.IsValidIndex(SelectedRow) ? &Rows[SelectedRow] : nullptr;
	const bool bKey = Row && Row->Id == ESettingId::FixedKey;
	FText Line;
	if (bKey)
	{
		const TArray<FText> Sections = WoRSettingsScreen::GetKeySections();
		Line = Sections.IsValidIndex(Kind) ? Sections[Kind] : FText::GetEmpty();
	}
	else if (Row && !Row->Value.IsEmpty())
	{
		Line = FText::Format(LOCTEXT("PreviewNow", "Now: {0}"), Row->Value);
	}
	PreviewName->SetText(Row ? Row->Label : WoRSettingsScreen::GetPageName(Page));
	PreviewLine->SetText(Line);
	SetShown(PreviewLine, !Line.IsEmpty());

	// TODO: a picture per setting (the mockup's Turn View sketch); only a key's caps are drawn so far.
	SetShown(KeyPicture->GetParent(), bKey);
	for (int32 Index = 0; Index < KeyCaps.Num(); ++Index)
	{
		const bool bShown = bKey && Row->Choices.IsValidIndex(Index);
		if (KeyCaps[Index].Key.IsValid() && KeyCaps[Index].Device.IsValid())
		{
			KeyCaps[Index].Key->SetText(bShown ? Row->Choices[Index].What : FText::GetEmpty());
			KeyCaps[Index].Device->SetText(bShown ? Row->Choices[Index].Name : FText::GetEmpty());
		}
	}

	const FText Blurb = Row ? Row->Explanation : WoRSettingsScreen::GetPageNote(Page);
	PreviewBlurb->SetText(Blurb);
	SetShown(PreviewBlurb, !Blurb.IsEmpty());
}

void USettingsScreenWidget::RefreshDetails()
{
	using namespace SettingsScreenStyle;

	const FSettingsRow *Row = Rows.IsValidIndex(SelectedRow) ? &Rows[SelectedRow] : nullptr;
	const bool bKey = Row && Row->Id == ESettingId::FixedKey;
	FText Sub = WoRSettingsScreen::GetPageName(Page);
	if (Row)
	{
		Sub = Row->Choices.IsEmpty() ? Row->Label
			: bKey				   ? FText::Format(LOCTEXT("DetailsKeys", "{0}  ·  keys"), Row->Label)
								   : FText::Format(LOCTEXT("DetailsChoices", "{0}  ·  {1} choices"), Row->Label, FText::AsNumber(Row->Choices.Num()));
	}
	DetailsSub->SetText(Sub);

	const TArray<FSettingsChoice> NoChoices;
	const TArray<FSettingsChoice> &Choices = Row ? Row->Choices : NoChoices;
	while (ChoiceWidgets.Num() < Choices.Num())
	{
		ChoiceWidgets.Add(BuildChoice());
	}
	for (int32 Index = 0; Index < ChoiceWidgets.Num(); ++Index)
	{
		const FChoiceWidgets &Widgets = ChoiceWidgets[Index];
		const bool bShown = Choices.IsValidIndex(Index);
		SetShown(Widgets.Card.Get(), bShown);
		if (!bShown || !Widgets.Card.IsValid() || !Widgets.Name.IsValid() || !Widgets.DefaultTag.IsValid() || !Widgets.What.IsValid())
		{
			continue;
		}
		const FSettingsChoice &Choice = Choices[Index];
		const bool bLit = Index == Row->Current;
		Widgets.Card->SetBrushColor(bLit ? HoverColor : PlateColor);
		Widgets.Name->SetText(Choice.Name);
		Widgets.Name->SetColorAndOpacity(FSlateColor(bLit ? CyanColor : TextColor));
		SetShown(Widgets.DefaultTag.Get(), Choice.bDefault);
		Widgets.What->SetText(Choice.What);
		Widgets.What->SetColorAndOpacity(FSlateColor(bLit ? TextColor : DimColor));
		SetShown(Widgets.What.Get(), !Choice.What.IsEmpty());
	}

	// Why it can't change: a setting not built yet warns; a fixed key just says keys are fixed.
	DetailsNote->SetText(Row ? Row->Refusal : FText::GetEmpty());
	DetailsNote->SetColorAndOpacity(FSlateColor(bKey ? DimColor : WarnColor));
	SetShown(DetailsNote, Row && !Row->Refusal.IsEmpty());
}

FText USettingsScreenWidget::GetHints() const
{
	TArray<FText> Parts;
	Parts.Add(LOCTEXT("HintPage", "{Tabs}  Page"));
	if (Page == ESettingsPage::Controls)
	{
		Parts.Add(FText::Format(LOCTEXT("HintKind", "‹ ›  {0}"), FText::Join(FText::AsCultureInvariant(TEXT(" / ")), WoRSettingsScreen::GetKeySections())));
	}
	else if (Rows.IsValidIndex(SelectedRow) && Rows[SelectedRow].CanChange())
	{
		Parts.Add(LOCTEXT("HintChange", "‹ ›  Change"));
		Parts.Add(LOCTEXT("HintNext", "{Confirm}  Next choice"));
		Parts.Add(LOCTEXT("HintDefault", "{Second}  Default"));
	}
	Parts.Add(LOCTEXT("HintBack", "{Back}  Back"));
	return FText::Join(FText::AsCultureInvariant(SettingsScreenStyle::HintGap), Parts);
}

void USettingsScreenWidget::MovePage(int32 Delta)
{
	const int32 Count = WoRSettingsScreen::PAGE_COUNT;
	Page = static_cast<ESettingsPage>(((static_cast<int32>(Page) + Delta) % Count + Count) % Count);
	SelectedRow = 0;
	Refresh();
}

void USettingsScreenWidget::MoveKind(int32 Delta)
{
	const int32 Count = WoRSettingsScreen::GetKeySections().Num();
	if (Page == ESettingsPage::Controls && Count > 1)
	{
		Kind = ((Kind + Delta) % Count + Count) % Count;
		SelectedRow = 0;
		Refresh();
	}
}

void USettingsScreenWidget::MoveSelection(int32 Delta)
{
	if (Rows.Num() > 0)
	{
		SelectRow(((SelectedRow + Delta) % Rows.Num() + Rows.Num()) % Rows.Num());
	}
}

void USettingsScreenWidget::SelectRow(int32 Index)
{
	if (Rows.IsValidIndex(Index) && Index != SelectedRow)
	{
		SelectedRow = Index;
		Refresh();
	}
}

void USettingsScreenWidget::ChangeSelected(int32 Direction)
{
	if (!Rows.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FSettingsRow &Row = Rows[SelectedRow];
	if (!Row.CanChange())
	{
		UE_CLOG(Row.Id != ESettingId::FixedKey, LogWoRUI, Log, TEXT("[Settings] %s refused: %s"), *Row.Label.ToString(), *Row.Refusal.ToString());
		return;
	}
	if (WoRSettingsScreen::ChangeSetting(GetWorld(), Row.Id, Direction))
	{
		bChanged = true;
		Refresh();
	}
}

void USettingsScreenWidget::PickChoice(int32 Choice)
{
	if (!Rows.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FSettingsRow &Row = Rows[SelectedRow];
	if (!Row.CanChange())
	{
		UE_CLOG(Row.Id != ESettingId::FixedKey, LogWoRUI, Log, TEXT("[Settings] %s refused: %s"), *Row.Label.ToString(), *Row.Refusal.ToString());
		return;
	}
	if (Choice != Row.Current && WoRSettingsScreen::SetSettingChoice(GetWorld(), Row.Id, Choice))
	{
		bChanged = true;
		Refresh();
	}
}

void USettingsScreenWidget::PressDefault()
{
	if (Rows.IsValidIndex(SelectedRow))
	{
		PickChoice(Rows[SelectedRow].GetDefaultChoice());
	}
}

FReply USettingsScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	const bool bSideKey = IsLeftKey(Key) || IsRightKey(Key);
	const bool bPageKey = IsTabPrevKey(Key) || IsTabNextKey(Key);
	if (bSideKey || bPageKey || IsConfirmKey(Key) || IsSecondActionKey(Key) || IsBackKey(Key))
	{
		if (InKeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}
		if (bPageKey)
		{
			MovePage(IsTabNextKey(Key) ? 1 : -1);
		}
		else if (bSideKey && Page == ESettingsPage::Controls)
		{
			MoveKind(IsRightKey(Key) ? 1 : -1); // B-631: Controls' Left / Right turn the kind
		}
		else if (bSideKey || IsConfirmKey(Key))
		{
			ChangeSelected(IsLeftKey(Key) ? -1 : 1); // A: the next choice
		}
		else if (IsSecondActionKey(Key))
		{
			PressDefault();
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

int32 USettingsScreenWidget::FindUnder(const TArray<TObjectPtr<UBorder>> &Boxes, const FPointerEvent &MouseEvent) const
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

int32 USettingsScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	// B-602: the rows' places as scrolled now (a row scrolled out of view keeps its last drawn place).
	return WoRMenuStyle::FindScrolledRowUnder(RowList, FMath::Min(RowWidgets.Num(), Rows.Num()),
											  [this](int32 Index) -> const UWidget * { return RowWidgets[Index].Frame.Get(); },
											  MouseEvent.GetScreenSpacePosition());
}

int32 USettingsScreenWidget::FindChoiceUnder(const FPointerEvent &MouseEvent) const
{
	const int32 Count = Rows.IsValidIndex(SelectedRow) ? FMath::Min(ChoiceWidgets.Num(), Rows[SelectedRow].Choices.Num()) : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const UBorder *Card = ChoiceWidgets[Index].Card.Get();
		if (Card && Card->IsVisible() && Card->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply USettingsScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	const int32 Row = InMouseEvent.GetCursorDelta().IsZero() ? INDEX_NONE : FindRowUnder(InMouseEvent);
	if (Rows.IsValidIndex(Row) && Row != SelectedRow)
	{
		SelectedRow = Row;
		Refresh(/*bScrollIntoView=*/false); // the list mustn't move under the cursor
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USettingsScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
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
	const int32 KindWord = Page == ESettingsPage::Controls ? FindUnder(KindWords, InMouseEvent) : INDEX_NONE;
	if (KindWord != INDEX_NONE)
	{
		if (KindWord != Kind)
		{
			MoveKind(KindWord - Kind);
		}
		return FReply::Handled();
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (Rows.IsValidIndex(Row)) // a hidden row keeps its old place on screen
	{
		// B-631: a click lights a row; a click on the lit row changes it, as A does.
		if (Row == SelectedRow)
		{
			ChangeSelected(1);
		}
		else
		{
			SelectedRow = Row;
			Refresh(/*bScrollIntoView=*/false);
		}
		return FReply::Handled();
	}
	const int32 Choice = FindChoiceUnder(InMouseEvent);
	if (Choice != INDEX_NONE)
	{
		PickChoice(Choice);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GSettingsOpenCommand(
	TEXT("wor.UI.Settings"),
	TEXT("B-452 / B-631: open the settings screen, as the game menu's Settings page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!USettingsScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Settings] wor.UI.Settings: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
