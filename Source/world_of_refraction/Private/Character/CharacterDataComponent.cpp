// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Equipment/Weapons/EWeaponSlotType.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Character/StanceData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/ItemConstants.h"
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

    // Subscribe to pool-affecting effect changes so MaxHP/MaxEP recompute when a transient
    // MaxHP/MaxEnergy buff/debuff applies or expires (Max is a stored field — it won't
    // update otherwise). Gated to this owner + pool types in the handler. Mirrors how BD
    // binds OnEPChanged; unbound in EndPlay.
    if (USkillEffectManager *SEM = GetSkillEffectManager())
    {
        SEM->OnEffectApplied.AddDynamic(this, &UCharacterDataComponent::HandlePoolEffectChanged);
        SEM->OnEffectRemoved.AddDynamic(this, &UCharacterDataComponent::HandlePoolEffectChanged);
    }
}

void UCharacterDataComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (USkillEffectManager *SEM = GetSkillEffectManager())
    {
        SEM->OnEffectApplied.RemoveDynamic(this, &UCharacterDataComponent::HandlePoolEffectChanged);
        SEM->OnEffectRemoved.RemoveDynamic(this, &UCharacterDataComponent::HandlePoolEffectChanged);
    }
    Super::EndPlay(EndPlayReason);
}

namespace
{
    // The transient pool effect types — a change to any of these on this actor means the
    // stored MaxHP/MaxEP must be recomputed. Everything else is ignored (no recompute).
    bool IsPoolEffect(ESkillEffectType Type)
    {
        return Type == ESkillEffectType::MaxHPBuff || Type == ESkillEffectType::MaxHPDebuff ||
               Type == ESkillEffectType::MaxEnergyBuff || Type == ESkillEffectType::MaxEnergyDebuff;
    }
}

