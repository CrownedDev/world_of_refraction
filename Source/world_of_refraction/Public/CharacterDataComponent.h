// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "CharacterData.h"
#include "CharacterDataComponent.generated.h"

/**
 * CharacterDataComponent - Runtime Character State
 *
 * Wraps a CharacterData asset (template) and maintains runtime state:
 * - Current HP/EP
 * - Status effects
 * - Temporary stat modifiers
 * - Replication for multiplayer
 *
 * Design: Separates static data (CharacterData asset) from runtime state (this component)
 * One CharacterData asset can be shared by many actors, each with unique runtime state.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCharacterDataComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterDataComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

public:
    // ========================================
    // CHARACTER DATA (TEMPLATE)
    // ========================================

    /** Static character data template (stats, abilities, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    UCharacterData *CharacterData;

    // ========================================
    // RUNTIME STATE (REPLICATED)
    // ========================================

    /** Current hit points */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, BlueprintReadOnly, Category = "Stats|Runtime")
    int32 CurrentHP;

    /** Current energy/stamina points */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentEP, BlueprintReadOnly, Category = "Stats|Runtime")
    int32 CurrentEP;

    /** Is character alive? */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats|Runtime")
    bool bIsAlive = true;

    // ========================================
    // MAX VALUES (CALCULATED FROM TEMPLATE)
    // ========================================

    /** Maximum HP (calculated from CharacterData) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats|Max")
    int32 MaxHP;

    /** Maximum EP (calculated from CharacterData) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats|Max")
    int32 MaxEP;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** Initialize from CharacterData template */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void InitializeFromTemplate();

    /** Reset to full HP/EP */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void ResetToMax();

    // ========================================
    // HP MANAGEMENT (SERVER AUTHORITATIVE)
    // ========================================

    /** Server: Take damage */
    UFUNCTION(BlueprintCallable, Category = "Character|HP")
    void ServerTakeDamage(int32 Damage);

    /** Server: Heal */
    UFUNCTION(BlueprintCallable, Category = "Character|HP")
    void ServerHeal(int32 Amount);

    /** Server: Set HP directly */
    UFUNCTION(BlueprintCallable, Category = "Character|HP")
    void ServerSetHP(int32 NewHP);

    // ========================================
    // EP MANAGEMENT (SERVER AUTHORITATIVE)
    // ========================================

    /** Server: Spend energy */
    UFUNCTION(BlueprintCallable, Category = "Character|EP")
    void ServerSpendEnergy(int32 Amount);

    /** Server: Gain energy */
    UFUNCTION(BlueprintCallable, Category = "Character|EP")
    void ServerGainEnergy(int32 Amount);

    /** Server: Set EP directly */
    UFUNCTION(BlueprintCallable, Category = "Character|EP")
    void ServerSetEP(int32 NewEP);

    // ========================================
    // QUERIES
    // ========================================

    /** Get HP percentage (0-1) */
    UFUNCTION(BlueprintPure, Category = "Character|HP")
    float GetHPPercent() const;

    /** Get EP percentage (0-1) */
    UFUNCTION(BlueprintPure, Category = "Character|EP")
    float GetEPPercent() const;

    /** Is character at full HP? */
    UFUNCTION(BlueprintPure, Category = "Character|HP")
    bool IsFullHP() const { return CurrentHP >= MaxHP; }

    /** Is character at full EP? */
    UFUNCTION(BlueprintPure, Category = "Character|EP")
    bool IsFullEP() const { return CurrentEP >= MaxEP; }

    // ========================================
    // REPLICATION CALLBACKS
    // ========================================

    UFUNCTION()
    void OnRep_CurrentHP();

    UFUNCTION()
    void OnRep_CurrentEP();

    // ========================================
    // EVENTS (BLUEPRINT ASSIGNABLE)
    // ========================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, int32, NewHP, int32, MaxHP);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPChanged, int32, NewEP, int32, MaxEP);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDied);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResurrected);

    UPROPERTY(BlueprintAssignable, Category = "Character|Events")
    FOnHPChanged OnHPChanged;

    UPROPERTY(BlueprintAssignable, Category = "Character|Events")
    FOnEPChanged OnEPChanged;

    UPROPERTY(BlueprintAssignable, Category = "Character|Events")
    FOnDied OnDied;

    UPROPERTY(BlueprintAssignable, Category = "Character|Events")
    FOnResurrected OnResurrected;

private:
    /** Calculate max HP from template */
    int32 CalculateMaxHP() const;

    /** Calculate max EP from template */
    int32 CalculateMaxEP() const;

    /** Check if HP reached zero */
    void CheckDeath();
};