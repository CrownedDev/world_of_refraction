// SettingsScreenWidget.cpp

#include "UI/Menus/SettingsScreenWidget.h"

#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRSettingsScreenWidget"

namespace SettingsScreenStyle
{
	// This screen's own layout (screen_mockups/menus/settings, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float HeaderGap = 27.0f;
	constexpr float ListWidth = 1008.0f;
	constexpr float RowHeight = 75.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowPaddingX = 27.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 723.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float KeyColumnWidth = 300.0f;
	constexpr float KeyRowGap = 6.0f;
	constexpr float KeyScrollStep = 90.0f;
	/** The most rows a page has (Gameplay and Accessibility have seven). */
	constexpr int32 MaxRows = 7;
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

	// Left: the page's rows, or the key list on Controls.
	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (int32 Index = 0; Index < MaxRows; ++Index)
	{
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetPadding(FMargin(RowPaddingX, 0.0f));
		Row->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(Columns);
		UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, DimColor, true);
		AddToRow(Columns, Label);
		UHorizontalBoxSlot *ValueSlot = AddToRow(Columns, Value);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		RowBoxes.Add(Row);
		RowLabels.Add(Label);
		RowValues.Add(Value);
		AddToColumn(ListColumn, MakeSized(WidgetTree, ListWidth, RowHeight, Row), FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	KeyList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	for (const FSettingsKeyRow &KeyRow : WoRSettingsScreen::BuildKeyRows())
	{
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *Action = MakeText(WidgetTree, LineFontSize, KeyRow.IsHeading() ? CyanColor : TextColor, KeyRow.IsHeading());
		Action->SetText(KeyRow.IsHeading() ? KeyRow.Section : KeyRow.Action);
		UTextBlock *Keyboard = MakeText(WidgetTree, LineFontSize, DimColor, true);
		Keyboard->SetText(KeyRow.IsHeading() ? LOCTEXT("ColumnKeyboard", "Keyboard and mouse") : KeyRow.Keyboard);
		UTextBlock *Gamepad = MakeText(WidgetTree, LineFontSize, DimColor, true);
		Gamepad->SetText(KeyRow.IsHeading() ? LOCTEXT("ColumnGamepad", "Gamepad") : KeyRow.Gamepad);
		AddToRow(Line, Action)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(Line, MakeSized(WidgetTree, KeyColumnWidth, 0.0f, Keyboard));
		AddToRow(Line, MakeSized(WidgetTree, KeyColumnWidth, 0.0f, Gamepad));
		UBorder *LineBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LineBox->SetBrushColor(KeyRow.IsHeading() ? ScreenColor : PlateColor);
		LineBox->SetPadding(FMargin(RowPaddingX, KeyRowGap));
		LineBox->SetContent(Line);
		KeyList->AddChild(LineBox);
	}
	AddToColumn(ListColumn, KeyList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);

	// Right: what the focused setting is, in plain words.
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	PanelHeading = MakeText(WidgetTree, NameFontSize, TextColor, true);
	PanelBody = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	PanelRefusal = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(PanelColumn, PanelHeading);
	AddToColumn(PanelColumn, PanelBody, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, PanelRefusal, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// Bottom right: the hints. TODO: "Y Reset page" when resetting a page is built.
	UTextBlock *Hints = MakeHintText(WidgetTree, LineFontSize, DimColor); // B-481: the keys of the device in use
	WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{Tabs}  Page        Left / Right  Change        {Back}  Back"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

void USettingsScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// The stack may hand back a pooled instance: every open starts on the first page.
	Page = ESettingsPage::Gameplay;
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

void USettingsScreenWidget::Refresh()
{
	using namespace SettingsScreenStyle;

	Rows = WoRSettingsScreen::BuildRows(GetWorld(), Page);
	SelectedRow = Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Rows.Num() - 1) : 0;

	for (int32 Index = 0; Index < PageTabs.Num(); ++Index)
	{
		const bool bShownPage = Index == static_cast<int32>(Page);
		PageTabs[Index]->SetBrushColor(bShownPage ? FocusColor : PlateColor);
		PageTabLabels[Index]->SetColorAndOpacity(FSlateColor(bShownPage ? TextColor : DimColor));
	}
	for (int32 Index = 0; Index < RowBoxes.Num(); ++Index)
	{
		const bool bShown = Rows.IsValidIndex(Index);
		// The row sits in a sized box: hiding the box's parent keeps the list from holding empty rows.
		SetShown(RowBoxes[Index]->GetParent(), bShown);
		if (!bShown)
		{
			continue;
		}
		const FSettingsRow &Row = Rows[Index];
		const bool bFocused = Index == SelectedRow;
		RowBoxes[Index]->SetBrushColor(bFocused ? FocusColor : PlateColor);
		RowLabels[Index]->SetText(Row.Label);
		RowLabels[Index]->SetColorAndOpacity(FSlateColor(Row.CanChange() ? TextColor : OffColor));
		RowValues[Index]->SetText(bFocused && Row.CanChange() ? FText::Format(LOCTEXT("FocusedValue", "‹  {0}  ›"), Row.Value) : Row.Value);
		RowValues[Index]->SetColorAndOpacity(FSlateColor(bFocused ? TextColor : DimColor));
	}
	if (KeyList)
	{
		// Visible, not SelfHitTestInvisible: the scroll box itself has to take the mouse wheel.
		KeyList->SetVisibility(Page == ESettingsPage::Controls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!PanelHeading)
	{
		return; // headless: no tree
	}
	if (Rows.IsValidIndex(SelectedRow))
	{
		const FSettingsRow &Row = Rows[SelectedRow];
		PanelHeading->SetText(Row.Label);
		PanelBody->SetText(Row.Explanation);
		PanelRefusal->SetText(Row.Refusal);
		SetShown(PanelRefusal, !Row.CanChange());
	}
	else
	{
		PanelHeading->SetText(WoRSettingsScreen::GetPageName(Page));
		PanelBody->SetText(WoRSettingsScreen::GetPageNote(Page));
		SetShown(PanelRefusal, false);
	}
}

void USettingsScreenWidget::MovePage(int32 Delta)
{
	const int32 Count = WoRSettingsScreen::PAGE_COUNT;
	Page = static_cast<ESettingsPage>(((static_cast<int32>(Page) + Delta) % Count + Count) % Count);
	SelectedRow = 0;
	Refresh();
}

void USettingsScreenWidget::MoveSelection(int32 Delta)
{
	if (Rows.Num() > 0)
	{
		SelectRow(((SelectedRow + Delta) % Rows.Num() + Rows.Num()) % Rows.Num());
	}
	else if (KeyList && Page == ESettingsPage::Controls)
	{
		// The key list has no focused row: up and down scroll it.
		KeyList->SetScrollOffset(FMath::Max(KeyList->GetScrollOffset() + Delta * SettingsScreenStyle::KeyScrollStep, 0.0f));
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
		UE_LOG(LogWoRUI, Log, TEXT("[Settings] %s refused: %s"), *Row.Label.ToString(), *Row.Refusal.ToString());
		return;
	}
	if (WoRSettingsScreen::ChangeSetting(GetWorld(), Row.Id, Direction))
	{
		bChanged = true;
		Refresh();
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
	if (IsLeftKey(Key) || IsRightKey(Key) || IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			ChangeSelected(IsLeftKey(Key) ? -1 : 1);
		}
		return FReply::Handled();
	}
	if (IsTabPrevKey(Key) || IsTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			MovePage(IsTabNextKey(Key) ? 1 : -1);
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

FReply USettingsScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindUnder(RowBoxes, InMouseEvent));
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
		MovePage(Tab - static_cast<int32>(Page));
		return FReply::Handled();
	}
	const int32 Row = FindUnder(RowBoxes, InMouseEvent);
	if (Rows.IsValidIndex(Row)) // a hidden row keeps its old place on screen
	{
		SelectRow(Row);
		ChangeSelected(1);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GSettingsOpenCommand(
	TEXT("wor.UI.Settings"),
	TEXT("B-452: open the settings screen, as the game menu's Settings page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!USettingsScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Settings] wor.UI.Settings: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
