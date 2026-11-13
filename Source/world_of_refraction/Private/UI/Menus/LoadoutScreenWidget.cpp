// LoadoutScreenWidget.cpp

#include "UI/Menus/LoadoutScreenWidget.h"

#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutConstants.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutScreenWidget"

namespace LoadoutScreenStyle
{
	// This screen's own layout (screen_mockups/menus/loadout, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 978.0f;
	constexpr float RowHeight = 126.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowPadding = 21.0f;
	constexpr float NameColumnWidth = 480.0f;
	constexpr float SocketWidth = 190.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 753.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float DetailLabelWidth = 225.0f;
	constexpr float TabPaddingX = 21.0f;
	constexpr float TabPaddingY = 9.0f;
	constexpr float TabGap = 9.0f;
	constexpr float PipWidth = 51.0f;
	constexpr float PipHeight = 36.0f;
	constexpr float LoadoutBoxWidth = 708.0f;
	constexpr float HeaderGap = 27.0f;
	constexpr int32 SocketsPerRow = 2;
	constexpr int32 MaxDetailLines = 6;
	constexpr int32 MaxBudgetPips = LoadoutConstants::LOADOUT_TOTAL_BUDGET;

	UBorder *MakeTab(UWidgetTree *Tree, UTextBlock *&OutLabel)
	{
		UBorder *Tab = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tab->SetBrushColor(PlateColor);
		Tab->SetPadding(FMargin(TabPaddingX, TabPaddingY));
		OutLabel = MakeText(Tree, RowFontSize, DimColor, true);
		Tab->SetContent(OutLabel);
		return Tab;
	}

	/** The small dim key label beside a tab row: "LT", "RB". */
	UTextBlock *MakeKeyLabel(UWidgetTree *Tree, const FText &Label)
	{
		UTextBlock *Text = MakeText(Tree, SmallFontSize, CyanColor, true);
		Text->SetText(Label);
		return Text;
	}
}

ULoadoutScreenWidget::ULoadoutScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

ULoadoutScreenWidget *ULoadoutScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<ULoadoutScreenWidget>(Root->PushToMenu(ULoadoutScreenWidget::StaticClass()));
}

