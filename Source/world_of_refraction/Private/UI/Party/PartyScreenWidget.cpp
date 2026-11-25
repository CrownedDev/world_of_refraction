// PartyScreenWidget.cpp

#include "UI/Party/PartyScreenWidget.h"

#include "Algo/IndexOf.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRHubController.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPartyScreenWidget"

namespace PartyScreenStyle
{
	// B-628 (mockup menus/party/v2, 1280x720 shown at 1080p): the loadout screen's three columns (v5); the shared look
	// is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float LeftWidth = 600.0f;
	constexpr float MiddleWidth = 618.0f;
	constexpr float RightWidth = 504.0f;
	constexpr float PanelGap = 27.0f;
	constexpr float PanelPadding = 24.0f;
	constexpr float HeaderGap = 27.0f;
	constexpr float CardPadding = 18.0f;
	constexpr float CardGap = 15.0f;
	constexpr float EntryPadding = 15.0f;
	constexpr float EntryGap = 12.0f;
	constexpr float SeatWidth = 120.0f;
	constexpr float CellSize = 78.0f;
	constexpr float CellGap = 6.0f;
	inline const FLinearColor EmptyCellColor = TrackColor;
	constexpr int32 BUTTON_COUNT = static_cast<int32>(UPartyScreenWidget::EPartyButton::Count);
}

UPartyScreenWidget::UPartyScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its cards are plain borders
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

