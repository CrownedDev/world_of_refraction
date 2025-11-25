// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterDataComponent.h"
#include "CombatConstants.h"

UCharacterDataComponent::UCharacterDataComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedComponent(true);

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

void UCharacterDataComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
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

    // Calculate max values from template
    MaxHP = CalculateMaxHP();
    MaxEP = CalculateMaxEP();

    // Set to full
    CurrentHP = MaxHP;
    CurrentEP = MaxEP;
    bIsAlive = true;

    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: Initialized (%s) - HP: %d, EP: %d"),
           *GetOwner()->GetName(),
           *CharacterData->CharacterName.ToString(),
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

        UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: Reset to max"), *GetOwner()->GetName());
    }
}

// ========================================
// HP MANAGEMENT
// ========================================

void UCharacterDataComponent::ServerTakeDamage(int32 Damage)
{
    if (!GetOwner()->HasAuthority())
        return;

    if (Damage <= 0 || !bIsAlive)
        return;

    CurrentHP = FMath::Max(0, CurrentHP - Damage);

    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: Took %d damage (%d/%d HP)"),
           *GetOwner()->GetName(), Damage, CurrentHP, MaxHP);

    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    CheckDeath();
}

void UCharacterDataComponent::ServerHeal(int32 Amount)
{
    if (!GetOwner()->HasAuthority())
        return;

    if (Amount <= 0 || !bIsAlive)
        return;

    CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);

    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: Healed %d HP (%d/%d)"),
           *GetOwner()->GetName(), Amount, CurrentHP, MaxHP);

    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UCharacterDataComponent::ServerSetHP(int32 NewHP)
{
    if (!GetOwner()->HasAuthority())
        return;

    CurrentHP = FMath::Clamp(NewHP, 0, MaxHP);

    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: HP set to %d/%d"),
           *GetOwner()->GetName(), CurrentHP, MaxHP);

    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    CheckDeath();
}

// ========================================
// EP MANAGEMENT
// ========================================

void UCharacterDataComponent::ServerSpendEnergy(int32 Amount)
{
    if (!GetOwner()->HasAuthority())
        return;

    if (Amount <= 0)
        return;

    CurrentEP = FMath::Max(0, CurrentEP - Amount);

    UE_LOG(LogTemp, Verbose, TEXT("[CharacterDataComponent] %s: Spent %d energy (%d/%d)"),
           *GetOwner()->GetName(), Amount, CurrentEP, MaxEP);

    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::ServerGainEnergy(int32 Amount)
{
    if (!GetOwner()->HasAuthority())
        return;

    if (Amount <= 0)
        return;

    CurrentEP = FMath::Min(MaxEP, CurrentEP + Amount);

    UE_LOG(LogTemp, Verbose, TEXT("[CharacterDataComponent] %s: Gained %d energy (%d/%d)"),
           *GetOwner()->GetName(), Amount, CurrentEP, MaxEP);

    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::ServerSetEP(int32 NewEP)
{
    if (!GetOwner()->HasAuthority())
        return;

    CurrentEP = FMath::Clamp(NewEP, 0, MaxEP);

    UE_LOG(LogTemp, Verbose, TEXT("[CharacterDataComponent] %s: EP set to %d/%d"),
           *GetOwner()->GetName(), CurrentEP, MaxEP);

    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

// ========================================
// QUERIES
// ========================================

float UCharacterDataComponent::GetHPPercent() const
{
    if (MaxHP <= 0)
        return 0.0f;
    return static_cast<float>(CurrentHP) / MaxHP;
}

float UCharacterDataComponent::GetEPPercent() const
{
    if (MaxEP <= 0)
        return 0.0f;
    return static_cast<float>(CurrentEP) / MaxEP;
}

// ========================================
// REPLICATION CALLBACKS
// ========================================

void UCharacterDataComponent::OnRep_CurrentHP()
{
    OnHPChanged.Broadcast(CurrentHP, MaxHP);

    UE_LOG(LogTemp, Verbose, TEXT("[CharacterDataComponent] %s: HP replicated (%d/%d)"),
           *GetOwner()->GetName(), CurrentHP, MaxHP);
}

void UCharacterDataComponent::OnRep_CurrentEP()
{
    OnEPChanged.Broadcast(CurrentEP, MaxEP);

    UE_LOG(LogTemp, Verbose, TEXT("[CharacterDataComponent] %s: EP replicated (%d/%d)"),
           *GetOwner()->GetName(), CurrentEP, MaxEP);
}

// ========================================
// PRIVATE HELPERS
// ========================================

int32 UCharacterDataComponent::CalculateMaxHP() const
{
    if (!CharacterData)
    {
        return 100; // Default fallback
    }

    // Use your CharacterData formula
    // This is a placeholder - adjust to match your actual formula
    int32 BaseHP = 100;
    int32 BodyBonus = CharacterData->GetEffectiveBody() * 5;     // 5 HP per Body point
    int32 SpiritBonus = CharacterData->GetEffectiveSpirit() * 3; // 3 HP per Spirit point

    return BaseHP + BodyBonus + SpiritBonus;
}

int32 UCharacterDataComponent::CalculateMaxEP() const
{
    if (!CharacterData)
    {
        return 100; // Default fallback
    }

    // Use your CharacterData formula
    // This is a placeholder - adjust to match your actual formula
    int32 BaseEP = 100;
    int32 MindBonus = CharacterData->GetEffectiveMind() * 3;     // 3 EP per Mind point
    int32 SpiritBonus = CharacterData->GetEffectiveSpirit() * 2; // 2 EP per Spirit point

    return BaseEP + MindBonus + SpiritBonus;
}

void UCharacterDataComponent::CheckDeath()
{
    if (CurrentHP <= 0 && bIsAlive)
    {
        bIsAlive = false;
        OnDied.Broadcast();

        UE_LOG(LogTemp, Warning, TEXT("[CharacterDataComponent] %s: DIED"),
               *GetOwner()->GetName());
    }
    else if (CurrentHP > 0 && !bIsAlive)
    {
        bIsAlive = true;
        OnResurrected.Broadcast();

        UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: RESURRECTED"),
               *GetOwner()->GetName());
    }
}