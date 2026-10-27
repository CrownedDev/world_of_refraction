// DismantleWidget.h
// PLACEHOLDER UI (B-084): dismantle one of your weapons or rings, with the D-016 warning when it
// is equipped in a saved loadout. Plain buttons built in C++, opened with the console command
// wor.UI.Dismantle. Replaced by the CommonUI inventory screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts and shows or hides the confirm
// buttons, so nothing is recreated inside the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DismantleWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UDismantleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UDismantleWidget *Open(APlayerController *PC);

	/** Re-read the pawn's weapons and rings and rewrite every row. */
	void Refresh();

	/** Debug: one line per row as shown. */
	FString GetDismantleString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** One of the pawn's weapons or rings, by instance. */
	struct FDismantleChoice
	{
		FGuid PersistentID;
		FText Label;
		bool bRing = false;
	};

	TArray<FDismantleChoice> GatherChoices() const;

	/** Dismantle the selected item now; the warning was shown or there was none. */
	void DismantleSelected();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleDismantleClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 SelectedIndex = 0;

	/** The warning is showing and waits for Confirm or Cancel. */
	bool bConfirming = false;

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ItemText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WarningText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	UPROPERTY()
	TObjectPtr<UButton> DismantleButton;

	UPROPERTY()
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY()
	TObjectPtr<UButton> CancelButton;
};
