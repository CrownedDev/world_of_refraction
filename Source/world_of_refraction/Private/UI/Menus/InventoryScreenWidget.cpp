// InventoryScreenWidget.cpp

#include "UI/Menus/InventoryScreenWidget.h"

#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ElementColors.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRInventoryScreenWidget"

namespace InventoryScreenStyle
{
	// This screen's own layout (screen_mockups/menus/inventory, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr int32 Columns = 6;
	constexpr float TileSize = 144.0f;
	constexpr float TileGap = 9.0f; // each side: 18 between tiles
	constexpr float TilePadding = 9.0f;
	constexpr float GridWidth = Columns * (TileSize + 2.0f * TileGap);
	constexpr int32 MarkFontSize = 38;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 753.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float DetailLabelWidth = 225.0f;
	constexpr float ChoiceWidth = 450.0f;
	constexpr float ChoicePadding = 15.0f;
	constexpr int32 MaxDetailLines = 6;
	/** The verb menu has at most three lines (Give, Store, Take); "Give to" at most the rest of the party. */
	constexpr int32 MaxChoices = 3;

	FLinearColor MarkColor(ESpellElement Element)
	{
		if (Element == ESpellElement::None)
		{
			return DimColor;
		}
		// Darkness is near-black: on a dark tile it takes the pale outline colour instead (D-106).
		return ElementColors::NeedsTextOutline(Element) ? ElementColors::TextOutline : ElementColors::GetColorForElement(Element);
	}
}

UInventoryScreenWidget::UInventoryScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its tiles and lines are plain borders
}

UInventoryScreenWidget *UInventoryScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[InventoryScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UInventoryScreenWidget>(Root->PushToMenu(UInventoryScreenWidget::StaticClass()));
}

