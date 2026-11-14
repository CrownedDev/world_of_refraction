// PartyScreenWidget.cpp

#include "UI/Party/PartyScreenWidget.h"

#include "Combat/Grid/CombatGridConstants.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Coop/InviteScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPartyScreenWidget"

namespace PartyScreenStyle
{
	// This screen's own layout (screen_mockups/menus/party, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 960.0f;
	constexpr float RowHeight = 126.0f;
	constexpr float InviteRowHeight = 72.0f;
	constexpr float RowGap = 15.0f;
	constexpr float RowPadding = 21.0f;
	constexpr float ValueWidth = 270.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 690.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float HeaderGap = 36.0f;
	constexpr float CellSize = 120.0f;
	constexpr float CellGap = 9.0f;
	inline const FLinearColor EmptyCellColor = TrackColor;
}

UPartyScreenWidget::UPartyScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UPartyScreenWidget *UPartyScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[PartyScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UPartyScreenWidget>(Root->PushToMenu(UPartyScreenWidget::StaticClass()));
}

TSharedRef<SWidget> UPartyScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPartyScreenWidget::BuildTree()
{
	using namespace PartyScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	SubtitleText = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	AddToColumn(Main, TitleText);
	AddToColumn(Main, SubtitleText, FMargin(0.0f, LineGap, 0.0f, HeaderGap));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(ListColumn, MessageText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(ListColumn, RowList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);
	for (int32 SlotIndex = 0; SlotIndex < PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex)
	{
		RowWidgets.Add(BuildRow());
	}
	InviteRow = BuildRow();
	InviteRow.Name->SetText(LOCTEXT("Invite", "Invite"));
	SetShown(InviteRow.Line.Get(), false);
	if (USizeBox *InviteBox = Cast<USizeBox>(InviteRow.Frame->GetParent()))
	{
		InviteBox->SetHeightOverride(InviteRowHeight);
	}

	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	UTextBlock *GridHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	GridHeading->SetText(LOCTEXT("StartingPlaces", "Starting places"));
	AddToColumn(PanelColumn, GridHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	BuildGrid(PanelColumn);
	PanelName = MakeText(WidgetTree, NameFontSize, TextColor, true);
	PanelGuard = MakeText(WidgetTree, LineFontSize, CyanColor);
	PanelSeat = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, PanelName, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, PanelGuard, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, PanelSeat, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// TODO: B-458, "Y Swap order" (mockup) waits on a party reorder (Q-458).
	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "A  Change        X  Move on grid        B / Esc  Back"));
	AddToColumn(Main, Hints, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
}

UPartyScreenWidget::FRowWidgets UPartyScreenWidget::BuildRow()
{
	using namespace PartyScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *Detail = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Words, Name);
	AddToColumn(Words, Detail, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Line, Words, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Value = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	Value->SetJustification(ETextJustify::Right);
	AddToRow(Line, MakeSized(WidgetTree, ValueWidth, 0.0f, Value), FMargin(0.0f), VAlign_Center);
	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(RowList->AddChild(MakeSized(WidgetTree, ListWidth, RowHeight, Frame))))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.Line = Detail;
	Widgets.Value = Value;
	return Widgets;
}

