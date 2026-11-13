// MemoryJournalWidget.h
// PLACEHOLDER UI (B-194, Q-194): the hub's memory journal (HubAndStory §4). Three pages: the
// journal (every memory, lore object and conversation played, to reread), V (hear V's next line)
// and conversations (play an unlocked talk line by line). B-388: a Quests page lists the player's main and side quests
// with their goal (an unstarted hidden quest never shows, D-100; a found one is masked until revealed, Q-397b). Plain
// buttons built in C++, opened by
// AHubJournalStation or `wor.Journal.Open`. Replaced by the CommonUI journal screen (D-024).
//
// Built once in RebuildWidget; the page logic only writes Title / Body, so it runs unbuilt too.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MemoryJournalWidget.generated.h"

class UButton;
class UMemoryJournalSubsystem;
class UMemoryJournalWidget;
class UStoryLinesData;
class UTextBlock;
class UVerticalBox;
struct FStoryConversation;

/** Q-397b, D-198: one Show / Hide button's quest (a button's click carries no data). */
UCLASS()
class UJournalRevealAction : public UObject
{
	GENERATED_BODY()

public:
	FName QuestId;
	TWeakObjectPtr<UMemoryJournalWidget> Journal;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class WORLD_OF_REFRACTION_API UMemoryJournalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add one to PC's viewport on the journal page (UI-only input, cursor on); the open one when
	 *  PC already has it open. */
	static UMemoryJournalWidget *Open(APlayerController *PC);

	/** The V lines and conversations it reads (Open gathers every story asset). */
	void SetStory(const TArray<UStoryLinesData *> &InStory);

	/** Every memory, lore entry and played conversation. */
	void ShowJournalPage();

	/** B-388: the started main and side quests, each with its current goal (or done). Q-397b: a found hidden quest is
	 *  masked as "Secret Quest <n>" (D-196; n in found order, oldest 1) with a Show / Hide button (D-198), so a shared
	 *  screen spoils nothing. */
	void ShowQuestsPage();

	/** D-198 (changes Q-397b's reveal): one toggle. Shows a masked secret quest's title and goal, or masks a shown one
	 *  again; a shown one stays shown until the journal closes. False when QuestId is not a secret quest on the Quests
	 *  page now. */
	bool ToggleQuestShown(FName QuestId);
	bool IsQuestRevealed(FName QuestId) const { return RevealedQuests.Contains(QuestId); }

	/** Q-397b: the secret quests masked on the Quests page now (each one's button reads "Show"). */
	TArray<FName> GetMaskedQuestIds() const;

	/** Closes the journal; every revealed hidden quest is masked again (Q-397b). */
	void Close();

	/** V says the next unlocked line not heard yet. False when V has nothing new. */
	bool HearNextVLine();

	/** Pick the next unlocked conversation and show its first line. False when none is unlocked. */
	bool SelectNextConversation();

	/** The selected conversation's next line; past the last, it is played (logged in the journal).
	 *  False when no conversation is selected or it already ended. */
	bool AdvanceConversation();

	const FText &GetTitle() const { return Title; }
	const FText &GetBody() const { return Body; }

	/** Debug: the page as shown. */
	FString GetJournalWidgetString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleJournalClicked();

	UFUNCTION()
	void HandleQuestsClicked();

	UFUNCTION()
	void HandleVClicked();

	UFUNCTION()
	void HandleConversationsClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UMemoryJournalSubsystem *GetJournal() const;
	void SetPage(const FText &NewTitle, const FText &NewBody);
	void ShowConversationLine();

	/** The selected conversation's place in Unlocked, or INDEX_NONE. */
	int32 FindSelectedConversation(const TArray<const FStoryConversation *> &Unlocked) const;

	UPROPERTY()
	TArray<TObjectPtr<UStoryLinesData>> Story;

	FText Title;
	FText Body;

	enum class EPage : uint8
	{
		Journal,
		V,
		Conversation,
		Quests
	};
	EPage Page = EPage::Journal;
	/** By id: the unlocked list can grow while the journal is open. */
	FName SelectedConversation;
	int32 LineIndex = 0;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	/** Q-397b: a secret quest on the Quests page, masked or shown. */
	struct FSecretQuestRow
	{
		FName QuestId;
		FText MaskedName;
		bool bShown = false;
	};
	TArray<FSecretQuestRow> SecretQuests;
	/** Shown while this journal is open; cleared on Close. */
	TSet<FName> RevealedQuests;

	/** One Show / Hide button per secret quest (D-198), under the page text (built only when the widget is). */
	void RebuildRevealButtons();

	UPROPERTY()
	TObjectPtr<UVerticalBox> RevealBox;

	UPROPERTY()
	TArray<TObjectPtr<UJournalRevealAction>> RevealActions;
};
