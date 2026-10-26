// BossData.h
// B-193 (Enemies §5, D-098, Q-193): a boss's phases. Each phase is a list of conditions (all must
// hold: an HP point, or hidden ones the player discovers) and what changes once it is reached.
// Every change is optional and authored per boss. Phases are reached in order and stay reached.
// Mini-bosses with no authored phases use a preset picked by their style tag.
// Runtime: UBossPhaseComponent on the boss in the fight. Debug: PrintBossData, wor.Boss.Print.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/Definitions/ESpellElement.h"
#include "BossData.generated.h"

class UCharacterData;
class UEffectDefinition;
class USkillDataBase;

/** Pillar 8: what kind of challenge a boss is. Picks a mini-boss's preset phases; only a guide
 *  for a main boss. Append new values at the end. */
UENUM(BlueprintType)
enum class EBossStyle : uint8
{
	None,
	Planning,
	Timing,
	Both
};

/** Append new values at the end (saved in assets). */
UENUM(BlueprintType)
enum class EBossPhaseConditionKind : uint8
{
	/** The boss's HP at or below Value percent. */
	HPBelowPercent,
	/** The boss has taken at least Value turns this fight. */
	TurnCount,
	/** The boss's Broken Darkness absorption is at its maximum. */
	AbsorptionMaxed,
	/** The crystal on the boss's active weapon is broken. */
	CrystalBroken,
	/** The boss's status bar has broken at least Value times this fight. */
	StatusBreaks,
	/** TODO: counted once infusion hits report per target; never met for now. */
	InfusionHits
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBossPhaseCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	EBossPhaseConditionKind Kind = EBossPhaseConditionKind::HPBelowPercent;

	/** The percent, turn count or break count the kind reads; ignored by the others. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0"))
	float Value = 50.0f;
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBossPhase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FName PhaseId;

	/** All must hold. Empty: never reached by itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	TArray<FBossPhaseCondition> Conditions;

	/** Put on the boss when the phase is reached (resistance / weakness overrides, stat changes);
	 *  they stay for the fight and no cleanse strips them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes")
	TArray<TObjectPtr<UEffectDefinition>> PhaseEffects;

	/** How many steps harder the boss's AI plays from this phase on (added to earlier phases'). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes", meta = (ClampMin = "0"))
	int32 AggressionSteps = 0;

	/** The weather's pull toward the boss once reached, 0-1 (0: leave it as it is). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeatherIntensity = 0.0f;

	/** TODO: played by the dialogue system (B-194); logged when reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes")
	FName DialogueId;

	/** TODO: needs a runtime element override on the combatant; logged when reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes")
	bool bChangeElement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes", meta = (EditCondition = "bChangeElement"))
	ESpellElement Element = ESpellElement::None;

	/** TODO: needs the enemy loadout to take skills mid-fight; logged when reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Changes")
	TArray<TObjectPtr<USkillDataBase>> AddedSkills;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UBossData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The character this boss data belongs to (the one in the fight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UCharacterData> Boss = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	EBossStyle Style = EBossStyle::None;

	/** In order. Empty: the preset for Style (a mini-boss). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TArray<FBossPhase> Phases;

	/** The authored phases, else the style's preset. */
	TArray<FBossPhase> GetPhasesToUse() const;

	/** Placeholder presets per style (pillar 8). TODO: tune. */
	static TArray<FBossPhase> GetPresetPhases(EBossStyle Style);

	/** The phase a memory adds (Q-191 AddPhase): a last stand near the end. TODO: tune. */
	static FBossPhase GetMemoryAddedPhase();

	/** The boss data authored for Character (asset registry), or null. */
	static UBossData *FindFor(const UCharacterData *Character);

	FString GetBossDataString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintBossData() const;
};
