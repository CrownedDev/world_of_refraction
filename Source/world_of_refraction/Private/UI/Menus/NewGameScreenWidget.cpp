// NewGameScreenWidget.cpp

#include "UI/Menus/NewGameScreenWidget.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ElementColors.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/TitleScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRNewGameScreenWidget"

namespace NewGameScreenStyle
{
	// This screen's own layout (screen_mockups/front/new_game, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float BodyCardWidth = 276.0f;
	constexpr float BodyCardHeight = 384.0f;
	constexpr float PreviewWidth = 210.0f;
	constexpr float PreviewHeight = 279.0f;
	constexpr float CardGap = 15.0f;
	constexpr float ColumnGap = 45.0f;
	constexpr float ClassWidth = 630.0f;
	constexpr float ClassCardPadding = 27.0f;
	constexpr float BeginHeight = 84.0f;
	constexpr float BeginGap = 33.0f;
	constexpr float PanelWidth = 483.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float MarkSize = 30.0f;
	constexpr float ChipHeight = 60.0f;
	constexpr float ChipPadding = 12.0f;
	constexpr float ChipMarkSize = 18.0f;
	constexpr float HeadingGap = 60.0f;
	constexpr int32 HeadingFontSize = 48;
	constexpr int32 BeginFontSize = 29;
	constexpr int32 GridColumns = 3;
	inline const FLinearColor PreviewColor = FLinearColor::FromSRGBColor(FColor(12, 14, 20));

	FLinearColor MarkColor(ESpellElement Element)
	{
		// Darkness is near-black: on the dark panel it takes the pale outline colour instead (D-106).
		return ElementColors::NeedsTextOutline(Element) ? ElementColors::TextOutline : ElementColors::GetColorForElement(Element);
	}

	UBorder *MakeBox(UWidgetTree *Tree, const FLinearColor &Color, const FMargin &Padding)
	{
		UBorder *Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrushColor(Color);
		Box->SetPadding(Padding);
		return Box;
	}
}

UNewGameScreenWidget::UNewGameScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its cards are plain borders
}

UNewGameScreenWidget *UNewGameScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[NewGame] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UNewGameScreenWidget>(Root->PushToMenu(UNewGameScreenWidget::StaticClass()));
}

