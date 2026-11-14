// JournalScreenModel.h
// B-463 (D-181, D-196, D-198, D-199, UIScreens #6, mockups screen_mockups/menus/journal/): what the journal screen
// shows and what its keys do, with no widget in it, so the specs read it headless. UJournalScreenWidget draws it.
//
// Pages: Quests, Memories, Lore, Conversations. Each is a list on the left (with a sub-tab row on some) and a
// reader on the right.
// Quests: every started quest with the steps reached and no later one (D-199); a secret quest is masked as
// "Secret Quest <n>" until shown, shows its current step only (D-171), and is masked again when the journal
// closes (Q-397b).
// Conversations: a ready talk is played here line by line until NPC talk is built (Q-463, B-471).
// Debug: wor.UI.PrintJournal logs GetJournalString.

#pragma once

#include "CoreMinimal.h"
#include "Memory/HiddenQuestData.h" // EQuestKind

class UGameInstance;
class UStoryLinesData;

/** The pages, in tab order. */
enum class EJournalPage : uint8
{
	Quests,
	Memories,
	Lore,
	Conversations
};

/** The Quests page's sub-tabs, in order. */
enum class EJournalQuestFilter : uint8
{
	All,
	Main,
	Side,
	Secret
};

/** The Conversations page's sub-tabs, in order. */
enum class EJournalTalkFilter : uint8
{
	Ready,
	Played
};

/** What the open journal remembers until it closes. */
struct FJournalState
{
	/** The secret quests shown now (Q-397b: all masked again when the journal closes). */
	TSet<FName> ShownSecrets;
	/** The conversation being played, and how many of its lines are out. */
	FName TalkId;
	int32 TalkLines = 0;
};

/** One step of a quest in the reader: a done one, or the one in progress. */
struct FJournalStep
{
	FText Text;
	bool bDone = false;
};

/** One row of a page's list, with what the reader shows for it. */
struct FJournalRow
{
	FName Id;
	FText Title;
	FText Subtitle;
	/** The small word on the row's right: the quest's kind, or "NEW". */
	FText Tag;
	/** B-463c: a memory or lore entry not opened yet (Tag is "NEW"). */
	bool bNew = false;

	bool bQuest = false;
	EQuestKind Kind = EQuestKind::Main;
	/** A finished quest: the row is dimmed. */
	bool bDone = false;

	/** The reader: the line under the title, the steps (quests), the text, and a closing note. */
	FText Kicker;
	TArray<FJournalStep> Steps;
	FText Body;
	FText Note;

	/** What Select does on this row ("Play", "Next"); empty when it does nothing. */
	FText SelectVerb;
	/** What X does on this row ("Show" / "Hide", D-198); empty for a row that isn't a secret quest. */
	FText ToggleVerb;

	bool IsSecret() const { return !ToggleVerb.IsEmpty(); }
};

/** A page as shown: its sub-tabs, its rows, and the line under the list. */
struct FJournalView
{
	/** Empty for a page with no sub-tabs. */
	TArray<FText> Filters;
	TArray<FJournalRow> Rows;
	FText ListNote;
};

namespace WoRJournalScreen
{
	inline constexpr int32 PAGE_COUNT = 4;

	WORLD_OF_REFRACTION_API FText GetPageName(EJournalPage Page);

	/** The page under its sub-tab Filter (clamped). Story: the story assets conversations come from
	 *  (StoryUnlocks::GatherStoryData). */
	WORLD_OF_REFRACTION_API FJournalView BuildView(UGameInstance *GameInstance, EJournalPage Page, int32 Filter, const FJournalState &State,
												   const TArray<UStoryLinesData *> &Story);

	/** X (D-198): shows a masked secret quest, or masks a shown one. False when Row isn't a secret quest. */
	WORLD_OF_REFRACTION_API bool ToggleSecret(const FJournalRow &Row, FJournalState &State);

	/** Y (D-198): masks every shown secret quest. False when none was shown. */
	WORLD_OF_REFRACTION_API bool HideAllSecrets(FJournalState &State);

	/** Select on a ready conversation: its next line comes out; with the last one it is played (kept to re-read).
	 *  False when Row isn't a ready conversation or it has ended. */
	WORLD_OF_REFRACTION_API bool PlayNextLine(UGameInstance *GameInstance, const FJournalRow &Row, FJournalState &State,
											  const TArray<UStoryLinesData *> &Story);

	/** B-463b: A on a quest row tracks it (one at a time; the tracked one stops being tracked). False when Row can't be
	 *  tracked (a finished or secret quest, or not a quest). */
	WORLD_OF_REFRACTION_API bool ToggleTrack(UGameInstance *GameInstance, const FJournalRow &Row);

	/** B-463c: the journal showed Row in the reader: a new memory or lore entry stops being new (saved). False when
	 *  Row wasn't new. The row keeps its NEW in the view built before, so it shows while it is open. */
	WORLD_OF_REFRACTION_API bool MarkOpened(UGameInstance *GameInstance, const FJournalRow &Row);

	/** Debug: every page as the journal opens it (secret quests masked). */
	WORLD_OF_REFRACTION_API FString GetJournalString(UGameInstance *GameInstance, const TArray<UStoryLinesData *> &Story);
}
