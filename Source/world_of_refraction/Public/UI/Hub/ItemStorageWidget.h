// ItemStorageWidget.h
// PLACEHOLDER UI (B-203, D-097): move crystals and stones between the party's carried item slots
// and storage. Plain buttons built in C++, opened with the console command wor.UI.ItemStorage
// (D-097 places it at rest spots and in the hub; those spots open it once they exist, B-187).
// Replaced by the CommonUI inventory screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside
// the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "ItemStorageWidget.generated.h"

class UButton;
class UTextBlock;
class UPoolSubsystem;

UCLASS()
class WORLD_OF_REFRACTION_API UItemStorageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UItemStorageWidget *Open(APlayerController *PC);

	/** Re-read the carried items and storage and rewrite every row. */
	void Refresh();

	/** Debug: one line per row as shown. */
	FString GetItemStorageString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** One stack, carried or in storage. */
	struct FStackChoice
	{
		FCrystalId Id;
		int32 Count = 0;
		bool bCarried = false;
	};

	TArray<FStackChoice> GatherChoices() const;

	UPoolSubsystem *GetPool() const;

	/** Move Count of the selected stack to the other side. */
	void MoveSelected(int32 Count);

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleMoveOneClicked();

	UFUNCTION()
	void HandleMoveAllClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 SelectedIndex = 0;

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SlotsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ItemText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	UPROPERTY()
	TObjectPtr<UButton> MoveOneButton;

	UPROPERTY()
	TObjectPtr<UButton> MoveAllButton;
};