void UPartyScreenWidget::BuildGrid(UVerticalBox *Column)
{
	using namespace PartyScreenStyle;
	using namespace CombatGridConstants;

	// Drawn columns are the combat rows, Back on the left, Front by the enemies (mockup 1_party).
	UHorizontalBox *Heads = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 RowIndex = 0; RowIndex < GRID_ROWS; ++RowIndex)
	{
		UTextBlock *Head = MakeText(WidgetTree, SmallFontSize, DimColor);
		Head->SetText(WoRGameNames::GetCombatRowName(static_cast<ECombatRow>(RowIndex)));
		Head->SetJustification(ETextJustify::Center);
		AddToRow(Heads, MakeSized(WidgetTree, CellSize, 0.0f, Head), FMargin(0.0f, 0.0f, CellGap, 0.0f));
	}
	UTextBlock *Enemies = MakeText(WidgetTree, SmallFontSize, OffColor);
	Enemies->SetText(LOCTEXT("Enemies", "enemies ►"));
	AddToRow(Heads, Enemies, FMargin(CellGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
	AddToColumn(Column, Heads, FMargin(0.0f, 0.0f, 0.0f, LineGap));

	GridCells.SetNum(POSITIONS_PER_TEAM);
	GridLabels.SetNum(POSITIONS_PER_TEAM);
	for (int32 Line = 0; Line < GRID_COLUMNS; ++Line)
	{
		UHorizontalBox *Cells = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 RowIndex = 0; RowIndex < GRID_ROWS; ++RowIndex)
		{
			UBorder *Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Cell->SetBrushColor(EmptyCellColor);
			Cell->SetHorizontalAlignment(HAlign_Center);
			Cell->SetVerticalAlignment(VAlign_Center);
			UTextBlock *Label = MakeText(WidgetTree, SmallFontSize, TextColor, true);
			Cell->SetContent(Label);
			AddToRow(Cells, MakeSized(WidgetTree, CellSize, CellSize, Cell), FMargin(0.0f, 0.0f, CellGap, 0.0f));
			GridCells[Line * GRID_ROWS + RowIndex] = Cell;
			GridLabels[Line * GRID_ROWS + RowIndex] = Label;
		}
		AddToColumn(Column, Cells, FMargin(0.0f, 0.0f, 0.0f, CellGap));
	}
}

void UPartyScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[PartyScreen] Opened (%d member(s))"), Page.Members.Num());
}

UWidget *UPartyScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UPartyScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UPartyScreenWidget::RefreshData()
{
	using namespace PartyScreenStyle;

	Page = WoRPartyScreen::BuildPage(GetWorld());
	SelectedRow = FMath::Clamp(SelectedRow, 0, RowCount() - 1);
	if (TitleText)
	{
		TitleText->SetText(Page.Title);
		SubtitleText->SetText(Page.Subtitle);
		MessageText->SetText(Message);
		SetShown(MessageText, !Message.IsEmpty());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = RowWidgets[Index];
		const bool bShown = Page.Members.IsValidIndex(Index);
		if (UBorder *Frame = Widgets.Frame.Get())
		{
			SetShown(Frame->GetParent(), bShown);
		}
		if (!bShown || !Widgets.Name.IsValid() || !Widgets.Line.IsValid() || !Widgets.Value.IsValid())
		{
			continue;
		}
		const FPartyScreenMember &Member = Page.Members[Index];
		Widgets.Name->SetText(Member.bFallen ? FText::Format(LOCTEXT("FallenName", "{0}  (fallen)"), Member.Name) : Member.Name);
		Widgets.Line->SetText(FText::Format(LOCTEXT("MemberLine", "{0}\n{1}    {2}"), Member.ClassName, Member.StartLine, Member.FieldSkillLine));
		Widgets.Value->SetText(FText::Format(LOCTEXT("PlayedBy", "Played by {0}"), Member.PlayedBy));
	}
	if (InviteRow.Value.IsValid())
	{
		InviteRow.Value->SetText(Page.InviteStatus);
	}
	RefreshSelection();
}

