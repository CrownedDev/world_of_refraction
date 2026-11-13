// CreditsScreenWidget.cpp

#include "UI/Menus/CreditsScreenWidget.h"

#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/CreditsScreenModel.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRCreditsScreenWidget"

namespace CreditsScreenStyle
{
	// This screen's own layout (screen_mockups/front/credits, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	/** The column sits in the screen's left half, its text centred. */
	constexpr float ColumnWidth = 816.0f;
	constexpr int32 GameTitleFontSize = 44;
	constexpr int32 CreditFontSize = 27;
	constexpr float TitleGap = 72.0f;
	constexpr float HeadingGap = 39.0f;
	constexpr float NameGap = 9.0f;
	/** Under the last line: a screen's height, so everything scrolls away before the screen closes. */
	constexpr float TailHeight = 1080.0f;
	constexpr float MaxScrollStepSeconds = 0.1f;
}

UCreditsScreenWidget::UCreditsScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UCreditsScreenWidget *UCreditsScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Credits] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UCreditsScreenWidget>(Root->PushToMenu(UCreditsScreenWidget::StaticClass()));
}

TSharedRef<SWidget> UCreditsScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UCreditsScreenWidget::BuildTree()
{
	using namespace CreditsScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(FrontBackdropColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetVisibility(ESlateVisibility::HitTestInvisible); // it scrolls on its own; the mouse wheel doesn't move it
	UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	auto AddCentred = [Column](UTextBlock *Text, float TopGap)
	{
		Text->SetJustification(ETextJustify::Center);
		AddToColumn(Column, Text, FMargin(0.0f, TopGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Fill);
	};
	UTextBlock *GameTitle = MakeText(WidgetTree, GameTitleFontSize, TextColor, true);
	GameTitle->SetText(WoRCredits::GetGameTitle());
	AddCentred(GameTitle, TitleGap);
	bool bFirstSection = true;
	for (const FCreditsSection &Section : WoRCredits::BuildSections())
	{
		UTextBlock *Heading = MakeText(WidgetTree, LineFontSize, CyanColor, true);
		Heading->SetText(Section.Heading);
		AddCentred(Heading, bFirstSection ? TitleGap : HeadingGap);
		bFirstSection = false;
		for (const FText &Line : Section.Lines)
		{
			UTextBlock *Name = MakeWrappedText(WidgetTree, CreditFontSize, TextColor);
			Name->SetText(Line);
			AddCentred(Name, NameGap);
		}
	}
	USpacer *Tail = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Tail->SetSize(FVector2D(1.0f, TailHeight));
	Column->AddChildToVerticalBox(Tail);
	Scroll->AddChild(Column);
	UVerticalBoxSlot *ScrollSlot = AddToColumn(Main, MakeSized(WidgetTree, ColumnWidth, 0.0f, Scroll));
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScrollSlot->SetHorizontalAlignment(HAlign_Left); // not stretched: the column keeps its width, in the left half

	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "A  Hold: faster        B / Esc  Back"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

void UCreditsScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// The stack may hand back a pooled instance: every open starts at the top.
	ScrollOffset = 0.0f;
	bFaster = false;
	if (Scroll)
	{
		Scroll->SetScrollOffset(0.0f);
	}
	StopScroll();
	// A ticker, not the world's timers: the credits move even while the game under them is paused.
	ScrollTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		UpdateScroll(DeltaSeconds);
		return ScrollTicker.IsValid();
	}));
	UE_LOG(LogWoRUI, Log, TEXT("[Credits] Opened"));
}

void UCreditsScreenWidget::NativeOnDeactivated()
{
	StopScroll();
	Super::NativeOnDeactivated();
}

void UCreditsScreenWidget::StopScroll()
{
	if (ScrollTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ScrollTicker);
		ScrollTicker.Reset();
	}
}

UWidget *UCreditsScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UCreditsScreenWidget *>(this);
}

void UCreditsScreenWidget::UpdateScroll(float DeltaSeconds)
{
	if (!Scroll)
	{
		return; // headless: no tree
	}
	const float End = Scroll->GetScrollOffsetOfEnd();
	if (End <= 0.0f)
	{
		return; // not laid out yet
	}
	ScrollOffset += WoRCredits::GetScrollStep(FMath::Min(DeltaSeconds, CreditsScreenStyle::MaxScrollStepSeconds), bFaster);
	Scroll->SetScrollOffset(FMath::Min(ScrollOffset, End));
	if (ScrollOffset >= End)
	{
		UE_LOG(LogWoRUI, Log, TEXT("[Credits] The end: closing"));
		StopScroll();
		DeactivateWidget();
	}
}

FReply UCreditsScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsConfirmKey(Key))
	{
		// A fresh press only: the Enter that opened the credits, still held, doesn't start them fast.
		if (!InKeyEvent.IsRepeat())
		{
			SetFaster(true);
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

FReply UCreditsScreenWidget::NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (WoRMenuStyle::IsConfirmKey(InKeyEvent.GetKey()))
	{
		SetFaster(false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UCreditsScreenWidget::NativeOnFocusLost(const FFocusEvent &InFocusEvent)
{
	SetFaster(false); // no key-up arrives once the keys go elsewhere
	Super::NativeOnFocusLost(InFocusEvent);
}

FReply UCreditsScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SetFaster(true);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCreditsScreenWidget::NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SetFaster(false);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GCreditsOpenCommand(
	TEXT("wor.UI.Credits"),
	TEXT("B-470: open the credits, as the title's Credits row does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UCreditsScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[Credits] wor.UI.Credits: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
