// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/CharacterDataComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "WoRAuthority.h"
#include "Combat/CombatConstants.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
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
#include "Party/PartySessionSubsystem.h"
#include "Party/Party.h"
#include "Engine/GameInstance.h"

UCharacterDataComponent::UCharacterDataComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    // B-131: HP / EP / alive / Broken Darkness and the max pools reach the clients (co-op, D-103).
    SetIsReplicatedByDefault(true);

    CurrentHP = 100;
    CurrentEP = 100;
    MaxHP = 100;
    MaxEP = 100;
    bIsAlive = true;
}

const FPartyMember *UCharacterDataComponent::FindCarriedPartyMember() const
{
    const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
    return PartySession && CharacterData ? PartySession->GetMemberCarriedState(PartySession->GetMemberSlotByData(CharacterData)) : nullptr;
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
                // B-091 (D-020): a party member's gear is the party's shared armoury.
                Inventory->DrawSharedGear();
            }
            if (Loadout && Inventory)
            {
                Loadout->InitializeFromCharacterData(CharacterData, Inventory);
                // B-115: the crystal state the last fight of the run left, before the pools read it.
                if (const FPartyMember *Carried = FindCarriedPartyMember())
                {
                    Loadout->ApplyCarriedGear(Carried->CarriedGear);
                }
            }
        }

        // Seed the runtime world-stat layer from the asset baseline + head-start (§7). Each PIE
        // spawn is effectively a run-start, so this is the run seed until a run-state object exists.
        SeedWorldStats();

        // B-131: the pools are the server's; a client receives them by replication and must not
        // overwrite them from its own (local) party carry and loadout.
        // TODO: B-230 - the inventory / loadout init above still runs on a client from local data.
        const bool bSeedsPools = HasServerAuthority();

        // HP/EP init — crystal-aware path with equipment bonus folded in.
        // Formula lives in RecomputeMaxPools so equipment-swap code can re-run
        // it without duplicating the math.
        if (bSeedsPools)
        {
            RecomputeMaxPools();
        }

        // Trial carry-over: a rostered party member re-spawning into a battle
        // resumes the HP/EP it left the last one with, rather than arriving
        // topped up. Enemies are never rostered, so GetMemberSlotByData returns
        // INDEX_NONE and they fall through to the max path — no origin check.
        constexpr int32 MIN_POOL_VALUE = 0;
        bool bAppliedCarriedVitals = false;

        if (const UGameInstance *GI = bSeedsPools && GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (const UPartySessionSubsystem *PartySession = GI->GetSubsystem<UPartySessionSubsystem>())
            {
                const int32 SlotIndex = PartySession->GetMemberSlotByData(CharacterData);
                int32 CarriedHP = 0;
                int32 CarriedEP = 0;
                if (SlotIndex != INDEX_NONE && PartySession->GetMemberVitals(SlotIndex, CarriedHP, CarriedEP))
                {
                    // Clamp on read: a carry banked before an equipment or level
                    // change can exceed a max that has since moved.
                    CurrentHP = FMath::Clamp(CarriedHP, MIN_POOL_VALUE, MaxHP);
                    CurrentEP = FMath::Clamp(CarriedEP, MIN_POOL_VALUE, MaxEP);
                    bAppliedCarriedVitals = true;
                }
            }
        }

        if (bSeedsPools && !bAppliedCarriedVitals)
        {
            CurrentHP = MaxHP;
            CurrentEP = MaxEP;
        }

        // Character-created ("born") BD: seed the runtime flag and zero EP so
        // they start in BD runtime state without needing a transform event.
        // bBrokenDarknessInnate is the init seed only — thereafter the runtime
        // flag (read via IsBrokenDarkness()) is authoritative. The flag stays
        // true at rest, so the EP gates (ServerGainEnergy/ServerSetEP) keep
        // treating a born-BD as BD.
        if (HasServerAuthority() && CharacterData &&
            CharacterData->bBrokenDarknessInnate)
        {
            bIsBrokenDarkness = true;

            // The flag seed is identity and always runs. The EP zeroing is a
            // SEED, so it must not overwrite a real carry — a born-BD that
            // banked EP mid-trial would otherwise be silently reset to 0 on
            // every battle entry. A carried 0 still reads 0 via the carry path.
            if (!bAppliedCarriedVitals)
            {
                CurrentEP = 0;
            }
        }

        // Born-BD hand-off. MUST come after the flag above — that is the first
        // moment IsBrokenDarkness() is meaningful, and the manager's flip reads it.
        // This lives here rather than in UBrokenDarknessManager::BeginPlay because
        // that would depend on component BeginPlay order: native components run
        // before SCS ones, so promoting either component silently reorders them.
        // The failure was invisible (no error, born-BD characters only) — an
        // explicit call at the known-good moment removes the ordering dependency.
        if (Owner)
        {
            if (UBrokenDarknessManager *BDManager = Owner->FindComponentByClass<UBrokenDarknessManager>())
            {
                BDManager->InitializeBornBrokenDarkness();
                // B-115 (Q-115): after the base-Darkness seed and the EP carry above.
                if (const FPartyMember *Carried = FindCarriedPartyMember())
                {
                    BDManager->ApplyCarriedAbsorption(Carried->CarriedAbsorption);
                }
            }

            // B-159 (D-081): loaded loadouts drop their invalid parts now, not at the next fight.
            // Only here are the world stats (spell discount) and the BD flag the rules read seeded.
            // TODO: server-only once loadouts replicate for co-op (D-065).
            if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
            {
                Loadout->DropInvalidPartsFromAllLoadouts();
            }
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
    if (Target == GetOwner() && IsPoolEffect(Effect.EffectType) && HasServerAuthority()) // B-131: replicated to clients
    {
        RecomputeMaxPools();
    }
}

void UCharacterDataComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UCharacterDataComponent, CurrentHP);
    DOREPLIFETIME(UCharacterDataComponent, CurrentEP);
    DOREPLIFETIME(UCharacterDataComponent, MaxHP);
    DOREPLIFETIME(UCharacterDataComponent, MaxEP);
    // B-131: the server picks the template when it spawns the pawn; a client needs it for names and panels.
    DOREPLIFETIME_CONDITION(UCharacterDataComponent, CharacterData, COND_InitialOnly);
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

// ========================================
// WORLD STATS (runtime layer — §7)
// ========================================

int32 *UCharacterDataComponent::LiveFieldFor(EWorldPillar Pillar)
{
    switch (Pillar)
    {
    case EWorldPillar::Mind:   return &LiveWorldMind;
    case EWorldPillar::Body:   return &LiveWorldBody;
    case EWorldPillar::Spirit: return &LiveWorldSpirit;
    default:                   return nullptr;
    }
}

int32 UCharacterDataComponent::GetWorldStat(EWorldPillar Pillar) const
{
    switch (Pillar)
    {
    case EWorldPillar::Mind:   return LiveWorldMind;
    case EWorldPillar::Body:   return LiveWorldBody;
    case EWorldPillar::Spirit: return LiveWorldSpirit;
    default:                   return 0;
    }
}

void UCharacterDataComponent::SeedWorldStats()
{
    // Live = asset baseline + head-start, per pillar (clamped). Asset null → baseline 0.
    const int32 BaseMind   = CharacterData ? CharacterData->WorldMindLevel   : 0;
    const int32 BaseBody   = CharacterData ? CharacterData->WorldBodyLevel   : 0;
    const int32 BaseSpirit = CharacterData ? CharacterData->WorldSpiritLevel : 0;

    LiveWorldMind   = FMath::Clamp(BaseMind   + HeadStartMind,   0, WorldStatConstants::LIVE_MAX);
    LiveWorldBody   = FMath::Clamp(BaseBody   + HeadStartBody,   0, WorldStatConstants::LIVE_MAX);
    LiveWorldSpirit = FMath::Clamp(BaseSpirit + HeadStartSpirit, 0, WorldStatConstants::LIVE_MAX);
}

void UCharacterDataComponent::ResetRunWorldStats()
{
    SeedWorldStats();           // Live = asset + head-start (wipes this run's earned/bought)
    WorldStatPurchaseCount = 0; // reset the escalating vendor ramp
}

bool UCharacterDataComponent::AddEarnedWorldStat(EWorldPillar Pillar, int32 N)
{
    if (N <= 0)
    {
        return false;
    }
    int32 *Live = LiveFieldFor(Pillar);
    if (!Live || *Live >= WorldStatConstants::LIVE_MAX)
    {
        return false; // already at cap (or bad pillar) — nothing granted
    }
    *Live = FMath::Min(*Live + N, WorldStatConstants::LIVE_MAX);
    return true;
}

bool UCharacterDataComponent::SetHeadStartAllocation(int32 M, int32 B, int32 S)
{
    if (M < 0 || B < 0 || S < 0)
    {
        return false;
    }
    if (M + B + S > WorldStatConstants::HEAD_START_BUDGET)
    {
        return false; // over budget — reject the whole allocation
    }
    HeadStartMind = M;
    HeadStartBody = B;
    HeadStartSpirit = S;
    SeedWorldStats(); // new floor → re-seed live (NOTE: wipes earned this run — see header flag)
    return true;
}

