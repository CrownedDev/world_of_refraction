// NewGameScreenWidget.cpp

#include "UI/Menus/NewGameScreenWidget.h"

#include "Components/EditableTextBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ElementColors.h"
#include "Player/WoRHubController.h"
#include "Save/SaveSubsystem.h"
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
	/** B-546: the most entries a class's pick grid has (a Generic's twelve weapon types). */
	constexpr int32 MaxPickChips = 12;
	static_assert(static_cast<int32>(EWeaponType::Shield) + 1 <= MaxPickChips, "a Generic's grid needs a chip per weapon type");
	constexpr float NameBoxWidth = 480.0f;
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

UNewGameScreenWidget *UNewGameScreenWidget::OpenToJoin(APlayerController *PC)
{
	UNewGameScreenWidget *Screen = Open(PC);
	if (Screen)
	{
		Screen->bJoining = true; // kept until it closes, so a late activation can't turn it into a real new game
		UE_LOG(LogWoRUI, Log, TEXT("[NewGame] Opened to join a friend's game with a new character"));
	}
	return Screen;
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
	AddToColumn(Main, Subheading, FMargin(0.0f, LineGap, 0.0f, SectionGap));

	// D-228 (Crown, 2026-11-17): no typed name; the character is named by its class and element (WoRNewGame::GetCharacterName).
	// OBSOLETE: NameLabel / NameBox and the name handlers stay unbuilt until Crown's PIE pass, then go.

	UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Columns, FMargin(0.0f, HeadingGap - SectionGap, 0.0f, 0.0f)); // the name row's gap, kept

	// Body: the starting spirit, Male Nobody (Manny) or Female Nobody (Quinn) (D-234).
	UVerticalBox *BodyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToRow(Columns, BodyColumn, FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Top);
	BodyLabel = MakeText(WidgetTree, LineFontSize, DimColor, true);
	BodyLabel->SetText(LOCTEXT("StartingBody", "Starting body")); // D-241: was "Starting spirit"
	AddToColumn(BodyColumn, BodyLabel, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	UHorizontalBox *BodyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(BodyColumn, BodyRow);
	const FText BodyNames[] = {WoRGameNames::GetBaseFormName(EPlayerBaseForm::Manny), WoRGameNames::GetBaseFormName(EPlayerBaseForm::Quinn)};
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
	ElementMarks.Reset();
	for (int32 Index = 0; Index < MaxPickChips; ++Index)
	{
		UBorder *Chip = MakeBox(WidgetTree, PlateColor, FMargin(ChipPadding, 0.0f));
		Chip->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox *ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Chip->SetContent(ChipRow);
		UBorder *Mark = MakeBox(WidgetTree, TextColor, FMargin(0.0f));
		AddToRow(ChipRow, MakeSized(WidgetTree, ChipMarkSize, ChipMarkSize, Mark), FMargin(0.0f, 0.0f, WordGap, 0.0f));
		UTextBlock *Name = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		AddToRow(ChipRow, Name);
		USizeBox *ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ChipBox->SetHeightOverride(ChipHeight); // the grid shares the width out
		ChipBox->AddChild(Chip);
		UUniformGridSlot *ChipSlot = Grid->AddChildToUniformGrid(ChipBox, Index / GridColumns, Index % GridColumns);
		ChipSlot->SetHorizontalAlignment(HAlign_Fill);
		ElementChips.Add(Chip);
		ElementNames.Add(Name);
		ElementMarks.Add(Mark);
	}
	PanelFootnote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, PanelFootnote, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	UVerticalBoxSlot *Fill = AddToColumn(Main, WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()));
	Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Hints = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{Arrows}  Move        {Confirm}  Select / pick        {Back}  Back"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

void UNewGameScreenWidget::NativeOnDeactivated()
{
	bJoining = false;
	Super::NativeOnDeactivated();
}

void UNewGameScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// The stack may hand back a pooled instance: every open starts at the mockup's default pick.
	Pick = FNewGamePick();
	Focus = ENewGameFocus::Class;
	ResetPickCursor();
	if (NameBox)
	{
		NameBox->SetText(FText::GetEmpty());
	}
	Status = FText::GetEmpty();
	HoveredIndex = INDEX_NONE;
	Refresh();
	ShowCursorNextTick(); // D-233
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

