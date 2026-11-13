// CreditsScreenWidget.h
// B-470 (UIScreens #13, mockup screen_mockups/front/credits/1_credits.png): the credits. A CommonUI screen on the root
// layout's menu layer, opened from the title's Credits row (Back returns there) or wor.UI.Credits.
//
// A centred column that scrolls up on its own; holding A (Enter, Space, the left button) makes it go faster; B leaves.
// When the last line has scrolled away the screen closes. What it says is WoRCredits (CreditsScreenModel.h).
// Not built: the title backdrop behind it (B-467b).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UI/WoRActivatableWidget.h"
#include "CreditsScreenWidget.generated.h"

class APlayerController;
class UScrollBox;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UCreditsScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UCreditsScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the title, when that is what opened it). */
	static UCreditsScreenWidget *Open(APlayerController *PC);

	/** The scroll moves on (the ticker calls it); past the end the screen closes. */
	void UpdateScroll(float DeltaSeconds);
	void SetFaster(bool bInFaster) { bFaster = bInFaster; }
	bool IsFaster() const { return bFaster; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent &InFocusEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	void BuildTree();
	void StopScroll();

	float ScrollOffset = 0.0f;
	bool bFaster = false;
	FTSTicker::FDelegateHandle ScrollTicker;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;
};