void UCharacterDataComponent::IncrementPurchaseCount()
{
    ++WorldStatPurchaseCount;
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
        // Last Stand intercept — when the actor holds a LastStand skill-effect, PRE-EMPT death:
        // restore HP and skip the death broadcast (death DENIAL, not resurrection). The pre-empt is
        // unchanged — bIsAlive stays true, no OnDied, no ServerResurrect (that primitive is for
        // already-DEAD targets; a Last Stand bearer never dies, so HP is written directly here).
        // HealBlock stays bypassed by construction: this writes CurrentHP directly, never routing
        // through ApplyEffect / HealthRestore, so the HealBlock gate can't touch it.
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            if (AActor *Owner = GetOwner())
            {
                // Charge-aware: consume ONE charge on the matched LastStand effect (multi-life). The
                // returned value is the restore HP as a PERCENT of MaxHP; a negative sentinel means
                // no LastStand effect is present.
                const float LastStandPercent = SEM->ConsumeLastStandCharge(Owner);
                if (LastStandPercent >= 0.0f)
                {
                    // Value-driven HP: EffectValue is a percent (60 → 60% MaxHP). FALLBACK: a 0
                    // (unset/legacy) value reverts to the 30% constant so value-0 never means
                    // "survive at 0 HP" and old behaviour is preserved.
                    const float HPFraction = (LastStandPercent > 0.0f)
                                                 ? (LastStandPercent / 100.0f)
                                                 : ItemConstants::REVIVE_HP_PERCENT;
                    CurrentHP = FMath::Max(1, FMath::RoundToInt(MaxHP * HPFraction));
                    OnHPChanged.Broadcast(CurrentHP, MaxHP);
                    UE_LOG(LogTemp, Log, TEXT("[CharacterDataComponent] %s last stand at %d HP (%.0f%% of MaxHP)"),
                           *Owner->GetName(), CurrentHP, HPFraction * 100.0f);
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
    return WoRAuthority::HasAuthority(GetOwner());
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
    // Direct model: the runtime flag IS the truth. No innate-element fallback —
    // a reverted born-BD (arc 2 flip-off) must read false, and the flag is the
    // sole authority. A born-BD is seeded true at init (the auto-flip reads
    // bBrokenDarknessInnate once); born-nature is queried separately via
    // UCharacterData::bBrokenDarknessInnate when needed.
    return bIsBrokenDarkness;
}

ESpellElement UCharacterDataComponent::GetDisplayElement() const
{
    // Underlying display element. A BD reads as Darkness here (its innate element);
    // BD identity for tint/labels is surfaced separately via IsBrokenDarkness(), not
    // by emitting a distinct enum value (the BrokenDarkness value is being deleted).
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
    else
    {
        // BD->Darkness revert: reset EP to normal max (asymmetric vs carryover-on-activate).
        // Direct field set — bypasses the ServerSetEP BD guard (bIsBrokenDarkness is already
        // false here, set above), so the guard does not block the write.
        CurrentEP = MaxEP;
        OnEPChanged.Broadcast(CurrentEP, MaxEP);   // relabels bar Absorb -> EP
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
    using ECrystalPillar = EWorldPillar;

    /** The three layers as FWoRStatLayers (B-042t), so the getter and the GAS pillar sources build one
     *  number: StoneFactor = the crystal layer, GearFactor = the equipment layer, TransientFactor = the
     *  floored pillar buff, over the world base. */
    FWoRStatLayers BuildPillarLayers(AActor *Owner, float BaseValue, ECrystalPillar Pillar)
    {
        FWoRStatLayers Layers = FWoRStatLayers::ForPillar(BaseValue);
        if (!Owner)
        {
            return Layers;
        }
        ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>();
        if (!Loadout)
        {
            return Layers;
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
            Layers.StoneFactor = 1.0f + CrystalPercent / CombatConstants::STAT_PERCENT_DIVISOR;
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
                Layers.StoneFactor = 1.0f + CrystalPercent / CombatConstants::STAT_PERCENT_DIVISOR;
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
        Layers.GearFactor = 1.0f + EquipmentPercent / CombatConstants::STAT_PERCENT_DIVISOR;

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
                    Layers.TransientFactor = FMath::Max(0.0f, 1.0f + (PillarBuff - PillarDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
                }
            }
        }

        return Layers;
    }

    float ApplyEvolutionPillarModifier(AActor *Owner, float BaseValue, ECrystalPillar Pillar)
    {
        // [-100%, +100%] normalization: the COMPOSED modifier (crystal x equipment x transient) is
        // hard-capped to [0, 2], additional to the transient's own Max(0,...) floor; a <= -100% crystal
        // or equipment percent would otherwise make it negative. Inert below 2.0, so a bare or
        // normally geared character is byte-identical. The base is never clamped.
        const FWoRStatLayers Layers = BuildPillarLayers(Owner, BaseValue, Pillar);
        const float Modifier = FMath::Clamp(Layers.StoneFactor * Layers.GearFactor * Layers.TransientFactor,
                                            CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
        return BaseValue * Modifier;
    }
}

float UCharacterDataComponent::GetEvolutionModifiedMind() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    // §7 C2: world base from the LIVE level (asset + head-start + earned), not the asset's authored level.
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetMindAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    return ApplyEvolutionPillarModifier(GetOwner(), CharacterData->GetEffectiveMindForLevel(GetWorldMind()), ECrystalPillar::Mind);
}

float UCharacterDataComponent::GetEvolutionModifiedBody() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetBodyAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    return ApplyEvolutionPillarModifier(GetOwner(), CharacterData->GetEffectiveBodyForLevel(GetWorldBody()), ECrystalPillar::Body);
}

float UCharacterDataComponent::GetEvolutionModifiedSpirit() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetSpiritAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    return ApplyEvolutionPillarModifier(GetOwner(), CharacterData->GetEffectiveSpiritForLevel(GetWorldSpirit()), ECrystalPillar::Spirit);
}

bool UCharacterDataComponent::GetPillarLayers(EWorldPillar Pillar, FWoRStatLayers &OutLayers) const
{
    if (!CharacterData)
    {
        return false;
    }
    float WorldBase = 0.0f;
    switch (Pillar)
    {
    case EWorldPillar::Mind:   WorldBase = CharacterData->GetEffectiveMindForLevel(GetWorldMind());     break;
    case EWorldPillar::Body:   WorldBase = CharacterData->GetEffectiveBodyForLevel(GetWorldBody());     break;
    case EWorldPillar::Spirit: WorldBase = CharacterData->GetEffectiveSpiritForLevel(GetWorldSpirit()); break;
    }
    OutLayers = BuildPillarLayers(GetOwner(), WorldBase, Pillar);
    return true;
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
            HPPct = Loadout->GetAttachedStonePercentForType(ECrystalType::MaxHPStone);
            EPPct = Loadout->GetAttachedStonePercentForType(ECrystalType::MaxEPStone);
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
    // Gear MaxHP at a real per-point rate (TierPowerScaling §5) — was a flat +1/point.
    // Stacks OUTSIDE the stat clamp above (gear headroom over the 1000 cap, as before).
    MaxHPLayers.StatPortion = HPStatPortion;
    MaxHPLayers.GearAmount = BonusMaxHP * CombatConstants::GEAR_MAX_HEALTH_PER_POINT;
    MaxHPLayers.PercentFactor = 1.0f + HPPct / CombatConstants::STAT_PERCENT_DIVISOR;
    MaxHP = MaxHPLayers.Compose();

    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const float EPStatPortion = FMath::Min(
        CombatConstants::MAX_ENERGY_BASE +
            (ModifiedSpirit * CharacterData->GetTotalMaxEnergy() * CombatConstants::MAX_ENERGY_PER_POINT),
        CombatConstants::MAX_ENERGY_CAP);
    // Gear MaxEnergy at a real per-point rate (TierPowerScaling §5) — was a flat +1/point.
    // Stacks OUTSIDE the stat clamp above (gear headroom over the 1000 cap, as before).
    MaxEPLayers.StatPortion = EPStatPortion;
    MaxEPLayers.GearAmount = BonusMaxEnergy * CombatConstants::GEAR_MAX_ENERGY_PER_POINT;
    MaxEPLayers.PercentFactor = 1.0f + EPPct / CombatConstants::STAT_PERCENT_DIVISOR;
    MaxEP = MaxEPLayers.Compose();
}

bool UCharacterDataComponent::GetMaxHPLayers(FWoRPoolLayers &OutLayers) const
{
    if (!CharacterData || MaxHPLayers.Compose() != MaxHP)
    {
        return false;
    }
    OutLayers = MaxHPLayers;
    return true;
}

bool UCharacterDataComponent::GetMaxEPLayers(FWoRPoolLayers &OutLayers) const
{
    if (!CharacterData || MaxEPLayers.Compose() != MaxEP)
    {
        return false;
    }
    OutLayers = MaxEPLayers;
    return true;
}

float UCharacterDataComponent::GetEquipmentModifiedLuck() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }

    // Pattern P (cluster 5e-C1, GetStatLayers) — a NORMALIZED luck value (not raw): the crystal-aware
    // Spirit × Luck points stat caps ALONE at 1.0 (LUCK_RAW_MAX is the basis), then BonusLuck, the Luck
    // stone and the net LuckBuff / LuckDebuff multiply it, clamped [0, LUCK_GEAR_CEILING]. Consumers
    // (GetLuckModifiedChance) scale by their own max bonus and clamp at their own 100%.
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::Luck, Layers);
    return Layers.Compose();
}