void UNewGameScreenWidget::MovePickCursor(int32 Index)
{
	if (Index < 0 || Index >= WoRNewGame::GetPickChoiceCount(Pick.Class))
	{
		return;
	}
	PickCursor = Index;
	if (Pick.Class == ECharacterClass::Caster)
	{
		WoRNewGame::TogglePick(Pick, Index); // one pick: it follows the cursor, as the element grid always did
	}
}

void UNewGameScreenWidget::ResetPickCursor()
{
	PickCursor = 0;
	for (int32 Index = 0; Index < WoRNewGame::GetPickChoiceCount(Pick.Class); ++Index)
	{
		if (WoRNewGame::IsPicked(Pick, Index))
		{
			PickCursor = Index;
			return;
		}
	}
}

void UNewGameScreenWidget::SetName(const FString &Name)
{
	Pick.Name = Name.Left(WoRNewGame::MAX_NAME_LENGTH);
	Status = FText::GetEmpty();
}

void UNewGameScreenWidget::StartTypingName()
{
	Focus = ENewGameFocus::Name;
	if (NameBox)
	{
		NameBox->SetKeyboardFocus(); // a pad on PC types through the system keyboard (Steam), as every PC game's name box
	}
	Refresh();
}

void UNewGameScreenWidget::HandleNameChanged(const FText &Text)
{
	const FString Typed = Text.ToString();
	SetName(Typed);
	if (NameBox && Typed.Len() > WoRNewGame::MAX_NAME_LENGTH)
	{
		NameBox->SetText(FText::FromString(Pick.Name)); // no longer than the limit
	}
}

void UNewGameScreenWidget::HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod)
{
	HandleNameChanged(Text);
	SetFocus(); // every way out of the box (Enter, Esc, Tab, a click away) gives the keys back to the screen
	Focus = ENewGameFocus::Body;
	Refresh();
}