TSharedRef<SWidget> ULoadoutScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void ULoadoutScreenWidget::BuildTree()
{
	using namespace LoadoutScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	// Top: tabs and budget on the left, the saved loadout on the right.
	UHorizontalBox *Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Header);
	UVerticalBox *TabsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToRow(Header, TabsColumn, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBox *MemberRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "LT")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		UTextBlock *Label = nullptr;
		MemberTabs.Add(MakeTab(WidgetTree, Label));
		MemberTabLabels.Add(Label);
		AddToRow(MemberRow, MemberTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "RT")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(TabsColumn, MemberRow);

	UHorizontalBox *PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *GearLabel = nullptr;
	UBorder *GearTab = MakeTab(WidgetTree, GearLabel);
	GearTab->SetBrushColor(FocusColor);
	GearLabel->SetText(LOCTEXT("PageGear", "Gear"));
	GearLabel->SetColorAndOpacity(FSlateColor(TextColor));
	UTextBlock *SkillsLabel = nullptr;
	UBorder *SkillsTab = MakeTab(WidgetTree, SkillsLabel);
	SkillsLabel->SetText(LOCTEXT("PageSkills", "Skills")); // TODO: B-440d builds the page; until then the tab is off
	SkillsLabel->SetColorAndOpacity(FSlateColor(OffColor));
	AddToRow(PageRow, GearTab, FMargin(0.0f, 0.0f, TabGap, 0.0f));
	AddToRow(PageRow, SkillsTab);
	AddToColumn(TabsColumn, PageRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *BudgetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *BudgetLabel = MakeText(WidgetTree, LineFontSize, DimColor, true);
	BudgetLabel->SetText(LOCTEXT("Budget", "Budget"));
	AddToRow(BudgetRow, BudgetLabel, FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < MaxBudgetPips; ++Index)
	{
		UBorder *Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BudgetPips.Add(Pip);
		AddToRow(BudgetRow, MakeSized(WidgetTree, PipWidth, PipHeight, Pip), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	BudgetText = MakeText(WidgetTree, LineFontSize, TextColor, true);
	AddToRow(BudgetRow, BudgetText, FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(TabsColumn, BudgetRow, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	UBorder *LoadoutBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LoadoutBox->SetBrushColor(PanelColor);
	LoadoutBox->SetPadding(FMargin(PanelPadding, SectionGap));
	UVerticalBox *LoadoutColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LoadoutBox->SetContent(LoadoutColumn);
	UTextBlock *LoadoutHeading = MakeText(WidgetTree, SmallFontSize, DimColor, true);
	LoadoutHeading->SetText(LOCTEXT("LoadoutHeading", "Loadout"));
	LoadoutTitleText = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToColumn(LoadoutColumn, LoadoutHeading);
	AddToColumn(LoadoutColumn, LoadoutTitleText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Header, MakeSized(WidgetTree, LoadoutBoxWidth, 0.0f, LoadoutBox), FMargin(0.0f), VAlign_Top);

	// The rows, and the focused row's details.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RefusalText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(ListColumn, RefusalText);
	RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(ListColumn, RowList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);

	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	DetailName = MakeText(WidgetTree, NameFontSize, TextColor, true);
	DetailKindLine = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, DetailName);
	AddToColumn(PanelColumn, DetailKindLine, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	for (int32 Index = 0; Index < MaxDetailLines; ++Index)
	{
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		DetailLabels.Add(MakeText(WidgetTree, LineFontSize, DimColor));
		DetailValues.Add(MakeWrappedText(WidgetTree, LineFontSize, TextColor, true));
		AddToRow(Line, MakeSized(WidgetTree, DetailLabelWidth, 0.0f, DetailLabels.Last()), FMargin(0.0f), VAlign_Top);
		AddToRow(Line, DetailValues.Last(), FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToColumn(PanelColumn, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	}
	UTextBlock *CostHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	CostHeading->SetText(LOCTEXT("CostHeading", "Cost"));
	DetailCostLine = MakeText(WidgetTree, RowFontSize, TextColor, true);
	DetailNote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, CostHeading, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, DetailCostLine, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, DetailNote, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// Bottom right: the hints. TODO: B-440b adds "A Change" and "X Unequip".
	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "LT / RT  Character        B / Esc  Back"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

ULoadoutScreenWidget::FRowWidgets ULoadoutScreenWidget::BuildRow()
{
	using namespace LoadoutScreenStyle;

	FRowWidgets Widgets;
	UVerticalBox *Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Heading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	Heading->SetText(LOCTEXT("ApparelHeading", "Apparel"));
	AddToColumn(Root, Heading, FMargin(0.0f, LineGap, 0.0f, LineGap));

	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Columns);

	UVerticalBox *NameColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox *NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UBorder *ActiveChip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ActiveChip->SetBrushColor(ScreenColor);
	ActiveChip->SetPadding(FMargin(WordGap, 0.0f));
	UTextBlock *ActiveLabel = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	ActiveLabel->SetText(LOCTEXT("Active", "ACTIVE"));
	ActiveChip->SetContent(ActiveLabel);
	AddToRow(NameRow, Name);
	AddToRow(NameRow, ActiveChip, FMargin(SectionGap, 0.0f, 0.0f, 0.0f));
	UTextBlock *KindLine = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(NameColumn, NameRow);
	AddToColumn(NameColumn, KindLine);
	AddToRow(Columns, MakeSized(WidgetTree, NameColumnWidth, 0.0f, NameColumn));

	for (int32 Index = 0; Index < SocketsPerRow; ++Index)
	{
		UVerticalBox *Socket = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Widgets.SocketNames.Add(MakeText(WidgetTree, LineFontSize, TextColor, true));
		Widgets.SocketLabels.Add(MakeText(WidgetTree, SmallFontSize, DimColor));
		AddToColumn(Socket, Widgets.SocketNames.Last().Get());
		AddToColumn(Socket, Widgets.SocketLabels.Last().Get());
		UWidget *SocketBox = MakeSized(WidgetTree, SocketWidth, 0.0f, Socket);
		Widgets.SocketBoxes.Add(SocketBox);
		AddToRow(Columns, SocketBox);
	}

	UTextBlock *Cost = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UHorizontalBoxSlot *CostSlot = AddToRow(Columns, Cost);
	CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CostSlot->SetHorizontalAlignment(HAlign_Right);

	AddToColumn(Root, MakeSized(WidgetTree, ListWidth, RowHeight, Frame), FMargin(0.0f, 0.0f, 0.0f, RowGap));
	RowList->AddChild(Root);

	Widgets.Root = Root;
	Widgets.Heading = Heading;
	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.ActiveChip = ActiveChip;
	Widgets.KindLine = KindLine;
	Widgets.Cost = Cost;
	return Widgets;
}

void ULoadoutScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshData();
	RefreshSelection();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutScreen] Opened (%d member(s))"), Members.Num());
}

UWidget *ULoadoutScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<ULoadoutScreenWidget *>(this);
}

void ULoadoutScreenWidget::RefreshData()
{
	using namespace LoadoutScreenStyle;

	UWorld *World = GetWorld();
	Members = WoRLoadoutScreen::BuildMembers(World);
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	Page = Members.IsValidIndex(MemberIndex) ? WoRLoadoutScreen::BuildGearPage(World, Members[MemberIndex]) : FLoadoutScreenPage();
	if (!Members.IsValidIndex(MemberIndex))
	{
		Page.Refusal = LOCTEXT("NoParty", "There is no party to show.");
	}
	SelectedRow = Page.Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Page.Rows.Num() - 1) : 0;

	for (int32 Index = 0; Index < MemberTabs.Num(); ++Index)
	{
		const bool bShown = Members.IsValidIndex(Index);
		SetShown(MemberTabs[Index], bShown);
		if (bShown)
		{
			MemberTabLabels[Index]->SetText(Members[Index].Name);
			MemberTabs[Index]->SetBrushColor(Index == MemberIndex ? FocusColor : PlateColor);
			MemberTabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == MemberIndex ? TextColor : DimColor));
		}
	}
	for (int32 Index = 0; Index < BudgetPips.Num(); ++Index)
	{
		SetShown(BudgetPips[Index], Index < Page.Budget);
		BudgetPips[Index]->SetBrushColor(Index < Page.Spent ? CyanColor : TrackColor);
	}
	if (BudgetText && LoadoutTitleText && RefusalText)
	{
		BudgetText->SetText(FText::Format(LOCTEXT("BudgetValue", "{0} of {1}"), FText::AsNumber(Page.Spent), FText::AsNumber(Page.Budget)));
		BudgetText->SetColorAndOpacity(FSlateColor(Page.Spent > Page.Budget ? WarnColor : TextColor));
		LoadoutTitleText->SetText(Page.LoadoutTitle);
		RefusalText->SetText(Page.Refusal);
		SetShown(RefusalText, !Page.Refusal.IsEmpty());
	}

	if (!RowList)
	{
		return; // headless: no tree
	}
	while (RowWidgets.Num() < Page.Rows.Num())
	{
		RowWidgets.Add(BuildRow());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const bool bShown = Page.Rows.IsValidIndex(Index);
		SetShown(RowWidgets[Index].Root.Get(), bShown);
		if (bShown)
		{
			FillRow(RowWidgets[Index], Page.Rows[Index], Index == Page.FirstApparelRow);
		}
	}
}

