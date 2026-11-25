// SettingsScreenWidget.h
// B-452 (D-190, D-181, UIScreens #8, mockups screen_mockups/menus/settings/): the settings screen. A CommonUI
// screen on the root layout's menu layer, opened from the game menu's Settings page (Back returns there) or
// wor.UI.Settings.
//
// Page tabs on top (LB / RB). A change is live at once and is written to the save file when the screen closes.
// What it shows comes from WoRSettingsScreen (SettingsScreenModel.h), which the specs read.
// B-631 (mockup menus/settings/v2, Q-631): the loadout screen's three columns (v5). Left: the page's rows (‹ value › on
// the lit one, an On / Off row as a switch). Middle: the lit row's name, its value and what it does. Right: Details,
// every choice it has with what each does and the default (each clickable), and why it can't change. Left / Right
// change the lit row, A takes the next choice, Y the default. Controls: a ‹ Battle  Field  Menus › plate (Left / Right
// or a click) and that kind's fixed keys; the middle shows the lit key's caps. A click on a row lights it; a click on
// the lit row changes it.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/SettingsScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "SettingsScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
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
	/** B-631: Controls' kind plate (Battle, Field, Menus), wrapping. */
	void MoveKind(int32 Delta);
	/** B-631: puts the focused row on one of its choices (a Details card). */
	void PickChoice(int32 Choice);
	/** B-631: Y, the focused row's default choice. */
	void PressDefault();
	void PressBack() { HandleBackAction(); }

	ESettingsPage GetPage() const { return Page; }
	int32 GetKind() const { return Kind; }
	int32 GetSelectedRow() const { return SelectedRow; }
	const TArray<FSettingsRow> &GetRows() const { return Rows; }
	bool HasUnsavedChanges() const { return bChanged; }
	/** The bottom line: what each key does here. */
	FText GetHints() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> Value;
		/** The On / Off switch: a track with a knob at one end. */
		TWeakObjectPtr<UBorder> SwitchTrack;
	};
	struct FChoiceWidgets
	{
		TWeakObjectPtr<UBorder> Card;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> DefaultTag;
		TWeakObjectPtr<UTextBlock> What;
	};
	struct FKeyCapWidgets
	{
		TWeakObjectPtr<UTextBlock> Key;
		TWeakObjectPtr<UTextBlock> Device;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	FChoiceWidgets BuildChoice();
	UBorder *MakePanel(UVerticalBox *&OutColumn) const;
	/** The page's rows read fresh from the model, the list, the preview and Details. */
	void Refresh(bool bScrollIntoView = true);
	void RefreshPreview();
	void RefreshDetails();
	int32 FindUnder(const TArray<TObjectPtr<UBorder>> &Boxes, const FPointerEvent &MouseEvent) const;
	/** B-602: the settings row under the mouse, as the list is scrolled now. */
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;
	int32 FindChoiceUnder(const FPointerEvent &MouseEvent) const;

	ESettingsPage Page = ESettingsPage::Gameplay;
	/** B-631: the Controls page's kind (WoRSettingsScreen::GetKeySections). */
	int32 Kind = 0;
	TArray<FSettingsRow> Rows;
	int32 SelectedRow = 0;
	bool bChanged = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ListTitle;
	/** B-631: Controls' ‹ kind › plate. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> KindPlate;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> KindWords;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> KindLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> KindUnderlines;
	/** B-590 QA: the page's rows, scrolled to keep the selected one in view. */
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowList;
	TArray<FRowWidgets> RowWidgets;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ListNote;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewLine;
	/** B-631: a key's caps on Controls (keyboard, controller). */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> KeyPicture;
	TArray<FKeyCapWidgets> KeyCaps;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewBlurb;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsSub;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChoiceBox;
	TArray<FChoiceWidgets> ChoiceWidgets;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsNote;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;
};
