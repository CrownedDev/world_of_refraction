// EnemyKitCardWidget.cpp

#include "UI/Trial/EnemyKitCardWidget.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoREnemyKitCard"

namespace EnemyKitCardStyle
{
	// This screen's own layout; the shared look is WoRMenuStyle. TODO: Crown's look pass (built unseen).
	using namespace WoRMenuStyle;
	constexpr float CardWidth = 760.0f;
	constexpr float CardPadding = 54.0f;
}

UEnemyKitCardWidget::UEnemyKitCardWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UEnemyKitCardWidget *UEnemyKitCardWidget::Open(APlayerController *PC, const FEnemyKitCard &InCard)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[KitCard] Not opened: no AWoRHubController with a root layout"));
		return nullptr;
	}
	UEnemyKitCardWidget *Screen = Cast<UEnemyKitCardWidget>(Root->PushToMenu(UEnemyKitCardWidget::StaticClass()));
	if (Screen)
	{
		Screen->SetCard(InCard);
		UE_LOG(LogWoRUI, Log, TEXT("[KitCard] Opened: %s"), *WoRTrialPoolLook::GetKitCardString(InCard));
	}
	return Screen;
}

void UEnemyKitCardWidget::SetCard(const FEnemyKitCard &InCard)
{
	Card = InCard;
	Refresh();
}

TSharedRef<SWidget> UEnemyKitCardWidget::RebuildWidget()
{
	using namespace EnemyKitCardStyle;
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
		NameText = MakeText(WidgetTree, NameFontSize, TextColor, true);
		AddToColumn(Column, NameText)->SetHorizontalAlignment(HAlign_Center);
		ClassText = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToColumn(Column, ClassText, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);

		UTextBlock *CarriesLabel = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		CarriesLabel->SetText(LOCTEXT("Carries", "Carries"));
		AddToColumn(Column, CarriesLabel, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		PiecesText = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		AddToColumn(Column, PiecesText, FMargin(0.0f, LineGap, 0.0f, 0.0f));

		UTextBlock *SkillsLabel = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		SkillsLabel->SetText(LOCTEXT("Skills", "Skills"));
		AddToColumn(Column, SkillsLabel, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		SkillsText = MakeWrappedText(WidgetTree, RowFontSize, CyanColor);
		AddToColumn(Column, SkillsText, FMargin(0.0f, LineGap, 0.0f, 0.0f));

		UVerticalBoxSlot *CardSlot = AddToColumn(Main, MakeSized(WidgetTree, CardWidth, 0.0f, Panel));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Hints = MakeHintBar(WidgetTree);
		WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{Back}  Close"));
		AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
	}
	TSharedRef<SWidget> Built = Super::RebuildWidget();
	Refresh();
	return Built;
}

UWidget *UEnemyKitCardWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UEnemyKitCardWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UEnemyKitCardWidget::Refresh()
{
	if (!NameText)
	{
		return; // headless
	}
	NameText->SetText(Card.Name);
	ClassText->SetText(Card.ClassLine);
	const FText LineBreak = FText::FromString(TEXT("\n"));
	PiecesText->SetText(Card.Pieces.Num() > 0 ? FText::Join(LineBreak, Card.Pieces) : LOCTEXT("NoPieces", "Nothing"));
	SkillsText->SetText(Card.Skills.Num() > 0 ? FText::Join(LineBreak, Card.Skills) : LOCTEXT("NoSkills", "None"));
}

FReply UEnemyKitCardWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (WoRMenuStyle::IsBackKey(InKeyEvent.GetKey()))
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
static FAutoConsoleCommandWithWorld GPreviewKitCardCommand(
	TEXT("wor.UI.PreviewKitCard"),
	TEXT("B-565: open the enemy kit card with made-up contents (TrialPoolDesign example 2). B closes."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UEnemyKitCardWidget::Open(World ? World->GetFirstPlayerController() : nullptr, WoRTrialPoolLook::MakeKitCardExample()))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[KitCard] wor.UI.PreviewKitCard: not opened (use a level with a hub controller)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
