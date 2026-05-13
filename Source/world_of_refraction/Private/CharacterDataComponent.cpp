// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterDataComponent.h"
#include "CombatConstants.h"
#include "EWeaponSlotType.h"
#include "WeaponData.h"
#include "StanceData.h"
#include "InventoryComponent.h"
#include "LoadoutComponent.h"
#include "ItemData.h"

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

    // Initialize HP/EP from CharacterData
    if (CharacterData)
    {
        // Auto-initialize Inventory and Loadout BEFORE computing HP/EP so the
        // crystal-aware pillar reads in GetCrystalModifiedBody/Spirit can see
        // the slotted primary evolution crystal.
        AActor *Owner = GetOwner();
        if (Owner)
        {
            UInventoryComponent *Inventory = Owner->FindComponentByClass<UInventoryComponent>();
            ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>();

            if (Inventory)
            {
                Inventory->InitializeFromCharacterData(CharacterData);
            }
            if (Loadout && Inventory)
            {
                Loadout->InitializeFromCharacterData(CharacterData, Inventory);
            }
        }

        // HP/EP init — crystal-aware path. Inlines UCharacterData::CalculateMaxHealth
        // / CalculateMaxEnergy formulas against GetCrystalModifiedBody/Spirit so a
        // slotted primary evolution crystal feeds pool sizes.
        const float ModifiedBody = GetCrystalModifiedBody();
        MaxHP = FMath::RoundToInt(
            CombatConstants::MAX_HEALTH_BASE +
            (ModifiedBody * CharacterData->GetTotalMaxHealth() * CombatConstants::MAX_HEALTH_PER_POINT));
        CurrentHP = MaxHP;

        const float ModifiedSpirit = GetCrystalModifiedSpirit();
        MaxEP = FMath::RoundToInt(
            CombatConstants::MAX_ENERGY_BASE +
            (ModifiedSpirit * CharacterData->GetTotalMaxEnergy() * CombatConstants::MAX_ENERGY_PER_POINT));
        CurrentEP = MaxEP;

        // Character-created BD: auto-flip the runtime flag and zero EP so
        // they start in the correct state without needing a transform event.
        if (HasServerAuthority() &&
            CharacterData->InnateElement == ESpellElement::BrokenDarkness)
        {
            bIsBrokenDarkness = true;
            CurrentEP = 0;
        }
    }
}

void UCharacterDataComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UCharacterDataComponent, CurrentHP);
    DOREPLIFETIME(UCharacterDataComponent, CurrentEP);
    DOREPLIFETIME(UCharacterDataComponent, bIsAlive);
    DOREPLIFETIME(UCharacterDataComponent, bIsBrokenDarkness);
}

void UCharacterDataComponent::InitializeFromTemplate()
{
    if (!CharacterData)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterDataComponent] %s: Cannot initialize - no CharacterData!"),
               *GetOwner()->GetName());
        return;
    }

    MaxHP = CalculateMaxHealth();
    MaxEP = CalculateMaxEnergy();
    CurrentHP = MaxHP;
    CurrentEP = MaxEP;
    bIsAlive = true;

    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s: Initialized (%s) - HP: %d, EP: %d"),
           *GetOwner()->GetName(),
           *CharacterData->Name,
           MaxHP,
           MaxEP);
}

void UCharacterDataComponent::ResetToMax()
{
    if (HasServerAuthority())
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
    if (!HasServerAuthority() || Damage <= 0 || !bIsAlive)
        return;

    CurrentHP = FMath::Max(0, CurrentHP - Damage);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    CheckDeath();
}