float UCharacterDataComponent::GetLuckModifiedChance(float BaseChance, float LuckMaxBonus) const
{
    // GetEquipmentModifiedLuck now returns the NORMALIZED luck directly (5e-C1: stat caps ALONE at 1.0,
    // gear/transient multiply past toward LUCK_GEAR_CEILING) — so we do NOT re-normalize here. The gear's
    // past-1.0 value flows through; each consumer caps the final chance at its own 100% (e.g. crit /
    // break-skip clamp [0,1]). At maxed stat (norm 1.0): BaseChance + LuckMaxBonus = the listed max;
    // with gear (norm up to 2.0) the chance pushes past it. Negative (cursed) norm reduces the chance.
    return BaseChance + GetEquipmentModifiedLuck() * LuckMaxBonus;
}

bool UCharacterDataComponent::RollLuckChance(float BaseChance, float LuckMaxBonus) const
{
    // A negative chance (cursed wielder, BaseChance 0) is never < FRand()'s [0,1) — never fires.
    return UBattleRandomSubsystem::FRand(this) < GetLuckModifiedChance(BaseChance, LuckMaxBonus);
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
    // Cluster 5e-C2: crit chance is now LUCK-driven. base 0.05 + normalized Luck (stat caps ALONE at
    // 1.0 → +0.45 = 0.50 at maxed Luck stat) × CRIT_CHANCE_LUCK_BONUS; Luck gear pushes the norm past
    // 1.0 toward the [0,1] ceiling (100%) — capped at the GetCriticalChance final clamp. This does NOT
    // read the Mind crit substat — that substat (renamed CritDamage in 5e-C3) drives crit DAMAGE via
    // GetCritDamageMultiplier.
    return GetLuckModifiedChance(CombatConstants::CRIT_CHANCE_BASE, CombatConstants::CRIT_CHANCE_LUCK_BONUS);
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
            return 1.0f + Loadout->GetAttachedStonePercent(ESubStat::SpellDamage) / CombatConstants::STAT_PERCENT_DIVISOR;
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
            return 1.0f + SEM->GetNetStatModifier(Owner, ESubStat::SpellDamage) / CombatConstants::STAT_PERCENT_DIVISOR;
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
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetSpellDamageAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::SpellDamage, Layers);
    return Layers.Compose();
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
            return 1.0f + Loadout->GetAttachedStonePercent(ESubStat::RawDamage) / CombatConstants::STAT_PERCENT_DIVISOR;
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
            return 1.0f + SEM->GetNetStatModifier(Owner, ESubStat::RawDamage) / CombatConstants::STAT_PERCENT_DIVISOR;
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
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetRawDamageAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::RawDamage, Layers);
    return Layers.Compose();
}

