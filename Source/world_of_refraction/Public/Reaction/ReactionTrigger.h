// ReactionTrigger.h
// B-191 (RunStructure §5, D-032, Q-191): one structure for every reaction: if a party member wears
// all of these items while facing this character, trigger effects, a quest, a dialogue. Checked
// at encounter start (ABattleGameMode). Effects are ordinary FSkillEffects (the effects condition
// model, B-044 / D-031): a payload aimed at Enemy lands on the faced character, any other on the
// wearer. Boss changes come from each boss's own triggers since B-211 (BossTriggers).
// Debug: wor.Reaction.Print.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Effects/FSkillEffect.h"
#include "ReactionTrigger.generated.h"

class UCharacterData;
class UEquipmentDataBase;
class UMemoryData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FReactionTrigger
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FName TriggerId;

	/** All worn by the same party member (a weapon or ring slot). Empty: never fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TArray<TObjectPtr<UEquipmentDataBase>> RequiredItems;

	/** In the opposing team. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TObjectPtr<UCharacterData> FacingCharacter = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TArray<FSkillEffect> Effects;

	/** TODO: started by the quest system (not built). Logged when it fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FName QuestId;

	/** TODO: played by the dialogue system (B-194). Logged when it fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FName DialogueId;

	/** B-209 (D-100): a memory this reaction unlocks when it fires; empty: none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TObjectPtr<UMemoryData> UnlockMemory = nullptr;
};

/** A set of reactions, found by the asset registry (any number of these assets). */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UReactionTriggerData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TArray<FReactionTrigger> Triggers;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintTriggers() const;
};

/** One reaction that fired this fight. Points into the trigger list FindFired was given: use it
 *  before that list changes or goes away. */
struct FFiredReaction
{
	const FReactionTrigger *Trigger = nullptr;
	/** Index in the gathered list; keeps its effects' ids apart from other reactions'. */
	int32 TriggerIndex = INDEX_NONE;
	AActor *Wearer = nullptr;
	AActor *Faced = nullptr;
};

namespace ReactionTriggers
{
	/** Every authored reaction (all UReactionTriggerData assets, sorted by path). */
	TArray<FReactionTrigger> GatherTriggers();

	/** The gear Actor wears now: its weapons and rings. */
	TArray<const UEquipmentDataBase *> GetWornItems(const AActor *Actor);

	/** True when Wearer wears every RequiredItem and Faced is the FacingCharacter. */
	bool Matches(const FReactionTrigger &Trigger, const AActor *Wearer, const AActor *Faced);

	/** Each trigger fires at most once per fight: the first party member and opposing combatant
	 *  that match. */
	TArray<FFiredReaction> FindFired(const TArray<FReactionTrigger> &Triggers, const TArray<AActor *> &Party,
									 const TArray<AActor *> &Opposing);

	/** Put each fired reaction's effects on its wearer or faced character, log its quest and
	 *  dialogue. Returns the number of effects applied. */
	int32 ApplyFired(const TArray<FFiredReaction> &Fired);

	/** Memory boss changes for this fight: the run memories naming one of the Opposing combatants'
	 *  characters add up (weaken -1, strengthen +1 step, capped) to one net change on it; phase
	 *  changes wait for B-193. Returns the number of combatants changed. Obsolete since B-211
	 *  (D-100: memories no longer change bosses; BossTriggers do): no game caller; remove after
	 *  Crown's PIE pass. */
	int32 ApplyMemoryBossModifiers(const TArray<TObjectPtr<UMemoryData>> &RunMemories, const TArray<AActor *> &Opposing);

	/** NetSteps (weaken -1, strengthen +1 each; capped) on Boss's max HP and damage, before the
	 *  fight: it starts at its new full HP. False when nothing changed. Shared by memory boss
	 *  changes and boss triggers (B-211). Call once per boss per fight: a second call refreshes the
	 *  same effects instead of adding to them. */
	bool ApplyBossSteps(AActor *Boss, int32 NetSteps);

	FString GetTriggerString(const FReactionTrigger &Trigger);
}
