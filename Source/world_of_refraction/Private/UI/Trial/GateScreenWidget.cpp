// GateScreenWidget.cpp

#include "UI/Trial/GateScreenWidget.h"

#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "Trial/TrialData.h"
#include "UI/Menus/LoadoutScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Party/PartyScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRGateScreenWidget"

namespace GateScreenStyle
{
	// This screen's own layout (screen_mockups/run/gate, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 1040.0f;
	constexpr float TrialRowHeight = 69.0f;
	constexpr float TrialNameWidth = 186.0f;
	constexpr float RowGap = 9.0f;
	constexpr float RowPadding = 24.0f;
	constexpr float CardHeight = 120.0f;
	constexpr float CardGap = 6.0f;
	constexpr float PanelGap = 60.0f;
	constexpr float PanelWidth = 680.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float MemberGap = 15.0f;
	constexpr float EnterHeight = 90.0f;
	constexpr float HeaderGap = 24.0f;
	constexpr float CheckWidth = 36.0f;
	inline const FLinearColor LockedColor = TrackColor;
}

UGateScreenWidget::UGateScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UGateScreenWidget *UGateScreenWidget::Open(APlayerController *PC, UTrialData *StartGate)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[GateScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	UGateScreenWidget *Screen = Cast<UGateScreenWidget>(Root->PushToMenu(UGateScreenWidget::StaticClass()));
	if (Screen)
	{
		TArray<UTrialData *> All = WoRGateScreen::GatherGates();
		if (StartGate && !All.Contains(StartGate))
		{
			All.Insert(StartGate, 0); // the door's own gate is offered even when the registry missed it
		}
		Screen->SetGates(All, StartGate);
	}
	return Screen;
}

void UGateScreenWidget::SetGates(const TArray<UTrialData *> &InGates, const UTrialData *StartGate)
{
	Gates.Reset();
	for (UTrialData *Gate : InGates)
	{
		Gates.Add(Gate);
	}
	GateIndex = FMath::Max(Gates.IndexOfByKey(StartGate), 0);
	ResetChoice();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[GateScreen] %d gate(s), showing %s"), Gates.Num(), GetGate() ? *GetGate()->GetName() : TEXT("none"));
}

UTrialData *UGateScreenWidget::GetGate() const
{
	return Gates.IsValidIndex(GateIndex) ? Gates[GateIndex].Get() : nullptr;
}

void UGateScreenWidget::ResetChoice()
{
	ChosenTrial = WoRGateScreen::GetFirstOpenTrial(GetGameInstance(), GetGate());
	const UTrialData *Gate = GetGate();
	const int32 Trials = Gate ? Gate->Variations.Num() : 0;
	Focus = ChosenTrial != INDEX_NONE ? ChosenTrial : Trials; // no open trial: the difficulty row
	Message = FText::GetEmpty();
}

