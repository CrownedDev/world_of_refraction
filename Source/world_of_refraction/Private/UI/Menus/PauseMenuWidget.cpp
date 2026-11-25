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
#include "Player/WoRFrontEndGameMode.h"
#include "Player/WoRHubController.h"
#include "Player/WoRPlayerState.h"
#include "Save/SaveSubsystem.h"
#include "TimerManager.h"
#include "UI/Menus/InventoryScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/JournalScreenWidget.h"
#include "UI/Menus/CharacterSheetWidget.h"
#include "UI/Menus/SettingsScreenWidget.h"
#include "UI/Menus/TitleScreenWidget.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "UI/Hub/FormsScreenWidget.h"
#include "UI/Party/PartyControlWidget.h"
#include "UI/Party/PartyScreenWidget.h"
#include "UI/WoRIconTable.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPauseMenuWidget"

namespace PauseMenuStyle
{
	// This screen's own layout (B-632: screen_mockups/menus/pause/v2, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float HeaderGap = 48.0f;
	constexpr float PagesWidth = 602.0f;
	constexpr float ColumnGap = 27.0f;
	constexpr float ColumnPadding = 24.0f;
	constexpr float RowHeight = 78.0f;
	constexpr float RowGap = 10.0f;
	/** Leave trial sits apart from the pages above it. */
	constexpr float QuitRowGap = 28.0f;
	constexpr float RowTextInset = 24.0f;
	constexpr float StatLabelWidth = 270.0f;
	constexpr float StatGap = 9.0f;
	constexpr float DividerHeight = 2.0f;
	constexpr float BadgeSize = 33.0f;
	constexpr float MemberNameWidth = 132.0f;
	constexpr float MemberClassWidth = 210.0f;
	constexpr float PillarLabelWidth = 84.0f;
	constexpr float PillarWidth = 165.0f;
	constexpr float HoldBarHeight = 15.0f;
	constexpr float HoldBarWidth = 408.0f;

	constexpr float HoldTickSeconds = 0.03f;
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

	// Top: "Paused" and the place under it (B-632: no balances).
	TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	TitleText->SetText(LOCTEXT("Paused", "Paused"));
	SubtitleText = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	AddToColumn(Main, TitleText);
	AddToColumn(Main, SubtitleText);

	// The two columns: the pages, and the run with the lit page's note.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, PagesWidth, 0.0f, BuildPagesColumn()), FMargin(0.0f), VAlign_Fill);
	AddToRow(Body, BuildRunColumn(), FMargin(ColumnGap, 0.0f, 0.0f, 0.0f), VAlign_Fill)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Bottom right: the hints.
	UTextBlock *Hints = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{Confirm}  Open        {Back}  Resume"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

UWidget *UPauseMenuWidget::BuildPagesColumn()
{
	using namespace PauseMenuStyle;

	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(PanelColor);
	Frame->SetPadding(FMargin(ColumnPadding));
	UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->SetContent(Column);
	UTextBlock *Heading = MakeText(WidgetTree, LineFontSize, TextColor, true);
	Heading->SetText(LOCTEXT("MenuHeading", "Menu"));
	AddToColumn(Column, Heading, FMargin(0.0f, 0.0f, 0.0f, SectionGap));

	const TArray<FPauseMenuPage> MostRows = WoRPauseMenu::BuildPages(false, true, FText::GetEmpty(), false, /*bCanJoin=*/true, false);
	for (int32 Index = 0; Index < MostRows.Num(); ++Index)
	{
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetBrushColor(PlateColor);
		Row->SetPadding(FMargin(RowTextInset, 0.0f));
		Row->SetVerticalAlignment(VAlign_Center);
		Row->SetHorizontalAlignment(HAlign_Left);
		UVerticalBox *Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
		UTextBlock *SubLine = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Lines, Label);
		AddToColumn(Lines, SubLine);
		Row->SetContent(Lines);
		PageRows.Add(Row);
		PageLabels.Add(Label);
		PageSubLines.Add(SubLine);
		// The last row (Leave trial / Quit) stands apart.
		const bool bQuitRow = MostRows[Index].Page == EPauseMenuPage::Quit;
		AddToColumn(Column, MakeSized(WidgetTree, PagesWidth - ColumnPadding - ColumnPadding, RowHeight, Row),
					FMargin(0.0f, bQuitRow ? QuitRowGap - RowGap : 0.0f, 0.0f, RowGap));
	}
	return Frame;
}

