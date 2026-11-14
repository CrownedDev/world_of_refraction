// RecruitCardWidget.h
// B-471b (D-179; UIScreens.md #29, mockup hub/hud/3_recruit_card.png): the recruit card. A character met in a trial
// offers to join for this run: the card shows who they are, the rule line and who plays them; with room the one choice
// is "Invite for this run", with a full party one line per NPC who can step out. A invites, B closes. A refusal (a full
// party of players) shows in amber. A CommonUI screen on the root layout's menu layer, opened by the recruit's Interact
// in a run (UPartyInteractionComponent) or wor.UI.RecruitCard; what it shows is WoRRecruitCard::Build's.

#pragma once

#include "CoreMinimal.h"
#include "Party/RecruitCard.h"
#include "UI/WoRActivatableWidget.h"
#include "RecruitCardWidget.generated.h"

class APlayerController;
class UBorder;
class UPartyInteractionComponent;
class UTextBlock;
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API URecruitCardWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	URecruitCardWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the card for Recruit onto PC's root layout; null without one. */
	static URecruitCardWidget *Open(APlayerController *PC, UPartyInteractionComponent *Recruit);

	/** The recruit the card is for (set by Open; the specs set it too). */
	void SetRecruit(UPartyInteractionComponent *Recruit);

	/** The choices: "Invite for this run", or one per NPC who can step out. Empty when the offer is refused. */
	int32 GetChoiceCount() const;
	int32 GetSelectedChoice() const { return SelectedChoice; }
	void MoveSelection(int32 Delta);
	/** A: the lit choice; the screen closes when the recruit joins, a refusal stays on screen. */
	void PressInvite();

	const FRecruitCard &GetCard() const { return Card; }
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;

private:
	void Refresh();
	/** The lit choice's text: "Invite for this run" or "Invite: <name> steps out". */
	FText GetChoiceText(int32 Choice) const;

	TWeakObjectPtr<UPartyInteractionComponent> Recruit;
	FRecruitCard Card;
	int32 SelectedChoice = 0;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClassText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RuleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlayedByText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ChoiceRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceLabels;
};
