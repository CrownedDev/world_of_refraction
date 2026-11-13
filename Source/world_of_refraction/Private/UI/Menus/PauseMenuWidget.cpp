// PauseMenuWidget.cpp

#include "UI/Menus/PauseMenuWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "TimerManager.h"
#include "UI/Hub/InventoryWidget.h"
#include "UI/Hub/MemoryJournalWidget.h"
#include "UI/Party/PartyControlWidget.h"
#include "UI/Party/PartyDisplayWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPauseMenuWidget"

namespace PauseMenuStyle
{
	// The mockup (screen_mockups/menus/pause, 1280x720) at 1080p: Slate units, DPI-scaled.
	// TODO: the Hidris art pass replaces the flat colours and fonts (UI_Theme.md).
	constexpr float ScreenPadding = 72.0f;
	constexpr float PageWidth = 423.0f;
	constexpr float RowHeight = 72.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowTextInset = 24.0f;
	constexpr float ColumnGap = 102.0f;
	constexpr float HeaderGap = 48.0f;
	constexpr float CardWidth = 780.0f;
	constexpr float CardPadding = 24.0f;
	constexpr float CardGap = 21.0f;
	constexpr float BarWidth = 330.0f;
	constexpr float BarHeight = 30.0f; // tall enough to hold its value
	constexpr float HoldBarHeight = 15.0f;
	constexpr float BarLabelWidth = 51.0f;
	constexpr float LineGap = 9.0f;
	constexpr float PillarLabelWidth = 105.0f;
	constexpr float PanelGap = 60.0f;
	constexpr float PanelWidth = 408.0f;
	constexpr float PanelPadding = 27.0f;
	constexpr float PanelInnerWidth = PanelWidth - PanelPadding - PanelPadding;
	constexpr float SectionGap = 18.0f;
	constexpr float NoteGap = 27.0f;
	constexpr float ChipGap = 36.0f;
	constexpr float ChipTopInset = 15.0f;
	constexpr float WordGap = 9.0f;

	constexpr int32 TitleFontSize = 38;
	constexpr int32 SubtitleFontSize = 20;
	constexpr int32 RowFontSize = 22;
	constexpr int32 NameFontSize = 29;
	constexpr int32 LineFontSize = 19;
	constexpr int32 SmallFontSize = 17;

	constexpr float LowHPFraction = 0.3f;
	constexpr float HoldTickSeconds = 0.03f;
	constexpr int32 MaxChips = 3;

