// TitleScreenWidget.h
// B-467 (D-202, D-060; UIScreens #9, #10, mockups screen_mockups/front/title/1_title.png to 3_new_game_confirm.png): the
// title screen. A CommonUI screen on the root layout's menu layer. B-535: it opens on a play session's first map load
// when that is the game's default map (the hub), PIE included; wor.UI.TitleAtStart 0 skips it, wor.UI.Title opens it.
// Back never closes it.
//
// B-546 (D-229, stage 1): all centred: "WORLD OF REFRACTION", "TALES OF THE VOID", then the character slots
// (WoRTitleScreen::BuildRows), then Settings, Credits and Quit as buttons along the bottom. A filled slot continues
// (the level is already the save's); the + opens the new game screen (B-469), over a save after a yes / no (one save
// file until stage 3). R / Y on a filled slot deletes it after a yes / no. "Your save was reset" (D-060) shows first
// when the load wiped an old save. Not built: the nebula and the hexagon vortex (an editor step, D-202), and the
// character figure: it goes on the right, and TitleScreenStyle::SlotsAlignment moves the slots left when it comes.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UI/Menus/TitleScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "TitleScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UProgressBar;
class UTextBlock;
class UWidget;

enum class ETitlePanel : uint8
{
	None,
	/** D-060: the load wiped an old save. */
	SaveReset,
	/** The + over a save: yes / no to start over (B-546; it was a 2-second hold). */
	NewGameConfirm,
	/** B-546: R / Y on a filled slot: yes / no to delete it. */
	DeleteConfirm
};

UCLASS()
class WORLD_OF_REFRACTION_API UTitleScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UTitleScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout. */
	static UTitleScreenWidget *Open(APlayerController *PC);
	/** B-562 (D-229, stage 5): a co-op guest's join, from the game menu: the same slot list without the buttons. A
	 *  filled slot sends that character's picks to the host (WoRTitleScreen::SendJoin); the + opens the new game screen
	 *  to join (UNewGameScreenWidget::OpenToJoin). No delete; Back closes it. */
	static UTitleScreenWidget *OpenToJoin(APlayerController *PC);
	bool IsJoining() const { return bJoining; }
	/** B-535: Open on a play session's first map load, when it is the game's default map (WoRTitleScreen::ShouldOpenAtStart).
	 *  OBSOLETE (B-537): the menu level opens the title itself; once it is GameDefaultMap this opens nothing. */
	static void OpenAtLaunch(APlayerController *PC);
	/** B-469: the new game began from the screen over the title. OBSOLETE (B-535): OpenAtLaunch opens once a session anyway. */
	static void MarkLeftThisLaunch();

	// What the keys and the mouse do.
	/** Up / Down: through the slots, then down into the buttons. */
	void MoveSelection(int32 Delta);
	/** B-546: Left / Right: across the buttons, or between a panel's yes and no. */
	void MoveSide(int32 Delta);
	void SelectRow(int32 Index);
	void Confirm();
	/** B-546: R / Y: a filled slot asks to be deleted. */
	void RequestDelete();
	/** B-546: a yes / no panel's answer. */
	void ChooseAnswer(bool bYes);
	/** New Game's confirm: held time added. OBSOLETE (B-546): the confirm is a yes / no; retire after PIE. */
	void UpdateHold(float DeltaSeconds);

	ETitlePanel GetPanel() const { return Panel; }
	bool IsYesLit() const { return bPanelYes; }
	const TArray<FTitleScreenRow> &GetRows() const { return Rows; }
	int32 GetSelectedRow() const { return SelectedRow; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent &InFocusEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	void BuildTree();
	/** The rows, read fresh from the save, and the panel. */
	void Refresh();
	void ShowPanel(ETitlePanel InPanel);
	/** Continue, or New Game with no save: the title is done for this launch. */
	void Leave();
	/** OBSOLETE (B-546): nothing starts the hold now; retire after PIE. */
	void StartHold();
	void StopHold();
	/** The + over a save, answered yes: the save is deleted and the new game screen opens (B-469). */
	void StartOver();
	/** B-546: the delete answered yes: the save is deleted and the slots read again. */
	void DeleteCharacter();
	/** B-562: the lit slot's character asks the host for a seat, and the screen closes. */
	void JoinAs(const FTitleScreenRow &Row);
	/** The slots come first in Rows, then the buttons. */
	int32 GetSlotCount() const;
	UBorder *GetRowBox(int32 Index) const;
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;
	/** 1 under Yes, 0 under No, else INDEX_NONE. */
	int32 FindAnswerUnder(const FPointerEvent &MouseEvent) const;
	bool IsYesNoPanel() const { return Panel == ETitlePanel::NewGameConfirm || Panel == ETitlePanel::DeleteConfirm; }

	TArray<FTitleScreenRow> Rows;
	/** B-554: the lit slot's save (the + and the buttons: the active slot's), for the panels and the log. */
	FTitleSaveSummary Save;
	/** B-554 (D-229): every filled slot, read from disk when the title opens and after a delete (not on every move). */
	TArray<FTitleSaveSummary> SlotSaves;
	bool bSlotsRead = false;
	/** B-562: opened to join a friend's game (OpenToJoin); cleared when it closes. */
	bool bJoining = false;
	int32 SelectedRow = 0;
	ETitlePanel Panel = ETitlePanel::None;
	/** B-546: a yes / no panel opens on No. */
	bool bPanelYes = false;
	FText Status;
	float HeldSeconds = 0.0f;
	FTSTicker::FDelegateHandle HoldTicker;

	/** The slots' boxes. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RowBoxes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowLines;
	/** B-546: Settings, Credits and Quit along the bottom. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ButtonBoxes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ButtonLabels;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> PanelLayer;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelBody;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> VersionLines;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OldVersionText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NeededVersionText;
	/** OBSOLETE (B-546): never shown now; retire with the hold. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> HoldLines;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HoldBar;
	/** B-546: a yes / no panel's answers. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> AnswerLines;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> YesBox;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> NoBox;
};
