// CombatResultsWidget.cpp

#include "UI/Combat/CombatResultsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace CombatResultsStyle
{
    // TODO: style pass.
    const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.6f);
    const FLinearColor VictoryColor(1.0f, 0.85f, 0.3f);
    const FLinearColor DefeatColor(0.85f, 0.25f, 0.25f);
    const FLinearColor NeutralColor(0.9f, 0.9f, 0.9f);
    constexpr int32 TitleFontSize = 64;
    constexpr int32 BodyFontSize = 24;
    constexpr const TCHAR *Prompt = TEXT("Press any key to continue");
}

namespace CombatResultsBuild
{
    UTextBlock *AddCentredText(UWidgetTree *Tree, UVerticalBox *Box, const FName Name, int32 FontSize)
    {
        UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
        {
            BoxSlot->SetHorizontalAlignment(HAlign_Center);
        }
        return Text;
    }

    FText TitleFor(ECombatState State)
    {
        switch (State)
        {
        case ECombatState::Victory:
            return NSLOCTEXT("CombatResults", "Victory", "VICTORY");
        case ECombatState::Defeat:
            return NSLOCTEXT("CombatResults", "Defeat", "DEFEAT");
        case ECombatState::Draw:
            return NSLOCTEXT("CombatResults", "Draw", "DRAW");
        case ECombatState::Fled:
            return NSLOCTEXT("CombatResults", "Fled", "FLED");
        default:
            return NSLOCTEXT("CombatResults", "CombatOver", "COMBAT OVER");
        }
    }

    FLinearColor TitleColorFor(ECombatState State)
    {
        switch (State)
        {
        case ECombatState::Victory:
            return CombatResultsStyle::VictoryColor;
        case ECombatState::Defeat:
            return CombatResultsStyle::DefeatColor;
        default:
            return CombatResultsStyle::NeutralColor;
        }
    }
}

TSharedRef<SWidget> UCombatResultsWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
        Overlay->SetBrushColor(CombatResultsStyle::OverlayColor);
        Overlay->SetHorizontalAlignment(HAlign_Center);
        Overlay->SetVerticalAlignment(VAlign_Center);
        WidgetTree->RootWidget = Overlay;

        UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
        Overlay->SetContent(Box);

        TitleText = CombatResultsBuild::AddCentredText(WidgetTree, Box, TEXT("TitleText"), CombatResultsStyle::TitleFontSize);
        DetailText = CombatResultsBuild::AddCentredText(WidgetTree, Box, TEXT("DetailText"), CombatResultsStyle::BodyFontSize);
        DroppedText = CombatResultsBuild::AddCentredText(WidgetTree, Box, TEXT("DroppedText"), CombatResultsStyle::BodyFontSize);
        DroppedText->SetColorAndOpacity(FSlateColor(CombatResultsStyle::VictoryColor));
        DroppedText->SetVisibility(ESlateVisibility::Collapsed);
        PromptText = CombatResultsBuild::AddCentredText(WidgetTree, Box, TEXT("PromptText"), CombatResultsStyle::BodyFontSize);
    }
    return Super::RebuildWidget();
}

void UCombatResultsWidget::ShowResult(const FCombatResult &Result)
{
    ClearFlags(RF_Transactional);
    bShown = true;
    bContinued = false;

    if (TitleText)
    {
        TitleText->SetText(CombatResultsBuild::TitleFor(Result.FinalState));
        TitleText->SetColorAndOpacity(FSlateColor(CombatResultsBuild::TitleColorFor(Result.FinalState)));
    }

    LastResultLine = FString::Printf(TEXT("%d turns  |  survivors %d vs %d"),
                                     Result.TotalTurns, Result.LocalPartySurvivors, Result.OpposingPartySurvivors);
    if (DetailText)
    {
        DetailText->SetText(FText::FromString(LastResultLine));
    }
    if (PromptText)
    {
        PromptText->SetText(FText::FromString(CombatResultsStyle::Prompt));
    }

    SetIsFocusable(true);
    SetKeyboardFocus();

    if (UWorld *World = GetWorld())
    {
        ShownAtSeconds = World->GetRealTimeSeconds();
        // A zero rate would clear the timer instead of arming it — never strand the player.
        constexpr float MinAutoContinueSeconds = 0.5f;
        World->GetTimerManager().SetTimer(AutoContinueTimerHandle, this, &UCombatResultsWidget::Continue,
                                          FMath::Max(AutoContinueSeconds, MinAutoContinueSeconds), /*bLoop=*/false);
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatResults] %s - %s"), *CombatResultsBuild::TitleFor(Result.FinalState).ToString(), *LastResultLine);
    OnResultShown(Result);
}