void UPartyScreenWidget::RefreshSelection()
{
	using namespace PartyScreenStyle;
	using namespace CombatGridConstants;

	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = RowWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		}
	}
	if (UBorder *Frame = InviteRow.Frame.Get())
	{
		Frame->SetBrushColor(IsInviteRow(SelectedRow) ? FocusColor : PlateColor);
	}

	for (int32 Cell = 0; Cell < GridCells.Num(); ++Cell)
	{
		if (UBorder *Border = GridCells[Cell].Get())
		{
			Border->SetBrushColor(EmptyCellColor);
		}
		if (UTextBlock *Label = GridLabels[Cell].Get())
		{
			Label->SetText(FText::GetEmpty());
		}
	}
	for (int32 Index = 0; Index < Page.Members.Num(); ++Index)
	{
		const FPartyScreenMember &Member = Page.Members[Index];
		const int32 Cell = Member.Column * GRID_ROWS + static_cast<int32>(Member.Row);
		if (!Member.bHasCell || !GridCells.IsValidIndex(Cell))
		{
			continue; // placed for you at the fight's start
		}
		if (UBorder *Border = GridCells[Cell].Get())
		{
			Border->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		}
		if (UTextBlock *Label = GridLabels[Cell].Get())
		{
			Label->SetText(Member.Name);
		}
	}

	if (!PanelName)
	{
		return; // headless
	}
	const bool bMember = Page.Members.IsValidIndex(SelectedRow);
	SetShown(PanelGuard, bMember);
	SetShown(PanelSeat, true);
	if (!bMember)
	{
		PanelName->SetText(LOCTEXT("InviteHeading", "Invite"));
		PanelSeat->SetText(LOCTEXT("InviteNote", "Host a session and invite a friend to take a seat."));
		if (RowList && InviteRow.Frame.IsValid())
		{
			RowList->ScrollWidgetIntoView(InviteRow.Frame.Get(), /*AnimateScroll=*/false);
		}
		return;
	}
	const FPartyScreenMember &Member = Page.Members[SelectedRow];
	PanelName->SetText(Member.Name);
	PanelGuard->SetText(FText::Format(LOCTEXT("Guard", "Guard: {0}"), Member.GuardLine));
	PanelSeat->SetText(Member.bLeader ? LOCTEXT("LeaderNote", "The leader can't be given to the AI.")
										: LOCTEXT("SeatNote", "If a friend joins: they can take this seat."));
	if (RowList && RowWidgets.IsValidIndex(SelectedRow) && RowWidgets[SelectedRow].Frame.IsValid())
	{
		RowList->ScrollWidgetIntoView(RowWidgets[SelectedRow].Frame.Get(), /*AnimateScroll=*/false);
	}
}

void UPartyScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = RowCount();
	SelectRow(((SelectedRow + Delta) % Count + Count) % Count);
}

void UPartyScreenWidget::SelectRow(int32 Index)
{
	if (Index < 0 || Index >= RowCount() || Index == SelectedRow)
	{
		return;
	}
	SelectedRow = Index;
	if (!Message.IsEmpty())
	{
		Message = FText::GetEmpty(); // a refusal belongs to the row it was about
		WoRMenuStyle::SetShown(MessageText, false);
	}
	RefreshSelection();
}

void UPartyScreenWidget::PressChange()
{
	if (IsInviteRow(SelectedRow))
	{
		if (!UInviteScreenWidget::Open(GetOwningPlayer())) // B-459b, over this screen: Back returns here
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[PartyScreen] Invite: Play together didn't open"));
		}
		return;
	}
	Message = WoRPartyScreen::ToggleSeat(GetWorld(), SelectedRow);
	RefreshData();
}

void UPartyScreenWidget::PressMoveOnGrid()
{
	if (Page.Members.IsValidIndex(SelectedRow))
	{
		Message = FText::GetEmpty();
		WoRPartyScreen::MoveOnGrid(GetWorld(), SelectedRow);
		RefreshData();
	}
}

FReply UPartyScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsToggleKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressChange() : PressMoveOnGrid();
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

int32 UPartyScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	if (!RowList || !RowList->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < RowWidgets.Num() && Index < Page.Members.Num(); ++Index)
	{
		const UBorder *Frame = RowWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	const UBorder *Invite = InviteRow.Frame.Get();
	return Invite && Invite->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()) ? Page.Members.Num() : INDEX_NONE;
}

FReply UPartyScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPartyScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const int32 Row = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindRowUnder(InMouseEvent) : INDEX_NONE;
	if (Row == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	// The first click lights a row, a click on the lit row is A: a stray click doesn't flip a seat.
	if (Row != SelectedRow)
	{
		SelectRow(Row);
	}
	else
	{
		PressChange();
	}
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPartyScreenOpenCommand(
	TEXT("wor.UI.Party"),
	TEXT("B-458: open the party screen (who plays each member, start cells; A change, X move on grid)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UPartyScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[PartyScreen] wor.UI.Party: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