void UCharacterDataComponent::HandlePoolEffectChanged(AActor *Target, const FActiveSkillEffect &Effect)
{
    // Only recompute for THIS owner and only for pool-affecting effects — unrelated
    // effects (the vast majority) must not trigger a pool recompute. RecomputeMaxPools
    // writes only Max; CurrentHP/EP are untouched (overcap on a debuff/expiry, no heal
    // on a buff) — consistent with P0/P1 and the overcap-not-clamp design.
    if (Target == GetOwner() && IsPoolEffect(Effect.EffectType))
    {
        RecomputeMaxPools();
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
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            if (AActor *Owner = GetOwner())
            {
                if (SEM->HasEffectOfType(Owner, ESkillEffectType::Revive))
                {
                    CurrentHP = FMath::Max(1, FMath::RoundToInt(MaxHP * ItemConstants::REVIVE_HP_PERCENT));
                    SEM->RemoveEffectsByType(Owner, ESkillEffectType::Revive);
                    OnHPChanged.Broadcast(CurrentHP, MaxHP);
                    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s revived at %d HP (30%% of MaxHP)"),
                           *Owner->GetName(), CurrentHP);
                    return;
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
    /** Shared body for the three GetEvolutionModifiedX helpers. Layers THREE
     *  pillar-modifier sources, each multiplicative, on top of the supplied base value:
     *   1. Crystal — primary slot's evolution crystal pillar percent, read from its
     *      BaseStatBonus.Bonus{Mind,Body,Spirit}ModifierPercent.
     *   2. Equipment — active loadout's BonusMindModifierPercent /
     *      BonusBodyModifierPercent / BonusSpiritModifierPercent, applied
     *      multiplicatively after the crystal layer.
     *   3. Transient — pillar-wide skill-effect buff/debuff (MindBuff/MindDebuff etc.),
     *      the broad "scale all of this pillar's sub-stats" channel, with a Max(0,…)
     *      negative floor.
     *  The composed product of the three factors is then hard-capped to
     *  [STAT_MODIFIER_MIN, STAT_MODIFIER_MAX] = [0, 2] (the [-100%,+100%] normalization
     *  model) before applying to the base. Every layer is independently optional; each
     *  collapses to ×1.0 when absent, so a character with none is byte-identical to the
     *  raw base value (the cap is inert below 2.0). */
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
        // Compose the MODIFIER (product of the layer factors) separately from BaseValue, so
        // the [STAT_MODIFIER_MIN, STAT_MODIFIER_MAX] cap below applies to the modifier alone —
        // the per-character base (the source of build uniqueness) is never clamped.
        // U3c: pillar percent = asset Base (authored, stays) + the slotted
        // attachment's Generated (the per-instance roll) — rolled pillars are
        // PERMANENT (always-on), inheriting Base pillars' permanence per the
        // locked option-(a) split. All-zero Generated falls back to Base only.
        float Modifier = 1.0f;
        const FEvolutionAttachment WeaponEvo = Loadout->GetActivePrimaryEvolutionAttachment(Owner);
        if (WeaponEvo.Item)
        {
            float CrystalPercent = 0.0f;
            switch (Pillar)
            {
            case ECrystalPillar::Mind:   CrystalPercent = WeaponEvo.Item->BaseStatBonus.BonusMindModifierPercent   + WeaponEvo.GeneratedStatBonus.BonusMindModifierPercent;   break;
            case ECrystalPillar::Body:   CrystalPercent = WeaponEvo.Item->BaseStatBonus.BonusBodyModifierPercent   + WeaponEvo.GeneratedStatBonus.BonusBodyModifierPercent;   break;
            case ECrystalPillar::Spirit: CrystalPercent = WeaponEvo.Item->BaseStatBonus.BonusSpiritModifierPercent + WeaponEvo.GeneratedStatBonus.BonusSpiritModifierPercent; break;
            }
            Modifier *= (1.0f + CrystalPercent / CombatConstants::STAT_PERCENT_DIVISOR);
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
                case ECrystalPillar::Mind:   CrystalPercent = PrimaryEvo->BaseStatBonus.BonusMindModifierPercent   + Active.PrimaryEvolution.GeneratedStatBonus.BonusMindModifierPercent;   break;
                case ECrystalPillar::Body:   CrystalPercent = PrimaryEvo->BaseStatBonus.BonusBodyModifierPercent   + Active.PrimaryEvolution.GeneratedStatBonus.BonusBodyModifierPercent;   break;
                case ECrystalPillar::Spirit: CrystalPercent = PrimaryEvo->BaseStatBonus.BonusSpiritModifierPercent + Active.PrimaryEvolution.GeneratedStatBonus.BonusSpiritModifierPercent; break;
                }
                Modifier *= (1.0f + CrystalPercent / CombatConstants::STAT_PERCENT_DIVISOR);
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
        Modifier *= (1.0f + EquipmentPercent / CombatConstants::STAT_PERCENT_DIVISOR);

        // Transient layer — pillar-wide buff/debuff (MindBuff/BodyBuff/SpiritBuff and their
        // Debuffs): the broad "scale ALL of this pillar's sub-stats" skill-effect channel.
        // Distinct effect types from the stat-specific transients (SpellDamageBuff etc.),
        // which scale the derived OUTPUT — these scale the pillar INPUT, so both apply with
        // no double-count. Multiplicative ×(1+(buff−debuff)/100), matching the crystal +
        // equipment layers and GetTransientSpellDamageFactor's source pattern. The Max(0,…)
        // floor prevents a ≥100% debuff inverting THIS layer; the composed-product cap below
        // bounds the COMBINED modifier. Collapses to ×1.0 when no pillar buff/debuff is
        // active, so a bare character stays byte-identical.
        if (UWorld *World = Owner->GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                if (USkillEffectManager *SEM = GI->GetSubsystem<USkillEffectManager>())
                {
                    ESkillEffectType BuffType = ESkillEffectType::MindBuff;
                    ESkillEffectType DebuffType = ESkillEffectType::MindDebuff;
                    switch (Pillar)
                    {
                    case ECrystalPillar::Mind:   BuffType = ESkillEffectType::MindBuff;   DebuffType = ESkillEffectType::MindDebuff;   break;
                    case ECrystalPillar::Body:   BuffType = ESkillEffectType::BodyBuff;   DebuffType = ESkillEffectType::BodyDebuff;   break;
                    case ECrystalPillar::Spirit: BuffType = ESkillEffectType::SpiritBuff; DebuffType = ESkillEffectType::SpiritDebuff; break;
                    }
                    const float PillarBuff = SEM->GetTotalStatModifier(Owner, BuffType);
                    const float PillarDebuff = SEM->GetTotalStatModifier(Owner, DebuffType);
                    Modifier *= FMath::Max(0.0f, 1.0f + (PillarBuff - PillarDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
                }
            }
        }

        // [-100%, +100%] normalization — hard-cap the COMPOSED modifier (crystal × equipment
        // × transient) to [0, 2], additional to the inner Max(0,…) transient floor. Byte-
        // identical below 2.0 (a bare/normally-geared character's product is < 2.0, so the
        // clamp is inert). The lower bound (0) is the safety floor: the transient is already
        // Max(0,…)-floored, but a ≤−100% crystal/equipment modifier percent would make its
        // factor negative — the Clamp guarantees the modifier never goes below 0 regardless.
        Modifier = FMath::Clamp(Modifier, CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
        return BaseValue * Modifier;
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
    // Pool % bonus = attached pool stone (P1) + transient pool buff/debuff (P2b), summed
    // into one factor (additive in the %, like the StatusMultiplier getter sums buff-debuff).
    float HPPct = 0.0f;
    float EPPct = 0.0f;
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Owner);
            BonusMaxHP = Bonus.BonusMaxHP;
            BonusMaxEnergy = Bonus.BonusMaxEnergy;

            // Attached pool stones — a % of the computed max, read from the active weapon
            // attachment (0 unless the matching stone is attached).
            if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
            {
                const FRuntimeAttachedItem &Att = *AttPtr;
                HPPct = CrystalEffectTable::GetAttachedStonePercentForType(Att, ECrystalType::MaxHPStone);
                EPPct = CrystalEffectTable::GetAttachedStonePercentForType(Att, ECrystalType::MaxEPStone);
            }
        }

        // Transient pool buff/debuff (P2b) — skill-effect layer, summed via
        // GetTotalStatModifier (same SEM idiom as the Luck/Efficiency reads in this file).
        // Added to the stone % so both stack additively; 0 when none active or SEM
        // unavailable (byte-neutral vs P1). MaxHP/MaxEnergy Buff raise, Debuff lower.
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            HPPct += SEM->GetTotalStatModifier(Owner, ESkillEffectType::MaxHPBuff) -
                     SEM->GetTotalStatModifier(Owner, ESkillEffectType::MaxHPDebuff);
            EPPct += SEM->GetTotalStatModifier(Owner, ESkillEffectType::MaxEnergyBuff) -
                     SEM->GetTotalStatModifier(Owner, ESkillEffectType::MaxEnergyDebuff);
        }
    }

    // Subtotal (base + pillar points + equipment flat), then the attached pool stone
    // applies as a final % of the WHOLE max — single RoundToInt on the product (no
    // double-round). Byte-neutral when no stone (pct 0 -> x1.0; BonusMax* is integer, so
    // folding it inside the round doesn't change the result).
    // Intrinsic stat portion (base + pillar points) is clamped to the design cap so max
    // investment lands exactly on it (HP/EP 1000). Gear — flat BonusMax* + stone/buff % —
    // stacks OUTSIDE the clamp (gear headroom above the stat cap; cluster 2).
    const float ModifiedBody = GetEvolutionModifiedBody();
    const float HPStatPortion = FMath::Min(
        CombatConstants::MAX_HEALTH_BASE +
            (ModifiedBody * CharacterData->GetTotalMaxHealth() * CombatConstants::MAX_HEALTH_PER_POINT),
        CombatConstants::MAX_HEALTH_CAP);
    const float HPSubtotal = HPStatPortion + BonusMaxHP;
    MaxHP = FMath::RoundToInt(HPSubtotal * (1.0f + HPPct / CombatConstants::STAT_PERCENT_DIVISOR));

    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const float EPStatPortion = FMath::Min(
        CombatConstants::MAX_ENERGY_BASE +
            (ModifiedSpirit * CharacterData->GetTotalMaxEnergy() * CombatConstants::MAX_ENERGY_PER_POINT),
        CombatConstants::MAX_ENERGY_CAP);
    const float EPSubtotal = EPStatPortion + BonusMaxEnergy;
    MaxEP = FMath::RoundToInt(EPSubtotal * (1.0f + EPPct / CombatConstants::STAT_PERCENT_DIVISOR));
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
        if (USkillEffectManager *SkillMgr = GetSkillEffectManager())
        {
            const float LuckBuff = SkillMgr->GetTotalStatModifier(Owner, ESkillEffectType::LuckBuff);
            const float LuckDebuff = SkillMgr->GetTotalStatModifier(Owner, ESkillEffectType::LuckDebuff);
            Luck += (LuckBuff - LuckDebuff);
        }
    }

    // No upper clamp: input is unbounded, each consumer clamps its own
    // normalized fraction (RawLuck / LUCK_RAW_MAX) to [0,1] before scaling.
    return Luck;
}

