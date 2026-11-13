// PauseMenuWidget.h
// B-439 (D-181, UIScreens #1, mockups screen_mockups/menus/pause/): the game menu. A CommonUI screen on the
// root layout's menu layer: the page list on the left, the party beside it, one side panel that says what the
// focused page is (or why it can't open yet), the place and its money on top.
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
	/** One party card's widgets, built once and refilled each time the menu opens (never rebuilt: TransBuffer). */
	struct FMemberCard
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Tier;
		TWeakObjectPtr<UTextBlock> ClassLine;
		TWeakObjectPtr<UProgressBar> HPBar;
		TWeakObjectPtr<UTextBlock> HPValue;
		TWeakObjectPtr<UProgressBar> EPBar;
		TWeakObjectPtr<UTextBlock> EPValue;
		TWeakObjectPtr<UTextBlock> Mind;
		TWeakObjectPtr<UTextBlock> Body;
		TWeakObjectPtr<UTextBlock> Spirit;
	};

	void BuildTree();
	FMemberCard BuildMemberCard(class UVerticalBox *Into);
	/** Header, pages and party, read fresh from the model. */
	void RefreshData();
	/** The focused row's colour and the side panel. */
	void RefreshSelection();
	void OpenPage(const FPauseMenuPage &Page);
	/** Closes this menu, then opens the page's old viewport widget once the root layout has let go of it. */
	void OpenPlaceholder(EPauseMenuPage Page);
	void BeginQuitHold();
	void StopQuitHold();
	void TickQuitHold();
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	TArray<FPauseMenuPage> Pages;
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
	TArray<TObjectPtr<UTextBlock>> ChipValues;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChipLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PageRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PageLabels;
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
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExtractNote;

	TArray<FMemberCard> MemberCards;
};