TSharedRef<SWidget> UGateScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGateScreenWidget::BuildTree()
{
	using namespace GateScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	UTextBlock *Title = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	Title->SetText(LOCTEXT("Title", "The Gate"));
	UTextBlock *Subtitle = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	Subtitle->SetText(LOCTEXT("Subtitle", "Pick a gate, then a trial, then the difficulty"));
	AddToColumn(Main, Title);
	AddToColumn(Main, Subtitle, FMargin(0.0f, LineGap, 0.0f, SectionGap));

	UHorizontalBox *TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("PrevGateKey", "LB")), FMargin(0.0f, 0.0f, TabGap, 0.0f), VAlign_Center);
	GateTabBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(TabRow, GateTabBox);
	AddToRow(TabRow, MakeKeyLabel(WidgetTree, LOCTEXT("NextGateKey", "RB")), FMargin(TabGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
	AddToColumn(Main, TabRow, FMargin(0.0f, 0.0f, 0.0f, HeaderGap));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Left: the trials, the difficulty cards and the two notes.
	UVerticalBox *Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *TrialHeading = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	TrialHeading->SetText(LOCTEXT("TrialHeading", "Trial"));
	AddToColumn(Left, TrialHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	TrialList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(Left, TrialList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *DifficultyHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	DifficultyHeading->SetText(LOCTEXT("DifficultyHeading", "Difficulty"));
	AddToColumn(Left, DifficultyHeading, FMargin(0.0f, SectionGap, 0.0f, LineGap));
	UHorizontalBox *Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const float CardWidth = (ListWidth - CardGap * (TrialGate::DIFFICULTY_COUNT - 1)) / TrialGate::DIFFICULTY_COUNT;
	for (int32 Index = 0; Index < TrialGate::DIFFICULTY_COUNT; ++Index)
	{
		FRowWidgets Card;
		UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetPadding(FMargin(RowPadding * 0.5f));
		UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Words);
		Card.Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
		Card.Line = MakeText(WidgetTree, SmallFontSize, DimColor);
		Card.Mark = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Words, Card.Name.Get());
		AddToColumn(Words, Card.Line.Get(), FMargin(0.0f, LineGap, 0.0f, 0.0f));
		AddToColumn(Words, Card.Mark.Get());
		Card.Frame = Frame;
		AddToRow(Cards, MakeSized(WidgetTree, CardWidth, CardHeight, Frame),
				 FMargin(0.0f, 0.0f, Index + 1 < TrialGate::DIFFICULTY_COUNT ? CardGap : 0.0f, 0.0f));
		DifficultyCards.Add(Card);
	}
	AddToColumn(Left, Cards);
	DifficultyNoteText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	RunNoteText = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(Left, DifficultyNoteText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(Left, RunNoteText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(Left, MessageText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, Left), FMargin(0.0f), VAlign_Fill);

	// Right: who goes in, then Enter.
	UVerticalBox *Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	UTextBlock *GoingIn = MakeText(WidgetTree, LineFontSize, DimColor, true);
	GoingIn->SetText(LOCTEXT("GoingIn", "Going in"));
	AddToColumn(PanelColumn, GoingIn, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		MemberRows.Add(BuildMemberRow(PanelColumn));
	}
	UHorizontalBox *Wipes = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *WipesLabel = MakeText(WidgetTree, LineFontSize, DimColor);
	WipesLabel->SetText(LOCTEXT("WipesAllowed", "Wipes allowed"));
	AddToRow(Wipes, WipesLabel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WipesText = MakeText(WidgetTree, LineFontSize, TextColor, true);
	AddToRow(Wipes, WipesText);
	AddToColumn(PanelColumn, Wipes, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(Right, Panel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	EnterFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EnterFrame->SetHorizontalAlignment(HAlign_Center);
	EnterFrame->SetVerticalAlignment(VAlign_Center);
	UTextBlock *EnterLabel = MakeText(WidgetTree, NameFontSize, TextColor, true);
	EnterLabel->SetText(LOCTEXT("Enter", "Enter the trial"));
	EnterFrame->SetContent(EnterLabel);
	AddToColumn(Right, MakeSized(WidgetTree, PanelWidth, EnterHeight, EnterFrame), FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Right), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "X  Party        Y  Loadout        A  Select        B / Esc  Cancel"));
	AddToColumn(Main, Hints, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
}

UGateScreenWidget::FRowWidgets UGateScreenWidget::BuildTrialRow()
{
	using namespace GateScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding, 0.0f));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToRow(Line, MakeSized(WidgetTree, TrialNameWidth, 0.0f, Name), FMargin(0.0f), VAlign_Center);
	UTextBlock *Detail = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToRow(Line, Detail, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Mark = MakeText(WidgetTree, RowFontSize, CyanColor, true);
	AddToRow(Line, MakeSized(WidgetTree, CheckWidth, 0.0f, Mark), FMargin(0.0f), VAlign_Center);
	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(TrialList->AddChild(MakeSized(WidgetTree, ListWidth, TrialRowHeight, Frame))))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.Line = Detail;
	Widgets.Mark = Mark;
	return Widgets;
}

UGateScreenWidget::FRowWidgets UGateScreenWidget::BuildMemberRow(UVerticalBox *Column)
{
	using namespace GateScreenStyle;

	FRowWidgets Widgets;
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Widgets.Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Widgets.Line = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Words, Widgets.Name.Get());
	AddToColumn(Words, Widgets.Line.Get());
	AddToRow(Line, Words)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Widgets.Mark = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	AddToRow(Line, Widgets.Mark.Get(), FMargin(0.0f), VAlign_Center);
	AddToColumn(Column, Line, FMargin(0.0f, 0.0f, 0.0f, MemberGap));
	return Widgets;
}

void UGateScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (Gates.IsEmpty())
	{
		SetGates(WoRGateScreen::GatherGates(), nullptr); // wor.UI.Gate, or a push without Open
	}
	else
	{
		Refresh();
	}
}

