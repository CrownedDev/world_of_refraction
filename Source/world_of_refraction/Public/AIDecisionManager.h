// AIDecisionManager.h
// AI decision making for combat turns

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EAIDifficulty.h"
#include "ActionStructs.h"
#include "EDefenseType.h"
#include "ESkillEffectType.h"
#include "FCrystalId.h"
#include "AIDecisionManager.generated.h"

class ACombatOrchestrator;
class UCharacterDataComponent;
class ULoadoutComponent;
class USkillEffectManager;
class UDamageCalculator;
class UActionExecutor;
class UDefenseSystem;
class UCharacterData;

/**
 * Handles AI decision making during combat
 * Routes turn decisions through standard action pipeline
 */
UCLASS()
class WORLD_OF_REFRACTION_API UAIDecisionManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    // ==================== COMBAT REGISTRATION ====================

    /** Set the active combat orchestrator */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetCombatOrchestrator(ACombatOrchestrator *Orchestrator);

    /** Clear combat reference */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void ClearCombatOrchestrator();

    // ==================== DECISION MAKING ====================

    /**
     * Request AI decision for an actor's turn
     * Applies thinking delay based on difficulty, then submits action
     */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void RequestDecision(AActor *AIActor);

    // ==================== DEFENSE DECISIONS ====================

    /**
     * Schedule defense decision for AI defender
     * Called by DefenseSystem when window opens
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Defense")
    void ScheduleDefenseDecision(AActor *Defender, float AttackSize, int32 BaseDamage, float WindowDuration);

private:
    // ==================== INTERNAL ====================

    USkillEffectManager *GetSkillEffectManager() const;

    /** Get ActionExecutor subsystem */
    UActionExecutor *GetActionExecutor() const;

    /** Active combat reference */
    UPROPERTY()
    ACombatOrchestrator *CurrentCombat = nullptr;

    /** Timer for thinking delay */
    FTimerHandle ThinkingTimerHandle;

    /** Actor waiting for decision */
    UPROPERTY()
    AActor *PendingActor = nullptr;

    // ==================== DECISION LOGIC ====================

    /** Called after thinking delay - makes and submits decision */
    void ExecuteDecision();

    /** Build action for AI actor */
    FAction BuildAction(AActor *AIActor);

    /** Pick a random action type based on available options */
    EActionType ChooseActionType(AActor *AIActor, ULoadoutComponent *Loadout);

    /** Get thinking delay range for difficulty */
    void GetThinkingDelayRange(EAIDifficulty Difficulty, float &OutMin, float &OutMax) const;

    /** Calculate random thinking delay */
    float CalculateThinkingDelay(EAIDifficulty Difficulty) const;

    // ==================== DEFENSE DECISIONS ====================

    /** Defense system reference */
    UPROPERTY()
    UDefenseSystem *DefenseSystemRef = nullptr;

    /** Active defense timers per actor */
    TMap<AActor *, FTimerHandle> DefenseTimerHandles;

    // ==================== DEFENSE LOGIC ====================

    /** Choose defense type based on attack, incoming damage and difficulty */
    EDefenseType ChooseDefenseType(AActor *Defender, float AttackSize, int32 BaseDamage, EAIDifficulty Difficulty) const;

    /** Get defense attempt chance for difficulty */
    float GetDefenseAttemptChance(EAIDifficulty Difficulty) const;

    /** Get defense timing accuracy for difficulty */
    float GetDefenseAccuracy(EAIDifficulty Difficulty) const;

    /** Calculate reaction delay for defense */
    float CalculateDefenseReactionDelay(EAIDifficulty Difficulty, float WindowDuration) const;

    // ==================== QUERY ====================

    /** Get current difficulty from combat */
    UFUNCTION(BlueprintPure, Category = "AI")
    EAIDifficulty GetCurrentDifficulty() const;

    // ==================== TARGET SCORING ====================

    /** Score a potential target */
    int32 ScoreTarget(AActor *Attacker, AActor *Target, int32 EstimatedDamage);

    /** Get best target from enemy list */
    AActor *SelectBestTarget(AActor *Attacker, const TArray<AActor *> &Enemies);

    /** Estimate damage to target with best available action */
    int32 EstimateBestDamage(AActor *Attacker, AActor *Target);

    /** Execution-accurate spell damage estimate — routes the spell through
     *  DamageCalculator so attacker stats, ActionMods and defender defense
     *  are all applied, matching what real execution would deal. */
    int32 EstimateSpellDamage(AActor *Attacker, AActor *Target, USpellData *Spell, int32 InfusionLevel = 0) const;

    /** Execution-accurate ability damage estimate — see EstimateSpellDamage. */
    int32 EstimateAbilityDamage(AActor *Attacker, AActor *Target, UAbilityData *Ability, int32 InfusionLevel = 0) const;

    /** Score the value of a spell's status-buildup payload against a target
     *  (pre-weight, ~0..50). Non-const — reads HasDangerousDebuff. */
    float EstimateStatusScore(AActor *Attacker, AActor *Target, USpellData *Spell);

    /** Ability equivalent of EstimateStatusScore — see the spell overload. */
    float EstimateStatusScore(AActor *Attacker, AActor *Target, UAbilityData *Ability);

    /** True if the actor can afford the spell at the given infusion level. */
    bool CanAffordSpell(AActor *Actor, USpellData *Spell, int32 InfusionLevel = 0) const;

    /** True if the actor can afford the ability at the given infusion level. */
    bool CanAffordAbility(AActor *Actor, UAbilityData *Ability, int32 InfusionLevel = 0) const;

    /** Calculate threat level of an actor */
    int32 CalculateThreatLevel(AActor *Actor);

    /** Check if we can kill target this turn */
    bool CanKillTarget(AActor *Attacker, AActor *Target, int32 Damage);

    /** Get HP percentage (0.0 - 1.0) */
    float GetHPPercent(AActor *Actor);

    /** Get current HP */
    int32 GetCurrentHP(AActor *Actor);

    /** Get max HP */
    int32 GetMaxHP(AActor *Actor);

    // ==================== DECISION BRANCHES ====================

    /** Main smart decision builder (replaces random BuildAction for Medium+) */
    FAction BuildAction_Smart(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp);

    /** Branch 1: Survival - heal or defend if low HP */
    bool TrySurvivalBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction);

    /** Branch 2: Cleanse - remove dangerous debuffs */
    bool TryCleanseBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction);

    /** Branch 3: Offensive - attack best target */
    FAction BuildOffensiveAction(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp);

    /** Check if actor has dangerous debuffs */
    bool HasDangerousDebuff(AActor *Actor);

    // Healing detection
    USpellData *FindHealingSpell(ULoadoutComponent *Loadout);
    /** Crystal-refactor 9.5b: returns FCrystalId of a matching slot, or a
     *  default-constructed FCrystalId{Garnet, F_Tier} with bOutFound=false
     *  semantics encoded via the companion bool. Callers must check bFound
     *  before assigning to OutAction.ItemData. */
    FCrystalId FindHealingItem(ULoadoutComponent *Loadout, bool &bOutFound);

    // Cleanse detection
    USpellData *FindCleanseSpell(ULoadoutComponent *Loadout);
    FCrystalId FindCleanseItem(ULoadoutComponent *Loadout, bool &bOutFound);

    // Energy detection
    FCrystalId FindEnergyItem(ULoadoutComponent *Loadout, bool &bOutFound);

    // Energy helpers
    int32 GetCurrentEP(AActor *Actor) const;
    int32 GetMaxEP(AActor *Actor) const;

    // Character data helper
    UCharacterData *GetCharacterData(AActor *Actor) const;
    // ==================== STATUS BAR QUERIES ====================

    /** Check if target's status bar is near triggering */
    bool IsStatusBarNearTrigger(AActor *Target, float Threshold = 0.70f) const;

    /** Would this spell/ability trigger the status bar? */
    bool WouldTriggerStatusBar(AActor *Attacker, AActor *Target, float BuildupAmount) const;

    /** Is the pending status valuable for this situation? */
    bool IsValuableStatus(ESkillEffectType StatusType, AActor *Target) const;

    // ==================== INFUSION DECISIONS ====================

    /** Decide infusion level for a spell (0, 1, or 2) */
    int32 DecideSpellInfusionLevel(AActor *Attacker, AActor *Target, USpellData *Spell) const;

    /** Decide infusion level for an ability (0, 1, or 2) */
    int32 DecideAbilityInfusionLevel(AActor *Attacker, AActor *Target, UAbilityData *Ability) const;
};