	// The mockup kit's tokens (screen_mockups/_kit/wor_ui.py).
	const FLinearColor ScreenColor = FLinearColor::FromSRGBColor(FColor(16, 18, 25, 240));
	const FLinearColor PanelColor = FLinearColor::FromSRGBColor(FColor(20, 22, 28));
	const FLinearColor PlateColor = FLinearColor::FromSRGBColor(FColor(46, 46, 46));
	const FLinearColor TrackColor = FLinearColor::FromSRGBColor(FColor(42, 45, 53));
	const FLinearColor FocusColor = FLinearColor::FromSRGBColor(FColor(51, 127, 141));
	const FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor(238, 241, 245));
	const FLinearColor DimColor = FLinearColor::FromSRGBColor(FColor(169, 175, 184));
	const FLinearColor OffColor = FLinearColor::FromSRGBColor(FColor(107, 112, 120));
	const FLinearColor HPColor = FLinearColor::FromSRGBColor(FColor(224, 64, 127));
	const FLinearColor LowHPColor = FLinearColor::FromSRGBColor(FColor(255, 143, 180));
	const FLinearColor EPColor = FLinearColor::FromSRGBColor(FColor(111, 214, 228));
	const FLinearColor WarnColor = FLinearColor::FromSRGBColor(FColor(242, 162, 58));
	const FLinearColor ValueBackingColor = FLinearColor::FromSRGBColor(FColor(16, 18, 25, 170));

	const FName BoldTypeface(TEXT("Bold"));

	// Menu keys (InputMap.md, Menus). The Menu button that opened the screen closes it too.
	bool IsUpKey(const FKey &Key)
	{
		return Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up;
	}
	bool IsDownKey(const FKey &Key)
	{
		return Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down;
	}
	bool IsConfirmKey(const FKey &Key)
	{
		return Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
	}
	bool IsBackKey(const FKey &Key)
	{
		return Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
	}

	UTextBlock *MakeText(UWidgetTree *Tree, int32 FontSize, const FLinearColor &Color, bool bBold = false)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		if (bBold)
		{
			Font.TypefaceFontName = BoldTypeface;
		}
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UTextBlock *MakeWrappedText(UWidgetTree *Tree, int32 FontSize, const FLinearColor &Color, bool bBold = false)
	{
		UTextBlock *Text = MakeText(Tree, FontSize, Color, bBold);
		Text->SetAutoWrapText(true);
		return Text;
	}

	UProgressBar *MakeBar(UWidgetTree *Tree, const FLinearColor &FillColor)
	{
		UProgressBar *Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage = FSlateColorBrush(TrackColor);
		Style.FillImage = FSlateColorBrush(FLinearColor::White);
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(FillColor);
		return Bar;
	}

	/** Content in a box of a fixed width (and height, when above zero). */
	USizeBox *MakeSized(UWidgetTree *Tree, float Width, float Height, UWidget *Content)
	{
		USizeBox *Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		if (Height > 0.0f)
		{
			Box->SetHeightOverride(Height);
		}
		Box->AddChild(Content);
		return Box;
	}

	UHorizontalBoxSlot *AddToRow(UHorizontalBox *Row, UWidget *Child, const FMargin &Padding = FMargin(0.0f),
								 EVerticalAlignment VAlign = VAlign_Center)
	{
		UHorizontalBoxSlot *BoxSlot = Row->AddChildToHorizontalBox(Child);
		BoxSlot->SetPadding(Padding);
		BoxSlot->SetVerticalAlignment(VAlign);
		return BoxSlot;
	}

	UVerticalBoxSlot *AddToColumn(UVerticalBox *Column, UWidget *Child, const FMargin &Padding = FMargin(0.0f))
	{
		UVerticalBoxSlot *BoxSlot = Column->AddChildToVerticalBox(Child);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	void SetShown(UWidget *Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UPauseMenuWidget *UPauseMenuWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[PauseMenu] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	if (Root->HasActiveScreen())
	{
		return nullptr; // one screen at a time: the menu is up already, or another screen is
	}
	return Cast<UPauseMenuWidget>(Root->PushToMenu(UPauseMenuWidget::StaticClass()));
}

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPauseMenuWidget::BuildTree()
{
	using namespace PauseMenuStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	// Top line: the place, then its money.
	UHorizontalBox *Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Header);
	UVerticalBox *Place = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	SubtitleText = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	AddToColumn(Place, TitleText);
	AddToColumn(Place, SubtitleText);
	AddToRow(Header, MakeSized(WidgetTree, PageWidth + ColumnGap, 0.0f, Place), FMargin(0.0f), VAlign_Top);
	for (int32 Index = 0; Index < MaxChips; ++Index)
	{
		ChipValues.Add(MakeText(WidgetTree, SubtitleFontSize, TextColor, true));
		ChipLabels.Add(MakeText(WidgetTree, SmallFontSize, DimColor));
		AddToRow(Header, ChipValues.Last(), FMargin(0.0f, ChipTopInset, WordGap, 0.0f), VAlign_Top);
		AddToRow(Header, ChipLabels.Last(), FMargin(0.0f, ChipTopInset, ChipGap, 0.0f), VAlign_Top);
	}

	// Pages, party, side panel.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox *PagesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const int32 PageCount = WoRPauseMenu::BuildPages(false, true).Num();
	for (int32 Index = 0; Index < PageCount; ++Index)
	{
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetBrushColor(PlateColor);
		Row->SetPadding(FMargin(RowTextInset, 0.0f));
		Row->SetVerticalAlignment(VAlign_Center);
		Row->SetHorizontalAlignment(HAlign_Left);
		UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
		Row->SetContent(Label);
		PageRows.Add(Row);
		PageLabels.Add(Label);
		AddToColumn(PagesBox, MakeSized(WidgetTree, PageWidth, RowHeight, Row), FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	AddToRow(Body, PagesBox, FMargin(0.0f), VAlign_Top);

	UVerticalBox *PartyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		MemberCards.Add(BuildMemberCard(PartyBox));
	}
	AddToRow(Body, MakeSized(WidgetTree, CardWidth, 0.0f, PartyBox), FMargin(ColumnGap, 0.0f, 0.0f, 0.0f), VAlign_Top);

	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelBox);
	PanelHeading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	PanelBody = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	PanelRefusal = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	HoldHint = MakeWrappedText(WidgetTree, LineFontSize, WarnColor, true);
	HoldHint->SetText(LOCTEXT("HoldHint", "Hold A or Enter to quit. B or Esc goes back."));
	HoldBar = MakeBar(WidgetTree, WarnColor);
	ExtractNote = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	ExtractNote->SetText(WoRPauseMenu::GetExtractNote());
	AddToColumn(PanelBox, PanelHeading);
	AddToColumn(PanelBox, PanelBody, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelBox, PanelRefusal, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelBox, HoldHint, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(PanelBox, MakeSized(WidgetTree, PanelInnerWidth, HoldBarHeight, HoldBar), FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelBox, ExtractNote, FMargin(0.0f, NoteGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Top);

	// Bottom right: the hints.
	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "A / Enter  Select        B / Esc  Resume"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

UPauseMenuWidget::FMemberCard UPauseMenuWidget::BuildMemberCard(UVerticalBox *Into)
{
	using namespace PauseMenuStyle;

	FMemberCard Card;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(PanelColor);
	Frame->SetPadding(FMargin(CardPadding));
	UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Columns);

	// Left: who they are, HP and EP.
	UVerticalBox *Who = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox *NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, NameFontSize, TextColor, true);
	UTextBlock *Tier = MakeText(WidgetTree, LineFontSize, DimColor, true);
	AddToRow(NameRow, Name);
	AddToRow(NameRow, Tier, FMargin(SectionGap, 0.0f, 0.0f, 0.0f));
	UTextBlock *ClassLine = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Who, NameRow);
	AddToColumn(Who, ClassLine);
	const auto AddVital = [this, Who](const FText &Label, const FLinearColor &Color, TWeakObjectPtr<UProgressBar> &OutBar, TWeakObjectPtr<UTextBlock> &OutValue)
	{
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		LabelText->SetText(Label);
		UProgressBar *Bar = MakeBar(WidgetTree, Color);
		// The value sits centred inside its bar on a dark backing, as on the battle HUD's cards (Crown, 2026-11-13).
		UTextBlock *Value = MakeText(WidgetTree, SmallFontSize, TextColor, true);
		UBorder *Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Backing->SetBrushColor(ValueBackingColor);
		Backing->SetPadding(FMargin(WordGap, 0.0f));
		Backing->SetContent(Value);
		UOverlay *BarWithValue = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		BarWithValue->AddChildToOverlay(Bar)->SetHorizontalAlignment(HAlign_Fill);
		UOverlaySlot *BackingSlot = BarWithValue->AddChildToOverlay(Backing);
		BackingSlot->SetHorizontalAlignment(HAlign_Center);
		BackingSlot->SetVerticalAlignment(VAlign_Center);
		AddToRow(Row, MakeSized(WidgetTree, BarLabelWidth, 0.0f, LabelText));
		AddToRow(Row, MakeSized(WidgetTree, BarWidth, BarHeight, BarWithValue));
		AddToColumn(Who, Row, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		OutBar = Bar;
		OutValue = Value;
	};
	AddVital(LOCTEXT("HP", "HP"), HPColor, Card.HPBar, Card.HPValue);
	AddVital(LOCTEXT("EP", "EP"), EPColor, Card.EPBar, Card.EPValue);
	AddToRow(Columns, Who, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Right: the three pillars.
	UVerticalBox *Pillars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const auto AddPillar = [this, Pillars](const FText &Label, TWeakObjectPtr<UTextBlock> &OutValue)
	{
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, SmallFontSize, DimColor);
		LabelText->SetText(Label);
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, TextColor, true);
		AddToRow(Row, MakeSized(WidgetTree, PillarLabelWidth, 0.0f, LabelText));
		AddToRow(Row, Value);
		AddToColumn(Pillars, Row, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
		OutValue = Value;
	};
	AddPillar(LOCTEXT("Mind", "Mind"), Card.Mind);
	AddPillar(LOCTEXT("Body", "Body"), Card.Body);
	AddPillar(LOCTEXT("Spirit", "Spirit"), Card.Spirit);
	AddToRow(Columns, Pillars, FMargin(0.0f), VAlign_Top);

	AddToColumn(Into, Frame, FMargin(0.0f, 0.0f, 0.0f, CardGap));
	Card.Root = Frame;
	Card.Name = Name;
	Card.Tier = Tier;
	Card.ClassLine = ClassLine;
	return Card;
}

void UPauseMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	SelectedIndex = 0;
	bConfirmingQuit = false;
	RefreshData();
	RefreshSelection();
	SetPartyStripHidden(true);
	UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] Opened (%s)"), bInRun ? TEXT("in a run") : TEXT("hub"));
}

