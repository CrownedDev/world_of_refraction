// GateScreenWidget.h
// B-466 (D-191, D-192; UIScreens #21, mockups screen_mockups/run/gate/): the Gate. A CommonUI screen on the root
// layout's menu layer, opened by the hub's trial door (ATrialDoor::Interact) or wor.UI.Gate. LB / RB pick the gate,
// Up / Down move through the trials, the difficulty row and Enter; A picks the lit trial, steps the difficulty (Left /
// Right too) or enters; X opens the party screen, Y the loadout, B goes back. What it shows comes from WoRGateScreen
// (GateScreenModel.h), which the specs read. Replaces the placeholder UTrialGateWidget, which the door still opens
// when there is no root layout (removed after Crown's PIE pass).

#pragma once

#include "CoreMinimal.h"
#include "UI/Trial/GateScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "GateScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UTrialData;
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API UGateScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UGateScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout, on StartGate's tab (the first gate when null). Null without a root layout. */
	static UGateScreenWidget *Open(APlayerController *PC, UTrialData *StartGate);

	/** The gates to pick from and the one shown (the specs set their own). */
	void SetGates(const TArray<UTrialData *> &InGates, const UTrialData *StartGate);

	// What the keys and the mouse do (the specs call these too).
	void MoveGate(int32 Delta);
	void MoveFocus(int32 Delta);
	void StepDifficulty(int32 Delta);
	/** A on the lit row: picks a trial, steps the difficulty, or enters. */
	void PressSelect();

	const FGateScreenPage &GetPage() const { return Page; }
	UTrialData *GetGate() const;
	/** 0..Trials-1 a trial, then the difficulty row, then Enter. */
	int32 GetFocus() const { return Focus; }
	int32 GetDifficultyRow() const { return Page.Trials.Num(); }
	int32 GetEnterRow() const { return Page.Trials.Num() + 1; }
	int32 GetChosenTrial() const { return ChosenTrial; }
	ETrialDifficulty GetDifficulty() const { return Difficulty; }
	/** The last refusal; empty once something goes through. */
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Line;
		TWeakObjectPtr<UTextBlock> Mark;
	};

	void BuildTree();
	FRowWidgets BuildTrialRow();
	FRowWidgets BuildMemberRow(UVerticalBox *Column);
	/** The page for the shown gate, read fresh, then drawn. */
	void Refresh();
	/** The gate's first open trial is picked; the lit row follows it. */
	void ResetChoice();
	int32 RowCount() const { return Page.Trials.Num() + 2; }
	/** The row under the cursor: a trial, the difficulty row (with its card), or Enter; INDEX_NONE for none. */
	int32 FindRowUnder(const FPointerEvent &MouseEvent, int32 &OutCard) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTrialData>> Gates;
	int32 GateIndex = 0;
	int32 ChosenTrial = INDEX_NONE;
	ETrialDifficulty Difficulty = TrialGate::DEFAULT_DIFFICULTY;
	int32 Focus = 0;
	FGateScreenPage Page;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> GateTabBox;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> TrialList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DifficultyNoteText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunNoteText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WipesText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> EnterFrame;

	TArray<TWeakObjectPtr<UBorder>> GateTabs;
	TArray<TWeakObjectPtr<UTextBlock>> GateTabLabels;
	TArray<FRowWidgets> TrialRows;
	/** Name = the level, Line = damage, Mark = payout. */
	TArray<FRowWidgets> DifficultyCards;
	/** Name, Line = class, Mark = who plays it. */
	TArray<FRowWidgets> MemberRows;
};