float UCharacterDataComponent::GetLuckModifiedChance(float BaseChance, float LuckMaxBonus) const
{
    // Normalized Luck in (-inf, 1]: upper-clamped to 1 (positive luck plateaus at LuckMaxBonus);
    // negatives pass through (curse). Byte-identical to the inline pattern every Luck consumer used
    // (FMath::Min(RawLuck / LUCK_RAW_MAX, 1) * MAX), now centralized.
    const float Norm = FMath::Min(GetEquipmentModifiedLuck() / CombatConstants::LUCK_RAW_MAX, 1.0f);
    return BaseChance + Norm * LuckMaxBonus;
}

bool UCharacterDataComponent::RollLuckChance(float BaseChance, float LuckMaxBonus) const
{
    // A negative chance (cursed wielder, BaseChance 0) is never < FRand()'s [0,1) — never fires.
    return FMath::FRand() < GetLuckModifiedChance(BaseChance, LuckMaxBonus);
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
    // Pattern P (cluster 5d): the STAT crit base caps ALONE at UNIVERSAL_STAT_CAP (0.5 — now ENFORCED;
    // was wrongly 1.0). Gear (BonusCritChance, CritStone) then MULTIPLIES this past 0.5 toward the
    // final [0,1] ceiling in GetCriticalChance — that gear-beyond layer is already Pattern-P-shaped.
    return FMath::Clamp(
        CombatConstants::CRIT_CHANCE_BASE + (ModifiedMind * TotalPoints * CombatConstants::CRIT_CHANCE_PER_POINT),
        CombatConstants::CRIT_CHANCE_BASE,
        CombatConstants::UNIVERSAL_STAT_CAP);
}

