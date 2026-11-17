// TitleScreenWidget.cpp

#include "UI/Menus/TitleScreenWidget.h"

#include "Algo/Count.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"   // B-535: GameDefaultMap
#include "UObject/SoftObjectPath.h"
#include "Player/WoRFrontEndGameMode.h"
#include "Player/WoRHubController.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "Character/PlayerPicks.h"
#include "UI/Menus/CreditsScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/NewGameScreenWidget.h"
#include "UI/Menus/SettingsScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRTitleScreenWidget"

namespace TitleScreenStyle
{
	// This screen's own layout (screen_mockups/front/title, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr int32 GameTitleFontSize = 60;
	constexpr int32 GameSubtitleFontSize = 24;
	/** The subtitle's spread-out letters (1/1000 em). */
	constexpr int32 SubtitleLetterSpacing = 600;
	/** The mockup's left margin (72 px at 720p). */
	constexpr float ScreenPaddingX = 108.0f;
	constexpr float TitleGap = 36.0f;
	constexpr float RowsTopGap = 96.0f;
	/** B-546: the slots stand centred while there is nothing beside them. With a character figure (Crown: it goes on the
	 *  right), set HAlign_Left and put the figure on the right of Main's slot row. */
	constexpr EHorizontalAlignment SlotsAlignment = HAlign_Center;
	constexpr float ListWidth = 720.0f;
	constexpr float RowHeight = 96.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowPaddingX = 24.0f;
	/** shortcut: room for the boxes one save file can fill (a slot and the +); stage 3 (a save per character) puts the
	 *  slots in a ScrollBox of GetMaxSlots boxes. */
	constexpr int32 MaxSlotBoxes = 4;
	/** Settings, Credits, Quit. */
	constexpr int32 MaxButtons = 3;
	constexpr float ButtonWidth = 240.0f;
	constexpr float ButtonHeight = 60.0f;
	constexpr float ButtonGap = 24.0f;
	constexpr float PanelWidth = 900.0f;
	constexpr float PanelPadding = 36.0f;
	constexpr float AnswerWidth = 180.0f;
	constexpr float AnswerHeight = 56.0f;
	constexpr float HoldBarHeight = 12.0f;
	/** UIScreens #9: New Game over a save needs A held this long. OBSOLETE (B-546): a yes / no now. */
	constexpr float HOLD_TO_START_OVER_SECONDS = 2.0f;
	constexpr float MaxHoldStepSeconds = 0.1f;
	inline const FLinearColor DimLayerColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
	/** "Your save was reset": the mockup's amber title. */
	inline const FLinearColor NoticeTitleColor = WarnColor;
}

namespace
{
	/** Set when the player leaves the title (Continue, or New Game with no save): it doesn't open again this launch. */
	bool GTitleLeftThisLaunch = false; // OBSOLETE (B-535): no longer read; OpenAtLaunch keys on GTitleStartSeenFor. Retire after PIE
	/** B-535: the play session whose first map load OpenAtLaunch has seen (weak: a PIE run's game instance goes). */
	TWeakObjectPtr<const UGameInstance> GTitleStartSeenFor;
	TAutoConsoleVariable<bool> CVarTitleAtStart(TEXT("wor.UI.TitleAtStart"), true,
		TEXT("B-535: open the title on a play session's first map load (the default map only). 0 skips it, for testing."));
	/** B-469: the save was deleted this launch (the title says so until the new game begins). */
	bool GTitleSaveDeletedThisLaunch = false;
	/** "Your save was reset" is said once a launch. */
	bool GSaveResetNoticeShown = false;

	FText GetSaveDeletedText()
	{
		return LOCTEXT("SaveDeleted", "Your save is deleted. Pick + to begin.");
	}
}

UTitleScreenWidget::UTitleScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UTitleScreenWidget *UTitleScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UTitleScreenWidget>(Root->PushToMenu(UTitleScreenWidget::StaticClass()));
}

