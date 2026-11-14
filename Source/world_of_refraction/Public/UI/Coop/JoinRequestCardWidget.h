// JoinRequestCardWidget.h
// B-459c (D-150, D-200; UIScreens #32, mockup screen_mockups/coop/session/2_join_request.png): the host's join request
// card, small in the top-right corner so it never stops play (Elden Ring's summon notice, Deep Rock Galactic's join
// prompt). UJoinRequestCardPresenter shows it while a join request waits and listens to
// URunStateComponent::OnJoinRequestsChanged. Hold View (Tab) to answer: the card takes the keys; Up / Down light a
// seat, A gives it, X refuses, B lets go. What it shows comes from WoRJoinRequestCard (JoinRequestModel.h), which the
// specs read. Replaces the placeholder UJoinSeatWidget (still wor.UI.JoinSeat, removed after Crown's PIE pass).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Coop/JoinRequestModel.h"
#include "JoinRequestCardWidget.generated.h"

class APlayerController;
class UBorder;
class UInputAction;
class UInputMappingContext;
class URunStateComponent;
class UTextBlock;
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API UJoinRequestCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UJoinRequestCardWidget(const FObjectInitializer &ObjectInitializer);

	/** Rewrites the texts and seat rows (built once, never rebuilt); keeps the lit seat when it is still there. */
	void Refresh(const FJoinRequestCard &InCard, bool bInFocused);

	// What the keys do while the card has them (the specs call these too).
	void MoveSelection(int32 Delta);
	/** A: give the lit seat to the request. */
	void PressGive();
	/** X: refuse the request. */
	void PressRefuse();

	const FJoinRequestCard &GetCard() const { return Card; }
	int32 GetSelectedSeat() const { return SeatRow; }
	const FText &GetMessage() const { return Message; }

	/** B: the card lets go of the keys (the presenter's). */
	TFunction<void()> OnLetGo;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;

private:
	struct FSeatWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Status;
	};

	void RefreshSelection();
	void ShowMessage(const FText &InMessage);

	FJoinRequestCard Card;
	bool bFocused = false;
	int32 SeatRow = 0;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WithText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReplaceText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoteText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	TArray<FSeatWidgets> SeatWidgets;
};

/** Shows the join request card on the host while a request waits; the hold-to-answer key comes and goes with it. */
UCLASS()
class WORLD_OF_REFRACTION_API UJoinRequestCardPresenter : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld &InWorld) override;
	virtual void Deinitialize() override;

	UJoinRequestCardWidget *GetCard() const { return Card.Get(); }
	bool IsFocused() const { return bFocused; }

	/** Shows, refreshes or hides the card for the run's join requests. */
	void Sync();
	/** Hold View (Tab): the card takes the keys (not in a fight: seats are fixed). */
	void Focus();
	/** B, or the last request answered: the keys go back to the game. */
	void LetGo();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Hide();
	/** Gives the keys back to play; false when the card didn't have them. */
	bool ReleaseKeys();
	void AddHoldKey(APlayerController *PC);
	void RemoveHoldKey();

	TWeakObjectPtr<UJoinRequestCardWidget> Card;
	TWeakObjectPtr<URunStateComponent> Run;
	TWeakObjectPtr<APlayerController> KeyOwner;
	/** The input component the hold is bound on (unbound from it, even if the controller makes a new one). */
	TWeakObjectPtr<class UEnhancedInputComponent> KeyInput;
	FDelegateHandle RequestsChangedHandle;
	uint32 HoldBindingHandle = 0;
	bool bFocused = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AnswerAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AnswerContext;
};
