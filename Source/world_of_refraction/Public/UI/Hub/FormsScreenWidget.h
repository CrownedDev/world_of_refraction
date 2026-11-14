// FormsScreenWidget.h
// B-444 (D-184, D-181; UIScreens #19, mockup screen_mockups/hub/skins/1_forms.png): the "Spirit of {character}"
// screen. A CommonUI screen on the root layout's menu layer, opened by wor.UI.Forms (a rest menu row comes with the
// rest menu). Left: your own form, then each character form with its meter ("???" for one not met). Right: the lit
// form, its percent, its line, their pieces (owned / worn named, the rest "?"). A wears the lit form, Y switches the
// base body, B goes back. What it shows comes from WoRFormsScreen (FormsScreenModel.h), which the specs read.
// Replaces the placeholder UCharacterFormWidget (now wor.UI.FormsOld, removed after Crown's PIE pass).

#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/FormsScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "FormsScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UFormsScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UFormsScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout. */
	static UFormsScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index);
	/** A: wears the lit form. */
	void PressWear();
	/** Y: the other base body. */
	void PressBaseBody();

	const FFormsScreenPage &GetPage() const { return Page; }
	int32 GetSelectedRow() const { return SelectedRow; }
	/** The last refusal; empty after a change goes through. */
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One form row's widgets, made once and refilled (never rebuilt). */
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Tag;
		TWeakObjectPtr<UTextBlock> Line;
		TWeakObjectPtr<UWidget> Meter;
		TWeakObjectPtr<UProgressBar> Bar;
		TWeakObjectPtr<UTextBlock> Percent;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	/** The page, read fresh from the model. */
	void RefreshData();
	/** The lit row's colour and the side panel. */
	void RefreshSelection();
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	FFormsScreenPage Page;
	int32 SelectedRow = 0;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WearingText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelPercent;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelStatus;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PiecesHeading;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PiecesText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	TArray<FRowWidgets> RowWidgets;
};
