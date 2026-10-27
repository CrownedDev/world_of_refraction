// PartyControlWidget.h
// PLACEHOLDER UI (B-071): who plays each party member in battle (D-004), what
// happens when your fighters fall (D-045), each member's start cell (B-149, D-042) and a
// stand-in rest spot that revives fallen members (B-150, D-043). Plain buttons built in C++, opened with
// the console command wor.UI.PartyControl. Replaced by the CommonUI party and
// Settings screens (D-024).
//
// Built once in RebuildWidget (one row per party slot, the fall setting, Close);
// Refresh only rewrites the texts, so nothing is recreated inside the widget.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Party/Party.h"
#include "PartyControlWidget.generated.h"

class UButton;
class UTextBlock;
class UParty;

UCLASS()
class WORLD_OF_REFRACTION_API UPartyControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UPartyControlWidget *Open(APlayerController *PC);

	/** Re-read the party and rewrite every row. */
	void Refresh();

	/** Debug: one line per row as shown. */
	FString GetPartyControlString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// One handler per slot: a dynamic OnClicked can't carry the slot index.
	static_assert(PartyConstants::MAX_PARTY_MEMBERS == 3, "Add a slot handler per party slot");

	UFUNCTION()
	void HandleSlot1Clicked();

	UFUNCTION()
	void HandleSlot2Clicked();

	UFUNCTION()
	void HandleSlot0FormationClicked();

	UFUNCTION()
	void HandleSlot1FormationClicked();

	UFUNCTION()
	void HandleSlot2FormationClicked();

	UFUNCTION()
	void HandleFallSettingClicked();

	UFUNCTION()
	void HandleReviveFallenClicked();

	/** B-178: pay Gold to repair the party's carried gear, as at a rest spot. */
	UFUNCTION()
	void HandleRepairGearClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePartyChanged(UParty *Party);

	void ToggleSlot(int32 SlotIndex);

	/** Step the member to the next start cell (Back/Left .. Front/Right, wrapping). */
	void CycleFormation(int32 SlotIndex);

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotTexts;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> SlotButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> FormationTexts;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> FormationButtons;

	UPROPERTY()
	TObjectPtr<UTextBlock> FallSettingText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RepairGearText;
};
