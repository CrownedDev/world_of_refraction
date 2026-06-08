// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Equipment/Weapons/EWeaponSlotType.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Character/StanceData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/GameInstance.h"

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
        // crystal-aware pillar reads in GetEvolutionModifiedBody/Spirit can see
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

        // HP/EP init — crystal-aware path with equipment bonus folded in.
        // Formula lives in RecomputeMaxPools so equipment-swap code can re-run
        // it without duplicating the math.
        RecomputeMaxPools();
        CurrentHP = MaxHP;
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

        // Broken Darkness gains energy only through absorption (no passive
        // regen), so resetting to MaxEP would violate the design rule. Reset
        // to 0 — a BD must absorb before it can cast.
        CurrentEP = IsBrokenDarkness() ? 0 : MaxEP;

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

    // BD characters do not gain passive-regen EP — their energy is event-driven
    // (parry/block absorption). The absorption gain path is
    // ServerGainBrokenDarknessEnergy, which deliberately bypasses this early-out.
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

    // BD characters: this generic setter only allows clearing to 0. BD energy
    // is mutated through ServerSpendEnergy (cast/drain) and
    // ServerGainBrokenDarknessEnergy (absorption, overload-aware).
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

void UCharacterDataComponent::ServerGainBrokenDarknessEnergy(int32 Amount, int32 AbsoluteMax)
{
    if (!HasServerAuthority() || Amount <= 0)
        return;

    // BD absorption — bypasses the ServerGainEnergy BD early-out and permits
    // CurrentEP to exceed MaxEP up to AbsoluteMax (MaxEP + OverloadCapacity,
    // supplied by BrokenDarknessManager) so the overload mechanic still fires.
    CurrentEP = FMath::Min(CurrentEP + Amount, AbsoluteMax);
    OnEPChanged.Broadcast(CurrentEP, MaxEP);
}

void UCharacterDataComponent::CheckDeath()
{
    if (CurrentHP <= 0 && bIsAlive)
    {
        // Revive intercept — when the actor holds a Revive skill-effect, restore
        // HP to 30% of MaxHP, consume the effect, and skip the death broadcast.
        if (UWorld *World = GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                if (USkillEffectManager *SEM = GI->GetSubsystem<USkillEffectManager>())
                {
                    if (AActor *Owner = GetOwner())
                    {
                        if (SEM->HasEffectOfType(Owner, ESkillEffectType::Revive))
                        {
                            CurrentHP = FMath::Max(1, FMath::RoundToInt(MaxHP * 0.3f));
                            SEM->RemoveEffectsByType(Owner, ESkillEffectType::Revive);
                            OnHPChanged.Broadcast(CurrentHP, MaxHP);
                            UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s revived at %d HP (30%% of MaxHP)"),
                                   *Owner->GetName(), CurrentHP);
                            return;
                        }
                    }
                }
            }
        }

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

ESpellElement UCharacterDataComponent::GetDisplayElement() const
{
    if (IsBrokenDarkness())
    {
        return ESpellElement::BrokenDarkness;
    }
    return CharacterData ? CharacterData->GetElement() : ESpellElement::Generic;
}

void UCharacterDataComponent::ServerSetBrokenDarkness(bool bNewState)
{
    if (!HasServerAuthority())
        return;

    if (bIsBrokenDarkness == bNewState)
        return;

    bIsBrokenDarkness = bNewState;

    // Energy carries over on transition to BD — whatever CurrentEP the
    // character held becomes their starting absorption buffer. Re-broadcast
    // OnEPChanged (value unchanged) so the panel relabels the bar EP -> Absorb.
    if (bIsBrokenDarkness)
    {
        OnEPChanged.Broadcast(CurrentEP, MaxEP);
    }
}

void UCharacterDataComponent::OnRep_bIsBrokenDarkness()
{
    // Client-side: when the flag flips, re-broadcast so the panel relabels the
    // energy bar (EP -> Absorb). CurrentEP carries over and replicates
    // independently — no state mutation here.
    if (bIsBrokenDarkness)
    {
        OnEPChanged.Broadcast(CurrentEP, MaxEP);
    }
}