UTitleScreenWidget *UTitleScreenWidget::OpenToJoin(APlayerController *PC)
{
	UTitleScreenWidget *Title = Open(PC);
	if (Title)
	{
		Title->bJoining = true;
		Title->SelectedRow = 0;
		// A guest's join shows no reset notice or deleted-save line; the plain title still shows the notice later.
		GSaveResetNoticeShown &= Title->Panel != ETitlePanel::SaveReset;
		Title->Panel = ETitlePanel::None;
		Title->Status = FText::GetEmpty();
		Title->Refresh();
		UE_LOG(LogWoRUI, Log, TEXT("[Title] Opened to join a friend's game"));
	}
	return Title;
}

void UTitleScreenWidget::OpenAtLaunch(APlayerController *PC)
{
	// B-535: once per play session (the game instance: a new one each PIE run and each launch), on its first map load
	// only, and only on the game's default map (the hub), so later loads and a trial test level never open it.
	UWorld *World = PC && PC->IsLocalController() ? PC->GetWorld() : nullptr;
	const UGameInstance *GameInstance = World && World->GetNetMode() != NM_Client ? World->GetGameInstance() : nullptr; // a friend joining lands in the host's hub
	if (!GameInstance || GTitleStartSeenFor.Get() == GameInstance)
	{
		return;
	}
	GTitleStartSeenFor = GameInstance;
	FString DefaultMap;
	GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), DefaultMap, GEngineIni);
	const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (WoRTitleScreen::ShouldOpenAtStart(CVarTitleAtStart.GetValueOnGameThread(), GIsAutomationTesting, Map,
										  FSoftObjectPath(DefaultMap).GetLongPackageName()))
	{
		Open(PC);
		return;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[Title] Not opened at start: %s is not the default map %s, wor.UI.TitleAtStart is 0, or tests run"), *Map, *DefaultMap);
}

void UTitleScreenWidget::MarkLeftThisLaunch()
{
	GTitleLeftThisLaunch = true;
}

