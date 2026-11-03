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

	/** Re-read the viewed character's pouch and storage and rewrite every row. */
	void Refresh();

	/** B-322: whose pouch the screen shows (a character key), NAME_None with no party. */
	FName GetViewedCharacterKey() const;

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

	/** One party member whose pouch can be viewed. */
	struct FPouchOwner
	{
		FName Key;
		FText Name;
	};

	/** The local party's members with their own pouch (run-only recruits too: they carry items). */
	TArray<FPouchOwner> GatherPouchOwners() const;

	/** Move Count of the selected stack to the other side. */
	void MoveSelected(int32 Count);

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleNextCharacterClicked();

	UFUNCTION()
	void HandleMoveOneClicked();

	UFUNCTION()
	void HandleMoveAllClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 SelectedIndex = 0;

	/** Index into GatherPouchOwners. */
	int32 ViewedOwnerIndex = 0;

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