float UCharacterDataComponent::GetEvolutionModifiedFlatDefense() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    // Crystal-aware defense REDUCTION fraction [0, 0.5] (cluster 4: flat-int -> capped %).
    // GetEvolutionModifiedBody feeds the slotted crystal's Body pillar into the curve.
    // Name kept for now (BP/.uasset refs); TODO rename to GetEvolutionModifiedDefenseReduction.
    const float ModifiedBody = GetEvolutionModifiedBody();
    const int32 TotalPoints = CharacterData->GetTotalDefense();
    return FMath::Min(ModifiedBody * TotalPoints * CombatConstants::DEFENSE_PER_POINT, CombatConstants::UNIVERSAL_STAT_CAP);
}

float UCharacterDataComponent::GetEquipmentSpellDamageTerm() const
{
    // L2 — additive equipment contribution. Byte-identical to the inline term in
    // DamageCalculator Step 1 (Bonus.BonusSpellDamage × SPELL_DAMAGE_PER_POINT).
    // 0 when the owner has no loadout / no BonusSpellDamage. Sourced exactly like
    // GetEffectiveEfficiencyMultiplier's equipment read.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            return Loadout->GetActiveStatBonus(Owner).BonusSpellDamage * CombatConstants::SPELL_DAMAGE_PER_POINT;
        }
    }
    return 0.0f;
}

float UCharacterDataComponent::GetStoneSpellDamageFactor() const
{
    // L3 — fusion-aware attached-stone multiplier (1 + stone%/100). Byte-identical to
    // DamageCalculator Step 1.25b: GetAttachedStonePercent returns 0 for any non-stone /
    // non-fusion attachment (and the 1.0 fallbacks cover no-loadout / no-active-weapon),
    // so the factor collapses to 1.0 exactly as the inline guard's skip did.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
            {
                const FRuntimeAttachedItem &Att = *AttPtr;
                return 1.0f + CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::SpellDamage) / CombatConstants::STAT_PERCENT_DIVISOR;
            }
        }
    }
    return 1.0f;
}

