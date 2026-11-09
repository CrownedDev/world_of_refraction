// HiddenQuestData.h
// B-377 (MemoryCatalogue §3.6, Crown 2026-11-08; SLF's Unique Scenarios): a hidden quest. It starts when the player
// gets one of its start memories at the depth set for it (any memory of any kind; a reaction starts one through its
// memory), then its steps are done in order, each one a condition set met at memory events (TimesInOneRun times in
// one run), and it ends with EndMemory (its rewards are the quest's). No text tells the player what comes next (D-100).
// Progress lives in UHiddenQuestSubsystem. Debug: PrintQuest in the Details panel; wor.Quest.Print.
// B-388 (MemoryCatalogue §3.12 / §3.13, Crown 2026-11-08): the same data is a main or side quest when Kind says so:
// visible, in the journal with its Title and the current step's Goal shown (hidden ones stay unhinted, D-100).

#pragma once

#include "CoreMinimal.h"
#include "Conditions/WoRCondition.h"
#include "Engine/DataAsset.h"
#include "Memory/MemoryData.h"
#include "HiddenQuestData.generated.h"

/** B-388: what kind of quest a quest asset is. Append only (assets store it). */
UENUM(BlueprintType)
enum class EQuestKind : uint8
{
	/** No hints (D-100, MemoryCatalogue §3.6). */
	Hidden = 0,
	/** The main story's, visible and tracked (§3.12). */
	Main = 1,
	/** Optional, visible and tracked (§3.13). */
	Side = 2
};

/** One way in: Memory, got at Depth (None: at any depth, and a memory for every character). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FHiddenQuestStart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UMemoryData> Memory = nullptr;

	/** Set per depth (Crown): a memory can start this quest for the character it is Deep for and not a Minor one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EMemoryDepth Depth = EMemoryDepth::None;
};

/** One step: Conditions met at TimesInOneRun memory events within one run (the count resets at run end). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FHiddenQuestStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FWoRConditionSet Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = 1))
	int32 TimesInOneRun = 1;

	/** B-388: a main or side quest's goal while this step is in progress (shown in the journal). Unused for a hidden
	 *  quest. TODO(Crown): content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Goal;

	/** Q-378 (Crown, 2026-11-08): given when this step is done. Curse / RemoveCurse are granted (UCurseSubsystem); a cleared
	 *  curse from here comes back when this step's conditions are met again. Other kinds are logged until they exist. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FMemoryReward> Rewards;

	/** For designers only; never shown (no hints, D-100). */
	UPROPERTY(EditAnywhere, Category = "Quest")
	FString DesignerNote;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UHiddenQuestData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Saved progress keys by this; empty: the asset's name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	/** B-388: Hidden (the default, no hints), or a visible Main / Side quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestKind Kind = EQuestKind::Hidden;

	/** B-388: a main or side quest's name in the journal and its notices. TODO(Crown): content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FHiddenQuestStart> Starts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FHiddenQuestStep> Steps;

	/** Unlocked when the last step is done; its rewards are the quest's (MemoryCatalogue §4). TODO(Crown): content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UMemoryData> EndMemory = nullptr;

	FName GetQuestId() const;

	/** B-388: a main or side quest (shown in the journal). */
	bool IsVisible() const { return Kind != EQuestKind::Hidden; }

	/** B-388: the shown name (Title, else the quest id). */
	FText GetDisplayTitle() const;

	/** B-388: the goal of the step in progress after StepsDone steps (empty when finished or past the last). */
	FText GetGoalAfter(int32 StepsDone) const;

	/** Memory, got at Depth for the player, is one of Starts. */
	bool StartsFrom(const UMemoryData *Memory, EMemoryDepth Depth) const;

	/** Every hidden quest asset (the asset registry, sorted by path so checks run in a fixed order). */
	static TArray<UHiddenQuestData *> GatherAll();

	/** B-377 review: false while the asset registry is still scanning (GatherAll may miss quests then). */
	static bool IsCatalogueReady();

	/** Debug: id, starts, each step's conditions and times, the end memory. */
	FString GetQuestString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintQuest() const;
};
