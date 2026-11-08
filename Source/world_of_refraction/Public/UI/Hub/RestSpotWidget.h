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

	/** Debug: what the menu shows. */
	FString GetRestSpotWidgetString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

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

	void CloseMenu();

	/** The leave button asks first; the second press (Confirm) leaves. */
	bool bConfirmingLeave = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PayoutText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;
};
