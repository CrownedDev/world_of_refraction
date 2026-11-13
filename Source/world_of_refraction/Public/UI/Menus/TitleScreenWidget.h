// TitleScreenWidget.h
// B-467 (D-202, D-060; UIScreens #9, #10, mockups screen_mockups/front/title/1_title.png to 3_new_game_confirm.png): the
// title screen. A CommonUI screen on the root layout's menu layer. Outside the editor it opens once per launch, over
// the first level (the save is loaded by then); in PIE wor.UI.Title opens it. Back never closes it.
//
// Left: "WORLD OF REFRACTION", "TALES OF THE VOID" and the rows (WoRTitleScreen). Continue goes on where the save is
// (the level is already the save's); New Game over a save asks first and needs A held for 2 seconds; Settings opens
// the settings screen over it; Quit quits. "Your save was reset" (D-060) shows first when the load wiped an old save.
// Not built: the nebula and the hexagon vortex (an editor step, D-202); a fresh game in place after New Game deletes
// the save (B-469; until then the new game begins at the next start).

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
	/** New Game over a save: hold A to start over. */
	NewGameConfirm
};

UCLASS()
class WORLD_OF_REFRACTION_API UTitleScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UTitleScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout. */
	static UTitleScreenWidget *Open(APlayerController *PC);
	/** Open, the first time a level starts in this launch of the game, until the player leaves the title. */
	static void OpenAtLaunch(APlayerController *PC);

	// What the keys and the mouse do.
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index);
	void Confirm();
	/** New Game's confirm: held time added (the ticker calls it while A is down). */
	void UpdateHold(float DeltaSeconds);

	ETitlePanel GetPanel() const { return Panel; }
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
	void StartHold();
	void StopHold();
	/** The hold is done: the save is deleted (the new game begins at the next start, B-469). */
	void StartOver();
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	TArray<FTitleScreenRow> Rows;
	FTitleSaveSummary Save;
	int32 SelectedRow = 0;
	ETitlePanel Panel = ETitlePanel::None;
	FText Status;
	float HeldSeconds = 0.0f;
	FTSTicker::FDelegateHandle HoldTicker;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RowBoxes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowLines;
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
	UPROPERTY(Transient)
	TObjectPtr<UWidget> HoldLines;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HoldBar;
};
