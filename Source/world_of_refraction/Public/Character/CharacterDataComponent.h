// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "CharacterDataComponent.generated.h"

/**
 * Delegate signatures for HP/EP changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, int32, CurrentHP, int32, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPChanged, int32, CurrentEP, int32, MaxEP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, AActor *, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResurrected, AActor *, ResurrectedActor);

class USkillEffectManager;

/**
 * FEffectiveStats — a one-call snapshot of a character's effective (layered) stats,
 * packed by UCharacterDataComponent::GetEffectiveStats. Each field is the VERBATIM
 * return of an existing getter — no recompute, no reorder — so a consumer reading a
 * field gets the same value as calling that getter directly, in one call. Additive
 * convenience: existing direct-getter callers are unaffected.
 *
 * ⚠️ SpellDamage is the COMPOSED scalar — read freely by Broken Darkness and the
 * crystal-wear power term, but DamageCalculator MUST keep its own per-layer sourcing
 * (see the field note). It is the only field with a consumer restriction.
 */
USTRUCT(BlueprintType)
struct FEffectiveStats
{
    GENERATED_BODY()

    /** Composed 4-layer SpellDamage scalar = GetEffectiveSpellDamage()
     *  ((innate+equipment) × stone × transient). This is the value the non-pipeline
     *  consumers read: Broken Darkness overload / forbidden-cast scaling AND the
     *  crystal-wear POWER term (more spell power = more strain = more wear), plus UI.
     *  ⚠️ DamageCalculator MUST keep its per-layer sourcing — do NOT read this field in
     *     the damage pipeline: it interleaves ActionMods / Grid / defender terms, so
     *     reading the pre-composed value would reintroduce the byte-identity break.
     *     Wear and BD use it freely; only the damage pipeline is off-limits. */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Stats")
    float SpellDamage = 1.0f;

    /** = GetEffectiveStatusMultiplier() — composed (innate + equipment + StatusStone)
     *  × transient buff/debuff. */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Stats")
    float StatusMultiplier = 1.0f;

    /** = GetEffectiveEfficiencyMultiplier() — composed (innate + equipment + EfficiencyStone
     *  + transient). */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Stats")
    float EfficiencyMultiplier = 1.0f;

    /** = GetEffectiveResistance() — composed, element-AGNOSTIC self-resistance (innate +
     *  equipment + ResistanceStone + transient ModifyStatusResist). NOT the SBM element-
     *  matched defense value. */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Stats")
    float Resistance = 0.0f;
};

