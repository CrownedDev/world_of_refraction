// HostLostCardWidget.h
// B-459a (UIScreens #33, mockup screen_mockups/coop/session/3_host_lost.png): the "The host left" card, centred over
// whatever world is up while the run moves to the next host. UHostLostCardPresenter shows it: it listens to the
// lost-host signal (UWoRHostHandOverSubsystem::OnStepChanged) and to each new world, since the hand-over travels
// between levels and a viewport widget goes with its world. B / Esc stops waiting when this machine waits on someone
// else. What it shows comes from WoRHostHandOverScreens::BuildCard (HostHandOverModel.h), which the specs read.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Coop/HostHandOverModel.h"
#include "HostLostCardWidget.generated.h"

class UBorder;
class UTextBlock;
struct FActorsInitializedParams;

UCLASS()
class WORLD_OF_REFRACTION_API UHostLostCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHostLostCardWidget(const FObjectInitializer &ObjectInitializer);

	/** Rewrites the texts and pips (built once, never rebuilt). */
	void Refresh(const FHostLostCard &InCard);
	const FHostLostCard &GetCard() const { return Card; }

	/** B: stop waiting (only when the card allows it). */
	void PressStop();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;

private:
	FHostLostCard Card;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HostText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeptText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> Pips;
};

/** Shows and hides the host-lost card as the hand-over moves on, in whichever world is up. */
UCLASS()
class WORLD_OF_REFRACTION_API UHostLostCardPresenter : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	/** The card in World now (null when hidden). */
	UHostLostCardWidget *GetCard() const { return Card.Get(); }

	/** Shows, refreshes or hides the card for the current hand-over step. */
	void Sync();

private:
	void HandleWorldActorsInitialized(const FActorsInitializedParams &Params);
	void Hide();

	TWeakObjectPtr<UHostLostCardWidget> Card;
	/** Looks at a world still waiting for its local player controller (reset per world). */
	int32 PlayerRetries = 0;
	FDelegateHandle StepChangedHandle;
	FDelegateHandle WorldInitHandle;
};