TSharedRef<SWidget> UTitleScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UTitleScreenWidget::BuildTree()
{
	using namespace TitleScreenStyle;

	UOverlay *Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(FrontBackdropColor);
	Screen->SetPadding(FMargin(ScreenPaddingX, ScreenPadding));
	UOverlaySlot *ScreenSlot = Root->AddChildToOverlay(Screen);
	ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
	ScreenSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	UTextBlock *GameTitle = MakeText(WidgetTree, GameTitleFontSize, TextColor);
	GameTitle->SetText(LOCTEXT("GameTitle", "WORLD OF REFRACTION"));
	UTextBlock *GameSubtitle = MakeText(WidgetTree, GameSubtitleFontSize, DimColor);
	FSlateFontInfo SubtitleFont = GameSubtitle->GetFont();
	SubtitleFont.LetterSpacing = SubtitleLetterSpacing;
	GameSubtitle->SetFont(SubtitleFont);
	GameSubtitle->SetText(LOCTEXT("GameSubtitle", "TALES OF THE VOID"));
	UVerticalBox *TitleBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(TitleBlock, GameTitle)->SetHorizontalAlignment(HAlign_Center);
	AddToColumn(TitleBlock, GameSubtitle, FMargin(0.0f, TitleGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
	AddToColumn(Main, TitleBlock)->SetHorizontalAlignment(HAlign_Center); // B-546: the whole screen is centred

	// The slots: the character's name over its last run's levels ("Mind 3  ·  Body 2  ·  Spirit 1"), or "+" over "New character".
	UVerticalBox *List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (int32 Index = 0; Index < MaxSlotBoxes; ++Index)
	{
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetPadding(FMargin(RowPaddingX, 0.0f));
		Row->SetVerticalAlignment(VAlign_Center);
		UVerticalBox *Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Row->SetContent(Lines);
		UTextBlock *Label = MakeText(WidgetTree, NameFontSize, TextColor, true);
		UTextBlock *Line = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Lines, Label);
		AddToColumn(Lines, Line, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		RowBoxes.Add(Row);
		RowLabels.Add(Label);
		RowLines.Add(Line);
		AddToColumn(List, MakeSized(WidgetTree, ListWidth, RowHeight, Row), FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	AddToColumn(Main, List, FMargin(0.0f, RowsTopGap, 0.0f, 0.0f))->SetHorizontalAlignment(SlotsAlignment);
	StatusText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	StatusText->SetJustification(ETextJustify::Center);
	AddToColumn(Main, MakeSized(WidgetTree, ListWidth, 0.0f, StatusText), FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(SlotsAlignment);

	UVerticalBox *Spacer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Main, Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// B-546: Settings, Credits and Quit along the bottom, centred under the slots.
	UHorizontalBox *Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < MaxButtons; ++Index)
	{
		UBorder *Button = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Button->SetHorizontalAlignment(HAlign_Center);
		Button->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Label = MakeText(WidgetTree, LineFontSize, TextColor, true);
		Button->SetContent(Label);
		ButtonBoxes.Add(Button);
		ButtonLabels.Add(Label);
		AddToRow(Buttons, MakeSized(WidgetTree, ButtonWidth, ButtonHeight, Button), FMargin(Index == 0 ? 0.0f : ButtonGap, 0.0f, 0.0f, 0.0f));
	}
	AddToColumn(Main, Buttons)->SetHorizontalAlignment(HAlign_Center);
	HintsText = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	AddToColumn(Main, HintsText, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);

	// Over everything: the dimmed screen and one centred panel (the reset notice, or a yes / no).
	UBorder *Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelLayer"));
	Dim->SetBrushColor(DimLayerColor);
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot *DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);
	PanelLayer = Dim;

	UBorder *PanelBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PanelBox->SetBrushColor(PanelColor);
	PanelBox->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelBox->SetContent(PanelColumn);
	PanelTitle = MakeText(WidgetTree, NameFontSize, TextColor, true);
	PanelBody = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(PanelColumn, PanelTitle);
	AddToColumn(PanelColumn, PanelBody, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	UVerticalBox *Versions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	auto AddVersionLine = [this, Versions](const FText &Label, TObjectPtr<UTextBlock> &OutValue)
	{
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, LineFontSize, DimColor);
		LabelText->SetText(Label);
		OutValue = MakeText(WidgetTree, LineFontSize, TextColor, true);
		AddToRow(Line, LabelText);
		UHorizontalBoxSlot *ValueSlot = AddToRow(Line, OutValue);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		AddToColumn(Versions, Line, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	};
	AddVersionLine(LOCTEXT("OldSave", "Old save"), OldVersionText);
	AddVersionLine(LOCTEXT("BuildNeeds", "This build needs"), NeededVersionText);
	AddToColumn(PanelColumn, Versions, FMargin(0.0f, SectionGap * 2.0f, 0.0f, 0.0f));
	VersionLines = Versions;

	// OBSOLETE (B-546): the hold's lines, never shown now; retire with the hold after PIE.
	UVerticalBox *Hold = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	HoldBar = MakeBar(WidgetTree, WarnColor);
	AddToColumn(Hold, MakeSized(WidgetTree, PanelWidth - PanelPadding * 2.0f, HoldBarHeight, HoldBar));
	AddToColumn(PanelColumn, Hold);
	HoldLines = Hold;

	// B-546: Yes and No, No lit first.
	UHorizontalBox *Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto AddAnswer = [this, Answers](const FText &Word, float LeftGap) -> UBorder *
	{
		UBorder *Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetHorizontalAlignment(HAlign_Center);
		Box->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Label = MakeText(WidgetTree, LineFontSize, TextColor, true);
		Label->SetText(Word);
		Box->SetContent(Label);
		AddToRow(Answers, MakeSized(WidgetTree, AnswerWidth, AnswerHeight, Box), FMargin(LeftGap, 0.0f, 0.0f, 0.0f));
		return Box;
	};
	YesBox = AddAnswer(LOCTEXT("Yes", "Yes"), 0.0f);
	NoBox = AddAnswer(LOCTEXT("No", "No"), ButtonGap);
	AddToColumn(PanelColumn, Answers, FMargin(0.0f, SectionGap * 2.0f, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
	AnswerLines = Answers;

	Dim->SetContent(MakeSized(WidgetTree, PanelWidth, 0.0f, PanelBox));
}

void UTitleScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	SelectedRow = 0;
	Status = GTitleSaveDeletedThisLaunch ? GetSaveDeletedText() : FText::GetEmpty();
	StopHold();
	const USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	const bool bShowReset = Saves && Saves->WasWiped() && !GSaveResetNoticeShown;
	GSaveResetNoticeShown |= bShowReset;
	Panel = bShowReset ? ETitlePanel::SaveReset : ETitlePanel::None;
	bSlotsRead = false; // B-554: the slots are read again each time the title opens
	Refresh();
	ShowCursorNextTick(); // D-233
	UE_LOG(LogWoRUI, Log, TEXT("[Title] Opened%s"), bShowReset ? TEXT(" (the save was reset: notice shown)") : TEXT(""));
}

void UTitleScreenWidget::NativeOnDeactivated()
{
	StopHold();
	bJoining = false; // the root layout may hand this widget out again as the plain title
	Super::NativeOnDeactivated();
}

UWidget *UTitleScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UTitleScreenWidget *>(this);
}

int32 UTitleScreenWidget::GetSlotCount() const
{
	return Algo::CountIf(Rows, [](const FTitleScreenRow &Row) { return !Row.bButton; });
}

UBorder *UTitleScreenWidget::GetRowBox(int32 Index) const
{
	const int32 Slots = GetSlotCount();
	if (Index < 0)
	{
		return nullptr;
	}
	if (Index < Slots)
	{
		return RowBoxes.IsValidIndex(Index) ? RowBoxes[Index].Get() : nullptr;
	}
	return ButtonBoxes.IsValidIndex(Index - Slots) ? ButtonBoxes[Index - Slots].Get() : nullptr;
}

void UTitleScreenWidget::Refresh()
{
	using namespace TitleScreenStyle;

	if (!bSlotsRead)
	{
		SlotSaves = WoRTitleScreen::SummarizeSlots(GetGameInstance());
		bSlotsRead = true;
	}
	Rows = bJoining ? WoRTitleScreen::BuildJoinRows(SlotSaves) : WoRTitleScreen::BuildSlotRows(SlotSaves);
	SelectedRow = FMath::Clamp(SelectedRow, 0, Rows.Num() - 1);
	const int32 LitSlot = Rows.IsValidIndex(SelectedRow) ? Rows[SelectedRow].Slot : INDEX_NONE;
	const FTitleSaveSummary *LitSave = SlotSaves.FindByPredicate([LitSlot](const FTitleSaveSummary &Entry) { return Entry.Slot == LitSlot; });
	Save = LitSave ? *LitSave : WoRTitleScreen::SummarizeSave(GetGameInstance());

	const int32 Slots = GetSlotCount();
	if (Slots > RowBoxes.Num() && !RowBoxes.IsEmpty())
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] %d slots, room for %d (MaxSlotBoxes): the rest aren't drawn"), Slots, RowBoxes.Num());
	}
	for (int32 Index = 0; Index < RowBoxes.Num(); ++Index)
	{
		const bool bShown = Index < Slots;
		SetShown(RowBoxes[Index]->GetParent(), bShown);
		if (!bShown)
		{
			continue;
		}
		const FTitleScreenRow &Row = Rows[Index];
		RowBoxes[Index]->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		RowLabels[Index]->SetText(Row.Label);
		RowLabels[Index]->SetColorAndOpacity(FSlateColor(Row.CanUse() ? TextColor : OffColor));
		RowLines[Index]->SetText(Row.Line);
	}
	for (int32 Index = 0; Index < ButtonBoxes.Num(); ++Index)
	{
		const int32 RowIndex = Slots + Index;
		const bool bShown = Rows.IsValidIndex(RowIndex);
		SetShown(ButtonBoxes[Index]->GetParent(), bShown);
		if (bShown)
		{
			ButtonBoxes[Index]->SetBrushColor(RowIndex == SelectedRow ? FocusColor : PlateColor);
			ButtonLabels[Index]->SetText(Rows[RowIndex].Label);
		}
	}
	if (!StatusText)
	{
		return; // headless: no tree
	}
	StatusText->SetText(Status);
	SetShown(StatusText, !Status.IsEmpty());

	SetShown(PanelLayer, Panel != ETitlePanel::None);
	SetShown(VersionLines, Panel == ETitlePanel::SaveReset);
	SetShown(HoldLines, false);
	SetShown(AnswerLines, IsYesNoPanel());
	YesBox->SetBrushColor(bPanelYes ? FocusColor : PlateColor);
	NoBox->SetBrushColor(bPanelYes ? PlateColor : FocusColor);
	const FText YesNoHints = LOCTEXT("YesNoHints", "{Arrows}  Yes / No        {Confirm}  Choose        {Back}  No");
	if (Panel == ETitlePanel::SaveReset)
	{
		const USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
		PanelTitle->SetText(LOCTEXT("ResetTitle", "Your save was reset"));
		PanelTitle->SetColorAndOpacity(FSlateColor(NoticeTitleColor));
		PanelBody->SetText(LOCTEXT("ResetBody", "This build can't read your old save, so the game starts fresh. This can happen until the first public release."));
		OldVersionText->SetText(WoRTitleScreen::GetOldVersionText(Saves ? Saves->GetWipedFromVersion() : 0));
		NeededVersionText->SetText(WoRTitleScreen::GetNeededVersionText());
		WoRKeyHints::SetHint(HintsText, LOCTEXT("ResetHints", "{Confirm}  Start fresh"));
	}
	else if (Panel == ETitlePanel::NewGameConfirm)
	{
		PanelTitle->SetText(LOCTEXT("NewGameTitle", "Start a new game?"));
		PanelTitle->SetColorAndOpacity(FSlateColor(TextColor));
		PanelBody->SetText(WoRTitleScreen::GetNewGameWarning(Save));
		WoRKeyHints::SetHint(HintsText, YesNoHints);
	}
	else if (Panel == ETitlePanel::DeleteConfirm)
	{
		PanelTitle->SetText(LOCTEXT("DeleteTitle", "Delete a character"));
		PanelTitle->SetColorAndOpacity(FSlateColor(WarnColor));
		PanelBody->SetText(WoRTitleScreen::GetDeleteWarning(Save));
		WoRKeyHints::SetHint(HintsText, YesNoHints);
	}
	else if (bJoining)
	{
		WoRKeyHints::SetHint(HintsText, LOCTEXT("JoinHints", "{Arrows}  Move        {Confirm}  Join as this character        {Back}  Back"));
	}
	else if (Rows.IsValidIndex(SelectedRow) && Rows[SelectedRow].Id == ETitleScreenRow::Continue)
	{
		WoRKeyHints::SetHint(HintsText, LOCTEXT("SlotHints", "{Arrows}  Move        {Confirm}  Continue        {Second}  Delete"));
	}
	else
	{
		WoRKeyHints::SetHint(HintsText, LOCTEXT("Hints", "{Arrows}  Move        {Confirm}  Select"));
	}
}

void UTitleScreenWidget::ShowPanel(ETitlePanel InPanel)
{
	StopHold();
	Panel = InPanel;
	bPanelYes = false; // a yes / no opens on No
	Refresh();
}

void UTitleScreenWidget::MoveSelection(int32 Delta)
{
	if (Panel != ETitlePanel::None || Rows.IsEmpty())
	{
		return;
	}
	const int32 Slots = GetSlotCount();
	if (SelectedRow < Slots)
	{
		// Down past the last slot: the first button. Up from the first slot stays.
		SelectRow(SelectedRow + Delta >= Slots ? Slots : FMath::Max(SelectedRow + Delta, 0));
	}
	else if (Delta < 0 && Slots > 0)
	{
		SelectRow(Slots - 1); // Up from a button: the last slot
	}
}

void UTitleScreenWidget::MoveSide(int32 Delta)
{
	if (IsYesNoPanel())
	{
		bPanelYes = !bPanelYes;
		Refresh();
		return;
	}
	const int32 Slots = GetSlotCount();
	const int32 Buttons = Rows.Num() - Slots;
	if (Panel == ETitlePanel::None && SelectedRow >= Slots && Buttons > 0)
	{
		SelectRow(Slots + ((SelectedRow - Slots + Delta) % Buttons + Buttons) % Buttons);
	}
}

void UTitleScreenWidget::SelectRow(int32 Index)
{
	if (Rows.IsValidIndex(Index) && Index != SelectedRow)
	{
		SelectedRow = Index;
		Status = FText::GetEmpty();
		Refresh();
	}
}

void UTitleScreenWidget::Confirm()
{
	if (Panel == ETitlePanel::SaveReset)
	{
		ShowPanel(ETitlePanel::None);
		return;
	}
	if (IsYesNoPanel())
	{
		ChooseAnswer(bPanelYes);
		return;
	}
	if (!Rows.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FTitleScreenRow &Row = Rows[SelectedRow];
	if (!Row.CanUse())
	{
		Status = Row.Refusal;
		Refresh();
		return;
	}
	switch (Row.Id)
	{
	case ETitleScreenRow::Continue:
	{
		if (bJoining)
		{
			JoinAs(Row);
			break;
		}
		// B-554 (D-229): the lit slot's character is loaded in place of the one in memory, then the hub opens.
		USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
		if (Saves && !Saves->SwitchToSlot(Row.Slot))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Title] Slot %d not loaded: continuing the character in memory"), Row.Slot);
		}
		Leave();
		break;
	}
	case ETitleScreenRow::NewGame:
		// B-554 (D-229): a new character gets its own slot (Begin finds a free one), so nothing is replaced. The
		// NewGameConfirm panel (one save file, B-546) is no longer opened: OBSOLETE with StartOver.
		if (bJoining)
		{
			APlayerController *PC = GetOwningPlayer();
			DeactivateWidget(); // B-562: the new character's screen sends the join; Back there returns to the game
			UNewGameScreenWidget::OpenToJoin(PC);
			break;
		}
		UNewGameScreenWidget::Open(GetOwningPlayer());
		break;
	case ETitleScreenRow::Settings:
		USettingsScreenWidget::Open(GetOwningPlayer());
		break;
	case ETitleScreenRow::Credits:
		UCreditsScreenWidget::Open(GetOwningPlayer());
		break;
	case ETitleScreenRow::Quit:
		UE_LOG(LogWoRUI, Log, TEXT("[Title] Quit"));
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	default:
		break;
	}
}

void UTitleScreenWidget::JoinAs(const FTitleScreenRow &Row)
{
	FPlayerPicks Picks;
	if (!WoRTitleScreen::ReadSlotPicks(GetGameInstance(), Row.Slot, Picks))
	{
		// A save from before B-554 has no new-game picks to build the character from on the host.
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] Slot %d has no new-game picks: it can't join"), Row.Slot);
		Status = LOCTEXT("NoPicksToJoin", "This character was made before characters could join. Make a new one with the +.");
		Refresh();
		return;
	}
	APlayerController *PC = GetOwningPlayer();
	DeactivateWidget(); // the host picks the seat (D-150); its answer comes back as the seat, not to this screen
	WoRTitleScreen::SendJoin(PC, Picks);
}