int32 UCharacterDataComponent::CalculateMaxHealth() const
{
    if (!CharacterData)
        return 100;
    return CharacterData->CalculateMaxHealth();
}

int32 UCharacterDataComponent::CalculateMaxEnergy() const
{
    if (!CharacterData)
        return 100;
    return CharacterData->CalculateMaxEnergy();
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
    /** Shared body for the three GetEvolutionModifiedX helpers. Layers TWO
     *  pillar-modifier sources on top of the supplied base value:
     *   1. Crystal — primary-weapon-slot evolution crystal's Pillar/SubStats
     *      modifier via UEvolutionItemData::CalculateModified{Mind,Body,Spirit}.
     *   2. Equipment — active loadout's BonusMindModifierPercent /
     *      BonusBodyModifierPercent / BonusSpiritModifierPercent, applied
     *      multiplicatively after the crystal layer.
     *  Either layer is independently optional; if no crystal is slotted the
     *  equipment percent still applies. */
    enum class ECrystalPillar : uint8 { Mind, Body, Spirit };

    float ApplyEvolutionPillarModifier(AActor *Owner, float BaseValue, ECrystalPillar Pillar)
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

        // Crystal layer — read pillar percent directly from the crystal's
        // BaseStatBonus (the new canonical authoring surface; old CalculateModified*
        // helpers were removed in the BaseStatBonus migration).
        // Case A: evolution attached to the active weapon (PrimarySlotType==Weapon).
        // Case B: evolution slotted as the primary slot itself (PrimarySlotType==Evolution,
        // e.g. Broken Darkness). The two cases are mutually exclusive on PrimarySlotType,
        // so the else-if cannot double-apply.
        float Result = BaseValue;
        if (UEvolutionItemData *Crystal = Loadout->GetActivePrimaryEvolutionCrystal(Owner))
        {
            float CrystalPercent = 0.0f;
            switch (Pillar)
            {
            case ECrystalPillar::Mind:   CrystalPercent = Crystal->BaseStatBonus.BonusMindModifierPercent;   break;
            case ECrystalPillar::Body:   CrystalPercent = Crystal->BaseStatBonus.BonusBodyModifierPercent;   break;
            case ECrystalPillar::Spirit: CrystalPercent = Crystal->BaseStatBonus.BonusSpiritModifierPercent; break;
            }
            Result *= (1.0f + CrystalPercent / CombatConstants::STAT_PERCENT_DIVISOR);
        }
        else
        {
            const FCombatLoadout Active = Loadout->GetActiveLoadout();
            if (Active.PrimarySlotType == EPrimarySlotType::Evolution && Active.PrimaryEvolution.Item)
            {
                UEvolutionItemData *PrimaryEvo = Active.PrimaryEvolution.Item;
                float CrystalPercent = 0.0f;
                switch (Pillar)
                {
                case ECrystalPillar::Mind:   CrystalPercent = PrimaryEvo->BaseStatBonus.BonusMindModifierPercent;   break;
                case ECrystalPillar::Body:   CrystalPercent = PrimaryEvo->BaseStatBonus.BonusBodyModifierPercent;   break;
                case ECrystalPillar::Spirit: CrystalPercent = PrimaryEvo->BaseStatBonus.BonusSpiritModifierPercent; break;
                }
                Result *= (1.0f + CrystalPercent / CombatConstants::STAT_PERCENT_DIVISOR);
            }
        }

        // Equipment layer — multiplicative percent on top of the crystal layer.
        const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Owner);
        float EquipmentPercent = 0.0f;
        switch (Pillar)
        {
        case ECrystalPillar::Mind:   EquipmentPercent = Bonus.BonusMindModifierPercent;   break;
        case ECrystalPillar::Body:   EquipmentPercent = Bonus.BonusBodyModifierPercent;   break;
        case ECrystalPillar::Spirit: EquipmentPercent = Bonus.BonusSpiritModifierPercent; break;
        }
        Result *= (1.0f + EquipmentPercent / CombatConstants::STAT_PERCENT_DIVISOR);

        return Result;
    }
}

