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
     * Also clears CurrentEP — BDs use BrokenDarknessManager::AbsorptionEnergy,
     * not regular EP.
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