void UTitleScreenWidget::RequestDelete()
{
	if (!bJoining && Panel == ETitlePanel::None && Rows.IsValidIndex(SelectedRow) && Rows[SelectedRow].Id == ETitleScreenRow::Continue)
	{
		ShowPanel(ETitlePanel::DeleteConfirm);
	}
}

void UTitleScreenWidget::ChooseAnswer(bool bYes)
{
	const ETitlePanel Asked = Panel;
	if (!bYes || !IsYesNoPanel())
	{
		ShowPanel(ETitlePanel::None);
		return;
	}
	if (Asked == ETitlePanel::NewGameConfirm)
	{
		StartOver();
	}
	else
	{
		DeleteCharacter();
	}
}

void UTitleScreenWidget::Leave()
{
	GTitleLeftThisLaunch = true;
	UE_LOG(LogWoRUI, Log, TEXT("[Title] Left: %s"), Save.bHasSave ? TEXT("continuing the save") : TEXT("a new game"));
	// B-537 (D-232): from the menu level Continue goes to the hub, where the saved leader stands (B-529).
	UTrialRunSubsystem *TrialRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (TrialRun && TrialRun->HasHubLevel() && WoRFrontEnd::IsFrontEndWorld(GetWorld()))
	{
		UGameplayStatics::OpenLevel(this, FName(*TrialRun->GetHubLevelPackage())); // a saved run settles there (USaveSubsystem)
	}
	DeactivateWidget();
}