void ULoadoutScreenWidget::FillRow(const FRowWidgets &Widgets, const FLoadoutScreenRow &Row, bool bHeading) const
{
	using namespace LoadoutScreenStyle;

	if (!Widgets.Name.IsValid() || !Widgets.KindLine.IsValid() || !Widgets.Cost.IsValid())
	{
		return;
	}
	const bool bEmptySlot = Row.Kind == ELoadoutScreenRowKind::EmptyApparel;
	SetShown(Widgets.Heading.Get(), bHeading);
	Widgets.Name->SetText(Row.Name);
	Widgets.Name->SetColorAndOpacity(FSlateColor(bEmptySlot ? OffColor : TextColor));
	SetShown(Widgets.ActiveChip.Get(), Row.bActive);
	Widgets.KindLine->SetText(Row.KindLine);
	for (int32 Index = 0; Index < Widgets.SocketBoxes.Num(); ++Index)
	{
		const bool bShown = Row.Sockets.IsValidIndex(Index);
		SetShown(Widgets.SocketBoxes[Index].Get(), bShown);
		if (bShown && Widgets.SocketNames[Index].IsValid() && Widgets.SocketLabels[Index].IsValid())
		{
			const FLoadoutScreenSocket &Socket = Row.Sockets[Index];
			Widgets.SocketNames[Index]->SetText(Socket.bEmpty ? LOCTEXT("SocketEmpty", "empty") : Socket.Name);
			Widgets.SocketNames[Index]->SetColorAndOpacity(FSlateColor(Socket.bEmpty ? OffColor : TextColor));
			Widgets.SocketLabels[Index]->SetText(Socket.Label);
		}
	}
	Widgets.Cost->SetText(Row.bCountsToBudget ? FText::AsNumber(Row.Cost) : Row.CostLine);
	Widgets.Cost->SetColorAndOpacity(FSlateColor(Row.bCountsToBudget ? TextColor : DimColor));
}