void UPauseMenuWidget::NativeOnDeactivated()
{
	StopQuitHold();
	bConfirmingQuit = false;
	SetPartyStripHidden(false);
	Super::NativeOnDeactivated();
}

UWidget *UPauseMenuWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UPauseMenuWidget *>(this);
}

bool UPauseMenuWidget::NativeOnHandleBackAction()
{
	if (bConfirmingQuit)
	{
		StopQuitHold();
		bConfirmingQuit = false;
		RefreshSelection();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

void UPauseMenuWidget::RefreshData()
{
	const UWorld *World = GetWorld();
	const FPauseMenuHeader Header = WoRPauseMenu::BuildHeader(World);
	bInRun = Header.bInRun;
	Pages = WoRPauseMenu::BuildPages(bInRun, !World || World->GetNetMode() != NM_Client);

	if (TitleText && SubtitleText)
	{
		TitleText->SetText(Header.Title);
		SubtitleText->SetText(Header.Subtitle);
	}
	for (int32 Index = 0; Index < ChipValues.Num(); ++Index)
	{
		const bool bShown = Header.Chips.IsValidIndex(Index);
		PauseMenuStyle::SetShown(ChipValues[Index], bShown);
		PauseMenuStyle::SetShown(ChipLabels[Index], bShown);
		if (bShown)
		{
			ChipValues[Index]->SetText(Header.Chips[Index].Value);
			ChipLabels[Index]->SetText(Header.Chips[Index].Label);
		}
	}
	for (int32 Index = 0; Index < PageLabels.Num() && Index < Pages.Num(); ++Index)
	{
		PageLabels[Index]->SetText(Pages[Index].Label);
	}
	PauseMenuStyle::SetShown(ExtractNote, bInRun);

	const TArray<FPauseMenuMember> Members = WoRPauseMenu::BuildParty(World);
	for (int32 Index = 0; Index < MemberCards.Num(); ++Index)
	{
		const FMemberCard &Card = MemberCards[Index];
		const bool bShown = Members.IsValidIndex(Index);
		PauseMenuStyle::SetShown(Card.Root.Get(), bShown);
		if (!bShown || !Card.Name.IsValid() || !Card.HPBar.IsValid() || !Card.EPBar.IsValid())
		{
			continue;
		}
		const FPauseMenuMember &Member = Members[Index];
		const float HPFraction = Member.MaxHP > 0 ? static_cast<float>(Member.HP) / Member.MaxHP : 0.0f;
		const FText ValueFormat = LOCTEXT("VitalValue", "{0}/{1}");
		Card.Name->SetText(Member.Name);
		// The tier beside the name, "Garrick (C)", as on the battle HUD's cards.
		Card.Tier->SetText(Member.Tier.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("TierBesideName", "({0})"), Member.Tier));
		Card.ClassLine->SetText(Member.ClassLine);
		Card.HPBar->SetPercent(HPFraction);
		Card.HPBar->SetFillColorAndOpacity(HPFraction < PauseMenuStyle::LowHPFraction ? PauseMenuStyle::LowHPColor : PauseMenuStyle::HPColor);
		Card.HPValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Member.HP), FText::AsNumber(Member.MaxHP)));
		Card.EPBar->SetPercent(Member.MaxEP > 0 ? static_cast<float>(Member.EP) / Member.MaxEP : 0.0f);
		Card.EPValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Member.EP), FText::AsNumber(Member.MaxEP)));
		Card.Mind->SetText(FText::AsNumber(Member.Mind));
		Card.Body->SetText(FText::AsNumber(Member.Body));
		Card.Spirit->SetText(FText::AsNumber(Member.Spirit));
	}
}