void UTitleScreenWidget::StartHold()
{
	if (Panel != ETitlePanel::NewGameConfirm || HoldTicker.IsValid())
	{
		return;
	}
	HeldSeconds = 0.0f;
	// A ticker, not the world's timers: the hold counts even while the game under the title is paused.
	HoldTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		UpdateHold(DeltaSeconds);
		return HoldTicker.IsValid();
	}));
}

void UTitleScreenWidget::StopHold()
{
	if (HoldTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HoldTicker);
		HoldTicker.Reset();
	}
	HeldSeconds = 0.0f;
	if (HoldBar)
	{
		HoldBar->SetPercent(0.0f);
	}
}

void UTitleScreenWidget::UpdateHold(float DeltaSeconds)
{
	using namespace TitleScreenStyle;
	if (Panel != ETitlePanel::NewGameConfirm)
	{
		return;
	}
	HeldSeconds += FMath::Min(DeltaSeconds, MaxHoldStepSeconds); // a hitch can't finish the hold by itself
	if (HoldBar)
	{
		HoldBar->SetPercent(FMath::Min(HeldSeconds / HOLD_TO_START_OVER_SECONDS, 1.0f));
	}
	if (HeldSeconds >= HOLD_TO_START_OVER_SECONDS)
	{
		StartOver();
	}
}

