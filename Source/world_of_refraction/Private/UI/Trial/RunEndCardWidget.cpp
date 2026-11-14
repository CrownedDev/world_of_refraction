// RunEndCardWidget.cpp

#include "UI/Trial/RunEndCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Trial/DefeatRules.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRRunEndCard"

namespace RunEndCardStyle
{
	// This card's own layout (mockups run/run_end, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	inline const FLinearColor OverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
	inline const FLinearColor ClearedColor = FLinearColor::FromSRGBColor(FColor(240, 200, 90));
	inline const FLinearColor EndedColor = FLinearColor::FromSRGBColor(FColor(220, 110, 130));
	constexpr float CardWidth = 1000.0f;
	constexpr float CardPadding = 54.0f;
	constexpr float ButtonPadding = 18.0f;
	/** Above the battle HUD, as the old results screen. */
	constexpr int32 CardZOrder = 100;
	/** The pips the wipes line draws: used, then left. */
	const TCHAR *const PipUsed = TEXT("\u25CF");
	const TCHAR *const PipLeft = TEXT("\u25CB");
}

URunEndCardWidget *URunEndCardWidget::AddToPlayer(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	URunEndCardWidget *Card = CreateWidget<URunEndCardWidget>(PC, URunEndCardWidget::StaticClass());
	if (!Card)
	{
		return nullptr;
	}
	Card->AddToViewport(RunEndCardStyle::CardZOrder);
	// GameOnly input keeps keys in the viewport; route them to the card (the next level's controller resets it).
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Card->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Card;
}

TSharedRef<SWidget> URunEndCardWidget::RebuildWidget()
{
	using namespace RunEndCardStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(OverlayColor);
		Dim->SetHorizontalAlignment(HAlign_Center);
		Dim->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Dim;

		UBorder *Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrushColor(PanelColor);
		Card->SetPadding(FMargin(CardPadding));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Column);
		TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
		TitleText->SetJustification(ETextJustify::Center);
		AddToColumn(Column, TitleText)->SetHorizontalAlignment(HAlign_Center);
		SubtitleText = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		SubtitleText->SetJustification(ETextJustify::Center);
		AddToColumn(Column, SubtitleText, FMargin(0.0f, LineGap, 0.0f, SectionGap))->SetHorizontalAlignment(HAlign_Center);
		LineBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToColumn(Column, LineBox)->SetHorizontalAlignment(HAlign_Center);
		FooterText = MakeWrappedText(WidgetTree, RowFontSize, CyanColor, true);
		FooterText->SetJustification(ETextJustify::Center);
		AddToColumn(Column, FooterText, FMargin(0.0f, SectionGap, 0.0f, SectionGap))->SetHorizontalAlignment(HAlign_Center);
		UBorder *Button = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Button->SetBrushColor(FocusColor);
		Button->SetPadding(FMargin(ButtonPadding));
		ButtonLabel = MakeText(WidgetTree, RowFontSize, TextColor, true);
		Button->SetContent(ButtonLabel);
		AddToColumn(Column, Button)->SetHorizontalAlignment(HAlign_Center);
		Dim->SetContent(MakeSized(WidgetTree, CardWidth, 0.0f, Card));
	}
	return Super::RebuildWidget();
}

void URunEndCardWidget::ShowWipe(const FWipeCard &Card)
{
	using namespace RunEndCardStyle;
	Title = Card.Title;
	Subtitle = Card.LastBlowLine;
	Lines = Card.Lines;
	FString Pips;
	for (int32 Pip = 0; Pip < Card.WipesMax; ++Pip)
	{
		Pips += Pip < Card.WipesUsed ? PipUsed : PipLeft;
	}
	Footer = FText::Format(LOCTEXT("WipesFooter", "{0}   {1}"), Card.WipesLine, FText::FromString(Pips));
	ButtonText = FText::Format(LOCTEXT("ButtonA", "A   {0}"), Card.ButtonText);
	Present(EndedColor);
	UE_LOG(LogWoRUI, Log, TEXT("[RunEndCard] Wipe: %s"), *WoRRunEnd::GetWipeCardString(Card));
}

void URunEndCardWidget::ShowRunEnd(const FRunEndPage &Page)
{
	using namespace RunEndCardStyle;
	Title = Page.Title;
	Subtitle = Page.ShareLine;
	Lines.Reset();
	Footer = Page.PayoutLine; // D-187: one centred line, what the run gave
	ButtonText = FText::Format(LOCTEXT("ButtonA", "A   {0}"), Page.ButtonText);
	Present(Page.bCleared ? ClearedColor : EndedColor);
	UE_LOG(LogWoRUI, Log, TEXT("[RunEndCard] Run end: %s"), *WoRRunEnd::GetRunEndString(Page));
}

