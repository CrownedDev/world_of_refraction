// TrialGateWidget.h
// PLACEHOLDER UI (B-183): what the trial door asks before a run, its variation and its
// difficulty (RunStructure §3, Q-183). Plain buttons built in C++, opened by ATrialDoor::Interact.
// Replaced by the CommonUI gate screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Trial/TrialGate.h"
#include "TrialGateWidget.generated.h"

class UButton;
class UTextBlock;
class UTrialData;

UCLASS()
class WORLD_OF_REFRACTION_API UTrialGateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one asking about Trial to PC's viewport (UI-only input, cursor on).
	 *  Null when PC or Trial is null. */
	static UTrialGateWidget *Open(APlayerController *PC, UTrialData *Trial);

	void Refresh();

	/** Debug: the rows as shown. */
	FString GetTrialGateString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleVariationClicked();

	UFUNCTION()
	void HandleDifficultyClicked();

	UFUNCTION()
	void HandleEnterClicked();

	UFUNCTION()
	void HandleCancelClicked();

	/** Give the player back game input and remove the panel. */
	void Close();

	UPROPERTY()
	TObjectPtr<UTrialData> Trial;

	int32 VariationIndex = INDEX_NONE;
	ETrialDifficulty Difficulty = TrialGate::DEFAULT_DIFFICULTY;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> VariationText;

	UPROPERTY()
	TObjectPtr<UButton> VariationButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> DifficultyText;
};