float UCharacterDataComponent::GetEffectiveActionSpeed() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }
    // Pattern P (cluster 5g) — animation/attack-pacing mirror of GetEffectiveRawDamage. The STAT
    // term (CalculateAnimationSpeed, itself ×1.5-capped) is clamped ALONE at STAT_MULT_CAP, THEN
    // gear MULTIPLIES it: equipment BonusActionSpeed (additive per-point read as a fraction),
    // attached ActionSpeedStone, and transient ActionSpeedBuff/Debuff — bounded by the
    // STAT_MODIFIER_MAX (×2.0) compose ceiling. Returns a pure ×1.0–×2.0 multiplier; the montage
    // BaseAnimSpeed and per-action ActionMods stay at the ActionExecutor call site. ×1.0 at zero
    // gear (the old additive path's zero-point), so byte-identical with no speed gear.
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetActionSpeedAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::ActionSpeed, Layers);
    return Layers.Compose();
}

float UCharacterDataComponent::GetEffectiveSpellSpeed() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }
    // Pattern P (cluster 5g) — Mind/cast mirror of GetEffectiveActionSpeed. Stat (CalculateSpellSpeed,
    // ×1.5-capped) clamped ALONE, THEN BonusSpellSpeed (fraction) × attached SpellSpeedStone ×
    // transient SpellSpeedBuff/Debuff, bounded by STAT_MODIFIER_MAX (×2.0). Pure multiplier; the
    // montage BaseAnimSpeed and per-action ActionMods stay at the call site. ×1.0 at zero gear.
    float OnGAS = 0.0f;
    if (UWoRAttributeSyncComponent::ReadComposedOnGAS(GetOwner(), UWoRAttributeSet_Stats::GetSpellSpeedAttribute(), OnGAS))
    {
        return OnGAS; // B-042yb: GAS's value at UseGAS 1
    }
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::SpellSpeed, Layers);
    return Layers.Compose();
}