void ULoadoutScreenWidget::RefreshSelection(bool bScrollIntoView)
{
	using namespace LoadoutScreenStyle;

	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = RowWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		}
	}
	if (!DetailName)
	{
		return; // headless: no tree
	}
	const bool bHasRow = Page.Rows.IsValidIndex(SelectedRow);
	const FLoadoutScreenRow Row = bHasRow ? Page.Rows[SelectedRow] : FLoadoutScreenRow();
	DetailName->SetText(bHasRow && !Row.Tier.IsEmpty() ? FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Row.Name, Row.Tier) : Row.Name);
	DetailKindLine->SetText(Row.KindLine);
	for (int32 Index = 0; Index < DetailLabels.Num(); ++Index)
	{
		const bool bShown = Row.Details.IsValidIndex(Index);
		SetShown(DetailLabels[Index], bShown);
		SetShown(DetailValues[Index], bShown);
		if (bShown)
		{
			DetailLabels[Index]->SetText(Row.Details[Index].Label);
			DetailValues[Index]->SetText(Row.Details[Index].Value);
		}
	}
	DetailCostLine->SetText(Row.CostLine);
	DetailNote->SetText(Row.Note);
	if (bScrollIntoView && RowList && RowWidgets.IsValidIndex(SelectedRow))
	{
		if (UWidget *RowRoot = RowWidgets[SelectedRow].Root.Get())
		{
			RowList->ScrollWidgetIntoView(RowRoot, /*AnimateScroll=*/false);
		}
	}
}

void ULoadoutScreenWidget::MoveSelection(int32 Delta)
{
	if (Page.Rows.Num() > 0)
	{
		SelectRow(((SelectedRow + Delta) % Page.Rows.Num() + Page.Rows.Num()) % Page.Rows.Num());
	}
}

void ULoadoutScreenWidget::SelectRow(int32 Index, bool bScrollIntoView)
{
	if (Page.Rows.IsValidIndex(Index) && Index != SelectedRow)
	{
		SelectedRow = Index;
		RefreshSelection(bScrollIntoView);
	}
}

void ULoadoutScreenWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	SelectedRow = 0;
	RefreshData();
	RefreshSelection();
}

FReply ULoadoutScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key))
	{
		MoveSelection(-1);
		return FReply::Handled();
	}
	if (IsDownKey(Key))
	{
		MoveSelection(1);
		return FReply::Handled();
	}
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
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

int32 ULoadoutScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	// A row scrolled out of view keeps its geometry past the list's edge: only the list's own area counts.
	if (!RowList || !RowList->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < RowWidgets.Num() && Index < Page.Rows.Num(); ++Index)
	{
		const UBorder *Frame = RowWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 ULoadoutScreenWidget::FindMemberTabUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
	{
		if (MemberTabs[Index] && MemberTabs[Index]->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply ULoadoutScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent), /*bScrollIntoView=*/false); // the list mustn't move under the cursor
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply ULoadoutScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const int32 Tab = FindMemberTabUnder(InMouseEvent);
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || Tab == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	MoveMember(Tab - MemberIndex);
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadoutScreenOpenCommand(
	TEXT("wor.UI.Loadout"),
	TEXT("B-440a: open the loadout screen (gear page, read only), as the game menu's Loadout page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!ULoadoutScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutScreen] wor.UI.Loadout: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