void UPauseMenuWidget::RefreshSelection()
{
	for (int32 Index = 0; Index < PageRows.Num() && Index < Pages.Num(); ++Index)
	{
		PageRows[Index]->SetBrushColor(Index == SelectedIndex ? PauseMenuStyle::FocusColor : PauseMenuStyle::PlateColor);
		PageLabels[Index]->SetColorAndOpacity(FSlateColor(Pages[Index].CanOpen() ? PauseMenuStyle::TextColor : PauseMenuStyle::OffColor));
	}
	if (!Pages.IsValidIndex(SelectedIndex) || !PanelHeading)
	{
		return;
	}
	const FPauseMenuPage &Page = Pages[SelectedIndex];
	PanelHeading->SetText(Page.Label);
	PanelBody->SetText(Page.Preview);
	PanelRefusal->SetText(Page.Refusal);
	PauseMenuStyle::SetShown(PanelRefusal, !Page.CanOpen());
	PauseMenuStyle::SetShown(HoldHint, bConfirmingQuit);
	PauseMenuStyle::SetShown(HoldBar, bConfirmingQuit);
}

void UPauseMenuWidget::MoveSelection(int32 Delta)
{
	if (Pages.Num() > 0)
	{
		SelectIndex(((SelectedIndex + Delta) % Pages.Num() + Pages.Num()) % Pages.Num());
	}
}

