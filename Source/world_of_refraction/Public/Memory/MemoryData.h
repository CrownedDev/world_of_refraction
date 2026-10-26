// MemoryData.h
// B-190 (RunStructure §5, Q-190): a memory a fragment unlocks: its scene and lore (placeholder
// text for now), how deep it runs for each character, and how it changes a boss. No hint: the
// player learns what it does when it plays.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MemoryData.generated.h"

class UCharacterData;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory")
	TArray<FMemoryBossModifier> BossModifiers;

	FName GetMemoryId() const;

	EMemoryDepth GetDepthFor(const UCharacterData *Character) const;

	/** Debug: id, title, each character's depth and the boss modifiers. */
	FString GetMemoryString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintMemory() const;
};