UWidget *UPauseMenuWidget::BuildRunColumn()
{
	using namespace PauseMenuStyle;

	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(PanelColor);
	Frame->SetPadding(FMargin(ColumnPadding));
	UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->SetContent(Column);
	const auto AddDivider = [this](UVerticalBox *Into)
	{
		UBorder *Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Line->SetBrushColor(PlateColor);
		USizeBox *Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(DividerHeight);
		Box->AddChild(Line);
		AddToColumn(Into, Box, FMargin(0.0f, SectionGap));
	};

	// This run: in a run only.
	UVerticalBox *Run = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RunBlock = Run;
	UTextBlock *RunHeading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	RunHeading->SetText(LOCTEXT("ThisRun", "This run"));
	RunPlace = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Run, RunHeading);
	AddToColumn(Run, RunPlace, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	const auto AddStat = [this, Run](const FText &Label, const FLinearColor &ValueColor) -> UTextBlock *
	{
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, LineFontSize, DimColor);
		LabelText->SetText(Label);
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, ValueColor, true);
		AddToRow(Row, MakeSized(WidgetTree, StatLabelWidth, 0.0f, LabelText));
		AddToRow(Row, Value);
		AddToColumn(Run, Row, FMargin(0.0f, 0.0f, 0.0f, StatGap));
		return Value;
	};
	FightsWonValue = AddStat(LOCTEXT("FightsWon", "Fights won"), TextColor);
	RunGoldValue = AddStat(LOCTEXT("RunGold", "Run gold"), WarnColor);
	ItemsFoundValue = AddStat(LOCTEXT("ItemsFound", "Items found"), TextColor);
	TimeValue = AddStat(LOCTEXT("Time", "Time"), TextColor);
	AddDivider(Run);
	AddToColumn(Column, Run);

	// Each member's Mind, Body and Spirit now.
	UTextBlock *PillarsHeading = MakeText(WidgetTree, LineFontSize, TextColor, true);
	PillarsHeading->SetText(LOCTEXT("PillarsHeading", "Mind  ·  Body  ·  Spirit"));
	AddToColumn(Column, PillarsHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	UVerticalBox *Members = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		MemberRows.Add(BuildMemberRow(Members));
	}
	AddToColumn(Column, Members);
	AddDivider(Column);

	// The lit page's note, why it can't open, or the quit question.
	PanelHeading = MakeText(WidgetTree, LineFontSize, TextColor, true);
	PanelBody = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	PanelRefusal = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	HoldHint = MakeHintText(WidgetTree, LineFontSize, WarnColor, true);
	HoldHint->SetAutoWrapText(true);
	// The hold / press hint is set in RefreshSelection (B-453c: it follows the setting).
	HoldBar = MakeBar(WidgetTree, WarnColor);
	AddToColumn(Column, PanelHeading);
	AddToColumn(Column, PanelBody, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(Column, PanelRefusal, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(Column, HoldHint, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(Column, MakeSized(WidgetTree, HoldBarWidth, HoldBarHeight, HoldBar), FMargin(0.0f, LineGap, 0.0f, 0.0f))
		->SetHorizontalAlignment(HAlign_Left);
	return Frame;
}

UPauseMenuWidget::FMemberRow UPauseMenuWidget::BuildMemberRow(UVerticalBox *Into)
{
	using namespace PauseMenuStyle;

	FMemberRow Member;
	UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UImage *Badge = MakeIcon(WidgetTree, BadgeSize);
	UTextBlock *Name = MakeText(WidgetTree, LineFontSize, TextColor, true);
	UTextBlock *ClassName = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToRow(Row, MakeSized(WidgetTree, BadgeSize, BadgeSize, Badge), FMargin(0.0f, 0.0f, LineGap, 0.0f));
	AddToRow(Row, MakeSized(WidgetTree, MemberNameWidth, 0.0f, Name));
	AddToRow(Row, MakeSized(WidgetTree, MemberClassWidth, 0.0f, ClassName));
	const auto AddPillar = [this, Row](const FText &Label) -> UTextBlock *
	{
		UHorizontalBox *Pillar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, SmallFontSize, DimColor);
		LabelText->SetText(Label);
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, TextColor, true);
		AddToRow(Pillar, MakeSized(WidgetTree, PillarLabelWidth, 0.0f, LabelText));
		AddToRow(Pillar, Value);
		AddToRow(Row, MakeSized(WidgetTree, PillarWidth, 0.0f, Pillar));
		return Value;
	};
	Member.Mind = AddPillar(LOCTEXT("Mind", "Mind"));
	Member.Body = AddPillar(LOCTEXT("Body", "Body"));
	Member.Spirit = AddPillar(LOCTEXT("Spirit", "Spirit"));
	AddToColumn(Into, Row, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	Member.Root = Row;
	Member.Badge = Badge;
	Member.Name = Name;
	Member.ClassName = ClassName;
	return Member;
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
	Summary = WoRPauseMenu::BuildRunSummary(World);
	bInRun = Summary.bInRun;
	const bool bGuest = World && World->GetNetMode() == NM_Client;
	const AWoRPlayerState *Player = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AWoRPlayerState>() : nullptr;
	Pages = WoRPauseMenu::BuildPages(bInRun, !bGuest, WoRPauseMenu::GetTrackedLine(World),
									   WoRFrontEnd::HasFrontEnd(World ? World->GetGameInstance() : nullptr), bGuest, Player && Player->HasAnyClaim());
	UE_LOG(LogWoRUI, Verbose, TEXT("%s"), *WoRPauseMenu::GetPauseMenuString(World));

	if (SubtitleText)
	{
		SubtitleText->SetText(Summary.Place);
	}
	for (int32 Index = 0; Index < PageLabels.Num(); ++Index)
	{
		PauseMenuStyle::SetShown(PageRows[Index], Pages.IsValidIndex(Index)); // B-562: the Join row is a guest's only
		if (Pages.IsValidIndex(Index))
		{
			PageLabels[Index]->SetText(Pages[Index].Label);
			PageSubLines[Index]->SetText(Pages[Index].SubLine);
		}
	}

	PauseMenuStyle::SetShown(RunBlock, bInRun);
	if (RunPlace)
	{
		RunPlace->SetText(Summary.Place);
		FightsWonValue->SetText(FText::AsNumber(Summary.FightsWon));
		RunGoldValue->SetText(FText::AsNumber(Summary.RunGold));
		ItemsFoundValue->SetText(FText::AsNumber(Summary.ItemsFound));
		TimeValue->SetText(WoRPauseMenu::FormatRunTime(Summary.Seconds));
	}

	const TArray<FPauseMenuMember> Members = WoRPauseMenu::BuildParty(World);
	for (int32 Index = 0; Index < MemberRows.Num(); ++Index)
	{
		const FMemberRow &Row = MemberRows[Index];
		const bool bShown = Members.IsValidIndex(Index);
		PauseMenuStyle::SetShown(Row.Root.Get(), bShown);
		if (!bShown || !Row.Name.IsValid() || !Row.Mind.IsValid())
		{
			continue;
		}
		const FPauseMenuMember &Member = Members[Index];
		PauseMenuStyle::SetIcon(Row.Badge.Get(), WoRIcons::Find(Member.ElementIcon), /*bKeepSpace=*/true);
		Row.Name->SetText(Member.Name);
		Row.ClassName->SetText(Member.ClassName);
		Row.Mind->SetText(FText::AsNumber(Member.Mind));
		Row.Body->SetText(FText::AsNumber(Member.Body));
		Row.Spirit->SetText(FText::AsNumber(Member.Spirit));
	}
}

FText UPauseMenuWidget::GetNoteHeading() const
{
	return Pages.IsValidIndex(SelectedIndex) ? Pages[SelectedIndex].Label : FText::GetEmpty();
}

FText UPauseMenuWidget::GetNoteText() const
{
	return Pages.IsValidIndex(SelectedIndex) ? Pages[SelectedIndex].Preview : FText::GetEmpty();
}

void UPauseMenuWidget::RefreshSelection()
{
	for (int32 Index = 0; Index < PageRows.Num() && Index < Pages.Num(); ++Index)
	{
		PageRows[Index]->SetBrushColor(Index == SelectedIndex ? PauseMenuStyle::FocusColor : PauseMenuStyle::PlateColor);
		// Mockup v2: Leave trial's name is amber (it ends the run); a refused page is greyed.
		const bool bRunEnder = Pages[Index].Page == EPauseMenuPage::Quit && bInRun;
		PageLabels[Index]->SetColorAndOpacity(FSlateColor(!Pages[Index].CanOpen() ? PauseMenuStyle::OffColor
														  : bRunEnder			  ? PauseMenuStyle::WarnColor
																				  : PauseMenuStyle::TextColor));
		PageSubLines[Index]->SetColorAndOpacity(FSlateColor(Index == SelectedIndex ? PauseMenuStyle::TextColor : PauseMenuStyle::DimColor));
	}
	if (!Pages.IsValidIndex(SelectedIndex) || !PanelHeading)
	{
		return;
	}
	// B-632 (mockup v2): the note always shows (B-543's Help toggle is gone), with a refusal or the quit question under it.
	const FPauseMenuPage &Page = Pages[SelectedIndex];
	PanelHeading->SetText(Page.Label);
	PanelBody->SetText(Page.Preview);
	PanelRefusal->SetText(Page.Refusal);
	PauseMenuStyle::SetShown(PanelRefusal, !Page.CanOpen());
	const bool bHold = IsHoldToConfirmOn(); // B-453c (D-190)
	WoRKeyHints::SetHint(HoldHint, bHold ? LOCTEXT("HoldHint", "Hold {Confirm} to quit.")
							: LOCTEXT("PressHint", "Press {Confirm} again to quit. {Back} goes back."));
	PauseMenuStyle::SetShown(HoldHint, bConfirmingQuit);
	PauseMenuStyle::SetShown(HoldBar, bConfirmingQuit && bHold);
}

bool UPauseMenuWidget::IsHoldToConfirmOn() const
{
	const USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	return !Saves || Saves->IsHoldToConfirmOn(); // no save: the safe default, a hold
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
		ULoadoutV2ScreenWidget::OpenWithLoadout(GetOwningPlayer()); // over this menu: Back returns here. D-249: the one screen, the loadout in
		break;
	case EPauseMenuPage::Character:
		UCharacterSheetWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Forms:
		UFormsScreenWidget::Open(GetOwningPlayer()); // B-536, over this menu: Back returns here
		break;
	case EPauseMenuPage::Settings:
		USettingsScreenWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Inventory:
		// D-249: Inventory and Loadout are one screen; this page opens it with the loadout out.
		// OBSOLETE (D-249; delete after Crown's PIE pass and pieces 2-3): UInventoryScreenWidget. Its Give / Store / Take
		// are not on the merged screen yet; until then it opens with wor.UI.InventoryScreen.
		ULoadoutV2ScreenWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Journal:
		UJournalScreenWidget::Open(GetOwningPlayer());
		break;
	case EPauseMenuPage::Party:
		UPartyScreenWidget::Open(GetOwningPlayer()); // B-458, over this menu: Back returns here
		break;
	case EPauseMenuPage::Join:
		UTitleScreenWidget::OpenToJoin(GetOwningPlayer()); // B-562, over this menu: Back returns here
		break;
	case EPauseMenuPage::Quit:
		if (bConfirmingQuit && !IsHoldToConfirmOn())
		{
			// B-453c: hold to confirm is off, so the press on the confirm quits (the confirm step stays, D-176).
			UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] Quit confirmed with a press (%s)"), bInRun ? TEXT("in a run: it is lost, D-176") : TEXT("hub"));
			UWorld *World = GetWorld();
			APlayerController *PC = GetOwningPlayer();
			DeactivateWidget();
			WoRPauseMenu::Quit(World, PC);
		}
		else if (bConfirmingQuit)
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
	if (IsBackKey(Key) || Key == EKeys::Tab) // B-536: Tab opened it, Tab closes it (a quit question first, as Back)
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
	for (int32 Index = 0; Index < PageRows.Num() && Index < Pages.Num(); ++Index) // a hidden row keeps its old geometry
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