void UPauseMenuWidget::SelectIndex(int32 Index)
{
	if (!Pages.IsValidIndex(Index) || Index == SelectedIndex)
	{
		return;
	}
	StopQuitHold();
	bConfirmingQuit = false;
	SelectedIndex = Index;
	RefreshSelection();
}

void UPauseMenuWidget::PressSelected()
{
	if (!Pages.IsValidIndex(SelectedIndex))
	{
		return;
	}
	const FPauseMenuPage Page = Pages[SelectedIndex];
	if (!Page.CanOpen())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] %s refused: %s"), *Page.Label.ToString(), *Page.Refusal.ToString());
		return;
	}
	OpenPage(Page);
}

void UPauseMenuWidget::ReleaseSelected()
{
	StopQuitHold();
}

void UPauseMenuWidget::OpenPage(const FPauseMenuPage &Page)
{
	switch (Page.Page)
	{
	case EPauseMenuPage::Resume:
		DeactivateWidget();
		break;
	case EPauseMenuPage::Inventory:
	case EPauseMenuPage::Party:
	case EPauseMenuPage::Journal:
		OpenPlaceholder(Page.Page);
		break;
	case EPauseMenuPage::Quit:
		if (bConfirmingQuit)
		{
			BeginQuitHold();
		}
		else
		{
			bConfirmingQuit = true;
			RefreshSelection();
		}
		break;
	default: // Loadout, Character, Settings: refused by the model until their screens land
		break;
	}
}

void UPauseMenuWidget::OpenPlaceholder(EPauseMenuPage Page)
{
	const TWeakObjectPtr<APlayerController> WeakPC = GetOwningPlayer();
	const auto OpenIt = [WeakPC, Page]()
	{
		APlayerController *PC = WeakPC.Get();
		if (!PC)
		{
			return;
		}
		// TODO: B-441 (inventory) and the party and journal screens replace these viewport placeholders.
		switch (Page)
		{
		case EPauseMenuPage::Inventory:
			UInventoryWidget::Open(PC);
			break;
		case EPauseMenuPage::Party:
			UPartyControlWidget::Open(PC);
			break;
		case EPauseMenuPage::Journal:
			UMemoryJournalWidget::Open(PC);
			break;
		default:
			break;
		}
	};
	// The placeholders set their own input mode, so they open only after the root layout has let this screen go
	// and handed input back; before DeactivateWidget, while the layout still counts this screen.
	const AWoRHubController *Hub = Cast<AWoRHubController>(WeakPC.Get());
	if (UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr)
	{
		Root->RunWhenAllClosed(OpenIt);
		DeactivateWidget();
		return;
	}
	DeactivateWidget();
	OpenIt();
}

