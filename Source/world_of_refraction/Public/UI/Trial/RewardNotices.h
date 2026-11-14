// RewardNotices.h
// B-465b (UIScreens.md #25, mockup run/run_end/1_reward_popups.png; FeelGuide 6): a win shows no results screen; what
// it gave comes back as popups on the right of the field, one per thing, coloured by kind (run Prisms grey, a banked
// level cyan, a memory gold, a quest in the menu colour, a secret quest violet and marked "only you see this", D-171).
// None needs a press; they fade on their own (URewardPopupsWidget draws them).
//
// The notices are posted where the thing happens (the battle, the journal, the quest system) and wait in
// URewardNoticeSubsystem until the field's HUD shows them, so a fight's rewards show back on the trial map after the
// level change. It is a game-instance queue because it has to outlive that level change; it holds only the
// not-yet-shown popups, no fight state (D-064).
// Debug: wor.UI.RewardNotice <kind 0-4> posts a sample; wor.UI.PrintRewardNotices logs the waiting ones.

#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryJournalSubsystem.h" // EMemorySource, for the UFUNCTION handler (UHT)
#include "Subsystems/GameInstanceSubsystem.h"
#include "RewardNotices.generated.h"

class UMemoryData;

/** B-465b: what a popup is about; it picks the colour and the order a batch shows in. */
enum class ERewardNoticeKind : uint8
{
	/** The win and the run Prisms it paid. */
	Win,
	/** Pillar levels banked, to place at a rest spot. */
	Level,
	/** A memory recorded in the journal. */
	Memory,
	/** A main or side quest found, moved on or finished. */
	Quest,
	/** A secret (hidden) quest: masked words (Q-397b), "only you see this" (D-171). */
	SecretQuest
};

struct FRewardNotice
{
	ERewardNoticeKind Kind = ERewardNoticeKind::Win;
	/** "Victory", "New memory". */
	FText Head;
	/** "+180 Prisms", the memory's title. */
	FText Body;
	/** Marked "only you see this" (a secret quest). */
	bool bOnlyYou = false;
};

namespace WoRRewardNotice
{
	/** How long a popup stays, and how much of that it spends fading. TODO: tune in PIE */
	inline constexpr float SHOWN_SECONDS = 5.0f;
	inline constexpr float FADE_SECONDS = 1.0f;
	/** At most this many on screen; the rest wait their turn. */
	inline constexpr int32 MAX_SHOWN = 5;
	/** At most this many wait; past it the oldest goes (a long chain of notices with no field to show them). */
	inline constexpr int32 MAX_WAITING = 16;

	/** The win: "Victory", "+180 Prisms" (no Prisms line outside a run). */
	WORLD_OF_REFRACTION_API FRewardNotice MakeWin(int32 RunPrisms);
	/** "Level banked", "+2 pillar levels · place them at a rest spot". */
	WORLD_OF_REFRACTION_API FRewardNotice MakeLevels(int32 Levels);
	/** "New memory", the memory's title. */
	WORLD_OF_REFRACTION_API FRewardNotice MakeMemory(const FText &Title);
	/** A quest notice as the quest system words it; bSecret: the masked words of a secret quest, only you see it. */
	WORLD_OF_REFRACTION_API FRewardNotice MakeQuest(const FText &Notice, bool bSecret);

	/** The popup's colour: its edge bar and head. */
	WORLD_OF_REFRACTION_API FLinearColor GetColor(ERewardNoticeKind Kind);

	/** A batch in show order: the win first, then levels, memories, quests (a stable sort: same kinds keep theirs). */
	WORLD_OF_REFRACTION_API void SortForShow(TArray<FRewardNotice> &Notices);

	/** Posts Notice to WorldContext's game instance queue (nothing without one). */
	WORLD_OF_REFRACTION_API void Post(const UObject *WorldContext, const FRewardNotice &Notice);

	WORLD_OF_REFRACTION_API FString GetNoticeString(const FRewardNotice &Notice);
}

/** B-465b: the popups waiting for the field's HUD (see the file comment). */
UCLASS()
class WORLD_OF_REFRACTION_API URewardNoticeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Listens to the journal: every memory recorded on this machine posts its popup (a quest's end memory is the
	 *  quest system's own popup, masked when secret). */
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;

	void Post(const FRewardNotice &Notice);

	/** Hands every waiting notice over, in show order, and empties the queue. */
	TArray<FRewardNotice> TakeWaiting();

	const TArray<FRewardNotice> &GetWaiting() const { return Waiting; }

	/** Fires after each Post (the popups widget takes them at once when it is up). */
	FSimpleMulticastDelegate OnPosted;

	FString GetWaitingString() const;

private:
	UFUNCTION()
	void HandleMemoryRecorded(UMemoryData *Memory, EMemorySource Source);

	TArray<FRewardNotice> Waiting;
};