void URunEndCardWidget::Present(const FLinearColor &TitleColor)
{
	using namespace RunEndCardStyle;
	ClearFlags(RF_Transactional); // TransBuffer rule, as the old results screen
	bContinued = false;
	if (UWorld *World = GetWorld())
	{
		ShownAtSeconds = World->GetRealTimeSeconds();
	}
	SetIsFocusable(true);
	if (!TitleText)
	{
		return; // headless
	}
	TitleText->SetText(Title);
	TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	SubtitleText->SetText(Subtitle);
	SetShown(SubtitleText, !Subtitle.IsEmpty());
	while (LineTexts.Num() < Lines.Num())
	{
		UTextBlock *Line = MakeWrappedText(WidgetTree, RowFontSize, DimColor);
		Line->SetJustification(ETextJustify::Center);
		AddToColumn(LineBox, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap))->SetHorizontalAlignment(HAlign_Center);
		LineTexts.Add(Line);
	}
	for (int32 Index = 0; Index < LineTexts.Num(); ++Index)
	{
		const bool bShown = Lines.IsValidIndex(Index);
		SetShown(LineTexts[Index], bShown);
		if (bShown)
		{
			LineTexts[Index]->SetText(Lines[Index]);
		}
	}
	FooterText->SetText(Footer);
	SetShown(FooterText, !Footer.IsEmpty());
	ButtonLabel->SetText(ButtonText);
	SetKeyboardFocus();
}

bool URunEndCardWidget::IsPastInputGrace() const
{
	const UWorld *World = GetWorld();
	return !World || World->GetRealTimeSeconds() - ShownAtSeconds >= InputGraceSeconds;
}

void URunEndCardWidget::PressContinue()
{
	if (bContinued)
	{
		return;
	}
	bContinued = true;
	UE_LOG(LogWoRUI, Log, TEXT("[RunEndCard] %s pressed"), *ButtonText.ToString());
	OnContinue.Broadcast();
}

FReply URunEndCardWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (WoRMenuStyle::IsConfirmKey(InKeyEvent.GetKey()))
	{
		if (!InKeyEvent.IsRepeat() && IsPastInputGrace())
		{
			PressContinue();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URunEndCardWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (IsPastInputGrace())
	{
		PressContinue();
	}
	return FReply::Handled();
}

void URunEndCardWidget::ClosePreview()
{
	// A preview has no flow behind it: the button closes it and gives the keys back to the game.
	if (APlayerController *PC = GetOwningPlayer())
	{
		FInputModeGameOnly GameOnly;
		PC->SetInputMode(GameOnly);
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

FString URunEndCardWidget::GetCardString() const
{
	TArray<FString> Shown;
	for (const FText &Line : Lines)
	{
		Shown.Add(Line.ToString());
	}
	return FString::Printf(TEXT("[RunEndCard] %s | %s | %s | %s | [%s]%s"), *Title.ToString(), *Subtitle.ToString(), *FString::Join(Shown, TEXT(" / ")),
						   *Footer.ToString(), *ButtonText.ToString(), bContinued ? TEXT(" (continued)") : TEXT(""));
}

#if !UE_BUILD_SHIPPING
namespace RunEndCardPreview
{
	/** The previews' made-up numbers. */
	constexpr int32 PREVIEW_DROPPED = 120;
	constexpr int32 PREVIEW_WIPES = 2;
	constexpr int32 PREVIEW_GOLD = 1200;
	constexpr int32 PREVIEW_ESSENCE = 30;
}

static FAutoConsoleCommandWithWorldAndArgs GRunEndWipePreview(
	TEXT("wor.UI.WipeCard"),
	TEXT("B-465: preview the wipe card (made-up numbers; any argument names the last blow's foe). A / click closes it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		WoRDefeat::FWipeResult Wipe;
		Wipe.PrismsDropped = RunEndCardPreview::PREVIEW_DROPPED;
		Wipe.Wipes = RunEndCardPreview::PREVIEW_WIPES;
		Wipe.LastBlowBy = FText::FromString(Args.IsEmpty() ? TEXT("Kael") : FString::Join(Args, TEXT(" ")));
		URunEndCardWidget *Card = URunEndCardWidget::AddToPlayer(World ? World->GetFirstPlayerController() : nullptr);
		if (!Card)
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[RunEndCard] wor.UI.WipeCard: no player controller"));
			return;
		}
		Card->ShowWipe(WoRRunEnd::BuildWipeCard(Wipe, /*bHasRestStand=*/true));
		Card->OnContinue.AddUniqueDynamic(Card, &URunEndCardWidget::ClosePreview);
	}));

static FAutoConsoleCommandWithWorldAndArgs GRunEndPagePreview(
	TEXT("wor.UI.RunEnd"),
	TEXT("B-465: preview the run-end page: 0 boss beaten, 1 extracted, 2 out of wipes, 3 quit (made-up payout). A / click closes it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const int32 Index = Args.IsEmpty() ? 0 : FMath::Clamp(FCString::Atoi(*Args[0]), 0, static_cast<int32>(ERunEnding::Quit));
		FRunPayout Payout;
		Payout.HubGold = RunEndCardPreview::PREVIEW_GOLD;
		Payout.GearEssence = RunEndCardPreview::PREVIEW_ESSENCE;
		URunEndCardWidget *Card = URunEndCardWidget::AddToPlayer(World ? World->GetFirstPlayerController() : nullptr);
		if (!Card)
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[RunEndCard] wor.UI.RunEnd: no player controller"));
			return;
		}
		Card->ShowRunEnd(WoRRunEnd::BuildRunEnd(static_cast<ERunEnding>(Index), Payout));
		Card->OnContinue.AddUniqueDynamic(Card, &URunEndCardWidget::ClosePreview);
	}));
#endif

#undef LOCTEXT_NAMESPACE