// OBSOLETE (B-554, D-229): the + no longer asks to start over (a new character gets its own slot); delete with the
// NewGameConfirm panel and the hold once Crown's PIE pass of B-554 is done.
void UTitleScreenWidget::StartOver()
{
	USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	if (Saves && Saves->IsBoundToPlayer())
	{
		// B-469: the new game screen's Begin resets the old game still in memory and starts the new one.
		Saves->DeleteSave();
		// Shown if the player backs out of the new game screen: the old game can't be continued now.
		GTitleSaveDeletedThisLaunch = true;
		Status = GetSaveDeletedText();
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] New character over the save: the save is deleted; the new game screen opens"));
	}
	SelectedRow = 0;
	ShowPanel(ETitlePanel::None);
	UNewGameScreenWidget::Open(GetOwningPlayer());
}

void UTitleScreenWidget::DeleteCharacter()
{
	USaveSubsystem *Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
	if (Saves && Saves->IsBoundToPlayer() && Save.Slot != INDEX_NONE)
	{
		Saves->DeleteSlot(Save.Slot); // B-554 (D-229): that character's own file
		Status = FText::Format(LOCTEXT("CharacterDeleted", "{0} is deleted."), Save.LeaderName);
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] Slot %d deleted: %s"), Save.Slot, *Save.LeaderName.ToString());
	}
	bSlotsRead = false;
	SelectedRow = 0;
	ShowPanel(ETitlePanel::None);
}