float UCharacterDataComponent::GetTransientSpellDamageFactor() const
{
    // L4 — transient buff/debuff multiplier (1 + (SpellDamageBuff − SpellDamageDebuff)/100).
    // Byte-identical to the spell arm of GetStatusEffectDamageModifier (same SEM reads, same
    // actor). 1.0 when none active or SEM unavailable. Also carries the Amethyst gamble's
    // spell-damage arm, since that emits SpellDamageBuff.
    if (AActor *Owner = GetOwner())
    {
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            const float SpellBuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::SpellDamageBuff);
            const float SpellDebuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::SpellDamageDebuff);
            return 1.0f + (SpellBuff - SpellDebuff) / CombatConstants::STAT_PERCENT_DIVISOR;
        }
    }
    return 1.0f;
}

float UCharacterDataComponent::GetEffectiveSpellDamage() const
{
    // Full layered SpellDamage scalar for non-pipeline consumers (Broken Darkness):
    //   (L1 innate/evolution capped ALONE + L2 equipment) × L3 stone × L4 transient.
    // No ActionMods / Grid / defender — those are damage-call-specific and have no
    // analogue here. DamageCalculator DRY-sources the SAME helpers at its own step order
    // (Pattern P), so a normal cast stays byte-identical.
    // Pattern P (cluster 5a): the STAT term is capped ALONE at STAT_MULT_CAP (×1.5 — the stat
    // ceiling), THEN gear MULTIPLIES it (×(1+EquipTerm)) and stone/transient apply OUTSIDE that
    // clamp, bounded by the higher STAT_MODIFIER_MAX (×2.0) compose ceiling. Stat saturates at ×1.5;
    // gear/stone/buff scale it from there toward ×2.0. EquipTerm read as a FRACTION (option ii).
    // Byte-identical below the caps with no gear.
    const float StatBase = FMath::Min(GetEvolutionModifiedSpellDamage(), CombatConstants::STAT_MULT_CAP);
    const float Composed = StatBase * (1.0f + GetEquipmentSpellDamageTerm()) * GetStoneSpellDamageFactor() * GetTransientSpellDamageFactor();
    return FMath::Clamp(Composed, CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
}

float UCharacterDataComponent::GetEquipmentRawDamageTerm() const
{
    // L2 — additive equipment contribution. Byte-identical to the inline term in
    // DamageCalculator Step 1 (Bonus.BonusRawDamage × RAW_DAMAGE_PER_POINT). 0 when the
    // owner has no loadout / no BonusRawDamage. Physical mirror of GetEquipmentSpellDamageTerm.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            return Loadout->GetActiveStatBonus(Owner).BonusRawDamage * CombatConstants::RAW_DAMAGE_PER_POINT;
        }
    }
    return 0.0f;
}

float UCharacterDataComponent::GetStoneRawDamageFactor() const
{
    // L3 — fusion-aware attached-stone multiplier (1 + stone%/100). Byte-identical to
    // DamageCalculator Step 1.25: GetAttachedStonePercent returns 0 for any non-stone /
    // non-fusion attachment (and the 1.0 fallbacks cover no-loadout / no-active-weapon), so
    // the factor collapses to 1.0 exactly as the inline IsAugmentStone()||IsFusion() skip did.
    // Physical mirror of GetStoneSpellDamageFactor.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
            {
                const FRuntimeAttachedItem &Att = *AttPtr;
                return 1.0f + CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::RawDamage) / CombatConstants::STAT_PERCENT_DIVISOR;
            }
        }
    }
    return 1.0f;
}

float UCharacterDataComponent::GetTransientRawDamageFactor() const
{
    // L4 — transient buff/debuff multiplier (1 + (RawDamageBuff − RawDamageDebuff)/100).
    // Byte-identical to the physical arm of GetStatusEffectDamageModifier (same SEM reads,
    // same actor). 1.0 when none active or SEM unavailable. Physical mirror of
    // GetTransientSpellDamageFactor; also carries the Amethyst gamble's physical-damage arm.
    if (AActor *Owner = GetOwner())
    {
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            const float RawBuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::RawDamageBuff);
            const float RawDebuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::RawDamageDebuff);
            return 1.0f + (RawBuff - RawDebuff) / CombatConstants::STAT_PERCENT_DIVISOR;
        }
    }
    return 1.0f;
}