float UCharacterDataComponent::GetEvolutionModifiedMind() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    return ApplyEvolutionPillarModifier(GetOwner(), CharacterData->GetEffectiveMind(), ECrystalPillar::Mind);
}

float UCharacterDataComponent::GetEvolutionModifiedBody() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    return ApplyEvolutionPillarModifier(GetOwner(), CharacterData->GetEffectiveBody(), ECrystalPillar::Body);
}

float UCharacterDataComponent::GetEvolutionModifiedSpirit() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    return ApplyEvolutionPillarModifier(GetOwner(), CharacterData->GetEffectiveSpirit(), ECrystalPillar::Spirit);
}

void UCharacterDataComponent::RecomputeMaxPools()
{
    if (!CharacterData)
    {
        return;
    }

    int32 BonusMaxHP = 0;
    int32 BonusMaxEnergy = 0;
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Owner);
            BonusMaxHP = Bonus.BonusMaxHP;
            BonusMaxEnergy = Bonus.BonusMaxEnergy;
        }
    }

    const float ModifiedBody = GetEvolutionModifiedBody();
    MaxHP = FMath::RoundToInt(
        CombatConstants::MAX_HEALTH_BASE +
        (ModifiedBody * CharacterData->GetTotalMaxHealth() * CombatConstants::MAX_HEALTH_PER_POINT))
        + BonusMaxHP;

    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    MaxEP = FMath::RoundToInt(
        CombatConstants::MAX_ENERGY_BASE +
        (ModifiedSpirit * CharacterData->GetTotalMaxEnergy() * CombatConstants::MAX_ENERGY_PER_POINT))
        + BonusMaxEnergy;
}

float UCharacterDataComponent::GetEquipmentModifiedLuck() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }

    // Crystal-aware Luck: pillar-scaled against GetEvolutionModifiedSpirit
    // instead of the raw asset's GetEffectiveSpirit. Mirrors the asset's
    // CalculateLuck formula shape (no LUCK_BASE constant — bare per-point).
    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const int32 LuckPoints = CharacterData->GetTotalLuck();
    float Luck = ModifiedSpirit * LuckPoints * CombatConstants::LUCK_PER_POINT;

    // Equipment stat bonus — additive per-point on top of the pillar term.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Owner);
            Luck += Bonus.BonusLuck * CombatConstants::LUCK_PER_POINT;
        }

        // Skill-effect-driven LuckBuff / LuckDebuff (flat additive, percent-space).
        if (UWorld *World = GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                if (USkillEffectManager *SkillMgr = GI->GetSubsystem<USkillEffectManager>())
                {
                    const float LuckBuff = SkillMgr->GetTotalStatModifier(Owner, ESkillEffectType::LuckBuff);
                    const float LuckDebuff = SkillMgr->GetTotalStatModifier(Owner, ESkillEffectType::LuckDebuff);
                    Luck += (LuckBuff - LuckDebuff);
                }
            }
        }
    }

    return FMath::Min(Luck, CombatConstants::LUCK_RAW_MAX);
}

float UCharacterDataComponent::GetEvolutionModifiedSpellDamage() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }
    const float ModifiedMind = GetEvolutionModifiedMind();
    const int32 TotalPoints = CharacterData->GetTotalSpellDamage();
    return 1.0f + (ModifiedMind * TotalPoints * CombatConstants::SPELL_DAMAGE_PER_POINT);
}

float UCharacterDataComponent::GetEvolutionModifiedRawDamage() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }
    const float ModifiedBody = GetEvolutionModifiedBody();
    const int32 TotalPoints = CharacterData->GetTotalRawDamage();
    return 1.0f + (ModifiedBody * TotalPoints * CombatConstants::RAW_DAMAGE_PER_POINT);
}