void UCombatResultsWidget::SetDroppedLines(const TArray<FText> &Lines)
{
    TArray<FString> Strings;
    for (const FText &Line : Lines)
    {
        Strings.Add(Line.ToString());
    }
    LastDroppedLines = FString::Join(Strings, TEXT(" / "));
    if (DroppedText)
    {
        DroppedText->SetText(FText::Join(FText::FromString(LINE_TERMINATOR), Lines));
        DroppedText->SetVisibility(Lines.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    UE_LOG(LogTemp, Log, TEXT("[CombatResults] Dropped: %s"), Lines.Num() > 0 ? *LastDroppedLines : TEXT("nothing"));
}

FString UCombatResultsWidget::GetResultsStateString() const
{
    return FString::Printf(TEXT("[CombatResults] shown=%s continued=%s last=\"%s\" dropped=\"%s\""),
                           bShown ? TEXT("yes") : TEXT("no"),
                           bContinued ? TEXT("yes") : TEXT("no"),
                           *LastResultLine, *LastDroppedLines);
}

void UCombatResultsWidget::NativeDestruct()
{
    ClearAutoContinueTimer();
    Super::NativeDestruct();
}

FReply UCombatResultsWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
    if (bShown && IsPastInputGrace())
    {
        Continue();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UCombatResultsWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
    if (bShown && IsPastInputGrace())
    {
        Continue();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

bool UCombatResultsWidget::IsPastInputGrace() const
{
    const UWorld *World = GetWorld();
    return !World || (World->GetRealTimeSeconds() - ShownAtSeconds) >= InputGraceSeconds;
}

void UCombatResultsWidget::Continue()
{
    if (bContinued)
    {
        return;
    }
    bContinued = true;
    ClearAutoContinueTimer();
    OnContinue.Broadcast();
}

void UCombatResultsWidget::ClearAutoContinueTimer()
{
    if (!AutoContinueTimerHandle.IsValid())
    {
        return;
    }
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AutoContinueTimerHandle);
    }
}

#if !UE_BUILD_SHIPPING
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Trial/TrialPoolLook.h"

static FAutoConsoleCommandWithWorld GPreviewFightRewardCommand(
    TEXT("wor.UI.PreviewFightReward"),
    TEXT("B-565: show the fight's results with a made-up victory and the made-up drop line (TrialPoolDesign example 2). Closes on its own."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
        UCombatResultsWidget *Widget = PC ? CreateWidget<UCombatResultsWidget>(PC, UCombatResultsWidget::StaticClass()) : nullptr;
        if (!Widget)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatResults] wor.UI.PreviewFightReward: no player controller"));
            return;
        }
        Widget->AddToViewport();
        FCombatResult Result;
        Result.FinalState = ECombatState::Victory;
        Result.TotalTurns = 7; // made up
        Result.LocalPartySurvivors = 3;
        Widget->ShowResult(Result);
        Widget->SetDroppedLines({WoRTrialPoolLook::GetDroppedLine(WoRTrialPoolLook::MakeDropExample())});
        // The preview has no game mode listening to OnContinue: it removes itself when the real screen would continue.
        FTimerHandle RemoveHandle;
        World->GetTimerManager().SetTimer(RemoveHandle, FTimerDelegate::CreateWeakLambda(Widget, [Widget]() { Widget->RemoveFromParent(); }),
                                          Widget->AutoContinueSeconds, /*bLoop=*/false);
    }));
#endif
