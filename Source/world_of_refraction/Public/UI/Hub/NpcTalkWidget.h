// NpcTalkWidget.h
// B-471c (D-178; UIScreens.md #20, mockup hub/hud/2_npc_talk.png): the talk box a vendor opens before its shop. The
// world stays visible; one box at the bottom holds the speaker's role and line, the choices stack on its right: Shop
// (opens the vendor's screen, UMerchantShopSubsystem::OpenForMerchant), Talk (V's lines by a short name, NEW on the
// one not heard yet; A hears it) and Leave. Y shows the log (the lines heard). B steps back, and on the choices leaves.
// A CommonUI screen on the root layout's menu layer, opened by AMerchantInteractable::Interact or wor.UI.NpcTalk; what
// it shows is WoRNpcTalk::Build's. Placeholder look (D-024) until the art pass.

#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/NpcTalkModel.h"
#include "UI/WoRActivatableWidget.h"
#include "NpcTalkWidget.generated.h"

class APawn;
class APlayerController;
class UBorder;
class UMemoryJournalSubsystem;
class UMerchantData;
class UStoryLinesData;
class UTextBlock;
class UVerticalBox;

/** Which list the box shows. */
enum class ENpcTalkView : uint8
{
	Choices,
	Topics,
	Log
};

UCLASS()
class WORLD_OF_REFRACTION_API UNpcTalkWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UNpcTalkWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the talk box for Merchant (null: V with no shop) onto PC's root layout; null without one. Instigator is
	 *  the pawn the shop opens for. */
	static UNpcTalkWidget *Open(APlayerController *PC, UMerchantData *Merchant, APawn *Instigator);

	/** Who the box is for (set by Open; the specs set it too). */
	void SetSource(UMerchantData *InMerchant, APawn *InInstigator);
	/** The story the topics come from (Open gathers every story asset; the specs pass their own). */
	void SetStory(const TArray<UStoryLinesData *> &InStory);

	// What the keys do (the specs call these too).
	void MoveSelection(int32 Delta);
	/** A: the lit choice (Shop, Talk, Leave), or hears the lit topic. */
	void PressA();
	/** Y: the log, again: back. */
	void PressLog();
	/** B, Esc: the log or the topics go back to the choices; on the choices, Leave. */
	void PressBack() { HandleBackAction(); }

	const FNpcTalkPage &GetPage() const { return Page; }
	ENpcTalkView GetView() const { return View; }
	int32 GetSelected() const { return Selected; }
	/** What the box says now: the greeting, the line just heard, or the log. */
	const FText &GetSpoken() const { return Spoken; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual bool NativeOnHandleBackAction() override;

private:
	void Refresh();
	UMemoryJournalSubsystem *GetJournal() const;
	int32 GetLineCount() const;
	FText GetLineText(int32 Line) const;
	void OpenShop();

	TWeakObjectPtr<UMerchantData> Merchant;
	TWeakObjectPtr<APawn> Instigator;
	FNpcTalkPage Page;
	ENpcTalkView View = ENpcTalkView::Choices;
	/** The view the log was opened from (Y again or B goes back to it). */
	ENpcTalkView ViewBeforeLog = ENpcTalkView::Choices;
	int32 Selected = 0;
	FText Spoken;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStoryLinesData>> Story;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpokenText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChoiceList;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ChoiceRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceLabels;
};