float UCharacterDataComponent::GetEvolutionModifiedCritChance() const
{
    if (!CharacterData)
    {
        return CombatConstants::CRIT_CHANCE_BASE;
    }
    const float ModifiedMind = GetEvolutionModifiedMind();
    const int32 TotalPoints = CharacterData->GetTotalCritChance();
    return FMath::Clamp(
        CombatConstants::CRIT_CHANCE_BASE + (ModifiedMind * TotalPoints * CombatConstants::CRIT_CHANCE_PER_POINT),
        CombatConstants::CRIT_CHANCE_BASE,
        1.0f);
}

int32 UCharacterDataComponent::GetEvolutionModifiedFlatDefense() const
{
    if (!CharacterData)
    {
        return 0;
    }
    const float ModifiedBody = GetEvolutionModifiedBody();
    const int32 TotalPoints = CharacterData->GetTotalDefense();
    return FMath::RoundToInt(ModifiedBody * TotalPoints * CombatConstants::DEFENSE_PER_POINT);
}

float UCharacterDataComponent::GetEvolutionModifiedSpellDamageForHealing() const
{
    return GetEvolutionModifiedSpellDamage();
}

float UCharacterDataComponent::GetEffectiveEfficiencyMultiplier() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }

    // Innate reduction — the crystal-aware Mind × Efficiency-points cost-reduction term.
    const float ModifiedMind = GetEvolutionModifiedMind();
    const int32 TotalPoints = CharacterData->GetTotalEfficiency();
    float Reduction = ModifiedMind * TotalPoints * CombatConstants::EFFICIENCY_PER_POINT;

    // Equipment BonusEfficiency + attached EfficiencyStone — both from the owner's
    // active loadout (one lookup, reused). Same BonusEfficiency the EP-cost
    // EquipmentMult reads; stone is the owner's active-weapon attachment. Each adds 0
    // when absent, so the result stays byte-identical to the canonical getter for a
    // character with no BonusEfficiency and no EfficiencyStone.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            Reduction += Loadout->GetActiveStatBonus(Owner).BonusEfficiency * CombatConstants::EFFICIENCY_PER_POINT;

            if (const FWeaponLoadoutEntry *ActiveWeapon = Loadout->GetActiveWeaponLoadout())
            {
                const FRuntimeAttachedItem &Att = ActiveWeapon->WeaponEntry.GetAttachedItem();
                Reduction += CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::Efficiency)
                             / CombatConstants::STAT_PERCENT_DIVISOR;
            }
        }

        // Transient Efficiency buff/debuff — skill-effect layer (mirrors StatusMultiplier's
        // transient accessor). A BUFF adds to the reduction → smaller multiplier → cheaper
        // EP / slower BD drain / less wear (the favourable direction). 0 when none active,
        // so byte-identical to the pre-H0 getter then. Distinct effect type from EP's
        // SpellCostBuff/Debuff, so no double-count with the EP SkillEffectMult layer.
        if (UWorld *World = GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                if (USkillEffectManager *SEM = GI->GetSubsystem<USkillEffectManager>())
                {
                    const float EffBuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::EfficiencyBuff);
                    const float EffDebuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::EfficiencyDebuff);
                    Reduction += (EffBuff - EffDebuff) / CombatConstants::STAT_PERCENT_DIVISOR;
                }
            }
        }
    }

    return FMath::Clamp(1.0f - Reduction, 1.0f - CombatConstants::EFFICIENCY_MAX, 1.0f);
}

float UCharacterDataComponent::GetEvolutionModifiedStatusMultiplier() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }
    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const int32 TotalPoints = CharacterData->GetTotalStatusMultiplier();
    return 1.0f + (ModifiedSpirit * TotalPoints * CombatConstants::STATUS_MULTIPLIER_PER_POINT);
}

float UCharacterDataComponent::GetEvolutionModifiedResistance() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const int32 TotalPoints = CharacterData->GetTotalResistance();
    return FMath::Clamp(
        ModifiedSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT,
        0.0f,
        CombatConstants::RESISTANCE_MAX);
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