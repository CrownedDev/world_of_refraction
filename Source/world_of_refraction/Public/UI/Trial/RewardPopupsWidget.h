// RewardPopupsWidget.h
// B-465b (UIScreens.md #25, mockup run/run_end/1_reward_popups.png): the reward popups on the right of the field. It
// takes what URewardNoticeSubsystem holds (at once when it is up, else when it is made back on the field after a
// fight), shows up to WoRRewardNotice::MAX_SHOWN stacked, each for SHOWN_SECONDS, fading over its last FADE_SECONDS.
// None takes a press or a click. UHubHUDWidget hosts it, so it hides with the HUD while a menu screen is up.
// A short looping timer runs only while a popup is shown or waiting (no Tick). Placeholder look (D-024).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Trial/RewardNotices.h"
#include "RewardPopupsWidget.generated.h"

class UWidget;
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API URewardPopupsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Moves the queue's notices in (shown while there is room, else waiting here) and starts the timer. */
	void TakeNotices();

	/** At NowSeconds (world time): fades and drops the old popups and shows waiting ones in their place. The timer
	 *  calls it; so can a spec. */
	void UpdatePopups(double NowSeconds);

	/** The popups on screen, oldest first. */
	TArray<FRewardNotice> GetShown() const;
	int32 GetWaitingCount() const { return Waiting.Num(); }
	FString GetPopupsString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	struct FShownPopup
	{
		FRewardNotice Notice;
		double ShownAtSeconds = 0.0;
		/** The popup on screen (its sized outer box), removed when it goes. */
		TWeakObjectPtr<UWidget> Box;
	};

	void ShowNext(double NowSeconds);
	UWidget *MakePopupBox(const FRewardNotice &Notice);
	void HandlePosted();
	void HandleTimer();
	double GetNowSeconds() const;

	TArray<FShownPopup> Shown;
	TArray<FRewardNotice> Waiting;
	FTimerHandle PopupTimer;
	FDelegateHandle PostedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Stack;
};
