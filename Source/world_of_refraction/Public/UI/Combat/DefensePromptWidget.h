// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefensePromptWidget.generated.h"

class AActor;
class UDefenseSystem;
class UTextBlock;
struct FDefenseResult;

/**
 * UDefensePromptWidget
 *
 * Shows the defense prompt when the local player's character must block / parry /
 * dodge. Created by UCombatHUD as a standalone viewport widget.
 *
 * Native-first: with no WBP skin, the widget builds its own centred PromptText in
 * RebuildWidget. A WBP subclass may supply a "PromptText" TextBlock (BindWidgetOptional)
 * and override the cosmetic Show/Hide events — the C++ implementations do the job
 * when nothing overrides them.
 *
 * Flow: OnDefenseCueTriggered -> "Get ready" · OnDefenseWindowOpened -> "NOW" ·
 * OnDefenseWindowClosed -> outcome text, auto-hidden after ResultDisplaySeconds.
 * Filtered to defenders controlled by a local player.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UDefensePromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to UDefenseSystem delegates. */
	UFUNCTION(BlueprintCallable, Category = "Defense Prompt")
	void InitialiseForCombat();

	/** Unbind delegates. */
	UFUNCTION(BlueprintCallable, Category = "Defense Prompt")
	void TeardownPrompt();

	/** Debug: bound / visible / last text. */
	FString GetPromptStateString() const;

	/** Seconds the outcome text stays up after a window closes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense Prompt", meta = (ClampMin = 0))
	float ResultDisplaySeconds = 0.8f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

	UFUNCTION() void HandleDefenseCueTriggered(AActor* Defender, float TimeUntilWindow);
	UFUNCTION() void HandleDefenseWindowOpened(AActor* Defender, float AttackSize, float WindowDuration);
	UFUNCTION() void HandleDefenseWindowClosed(AActor* Defender, const FDefenseResult& Result);

	/** Cosmetic hook. Native default: show "Get ready" text. */
	UFUNCTION(BlueprintNativeEvent, Category = "Defense Prompt")
	void ShowCue(AActor* Defender, float TimeUntilWindow);
	virtual void ShowCue_Implementation(AActor* Defender, float TimeUntilWindow);

	/** Cosmetic hook. Native default: show the input prompt text. */
	UFUNCTION(BlueprintNativeEvent, Category = "Defense Prompt")
	void ShowPrompt(AActor* Defender, float WindowDuration, float AttackSize);
	virtual void ShowPrompt_Implementation(AActor* Defender, float WindowDuration, float AttackSize);

	/** Cosmetic hook. Native default: show the outcome, then auto-hide. */
	UFUNCTION(BlueprintNativeEvent, Category = "Defense Prompt")
	void HidePrompt(const FDefenseResult& Result);
	virtual void HidePrompt_Implementation(const FDefenseResult& Result);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

private:
	UPROPERTY()
	TWeakObjectPtr<UDefenseSystem> CachedDefenseSystem;

	FTimerHandle HideTimerHandle;

	bool bBound = false;

	bool IsLocallyControlledDefender(const AActor* Defender) const;
	void SetPromptText(const FString& Text);
	void HideNow();
	void ClearHideTimer();
};
