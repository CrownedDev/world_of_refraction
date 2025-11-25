// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterDataComponent.h"
#include "CombatConstants.h"

UCharacterDataComponent::UCharacterDataComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    CurrentHP = 100;
    CurrentEP = 100;
    MaxHP = 100;
    MaxEP = 100;
    bIsAlive = true;
}

void UCharacterDataComponent::BeginPlay()
{
    Super::BeginPlay();

    if (CharacterData)
    {
        InitializeFromTemplate();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterDataComponent] %s: No CharacterData assigned!"),
            *GetOwner()->GetName());
    }
}

void UCharacterDataComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UCharacterDataComponent, CurrentHP);
    DOREPLIFETIME(UCharacterDataComponent, CurrentEP);
    DOREPLIFETIME(UCharacterDataComponent, bIsAlive);
}

void UCharacterDataComponent::InitializeFromTemplate()
{
    if (!CharacterData)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterDataComponent] %s: Cannot initialize - no CharacterData!"),
            *GetOwner()->GetName());
        return;
    }

    MaxHP = CalculateMaxHP();
    MaxEP = CalculateMaxEP();
    CurrentHP = MaxHP;
    CurrentEP = MaxEP;
    bIsAlive = true;

    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: Initialized (%s) - HP: %d, EP: %d"),
        *GetOwner()->GetName(),
        *CharacterData->CharacterName,
        MaxHP,
        MaxEP);
}

void UCharacterDataComponent::ResetToMax()
{
    if (GetOwner()->HasAuthority())
    {
        CurrentHP = MaxHP;
        CurrentEP = MaxEP;
        bIsAlive = true;
        OnHPChanged.Broadcast(CurrentHP, MaxHP);
        OnEPChanged.Broadcast(CurrentEP, MaxEP);
    }
}

void UCharacterDataComponent::ServerTakeDamage(int32 Damage)
{
    if (!GetOwner()->HasAuthority() || Damage <= 0 || !bIsAlive)
        return;

    CurrentHP = FMath::Max(0, CurrentHP - Damage);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    CheckDeath();
}

void UCharacterDataComponent::ServerHeal(int32 Amount)
{
    if (!GetOwner()->HasAuthority() || Amount <= 0 || !bIsAlive)
        return;

    CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UCharacterDataComponent::ServerSetHP(int32 NewHP)
{
    if (!GetOwner()->HasAuthority())
        return;

    CurrentHP = FMath::Clamp(NewHP, 0, MaxHP);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    CheckDeath();
}

void UCharacterDataComponent::ServerSpendEnergy(int32 Amount)
{
    if (!GetOwner()->HasAuthority() || Amount <= 0)
        return;

    CurrentEP = FMath::Max(0, CurrentEP - Amount);
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::ServerGainEnergy(int32 Amount)
{
    if (!GetOwner()->HasAuthority() || Amount <= 0)
        return;

    CurrentEP = FMath::Min(MaxEP, CurrentEP + Amount);
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::ServerSetEP(int32 NewEP)
{
    if (!GetOwner()->HasAuthority())
        return;

    CurrentEP = FMath::Clamp(NewEP, 0, MaxEP);
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::CheckDeath()
{
    if (CurrentHP <= 0 && bIsAlive)
    {
        bIsAlive = false;
        OnDied.Broadcast(GetOwner());
    }
}

void UCharacterDataComponent::ServerResurrect(int32 HPToRestore)
{
    if (!GetOwner()->HasAuthority() || bIsAlive)
        return;

    bIsAlive = true;
    CurrentHP = FMath::Clamp(HPToRestore, 1, MaxHP);
    OnResurrected.Broadcast(GetOwner());
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UCharacterDataComponent::OnRep_CurrentHP()
{
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UCharacterDataComponent::OnRep_CurrentEP()
{
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::OnRep_bIsAlive()
{
    if (bIsAlive)
        OnResurrected.Broadcast(GetOwner());
    else
        OnDied.Broadcast(GetOwner());
}

int32 UCharacterDataComponent::CalculateMaxHP() const
{
    if (!CharacterData)
        return 100;

    // TODO: Implement actual HP formula
    return 100;
}

int32 UCharacterDataComponent::CalculateMaxEP() const
{
    if (!CharacterData)
        return 100;

    // TODO: Implement actual EP formula
    return 100;
}