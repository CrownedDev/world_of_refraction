// PauseMenuWidget.cpp

#include "UI/Menus/PauseMenuWidget.h"

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
#include "UI/Menus/InventoryScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/JournalScreenWidget.h"
#include "UI/Menus/CharacterSheetWidget.h"
#include "UI/Menus/SettingsScreenWidget.h"
#include "UI/Menus/LoadoutScreenWidget.h"
#include "UI/Party/PartyControlWidget.h"
#include "UI/Party/PartyScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPauseMenuWidget"

namespace PauseMenuStyle
{
	// This screen's own layout (screen_mockups/menus/pause, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
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
	constexpr float BarHeight = BarWithValueHeight;
	constexpr float HoldBarHeight = 15.0f;
	constexpr float BarLabelWidth = 51.0f;
	constexpr float PillarLabelWidth = 105.0f;
	constexpr float PanelGap = 60.0f;
	constexpr float PanelWidth = 408.0f;
	constexpr float PanelPadding = 27.0f;
	constexpr float PanelInnerWidth = PanelWidth - PanelPadding - PanelPadding;
	constexpr float NoteGap = 27.0f;
	constexpr float ChipGap = 36.0f;
	constexpr float ChipTopInset = 15.0f;

	constexpr float LowHPFraction = 0.3f;
	constexpr float HoldTickSeconds = 0.03f;
	constexpr int32 MaxChips = 3;
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
	UPauseMenuWidget *Menu = Cast<UPauseMenuWidget>(Root->PushToMenu(UPauseMenuWidget::StaticClass()));
	if (Menu)
	{
		// The stack may hand back a pooled instance, and may activate it only after its transition: a fresh
		// open starts on Resume either way.
		Menu->SelectedIndex = 0;
		Menu->RefreshSelection();
	}
	return Menu;
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
		UProgressBar *Bar = nullptr;
		UTextBlock *Value = nullptr;
		UWidget *BarWithValue = MakeBarWithValue(WidgetTree, Color, Bar, Value);
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
	// The selection is kept: coming back from a page (the loadout screen) lands on that page's row. Open resets it.
	bConfirmingQuit = false;
	RefreshData();
	SelectedIndex = Pages.IsValidIndex(SelectedIndex) ? SelectedIndex : 0;
	RefreshSelection();
	UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] Opened (%s)"), bInRun ? TEXT("in a run") : TEXT("hub"));
}

void UPauseMenuWidget::NativeOnDeactivated()
{
	StopQuitHold();
	bConfirmingQuit = false;
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
	Pages = WoRPauseMenu::BuildPages(bInRun, !World || World->GetNetMode() != NM_Client, WoRPauseMenu::GetTrackedLine(World));

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
		Card.HPBar->SetFillColorAndOpacity(HPFraction < PauseMenuStyle::LowHPFraction ? PauseMenuStyle::LowHPBarColor : PauseMenuStyle::HPColor);
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
	case EPauseMenuPage::Loadout:
		ULoadoutScreenWidget::Open(GetOwningPlayer()); // over this menu: Back returns here
		break;
	case EPauseMenuPage::Character:
		UCharacterSheetWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Settings:
		USettingsScreenWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Inventory:
		UInventoryScreenWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Journal:
		UJournalScreenWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Party:
		UPartyScreenWidget::Open(GetOwningPlayer()); // B-458, over this menu: Back returns here
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
	default:
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
		// OBSOLETE (B-458): the Party page opens UPartyScreenWidget; remove after Crown's PIE pass.
		switch (Page)
		{
		case EPauseMenuPage::Party:
			UPartyControlWidget::Open(PC);
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
