// InventoryWidget.h
// PLACEHOLDER UI (B-339, D-131): the one inventory screen. A member's store (its weapons, adapters and pouch stacks)
// and the shared home chest (chest gear and stored stacks), with the verbs Give, Store, Take, Dismantle and Convert.
// Plain buttons built in C++, opened with the console command wor.UI.Inventory (Backlog rule 4). The rules are
// WoRInventoryVerbs' (Inventory/InventoryVerbs.h). Replaces wor.UI.Give / Storage / Dismantle / Duplicates (flagged
// obsolete); replaced in turn by the CommonUI screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites texts and enables, so nothing is recreated inside the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/InventoryVerbs.h"
#include "InventoryWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UInventoryWidget *Open(APlayerController *PC);

	/** Re-read the member's store and the chest and rewrite every row. */
	void Refresh();

	/** Runs Verb on the selected row (for a stack: one, or the whole stack while the whole-stack toggle is on). A verb
	 *  with something to confirm (D-016's dismantle warning, a convert's Gold) shows it first and runs on a second
	 *  press of the same verb on the same row. */
	void RunVerb(EInventoryVerb Verb);

	/** Debug: one line per row as shown. */
	FString GetInventoryString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	/** Party members by key, with their names. */
	TArray<TPair<FName, FText>> GatherMembers() const;
	FName MemberKey() const;
	FName ReceiverKey() const;

	UFUNCTION()
	void HandleNextMemberClicked();

	UFUNCTION()
	void HandleNextRowClicked();

	UFUNCTION()
	void HandleNextReceiverClicked();

	UFUNCTION()
	void HandleGiveClicked();

	UFUNCTION()
	void HandleStoreClicked();

	UFUNCTION()
	void HandleTakeClicked();

	UFUNCTION()
	void HandleDismantleClicked();

	UFUNCTION()
	void HandleConvertClicked();

	UFUNCTION()
	void HandleWholeStackClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 MemberIndex = 0;
	int32 ReceiverIndex = 1;
	int32 RowIndex = 0;
	/** Stack verbs move the whole stack instead of one (toggled by the whole-stack button). */
	bool bWholeStack = false;

	/** A verb waiting for its second press (D-016), on the row it was pressed on; any other choice cancels it. */
	bool bConfirming = false;
	EInventoryVerb PendingVerb = EInventoryVerb::Dismantle;
	FGuid PendingGearID;
	FText ConfirmText;

	/** Forgets a waiting confirmation and the last status. */
	void ClearPending();

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> MemberText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RowText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ReceiverText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;

	/** One button per verb, in WoRInventoryVerbs::GetAllVerbs order. */
	UPROPERTY()
	TArray<TObjectPtr<UButton>> VerbButtons;

	UPROPERTY()
	TObjectPtr<UButton> WholeStackButton;

	/** The whole-stack button's label, showing its state. */
	UPROPERTY()
	TObjectPtr<UTextBlock> WholeStackText;
};
