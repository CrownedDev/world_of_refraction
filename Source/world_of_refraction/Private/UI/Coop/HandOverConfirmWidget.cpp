// HandOverConfirmWidget.cpp

#include "UI/Coop/HandOverConfirmWidget.h"

#include "Components/Border.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRHandOverConfirm"

namespace HandOverConfirmStyle
{
	// This screen's own layout (mockup 4_planned_handover, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float CardWidth = 900.0f;
	constexpr float CardPadding = 54.0f;
	constexpr float LabelWidth = 300.0f;
}

UHandOverConfirmWidget::UHandOverConfirmWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UHandOverConfirmWidget *UHandOverConfirmWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[HandOverConfirm] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UHandOverConfirmWidget>(Root->PushToMenu(UHandOverConfirmWidget::StaticClass()));
}

TSharedRef<SWidget> UHandOverConfirmWidget::RebuildWidget()
{
	using namespace HandOverConfirmStyle;
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
		UTextBlock *Title = MakeText(WidgetTree, NameFontSize, TextColor, true);
		Title->SetText(LOCTEXT("Title", "Hand over host?"));
		AddToColumn(Column, Title)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock *Body = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		Body->SetText(LOCTEXT("Body", "The next player hosts and you rejoin as a guest. The trial reopens for everyone; the run, gear and progress are kept."));
		AddToColumn(Column, Body, FMargin(0.0f, SectionGap, 0.0f, SectionGap));

		const auto AddLine = [this, Column](const FText &Label, UTextBlock *Value)
		{
			UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock *LabelText = MakeText(WidgetTree, LineFontSize, DimColor);
			LabelText->SetText(Label);
			AddToRow(Line, MakeSized(WidgetTree, LabelWidth, 0.0f, LabelText));
			AddToRow(Line, Value, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Value->SetJustification(ETextJustify::Right);
			AddToColumn(Column, Line, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		};
		NextHostText = MakeText(WidgetTree, RowFontSize, CyanColor, true);
		AddLine(LOCTEXT("NextHost", "Next host"), NextHostText);
		UTextBlock *Takes = MakeText(WidgetTree, LineFontSize, TextColor, true);
		Takes->SetText(LOCTEXT("TakesValue", "a short reload for everyone"));
		AddLine(LOCTEXT("Takes", "Takes about"), Takes);
		UTextBlock *Note = MakeText(WidgetTree, SmallFontSize, OffColor);
		Note->SetText(LOCTEXT("Note", "Only at a rest spot, outside a fight."));
		AddToColumn(Column, Note, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
		AddToColumn(Column, MessageText, FMargin(0.0f, LineGap, 0.0f, 0.0f));

		UVerticalBoxSlot *CardSlot = AddToColumn(Main, MakeSized(WidgetTree, CardWidth, 0.0f, Panel));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
		Hints->SetText(LOCTEXT("Hints", "A  Hand over        B / Esc  Stay host"));
		AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
	}
	return Super::RebuildWidget();
}

void UHandOverConfirmWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Message = FText::GetEmpty();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[HandOverConfirm] Opened (next host %s, %s)"), *Confirm.NextHost.ToString(),
		   Confirm.bCanHandOver ? TEXT("can hand over") : *Confirm.Refusal.ToString());
}

UWidget *UHandOverConfirmWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UHandOverConfirmWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UHandOverConfirmWidget::Refresh()
{
	Confirm = WoRHostHandOverScreens::BuildConfirm(GetWorld());
	if (!NextHostText)
	{
		return; // headless
	}
	NextHostText->SetText(Confirm.NextHost);
	const FText Shown = Message.IsEmpty() ? Confirm.Refusal : Message;
	MessageText->SetText(Shown);
	WoRMenuStyle::SetShown(MessageText, !Shown.IsEmpty());
}

void UHandOverConfirmWidget::PressHandOver()
{
	Confirm = WoRHostHandOverScreens::BuildConfirm(GetWorld());
	const AWoRHubController *Hub = Cast<AWoRHubController>(GetOwningPlayer());
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Confirm.bCanHandOver || !Root)
	{
		Message = Confirm.Refusal;
		Refresh();
		return;
	}
	// The host-lost card takes the input, so the hand-over starts only once the menus have let it go back.
	const TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
	Root->RunWhenAllClosed([WeakWorld]()
	{
		const FText Refusal = WoRHostHandOverScreens::HandOver(WeakWorld.Get());
		if (!Refusal.IsEmpty())
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[HandOverConfirm] %s"), *Refusal.ToString());
		}
	});
	if (UCommonActivatableWidgetStack *Menus = Root->GetLayerStack(EWoRUILayer::Menu))
	{
		Menus->ClearWidgets();
	}
}

FReply UHandOverConfirmWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressHandOver();
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
static FAutoConsoleCommandWithWorld GHandOverConfirmOpenCommand(
	TEXT("wor.UI.HandOver"),
	TEXT("B-459a: open the Hand over host? confirm (A hand over, B stay host)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UHandOverConfirmWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[HandOverConfirm] wor.UI.HandOver: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
