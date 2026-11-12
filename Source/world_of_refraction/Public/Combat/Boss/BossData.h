// BossData.h
// B-193 (Enemies §5, D-098, Q-193): a boss's phases. Each phase is a list of conditions (all must
// hold: an HP point, or hidden ones the player discovers) and what changes once it is reached.
// Every change is optional and authored per boss. Phases are reached in order and stay reached.
// Mini-bosses with no authored phases use a preset picked by their style tag.
// Runtime: UBossPhaseComponent on the boss in the fight. Debug: PrintBossData, wor.Boss.Print.
// B-211 (D-100 part 1): Triggers, the boss's own "if these conditions, then this" (BossTriggers).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Reaction/FightMoment.h"
#include "Combat/Boss/BossRules.h"
#include "Skills/Definitions/ESpellElement.h"
#include "BossData.generated.h"

class UCharacterData;
class UEffectDefinition;
class UEquipmentDataBase;
class ULevelSequence;
class UMemoryData;
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

/** OBSOLETE (B-355, D-144): phases change through the rule list's EWoRConditionSubject boss subjects;
 *  read across by BossRules::MakePhaseRule. Kept: assets store it.
 *  Append new values at the end (saved in assets). */
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

/** OBSOLETE (B-355): an FWoRCondition in a FBossRule; read across on load. */
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

	/** OBSOLETE (B-355): when a phase is reached is a "next phase" rule now (UBossData::Rules); these
	 *  map across on read (BossRules::MakePhaseRule). All must hold. Empty: never reached by itself. */
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

	/** B-406 (FeelGuide 9, on trial): a HUD-free cinematic played when this phase is reached (bindings: "Boss", "Party").
	 *  Empty: none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic")
	TSoftObjectPtr<ULevelSequence> CinematicSequence;
};

/** OBSOLETE (B-355): FWoRCondition (CharacterInParty / GearEquipped / Hidden); read across on load.
 *  B-211 (D-100 part 1, Q-211): what must be true in the fight for a boss trigger. Append new
 *  values at the end (saved in assets). */
UENUM(BlueprintType)
enum class EBossTriggerConditionKind : uint8
{
	/** Character is in the party. */
	PartyMember,
	/** Item is worn (by Character when set, else by anyone) at least UpgradeLevel tiers above its
	 *  authored tier. */
	GearWorn,
	/** TODO: hidden conditions (pillar 10) are not built; never met for now. */
	Hidden
};

/** OBSOLETE (B-355): an FWoRCondition in a FBossRule; read across on load. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBossTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	EBossTriggerConditionKind Kind = EBossTriggerConditionKind::PartyMember;

	/** PartyMember: who. GearWorn: who must wear it (empty: anyone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UCharacterData> Character = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UEquipmentDataBase> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (ClampMin = "0"))
	int32 UpgradeLevel = 0;
};

/** OBSOLETE (B-355): EBossRuleResult. What a fired boss trigger does. Append new values at the end (saved in assets). */
UENUM(BlueprintType)
enum class EBossTriggerResult : uint8
{
	Weaken,
	Strengthen,
	SkipPhase,
	AddPhase,
	/** Plays the trigger's DialogueId (logged until the dialogue system, B-194). */
	Dialogue
};

/** OBSOLETE (B-355): a FBossRule (BossRules::MakeTriggerRule reads it across).
 *  B-211: "if these conditions, then this", owned by the boss. Its conditions are checked at fight
 *  start; it fires then or at its When (B-212). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FBossTrigger
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	FName TriggerId;

	/** All must hold. Empty: never fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TArray<FBossTriggerCondition> Conditions;

	/** B-212 (Q-212): when it fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	FFightMoment When;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TArray<EBossTriggerResult> Results;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	FName DialogueId;
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UBossData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The character this boss data belongs to (the one in the fight). Searchable (B-263): FindFor matches it in
	 *  the asset registry without loading every boss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category = "Boss")
	TObjectPtr<UCharacterData> Boss = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	EBossStyle Style = EBossStyle::None;

	/** B-209 (D-100): the memory beating this boss or mini-boss unlocks; empty: none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UMemoryData> MemoryOnDefeat = nullptr;

	/** In order. Empty: the preset for Style (a mini-boss). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TArray<FBossPhase> Phases;

	/** OBSOLETE (B-355): author Rules; these map across on read.
	 *  B-211 (D-100 part 1): what changes this boss, from who is in the fight and what they wear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TArray<FBossTrigger> Triggers;

	/** B-355 (D-144): the boss's one rule list: when -> condition -> result ("next phase" is a result).
	 *  Phases above keep only what changes when reached. BossRules::GetRules adds the old phase
	 *  conditions and triggers after these. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TArray<FBossRule> Rules;

	/** B-406 (FeelGuide 9, on trial): a HUD-free cinematic before the first turn (bindings: "Boss", "Party"); turn 1
	 *  waits for it. Empty: none. Phase cinematics are on each phase (FBossPhase::CinematicSequence). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic")
	TSoftObjectPtr<ULevelSequence> OpeningSequence;

	/** The authored phases, else the style's preset. */
	TArray<FBossPhase> GetPhasesToUse() const;

	/** Placeholder presets per style (pillar 8). TODO: tune. */
	static TArray<FBossPhase> GetPresetPhases(EBossStyle Style);

	/** The phase a memory adds (Q-191 AddPhase): a last stand near the end. TODO: tune. */
	static FBossPhase GetMemoryAddedPhase();

	/** The boss data authored for Character (asset registry), or null. */
	static UBossData *FindFor(const UCharacterData *Character);

	FString GetBossDataString() const;

	/** Debug: one trigger in words. */
	static FString GetTriggerString(const FBossTrigger &Trigger);

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintBossData() const;
};