TSharedRef<SWidget> UNewGameScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UNewGameScreenWidget::BuildTree()
{
	using namespace NewGameScreenStyle;

	UBorder *Screen = MakeBox(WidgetTree, FrontBackdropColor, FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	// TODO: the heading and how the opening leads here are Crown's (lore); the mockup's words stand in.
	UTextBlock *Heading = MakeText(WidgetTree, HeadingFontSize, TextColor, true);
	Heading->SetText(LOCTEXT("Heading", "Who wakes"));
	AddToColumn(Main, Heading);
	UTextBlock *Subheading = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	Subheading->SetText(LOCTEXT("Subheading", "placeholder heading"));
	AddToColumn(Main, Subheading, FMargin(0.0f, LineGap, 0.0f, HeadingGap));

	UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Columns);

	// Body: Manny and Quinn.
	UVerticalBox *BodyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToRow(Columns, BodyColumn, FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Top);
	BodyLabel = MakeText(WidgetTree, LineFontSize, DimColor, true);
	BodyLabel->SetText(LOCTEXT("Body", "Body"));
	AddToColumn(BodyColumn, BodyLabel, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	UHorizontalBox *BodyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(BodyColumn, BodyRow);
	const FText BodyNames[] = {LOCTEXT("Manny", "Manny"), LOCTEXT("Quinn", "Quinn")};
	BodyCards.Reset();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BodyNames); ++Index)
	{
		UBorder *Card = MakeBox(WidgetTree, PlateColor, FMargin(CardGap));
		UVerticalBox *CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(CardColumn);
		// TODO: the body's own preview (an editor step); a grey box until then.
		UBorder *Preview = MakeBox(WidgetTree, PreviewColor, FMargin(0.0f));
		Preview->SetHorizontalAlignment(HAlign_Center);
		Preview->SetVerticalAlignment(VAlign_Center);
		UTextBlock *PreviewText = MakeText(WidgetTree, SmallFontSize, OffColor);
		PreviewText->SetText(LOCTEXT("Preview", "preview"));
		Preview->SetContent(PreviewText);
		AddToColumn(CardColumn, MakeSized(WidgetTree, PreviewWidth, PreviewHeight, Preview))->SetHorizontalAlignment(HAlign_Center);
		UTextBlock *Name = MakeText(WidgetTree, NameFontSize, TextColor, true);
		Name->SetText(BodyNames[Index]);
		AddToColumn(CardColumn, Name, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
		AddToRow(BodyRow, MakeSized(WidgetTree, BodyCardWidth, BodyCardHeight, Card), FMargin(0.0f, 0.0f, CardGap, 0.0f));
		BodyCards.Add(Card);
	}

	// Class: three cards, then Begin.
	UVerticalBox *ClassColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToRow(Columns, MakeSized(WidgetTree, ClassWidth, 0.0f, ClassColumn), FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Top);
	ClassLabel = MakeText(WidgetTree, LineFontSize, DimColor, true);
	ClassLabel->SetText(LOCTEXT("Class", "Class"));
	AddToColumn(ClassColumn, ClassLabel, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	ClassCards.Reset();
	ClassKinds.Reset();
	ClassLines.Reset();
	for (const ECharacterClass Class : WoRNewGame::GetClasses())
	{
		UBorder *Card = MakeBox(WidgetTree, PlateColor, FMargin(ClassCardPadding, SectionGap));
		UVerticalBox *CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(CardColumn);
		UHorizontalBox *Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddToColumn(CardColumn, Top);
		UTextBlock *Name = MakeText(WidgetTree, NameFontSize, TextColor, true);
		Name->SetText(WoRGameNames::GetCharacterClassName(Class));
		AddToRow(Top, Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock *Kind = MakeText(WidgetTree, LineFontSize, DimColor, true);
		Kind->SetText(WoRNewGame::GetClassKind(Class));
		AddToRow(Top, Kind);
		UTextBlock *Line = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
		Line->SetText(WoRNewGame::GetClassLine(Class));
		AddToColumn(CardColumn, Line, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		AddToColumn(ClassColumn, Card, FMargin(0.0f, 0.0f, 0.0f, CardGap));
		ClassCards.Add(Card);
		ClassKinds.Add(Kind);
		ClassLines.Add(Line);
	}
	BeginCard = MakeBox(WidgetTree, PlateColor, FMargin(0.0f));
	BeginCard->SetHorizontalAlignment(HAlign_Center);
	BeginCard->SetVerticalAlignment(VAlign_Center);
	BeginText = MakeText(WidgetTree, BeginFontSize, TextColor, true);
	BeginCard->SetContent(BeginText);
	AddToColumn(ClassColumn, MakeSized(WidgetTree, ClassWidth, BeginHeight, BeginCard), FMargin(0.0f, BeginGap - CardGap, 0.0f, 0.0f));
	StatusText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(ClassColumn, StatusText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	// The side panel: what the pick is, and a Refractor's element.
	UBorder *Panel = MakeBox(WidgetTree, PanelColor, FMargin(PanelPadding));
	AddToRow(Columns, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(0.0f, LineFontSize + SectionGap, 0.0f, 0.0f), VAlign_Top);
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	UHorizontalBox *TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(PanelColumn, TitleRow);
	PanelMark = MakeBox(WidgetTree, TextColor, FMargin(0.0f));
	AddToRow(TitleRow, MakeSized(WidgetTree, MarkSize, MarkSize, PanelMark), FMargin(0.0f, 0.0f, WordGap * 2.0f, 0.0f));
	PanelTitle = MakeText(WidgetTree, NameFontSize + 4, TextColor, true);
	AddToRow(TitleRow, PanelTitle);
	PanelAbout = MakeWrappedText(WidgetTree, LineFontSize + 2, TextColor);
	AddToColumn(PanelColumn, PanelAbout, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	PanelElementLine = MakeWrappedText(WidgetTree, LineFontSize + 2, CyanColor);
	AddToColumn(PanelColumn, PanelElementLine, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UVerticalBox *ElementColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ElementArea = ElementColumn;
	AddToColumn(PanelColumn, ElementColumn, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	ElementLabel = MakeText(WidgetTree, LineFontSize + 2, DimColor, true);
	ElementLabel->SetText(LOCTEXT("Element", "Element"));
	AddToColumn(ElementColumn, ElementLabel, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	UUniformGridPanel *Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(LineGap * 0.5f));
	AddToColumn(ElementColumn, Grid);
	ElementChips.Reset();
	ElementNames.Reset();
	const TArray<ESpellElement> &Elements = WoRNewGame::GetElements();
	for (int32 Index = 0; Index < Elements.Num(); ++Index)
	{
		UBorder *Chip = MakeBox(WidgetTree, PlateColor, FMargin(ChipPadding, 0.0f));
		Chip->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox *ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Chip->SetContent(ChipRow);
		AddToRow(ChipRow, MakeSized(WidgetTree, ChipMarkSize, ChipMarkSize, MakeBox(WidgetTree, MarkColor(Elements[Index]), FMargin(0.0f))),
				 FMargin(0.0f, 0.0f, WordGap, 0.0f));
		UTextBlock *Name = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		Name->SetText(WoRGameNames::GetElementName(Elements[Index]));
		AddToRow(ChipRow, Name);
		USizeBox *ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ChipBox->SetHeightOverride(ChipHeight); // the grid shares the width out
		ChipBox->AddChild(Chip);
		UUniformGridSlot *ChipSlot = Grid->AddChildToUniformGrid(ChipBox, Index / GridColumns, Index % GridColumns);
		ChipSlot->SetHorizontalAlignment(HAlign_Fill);
		ElementChips.Add(Chip);
		ElementNames.Add(Name);
	}
	PanelFootnote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, PanelFootnote, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	UVerticalBoxSlot *Fill = AddToColumn(Main, WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()));
	Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Hints = MakeHintText(WidgetTree, LineFontSize, DimColor); // B-481: the keys of the device in use
	WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{Arrows}  Move        {Confirm}  Select        {Back}  Back to title"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

void UNewGameScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// The stack may hand back a pooled instance: every open starts at the mockup's default pick.
	Pick = FNewGamePick();
	Focus = ENewGameFocus::Class;
	Status = FText::GetEmpty();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[NewGame] Opened"));
}

UWidget *UNewGameScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UNewGameScreenWidget *>(this);
}

int32 UNewGameScreenWidget::GetClassIndex() const
{
	return WoRNewGame::GetClasses().IndexOfByKey(Pick.Class);
}

int32 UNewGameScreenWidget::GetElementIndex() const
{
	return WoRNewGame::GetElements().IndexOfByKey(Pick.Element);
}

void UNewGameScreenWidget::MoveUp()
{
	const TArray<ECharacterClass> &Classes = WoRNewGame::GetClasses();
	switch (Focus)
	{
	case ENewGameFocus::Class:
		if (GetClassIndex() > 0)
		{
			Pick.Class = Classes[GetClassIndex() - 1];
		}
		else
		{
			Focus = ENewGameFocus::Body;
		}
		break;
	case ENewGameFocus::Element:
		if (GetElementIndex() >= NewGameScreenStyle::GridColumns)
		{
			Pick.Element = WoRNewGame::GetElements()[GetElementIndex() - NewGameScreenStyle::GridColumns];
		}
		break;
	case ENewGameFocus::Begin:
		Focus = ENewGameFocus::Class;
		break;
	default:
		break;
	}
	Status = FText::GetEmpty();
	Refresh();
}

void UNewGameScreenWidget::MoveDown()
{
	const TArray<ECharacterClass> &Classes = WoRNewGame::GetClasses();
	const TArray<ESpellElement> &Elements = WoRNewGame::GetElements();
	switch (Focus)
	{
	case ENewGameFocus::Body:
		Focus = ENewGameFocus::Class;
		break;
	case ENewGameFocus::Class:
		if (GetClassIndex() < Classes.Num() - 1)
		{
			Pick.Class = Classes[GetClassIndex() + 1];
		}
		else
		{
			Focus = ENewGameFocus::Begin;
		}
		break;
	case ENewGameFocus::Element:
		if (GetElementIndex() + NewGameScreenStyle::GridColumns < Elements.Num())
		{
			Pick.Element = Elements[GetElementIndex() + NewGameScreenStyle::GridColumns];
		}
		else
		{
			Focus = ENewGameFocus::Begin;
		}
		break;
	default:
		break;
	}
	Status = FText::GetEmpty();
	Refresh();
}

void UNewGameScreenWidget::MoveSide(int32 Delta)
{
	using NewGameScreenStyle::GridColumns;
	switch (Focus)
	{
	case ENewGameFocus::Body:
		Pick.Body = Delta < 0 ? EPlayerBaseForm::Manny : EPlayerBaseForm::Quinn;
		break;
	case ENewGameFocus::Class:
	case ENewGameFocus::Begin:
		// The element grid stands to the right of the class cards.
		if (Delta > 0 && HasElements())
		{
			Focus = ENewGameFocus::Element;
		}
		break;
	case ENewGameFocus::Element:
	{
		const int32 Column = GetElementIndex() % GridColumns;
		if (Delta < 0 && Column == 0)
		{
			Focus = ENewGameFocus::Class;
		}
		else if (Column + Delta >= 0 && Column + Delta < GridColumns)
		{
			Pick.Element = WoRNewGame::GetElements()[GetElementIndex() + Delta];
		}
		break;
	}
	}
	Status = FText::GetEmpty();
	Refresh();
}

void UNewGameScreenWidget::Confirm()
{
	switch (Focus)
	{
	case ENewGameFocus::Body:
		Focus = ENewGameFocus::Class;
		break;
	case ENewGameFocus::Class:
		Focus = HasElements() ? ENewGameFocus::Element : ENewGameFocus::Begin;
		break;
	case ENewGameFocus::Element:
		Focus = ENewGameFocus::Begin;
		break;
	case ENewGameFocus::Begin:
		Status = WoRNewGame::Begin(GetOwningPlayer(), Pick);
		if (Status.IsEmpty())
		{
			// The hub opens next: the title stays closed in it.
			UTitleScreenWidget::MarkLeftThisLaunch();
			DeactivateWidget();
			return;
		}
		break;
	}
	Refresh();
}

void UNewGameScreenWidget::Refresh()
{
	using namespace NewGameScreenStyle;
	if (!BeginCard)
	{
		return; // headless: no tree
	}
	auto Fill = [this](bool bFocused, bool bPicked)
	{
		return bFocused ? FocusColor : bPicked ? TrackColor : PlateColor;
	};
	for (int32 Index = 0; Index < BodyCards.Num(); ++Index)
	{
		const bool bPicked = static_cast<int32>(Pick.Body) == Index;
		BodyCards[Index]->SetBrushColor(Fill(bPicked && Focus == ENewGameFocus::Body, bPicked));
	}
	const int32 ClassIndex = GetClassIndex();
	for (int32 Index = 0; Index < ClassCards.Num(); ++Index)
	{
		const bool bPicked = Index == ClassIndex;
		ClassCards[Index]->SetBrushColor(Fill(bPicked && Focus == ENewGameFocus::Class, bPicked));
		ClassKinds[Index]->SetColorAndOpacity(FSlateColor(bPicked ? CyanColor : DimColor));
		ClassLines[Index]->SetColorAndOpacity(FSlateColor(bPicked ? TextColor : DimColor));
	}
	BeginCard->SetBrushColor(Fill(Focus == ENewGameFocus::Begin, false));
	BeginText->SetText(WoRNewGame::GetBeginLabel(Pick));
	BodyLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Body ? CyanColor : DimColor));
	ClassLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Class ? CyanColor : DimColor));
	ElementLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Element ? CyanColor : DimColor));

	const bool bElements = HasElements();
	SetShown(PanelMark, bElements);
	PanelMark->SetBrushColor(MarkColor(Pick.Element));
	PanelTitle->SetText(WoRNewGame::GetPanelTitle(Pick));
	PanelAbout->SetText(WoRNewGame::GetAbout(Pick));
	SetShown(PanelElementLine, bElements);
	PanelElementLine->SetText(WoRNewGame::GetElementLine(Pick.Element));
	PanelElementLine->SetColorAndOpacity(FSlateColor(MarkColor(Pick.Element)));
	SetShown(ElementArea, bElements);
	const int32 ElementIndex = GetElementIndex();
	for (int32 Index = 0; Index < ElementChips.Num(); ++Index)
	{
		const bool bPicked = Index == ElementIndex;
		ElementChips[Index]->SetBrushColor(Fill(bPicked && Focus == ENewGameFocus::Element, bPicked));
		ElementNames[Index]->SetColorAndOpacity(FSlateColor(bPicked ? TextColor : DimColor));
	}
	PanelFootnote->SetText(WoRNewGame::GetPanelFootnote(Pick));
	StatusText->SetText(Status);
}

FReply UNewGameScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key))
	{
		MoveUp();
		return FReply::Handled();
	}
	if (IsDownKey(Key))
	{
		MoveDown();
		return FReply::Handled();
	}
	if (IsLeftKey(Key) || IsRightKey(Key))
	{
		MoveSide(IsRightKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key))
	{
		// A fresh press only: the title's A, still held, doesn't walk the screen.
		if (!InKeyEvent.IsRepeat())
		{
			Confirm();
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

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GNewGameOpenCommand(
	TEXT("wor.UI.NewGame"),
	TEXT("B-469: open the new game screen, as the title's New Game does. Begin starts a fresh game (the save is replaced)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UNewGameScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[NewGame] wor.UI.NewGame: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