void UCharacterDataComponent::ServerHeal(int32 Amount)
{
    if (!HasServerAuthority() || Amount <= 0 || !bIsAlive)
        return;

    CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UCharacterDataComponent::ServerSetHP(int32 NewHP)
{
    if (!HasServerAuthority())
        return;

    CurrentHP = FMath::Clamp(NewHP, 0, MaxHP);
    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    CheckDeath();
}

void UCharacterDataComponent::ServerSpendEnergy(int32 Amount)
{
    if (!HasServerAuthority() || Amount <= 0)
        return;

    CurrentEP = FMath::Max(0, CurrentEP - Amount);
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::ServerGainEnergy(int32 Amount)
{
    if (!HasServerAuthority() || Amount <= 0)
        return;

    // BD characters do not gain regular EP — their energy lives on
    // BrokenDarknessManager::AbsorptionEnergy.
    if (bIsBrokenDarkness)
        return;

    // Resonators only accumulate EP when they have a usable EP-spend target.
    // That's either an active weapon (weapon attacks cost EP) or an Evolution
    // primary (Evolution spells cost EP per locked design). Without either,
    // EP gain is suppressed — the pool exists but is dormant.
    if (CharacterData &&
        CharacterData->CharacterClass == ECharacterClass::Resonator &&
        !HasUsableEPTarget())
    {
        return;
    }

    CurrentEP = FMath::Min(MaxEP, CurrentEP + Amount);
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::ServerSetEP(int32 NewEP)
{
    if (!HasServerAuthority())
        return;

    // BD characters: only allow setting to 0. Non-zero sets are silently
    // ignored — BD energy lives on BrokenDarknessManager::AbsorptionEnergy.
    if (bIsBrokenDarkness && NewEP > 0)
        return;

    // Resonators without a usable EP-spend target: only allow setting to 0.
    // Symmetric to ServerGainEnergy — pool is dormant until armed or
    // Evolution-primary.
    if (CharacterData &&
        CharacterData->CharacterClass == ECharacterClass::Resonator &&
        !HasUsableEPTarget() &&
        NewEP > 0)
    {
        return;
    }

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

bool UCharacterDataComponent::HasServerAuthority() const
{
    // In standalone mode (PIE, no networking), always have authority
    if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
    {
        return true;
    }

    return GetOwner() && GetOwner()->HasAuthority();
}

void UCharacterDataComponent::ServerResurrect(int32 HPToRestore)
{
    if (!HasServerAuthority() || bIsAlive)
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

// ========================================
// BROKEN DARKNESS STATE
// ========================================

bool UCharacterDataComponent::IsBrokenDarkness() const
{
    if (bIsBrokenDarkness)
    {
        return true;
    }
    if (CharacterData && CharacterData->InnateElement == ESpellElement::BrokenDarkness)
    {
        return true;
    }
    return false;
}

void UCharacterDataComponent::ServerSetBrokenDarkness(bool bNewState)
{
    if (!HasServerAuthority())
        return;

    if (bIsBrokenDarkness == bNewState)
        return;

    bIsBrokenDarkness = bNewState;

    // Zero EP on transition to BD — they use BrokenDarknessManager
    // absorption energy, not regular EP, going forward.
    if (bIsBrokenDarkness)
    {
        CurrentEP = 0;
        OnEPChanged.Broadcast(CurrentEP, MaxEP);
    }
}

void UCharacterDataComponent::OnRep_bIsBrokenDarkness()
{
    // Client-side: when the flag flips, ensure EP UI shows 0.
    // No state mutation here — server already cleared CurrentEP, replication
    // delivers it. Just broadcast so the panel re-paints.
    if (bIsBrokenDarkness)
    {
        OnEPChanged.Broadcast(CurrentEP, MaxEP);
    }
}

int32 UCharacterDataComponent::CalculateMaxHealth() const
{
    if (!CharacterData)
        return 100;

    // TODO: Implement actual HP formula
    return 100;
}

int32 UCharacterDataComponent::CalculateMaxEnergy() const
{
    if (!CharacterData)
        return 100;

    // TODO: Implement actual EP formula
    return 100;
}

UWeaponData *UCharacterDataComponent::GetActiveWeapon() const
{
    ULoadoutComponent *Loadout = GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
    if (Loadout)
    {
        return Loadout->GetActiveWeapon();
    }
    return nullptr;
}

bool UCharacterDataComponent::HasUsableEPTarget() const
{
    ULoadoutComponent *Loadout = GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
    if (!Loadout)
    {
        return false;
    }

    if (Loadout->GetActiveWeapon() != nullptr)
    {
        return true;
    }

    if (Loadout->GetPrimarySlotType() == EPrimarySlotType::Evolution)
    {
        return true;
    }

    return false;
}

// ==================== CRYSTAL-AWARE PILLAR VALUES ====================

namespace
{
    /** Shared body for the three GetCrystalModifiedX helpers. Looks up the
     *  primary-weapon-slot evolution crystal via the loadout, then applies
     *  UItemData's Pillar/SubStats modifier to the supplied base value. */
    enum class ECrystalPillar : uint8 { Mind, Body, Spirit };

    float ApplyCrystalPillarModifier(AActor *Owner, float BaseValue, ECrystalPillar Pillar)
    {
        if (!Owner)
        {
            return BaseValue;
        }
        ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>();
        if (!Loadout)
        {
            return BaseValue;
        }
        UItemData *Crystal = Loadout->GetActivePrimaryEvolutionCrystal(Owner);
        if (!Crystal || !Crystal->bIsEvolutionCrystal)
        {
            return BaseValue;
        }
        switch (Pillar)
        {
        case ECrystalPillar::Mind:   return Crystal->CalculateModifiedMind(BaseValue);
        case ECrystalPillar::Body:   return Crystal->CalculateModifiedBody(BaseValue);
        case ECrystalPillar::Spirit: return Crystal->CalculateModifiedSpirit(BaseValue);
        }
        return BaseValue;
    }
}

float UCharacterDataComponent::GetCrystalModifiedMind() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    return ApplyCrystalPillarModifier(GetOwner(), CharacterData->GetEffectiveMind(), ECrystalPillar::Mind);
}

float UCharacterDataComponent::GetCrystalModifiedBody() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    return ApplyCrystalPillarModifier(GetOwner(), CharacterData->GetEffectiveBody(), ECrystalPillar::Body);
}

float UCharacterDataComponent::GetCrystalModifiedSpirit() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    return ApplyCrystalPillarModifier(GetOwner(), CharacterData->GetEffectiveSpirit(), ECrystalPillar::Spirit);
}

void UCharacterDataComponent::DebugToggleWeapon()
{
    ULoadoutComponent *Loadout = GetOwner() ? GetOwner()->FindComponentByClass<ULoadoutComponent>() : nullptr;
    if (Loadout)
    {
        Loadout->ToggleEquipment();
        UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] Toggled weapon via LoadoutComponent"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterDataComponent] No LoadoutComponent found"));
    }
}