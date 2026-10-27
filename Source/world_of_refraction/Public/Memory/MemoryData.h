// MemoryData.h
// B-190 (RunStructure §5, Q-190): a memory a fragment unlocks: its scene and lore (placeholder
// text for now), how deep it runs for each character, and how it changes a boss. No hint: the
// player learns what it does when it plays.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MemoryData.generated.h"

class UCharacterData;
class UEquipmentDataBase;
class UMemoryData;
class USkillDataBase;

/** How much a memory means to one character: the same fragment can be deep for one lord and
 *  minor for another. */
UENUM(BlueprintType)
enum class EMemoryDepth : uint8
{
	None,
	Minor,
	Deep
};

/** How a memory changes a boss fight. Applied by the reaction trigger hook (B-191) and boss
 *  phases (B-193); nothing reads it yet. */
UENUM(BlueprintType)
enum class EMemoryBossModifierKind : uint8
{
	Weaken,
	Strengthen,
	SkipPhase,
	AddPhase
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FMemoryBossModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TObjectPtr<UCharacterData> Boss = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	EMemoryBossModifierKind Kind = EMemoryBossModifierKind::Weaken;
};

/** B-209 (D-100 part 2, Q-209b): one thing that must be true for a memory to unlock. Append new
 *  values at the end (saved in assets). */
UENUM(BlueprintType)
enum class EMemoryConditionKind : uint8
{
	/** Character is in the party. */
	Character,
	/** Item is worn (by Character when set, else by anyone) at least UpgradeLevel tiers above its
	 *  authored tier. A gear upgrade counts the levelled item as worn. */
	GearEquipped,
	/** LocationId is where it happens: the trial's asset name, or the fragment / spot's id. */
	Location,
	/** Enemy was beaten in the fight just won. */
	EnemyBeaten,
	/** The fight was won with no party member below their HP at its start. */
	NoDamageTaken,
	/** PriorMemory is already in the journal (memories chain). */
	PriorMemory,
	/** TODO: hidden conditions (pillar 10) are not built; never met for now. */
	Hidden
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FMemoryCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	EMemoryConditionKind Kind = EMemoryConditionKind::Character;

	/** Character: who must be in the party. GearEquipped: who must wear it (empty: anyone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TObjectPtr<UCharacterData> Character = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TObjectPtr<UEquipmentDataBase> Item = nullptr;

	/** GearEquipped: tiers above the item's authored tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory", meta = (ClampMin = "0"))
	int32 UpgradeLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	FName LocationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TObjectPtr<UCharacterData> Enemy = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TObjectPtr<UMemoryData> PriorMemory = nullptr;
};

/** Q-209c (D-100 alternative routes): another way to open a memory; all its conditions needed. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FMemoryConditionSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	TArray<FMemoryCondition> Conditions;
};

/** Q-209c (D-100 rewards): different, not stronger. Append new values at the end. */
UENUM(BlueprintType)
enum class EMemoryRewardKind : uint8
{
	Item,
	Skill,
	Title,
	/** A penalty and a key at once. */
	Curse
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FMemoryReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	EMemoryRewardKind Kind = EMemoryRewardKind::Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TObjectPtr<UEquipmentDataBase> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TObjectPtr<USkillDataBase> Skill = nullptr;

	/** Title / Curse: its id (logged until titles and curses exist). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName RewardId;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UMemoryData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The key the journal, V's lines and conversations use (B-194). Empty: the asset's name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName MemoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText Title;

	/** Placeholder until the scene is written. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FText Text;

	/** Characters this memory means something to; anyone not listed: None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TMap<TObjectPtr<UCharacterData>, EMemoryDepth> CharacterDepth;

	/** Obsolete since B-211 (D-100: memories no longer change bosses; UBossData::Triggers do). Kept,
	 *  unread, until Crown's PIE pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TArray<FMemoryBossModifier> BossModifiers;

	/** B-209 (D-100 part 2): all must hold; never shown to the player. Empty: only a source that
	 *  names this memory unlocks it (a fragment, a boss's MemoryOnDefeat...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	TArray<FMemoryCondition> UnlockConditions;

	/** Q-209c: other routes; meeting UnlockConditions or any one of these opens the memory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	TArray<FMemoryConditionSet> AlternativeConditionSets;

	/** Q-209c: granted once, when the memory unlocks (logged until each grant exists). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	TArray<FMemoryReward> Rewards;

	/** UnlockConditions and each alternative set, empty ones left out. */
	TArray<TArray<FMemoryCondition>> GetConditionSets() const;

	/** Some set has conditions (else only a source naming it opens it). */
	bool HasConditions() const { return !GetConditionSets().IsEmpty(); }

	FName GetMemoryId() const;

	/** Debug: one condition in words. */
	static FString GetConditionString(const FMemoryCondition &Condition);

	/** Debug: one reward in words. */
	static FString GetRewardString(const FMemoryReward &Reward);

	EMemoryDepth GetDepthFor(const UCharacterData *Character) const;

	/** Debug: id, title, each character's depth and the boss modifiers. */
	FString GetMemoryString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintMemory() const;
};