/**
 * CharacterDataComponent
 * Runtime character state (HP, EP, alive/dead)
 * Wraps CharacterData asset for combat use
 * Replicated for multiplayer
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCharacterDataComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterDataComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

    /** Re-runs RecomputeMaxPools when a transient pool (MaxHP/MaxEnergy) buff/debuff
     *  applies or expires on this owner. Bound to the SkillEffectManager's OnEffectApplied
     *  + OnEffectRemoved in BeginPlay; gated to this owner + pool effect types. */
    UFUNCTION()
    void HandlePoolEffectChanged(AActor *Target, const FActiveSkillEffect &Effect);

    // ========================================
    // CHARACTER TEMPLATE
    // ========================================

    /** Reference to character template (set in editor/BP) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    UCharacterData *CharacterData;

    // ========================================
    // RUNTIME STATE (REPLICATED)
    // ========================================

    UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, BlueprintReadOnly, Category = "Combat")
    int32 CurrentHP;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentEP, BlueprintReadOnly, Category = "Combat")
    int32 CurrentEP;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    int32 MaxHP;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    int32 MaxEP;

    UPROPERTY(ReplicatedUsing = OnRep_bIsAlive, BlueprintReadOnly, Category = "Combat")
    bool bIsAlive;

    /**
     * Runtime BD transformation state. True if the character is behaviourally
     * Broken Darkness — either via runtime transformation (RollForBreak success
     * → TriggerTransformation) or via being constructed with InnateElement =
     * BrokenDarkness at character creation.
     *
     * IMPORTANT: Read via IsBrokenDarkness() — never read this field directly.
     * The helper handles the "or character-created" case uniformly.
     *
     * SaveGame-tagged for future persistence wiring; no save system exists yet,
     * so the flag is session-only as of this commit.
     */
    UPROPERTY(SaveGame, ReplicatedUsing = OnRep_bIsBrokenDarkness, VisibleAnywhere, BlueprintReadOnly, Category = "Combat|BrokenDarkness")
    bool bIsBrokenDarkness = false;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize HP/EP from CharacterData template */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void InitializeFromTemplate();

    /** Reset to full HP/EP */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void ResetToMax();

    /** Recompute MaxHP / MaxEP using the crystal-modified Body/Spirit pillars
     *  plus the active loadout's BonusMaxHP / BonusMaxEnergy contribution.
     *  Called from BeginPlay; safe to call again when equipment changes
     *  mid-combat. Does NOT clamp CurrentHP/CurrentEP or broadcast change
     *  events — caller decides clamp/refill/notify policy. */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void RecomputeMaxPools();

    // ========================================
    // HP MANAGEMENT (SERVER ONLY)
    // ========================================

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerTakeDamage(int32 Damage);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerHeal(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerSetHP(int32 NewHP);

    // ========================================
    // EP MANAGEMENT (SERVER ONLY)
    // ========================================

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerSpendEnergy(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerGainEnergy(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerSetEP(int32 NewEP);

    /**
     * Server-only: Broken Darkness absorption-energy gain. Unlike
     * ServerGainEnergy this is NOT suppressed for BD characters — it is the
     * event-driven gain path (parry / block absorption, crystal use). Permits
     * CurrentEP to rise above MaxEP into overload: the upper bound is
     * AbsoluteMax, which BrokenDarknessManager supplies as MaxEP + its
     * OverloadCapacity. Broadcasts OnEPChanged.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerGainBrokenDarknessEnergy(int32 Amount, int32 AbsoluteMax);

    // ========================================
    // DEATH/RESURRECTION (SERVER ONLY)
    // ========================================

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ServerResurrect(int32 HPToRestore);

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnHPChanged OnHPChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnEPChanged OnEPChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnDied OnDied;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnResurrected OnResurrected;

    // ==================== EQUIPMENT ACCESS ====================

    /** Get currently active weapon (for visual systems like mesh, VFX) */
    UFUNCTION(BlueprintPure, Category = "Equipment")
    UWeaponData *GetActiveWeapon() const;

    /** True if this character has a usable EP-spend target.
     *  Resonator-specific in spirit; for other classes EP is always usable so
     *  this is moot. Returns true when:
     *   - Active weapon present (weapon attacks cost EP), OR
     *   - Primary slot is Evolution (Evolution spells cost EP per locked design).
     *  Used by ServerGainEnergy / ServerSetEP suppression and the panel
     *  EP-bar visibility rule. */
    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool HasUsableEPTarget() const;

    // ==================== DEBUG ====================
    /** Debug: Toggle between primary and secondary weapon */
    UFUNCTION(BlueprintCallable, Category = "Debug", meta = (CallInEditor = "true"))
    void DebugToggleWeapon();

    // ==================== CRYSTAL-AWARE PILLAR VALUES ====================

    /** Base EffectiveMind from the asset, modified by TWO pillar-modifier
     *  sources layered in order:
     *   1. The slotted primary evolution crystal's Pillar/SubStats contribution.
     *   2. The active loadout's BonusMindModifierPercent (multiplicative on top).
     *  Either layer is independently optional. Falls back to the raw asset
     *  value if no LoadoutComponent is present.
     *  Use in place of CharacterData->GetEffectiveMind() for any system that
     *  needs to see crystal- and equipment-driven pillar adjustments. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedMind() const;

    /** Body equivalent of GetEvolutionModifiedMind — see comment there.
     *  Equipment layer uses BonusBodyModifierPercent. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedBody() const;

    /** Spirit equivalent of GetEvolutionModifiedMind — see comment there.
     *  Equipment layer uses BonusSpiritModifierPercent. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedSpirit() const;

    /** Crystal-aware Luck: pillar-scaled against GetEvolutionModifiedSpirit plus
     *  the active loadout's BonusLuck contribution. Input is UNBOUNDED (no upper
     *  cap) and may go negative (debuff curse); LUCK_RAW_MAX is the per-consumer
     *  normalization basis, not a cap here. Each consumer upper-clamps its own
     *  normalized fraction (FMath::Min(RawLuck/LUCK_RAW_MAX, 1)) and lets negatives
     *  pass. Use in place of CharacterData->CalculateLuck() for any consumer that
     *  should respect crystal Spirit modifier AND equipment-driven bonuses
     *  (crit bonus, break skip, dodge, drops). */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEquipmentModifiedLuck() const;

    /** Crystal-aware derived-stat helpers. Each mirrors the asset's matching
     *  Calculate* formula shape, but reads GetEvolutionModified{Mind,Body,Spirit}
     *  as the EffectivePillar input. Use these in place of the raw asset
     *  Calculate* calls anywhere the slotted primary evolution crystal's
     *  pillar modifier should apply. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedSpellDamage() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedRawDamage() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedCritChance() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    int32 GetEvolutionModifiedFlatDefense() const;

    /** Same formula as GetEvolutionModifiedSpellDamage — separate entry point
     *  for clarity at healing call sites (healing scales with spell power). */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedSpellDamageForHealing() const;

    /** Fully-layered SpellDamage scalar: innate (GetEvolutionModifiedSpellDamage)
     *  + equipment BonusSpellDamage, then ×stone ×transient. This is the SAME value
     *  DamageCalculator's spell path produces MINUS the call-specific ActionMods,
     *  Grid, and defender terms (which have no analogue outside a damage call) — so
     *  it is the correct scalar for non-pipeline consumers (Broken Darkness overload
     *  / forbidden-cast self-cost). Composed from the three per-layer helpers below,
     *  which DamageCalculator DRY-sources at its own (unchanged) step order. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEffectiveSpellDamage() const;

    /** L2 — additive equipment SpellDamage term (BonusSpellDamage × per-point), the
     *  value Step 1 of CalculateDamage adds into AttackerMult AFTER ActionMods. Single
     *  source for the equipment layer. 0 when no loadout / no BonusSpellDamage. */
    float GetEquipmentSpellDamageTerm() const;

    /** L3 — fusion-aware attached-stone SpellDamage MULTIPLIER (1 + stone%/100), the
     *  factor Step 1.25b applies to RunningDamage. Reads through GetAttachedStonePercent
     *  so a SpellDamageStone — direct or as a fusion half — flows. 1.0 when none. */
    float GetStoneSpellDamageFactor() const;

    /** L4 — transient SpellDamage MULTIPLIER (1 + (SpellDamageBuff − SpellDamageDebuff)/100),
     *  the factor Step 2 applies inside GetStatusEffectDamageModifier (also the Amethyst
     *  gamble's spell arm). Single source for the transient layer. 1.0 when none. */
    float GetTransientSpellDamageFactor() const;

    /** Physical mirror of GetEffectiveSpellDamage — fully-layered RawDamage scalar:
     *  (innate GetEvolutionModifiedRawDamage + equipment BonusRawDamage) × stone × transient,
     *  then the [0,2] normalization clamp. Composed from the three per-layer helpers below,
     *  which DamageCalculator DRY-sources at its own (unchanged) physical step order. For
     *  non-pipeline / future composed consumers (e.g. AI threat scoring); the pipeline does
     *  NOT read it (it composes per-layer with call-specific terms interleaved). */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEffectiveRawDamage() const;

    /** L2 — additive equipment RawDamage term (BonusRawDamage × per-point), the value Step 1
     *  of CalculateDamage adds into AttackerMult. Physical mirror of GetEquipmentSpellDamageTerm.
     *  0 when no loadout / no BonusRawDamage. */
    float GetEquipmentRawDamageTerm() const;

    /** L3 — fusion-aware attached-stone RawDamage MULTIPLIER (1 + stone%/100), the factor
     *  Step 1.25 applies to RunningDamage. Physical mirror of GetStoneSpellDamageFactor.
     *  1.0 when none. */
    float GetStoneRawDamageFactor() const;

    /** L4 — transient RawDamage MULTIPLIER (1 + (RawDamageBuff − RawDamageDebuff)/100), the
     *  factor Step 2 applies (physical arm) inside GetStatusEffectDamageModifier. Physical
     *  mirror of GetTransientSpellDamageFactor. 1.0 when none. */
    float GetTransientRawDamageFactor() const;

    /** One-call snapshot of the effective layered stats (FEffectiveStats). Each field
     *  is the verbatim return of an existing getter. Additive convenience for snapshot
     *  consumers (e.g. the crystal-wear input cluster); existing direct-getter callers
     *  are unaffected. See the struct's SpellDamage field note — it is the composed
     *  scalar (BD + wear read it; the damage pipeline must not). */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    FEffectiveStats GetEffectiveStats() const;

    /** Effective Efficiency cost/drain multiplier: innate (crystal-aware Mind) +
     *  equipment BonusEfficiency + attached EfficiencyStone, all as reductions under
     *  ONE clamp. The sole Efficiency getter — drives BD drain / durability / EP cost.
     *  Collapses to the innate-only term when the owner has no BonusEfficiency and no
     *  EfficiencyStone. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEffectiveEfficiencyMultiplier() const;

    /** Crystal-aware StatusMultiplier — mirrors
     *  UCharacterData::CalculateStatusMultiplier but uses
     *  GetEvolutionModifiedSpirit() in place of GetEffectiveSpirit().
     *  Returns 1+frac like the raw formula. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedStatusMultiplier() const;

    /** Crystal-aware Resistance — mirrors
     *  UCharacterData::CalculateResistance but uses
     *  GetEvolutionModifiedSpirit() in place of GetEffectiveSpirit().
     *  Returns the raw fraction clamped to [0, RESISTANCE_MAX]. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEvolutionModifiedResistance() const;

    /** Fully-composed StatusMultiplier = base × transient, where base is the
     *  (innate + equipment + StatusStone) factor composed inline to match
     *  UStatusBuildupManager::GetSourceStatusMultiplierFactor term-for-term, and transient
     *  is Max(0, 1 + (StatusMultiplierBuff − StatusMultiplierDebuff)/100). Equals the
     *  BD/CombatOrchestrator inline value BY CONSTRUCTION (BD is NOT re-pointed). Used by
     *  the crystal-wear POWER term so a StatusStone/Status-buff raises wear. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEffectiveStatusMultiplier() const;

    /** Element-AGNOSTIC composed self-resistance for the crystal-wear CONTROL term:
     *  innate (raw, unclamped) + equipment BonusResistance + attached ResistanceStone +
     *  transient ModifyStatusResist (the blanket channel — NOT the element-matched
     *  ResistanceBuff/Debuff). Deliberately NOT the StatusBuildupManager element-matched
     *  defense aggregate, and NO inner [0/-1, MAX] clamp (wear's ControlFactor bounds it).
     *  A ResistanceStone/resist-buff raises this → more control → less wear. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEffectiveResistance() const;

    // ========================================
    // BROKEN DARKNESS STATE
    // ========================================

    /**
     * Returns true if this character is behaviourally Broken Darkness — either
     * via runtime transformation (bIsBrokenDarkness flag) OR via being a
     * character-created BD (InnateElement = BrokenDarkness on the asset).
     *
     * All BD-aware code should call this helper rather than checking
     * InnateElement or bIsBrokenDarkness directly. The helper unifies the
     * two paths so character-created BDs and runtime-transformed BDs are
     * treated identically.
     */
    UFUNCTION(BlueprintPure, Category = "BrokenDarkness")
    bool IsBrokenDarkness() const;

    /**
     * Server-only: set the runtime BD flag. Called by
     * BrokenDarknessManager::TriggerTransformation on a successful break roll.
     * CurrentEP carries over — the energy held at transformation becomes the
     * new BD's starting absorption buffer.
     */
    UFUNCTION(BlueprintCallable, Category = "BrokenDarkness")
    void ServerSetBrokenDarkness(bool bNewState);

    /**
     * Display-time element for UI. Single source of truth for "what element
     * does this character read as right now". When the character is BD (per
     * IsBrokenDarkness — covers both runtime-transformed and character-created
     * paths), this returns ESpellElement::BrokenDarkness so panels / labels
     * surface the BD identity rather than the pre-transform element. Otherwise
     * delegates to UCharacterData::GetElement (Caster → InnateElement; others
     * → Generic). Generic when no CharacterData is set.
     *
     * UI callers (CharacterPanelWidget energy bar, ClassElementText in
     * WBP_CharacterPanel, etc.) should prefer this over reading InnateElement
     * directly. Gameplay-internal reads (ActionExecutor element resolution,
     * BD validation) keep their existing paths — those reason about the
     * underlying CharacterData, not the display identity.
     */
    UFUNCTION(BlueprintPure, Category = "Character|Element")
    ESpellElement GetDisplayElement() const;

private:
    // ========================================
    // REPLICATION CALLBACKS
    // ========================================

    UFUNCTION()
    void OnRep_CurrentHP();

    UFUNCTION()
    void OnRep_CurrentEP();

    UFUNCTION()
    void OnRep_bIsAlive();

    UFUNCTION()
    void OnRep_bIsBrokenDarkness();

    // ========================================
    // INTERNAL HELPERS
    // ========================================

    void CheckDeath();
    bool HasServerAuthority() const;
    int32 CalculateMaxHealth() const;
    int32 CalculateMaxEnergy() const;

    /** Cached SkillEffectManager accessor — lazily resolves the GameInstance subsystem
     *  and re-resolves whenever the cache is null. Honours "never cache subsystem
     *  pointers across PIE sessions": a fresh component starts with a null cache and
     *  re-fetches. Mirrors UDamageCalculator::GetSkillEffectManager. Same subsystem
     *  pointer as the prior inline GetWorld()->GetGameInstance()->GetSubsystem reads,
     *  so every routed site stays byte-identical. */
    USkillEffectManager *GetSkillEffectManager() const;

    UPROPERTY()
    mutable USkillEffectManager *CachedSkillEffectManager = nullptr;
};