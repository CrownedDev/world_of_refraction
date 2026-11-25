// PauseMenuWidget.h
// B-439 (D-181, UIScreens #1), B-632 (mockup screen_mockups/menus/pause/v2): the game menu. A CommonUI screen on the
// root layout's menu layer, two columns: the pages on the left (a name and a short line each); on the right "This run"
// (fights won, run gold, items found, time; in a run only), each member's Mind, Body and Spirit, then the lit page's
// note (or why it can't open yet, or the quit question). "Paused" and the place on top.
//
// Built in C++ (no WBP); the layout follows the mockup, the look is the mockup kit's flat colours until the
// Hidris art pass. What it shows comes from WoRPauseMenu (PauseMenuModel.h), which the specs read.
//
// Opened by IA_ToggleMenu (AWoRHubController::OpenGameMenu) or wor.UI.Pause. Nothing pauses (co-op, Q-439).
// A page with no CommonUI screen yet opens its old placeholder widget after this menu has closed.
// Quit asks for a hold (WoRPauseMenu::QUIT_HOLD_SECONDS); in a run it loses everything (D-176).

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/PauseMenuModel.h"
#include "UI/WoRActivatableWidget.h"
#include "PauseMenuWidget.generated.h"

class APlayerController;
class UBorder;
class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API UPauseMenuWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UPauseMenuWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the menu onto PC's root layout. Null when PC has none or a screen is already up. */
	static UPauseMenuWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	void SelectIndex(int32 Index);
	/** Confirm pressed on the focused page: opens it; Quit asks first, then a second press starts the hold. */
	void PressSelected();
	/** Confirm let go: an unfinished quit hold stops. */
	void ReleaseSelected();
	/** Back: leaves the quit confirm, else closes the menu. */
	void PressBack() { HandleBackAction(); }

	int32 GetSelectedIndex() const { return SelectedIndex; }
	bool IsConfirmingQuit() const { return bConfirmingQuit; }
	bool IsHoldingQuit() const { return QuitHoldTimer.IsValid(); }
	const TArray<FPauseMenuPage> &GetPages() const { return Pages; }
	/** B-632: what the right column shows (the specs read it). */
	const FPauseMenuRunSummary &GetRunSummary() const { return Summary; }
	/** B-632: the lit page's note heading and text, as drawn. */
	FText GetNoteHeading() const;
	FText GetNoteText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent &InFocusEvent) override;

private:
	/** One member's Mind · Body · Spirit row, built once and refilled each time the menu opens (never rebuilt: TransBuffer). */
	struct FMemberRow
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<class UImage> Badge;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> ClassName;
		TWeakObjectPtr<UTextBlock> Mind;
		TWeakObjectPtr<UTextBlock> Body;
		TWeakObjectPtr<UTextBlock> Spirit;
	};

	void BuildTree();
	UWidget *BuildPagesColumn();
	UWidget *BuildRunColumn();
	FMemberRow BuildMemberRow(class UVerticalBox *Into);
	/** Header, pages, the run and the party, read fresh from the model. */
	void RefreshData();
	/** The focused row's colour and the side panel. */
	void RefreshSelection();
	void OpenPage(const FPauseMenuPage &Page);
	/** Closes this menu, then opens the page's old viewport widget once the root layout has let go of it. */
	void OpenPlaceholder(EPauseMenuPage Page);
	void BeginQuitHold();
	/** B-453c: the player's hold-to-confirm setting (on without a save). */
	bool IsHoldToConfirmOn() const;
	void StopQuitHold();
	void TickQuitHold();
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	TArray<FPauseMenuPage> Pages;
	FPauseMenuRunSummary Summary;
	bool bInRun = false;
	int32 SelectedIndex = 0;
	bool bConfirmingQuit = false;
	FTimerHandle QuitHoldTimer;
	double QuitHoldStartSeconds = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageSubLines;
	/** The run block: "This run", the place, the four rows and the line under them (hidden outside a run). */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> RunBlock;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunPlace;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FightsWonValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunGoldValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ItemsFoundValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelHeading;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelBody;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelRefusal;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HoldHint;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HoldBar;

	TArray<FMemberRow> MemberRows;
};
