// HandOverConfirmWidget.h
// B-459a (UIScreens #33, mockup screen_mockups/coop/session/4_planned_handover.png): "Hand over host?", the host's
// confirm before a planned hand-over at a rest spot. A CommonUI screen on the root layout's menu layer, opened by the
// rest menu's hand-over row (or wor.UI.HandOver). It names the next host; A hands over, B stays host. A refusal (not
// the host, nobody else here, in a fight) shows in amber. What it shows comes from WoRHostHandOverScreens::BuildConfirm.

#pragma once

#include "CoreMinimal.h"
#include "UI/Coop/HostHandOverModel.h"
#include "UI/WoRActivatableWidget.h"
#include "HandOverConfirmWidget.generated.h"

class APlayerController;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UHandOverConfirmWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UHandOverConfirmWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout; null without one. */
	static UHandOverConfirmWidget *Open(APlayerController *PC);

	/** A: hand over now (the screen closes); a refusal stays on screen. */
	void PressHandOver();

	const FHandOverConfirm &GetConfirm() const { return Confirm; }
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;

private:
	void Refresh();

	FHandOverConfirm Confirm;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NextHostText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
};