TSharedRef<SWidget> UInventoryScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UInventoryScreenWidget::BuildTree()
{
	using namespace InventoryScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	// Top: the character tabs, then the Gear / Pouch / Chest tabs.
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
	AddToColumn(Main, MemberRow);

	UHorizontalBox *PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(PageRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "LB")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (const EInventoryScreenTab PageTab : WoRInventoryScreen::GetTabs())
	{
		UTextBlock *Label = nullptr;
		PageTabs.Add(MakeTab(WidgetTree, Label));
		PageTabLabels.Add(Label);
		Label->SetText(WoRInventoryScreen::GetTabName(PageTab));
		AddToRow(PageRow, PageTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(PageRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabNext", "RB")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, PageRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	// The tiles, and the focused tile's details and verbs.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, SectionGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox *GridColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	HeadingText = MakeText(WidgetTree, LineFontSize, DimColor, true);
	EmptyText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(GridColumn, HeadingText, FMargin(TileGap, 0.0f, 0.0f, LineGap));
	AddToColumn(GridColumn, EmptyText, FMargin(TileGap, 0.0f, 0.0f, LineGap));
	TileScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	TileGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	TileGrid->SetSlotPadding(FMargin(TileGap));
	TileScroll->AddChild(TileGrid);
	AddToColumn(GridColumn, TileScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Legend = MakeText(WidgetTree, SmallFontSize, DimColor);
	Legend->SetText(LOCTEXT("Legend", "E  worn in a loadout"));
	AddToColumn(GridColumn, Legend, FMargin(TileGap, LineGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, GridWidth, 0.0f, GridColumn), FMargin(0.0f), VAlign_Fill);

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
	DetailNote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, DetailNote, FMargin(0.0f, LineGap, 0.0f, SectionGap));

	ChoiceHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	ChoiceHeading->SetText(LOCTEXT("GiveTo", "Give to"));
	AddToColumn(PanelColumn, ChoiceHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	for (int32 Index = 0; Index < MaxChoices; ++Index)
	{
		ChoiceWidgets.Add(BuildChoice(PanelColumn));
	}
	WholeStackText = MakeText(WidgetTree, LineFontSize, CyanColor);
	AddToColumn(PanelColumn, WholeStackText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	StatusText = MakeWrappedText(WidgetTree, LineFontSize, GoodColor);
	AddToColumn(PanelColumn, StatusText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "LT / RT  Character        LB / RB  Tab        A / Enter  Choose        X  Whole stack        B / Esc  Back"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

UInventoryScreenWidget::FTileWidgets UInventoryScreenWidget::BuildTile(int32 Index)
{
	using namespace InventoryScreenStyle;

	FTileWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(TilePadding));
	UOverlay *Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Frame->SetContent(Layers);

	UTextBlock *Mark = MakeText(WidgetTree, MarkFontSize, DimColor, true);
	UOverlaySlot *MarkSlot = Layers->AddChildToOverlay(Mark);
	MarkSlot->SetHorizontalAlignment(HAlign_Center);
	MarkSlot->SetVerticalAlignment(VAlign_Center);
	UTextBlock *Worn = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	UOverlaySlot *WornSlot = Layers->AddChildToOverlay(Worn);
	WornSlot->SetHorizontalAlignment(HAlign_Left);
	WornSlot->SetVerticalAlignment(VAlign_Top);
	UTextBlock *Count = MakeText(WidgetTree, SmallFontSize, TextColor, true);
	UOverlaySlot *CountSlot = Layers->AddChildToOverlay(Count);
	CountSlot->SetHorizontalAlignment(HAlign_Right);
	CountSlot->SetVerticalAlignment(VAlign_Bottom);

	UWidget *Root = MakeSized(WidgetTree, TileSize, TileSize, Frame);
	TileGrid->AddChildToUniformGrid(Root, Index / Columns, Index % Columns);

	Widgets.Root = Root;
	Widgets.Frame = Frame;
	Widgets.Mark = Mark;
	Widgets.Worn = Worn;
	Widgets.Count = Count;
	return Widgets;
}

UInventoryScreenWidget::FChoiceWidgets UInventoryScreenWidget::BuildChoice(UVerticalBox *Column)
{
	using namespace InventoryScreenStyle;

	FChoiceWidgets Widgets;
	UVerticalBox *Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(ChoicePadding));
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *Note = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToRow(Line, Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Line, Note);
	UTextBlock *Reason = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	AddToColumn(Root, MakeSized(WidgetTree, ChoiceWidth, 0.0f, Frame))->SetHorizontalAlignment(HAlign_Left);
	AddToColumn(Root, Reason, FMargin(WordGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Column, Root, FMargin(0.0f, 0.0f, 0.0f, LineGap));

	Widgets.Root = Root;
	Widgets.Frame = Frame;
	Widgets.Label = Label;
	Widgets.Note = Note;
	Widgets.Reason = Reason;
	return Widgets;
}

void UInventoryScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// Every open starts on the first tab's first tile, with nothing pending.
	Tab = EInventoryScreenTab::Gear;
	Focus = EInventoryScreenFocus::Grid;
	SelectedTile = 0;
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
	UE_LOG(LogWoRUI, Log, TEXT("[InventoryScreen] Opened (%d member(s))"), Members.Num());
}

bool UInventoryScreenWidget::NativeOnHandleBackAction()
{
	if (Focus == EInventoryScreenFocus::Grid)
	{
		return Super::NativeOnHandleBackAction();
	}
	Focus = Focus == EInventoryScreenFocus::Receivers ? EInventoryScreenFocus::Verbs : EInventoryScreenFocus::Grid;
	Choice = 0;
	RefreshSelection();
	return true;
}

UWidget *UInventoryScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UInventoryScreenWidget *>(this);
}

FName UInventoryScreenWidget::MemberKey() const
{
	return Members.IsValidIndex(MemberIndex) ? Members[MemberIndex].Key : NAME_None;
}

const FInventoryScreenTile *UInventoryScreenWidget::FocusedTile() const
{
	return Page.Tiles.IsValidIndex(SelectedTile) ? &Page.Tiles[SelectedTile] : nullptr;
}

int32 UInventoryScreenWidget::ChoiceCount() const
{
	return Focus == EInventoryScreenFocus::Receivers ? Receivers.Num() : Verbs.Num();
}

void UInventoryScreenWidget::RefreshData()
{
	using namespace InventoryScreenStyle;

	UWorld *World = GetWorld();
	Members = WoRLoadoutScreen::BuildMembers(World);
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	Page = WoRInventoryScreen::BuildPage(World, MemberKey(), Tab);
	SelectedTile = Page.Tiles.Num() > 0 ? FMath::Clamp(SelectedTile, 0, Page.Tiles.Num() - 1) : 0;

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
	const TArray<EInventoryScreenTab> &Tabs = WoRInventoryScreen::GetTabs();
	for (int32 Index = 0; Index < PageTabs.Num() && Index < Tabs.Num(); ++Index)
	{
		PageTabs[Index]->SetBrushColor(Tabs[Index] == Tab ? FocusColor : PlateColor);
		PageTabLabels[Index]->SetColorAndOpacity(FSlateColor(Tabs[Index] == Tab ? TextColor : DimColor));
	}
	if (!TileGrid || !HeadingText || !EmptyText)
	{
		return; // headless: no tree
	}
	HeadingText->SetText(Page.Heading);
	SetShown(HeadingText, !Page.Heading.IsEmpty());
	EmptyText->SetText(Page.EmptyNote);
	SetShown(EmptyText, !Page.EmptyNote.IsEmpty());

	// The pouch also draws its empty slots, then the ones the Jeweler still sells.
	const int32 Shown = FMath::Max(Page.Tiles.Num(), Page.Slots) + Page.LockedSlots;
	while (TileWidgets.Num() < Shown)
	{
		TileWidgets.Add(BuildTile(TileWidgets.Num()));
	}
	for (int32 Index = 0; Index < TileWidgets.Num(); ++Index)
	{
		const FTileWidgets &Widgets = TileWidgets[Index];
		SetShown(Widgets.Root.Get(), Index < Shown);
		if (Index >= Shown || !Widgets.Mark.IsValid() || !Widgets.Worn.IsValid() || !Widgets.Count.IsValid())
		{
			continue;
		}
		const bool bTile = Page.Tiles.IsValidIndex(Index);
		const bool bLocked = !bTile && Index >= Page.Slots;
		const FInventoryScreenTile Tile = bTile ? Page.Tiles[Index] : FInventoryScreenTile();
		Widgets.Mark->SetText(Tile.Mark);
		Widgets.Mark->SetColorAndOpacity(FSlateColor(MarkColor(Tile.Element)));
		Widgets.Worn->SetText(bLocked ? LOCTEXT("LockedSlot", "Jeweler") : Tile.bWorn ? LOCTEXT("WornMark", "E") : FText::GetEmpty());
		Widgets.Worn->SetColorAndOpacity(FSlateColor(bLocked ? OffColor : CyanColor));
		Widgets.Count->SetText(bTile && !Tile.Row.IsGear() ? FText::Format(LOCTEXT("StackCount", "x{0}"), FText::AsNumber(Tile.Row.Count)) : FText::GetEmpty());
	}
}

void UInventoryScreenWidget::RefreshSelection(bool bScrollIntoView)
{
	using namespace InventoryScreenStyle;

	UWorld *World = GetWorld();
	const FInventoryScreenTile *Focused = FocusedTile();
	const FInventoryScreenTile Tile = Focused ? *Focused : FInventoryScreenTile();
	Verbs = Focused ? WoRInventoryScreen::BuildVerbs(World, MemberKey(), Tab, Tile) : TArray<FInventoryScreenVerb>();
	Receivers = Focused && Focus == EInventoryScreenFocus::Receivers ? WoRInventoryScreen::BuildReceivers(World, MemberKey(), Tile)
																	   : TArray<FInventoryScreenReceiver>();
	if (ChoiceCount() == 0)
	{
		Focus = EInventoryScreenFocus::Grid; // nothing to choose from: the keys go back to the tiles
	}
	Choice = ChoiceCount() > 0 ? FMath::Clamp(Choice, 0, ChoiceCount() - 1) : 0;

	for (int32 Index = 0; Index < TileWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = TileWidgets[Index].Frame.Get())
		{
			const bool bTile = Page.Tiles.IsValidIndex(Index);
			const bool bLocked = !bTile && Index >= Page.Slots;
			Frame->SetBrushColor(bTile && Index == SelectedTile ? FocusColor : bTile ? PlateColor : bLocked ? PanelColor : TrackColor);
		}
	}
	if (!DetailName)
	{
		return; // headless: no tree
	}
	DetailName->SetText(Focused && !Tile.Tier.IsEmpty() ? FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Tile.Name, Tile.Tier) : Tile.Name);
	DetailKindLine->SetText(Tile.KindLine);
	for (int32 Index = 0; Index < DetailLabels.Num(); ++Index)
	{
		const bool bShown = Tile.Details.IsValidIndex(Index);
		SetShown(DetailLabels[Index], bShown);
		SetShown(DetailValues[Index], bShown);
		if (bShown)
		{
			DetailLabels[Index]->SetText(Tile.Details[Index].Label);
			DetailValues[Index]->SetText(Tile.Details[Index].Value);
		}
	}
	DetailNote->SetText(Tile.Note);
	SetShown(DetailNote, !Tile.Note.IsEmpty());

	// The verb menu, or "Give to" in its place.
	const bool bGiving = Focus == EInventoryScreenFocus::Receivers;
	SetShown(ChoiceHeading, bGiving);
	for (int32 Index = 0; Index < ChoiceWidgets.Num(); ++Index)
	{
		const FChoiceWidgets &Widgets = ChoiceWidgets[Index];
		const bool bShown = Index < ChoiceCount();
		SetShown(Widgets.Root.Get(), bShown);
		if (!bShown || !Widgets.Frame.IsValid() || !Widgets.Label.IsValid() || !Widgets.Note.IsValid() || !Widgets.Reason.IsValid())
		{
			continue;
		}
		const FText Label = bGiving ? Receivers[Index].Name : Verbs[Index].Name;
		const FText Note = bGiving ? Receivers[Index].Note : FText::GetEmpty();
		const FText Reason = bGiving ? Receivers[Index].Refusal : Verbs[Index].Refusal;
		const bool bLit = Focus != EInventoryScreenFocus::Grid && Index == Choice;
		Widgets.Frame->SetBrushColor(bLit ? FocusColor : PlateColor);
		Widgets.Label->SetText(Label);
		Widgets.Label->SetColorAndOpacity(FSlateColor(Reason.IsEmpty() ? TextColor : OffColor));
		Widgets.Note->SetText(Note);
		Widgets.Note->SetColorAndOpacity(FSlateColor(Reason.IsEmpty() ? DimColor : WarnColor));
		Widgets.Reason->SetText(Reason);
		SetShown(Widgets.Reason.Get(), !Reason.IsEmpty());
	}
	const bool bStack = Focused && !Tile.Row.IsGear();
	SetShown(WholeStackText, bStack);
	WholeStackText->SetText(bWholeStack ? LOCTEXT("WholeStackOn", "X  Whole stack: on") : LOCTEXT("WholeStackOff", "X  Whole stack: off (one at a time)"));
	StatusText->SetText(Status);
	StatusText->SetColorAndOpacity(FSlateColor(bStatusIsRefusal ? WarnColor : GoodColor));
	SetShown(StatusText, !Status.IsEmpty());

	if (bScrollIntoView && TileScroll && TileWidgets.IsValidIndex(SelectedTile))
	{
		if (UWidget *TileRoot = TileWidgets[SelectedTile].Root.Get())
		{
			TileScroll->ScrollWidgetIntoView(TileRoot, /*AnimateScroll=*/false);
		}
	}
}

void UInventoryScreenWidget::MoveTile(int32 DeltaColumn, int32 DeltaRow)
{
	using namespace InventoryScreenStyle;

	const int32 Column = SelectedTile % Columns + DeltaColumn;
	const int32 Target = SelectedTile + DeltaColumn + DeltaRow * Columns;
	if (Column >= 0 && Column < Columns)
	{
		SelectTile(Target);
	}
}

void UInventoryScreenWidget::SelectTile(int32 Index, bool bScrollIntoView)
{
	if (Page.Tiles.IsValidIndex(Index) && (Index != SelectedTile || Focus != EInventoryScreenFocus::Grid))
	{
		SelectedTile = Index;
		Focus = EInventoryScreenFocus::Grid;
		Status = FText::GetEmpty();
		RefreshSelection(bScrollIntoView);
	}
}

void UInventoryScreenWidget::MoveTab(int32 Delta)
{
	const TArray<EInventoryScreenTab> &Tabs = WoRInventoryScreen::GetTabs();
	const int32 Index = ((Tabs.IndexOfByKey(Tab) + Delta) % Tabs.Num() + Tabs.Num()) % Tabs.Num();
	Tab = Tabs[Index];
	Focus = EInventoryScreenFocus::Grid;
	SelectedTile = 0;
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void UInventoryScreenWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	Focus = EInventoryScreenFocus::Grid;
	SelectedTile = 0;
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void UInventoryScreenWidget::MoveChoice(int32 Delta)
{
	if (Focus != EInventoryScreenFocus::Grid && ChoiceCount() > 0)
	{
		Choice = ((Choice + Delta) % ChoiceCount() + ChoiceCount()) % ChoiceCount();
		RefreshSelection();
	}
}

void UInventoryScreenWidget::Confirm()
{
	if (!FocusedTile())
	{
		return;
	}
	switch (Focus)
	{
	case EInventoryScreenFocus::Grid:
	{
		// Onto the first verb that can run (else the first, which says why it can't).
		const int32 FirstAllowed = Verbs.IndexOfByPredicate([](const FInventoryScreenVerb &Verb) { return Verb.Refusal.IsEmpty(); });
		Focus = EInventoryScreenFocus::Verbs;
		Choice = FMath::Max(0, FirstAllowed);
		Status = FText::GetEmpty();
		RefreshSelection();
		break;
	}
	case EInventoryScreenFocus::Verbs:
	{
		if (!Verbs.IsValidIndex(Choice))
		{
			return;
		}
		const FInventoryScreenVerb Verb = Verbs[Choice];
		if (!Verb.Refusal.IsEmpty())
		{
			Status = Verb.Refusal;
			bStatusIsRefusal = true;
			RefreshSelection();
		}
		else if (Verb.Verb == EInventoryVerb::Give)
		{
			Focus = EInventoryScreenFocus::Receivers;
			Receivers = WoRInventoryScreen::BuildReceivers(GetWorld(), MemberKey(), *FocusedTile());
			Choice = FMath::Max(0, Receivers.IndexOfByPredicate([](const FInventoryScreenReceiver &Receiver) { return Receiver.Refusal.IsEmpty(); }));
			Status = FText::GetEmpty();
			RefreshSelection();
		}
		else
		{
			RunVerb(Verb.Verb, nullptr);
		}
		break;
	}
	case EInventoryScreenFocus::Receivers:
		if (!Receivers.IsValidIndex(Choice))
		{
			return;
		}
		if (!Receivers[Choice].Refusal.IsEmpty())
		{
			Status = Receivers[Choice].Refusal;
			bStatusIsRefusal = true;
			RefreshSelection();
		}
		else
		{
			const FInventoryScreenReceiver Receiver = Receivers[Choice];
			RunVerb(EInventoryVerb::Give, &Receiver);
		}
		break;
	}
}

void UInventoryScreenWidget::RunVerb(EInventoryVerb Verb, const FInventoryScreenReceiver *Receiver)
{
	const FInventoryScreenTile *Focused = FocusedTile();
	if (!Focused)
	{
		return;
	}
	const FInventoryScreenTile Tile = *Focused;
	const int32 Count = WoRInventoryScreen::GetMoveCount(Tile, bWholeStack);
	const FText Refusal = WoRInventoryVerbs::Run(GetWorld(), Verb, MemberKey(), Receiver ? Receiver->Key : NAME_None, Tile.Row, Count);
	bStatusIsRefusal = !Refusal.IsEmpty();
	if (bStatusIsRefusal)
	{
		Status = Refusal;
	}
	else
	{
		const FText What = Tile.Row.IsGear() ? Tile.Name : FText::Format(LOCTEXT("CountAndName", "{0} x {1}"), FText::AsNumber(Count), Tile.Name);
		switch (Verb)
		{
		case EInventoryVerb::Give:
			Status = FText::Format(LOCTEXT("DoneGive", "Gave {0} to {1}."), What, Receiver ? Receiver->Name : FText::GetEmpty());
			break;
		case EInventoryVerb::Store:
			Status = FText::Format(LOCTEXT("DoneStore", "Stored {0}."), What);
			break;
		default:
			Status = FText::Format(LOCTEXT("DoneTake", "Took {0}."), What);
			break;
		}
	}
	UE_LOG(LogWoRUI, Log, TEXT("[InventoryScreen] %s"), *Status.ToString());
	Focus = EInventoryScreenFocus::Grid;
	RefreshData();
	RefreshSelection();
}

void UInventoryScreenWidget::ToggleWholeStack()
{
	bWholeStack = !bWholeStack;
	RefreshSelection(/*bScrollIntoView=*/false);
}

FReply UInventoryScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	const bool bGrid = Focus == EInventoryScreenFocus::Grid;
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		const int32 Delta = IsDownKey(Key) ? 1 : -1;
		bGrid ? MoveTile(0, Delta) : MoveChoice(Delta);
		return FReply::Handled();
	}
	if (IsLeftKey(Key) || IsRightKey(Key))
	{
		if (bGrid)
		{
			MoveTile(IsRightKey(Key) ? 1 : -1, 0);
		}
		return FReply::Handled();
	}
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled(); // the keys below act once per press
	}
	if (IsConfirmKey(Key))
	{
		Confirm();
		return FReply::Handled();
	}
	if (IsToggleKey(Key))
	{
		ToggleWholeStack();
		return FReply::Handled();
	}
	if (IsTabPrevKey(Key) || IsTabNextKey(Key))
	{
		MoveTab(IsTabNextKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsBackKey(Key))
	{
		// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
		HandleBackAction();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UInventoryScreenWidget::FindTileUnder(const FPointerEvent &MouseEvent) const
{
	// A tile scrolled out of view keeps its geometry past the grid's edge: only the grid's own area counts.
	if (!TileScroll || !TileScroll->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < TileWidgets.Num() && Index < Page.Tiles.Num(); ++Index)
	{
		const UBorder *Frame = TileWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UInventoryScreenWidget::FindChoiceUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < ChoiceWidgets.Num() && Index < ChoiceCount(); ++Index)
	{
		const UBorder *Frame = ChoiceWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UInventoryScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		const int32 OverChoice = FindChoiceUnder(InMouseEvent);
		if (Focus == EInventoryScreenFocus::Grid)
		{
			const int32 OverTile = FindTileUnder(InMouseEvent);
			if (OverTile != SelectedTile)
			{
				SelectTile(OverTile, /*bScrollIntoView=*/false); // the grid mustn't move under the cursor
			}
		}
		else if (OverChoice != INDEX_NONE && OverChoice != Choice)
		{
			Choice = OverChoice;
			RefreshSelection(/*bScrollIntoView=*/false);
		}
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UInventoryScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	for (int32 Index = 0; Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
	{
		if (MemberTabs[Index] && MemberTabs[Index]->GetCachedGeometry().IsUnderLocation(Position))
		{
			MoveMember(Index - MemberIndex);
			return FReply::Handled();
		}
	}
	const TArray<EInventoryScreenTab> &Tabs = WoRInventoryScreen::GetTabs();
	for (int32 Index = 0; Index < PageTabs.Num() && Index < Tabs.Num(); ++Index)
	{
		if (PageTabs[Index] && PageTabs[Index]->GetCachedGeometry().IsUnderLocation(Position))
		{
			MoveTab(Index - Tabs.IndexOfByKey(Tab));
			return FReply::Handled();
		}
	}
	const int32 OverChoice = FindChoiceUnder(InMouseEvent);
	if (OverChoice != INDEX_NONE)
	{
		// A click on a line chooses it, as moving onto it and pressing A does.
		if (Focus == EInventoryScreenFocus::Grid)
		{
			Focus = EInventoryScreenFocus::Verbs;
		}
		Choice = OverChoice;
		Confirm();
		return FReply::Handled();
	}
	const int32 OverTile = FindTileUnder(InMouseEvent);
	if (OverTile != INDEX_NONE)
	{
		SelectTile(OverTile, /*bScrollIntoView=*/false);
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GInventoryScreenOpenCommand(
	TEXT("wor.UI.InventoryScreen"),
	TEXT("B-441: open the inventory screen (Gear, Pouch, Chest; Give, Store, Take), as the game menu's Inventory page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UInventoryScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[InventoryScreen] wor.UI.InventoryScreen: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
