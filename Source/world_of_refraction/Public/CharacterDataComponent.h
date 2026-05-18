// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "CharacterData.h"
#include "CharacterDataComponent.generated.h"

/**
 * Delegate signatures for HP/EP changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, int32, CurrentHP, int32, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPChanged, int32, CurrentEP, int32, MaxEP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, AActor *, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResurrected, AActor *, ResurrectedActor);

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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

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
    float GetCrystalModifiedMind() const;

    /** Body equivalent of GetCrystalModifiedMind — see comment there.
     *  Equipment layer uses BonusBodyModifierPercent. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedBody() const;

    /** Spirit equivalent of GetCrystalModifiedMind — see comment there.
     *  Equipment layer uses BonusSpiritModifierPercent. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedSpirit() const;

    /** Crystal-aware Luck: pillar-scaled against GetCrystalModifiedSpirit plus
     *  the active loadout's BonusLuck contribution, clamped to LUCK_RAW_MAX.
     *  Use in place of CharacterData->CalculateLuck() for any consumer that
     *  should respect crystal Spirit modifier AND equipment-driven bonuses
     *  (crit bonus, break skip, dodge, drops). */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetEquipmentModifiedLuck() const;

    /** Crystal-aware derived-stat helpers. Each mirrors the asset's matching
     *  Calculate* formula shape, but reads GetCrystalModified{Mind,Body,Spirit}
     *  as the EffectivePillar input. Use these in place of the raw asset
     *  Calculate* calls anywhere the slotted primary evolution crystal's
     *  pillar modifier should apply. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedSpellDamage() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedRawDamage() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedCritChance() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    int32 GetCrystalModifiedFlatDefense() const;

    /** Same formula as GetCrystalModifiedSpellDamage — separate entry point
     *  for clarity at healing call sites (healing scales with spell power). */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedSpellDamageForHealing() const;

    /** Crystal-aware Efficiency multiplier — mirrors
     *  UCharacterData::CalculateEfficiencyMultiplier but uses
     *  GetCrystalModifiedMind() in place of GetEffectiveMind() so the
     *  slotted primary evolution crystal's Mind pillar modifier feeds the
     *  cost-reduction curve. Same clamp shape as the asset formula. */
    UFUNCTION(BlueprintPure, Category = "Combat|Stats")
    float GetCrystalModifiedEfficiencyMultiplier() const;

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
};