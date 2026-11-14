// NewGameScreenWidget.h
// B-469 (UIScreens #11, mockups screen_mockups/front/new_game/1_new_game.png to 3_new_game_generic.png): the new game
// screen. A CommonUI screen on the root layout's menu layer, opened from the title's New Game (with no save, or after
// the hold has deleted it) or wor.UI.NewGame. B goes back to the title.
//
// Body (Manny / Quinn), Class (Generic, Refractor, Resonator with its line and budget), and for a Refractor the nine
// elements in the side panel; the panel's title and text, and the Begin button, follow the pick. What it says and
// what Begin does are WoRNewGame (NewGameScreenModel.h). The heading is a placeholder: the opening is Crown's.
// Not built: the body previews (grey boxes), the mouse.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/NewGameScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "NewGameScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UTextBlock;
class UWidget;

/** Where the keys are: the body pair, the class cards, the element grid (a Refractor only), Begin. */
enum class ENewGameFocus : uint8
{
	Body,
	Class,
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

	// What the keys do (specs call them directly).
	void MoveUp();
	void MoveDown();
	void MoveSide(int32 Delta);
	/** A: Begin on the button, else the next part down. */
	void Confirm();

	const FNewGamePick &GetPick() const { return Pick; }
	ENewGameFocus GetFocus() const { return Focus; }
	const FText &GetStatus() const { return Status; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;

private:
	void BuildTree();
	void Refresh();
	bool HasElements() const { return Pick.Class == ECharacterClass::Caster; }
	int32 GetClassIndex() const;
	int32 GetElementIndex() const;

	FNewGamePick Pick;
	ENewGameFocus Focus = ENewGameFocus::Class;
	FText Status;

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
