// NewGameScreenWidget.h
// B-469 (UIScreens #11, mockups screen_mockups/front/new_game/1_new_game.png to 3_new_game_generic.png): the new game
// screen. A CommonUI screen on the root layout's menu layer, opened from the title's New Game (with no save, or after
// the hold has deleted it) or wor.UI.NewGame. B goes back to the title.
//
// Body (Manny / Quinn), Class (Generic, Refractor, Resonator with its line and budget), and for a Refractor the nine
// elements in the side panel; the panel's title and text, and the Begin button, follow the pick. What it says and
// what Begin does are WoRNewGame (NewGameScreenModel.h). The heading is a placeholder: the opening is Crown's.
// D-233 (B-538): the mouse too: hover lights a card, chip or Begin; a click picks it (ClickPart); the cursor shows.
// B-546 (D-228, stage 1, look only): a typed name on top; the side panel's grid is the class's own pick: a
// Refractor's element (Broken Darkness and Reality among them), a Generic's two weapons, a Resonator's two crystal
// types. Not built: the body previews (grey boxes).

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/NewGameScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "NewGameScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UEditableTextBox;
class UTextBlock;
class UWidget;

/** Where the keys are: the name, the body pair, the class cards, the class's pick grid, Begin. */
enum class ENewGameFocus : uint8
{
	Name, // B-546
	Body,
	Class,
	/** The class's own pick grid (B-546: every class has one; the name stays from the Refractor-only grid). */
	Element,
	Begin
};

UCLASS()
class WORLD_OF_REFRACTION_API UNewGameScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UNewGameScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the title). */
	static UNewGameScreenWidget *Open(APlayerController *PC);
	/** B-562 (stage 5): a co-op guest's new character: Begin sends its picks to the host (WoRTitleScreen::SendJoin)
	 *  instead of starting a game. shortcut: the character is not written to the guest's own slots (Q-561); make it
	 *  on the title first to keep it. */
	static UNewGameScreenWidget *OpenToJoin(APlayerController *PC);
	bool IsJoining() const { return bJoining; }

	// What the keys do (specs call them directly).
	void MoveUp();
	void MoveDown();
	void MoveSide(int32 Delta);
	/** A: Begin on the button, else the next part down. */
	void Confirm();
	/** D-233: a click on a part: a body card (Index: Manny 0, Quinn 1), a class card or an element chip (Index in
	 *  WoRNewGame's lists) takes the focus and the pick; Begin (Index unused) is Confirm on Begin, A's path. An index
	 *  out of range, or an element with no element grid, does nothing. */
	void ClickPart(ENewGameFocus Part, int32 Index);

	/** B-546: what the name box sets (specs call it; the box calls it as the player types). */
	void SetName(const FString &Name);

	const FNewGamePick &GetPick() const { return Pick; }
	ENewGameFocus GetFocus() const { return Focus; }
	/** B-546: the lit entry of the pick grid. */
	int32 GetPickCursor() const { return PickCursor; }
	const FText &GetStatus() const { return Status; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	void BuildTree();
	void Refresh();
	int32 GetClassIndex() const;
	/** B-546: the grid's cursor moves; a Refractor's pick follows it (one pick), the others pick with A or a click. */
	void MovePickCursor(int32 Index);
	/** B-546: the cursor on the class's first picked entry, else the first. */
	void ResetPickCursor();
	/** B-546: the name box takes the keys (A on the name, or a click on it). */
	void StartTypingName();
	UFUNCTION()
	void HandleNameChanged(const FText &Text);
	UFUNCTION()
	void HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod);
	/** The part and index under the cursor; false over none. */
	bool FindPartUnder(const FVector2D &ScreenPosition, ENewGameFocus &OutPart, int32 &OutIndex) const;

	FNewGamePick Pick;
	/** B-562: opened to join (OpenToJoin); cleared when it closes (the stack may hand the widget out again). */
	bool bJoining = false;
	ENewGameFocus Focus = ENewGameFocus::Class;
	int32 PickCursor = 0;
	FText Status;
	/** D-233: what the mouse is over, lit with the hover colour (INDEX_NONE: nothing). */
	ENewGameFocus HoveredPart = ENewGameFocus::Body;
	int32 HoveredIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> BodyCards;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ClassCards;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ClassKinds;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ClassLines;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ElementChips;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ElementNames;
	/** B-546: each chip's element mark (hidden for a weapon). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ElementMarks;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClassLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ElementLabel;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BeginCard;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BeginText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelMark;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelAbout;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelElementLine;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ElementArea;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PanelFootnote;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};
