// MemoryJournalWidget.h
// PLACEHOLDER UI (B-194, Q-194): the hub's memory journal (HubAndStory §4). Three pages: the
// journal (every memory, lore object and conversation played, to reread), V (hear V's next line)
// and conversations (play an unlocked talk line by line). Plain buttons built in C++, opened by
// AHubJournalStation or `wor.Journal.Open`. Replaced by the CommonUI journal screen (D-024).
//
// Built once in RebuildWidget; the page logic only writes Title / Body, so it runs unbuilt too.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MemoryJournalWidget.generated.h"

class UButton;
class UMemoryJournalSubsystem;
class UStoryLinesData;
class UTextBlock;
struct FStoryConversation;

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
		Conversation
	};
	EPage Page = EPage::Journal;
	/** By id: the unlocked list can grow while the journal is open. */
	FName SelectedConversation;
	int32 LineIndex = 0;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;
};
