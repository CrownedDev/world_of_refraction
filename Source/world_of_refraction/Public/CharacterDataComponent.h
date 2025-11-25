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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResurrected, AActor*, ResurrectedActor);

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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ========================================
    // CHARACTER TEMPLATE
    // ========================================

    /** Reference to character template (set in editor/BP) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    UCharacterData* CharacterData;

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

    // ========================================
    // INTERNAL HELPERS
    // ========================================

    void CheckDeath();
    int32 CalculateMaxHP() const;
    int32 CalculateMaxEP() const;
};