float UCharacterDataComponent::GetEffectiveRawDamage() const
{
    // Physical mirror of GetEffectiveSpellDamage — Pattern P (cluster 5a): the STAT term is capped
    // ALONE at STAT_MULT_CAP (×1.5), THEN gear MULTIPLIES it (×(1+EquipTerm)) and stone/transient
    // apply OUTSIDE that clamp, bounded by the higher STAT_MODIFIER_MAX (×2.0) compose ceiling. No
    // ActionMods / Grid / defender (damage-call-specific). The pipeline DRY-sources the same helpers
    // (Step 2.6). EquipTerm read as a FRACTION (option ii). Byte-identical below the caps with no gear.
    const float StatBase = FMath::Min(GetEvolutionModifiedRawDamage(), CombatConstants::STAT_MULT_CAP);
    const float Composed = StatBase * (1.0f + GetEquipmentRawDamageTerm()) * GetStoneRawDamageFactor() * GetTransientRawDamageFactor();
    return FMath::Clamp(Composed, CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
}

FEffectiveStats UCharacterDataComponent::GetEffectiveStats() const
{
    // Pure snapshot — each field is the verbatim return of an existing getter, no
    // recompute / reorder. SpellDamage is the COMPOSED scalar, read by BD and the
    // crystal-wear power term (NOT the damage pipeline — see struct doc).
    FEffectiveStats Stats;
    Stats.SpellDamage          = GetEffectiveSpellDamage();
    Stats.StatusMultiplier     = GetEffectiveStatusMultiplier();
    Stats.EfficiencyMultiplier = GetEffectiveEfficiencyMultiplier();
    Stats.Resistance           = GetEffectiveResistance();
    return Stats;
}

USkillEffectManager *UCharacterDataComponent::GetSkillEffectManager() const
{
    // Lazy re-resolve: re-fetch whenever the cache is null, so a fresh component in a
    // new PIE session starts clean. Returns the same subsystem pointer the prior inline
    // GetWorld()->GetGameInstance()->GetSubsystem<USkillEffectManager>() reads did.
    if (!CachedSkillEffectManager)
    {
        if (UWorld *World = GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                CachedSkillEffectManager = GI->GetSubsystem<USkillEffectManager>();
            }
        }
    }
    return CachedSkillEffectManager;
}

float UCharacterDataComponent::GetEffectiveEfficiencyMultiplier() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }

    // Pattern P (cluster 5b) — stat-capped, gear multiplies beyond. Efficiency is INVERTED: the stat
    // produces a cost REDUCTION (0 to 0.5) and the final multiplier = (1 − reduction), so a larger
    // reduction = a smaller multiplier = cheaper EP / slower BD drain / less wear. The crystal-aware
    // Mind × Efficiency-points stat reduction is capped ALONE at EFFICIENCY_MAX (0.5); THEN
    // gear/stone/buff MULTIPLY it past 0.5 toward EFFICIENCY_GEAR_CEILING (cheaper still on
    // high-Efficiency builds). Mirrors 5a's damage/Defense shape.
    const float ModifiedMind = GetEvolutionModifiedMind();
    const int32 TotalPoints = CharacterData->GetTotalEfficiency();
    float Reduction = FMath::Min(ModifiedMind * TotalPoints * CombatConstants::EFFICIENCY_PER_POINT,
                                 CombatConstants::EFFICIENCY_MAX);

    // Equipment BonusEfficiency + attached EfficiencyStone — both from the owner's active loadout
    // (one lookup, reused). Each MULTIPLIES the capped stat reduction (×(1+fraction)) OUTSIDE the 0.5
    // cap — option-(ii) interpretation (same as 5a damage gear): the BonusEfficiency × per-point
    // magnitude is read as a fraction. ×1 (inert) when absent, so byte-identical with no gear.
    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            Reduction *= (1.0f + Loadout->GetActiveStatBonus(Owner).BonusEfficiency * CombatConstants::EFFICIENCY_PER_POINT);

            if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
            {
                const FRuntimeAttachedItem &Att = *AttPtr;
                Reduction *= (1.0f + CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::Efficiency)
                              / CombatConstants::STAT_PERCENT_DIVISOR);
            }
        }

        // Transient Efficiency buff/debuff — skill-effect layer. MULTIPLIES the reduction (a buff
        // scales it up → cheaper; a debuff down). ×1 when none active, so byte-identical then.
        // Distinct effect type from EP's SpellCostBuff/Debuff, so no double-count with the EP
        // SkillEffectMult layer.
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            const float EffBuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::EfficiencyBuff);
            const float EffDebuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::EfficiencyDebuff);
            Reduction *= (1.0f + (EffBuff - EffDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
        }
    }

    // Pattern-P gear ceiling: stat alone capped at 0.5 above; gear multiplied past toward the higher
    // EFFICIENCY_GEAR_CEILING. A net debuff can drive Reduction below 0; the clamp floors it at 0.
    Reduction = FMath::Clamp(Reduction, 0.0f, CombatConstants::EFFICIENCY_GEAR_CEILING);
    return 1.0f - Reduction;
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

