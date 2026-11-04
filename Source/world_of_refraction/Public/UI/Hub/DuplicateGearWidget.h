// DuplicateGearWidget.h
// OBSOLETE (B-339, D-131): wor.UI.Inventory (UInventoryWidget) does this as one of its verbs; delete after Crown's PIE.
// PLACEHOLDER UI (B-319, D-115): a character keeps one copy of a weapon or adapter. Lists each
// weaker copy with the copy that stays, and converts it to Gold or dismantles it. Plain buttons
// built in C++, opened with the console command wor.UI.Duplicates. Replaced by the CommonUI
// inventory screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DuplicateGearWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UDuplicateGearWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UDuplicateGearWidget *Open(APlayerController *PC);

	/** Re-read the pawn's duplicates and rewrite every row. */
	void Refresh();

	/** Debug: the rows as shown. */
	FString GetDuplicateString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** Converts (true) or dismantles the selected weaker copy. */
	void Resolve(bool bConvertToGold);

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleGoldClicked();

	UFUNCTION()
	void HandleDismantleClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 SelectedIndex = 0;

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PairText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	UPROPERTY()
	TObjectPtr<UButton> GoldButton;

	UPROPERTY()
	TObjectPtr<UButton> DismantleButton;
};