UWidget *UGateScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UGateScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UGateScreenWidget::Refresh()
{
	using namespace GateScreenStyle;

	TArray<UTrialData *> GateList;
	for (UTrialData *Gate : Gates)
	{
		GateList.Add(Gate);
	}
	Page = WoRGateScreen::BuildPage(GetWorld(), GateList, GetGate());
	Focus = FMath::Clamp(Focus, 0, RowCount() - 1);
	if (!TrialList)
	{
		return; // headless: no tree
	}

	while (GateTabs.Num() < Page.Gates.Num())
	{
		UTextBlock *Label = nullptr;
		GateTabs.Add(MakeTab(WidgetTree, Label));
		GateTabLabels.Add(Label);
		AddToRow(GateTabBox, GateTabs.Last().Get(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	for (int32 Index = 0; Index < GateTabs.Num(); ++Index)
	{
		const bool bShown = Page.Gates.IsValidIndex(Index);
		SetShown(GateTabs[Index].Get(), bShown);
		if (bShown && GateTabs[Index].IsValid() && GateTabLabels[Index].IsValid())
		{
			GateTabLabels[Index]->SetText(Page.Gates[Index]);
			GateTabs[Index]->SetBrushColor(Index == GateIndex ? FocusColor : PlateColor);
			GateTabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == GateIndex ? TextColor : DimColor));
		}
	}

	while (TrialRows.Num() < Page.Trials.Num())
	{
		TrialRows.Add(BuildTrialRow());
	}
	for (int32 Index = 0; Index < TrialRows.Num(); ++Index)
	{
		const FRowWidgets &Row = TrialRows[Index];
		const bool bShown = Page.Trials.IsValidIndex(Index);
		if (UBorder *Frame = Row.Frame.Get())
		{
			SetShown(Frame->GetParent(), bShown);
			if (bShown)
			{
				const FGateTrialRow &Trial = Page.Trials[Index];
				Frame->SetBrushColor(Index == Focus ? FocusColor : Trial.bLocked ? LockedColor : PlateColor);
				Row.Name->SetText(Trial.Name);
				Row.Name->SetColorAndOpacity(FSlateColor(Trial.bLocked ? OffColor : TextColor));
				Row.Line->SetText(Trial.Line);
				// TODO: the mockup's tick comes with the art pass's icons; a plain mark until then (as the journal's steps).
				Row.Mark->SetText(Index == ChosenTrial ? LOCTEXT("ChosenMark", "•") : FText::GetEmpty());
			}
		}
	}

	const bool bDifficultyLit = Focus == GetDifficultyRow();
	for (int32 Index = 0; Index < DifficultyCards.Num() && Index < Page.Difficulties.Num(); ++Index)
	{
		const FRowWidgets &Card = DifficultyCards[Index];
		const FGateDifficultyCard &Shown = Page.Difficulties[Index];
		const bool bPicked = Index == static_cast<int32>(Difficulty);
		Card.Frame->SetBrushColor(bPicked && bDifficultyLit ? FocusColor : bPicked ? TrackColor : PlateColor);
		Card.Name->SetText(Shown.Name);
		Card.Line->SetText(Shown.DamageLine);
		Card.Mark->SetText(Shown.PayoutLine);
		Card.Mark->SetColorAndOpacity(FSlateColor(Shown.bPaysMore ? GoodColor : DimColor));
	}
	DifficultyNoteText->SetText(Page.DifficultyNote);
	RunNoteText->SetText(Page.RunNote);
	MessageText->SetText(Message);
	SetShown(MessageText, !Message.IsEmpty());

	for (int32 Index = 0; Index < MemberRows.Num(); ++Index)
	{
		const FRowWidgets &Row = MemberRows[Index];
		const bool bShown = Page.Members.IsValidIndex(Index);
		if (UWidget *Words = Row.Name.IsValid() ? Row.Name->GetParent() : nullptr)
		{
			SetShown(Words->GetParent(), bShown);
		}
		if (bShown)
		{
			Row.Name->SetText(Page.Members[Index].Name);
			Row.Line->SetText(Page.Members[Index].ClassName);
			Row.Mark->SetText(Page.Members[Index].PlayedBy);
		}
	}
	WipesText->SetText(Page.WipesLine);
	EnterFrame->SetBrushColor(Focus == GetEnterRow() ? FocusColor : PlateColor);

	if (Focus < TrialRows.Num() && TrialRows[Focus].Frame.IsValid())
	{
		TrialList->ScrollWidgetIntoView(TrialRows[Focus].Frame.Get(), /*AnimateScroll=*/false);
	}
}

void UGateScreenWidget::MoveGate(int32 Delta)
{
	const int32 Count = Gates.Num();
	if (Count > 1)
	{
		GateIndex = ((GateIndex + Delta) % Count + Count) % Count;
		ResetChoice();
		Refresh();
	}
}

void UGateScreenWidget::MoveFocus(int32 Delta)
{
	const int32 Count = RowCount();
	Focus = ((Focus + Delta) % Count + Count) % Count;
	Message = FText::GetEmpty(); // a refusal belongs to the row it was about
	Refresh();
}

void UGateScreenWidget::StepDifficulty(int32 Delta)
{
	const int32 Count = TrialGate::DIFFICULTY_COUNT;
	Difficulty = static_cast<ETrialDifficulty>(((static_cast<int32>(Difficulty) + Delta) % Count + Count) % Count);
	Refresh();
}

void UGateScreenWidget::PressSelect()
{
	if (Page.Trials.IsValidIndex(Focus))
	{
		if (Page.Trials[Focus].bLocked)
		{
			Message = LOCTEXT("LockedTrial", "That trial is still locked.");
		}
		else
		{
			ChosenTrial = Focus;
			Message = FText::GetEmpty();
			Focus = GetDifficultyRow(); // then the difficulty (the mockup's order)
		}
		Refresh();
		return;
	}
	if (Focus == GetDifficultyRow())
	{
		Focus = GetEnterRow();
		Refresh();
		return;
	}
	Message = WoRGateScreen::Enter(GetGameInstance(), GetGate(), ChosenTrial, Difficulty);
	if (Message.IsEmpty())
	{
		DeactivateWidget(); // the level change takes the rest
		return;
	}
	Refresh();
}

FReply UGateScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	const bool bFresh = !InKeyEvent.IsRepeat();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveFocus(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsLeftKey(Key) || IsRightKey(Key))
	{
		StepDifficulty(IsRightKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsTabPrevKey(Key) || IsTabNextKey(Key))
	{
		if (bFresh)
		{
			MoveGate(IsTabNextKey(Key) ? 1 : -1);
		}
		return FReply::Handled();
	}
	if (IsConfirmKey(Key))
	{
		if (bFresh)
		{
			PressSelect();
		}
		return FReply::Handled();
	}
	if (IsToggleKey(Key) || IsSecondActionKey(Key))
	{
		// Over this screen: Back returns here.
		if (bFresh && !(IsToggleKey(Key) ? static_cast<UObject *>(UPartyScreenWidget::Open(GetOwningPlayer()))
										  : static_cast<UObject *>(ULoadoutScreenWidget::Open(GetOwningPlayer()))))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[GateScreen] the %s screen didn't open"), IsToggleKey(Key) ? TEXT("party") : TEXT("loadout"));
		}
		return FReply::Handled();
	}
	if (IsBackKey(Key))
	{
		// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
		if (bFresh)
		{
			HandleBackAction();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UGateScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent, int32 &OutCard) const
{
	const FVector2D At = MouseEvent.GetScreenSpacePosition();
	const auto IsUnder = [At](const UWidget *Widget) { return Widget && Widget->GetCachedGeometry().IsUnderLocation(At); };
	OutCard = INDEX_NONE;
	for (int32 Index = 0; Index < Page.Trials.Num() && Index < TrialRows.Num(); ++Index)
	{
		if (IsUnder(TrialRows[Index].Frame.Get()))
		{
			return Index;
		}
	}
	for (int32 Index = 0; Index < DifficultyCards.Num(); ++Index)
	{
		if (IsUnder(DifficultyCards[Index].Frame.Get()))
		{
			OutCard = Index;
			return GetDifficultyRow();
		}
	}
	return IsUnder(EnterFrame) ? GetEnterRow() : INDEX_NONE;
}

FReply UGateScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	int32 Card = INDEX_NONE;
	const int32 Row = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindRowUnder(InMouseEvent, Card) : INDEX_NONE;
	if (Row == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (Card != INDEX_NONE)
	{
		Difficulty = static_cast<ETrialDifficulty>(Card); // a click on a card picks it
		Focus = Row;
		Refresh();
		return FReply::Handled();
	}
	// The first click lights a row, a click on the lit row is A: a stray click doesn't enter.
	if (Row != Focus)
	{
		Focus = Row;
		Message = FText::GetEmpty();
		Refresh();
	}
	else
	{
		PressSelect();
	}
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GGateScreenOpenCommand(
	TEXT("wor.UI.Gate"),
	TEXT("B-466: open the Gate screen (LB / RB gate, Up / Down trial, Left / Right difficulty, A select or enter)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UGateScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr, nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[GateScreen] wor.UI.Gate: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