float UCharacterDataComponent::GetEffectiveStatusMultiplier() const
{
    // Base (innate + equipment + attached StatusStone), composed inline to MATCH
    // UStatusBuildupManager::GetSourceStatusMultiplierFactor term-for-term (StatusMultiplier
    // SUMS, never compounds), then the transient buff/debuff compounded on top — so this
    // equals the BD/CombatOrchestrator inline value (base × transient) BY CONSTRUCTION.
    // We do NOT re-point BD; this getter just gives wear (and future consumers) the same
    // composed value. ⚠️ Mirrors GetSourceStatusMultiplierFactor — if its layer set changes,
    // update here too. Structurally parallel to GetEffectiveEfficiencyMultiplier.
    float Factor = GetEvolutionModifiedStatusMultiplier(); // innate: 1 + ModifiedSpirit × points × per-pt

    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            Factor += Loadout->GetActiveStatBonus(Owner).BonusStatusMultiplier * CombatConstants::STATUS_MULTIPLIER_PER_POINT;

            if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
            {
                const FRuntimeAttachedItem &Att = *AttPtr;
                Factor += CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::StatusMultiplier) / CombatConstants::STAT_PERCENT_DIVISOR;
            }
        }

        // Transient StatusMultiplierBuff/Debuff — same Max(0, 1 + (buff − debuff)/100) the
        // inline call sites apply (AddStatusBuildup step 5b, CombatOrchestrator BD bake).
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            const float SmBuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::StatusMultiplierBuff);
            const float SmDebuff = SEM->GetTotalStatModifier(Owner, ESkillEffectType::StatusMultiplierDebuff);
            Factor *= FMath::Max(0.0f, 1.0f + (SmBuff - SmDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
        }
    }

    // [-100%, +100%] normalization — cap the composed StatusMultiplier (base × transient) to
    // [0, 2]. This getter is the SOLE composition point (BD, crystal-wear, and the re-pointed
    // CombatOrchestrator site all read it), so one clamp bounds every consumer. Byte-identical
    // below 2.0; the inner Max(0,…) transient floor above is unchanged and additional to this.
    return FMath::Clamp(Factor, CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
}

float UCharacterDataComponent::GetEffectiveResistance() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }

    // Element-AGNOSTIC self-resistance for the crystal-wear CONTROL term — NOT the
    // StatusBuildupManager element-matched defense value. Same layer sources as that
    // defense block (innate + equipment BonusResistance + ResistanceStone + the blanket
    // ModifyStatusResist transient), but element-AGNOSTIC: NO GetTotalElementResistance
    // (element-matched), and NO inner [0/-1, MAX] clamp — wear's ControlFactor
    // [SUBSTAT_POWER_FACTOR_MIN, MAX] bounds the result. Innate is summed RAW (unclamped).
    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const int32 TotalPoints = CharacterData->GetTotalResistance();
    float Resistance = ModifiedSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT;

    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            Resistance += Loadout->GetActiveStatBonus(Owner).BonusResistance * CombatConstants::RESISTANCE_PER_POINT;

            if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
            {
                const FRuntimeAttachedItem &Att = *AttPtr;
                Resistance += CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::Resistance) / CombatConstants::STAT_PERCENT_DIVISOR;
            }
        }

        // Transient — ModifyStatusResist ONLY (the element-agnostic blanket channel the
        // ResistanceStone consumable feeds). Deliberately NOT ResistanceBuff/Debuff, which
        // are element-matched and belong only to the SBM defense aggregate.
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            Resistance += SEM->GetTotalStatModifier(Owner, ESkillEffectType::ModifyStatusResist) / CombatConstants::STAT_PERCENT_DIVISOR;
        }
    }

    return Resistance;
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