bool UCharacterDataComponent::GetStatLayers(ESubStat Stat, FWoRStatLayers &OutLayers) const
{
    // Pattern P (clusters 5a, 5g): the stat term capped ALONE at STAT_MULT_CAP (x1.5), then gear,
    // stone and transient multiply it (FWoRStatLayers::Compose clamps). Gear points read as a fraction.
    OutLayers = FWoRStatLayers::ForStat(Stat);
    AActor *Owner = GetOwner();
    ULoadoutComponent *Loadout = Owner ? Owner->FindComponentByClass<ULoadoutComponent>() : nullptr;
    const USkillEffectManager *SEM = Owner ? GetSkillEffectManager() : nullptr;
    auto ReadStoneAndTransient = [&]()
    {
        if (Loadout)
        {
            OutLayers.StoneFactor = 1.0f + Loadout->GetAttachedStonePercent(Stat) / CombatConstants::STAT_PERCENT_DIVISOR;
        }
        if (SEM)
        {
            OutLayers.TransientFactor = 1.0f + SEM->GetNetStatModifier(Owner, Stat) / CombatConstants::STAT_PERCENT_DIVISOR;
        }
    };
    // StatusMultiplier and Defense floor a 100%+ debuff at x0 (never inverts).
    auto FloorTransient = [&OutLayers]() { OutLayers.TransientFactor = FMath::Max(0.0f, OutLayers.TransientFactor); };

    switch (Stat)
    {
    case ESubStat::SpellDamage:
        OutLayers.StatBase = FMath::Min(GetEvolutionModifiedSpellDamage(), CombatConstants::STAT_MULT_CAP);
        OutLayers.GearFactor = 1.0f + GetEquipmentSpellDamageTerm();
        OutLayers.StoneFactor = GetStoneSpellDamageFactor();
        OutLayers.TransientFactor = GetTransientSpellDamageFactor();
        return true;
    case ESubStat::RawDamage:
        OutLayers.StatBase = FMath::Min(GetEvolutionModifiedRawDamage(), CombatConstants::STAT_MULT_CAP);
        OutLayers.GearFactor = 1.0f + GetEquipmentRawDamageTerm();
        OutLayers.StoneFactor = GetStoneRawDamageFactor();
        OutLayers.TransientFactor = GetTransientRawDamageFactor();
        return true;
    case ESubStat::ActionSpeed:
        if (CharacterData)
        {
            OutLayers.StatBase = FMath::Min(CharacterData->CalculateAnimationSpeed(), CombatConstants::STAT_MULT_CAP);
            if (Loadout)
            {
                OutLayers.GearFactor = 1.0f + Loadout->GetActiveStatBonus(Owner).BonusActionSpeed * CombatConstants::ANIMATION_SPEED_PER_POINT;
            }
            ReadStoneAndTransient();
        }
        return true;
    case ESubStat::SpellSpeed:
        if (CharacterData)
        {
            OutLayers.StatBase = FMath::Min(CharacterData->CalculateSpellSpeed(), CombatConstants::STAT_MULT_CAP);
            if (Loadout)
            {
                OutLayers.GearFactor = 1.0f + Loadout->GetActiveStatBonus(Owner).BonusSpellSpeed * CombatConstants::SPELL_SPEED_PER_POINT;
            }
            ReadStoneAndTransient();
        }
        return true;
    case ESubStat::StatusMultiplier:
        OutLayers.StatBase = FMath::Min(GetEvolutionModifiedStatusMultiplier(), CombatConstants::STAT_MULT_CAP);
        if (Loadout)
        {
            OutLayers.GearFactor =
                1.0f + Loadout->GetActiveStatBonus(Owner).BonusStatusMultiplier * CombatConstants::STATUS_MULTIPLIER_PER_POINT;
        }
        ReadStoneAndTransient();
        FloorTransient();
        return true;
    case ESubStat::Defense:
        // A reduction: the stat capped alone at UNIVERSAL_STAT_CAP (0.5), toward DEFENSE_REDUCTION_CEILING.
        if (!CharacterData)
        {
            return false;
        }
        OutLayers.StatBase = FMath::Min(GetEvolutionModifiedFlatDefense(), CombatConstants::UNIVERSAL_STAT_CAP);
        if (Loadout)
        {
            OutLayers.GearFactor = 1.0f + Loadout->GetActiveStatBonus(Owner).BonusDefense * CombatConstants::DEFENSE_PER_POINT;
        }
        ReadStoneAndTransient();
        FloorTransient();
        return true;
    case ESubStat::CritDamage:
        // x1.0 un-invested; the Mind crit-damage ramp capped alone at CRIT_DAMAGE_STAT_CAP.
        OutLayers.StatBase = CombatConstants::CRIT_DMG_BASE;
        if (CharacterData)
        {
            OutLayers.StatBase = FMath::Min(CombatConstants::CRIT_DMG_BASE + GetEvolutionModifiedMind() * CharacterData->GetTotalCritDamage() *
                                                                               CombatConstants::CRIT_DAMAGE_PER_POINT,
                                            CombatConstants::CRIT_DAMAGE_STAT_CAP);
        }
        if (Loadout)
        {
            OutLayers.GearFactor = 1.0f + Loadout->GetActiveStatBonus(Owner).BonusCritDamage * CombatConstants::CRIT_DAMAGE_PER_POINT;
        }
        ReadStoneAndTransient();
        return true;
    case ESubStat::Efficiency:
        // The cost REDUCTION: crystal-aware Mind x Efficiency points capped ALONE at EFFICIENCY_MAX (0.5).
        if (!CharacterData)
        {
            return false;
        }
        OutLayers.StatBase = FMath::Min(GetEvolutionModifiedMind() * CharacterData->GetTotalEfficiency() * CombatConstants::EFFICIENCY_PER_POINT,
                                        CombatConstants::EFFICIENCY_MAX);
        if (Loadout)
        {
            OutLayers.GearFactor = 1.0f + Loadout->GetActiveStatBonus(Owner).BonusEfficiency * CombatConstants::EFFICIENCY_PER_POINT;
        }
        ReadStoneAndTransient();
        return true;
    case ESubStat::Luck:
        // Normalized: the Spirit-scaled raw luck over LUCK_RAW_MAX, capped ALONE at 1.0.
        if (!CharacterData)
        {
            return false;
        }
        OutLayers.StatBase = FMath::Min(GetEvolutionModifiedSpirit() * CharacterData->GetTotalLuck() * CombatConstants::LUCK_PER_POINT /
                                            CombatConstants::LUCK_RAW_MAX,
                                        1.0f);
        if (Loadout)
        {
            OutLayers.GearFactor = 1.0f + Loadout->GetActiveStatBonus(Owner).BonusLuck * CombatConstants::LUCK_PER_POINT;
        }
        ReadStoneAndTransient();
        return true;
    case ESubStat::Reflex:
        // The evolution-pillar lift as a ratio over the RAW Body pillar (CalculateReflexWindowBonus uses
        // raw Body), both from the live world base, so the ratio is pure crystal / gear / buff lift; 1.0
        // without one. ApplyEvolutionPillarModifier already clamps the pillar modifier to [0, 2].
        if (!CharacterData)
        {
            return false;
        }
        {
            const float RawPillar = CharacterData->GetEffectiveBodyForLevel(GetWorldBody());
            OutLayers.StatBase = RawPillar > KINDA_SMALL_NUMBER ? GetEvolutionModifiedBody() / RawPillar : 1.0f;
        }
        if (Loadout)
        {
            OutLayers.GearFactor =
                1.0f + static_cast<float>(Loadout->GetActiveStatBonus(Owner).BonusReflex) * CombatConstants::STAT_MULT_PER_POINT;
        }
        ReadStoneAndTransient();
        return true;
    default:
        return false;
    }
}