void UPauseMenuWidget::BeginQuitHold()
{
	UWorld *World = GetWorld();
	if (!World || IsHoldingQuit())
	{
		return;
	}
	QuitHoldStartSeconds = FPlatformTime::Seconds();
	World->GetTimerManager().SetTimer(QuitHoldTimer, FTimerDelegate::CreateUObject(this, &UPauseMenuWidget::TickQuitHold),
									  PauseMenuStyle::HoldTickSeconds, /*bLoop=*/true);
}

void UPauseMenuWidget::StopQuitHold()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QuitHoldTimer);
	}
	QuitHoldTimer.Invalidate();
	if (HoldBar)
	{
		HoldBar->SetPercent(0.0f);
	}
}

void UPauseMenuWidget::TickQuitHold()
{
	const float Fraction = static_cast<float>((FPlatformTime::Seconds() - QuitHoldStartSeconds) / WoRPauseMenu::QUIT_HOLD_SECONDS);
	if (HoldBar)
	{
		HoldBar->SetPercent(FMath::Clamp(Fraction, 0.0f, 1.0f));
	}
	if (Fraction < 1.0f)
	{
		return;
	}
	UWorld *World = GetWorld();
	APlayerController *PC = GetOwningPlayer();
	UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] Quit held to the end (%s)"), bInRun ? TEXT("in a run: it is lost, D-176") : TEXT("hub"));
	DeactivateWidget(); // stops the hold
	WoRPauseMenu::Quit(World, PC);
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace PauseMenuStyle;
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
	if (IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressSelected();
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

FReply UPauseMenuWidget::NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (PauseMenuStyle::IsConfirmKey(InKeyEvent.GetKey()))
	{
		ReleaseSelected();
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

int32 UPauseMenuWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < PageRows.Num(); ++Index)
	{
		if (PageRows[Index] && PageRows[Index]->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UPauseMenuWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys; a quit hold keeps its row.
	if (!InMouseEvent.GetCursorDelta().IsZero() && !IsHoldingQuit())
	{
		SelectIndex(FindRowUnder(InMouseEvent));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const int32 Row = FindRowUnder(InMouseEvent);
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || Row == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	SelectIndex(Row);
	PressSelected();
	// A quit hold ends when the button comes up, wherever the cursor is by then.
	return IsHoldingQuit() ? FReply::Handled().CaptureMouse(TakeWidget()) : FReply::Handled();
}

FReply UPauseMenuWidget::NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ReleaseSelected();
		return FReply::Handled().ReleaseMouseCapture(); // always: a hold that already stopped must not keep the capture
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UPauseMenuWidget::NativeOnRemovedFromFocusPath(const FFocusEvent &InFocusEvent)
{
	// With focus gone the key-up never arrives, so the hold must not run on to the end by itself.
	StopQuitHold();
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
}

void UPauseMenuWidget::SetPartyStripHidden(bool bHidden)
{
	if (!bHidden)
	{
		for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility> &Hidden : HiddenStrips)
		{
			if (UWidget *Strip = Hidden.Key.Get())
			{
				Strip->SetVisibility(Hidden.Value);
			}
		}
		HiddenStrips.Reset();
		return;
	}
	if (HiddenStrips.Num() > 0 || !GetWorld())
	{
		return;
	}
	TArray<UUserWidget *> Strips;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Strips, UPartyDisplayWidget::StaticClass(), /*TopLevelOnly=*/false);
	for (UUserWidget *Strip : Strips)
	{
		HiddenStrips.Emplace(Strip, Strip->GetVisibility());
		Strip->SetVisibility(ESlateVisibility::Collapsed);
	}
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPauseMenuOpenCommand(
	TEXT("wor.UI.Pause"),
	TEXT("B-439: open the game menu, as the Menu button does (hub and trial; not in a battle)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UPauseMenuWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[PauseMenu] wor.UI.Pause: not opened (no hub controller with a root layout, or a screen is already up)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
