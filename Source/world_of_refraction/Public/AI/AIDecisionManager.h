// AIDecisionManager.h
// AI decision making for combat turns

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AI/EAIDifficulty.h"
#include "AI/AIDecisionConstants.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/DefenseDifficulty.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "AIDecisionManager.generated.h"

class ACombatOrchestrator;
class UCharacterDataComponent;
class ULoadoutComponent;
class USkillEffectManager;
class UDamageCalculator;
class UActionExecutor;
class UDefenseSystem;
class UCharacterData;
class USkillDataBase;

/**
 * AI-internal action category. Distinct from EActionType: the attack/ability merge folded
 * EActionType::Attack into Ability, but the AI still scores basic attacks (damage-only) separately
 * from slottable abilities (damage + status + affordability). This key preserves that three-way
 * distinction without the removed enum value; it maps to FAction (ActionType Ability/Spell + SkillData,
 * IsAttack() telling them apart) at construction. Defend is the no-option fallback.
 */
enum class EAIActionChoice : uint8
{
    Attack,
    Spell,
    Ability,
    Defend
};

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

    /** Drop a decision still thinking (B-071: the player took that member over on its turn). */
    void CancelPendingDecision();

    /** The level ForActor plays at: a local-party member's AI seat setting (B-346, default Medium),
     *  else the encounter's difficulty. */
    UFUNCTION(BlueprintPure, Category = "AI")
    EAIDifficulty GetDifficultyFor(const AActor *ForActor) const;

    /** The spells the AI may pick from: the loadout's spells minus those greyed out because
     *  their source broke (D-083, B-181), which ValidateAction would refuse. */
    static TArray<USpellData *> GetUsableSpells(const ULoadoutComponent *Loadout);

    /** B-199 (Q-063b, Q-199): a Void-broken combatant's turn, played for it, player or AI. One
     *  of its usable actions at random (its basic attack, a spell, an ability, Defend; no items,
     *  Flee or switch), each that ValidateAction accepts, aimed by its TargetType (Self: the
     *  holder; Ally: the holder or a living ally; else a living enemy). Defend when nothing else validates. */
    FAction BuildRandomAction(AActor *Actor) const;

    /** B-200: what the attacker's Skill builds before the charge, tier and the target's resistance:
     *  authored buildup × the live Status Multiplier (gear, StatusStone, buffs), as AddStatusBuildup does. */
    static float EstimateBaseStatusBuildup(const UCharacterDataComponent *AttackerComp, const USkillDataBase *Skill);

    // ==================== DEFENSE DECISIONS ====================

    /**
     * Per-impact AI defense synthesizer (Cluster B). Re-decides attempt/type/direction for ONE
     * impact and submits an impact-anchored timestamped press (SubmitDefenseInput's InputTime) so
     * MatchAndConsumeInput judges it for THIS impact. The band it aims at comes from this impact's
     * authored difficulty (matches the matcher); AI difficulty governs aim-within-band only.
     * Called per impact from ActionExecutor::ResolveImpactDefense — the per-window timer path it
     * replaced has been removed.
     */
    void TrySynthesizeImpactDefense(AActor *Defender, AActor *Attacker, EActionType AttackType,
                                    int32 BaseDamage, float AttackSize,
                                    const FDefenseDifficultyTriple &ImpactDifficulty, double ImpactTime);

