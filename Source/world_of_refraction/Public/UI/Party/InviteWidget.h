// InviteWidget.h
// PLACEHOLDER UI (B-233b, Backlog rule 4): invite friends to the co-op session. Shows the session state and the
// friends list (UWoRSessionSubsystem::ReadFriends), one friend picked at a time (Previous / Next), Invite, Host a session
// (when none is hosted), Seat a friend (opens UJoinSeatWidget) and Close. The Null subsystem (LAN) has no friends
// list: the screen says friends use wor.Session.Find / Join. Opened by wor.UI.Invite (the hub) or the rest menu.
// Replaced by the CommonUI party screen (D-024).
//
// Built once in RebuildWidget; Refresh only rewrites the texts, so nothing is recreated inside the widget.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InviteWidget.generated.h"

class UButton;
class UTextBlock;
class APlayerController;

UCLASS()
class WORLD_OF_REFRACTION_API UInviteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on) and read the friends list. Null when PC is null. */
	static UInviteWidget *Open(APlayerController *PC);

	/** Rewrite the texts from the session subsystem. */
	void Refresh();

	/** Debug: the lines as shown. */
	FString GetInviteString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandlePreviousClicked();
	UFUNCTION()
	void HandleNextClicked();
	UFUNCTION()
	void HandleInviteClicked();
	UFUNCTION()
	void HandleHostClicked();
	UFUNCTION()
	void HandleSeatClicked();
	UFUNCTION()
	void HandleCloseClicked();

	void HandleFriendsRead();
	void Close();

	/** The friend Invite sends to (index into GetFriendNames). */
	int32 PickedFriend = 0;
	FDelegateHandle FriendsReadHandle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SessionText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FriendsText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InviteLabel;
	UPROPERTY(Transient)
	TObjectPtr<UButton> InviteButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> HostButton;
};
