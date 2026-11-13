// SettingsScreenWidget.h
// B-452 (D-190, D-181, UIScreens #8, mockups screen_mockups/menus/settings/): the settings screen. A CommonUI
// screen on the root layout's menu layer, opened from the game menu's Settings page (Back returns there) or
// wor.UI.Settings.
//
// Page tabs on top (LB / RB), the page's rows on the left (value on the right of each row), and a side panel
// that explains the focused setting in plain words. Left / Right or Select changes the focused row; a row whose
// setting isn't built stays focusable and says so. The Controls page is a read-only key list.
// A change is live at once and is written to the save file when the screen closes.
// What it shows comes from WoRSettingsScreen (SettingsScreenModel.h), which the specs read.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/SettingsScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "SettingsScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UScrollBox;
class UTextBlock;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API USettingsScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	USettingsScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the game menu, when that is what opened it). */
	static USettingsScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MovePage(int32 Delta);
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index);
	/** Steps the focused row's setting (+1 / -1). */
	void ChangeSelected(int32 Direction);
	void PressBack() { HandleBackAction(); }

	ESettingsPage GetPage() const { return Page; }
	int32 GetSelectedRow() const { return SelectedRow; }
	const TArray<FSettingsRow> &GetRows() const { return Rows; }
	bool HasUnsavedChanges() const { return bChanged; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	void BuildTree();
	/** The page's rows, read fresh from the model, and the focused row's panel. */
	void Refresh();
	int32 FindUnder(const TArray<TObjectPtr<UBorder>> &Boxes, const FPointerEvent &MouseEvent) const;

	ESettingsPage Page = ESettingsPage::Gameplay;
	TArray<FSettingsRow> Rows;
	int32 SelectedRow = 0;
	bool bChanged = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RowBoxes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowValues;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> KeyList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelHeading;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelBody;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelRefusal;
};
