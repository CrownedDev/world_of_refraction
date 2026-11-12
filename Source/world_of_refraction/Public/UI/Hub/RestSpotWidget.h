// RestSpotWidget.h
// PLACEHOLDER UI (B-187, Backlog rule 4): the rest spot's menu. The party has already rested (full HP / EP, the
// fallen revived); from here the player opens the inventory, or leaves the trial for a reduced payout after a
// confirm. Plain buttons built in C++ (like ULevelDropWidget); opened by ATrialRestSpot or wor.Trial.Rest. Replaced by
// the CommonUI screen (D-024). B-233b: the host invites and seats friends from here (drop-in happens at rest spots).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RestSpotWidget.generated.h"

class UTextBlock;
class UButton;
class APlayerController;

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

	/** B-187e (Q-187e changed): PC's player answers the open revive vote (a client through ServerCastReviveVote, the host
	 *  directly). The menu's buttons and the wor.Trial.Vote console placeholder use it. */
	static void CastReviveVoteFor(APlayerController *PC, bool bYes);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void Refresh();

	/** B-363: the level screen (place and buy pillar points). */
	UFUNCTION()
	void HandleLevelUpClicked();

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

	/** B-187e (Q-187e changed): this player's answer to the open revive vote. */
	UFUNCTION()
	void HandleVoteYesClicked();
	UFUNCTION()
	void HandleVoteNoClicked();

	/** The revive vote changed (AWoRGameState::OnReviveVoteChanged): redraw. */
	UFUNCTION()
	void HandleReviveVoteChanged();
	/** B-234c: the host hands the run over to the next player (shown only to a host with another player). */
	UFUNCTION()
	void HandleHandOverClicked();
	/** B-233b: the host opens the invite screen (the party stays at the rest spot, so friends can be seated). */
	UFUNCTION()
	void HandleInviteClicked();

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

	/** B-187e (Q-187e changed): the vote's line and this player's two answers (placeholder until the real screen). */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VoteText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VoteYesButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VoteNoButton;
	/** B-234c: Hand over host. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> HandOverButton;
	/** B-233b: Invite friends (the host only). */
	UPROPERTY(Transient)
	TObjectPtr<UButton> InviteButton;

	/** B-187d: Leave, hidden on a client (with the run's earnings) until B-187e routes it to the host. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;
};
