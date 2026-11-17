// DuplicateDropWidget.cpp

#include "UI/Trial/DuplicateDropWidget.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRDuplicateDrop"

namespace DuplicateDropStyle
{
	// This screen's own layout; the shared look is WoRMenuStyle. TODO: Crown's look pass (built unseen).
	using namespace WoRMenuStyle;
	constexpr float CardWidth = 760.0f;
	constexpr float CardPadding = 54.0f;
	constexpr float ChoicePadding = 18.0f;
	constexpr int32 ChoiceCount = UE_ARRAY_COUNT(WoRTrialPoolLook::DUPLICATE_CHOICES);
}

UDuplicateDropWidget::UDuplicateDropWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UDuplicateDropWidget *UDuplicateDropWidget::Open(APlayerController *PC, const FDuplicateDropView &InView, TFunction<void(EDuplicateChoice)> InOnChosen)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[DuplicateDrop] Not opened: no AWoRHubController with a root layout"));
		return nullptr;
	}
	UDuplicateDropWidget *Screen = Cast<UDuplicateDropWidget>(Root->PushToMenu(UDuplicateDropWidget::StaticClass()));
	if (Screen)
	{
		Screen->SetView(InView, MoveTemp(InOnChosen));
		UE_LOG(LogWoRUI, Log, TEXT("[DuplicateDrop] Opened: %s"), *WoRTrialPoolLook::GetDuplicateQuestion(InView).ToString());
	}
	return Screen;
}

void UDuplicateDropWidget::SetView(const FDuplicateDropView &InView, TFunction<void(EDuplicateChoice)> InOnChosen)
{
	View = InView;
	OnChosen = MoveTemp(InOnChosen);
	SelectedChoice = 0;
	Refresh();
}

TSharedRef<SWidget> UDuplicateDropWidget::RebuildWidget()
{
	using namespace DuplicateDropStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(ScreenColor);
		Screen->SetPadding(FMargin(ScreenPadding));
		WidgetTree->RootWidget = Screen;
		UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Screen->SetContent(Main);

		UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(CardPadding));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		UTextBlock *Title = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		Title->SetText(LOCTEXT("Title", "You already have this"));
		AddToColumn(Column, Title)->SetHorizontalAlignment(HAlign_Center);
		QuestionText = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		AddToColumn(Column, QuestionText, FMargin(0.0f, LineGap, 0.0f, SectionGap));
		for (int32 Index = 0; Index < ChoiceCount; ++Index)
		{
			UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Row->SetBrushColor(PlateColor);
			Row->SetPadding(FMargin(ChoicePadding));
			UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
			Row->SetContent(Label);
			ChoiceRows.Add(Row);
			ChoiceLabels.Add(Label);
			AddToColumn(Column, Row, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		}

		UVerticalBoxSlot *CardSlot = AddToColumn(Main, MakeSized(WidgetTree, CardWidth, 0.0f, Panel));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Hints = MakeHintBar(WidgetTree);
		WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "Up / Down  Choose        {Confirm}  Pick        {Back}  Keep both"));
		AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
	}
	TSharedRef<SWidget> Built = Super::RebuildWidget();
	Refresh();
	return Built;
}

UWidget *UDuplicateDropWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UDuplicateDropWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UDuplicateDropWidget::Refresh()
{
	using namespace DuplicateDropStyle;
	if (!QuestionText)
	{
		return; // headless
	}
	QuestionText->SetText(WoRTrialPoolLook::GetDuplicateQuestion(View));
	for (int32 Index = 0; Index < ChoiceRows.Num(); ++Index)
	{
		ChoiceLabels[Index]->SetText(WoRTrialPoolLook::GetDuplicateChoiceText(WoRTrialPoolLook::DUPLICATE_CHOICES[Index], View));
		ChoiceRows[Index]->SetBrushColor(Index == SelectedChoice ? FocusColor : PlateColor);
	}
}

void UDuplicateDropWidget::MoveSelection(int32 Delta)
{
	constexpr int32 Count = DuplicateDropStyle::ChoiceCount;
	SelectedChoice = ((SelectedChoice + Delta) % Count + Count) % Count;
	Refresh();
}

void UDuplicateDropWidget::Choose(EDuplicateChoice Choice)
{
	UE_LOG(LogWoRUI, Log, TEXT("[DuplicateDrop] Chose: %s"), *WoRTrialPoolLook::GetDuplicateChoiceText(Choice, View).ToString());
	TFunction<void(EDuplicateChoice)> Chosen = MoveTemp(OnChosen); // once
	OnChosen = nullptr;
	if (Chosen)
	{
		Chosen(Choice);
	}
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

bool UDuplicateDropWidget::NativeOnHandleBackAction()
{
	Choose(WoRTrialPoolLook::DUPLICATE_BACK_CHOICE);
	return true;
}

void UDuplicateDropWidget::NativeOnDeactivated()
{
	if (OnChosen)
	{
		Choose(WoRTrialPoolLook::DUPLICATE_BACK_CHOICE);
	}
	Super::NativeOnDeactivated();
}

FReply UDuplicateDropWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			Choose(WoRTrialPoolLook::DUPLICATE_CHOICES[SelectedChoice]);
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

int32 UDuplicateDropWidget::FindChoiceUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < ChoiceRows.Num(); ++Index)
	{
		if (ChoiceRows[Index] && ChoiceRows[Index]->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UDuplicateDropWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a choice, so a resting cursor doesn't fight the keys.
	const int32 Choice = InMouseEvent.GetCursorDelta().IsZero() ? INDEX_NONE : FindChoiceUnder(InMouseEvent);
	if (Choice != INDEX_NONE && Choice != SelectedChoice)
	{
		MoveSelection(Choice - SelectedChoice);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UDuplicateDropWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const int32 Choice = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindChoiceUnder(InMouseEvent) : INDEX_NONE;
	if (Choice == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	SelectedChoice = Choice;
	Choose(WoRTrialPoolLook::DUPLICATE_CHOICES[Choice]);
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPreviewDuplicateCommand(
	TEXT("wor.UI.PreviewDuplicate"),
	TEXT("B-565: open the duplicate prompt with made-up contents (a second Arming Sword D while holding one at F). The choice is only logged."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UDuplicateDropWidget::Open(World ? World->GetFirstPlayerController() : nullptr, WoRTrialPoolLook::MakeDuplicateExample()))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[DuplicateDrop] wor.UI.PreviewDuplicate: not opened (use a level with a hub controller)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