void UNewGameScreenWidget::MoveUp()
{
	const TArray<ECharacterClass> &Classes = WoRNewGame::GetClasses();
	switch (Focus)
	{
	case ENewGameFocus::Body:
		break; // D-228: the body pair is the top row now (no name box)
	case ENewGameFocus::Class:
		if (GetClassIndex() > 0)
		{
			Pick.Class = Classes[GetClassIndex() - 1];
			ResetPickCursor();
		}
		else
		{
			Focus = ENewGameFocus::Body;
		}
		break;
	case ENewGameFocus::Element:
		MovePickCursor(PickCursor - NewGameScreenStyle::GridColumns);
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
	switch (Focus)
	{
	case ENewGameFocus::Name:
		Focus = ENewGameFocus::Body;
		break;
	case ENewGameFocus::Body:
		Focus = ENewGameFocus::Class;
		break;
	case ENewGameFocus::Class:
		if (GetClassIndex() < Classes.Num() - 1)
		{
			Pick.Class = Classes[GetClassIndex() + 1];
			ResetPickCursor();
		}
		else
		{
			Focus = ENewGameFocus::Begin;
		}
		break;
	case ENewGameFocus::Element:
		if (PickCursor + NewGameScreenStyle::GridColumns < WoRNewGame::GetPickChoiceCount(Pick.Class))
		{
			MovePickCursor(PickCursor + NewGameScreenStyle::GridColumns);
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
	case ENewGameFocus::Name:
		break;
	case ENewGameFocus::Body:
		Pick.Body = Delta < 0 ? EPlayerBaseForm::Manny : EPlayerBaseForm::Quinn;
		break;
	case ENewGameFocus::Class:
	case ENewGameFocus::Begin:
		// The pick grid stands to the right of the class cards, the body cards to their left (B-539).
		if (Delta > 0)
		{
			Focus = ENewGameFocus::Element;
			ResetPickCursor(); // the cursor starts on the pick
		}
		else if (Delta < 0)
		{
			Focus = ENewGameFocus::Body;
		}
		break;
	case ENewGameFocus::Element:
	{
		const int32 Column = PickCursor % GridColumns;
		if (Delta < 0 && Column == 0)
		{
			Focus = ENewGameFocus::Class;
		}
		else if (Column + Delta >= 0 && Column + Delta < GridColumns)
		{
			MovePickCursor(PickCursor + Delta);
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
	case ENewGameFocus::Name:
		StartTypingName();
		return;
	case ENewGameFocus::Body:
		Focus = ENewGameFocus::Class;
		break;
	case ENewGameFocus::Class:
		Focus = ENewGameFocus::Element;
		ResetPickCursor();
		break;
	case ENewGameFocus::Element:
		if (Pick.Class == ECharacterClass::Caster)
		{
			Focus = ENewGameFocus::Begin;
		}
		else
		{
			WoRNewGame::TogglePick(Pick, PickCursor); // two picks: A takes or drops the lit one
			Status = FText::GetEmpty();
		}
		break;
	case ENewGameFocus::Begin:
		if (bJoining)
		{
			Status = WoRNewGame::GetBeginRefusal(Pick);
			const USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
			if (Status.IsEmpty() && Saves && Saves->FindFreeSlot(WoRTitleScreen::GetMaxSlots()) == INDEX_NONE)
			{
				// B-563 (Q-561): a new character made to join keeps what it earns in a slot of its own.
				Status = LOCTEXT("JoinSlotsFull", "Every character slot is in use: join as one of your characters, or delete one on the title.");
			}
			if (Status.IsEmpty())
			{
				APlayerController *PC = GetOwningPlayer();
				DeactivateWidget(); // B-562: the host builds it and picks the seat (D-150)
				WoRTitleScreen::SendJoin(PC, Pick, INDEX_NONE);
				return;
			}
			break;
		}
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

void UNewGameScreenWidget::ClickPart(ENewGameFocus Part, int32 Index)
{
	switch (Part)
	{
	case ENewGameFocus::Name:
		StartTypingName();
		return;
	case ENewGameFocus::Body:
		if (Index != static_cast<int32>(EPlayerBaseForm::Manny) && Index != static_cast<int32>(EPlayerBaseForm::Quinn))
		{
			return;
		}
		Pick.Body = static_cast<EPlayerBaseForm>(Index);
		break;
	case ENewGameFocus::Class:
		if (!WoRNewGame::GetClasses().IsValidIndex(Index))
		{
			return;
		}
		if (Pick.Class != WoRNewGame::GetClasses()[Index])
		{
			Pick.Class = WoRNewGame::GetClasses()[Index];
			ResetPickCursor();
		}
		break;
	case ENewGameFocus::Element:
		if (Index < 0 || Index >= WoRNewGame::GetPickChoiceCount(Pick.Class))
		{
			return;
		}
		PickCursor = Index;
		WoRNewGame::TogglePick(Pick, Index); // a Refractor takes it; the others take or drop it
		break;
	case ENewGameFocus::Begin:
		Focus = ENewGameFocus::Begin;
		Confirm(); // the same path as A on Begin
		return;
	}
	Focus = Part;
	Status = FText::GetEmpty();
	Refresh();
}

void UNewGameScreenWidget::Refresh()
{
	using namespace NewGameScreenStyle;
	if (!BeginCard)
	{
		return; // headless: no tree
	}
	auto Fill = [this](bool bFocused, bool bPicked, ENewGameFocus Part, int32 Index)
	{
		const bool bHovered = HoveredIndex == Index && HoveredPart == Part; // D-233: the mouse is over it
		return bFocused ? FocusColor : bPicked ? TrackColor : bHovered ? HoverColor : PlateColor;
	};
	for (int32 Index = 0; Index < BodyCards.Num(); ++Index)
	{
		const bool bPicked = static_cast<int32>(Pick.Body) == Index;
		BodyCards[Index]->SetBrushColor(Fill(bPicked && Focus == ENewGameFocus::Body, bPicked, ENewGameFocus::Body, Index));
	}
	const int32 ClassIndex = GetClassIndex();
	for (int32 Index = 0; Index < ClassCards.Num(); ++Index)
	{
		const bool bPicked = Index == ClassIndex;
		ClassCards[Index]->SetBrushColor(Fill(bPicked && Focus == ENewGameFocus::Class, bPicked, ENewGameFocus::Class, Index));
		ClassKinds[Index]->SetColorAndOpacity(FSlateColor(bPicked ? CyanColor : DimColor));
		ClassLines[Index]->SetColorAndOpacity(FSlateColor(bPicked ? TextColor : DimColor));
	}
	BeginCard->SetBrushColor(Fill(Focus == ENewGameFocus::Begin, false, ENewGameFocus::Begin, 0));
	BeginText->SetText(WoRNewGame::GetBeginLabel(Pick)); // D-241: duplicates allowed again (D-229's one of each kind removed)
	BodyLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Body ? CyanColor : DimColor));
	ClassLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Class ? CyanColor : DimColor));
	ElementLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Element ? CyanColor : DimColor));
	ElementLabel->SetText(WoRNewGame::GetPicksLabel(Pick.Class));
	if (NameLabel) // OBSOLETE (D-228): never built now
	{
		NameLabel->SetColorAndOpacity(FSlateColor(Focus == ENewGameFocus::Name ? CyanColor : DimColor));
	}

	const bool bRefractor = Pick.Class == ECharacterClass::Caster;
	SetShown(PanelMark, bRefractor);
	PanelMark->SetBrushColor(MarkColor(Pick.Element));
	PanelTitle->SetText(WoRNewGame::GetPanelTitle(Pick));
	PanelAbout->SetText(WoRNewGame::GetAbout(Pick));
	// A Refractor: the element's full-bar line, in its colour; the others: what they picked.
	PanelElementLine->SetText(bRefractor ? (Pick.bBrokenDarkness ? FText::GetEmpty() : WoRNewGame::GetElementLine(Pick.Element)) : WoRNewGame::GetPicksLine(Pick));
	PanelElementLine->SetColorAndOpacity(FSlateColor(bRefractor ? MarkColor(Pick.Element) : CyanColor));
	SetShown(PanelElementLine, !PanelElementLine->GetText().IsEmpty());
	const int32 ChoiceCount = WoRNewGame::GetPickChoiceCount(Pick.Class);
	for (int32 Index = 0; Index < ElementChips.Num(); ++Index)
	{
		const bool bShown = Index < ChoiceCount;
		SetShown(ElementChips[Index]->GetParent(), bShown);
		if (!bShown)
		{
			continue;
		}
		const bool bPicked = WoRNewGame::IsPicked(Pick, Index);
		const bool bLit = Focus == ENewGameFocus::Element && Index == PickCursor;
		ElementChips[Index]->SetBrushColor(Fill(bLit, bPicked, ENewGameFocus::Element, Index));
		ElementNames[Index]->SetText(WoRNewGame::GetPickChoiceName(Pick.Class, Index));
		ElementNames[Index]->SetColorAndOpacity(FSlateColor(bPicked ? TextColor : DimColor));
		ESpellElement ChoiceElement = ESpellElement::None;
		const bool bMarked = WoRNewGame::GetPickChoiceElement(Pick.Class, Index, ChoiceElement);
		SetShown(ElementMarks[Index], bMarked);
		ElementMarks[Index]->SetBrushColor(MarkColor(ChoiceElement));
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

bool UNewGameScreenWidget::FindPartUnder(const FVector2D &ScreenPosition, ENewGameFocus &OutPart, int32 &OutIndex) const
{
	auto Under = [&ScreenPosition](const TArray<TObjectPtr<UBorder>> &Boxes)
	{
		return Boxes.IndexOfByPredicate([&ScreenPosition](const TObjectPtr<UBorder> &Box)
		{
			return Box && Box->IsVisible() && Box->GetCachedGeometry().IsUnderLocation(ScreenPosition);
		});
	};
	const TPair<ENewGameFocus, int32> Hits[] = {{ENewGameFocus::Name, NameBox && NameBox->GetCachedGeometry().IsUnderLocation(ScreenPosition) ? 0 : INDEX_NONE},
												 {ENewGameFocus::Body, Under(BodyCards)},
												 {ENewGameFocus::Class, Under(ClassCards)},
												 {ENewGameFocus::Element, Under(ElementChips)},
												 {ENewGameFocus::Begin, Under({BeginCard})}};
	for (const TPair<ENewGameFocus, int32> &Hit : Hits)
	{
		if (Hit.Value != INDEX_NONE)
		{
			OutPart = Hit.Key;
			OutIndex = Hit.Value;
			return true;
		}
	}
	return false;
}

FReply UNewGameScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	ENewGameFocus Part = ENewGameFocus::Body;
	int32 Index = INDEX_NONE;
	FindPartUnder(InMouseEvent.GetScreenSpacePosition(), Part, Index);
	if (Part != HoveredPart || Index != HoveredIndex)
	{
		HoveredPart = Part;
		HoveredIndex = Index;
		Refresh();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UNewGameScreenWidget::NativeOnMouseLeave(const FPointerEvent &InMouseEvent)
{
	if (HoveredIndex != INDEX_NONE)
	{
		HoveredIndex = INDEX_NONE;
		Refresh();
	}
	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply UNewGameScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (!IsActivated())
	{
		return FReply::Handled(); // a double click after Begin: the game is already starting
	}
	ENewGameFocus Part = ENewGameFocus::Body;
	int32 Index = INDEX_NONE;
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && FindPartUnder(InMouseEvent.GetScreenSpacePosition(), Part, Index))
	{
		ClickPart(Part, Index);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
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
