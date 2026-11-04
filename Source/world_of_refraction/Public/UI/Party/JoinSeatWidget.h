// JoinSeatWidget.h
// PLACEHOLDER UI (B-360, D-150): the host chooses which AI seat a joining player takes. One line per
// pending join request, one button per AI seat ("Give slot N to the first request"), Refuse, Close.
// Plain buttons built in C++, opened on the host with the console command wor.UI.JoinSeat. Replaced
// by the CommonUI party screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside the widget.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Party/Party.h"
#include "JoinSeatWidget.generated.h"

class UButton;
class UTextBlock;
class UParty;

UCLASS()
class WORLD_OF_REFRACTION_API UJoinSeatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static UJoinSeatWidget *Open(APlayerController *PC);

	/** Re-read the join requests and the party, rewrite every line. */
	void Refresh();

	/** Debug: the lines as shown. */
	FString GetJoinSeatString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// One handler per seat slot: a dynamic OnClicked can't carry the slot index. Slot 0 is the leader's.
	static_assert(PartyConstants::MAX_PARTY_MEMBERS == 3, "Add a seat handler per party slot");
	UFUNCTION()
	void HandleSlot1Clicked();
	UFUNCTION()
	void HandleSlot2Clicked();
	UFUNCTION()
	void HandleRefuseClicked();
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandlePartyChanged(UParty *Party);

	/** The first pending request's player id, or INDEX_NONE. */
	int32 FirstRequestPlayerId() const;
	void GiveSeat(int32 SlotIndex);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RequestsText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SeatTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SeatButtons;
};
