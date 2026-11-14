// RunEndCardWidget.h
// B-465 (D-187, D-188, D-189; UIScreens.md #26, #27, mockups run/run_end/2_wipe.png, 4_run_over.png): the wipe card and
// the run-end page, one code-built card (placeholder look, D-024) over the dimmed battle. ABattleGameMode shows the wipe
// card instead of the old DEFEAT results; "Get up" goes back to the trial, or, when the wipe ended the run, to the hub.
// B-465c: every run end (boss beaten, extracted, out of wipes, quit) leaves the run-end page for the hub
// (UTrialRunSubsystem::AnnounceRunEnd), which shows it on arrival; its "Return" closes it. What it shows is
// WoRRunEnd's (RunEndModel.h).
// A press (A, Enter, Space, a click) continues, after a short grace so a held key doesn't skip it; nothing continues on
// its own (a wipe is read, FeelGuide 7). Previews: wor.UI.WipeCard, wor.UI.RunEnd <0-3>.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Trial/RunEndModel.h"
#include "RunEndCardWidget.generated.h"

class APlayerController;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRunEndCardContinue);

UCLASS()
class WORLD_OF_REFRACTION_API URunEndCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowWipe(const FWipeCard &Card);
	void ShowRunEnd(const FRunEndPage &Page);

	/** The button: fires once per Show. */
	UPROPERTY(BlueprintAssignable, Category = "Run End")
	FOnRunEndCardContinue OnContinue;

	/** Seconds input is ignored after showing, so a held attack key doesn't skip the card. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run End", meta = (ClampMin = 0))
	float InputGraceSeconds = 0.75f;

	/** The button's press (the specs call it; the keys and a click come here once the grace is over). */
	void PressContinue();

	const FText &GetTitle() const { return Title; }
	const TArray<FText> &GetLines() const { return Lines; }
	const FText &GetButtonText() const { return ButtonText; }
	bool HasContinued() const { return bContinued; }
	FString GetCardString() const;

	/** Closes the card and gives the keys back to the game: the console previews' button, and (B-465c) the hub's
	 *  run-end page's Return (the hub is already there behind it). */
	UFUNCTION()
	void ClosePreview();

	/** Adds a card to PC's viewport, focused, with UI-only input (as the old results screen). Null without PC. */
	static URunEndCardWidget *AddToPlayer(APlayerController *PC);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	void Present(const FLinearColor &TitleColor);
	bool IsPastInputGrace() const;

	FText Title;
	FText Subtitle;
	TArray<FText> Lines;
	FText Footer;
	FText ButtonText;
	double ShownAtSeconds = 0.0;
	bool bContinued = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LineBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineTexts;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FooterText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ButtonLabel;
};
