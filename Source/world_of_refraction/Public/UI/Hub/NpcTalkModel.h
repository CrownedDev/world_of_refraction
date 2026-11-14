// NpcTalkModel.h
// B-471c (D-178; UIScreens.md #20, mockup hub/hud/2_npc_talk.png): what the talk box shows, with no widget in it, so the
// specs read it headless. Every vendor is V in an outfit (HubAndStory §3), so the box is V's: the speaker's role (the
// vendor's, "Blacksmith"), a line, and three short choices, Shop / Talk / Leave. Under Talk sit V's lines by a short
// name: those already heard (to hear again) and the next one unlocked, marked NEW (StoryUnlocks::FindNextVLine). Y
// shows the log: the lines heard, in order. UNpcTalkWidget draws it. Q-471c.
// B-471d (Q-471d): a character in the hub (AHubCharacterSpot) opens the same box with its name: Talk lists the
// conversations it takes part in that are unlocked (StoryUnlocks::GetUnlockedConversations), NEW until played; playing
// one marks it played in the journal (it moves to Played there).
// Debug: wor.Talk.Print (NpcTalkDebug).

#pragma once

#include "CoreMinimal.h"
#include "Merchant/MerchantData.h"

class UCharacterData;
class UMemoryJournalSubsystem;
class UStoryLinesData;
struct FVLine;

enum class ENpcTalkChoice : uint8
{
	Shop,
	Talk,
	Leave
};

/** One topic under Talk: one of V's lines. */
struct FNpcTalkTopic
{
	FName LineId;
	/** The short name ("Trials"): the line's TopicName, else "Topic n". */
	FText Name;
	/** Not heard yet. */
	bool bNew = false;
	FText Line;
	/** B-471d: a conversation (LineId is its ConversationId; Line all its lines), not one of V's lines. */
	bool bConversation = false;
};

struct FNpcTalkPage
{
	/** The speaker's role (D-178): the vendor's ("Blacksmith"). */
	FText Role;
	/** What V says when the box opens. */
	FText Greeting;
	TArray<ENpcTalkChoice> Choices;
	TArray<FNpcTalkTopic> Topics;
	/** A topic is NEW: Talk shows the mark too. */
	bool bTalkHasNew = false;
	/** Y: the lines heard, in the order V says them. */
	TArray<FText> Log;
};

namespace WoRNpcTalk
{
	/** The talk box for Merchant (null: V without a shop, no Shop choice). Journal null: no topics, no log. */
	WORLD_OF_REFRACTION_API FNpcTalkPage Build(const UMerchantData *Merchant, const TArray<UStoryLinesData *> &Story, const UMemoryJournalSubsystem *Journal);

	/** B-471d: the talk box for a character in the hub: its name, Talk / Leave, and under Talk the unlocked
	 *  conversations it speaks in (played ones to hear again, the rest NEW); the log holds the played ones. */
	WORLD_OF_REFRACTION_API FNpcTalkPage BuildForCharacter(const UCharacterData *Character, const TArray<UStoryLinesData *> &Story,
														   const UMemoryJournalSubsystem *Journal);

	/** The topic's line is heard: kept in the journal (it saves), so it is no longer NEW and the next one unlocks.
	 *  B-471d: a conversation is marked played. */
	WORLD_OF_REFRACTION_API void Hear(UMemoryJournalSubsystem &Journal, const FNpcTalkTopic &Topic);

	/** The choice's word ("Shop", "Talk", "Leave"). */
	WORLD_OF_REFRACTION_API FText GetChoiceName(ENpcTalkChoice Choice);

	/** Debug: the page as text. */
	WORLD_OF_REFRACTION_API FString GetTalkString(const FNpcTalkPage &Page);
}
