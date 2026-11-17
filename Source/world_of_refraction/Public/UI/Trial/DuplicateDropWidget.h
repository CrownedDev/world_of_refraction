// DuplicateDropWidget.h
// B-565 (D-225, TrialPoolDesign §6 and stage 1): the duplicate prompt. A drop the player already has: Replace yours, Add
// it (keep both), or Convert to essence. Up / Down choose, A picks, a mouse click picks. Back keeps both (Q-565: closing
// it loses nothing). A CommonUI screen on the root layout's menu layer; stage 1 opens it with made-up contents
// (wor.UI.PreviewDuplicate) and only logs the choice; stage 6 acts on it through OnChosen.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialPoolLook.h"
#include "UI/WoRActivatableWidget.h"
#include "DuplicateDropWidget.generated.h"

class APlayerController;
class UBorder;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UDuplicateDropWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UDuplicateDropWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the prompt onto PC's root layout; null without one. OnChosen runs once, with the choice. */
	static UDuplicateDropWidget *Open(APlayerController *PC, const FDuplicateDropView &View, TFunction<void(EDuplicateChoice)> OnChosen = nullptr);

	void SetView(const FDuplicateDropView &InView, TFunction<void(EDuplicateChoice)> InOnChosen = nullptr);
	int32 GetSelectedChoice() const { return SelectedChoice; }
	void MoveSelection(int32 Delta);
	/** A: the lit choice; Back: Q-565's keep-both. Either closes the prompt. */
	void Choose(EDuplicateChoice Choice);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	/** Closed some other way (a layer pop, a level change): Q-565's keep-both still reaches OnChosen. */
	virtual void NativeOnDeactivated() override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	void Refresh();
	int32 FindChoiceUnder(const FPointerEvent &MouseEvent) const;

	FDuplicateDropView View;
	TFunction<void(EDuplicateChoice)> OnChosen;
	int32 SelectedChoice = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuestionText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ChoiceRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceLabels;
};
