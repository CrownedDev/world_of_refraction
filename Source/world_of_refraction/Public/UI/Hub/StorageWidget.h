// StorageWidget.h
// OBSOLETE (B-339, D-131): wor.UI.Inventory (UInventoryWidget) does this as one of its verbs; delete after Crown's PIE.
// PLACEHOLDER UI (B-324, D-117, Q-321): the shared home chest. One member at a time: its own weapons, adapters
// and pouch stacks (Put), and the chest's gear and stored item stacks (Take). Plain buttons built in C++,
// opened with the console command wor.UI.Storage (Backlog rule 4). Gear rules: WoRGive (Inventory/GearGive.h);
// item stacks: UPoolSubsystem::MoveToStorage / MoveToCarried (B-322). Replaced by the CommonUI screen (D-024).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "StorageWidget.generated.h"

class UButton;
class UTextBlock;
class UPoolSubsystem;

UCLASS()
class WORLD_OF_REFRACTION_API UStorageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UStorageWidget *Open(APlayerController *PC);

	/** Re-read the member's side and the chest and rewrite every row. */
	void Refresh();

	/** Debug: one line per row as shown. */
	FString GetStorageString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	struct FMember
	{
		FName Key;
		FText Name;
	};

	/** One thing on either side: a piece of gear (GearID) or an item stack (Item, Count); bMine = the member's. */
	struct FStorageChoice
	{
		FGuid GearID;
		FCrystalId Item;
		int32 Count = 0;
		FText Label;
		bool bMine = false;
		bool IsGear() const { return GearID.IsValid(); }
	};

	TArray<FMember> GatherMembers() const;
	TArray<FStorageChoice> GatherChoices() const;
	UPoolSubsystem *GetPool() const;
	FName MemberKey() const;

	/** Puts or takes the selected gear, or Count of the selected stack. */
	void MoveSelected(int32 Count);

	UFUNCTION()
	void HandleNextCharacterClicked();

	UFUNCTION()
	void HandleNextItemClicked();

	UFUNCTION()
	void HandleMoveOneClicked();

	UFUNCTION()
	void HandleMoveStackClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 MemberIndex = 0;
	int32 SelectedIndex = 0;

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> MemberText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ItemText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	UPROPERTY()
	TObjectPtr<UButton> MoveOneButton;

	UPROPERTY()
	TObjectPtr<UButton> MoveStackButton;
};
