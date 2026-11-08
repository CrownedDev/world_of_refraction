// RestSpotWidget.h
// PLACEHOLDER UI (B-187, Backlog rule 4): the rest spot's menu. The party has already rested (full HP / EP, the
// fallen revived); from here the player opens the inventory, or leaves the trial for a reduced payout after a
// confirm. Plain buttons built in C++ (like ULevelDropWidget); opened by ATrialRestSpot or wor.Trial.Rest. Replaced by
// the CommonUI screen (D-024).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RestSpotWidget.generated.h"

class UTextBlock;
class UButton;

UCLASS()
class WORLD_OF_REFRACTION_API URestSpotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static URestSpotWidget *Open(APlayerController *PC);

	/** B-187e: on a client, what the host said about this spot's mini-boss (the run lives on the host): Revive shows
	 *  when bCanRevive, labelled with RevivalNumber of MAX_REVIVALS. */
	void SetHostRevive(bool bCanRevive, int32 RevivalNumber);

	/** Debug: what the menu shows. */
	FString GetRestSpotWidgetString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void Refresh();

	UFUNCTION()
	void HandleInventoryClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandleConfirmLeaveClicked();

	UFUNCTION()
	void HandleCloseClicked();

	/** B-187b (Q-187b): revive the mini-boss whose rest spot this is (shown only when it can be). */
	UFUNCTION()
	void HandleReviveClicked();

	void CloseMenu();

	/** The leave button asks first; the second press (Confirm) leaves. */
	bool bConfirmingLeave = false;

	/** B-187e: the host's answer for a client's menu (SetHostRevive). */
	bool bHostCanRevive = false;
	int32 HostRevivalNumber = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PayoutText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;

	/** B-187b: Revive, with its label (which revival, how many left). */
	UPROPERTY(Transient)
	TObjectPtr<UButton> ReviveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReviveLabel;

	/** B-187d: Leave, hidden on a client (with the run's earnings) until B-187e routes it to the host. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;
};