float UCharacterDataComponent::SpeedWindowGearFactor(EActionType AttackType) const
{
    if (!CharacterData)
    {
        return 1.0f;
    }
    const bool bSpell = (AttackType == EActionType::Spell);

    // Evolution-pillar modifier as a ratio over the RAW pillar (CalculateSpeedWindowPenalty uses the
    // RAW GetEffectiveBody/Mind, so this ratio is the crystal/equipment-pillar/pillar-buff lift). 1.0
    // when no pillar gear. ApplyEvolutionPillarModifier already clamps the pillar modifier to [0,2].
    // RawPillar uses the LIVE world base too (matching ModPillar's GetEvolutionModified* base, §7 C2),
    // so the ratio cancels the world-stat term cleanly to the pure crystal/equipment/buff lift.
    const float RawPillar = bSpell ? CharacterData->GetEffectiveMindForLevel(GetWorldMind())
                                   : CharacterData->GetEffectiveBodyForLevel(GetWorldBody());
    const float ModPillar = bSpell ? GetEvolutionModifiedMind() : GetEvolutionModifiedBody();
    float Factor = (RawPillar > KINDA_SMALL_NUMBER) ? (ModPillar / RawPillar) : 1.0f;

    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Owner);
            // Equipment Bonus{Action,Spell}Speed — per-point read as a fraction (option-ii), same as
            // GetEffectiveActionSpeed/SpellSpeed. ×1 (inert) when absent.
            const float BonusPts = bSpell ? static_cast<float>(Bonus.BonusSpellSpeed) : static_cast<float>(Bonus.BonusActionSpeed);
            const float PerPoint = bSpell ? CombatConstants::SPELL_SPEED_PER_POINT : CombatConstants::ANIMATION_SPEED_PER_POINT;
            Factor *= (1.0f + BonusPts * PerPoint);

            // Attached {Action,Spell}SpeedStone — % multiplier. 0 (×1) unless that stone is attached.
            const ESubStat StoneStat = bSpell ? ESubStat::SpellSpeed : ESubStat::ActionSpeed;
            Factor *= (1.0f + Loadout->GetAttachedStonePercent(StoneStat) / CombatConstants::STAT_PERCENT_DIVISOR);
        }

        // Transient {Action,Spell}SpeedBuff/Debuff — mirrors GetEffectiveActionSpeed's transient layer.
        // Speed transient stays MULTIPLICATIVE here (unlike the additive transient policy on multiplier stats):
        // the window is measured in SECONDS, where "+20% speed" must scale the penalty PROPORTIONALLY. An
        // additive +0.20 would mean +0.20 SECONDS — enormous vs a ~0.5s window. Proportional is the correct
        // math for a time value, so temporary speed buffs tighten the window in scale. (Permanent speed gear:
        // also multiplicative, same reason. Multiplier-space stats — damage/status/etc. — use additive
        // transients; see the transient-additive conversion.)
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            const ESubStat SpeedStat = bSpell ? ESubStat::SpellSpeed : ESubStat::ActionSpeed;
            Factor *= (1.0f + SEM->GetNetStatModifier(Owner, SpeedStat) / CombatConstants::STAT_PERCENT_DIVISOR);
        }
    }

    return Factor;
}