FReply UTitleScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsLeftKey(Key) || IsRightKey(Key))
	{
		MoveSide(IsRightKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			Confirm();
		}
		return FReply::Handled();
	}
	if (IsSecondActionKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			RequestDelete();
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

FReply UTitleScreenWidget::NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (WoRMenuStyle::IsConfirmKey(InKeyEvent.GetKey()) && HoldTicker.IsValid())
	{
		StopHold(); // OBSOLETE (B-546) with the hold
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UTitleScreenWidget::NativeOnFocusLost(const FFocusEvent &InFocusEvent)
{
	StopHold();
	Super::NativeOnFocusLost(InFocusEvent);
}

bool UTitleScreenWidget::NativeOnHandleBackAction()
{
	// The title never closes on Back; Back only takes a panel away (a yes / no's No). B-562: opened to join, it closes.
	if (Panel != ETitlePanel::None)
	{
		ShowPanel(ETitlePanel::None);
	}
	else if (bJoining)
	{
		DeactivateWidget();
	}
	return true;
}

int32 UTitleScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const UBorder *Box = GetRowBox(Index);
		if (Box && Box->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UTitleScreenWidget::FindAnswerUnder(const FPointerEvent &MouseEvent) const
{
	const FVector2D Where = MouseEvent.GetScreenSpacePosition();
	if (YesBox && YesBox->GetCachedGeometry().IsUnderLocation(Where))
	{
		return 1;
	}
	if (NoBox && NoBox->GetCachedGeometry().IsUnderLocation(Where))
	{
		return 0;
	}
	return INDEX_NONE;
}

FReply UTitleScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		if (Panel == ETitlePanel::None)
		{
			SelectRow(FindRowUnder(InMouseEvent));
		}
		else if (IsYesNoPanel())
		{
			const int32 Answer = FindAnswerUnder(InMouseEvent);
			if (Answer != INDEX_NONE && (Answer == 1) != bPanelYes)
			{
				bPanelYes = Answer == 1;
				Refresh();
			}
		}
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTitleScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();
	if (Button == EKeys::RightMouseButton && Panel == ETitlePanel::None)
	{
		// B-546: a right-click on a filled slot asks to delete it (as R / Y; the hint bar's key cap works too).
		const int32 Row = FindRowUnder(InMouseEvent);
		if (Row != INDEX_NONE && Rows[Row].Id == ETitleScreenRow::Continue)
		{
			SelectRow(Row);
			RequestDelete();
			return FReply::Handled();
		}
	}
	if (Button != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (Panel == ETitlePanel::SaveReset)
	{
		Confirm();
		return FReply::Handled();
	}
	if (IsYesNoPanel())
	{
		const int32 Answer = FindAnswerUnder(InMouseEvent);
		if (Answer != INDEX_NONE)
		{
			ChooseAnswer(Answer == 1);
		}
		return FReply::Handled();
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (Row != INDEX_NONE)
	{
		SelectRow(Row);
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UTitleScreenWidget::NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HoldTicker.IsValid())
	{
		StopHold(); // OBSOLETE (B-546) with the hold
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTitleOpenCommand(
	TEXT("wor.UI.Title"),
	TEXT("B-467: open the title screen (outside the editor it opens by itself once a launch)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UTitleScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Title] wor.UI.Title: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
