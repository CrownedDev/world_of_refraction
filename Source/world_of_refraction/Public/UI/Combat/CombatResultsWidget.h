// CombatResultsWidget.h
// End-of-combat results screen. Shown by ABattleGameMode on OnCombatResultReady,
// BEFORE the level swap back to the trial (previously the swap happened one tick
// after the result, leaving no results beat).
//
// Native-first: with no WBP skin it builds its own full-screen dimmed overlay
// (title / details / prompt) in RebuildWidget. A WBP subclass may supply
// TitleText / DetailText / PromptText via BindWidgetOptional instead.
//
// Continue fires once: on any key / mouse press, or after AutoContinueSeconds.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "Combat/CombatOrchestrator.h"
#include "CombatResultsWidget.generated.h"

class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatResultsContinue);

UCLASS()
class WORLD_OF_REFRACTION_API UCombatResultsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Fill the texts from Result and start the auto-continue timer. */
    UFUNCTION(BlueprintCallable, Category = "Combat Results")
    void ShowResult(const FCombatResult &Result);

    /** Fires exactly once per ShowResult. */
    UPROPERTY(BlueprintAssignable, Category = "Combat Results")
    FOnCombatResultsContinue OnContinue;

    /** Seconds before continuing without input. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Results", meta = (ClampMin = 0.5))
    float AutoContinueSeconds = 6.0f;

    /** Seconds input is ignored after showing, so a held attack key doesn't skip it. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Results", meta = (ClampMin = 0))
    float InputGraceSeconds = 0.75f;

    /** Debug: shown state + last result line. */
    FString GetResultsStateString() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

    /** Cosmetic hook for a WBP skin (animations, sounds). Fired after texts are set. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Results")
    void OnResultShown(const FCombatResult &Result);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DetailText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PromptText;

private:
    FTimerHandle AutoContinueTimerHandle;
    double ShownAtSeconds = 0.0;
    bool bShown = false;
    bool bContinued = false;
    FString LastResultLine;

    bool IsPastInputGrace() const;
    void Continue();
    void ClearAutoContinueTimer();
};