float UCharacterDataComponent::ReflexWindowGearFactor() const
{
    if (!CharacterData)
    {
        return 1.0f;
    }

    // GetStatLayers: the Body pillar ratio x BonusReflex x ReflexStone x net Reflex buff, unclamped.
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::Reflex, Layers);
    return Layers.Compose();
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

float UCharacterDataComponent::GetEffectiveEfficiencyReduction() const
{
    // Pattern P (cluster 5b, GetStatLayers): the crystal-aware Mind x Efficiency-points reduction capped
    // ALONE at EFFICIENCY_MAX (0.5), then BonusEfficiency, the EfficiencyStone and the net Efficiency
    // buff / debuff multiply it toward EFFICIENCY_GEAR_CEILING; a net debuff floors at 0. Distinct effect
    // type from EP's SpellCostBuff/Debuff, so no double-count with the EP SkillEffectMult layer.
    FWoRStatLayers Layers;
    return GetStatLayers(ESubStat::Efficiency, Layers) ? Layers.Compose() : 0.0f;
}

float UCharacterDataComponent::GetEffectiveEfficiencyMultiplier() const
{
    // Efficiency is INVERTED: a larger reduction = a smaller multiplier = cheaper EP / slower BD drain /
    // less wear. 1.0 without CharacterData.
    return 1.0f - GetEffectiveEfficiencyReduction();
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

float UCharacterDataComponent::GetCrystalResistanceStatCapped() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }
    // Crystal-aware Spirit-Resistance STAT, capped ALONE at UNIVERSAL_STAT_CAP (0.5) — the stat-layer
    // base for the multiplicative general-resistance bucket (A2). Crystal pillar via
    // GetEvolutionModifiedSpirit (closes the gap where GetTotalStatusResistance used raw GetEffectiveSpirit).
    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const int32 TotalPoints = CharacterData->GetTotalResistance();
    return FMath::Min(
        ModifiedSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT,
        CombatConstants::UNIVERSAL_STAT_CAP);
}

float UCharacterDataComponent::GetEffectiveStatusMultiplier() const
{
    // THE SINGLE source of truth for a character's effective StatusMultiplier (T3 consolidation): read by
    // the live buildup, the BD overload and crystal-wear. Pattern P (GetStatLayers): the stat term capped
    // ALONE at STAT_MULT_CAP (x1.5), then BonusStatusMultiplier, the StatusStone and the element-agnostic
    // (None) StatusMultiplierBuff/Debuff net multiply it (the transient floored at x0), clamped [0, 2.0].
    // Element-keyed StatusMultiplierBuffs apply at the status-buildup site, where the element is known.
    FWoRStatLayers Layers;
    GetStatLayers(ESubStat::StatusMultiplier, Layers);
    return Layers.Compose();
}

float UCharacterDataComponent::GetEffectiveResistance() const
{
    if (!CharacterData)
    {
        return 0.0f;
    }

    // Wear-resistance CONTROL term: DELIBERATELY uncapped + ADDITIVE (ModifiedSpirit×pts + BonusResistance
    // + ResistanceStone + blanket ModifyStatusResist), an element-agnostic "control/power" value bounded
    // later by wear's ControlFactor [SUBSTAT_POWER_FACTOR_MIN, MAX] — NOT a defense fraction.
    // ⚠️ DISTINCT from the A2 buildup general (StatusBuildupManager::GetTotalStatusResistance /
    // GetCrystalResistanceStatCapped): that is 0.5-capped, MULTIPLICATIVE, element-matched, clamped [-1,1].
    // Different value, different consumer — do NOT merge (twin-trap EXCEPTION: kept separate by design).
    // Element-AGNOSTIC: NO GetTotalElementResistance, and NO inner clamp (wear's ControlFactor bounds it).
    const float ModifiedSpirit = GetEvolutionModifiedSpirit();
    const int32 TotalPoints = CharacterData->GetTotalResistance();
    float Resistance = ModifiedSpirit * TotalPoints * CombatConstants::RESISTANCE_PER_POINT;

    if (AActor *Owner = GetOwner())
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            Resistance += Loadout->GetActiveStatBonus(Owner).BonusResistance * CombatConstants::RESISTANCE_PER_POINT;

            Resistance += Loadout->GetAttachedStonePercent(ESubStat::Resistance) / CombatConstants::STAT_PERCENT_DIVISOR;
        }

        // Transient — ModifyStatusResist ONLY (the element-agnostic blanket channel the
        // ResistanceStone consumable feeds). Deliberately NOT ResistanceBuff/Debuff, which
        // are element-matched and belong only to the SBM defense aggregate.
        if (USkillEffectManager *SEM = GetSkillEffectManager())
        {
            Resistance += SEM->GetNetStatModifier(Owner, ESubStat::Resistance) / CombatConstants::STAT_PERCENT_DIVISOR;
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