UBorder *UPartyScreenWidget::MakePanel(UVerticalBox *&OutColumn) const
{
	using namespace PartyScreenStyle;
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	OutColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(OutColumn);
	return Panel;
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
	AddToColumn(Main, SubtitleText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(Main, MessageText, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox *Left = nullptr;
	UVerticalBox *Middle = nullptr;
	UVerticalBox *Right = nullptr;
	AddToRow(Body, MakeSized(WidgetTree, LeftWidth, 0.0f, MakePanel(Left)), FMargin(0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, MiddleWidth, 0.0f, MakePanel(Middle)), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, RightWidth, 0.0f, MakePanel(Right)), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// Left: the seats. No co-op line: who plays each seat already says it (Crown, mockup v2).
	UHorizontalBox *LeftHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *PartyHeading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	PartyHeading->SetText(LOCTEXT("PartyHeading", "Party"));
	AddToRow(LeftHead, PartyHeading, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CountText = MakeText(WidgetTree, SmallFontSize, DimColor, true);
	AddToRow(LeftHead, CountText, FMargin(0.0f), VAlign_Center);
	AddToColumn(Left, LeftHead);
	UTextBlock *Recruit = MakeText(WidgetTree, SmallFontSize, DimColor);
	Recruit->SetText(LOCTEXT("Recruit", "recruit in the hub or a trial"));
	AddToColumn(Left, Recruit, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	SeatList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(Left, SeatList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	for (int32 SlotIndex = 0; SlotIndex < PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex)
	{
		SeatWidgets.Add(BuildSeat());
	}

	BuildMiddle(Middle);
	BuildRight(Right);

	UTextBlock *Hints = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{Confirm}  Move start        ‹ ›  Played by        {Second}  Loadout        {Toggle}  Dismiss        {Back}  Back"));
	AddToColumn(Main, Hints, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
}

UPartyScreenWidget::FSeatWidgets UPartyScreenWidget::BuildSeat()
{
	using namespace PartyScreenStyle;

	FSeatWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(CardPadding));
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *ClassLine = MakeText(WidgetTree, SmallFontSize, DimColor);
	UTextBlock *PlayedBy = MakeText(WidgetTree, LineFontSize, TextColor, true);
	AddToColumn(Words, Name);
	AddToColumn(Words, ClassLine, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(Words, PlayedBy, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Line, Words, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Seat = MakeText(WidgetTree, SmallFontSize, OffColor, true);
	Seat->SetJustification(ETextJustify::Right);
	AddToRow(Line, MakeSized(WidgetTree, SeatWidth, 0.0f, Seat), FMargin(0.0f), VAlign_Top);
	if (UScrollBoxSlot *CardSlot = Cast<UScrollBoxSlot>(SeatList->AddChild(Frame)))
	{
		CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, CardGap));
	}
	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.ClassLine = ClassLine;
	Widgets.PlayedBy = PlayedBy;
	Widgets.Seat = Seat;
	return Widgets;
}

void UPartyScreenWidget::BuildMiddle(UVerticalBox *Column)
{
	using namespace PartyScreenStyle;

	// TODO: B-628, the mockup's HP / EP top right: GetMemberVitals gives the current values only, no maximum.
	MiddleName = MakeText(WidgetTree, NameFontSize, TextColor, true);
	MiddleClass = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Column, MiddleName);
	AddToColumn(Column, MiddleClass, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	UHorizontalBox *LoadoutHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LoadoutTitle = MakeText(WidgetTree, LineFontSize, TextColor, true);
	LoadoutTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	AddToRow(LoadoutHead, LoadoutTitle, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LoadoutInUse = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	LoadoutInUse->SetText(LOCTEXT("InUse", "✓ in use"));
	AddToRow(LoadoutHead, LoadoutInUse, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
	AddToColumn(Column, LoadoutHead, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	EntryList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(Column, EntryList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

void UPartyScreenWidget::BuildRight(UVerticalBox *Column)
{
	using namespace PartyScreenStyle;

	UTextBlock *Heading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Heading->SetText(LOCTEXT("Details", "Details"));
	AddToColumn(Column, Heading);
	RightName = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Column, RightName, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	auto AddFact = [this, Column](const FText &Label, const FLinearColor &ValueColor) -> UTextBlock *
	{
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, LineFontSize, DimColor);
		LabelText->SetText(Label);
		AddToRow(Row, LabelText, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, ValueColor, true);
		Value->SetJustification(ETextJustify::Right);
		AddToRow(Row, Value, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToColumn(Column, Row, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		return Value;
	};
	PlayedByValue = AddFact(LOCTEXT("PlayedByLabel", "Played by"), CyanColor);
	StartsValue = AddFact(LOCTEXT("StartsLabel", "Starts"), TextColor);
	BuildGrid(Column);

	for (int32 Index = 0; Index < BUTTON_COUNT; ++Index)
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
		AddToColumn(Column, Plate, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		Buttons[Index] = {Plate, Verb, Reason};
	}
	RightNote = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	RightNote->SetText(LOCTEXT("Note", "An AI seat defends itself and plays its own turns. The leader can't be dismissed."));
	AddToColumn(Column, RightNote, FMargin(0.0f, LineGap, 0.0f, 0.0f));
}

void UPartyScreenWidget::BuildGrid(UVerticalBox *Column)
{
	using namespace PartyScreenStyle;
	using namespace CombatGridConstants;

	// Drawn columns are the combat rows, Back on the left, Front by the enemies (mockups 1_party, party/v2).
	UHorizontalBox *Grid = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox *Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
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
			Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			Cell->SetContent(Label);
			AddToRow(Cells, MakeSized(WidgetTree, CellSize, CellSize, Cell), FMargin(0.0f, 0.0f, CellGap, 0.0f));
			GridCells[Line * GRID_ROWS + RowIndex] = Cell;
			GridLabels[Line * GRID_ROWS + RowIndex] = Label;
		}
		AddToColumn(Lines, Cells, FMargin(0.0f, 0.0f, 0.0f, CellGap));
	}
	UHorizontalBox *Heads = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 RowIndex = 0; RowIndex < GRID_ROWS; ++RowIndex)
	{
		UTextBlock *Head = MakeText(WidgetTree, SmallFontSize, DimColor);
		Head->SetText(WoRGameNames::GetCombatRowName(static_cast<ECombatRow>(RowIndex)));
		Head->SetJustification(ETextJustify::Center);
		Head->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddToRow(Heads, MakeSized(WidgetTree, CellSize, 0.0f, Head), FMargin(0.0f, 0.0f, CellGap, 0.0f));
	}
	AddToColumn(Lines, Heads);
	AddToRow(Grid, Lines);
	UTextBlock *Enemies = MakeText(WidgetTree, SmallFontSize, OffColor);
	Enemies->SetText(LOCTEXT("Enemies", "enemies ►"));
	AddToRow(Grid, Enemies, FMargin(CellGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
	AddToColumn(Column, Grid, FMargin(0.0f, LineGap, 0.0f, SectionGap));
}

void UPartyScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshData(); // also on the way back from the loadout screen (Y): what changed there shows
	UE_LOG(LogWoRUI, Log, TEXT("[PartyScreen] Opened (%d member(s))"), Page.Members.Num());
}

UWidget *UPartyScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UPartyScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UPartyScreenWidget::SetMessage(const FText &Text)
{
	Message = Text;
	if (MessageText)
	{
		MessageText->SetText(Message);
		WoRMenuStyle::SetShown(MessageText, !Message.IsEmpty());
	}
}

void UPartyScreenWidget::DisarmDismiss()
{
	if (bDismissArmed)
	{
		bDismissArmed = false;
		RefreshButtons();
	}
}

void UPartyScreenWidget::RefreshData()
{
	using namespace PartyScreenStyle;

	Page = WoRPartyScreen::BuildPage(GetWorld());
	SelectedRow = FMath::Clamp(SelectedRow, 0, FMath::Max(Page.Members.Num() - 1, 0));
	if (TitleText)
	{
		TitleText->SetText(Page.Title);
		SubtitleText->SetText(Page.Subtitle);
		CountText->SetText(FText::Format(LOCTEXT("Count", "{0} of {1}"), Page.Members.Num(), PartyConstants::MAX_PARTY_MEMBERS));
	}
	SetMessage(Message);
	for (int32 Index = 0; Index < SeatWidgets.Num(); ++Index)
	{
		const FSeatWidgets &Widgets = SeatWidgets[Index];
		const bool bShown = Page.Members.IsValidIndex(Index);
		SetShown(Widgets.Frame.Get(), bShown);
		if (!bShown || !Widgets.Name.IsValid() || !Widgets.ClassLine.IsValid() || !Widgets.PlayedBy.IsValid() || !Widgets.Seat.IsValid())
		{
			continue;
		}
		const FPartyScreenMember &Member = Page.Members[Index];
		Widgets.Name->SetText(Member.bFallen ? FText::Format(LOCTEXT("FallenName", "{0}  (fallen)"), Member.Name) : Member.Name);
		Widgets.ClassLine->SetText(Member.ClassName);
		Widgets.PlayedBy->SetText(Member.PlayedBy);
		Widgets.Seat->SetText(FText::Format(LOCTEXT("Seat", "Seat {0}"), Index + 1));
	}
	RefreshSelection();
}

void UPartyScreenWidget::RefreshSelection()
{
	using namespace PartyScreenStyle;
	using namespace CombatGridConstants;

	for (int32 Index = 0; Index < SeatWidgets.Num(); ++Index)
	{
		const bool bLit = Index == SelectedRow;
		if (UBorder *Frame = SeatWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(bLit ? FocusColor : PlateColor);
		}
		if (UTextBlock *PlayedBy = SeatWidgets[Index].PlayedBy.Get())
		{
			PlayedBy->SetColorAndOpacity(FSlateColor(bLit ? CyanColor : TextColor));
		}
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

	RefreshLoadout();
	if (!RightName)
	{
		RefreshButtons();
		return; // headless
	}
	const bool bMember = Page.Members.IsValidIndex(SelectedRow);
	const FPartyScreenMember *Member = bMember ? &Page.Members[SelectedRow] : nullptr;
	MiddleName->SetText(Member ? Member->Name : FText::GetEmpty());
	MiddleClass->SetText(Member ? Member->ClassName : FText::GetEmpty());
	RightName->SetText(Member ? Member->Name : FText::GetEmpty());
	// The leader's seat stays yours (ToggleSeat): no arrows to turn.
	PlayedByValue->SetText(!Member ? FText::GetEmpty()
						   : Member->bLeader ? Member->PlayedBy
											 : FText::Format(LOCTEXT("PlayedByTurn", "‹  {0}  ›"), Member->PlayedBy));
	StartsValue->SetText(Member ? Member->StartValue : FText::GetEmpty());
	RefreshButtons();
	if (SeatList && SeatWidgets.IsValidIndex(SelectedRow) && SeatWidgets[SelectedRow].Frame.IsValid())
	{
		SeatList->ScrollWidgetIntoView(SeatWidgets[SelectedRow].Frame.Get(), /*AnimateScroll=*/false);
	}
}

void UPartyScreenWidget::RefreshLoadout()
{
	using namespace PartyScreenStyle;

	Loadout = Page.Members.IsValidIndex(SelectedRow) ? WoRPartyScreen::BuildLoadout(GetWorld(), SelectedRow) : FPartyScreenLoadout();
	if (!LoadoutTitle)
	{
		return; // headless
	}
	const bool bHasLoadout = !Loadout.Title.IsEmpty();
	LoadoutTitle->SetText(bHasLoadout ? Loadout.Title : LOCTEXT("NoLoadout", "No loadout here"));
	LoadoutTitle->SetColorAndOpacity(FSlateColor(bHasLoadout ? TextColor : DimColor));
	SetShown(LoadoutInUse, bHasLoadout);
	// Plates are made once, as many as the longest loadout shown so far, and refilled.
	while (EntryWidgets.Num() < Loadout.Entries.Num())
	{
		UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(PlateColor);
		Frame->SetPadding(FMargin(EntryPadding));
		UVerticalBox *Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Lines);
		UHorizontalBox *Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *Name = MakeText(WidgetTree, LineFontSize, TextColor, true);
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddToRow(Head, Name, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock *TypeLine = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToRow(Head, TypeLine, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToColumn(Lines, Head);
		UTextBlock *Skills = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Lines, Skills, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		if (UScrollBoxSlot *EntrySlot = Cast<UScrollBoxSlot>(EntryList->AddChild(Frame)))
		{
			EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, EntryGap));
		}
		EntryWidgets.Add({Frame, Name, TypeLine, Skills});
	}
	for (int32 Index = 0; Index < EntryWidgets.Num(); ++Index)
	{
		const FEntryWidgets &Widgets = EntryWidgets[Index];
		const bool bShown = Loadout.Entries.IsValidIndex(Index);
		SetShown(Widgets.Frame.Get(), bShown);
		if (!bShown || !Widgets.Name.IsValid() || !Widgets.TypeLine.IsValid() || !Widgets.Skills.IsValid())
		{
			continue;
		}
		const FPartyScreenLoadoutEntry &Entry = Loadout.Entries[Index];
		Widgets.Name->SetText(Entry.Name);
		Widgets.TypeLine->SetText(Entry.TypeLine);
		Widgets.Skills->SetText(Entry.SkillsLine);
		SetShown(Widgets.Skills.Get(), !Entry.SkillsLine.IsEmpty());
	}
	if (EntryList)
	{
		EntryList->ScrollToStart();
	}
}

FText UPartyScreenWidget::GetButtonRefusal(EPartyButton Button) const
{
	if (!Page.Members.IsValidIndex(SelectedRow))
	{
		return LOCTEXT("NoMember", "No member.");
	}
	switch (Button)
	{
	case EPartyButton::OpenLoadout:
		return Loadout.Title.IsEmpty() ? LOCTEXT("NoLoadoutHere", "No loadout here.") : FText::GetEmpty();
	case EPartyButton::Dismiss:
		return Page.Members[SelectedRow].bLeader ? LOCTEXT("LeaderStays", "The leader can't be dismissed.") : FText::GetEmpty();
	default:
		return FText::GetEmpty();
	}
}

void UPartyScreenWidget::RefreshButtons()
{
	using namespace PartyScreenStyle;

	const FText Name = Page.Members.IsValidIndex(SelectedRow) ? Page.Members[SelectedRow].Name : FText::GetEmpty();
	const FText Labels[BUTTON_COUNT] = {
		LOCTEXT("MoveStartButton", "{Confirm}  Move start place"),
		LOCTEXT("OpenLoadoutButton", "{Second}  Open loadout"),
		bDismissArmed ? FText::Format(LOCTEXT("DismissArmedButton", "{Toggle}  again: dismiss {0}"), Name) : LOCTEXT("DismissButton", "{Toggle}  Dismiss"),
	};
	for (int32 Index = 0; Index < BUTTON_COUNT; ++Index)
	{
		const FButtonWidgets &Widgets = Buttons[Index];
		if (!Widgets.Plate.IsValid() || !Widgets.Verb.IsValid() || !Widgets.Reason.IsValid())
		{
			continue; // headless
		}
		const FText Refusal = GetButtonRefusal(static_cast<EPartyButton>(Index));
		const bool bArmed = bDismissArmed && static_cast<EPartyButton>(Index) == EPartyButton::Dismiss;
		Widgets.Plate->SetBrushColor(bArmed ? FocusColor : PlateColor);
		WoRKeyHints::SetHint(Widgets.Verb.Get(), Labels[Index]);
		Widgets.Verb->SetColorAndOpacity(FSlateColor(Refusal.IsEmpty() ? TextColor : OffColor));
		Widgets.Reason->SetText(Refusal);
		SetShown(Widgets.Reason.Get(), !Refusal.IsEmpty());
	}
}

void UPartyScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = Page.Members.Num();
	if (Count > 0)
	{
		SelectRow(((SelectedRow + Delta) % Count + Count) % Count);
	}
}

void UPartyScreenWidget::SelectRow(int32 Index)
{
	if (!Page.Members.IsValidIndex(Index) || Index == SelectedRow)
	{
		return;
	}
	SelectedRow = Index;
	bDismissArmed = false;
	if (!Message.IsEmpty())
	{
		SetMessage(FText::GetEmpty()); // a refusal belongs to the member it was about
	}
	RefreshSelection();
}

void UPartyScreenWidget::PressToggleSeat()
{
	if (!Page.Members.IsValidIndex(SelectedRow))
	{
		return;
	}
	bDismissArmed = false;
	Message = WoRPartyScreen::ToggleSeat(GetWorld(), SelectedRow);
	UE_LOG(LogWoRUI, Log, TEXT("[PartyScreen] Seat %d: %s"), SelectedRow, Message.IsEmpty() ? TEXT("changed") : *Message.ToString());
	RefreshData();
}

void UPartyScreenWidget::PressMoveOnGrid()
{
	if (!Page.Members.IsValidIndex(SelectedRow))
	{
		return;
	}
	bDismissArmed = false;
	Message = FText::GetEmpty();
	WoRPartyScreen::MoveOnGrid(GetWorld(), SelectedRow);
	RefreshData();
}

void UPartyScreenWidget::PressOpenLoadout()
{
	DisarmDismiss();
	const FText Refusal = GetButtonRefusal(EPartyButton::OpenLoadout);
	if (!Refusal.IsEmpty())
	{
		SetMessage(Refusal);
		return;
	}
	// The loadout screen's characters skip a member with no data: find this one by its key.
	const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party || !Party->Members.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FName Key = UPoolSubsystem::GetCharacterKey(Party->Members[SelectedRow]);
	const int32 Target = Algo::IndexOfByPredicate(WoRLoadoutScreen::BuildMembers(GetWorld()), [Key](const FLoadoutScreenMember &Member) { return Member.Key == Key; });
	ULoadoutV2ScreenWidget *Screen = ULoadoutV2ScreenWidget::OpenWithLoadout(GetOwningPlayer()); // over this screen: Back returns here
	if (!Screen)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[PartyScreen] Open loadout: the loadout screen didn't open"));
		return;
	}
	if (Target != INDEX_NONE && Target != Screen->GetMemberIndex())
	{
		Screen->MoveMember(Target - Screen->GetMemberIndex());
	}
	UE_LOG(LogWoRUI, Log, TEXT("[PartyScreen] Open loadout on %s (character %d, shown %d)"), *Page.Members[SelectedRow].Name.ToString(), Target,
		   Screen->GetMemberIndex());
}

void UPartyScreenWidget::PressDismiss()
{
	const FText Refusal = GetButtonRefusal(EPartyButton::Dismiss);
	if (!Refusal.IsEmpty())
	{
		SetMessage(Refusal);
		return;
	}
	if (!bDismissArmed)
	{
		bDismissArmed = true; // a second press dismisses: one stray press doesn't send a member away
		SetMessage(FText::GetEmpty());
		RefreshButtons();
		return;
	}
	bDismissArmed = false;
	const FText Name = Page.Members[SelectedRow].Name;
	Message = WoRPartyScreen::Dismiss(GetWorld(), SelectedRow);
	UE_LOG(LogWoRUI, Log, TEXT("[PartyScreen] Dismiss %s (seat %d): %s"), *Name.ToString(), SelectedRow + 1,
		   Message.IsEmpty() ? TEXT("done") : *Message.ToString());
	RefreshData();
}

void UPartyScreenWidget::PressButton(EPartyButton Button)
{
	switch (Button)
	{
	case EPartyButton::MoveStart:
		PressMoveOnGrid();
		break;
	case EPartyButton::OpenLoadout:
		PressOpenLoadout();
		break;
	case EPartyButton::Dismiss:
		PressDismiss();
		break;
	default:
		break;
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
	const bool bAction = IsLeftKey(Key) || IsRightKey(Key) || IsConfirmKey(Key) || IsSecondActionKey(Key) || IsToggleKey(Key);
	if (bAction)
	{
		if (!InKeyEvent.IsRepeat())
		{
			if (IsLeftKey(Key) || IsRightKey(Key))
			{
				PressToggleSeat(); // two seats only (you / AI): either side turns it
			}
			else
			{
				PressButton(IsConfirmKey(Key) ? EPartyButton::MoveStart : IsSecondActionKey(Key) ? EPartyButton::OpenLoadout : EPartyButton::Dismiss);
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

int32 UPartyScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	// B-602: the cards' places as scrolled now (a card scrolled out of view keeps its last drawn place).
	return WoRMenuStyle::FindScrolledRowUnder(SeatList, Page.Members.Num(),
											  [this](int32 Index) -> const UWidget *
											  {
												  return SeatWidgets.IsValidIndex(Index) ? SeatWidgets[Index].Frame.Get() : nullptr;
											  },
											  MouseEvent.GetScreenSpacePosition());
}

int32 UPartyScreenWidget::FindButtonUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < PartyScreenStyle::BUTTON_COUNT; ++Index)
	{
		const UBorder *Plate = Buttons[Index].Plate.Get();
		if (Plate && Plate->IsVisible() && Plate->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UPartyScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a card, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPartyScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (Row != INDEX_NONE)
	{
		SelectRow(Row); // a click only lights a card: the buttons act
		return FReply::Handled();
	}
	const int32 Button = FindButtonUnder(InMouseEvent);
	if (Button != INDEX_NONE)
	{
		PressButton(static_cast<EPartyButton>(Button));
		return FReply::Handled();
	}
	if (PlayedByValue && PlayedByValue->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		PressToggleSeat();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPartyScreenOpenCommand(
	TEXT("wor.UI.Party"),
	TEXT("B-458 / B-628: open the party screen (seats, the lit member's active loadout, Details; A move start, Left / Right played by, Y loadout, X dismiss)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UPartyScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[PartyScreen] wor.UI.Party: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