private:
    friend class FAIStatusQuerySpec;
    friend class FAIDamageEstimateSpec;
    friend class FAIInfusionLevelSpec;
    friend class FGASEffectReadsSpec;
    friend class FAIOneSkillPathSpec;
    friend class FSpellSchoolSpec;

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

    /** Used when the built action fails ValidateAction (stun / silence / EP / targets):
     *  basic attack on a living enemy if that validates, else Defend. */
    FAction BuildFallbackAction(AActor *AIActor) const;

    /** Pick a random action category based on available options (AI-internal; see EAIActionChoice). */
    EAIActionChoice ChooseActionType(AActor *AIActor, ULoadoutComponent *Loadout);

    /** Get thinking delay range for difficulty */
    void GetThinkingDelayRange(EAIDifficulty Difficulty, float &OutMin, float &OutMax) const;

    /** Calculate random thinking delay */
    float CalculateThinkingDelay(EAIDifficulty Difficulty) const;

    // ==================== DEFENSE DECISIONS ====================

    /** Defense system reference */
    UPROPERTY()
    UDefenseSystem *DefenseSystemRef = nullptr;

    // ==================== DEFENSE LOGIC ====================

    /** Choose defense type based on attack, incoming damage and difficulty */
    EDefenseType ChooseDefenseType(AActor *Defender, float AttackSize, int32 BaseDamage, EAIDifficulty Difficulty) const;

    /** Get defense attempt chance for difficulty */
    float GetDefenseAttemptChance(EAIDifficulty Difficulty) const;

    /**
     * Aim offset (seconds BEFORE impact) for a synthesized per-impact press. The perfect-band
     * is derived from the IMPACT's difficulty, keyed on ChosenType — mirroring MatchAndConsumeInput's
     * TypeTier EXACTLY (tier passed straight to DefenseDifficultyMultiplier; Inherit -> Easy x1.0 via
     * its terminal fallback) so the AI aims at the SAME band the matcher judges. AI difficulty selects
     * only the aim-within-band multiplier (EASY/.../EXPERT_DELTA_BAND_MULT) plus jitter. Clamped to
     * [0, Window x Mult]: never past impact (no late-bleed entry), never beyond the valid lead-in.
     */
    double CalculateDefenseDelta(EAIDifficulty AIDiff, EDefenseType ChosenType,
                                 const FDefenseDifficultyTriple &ImpactDifficulty,
                                 AActor *Defender, AActor *Attacker, EActionType AttackType) const;

    // ==================== QUERY ====================

    /** Get current difficulty from combat (the encounter's: it drives the enemies) */
    UFUNCTION(BlueprintPure, Category = "AI")
    EAIDifficulty GetCurrentDifficulty() const;

    // ==================== TARGET SCORING ====================

    /** Score a potential target */
    int32 ScoreTarget(AActor *Attacker, AActor *Target, int32 EstimatedDamage);

    /** Get best target from enemy list */
    AActor *SelectBestTarget(AActor *Attacker, const TArray<AActor *> &Enemies);

    /** Estimate damage to target with best available action */
    int32 EstimateBestDamage(AActor *Attacker, AActor *Target);

    /** OBSOLETE (B-345): forwards to EstimateSkillDamage; kept until the one path is proven in PIE.
     *  Execution-accurate spell damage estimate — routes the spell through
     *  DamageCalculator so attacker stats, ActionMods and defender defense
     *  are all applied, matching what real execution would deal. */
    int32 EstimateSpellDamage(AActor *Attacker, AActor *Target, USpellData *Spell, int32 InfusionLevel = 0, EInfusionSourceOption InfusionSource = EInfusionSourceOption::None) const;

    /** OBSOLETE (B-345): forwards to EstimateSkillDamage. Execution-accurate ability damage estimate. */
    int32 EstimateAbilityDamage(AActor *Attacker, AActor *Target, UAbilityData *Ability, int32 InfusionLevel = 0, EInfusionSourceOption InfusionSource = EInfusionSourceOption::None) const;

    /** One estimator behind EstimateSpellDamage / EstimateAbilityDamage (B-029): the executor's own
     *  pre-defense damage and element (UActionExecutor::PreviewActionDamage), through the damage
     *  calculator once, crit off, then the expected crit folded in. */
    int32 EstimateActionDamage(AActor *Attacker, AActor *Target, const FAction &Action) const;

    /** EstimateActionDamage's full preview, uncached. */
    int32 ComputeActionDamageEstimate(AActor *Attacker, AActor *Target, const FAction &Action) const;

    /** B-259: one decision's estimates, keyed by what EstimateActionDamage reads from the action
     *  (attacker, target, skill, type, infusion level, infusion source, spell source). Used only
     *  while bEstimateMemoActive (one BuildAction), emptied after, so nothing outlives a decision
     *  (per-call scratch, not per-fight state, D-064). */
    using FEstimateKey = TTuple<const AActor *, const AActor *, const USkillDataBase *, uint8, int32, uint8, uint8>;
    mutable TMap<FEstimateKey, int32> EstimateMemo;
    bool bEstimateMemoActive = false;

    /** Debug (B-259): full previews run and memo hits in the current or last decision. */
    mutable int32 EstimatePreviewCount = 0;
    mutable int32 EstimateMemoHitCount = 0;

    /** OBSOLETE (B-345): forwards to EstimateSkillStatusScore. */
    float EstimateStatusScore(AActor *Attacker, AActor *Target, USpellData *Spell);

    /** OBSOLETE (B-345): forwards to EstimateSkillStatusScore. */
    float EstimateStatusScore(AActor *Attacker, AActor *Target, UAbilityData *Ability);

    /** OBSOLETE (B-345): forwards to CanAffordSkill. */
    bool CanAffordSpell(AActor *Actor, USpellData *Spell, int32 InfusionLevel = 0) const;

    /** OBSOLETE (B-345): forwards to CanAffordSkill. */
    bool CanAffordAbility(AActor *Actor, UAbilityData *Ability, int32 InfusionLevel = 0) const;

    // ==================== ONE SKILL PATH (B-345, D-136) ====================
    // Spells and abilities are scored, costed and infused by the same functions; the type only
    // decides which FAction fields carry the skill (MakeSkillAction) and where the infusion source
    // comes from (DecideSkillInfusionSource).

    /** The FAction Skill goes out as: a spell carries SpellData, its resolved SpellSource and
     *  SpellInfusionLevel; anything else (abilities, weapon attacks) SkillData and AbilityInfusionLevel.
     *  A source rides only an infused (L1+) action. Target null = no target (a cost probe). */
    FAction MakeSkillAction(AActor *Attacker, AActor *Target, USkillDataBase *Skill, int32 InfusionLevel = 0,
                            EInfusionSourceOption InfusionSource = EInfusionSourceOption::None) const;

    /** Execution-accurate damage estimate for a spell or an ability (EstimateActionDamage). */
    int32 EstimateSkillDamage(AActor *Attacker, AActor *Target, USkillDataBase *Skill, int32 InfusionLevel = 0,
                              EInfusionSourceOption InfusionSource = EInfusionSourceOption::None) const;

    /** Value of Skill's status buildup against Target, pre-weight (trigger / contribute / redundant
     *  tiers from the tuning data). Non-const — reads HasDangerousDebuff. */
    float EstimateSkillStatusScore(AActor *Attacker, AActor *Target, USkillDataBase *Skill);

    /** The one skill score: damage estimate + status score x the tuning's AIStatusScoreWeight. */
    float ScoreSkill(AActor *Attacker, AActor *Target, USkillDataBase *Skill);

    /** True if Actor can pay Skill's EP at InfusionLevel (true when the executor is missing). */
    bool CanAffordSkill(AActor *Actor, USkillDataBase *Skill, int32 InfusionLevel = 0) const;

    /** A spell's origin-bound source (DecideSpellInfusionSource), else the ability heuristic. */
    EInfusionSourceOption DecideSkillInfusionSource(AActor *Attacker, USkillDataBase *Skill) const;

    /** DecideInfusionLevel with Skill's source and EstimateSkillDamage. */
    int32 DecideSkillInfusionLevel(AActor *Attacker, AActor *Target, USkillDataBase *Skill) const;

    /** The best affordable candidate on Target and its score (0 with none affordable). Ties keep the
     *  first. An unaffordable candidate that beats InOutUnaffordableBest replaces it and its L0 cost
     *  (the self-target Emerald reads them). */
    USkillDataBase *ChooseBestSkill(AActor *AIActor, AActor *Target, const TArray<USkillDataBase *> &Candidates,
                                    float &OutBestScore, float &InOutUnaffordableBest, int32 &InOutUnaffordableCost);

    /** Writes Skill into Action (whose Targets are set) through MakeSkillAction's fields, then its
     *  infusion: DecideSkillInfusionLevel, dropped to 0 when that level is unaffordable or there is no
     *  legal source, then ClampInfusionLevelForHP. */
    void ApplySkillChoice(AActor *AIActor, UCharacterDataComponent *CharComp, USkillDataBase *Skill, FAction &Action) const;

    /** The candidates of one offensive category: usable Destruction spells, or available abilities. */
    static TArray<USkillDataBase *> GetOffensiveSkills(ULoadoutComponent *Loadout, EAIActionChoice Category);

    /** B-356 (D-146): the AI reads a spell's effects, never its school. Offensive: aimed at enemies
     *  (Enemy / Anyone) and it deals damage or applies a debuff. */
    static bool IsOffensiveSpell(const USpellData &Spell);

    /** B-356: a spell with a Heal / HealthRestore payload (whatever its school). */
    static bool IsHealingSpell(const USpellData &Spell);

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

    /** DefyDeath (Sapphire) item slot if the loadout holds one — the self-ward Last Stand item.
     *  Mirrors FindHealingItem; matches EItemEffectType::DefyDeath. */
    FCrystalId FindDefyDeathItem(ULoadoutComponent *Loadout, bool &bOutFound);

    // Cleanse detection
    USpellData *FindCleanseSpell(ULoadoutComponent *Loadout);
    FCrystalId FindCleanseItem(ULoadoutComponent *Loadout, bool &bOutFound);

    // Energy detection
    FCrystalId FindEnergyItem(ULoadoutComponent *Loadout, bool &bOutFound);

    // Emerald (bonus-turn item) valuation — Medium+ only, scored in BuildOffensiveAction.
    /** GrantBonusTurn (Emerald) item slot if the loadout holds one. Mirrors FindHealingItem. */
    FCrystalId FindBonusTurnItem(ULoadoutComponent *Loadout, bool &bOutFound);

    /** Per-tick EffectValue of a ONE-TICK-LETHAL DoT on Target (EffectValue >= CurrentHP),
     *  else 0. The enemy-target Emerald gate — a single forced turn guarantees the kill. */
    float GetLethalDoTPerTick(AActor *Target);

    /** Enemy (target-team) turn-slots before Target's next appearance in the turn-order
     *  preview — the rescue window their team has to heal/cleanse before the DoT kills.
     *  Queries UTurnManager::PreviewTurnOrder; never caches the subsystem. */
    int32 GetRescueExposureTurns(AActor *AIActor, AActor *Target) const;

    // Energy helpers
    int32 GetCurrentEP(AActor *Actor) const;
    int32 GetMaxEP(AActor *Actor) const;
    /** B-241: current / max EP; 0 when the max is 0. */
    float GetEnergyFraction(AActor *Actor) const;

    /** B-241 (DRY audit A4): the infusion level for a spell or ability, one decision for both. Skill feeds the
     *  status-bar estimate; EstimateDamageAtLevel gives the damage at a level with Source's mode. */
    int32 DecideInfusionLevel(AActor *Attacker, AActor *Target, const USkillDataBase *Skill, EInfusionSourceOption Source,
                              TFunctionRef<int32(int32)> EstimateDamageAtLevel) const;

    // Character data helper
    UCharacterData *GetCharacterData(AActor *Actor) const;
    // ==================== STATUS BAR QUERIES ====================

    /** Check if target's status bar is at or above Threshold */
    bool IsStatusBarNearTrigger(AActor *Target, float Threshold) const;

    /** B-345: at or above the tuning data's AIStatusBarNearTrigger. */
    bool IsStatusBarNearTrigger(AActor *Target) const;

    /** Would this spell/ability trigger the status bar? */
    bool WouldTriggerStatusBar(AActor *Attacker, AActor *Target, float BuildupAmount) const;

    /** Is the pending status valuable for this situation? */
    bool IsValuableStatus(ESkillEffectType StatusType, AActor *Target) const;

    // ==================== INFUSION DECISIONS ====================

    /** OBSOLETE (B-345): forwards to DecideSkillInfusionLevel. */
    int32 DecideSpellInfusionLevel(AActor *Attacker, AActor *Target, USpellData *Spell) const;

    /** OBSOLETE (B-345): forwards to DecideSkillInfusionLevel. */
    int32 DecideAbilityInfusionLevel(AActor *Attacker, AActor *Target, UAbilityData *Ability) const;

    /** Decide the infusion SOURCE for a SPELL (origin-bound): the first allowed source from
     *  GetAllowedInfusionSourcesForSpell, Evolution preferred for the BD/Reality {Evolution, Innate}
     *  case. Returns None for Item / non-infusable (empty binding). Shared by BuildAction (sets
     *  SelectedSource) and DecideSpellInfusionLevel (source-aware prediction) so they never drift. */
    EInfusionSourceOption DecideSpellInfusionSource(AActor *Attacker, USpellData *Spell) const;

    /** Decide the infusion SOURCE for an ABILITY (abilities are not origin-bound). Heuristic over
     *  GetAvailableInfusionSources: Caster->Innate, Resonator->ActiveRing, else first crystal source
     *  (PrimaryRing/WeaponCrystal/Evolution), else Raw (always available). */
    EInfusionSourceOption DecideAbilityInfusionSource(AActor *Attacker) const;

    /** Clamp an infusion level so an HP-paying source can't kill the caster: drops L2->L1->L0 until
     *  WouldKill is false (prefers a weaker infusion over self-death). HP-paying = Raw / Innate-on-spell
     *  / Evolution; crystal sources (no HP) and L0 pass through unchanged. BaseEnergyCost is the
     *  un-charge-multiplied infused EP (Spell/Ability CalculateEnergyCost). Action is the
     *  guarded action (SpellData/SkillData + source populated) — used to mirror the real
     *  ApplyCommitCosts basis, which now folds the reciprocal tier-gap cost multiplier. */
    int32 ClampInfusionLevelForHP(AActor *Attacker, UCharacterDataComponent *Comp, int32 BaseEnergyCost,
                                  bool bIsSpell, EInfusionSourceOption Source, int32 Level,
                                  const FAction &Action) const;
};