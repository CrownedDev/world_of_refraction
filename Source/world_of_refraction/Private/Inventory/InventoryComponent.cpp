// InventoryComponent.cpp
// Character inventory storage and management implementation

#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Character/CharacterData.h"
#include "Inventory/InventoryData.h"
#include "Loadout/FSavedLoadout.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/FFusionId.h"            // fusion attach identity
#include "Equipment/Crystals/CrystalTypeHelpers.h"   // IsGemType / IsValidFusionPair / IsElementalFusion
#include "Equipment/Crystals/ItemIdentity.h"         // GetMaxDurability + GetDisplayName for attach
#include "Pool/PoolSubsystem.h" // InitializeFromPool draw source (step 2)
#include "Inventory/ItemTier.h" // TierHelpers::GetTierName for the instance dump
#include "Equipment/FRuntimeAttachedItem.h" // attached-crystal description in the instance dump
#include "Combat/CombatConstants.h"
#include "Combat/TurnManager.h" // speed-notify on speed-relevant crystal detach
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Character/FPillarWeights.h"
#include "Character/CharacterDataComponent.h" // owner Luck for the pickup quality roll
#include "Currency/EconomyYield.h"            // RollQuality (§11 weighted drop curve)
#include "WoRAuthority.h"
#include <type_traits>                        // std::is_same_v for the weapon/ring socket templates

namespace
{
    /** U2 pickup roll — when the asset opts in (bRandomGenerateOnPickup), a fresh
     *  instance ROLLS its stat/resistance layers at acquisition:
     *    - MaxPools seed from the per-asset override (when set) or the tier budget
     *      (POOL_OVERRIDE_USE_TIER sentinel); the roll then consumes the STORED
     *      MaxPool, so a future add/subtract lever mutates the pool and re-rolls.
     *    - Pillar percents roll alongside on the tier budget (not separately metered).
     *    - The result OVERWRITES the CreateFrom* copy as authored Base + fresh roll,
     *      so the asset's PREVIEW Generated layer cannot leak into a real instance.
     *    - Stat/ResistancePool (charge meters) stay 0 — start empty per design.
     *  Toggle-off assets never reach here; their entries keep the CreateFrom* copy. */
    void ApplyPickupRoll(const UEquipmentDataBase *Asset,
                         FEquipmentStatBonus &StatBonus, FResistanceBonus &ResistanceBonus,
                         int32 &StatMaxPool, int32 &ResistanceMaxPool)
    {
        StatMaxPool = (Asset->StatMaxPoolOverride != CombatConstants::POOL_OVERRIDE_USE_TIER)
                          ? Asset->StatMaxPoolOverride
                          : FEquipmentStatBonus::GetSubstatBudget(Asset->Tier);
        ResistanceMaxPool = (Asset->ResistanceMaxPoolOverride != CombatConstants::POOL_OVERRIDE_USE_TIER)
                                ? Asset->ResistanceMaxPoolOverride
                                : FResistanceBonus::GetResistanceBudget(Asset->Tier);

        FEquipmentStatBonus FreshStats;
        FreshStats.RerollSubstats(StatMaxPool, FPillarWeights());
        FreshStats.RerollPillars(Asset->Tier);

        FResistanceBonus FreshResistance;
        FreshResistance.RerollResistance(ResistanceMaxPool);

        StatBonus = Asset->BaseStatBonus;
        StatBonus.Accumulate(FreshStats);
        ResistanceBonus = Asset->BaseResistance;
        ResistanceBonus.Accumulate(FreshResistance);
    }

    /** Owner's normalized Luck for the pickup quality roll (0 when no character data resolves).
     *  Mirrors the GetEquipmentModifiedLuck convention every other Luck consumer reads. */
    float ResolveInventoryOwnerLuck(const UActorComponent *Comp)
    {
        if (Comp)
        {
            if (const AActor *Owner = Comp->GetOwner())
            {
                if (const UCharacterDataComponent *CDC = Owner->FindComponentByClass<UCharacterDataComponent>())
                {
                    return CDC->GetEquipmentModifiedLuck();
                }
            }
        }
        return 0.0f;
    }

    /** Build the runtime evolution attachment that REFERENCES an owned entry (player-attach, gear-i).
     *  Mirrors the primary-slot inflation copy: asset + leveled Tier + rolled state, plus the
     *  InstanceID link that marks it player-attached (vs authored-locked, which FromAttachedItem
     *  leaves invalid). */
    FRuntimeAttachedItem MakeEvolutionAttachment(const FEvolutionInventoryEntry &Entry, FGuid EvoInstanceID)
    {
        FRuntimeAttachedItem Runtime;
        Runtime.Kind = EAttachedItemKind::Evolution;
        FEvolutionAttachment &Att = Runtime.Evolution;
        Att.Item = Entry.Item;
        Att.InstanceID = EvoInstanceID;                                       // link to the owned entry (gear-i)
        Att.Tier = Entry.Tier;                                                // leveled instance tier (Part C7)
        Att.CurrentDurability = Entry.CurrentDurability;                      // persisted durability (gear-durability) — worn evos stay worn

        Att.GeneratedStatBonus = Entry.GeneratedStatBonus;
        Att.GeneratedResistance = Entry.GeneratedResistance;
        Att.StatPool = Entry.StatPool;
        Att.StatMaxPool = Entry.StatMaxPool;
        Att.ResistancePool = Entry.ResistancePool;
        Att.ResistanceMaxPool = Entry.ResistanceMaxPool;
        return Runtime;
    }

    /** Build a runtime crystal / augment-stone attachment for a player socket op. DEBIT model —
     *  the socketed crystal is fungible slot state (destroyed on detach, no owned-entry link,
     *  unlike MakeEvolutionAttachment). A gem writes Kind=Crystal seeded to the tier max durability;
     *  an augment stone writes Kind=AugmentStone with 0 durability (stones never wear). */
    FRuntimeAttachedItem MakeCrystalAttachment(FCrystalId Id, bool bIsStone)
    {
        FRuntimeAttachedItem Runtime;
        Runtime.Kind = bIsStone ? EAttachedItemKind::AugmentStone : EAttachedItemKind::Crystal;
        Runtime.Crystal.Id = Id;
        Runtime.Crystal.CurrentDurability = bIsStone ? 0 : ItemIdentity::GetMaxDurability(Id);
        return Runtime;
    }

    /** Build a runtime fusion attachment from the two-half identity (fuse-and-socket — fusions have
     *  no loose-inventory home; the caller consumes the two halves from the crystal stacks). Seeds
     *  whole-fusion durability via GetMaxDurability (gem-seeded; 0 for an augmented fusion). */
    FRuntimeAttachedItem MakeFusionAttachment(const FFusionId &FusionId)
    {
        FRuntimeAttachedItem Runtime;
        Runtime.Kind = EAttachedItemKind::Fusion;
        Runtime.Fusion.Id = FusionId;
        Runtime.Fusion.CurrentDurability = Runtime.Fusion.GetMaxDurability();
        return Runtime;
    }

    /** One-evo-one-slot guard: true if EvoInstanceID is already referenced by any primary slot
     *  (across saved loadouts) or any gear attachment (owned weapons/rings). */
    bool IsEvolutionSlottedAnywhere(const UInventoryComponent &Inv, FGuid EvoInstanceID)
    {
        for (const FCombatLoadout &L : Inv.SavedLoadouts)
        {
            if (L.PrimaryEvolutionInstance == EvoInstanceID)
            {
                return true;
            }
        }
        for (const FWeaponInventoryEntry &W : Inv.Weapons)
        {
            if (W.AttachedItem.IsEvolution() && W.AttachedItem.Evolution.InstanceID == EvoInstanceID)
            {
                return true;
            }
        }
        for (const FRingInventoryEntry &R : Inv.Rings)
        {
            if (R.AttachedItem.IsEvolution() && R.AttachedItem.Evolution.InstanceID == EvoInstanceID)
            {
                return true;
            }
        }
        return false;
    }

    /** Share of an evolution's max durability lost when the player removes it from gear (§5.3b). */
    constexpr float EVOLUTION_REMOVAL_WEAR_FRACTION = 0.10f;

    template <class TEntry>
    constexpr bool IsRingGear = std::is_same_v<TEntry, FRingInventoryEntry>;

    template <class TEntry>
    const TCHAR *GearLabel()
    {
        return IsRingGear<TEntry> ? TEXT("ring") : TEXT("weapon");
    }

    /** Authority gate for the socket ops (WoRAuthority). A null owner is refused silently; every
     *  op failed further in without one anyway (no evolution or crystal inventory to read). */
    bool GearOpHasAuthority(const AActor *Owner, const TCHAR *Op)
    {
        if (!WoRAuthority::HasAuthority(Owner))
        {
            if (Owner)
            {
                UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: no authority — ignored"), Op);
            }
            return false;
        }
        return true;
    }

    /** Index of the weapon/ring with GearID, or INDEX_NONE (logged). */
    template <class TEntry>
    int32 FindGearIndex(const TArray<TEntry> &Gear, FGuid GearID, const TCHAR *Op)
    {
        const int32 Index = Gear.IndexOfByPredicate([&GearID](const TEntry &E) { return E.PersistentID == GearID; });
        if (Index == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: no %s for GUID %s"), Op, GearLabel<TEntry>(),
                   *GearID.ToString());
        }
        return Index;
    }

    /** Index of the weapon/ring with GearID whose socket is empty (an attach never overwrites), or
     *  INDEX_NONE (logged). */
    template <class TEntry>
    int32 FindEmptyGearSlot(const TArray<TEntry> &Gear, FGuid GearID, const TCHAR *Op)
    {
        const int32 Index = FindGearIndex(Gear, GearID, Op);
        if (Index != INDEX_NONE && !Gear[Index].AttachedItem.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: %s slot already occupied"), Op, GearLabel<TEntry>());
            return INDEX_NONE;
        }
        return Index;
    }

    FEvolutionInventoryEntry *FindOwnedEvolution(AActor *Owner, FGuid EvoInstanceID)
    {
        UEvolutionInventoryComponent *EvoInv = Owner ? Owner->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
        return EvoInv ? EvoInv->Entries.FindByPredicate(
                            [&EvoInstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == EvoInstanceID; })
                      : nullptr;
    }
}

// ==================== SHARED WEAPON/RING SOCKET BODIES (B-026) ====================

template <class TEntry>
bool UInventoryComponent::AttachEvolutionToGear(TArray<TEntry> &Gear, FGuid GearID, FGuid EvoInstanceID, const TCHAR *Op)
{
    if (!GearOpHasAuthority(GetOwner(), Op) || !EvoInstanceID.IsValid())
    {
        return false;
    }

    // Resolve the owned evolution entry being attached (the reference target — it PERSISTS in EvoInv).
    const FEvolutionInventoryEntry *Entry = FindOwnedEvolution(GetOwner(), EvoInstanceID);
    if (!Entry || !Entry->Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: no owned evolution for GUID %s"), Op,
               *EvoInstanceID.ToString());
        return false;
    }

    // One evo, one slot: reject if it is already referenced by any primary or gear slot.
    if (IsEvolutionSlottedAnywhere(*this, EvoInstanceID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: evolution %s already slotted elsewhere"), Op,
               *EvoInstanceID.ToString());
        return false;
    }

    const int32 Index = FindEmptyGearSlot(Gear, GearID, Op);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    // Reference the owned entry (entry stays in EvoInv->Entries — reference model, not move).
    Gear[Index].AttachedItem = MakeEvolutionAttachment(*Entry, EvoInstanceID);
    RefreshLoadoutsFromOwned(GearID);
    UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Attached evolution %s (tier %d) to %s %s"),
           *EvoInstanceID.ToString(), static_cast<int32>(Entry->Tier), GearLabel<TEntry>(), *GearID.ToString());
    BroadcastInventoryChanged(EInventoryChangeType::Equipped);
    return true;
}

template <class TEntry>
bool UInventoryComponent::AttachCrystalToGear(TArray<TEntry> &Gear, FGuid GearID, FCrystalId Id, const TCHAR *Op)
{
    if (!GearOpHasAuthority(GetOwner(), Op))
    {
        return false;
    }

    const bool bIsStone = !CrystalTypeHelpers::IsGemType(Id.Type);
    if constexpr (IsRingGear<TEntry>)
    {
        // Inline ring-guard (mirrors URingData::IsDataValid:29-33): augment stones are weapon-only.
        // Rings carry only refined gems (their spell source).
        if (bIsStone)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: augment stones are weapon-only — rejected on ring"), Op);
            return false;
        }
    }

    const int32 Index = FindEmptyGearSlot(Gear, GearID, Op);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    // DEBIT BEFORE WRITE via the SILENT atomic core (the component dispatches gem/stone internally).
    // Silent so the socket fires exactly ONE signal — the Equipped below — not Removed + Equipped.
    // false means the crystal isn't owned: reject with no phantom socket.
    if (!CommitRemoveCrystals({Id}))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: %s not owned — nothing debited, socket rejected"), Op,
               *ItemIdentity::GetDisplayName(Id));
        return false;
    }

    Gear[Index].AttachedItem = MakeCrystalAttachment(Id, bIsStone);
    RefreshLoadoutsFromOwned(GearID);
    UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Attached %s %s to %s %s"),
           bIsStone ? TEXT("augment stone") : TEXT("refined crystal"),
           *ItemIdentity::GetDisplayName(Id), GearLabel<TEntry>(), *GearID.ToString());
    BroadcastInventoryChanged(EInventoryChangeType::Equipped);
    return true;
}

template <class TEntry>
bool UInventoryComponent::AttachFusionToGear(TArray<TEntry> &Gear, FGuid GearID, FFusionId FusionId, const TCHAR *Op)
{
    if (!GearOpHasAuthority(GetOwner(), Op))
    {
        return false;
    }

    // Validate the pair before touching anything (stat-stone + one contributor; evolution can't be a half).
    if (!CrystalTypeHelpers::IsValidFusionPair(FusionId.HalfA.Type, FusionId.HalfB.Type))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: invalid fusion pair — rejected"), Op);
        return false;
    }

    if constexpr (IsRingGear<TEntry>)
    {
        // Inline ring-guard (mirrors URingData::IsDataValid:40-44): rings accept only ELEMENTAL fusions
        // (one gem half); augmented fusions (two stones) are weapon-only.
        if (!CrystalTypeHelpers::IsElementalFusion(FusionId.HalfA.Type, FusionId.HalfB.Type))
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: augmented fusions are weapon-only — rejected on ring"), Op);
            return false;
        }
    }

    const int32 Index = FindEmptyGearSlot(Gear, GearID, Op);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    // Consume both halves atomically (the fuse) via the SILENT core — the component dispatches each
    // half to Crystals or Stones internally. The core does the verify-then-commit and the identical-halves
    // count (HalfA == HalfB → needs 2) for free, and stays silent so the fuse fires one signal
    // (Equipped) below.
    if (!CommitRemoveCrystals({FusionId.HalfA, FusionId.HalfB}))
    {
        return false;
    }

    Gear[Index].AttachedItem = MakeFusionAttachment(FusionId);
    RefreshLoadoutsFromOwned(GearID);
    UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Fused %s + %s onto %s %s"),
           *ItemIdentity::GetDisplayName(FusionId.HalfA), *ItemIdentity::GetDisplayName(FusionId.HalfB),
           GearLabel<TEntry>(), *GearID.ToString());
    BroadcastInventoryChanged(EInventoryChangeType::Equipped);
    return true;
}

template <class TEntry>
bool UInventoryComponent::RemoveEvolutionFromGear(TArray<TEntry> &Gear, FGuid GearID, const TCHAR *Op)
{
    if (!GearOpHasAuthority(GetOwner(), Op))
    {
        return false;
    }

    const int32 Index = FindGearIndex(Gear, GearID, Op);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    FRuntimeAttachedItem &Attached = Gear[Index].AttachedItem;
    // Must be a PLAYER-ATTACHED evolution: valid InstanceID. Authored-locked (invalid InstanceID) is
    // a built-in evolution and cannot be removed.
    if (!Attached.IsEvolution() || !Attached.Evolution.InstanceID.IsValid())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] %s: %s %s has no player-attached evolution (authored-locked or none)"),
               Op, GearLabel<TEntry>(), *GearID.ToString());
        return false;
    }
    const FGuid EvoInstanceID = Attached.Evolution.InstanceID;

    // Resolve the owned entry (persisted all along — reference model).
    FEvolutionInventoryEntry *Entry = FindOwnedEvolution(GetOwner(), EvoInstanceID);
    if (!Entry)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: attachment InstanceID %s resolves to no owned entry"), Op,
               *EvoInstanceID.ToString());
        return false; // data inconsistency — do not clear the slot
    }

    // Copy the worn runtime durability back onto the persisted entry, less the removal wear.
    const int32 MaxDur = Entry->Item ? Entry->Item->MaxDurability : 0;
    Entry->CurrentDurability =
        FMath::Max(0, Attached.Evolution.CurrentDurability - FMath::CeilToInt(MaxDur * EVOLUTION_REMOVAL_WEAR_FRACTION));

    // Un-reference: clear the gear slot regardless (the evo leaves the weapon/ring).
    Gear[Index].RemoveCrystal();
    RefreshLoadoutsFromOwned(GearID);

    // Q-159: an evolution is equipment, never lost to wear. If the removal wear took it to 0 it
    // returns to the inventory broken (repairable), no longer dismantled (§5.3b, superseded).
    UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Removed evolution %s from %s %s -> inventory (durability %d%s, %.0f%% wear)"),
           *EvoInstanceID.ToString(), GearLabel<TEntry>(), *GearID.ToString(), Entry->CurrentDurability,
           Entry->CurrentDurability <= 0 ? TEXT(", broken") : TEXT(""), EVOLUTION_REMOVAL_WEAR_FRACTION * 100.0f);
    BroadcastInventoryChanged(EInventoryChangeType::Removed);
    return true;
}

template <class TEntry>
bool UInventoryComponent::RemoveCrystalFromGear(TArray<TEntry> &Gear, int32 Index)
{
    if (!Gear.IsValidIndex(Index))
    {
        return false;
    }

    const bool bHadAttachment = !Gear[Index].AttachedItem.IsEmpty();
    // Speed-relevance gate, read BEFORE the detach: a TurnSpeed-contributing stone
    // (plain TurnSpeedStone or a fusion with a TurnSpeed half/bonus — same
    // GetAttachedStonePercent read CalculateSpeedRatios uses) feeds the stone factor,
    // so its removal must recalc turn order. Non-speed stones skip the notify.
    const bool bWasSpeedStone =
        CrystalEffectTable::GetAttachedStonePercent(Gear[Index].AttachedItem, ESubStat::TurnSpeed) > 0.0f;

    Gear[Index].RemoveCrystal();
    RefreshLoadoutsFromOwned(Gear[Index].PersistentID);

    // Notify TurnManager so the stone factor re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (bWasSpeedStone)
    {
        if (UWorld *World = GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                if (UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>())
                {
                    TurnManager->OnActorSpeedChanged(GetOwner());
                }
            }
        }
    }

    if (bHadAttachment)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return bHadAttachment;
}

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
}

// ==================== SPELL OPERATIONS ====================

void UInventoryComponent::BroadcastInventoryChanged(EInventoryChangeType ChangeType) const
{
    OnInventoryChanged.Broadcast(ChangeType);
}

bool UInventoryComponent::LearnSpell(USpellData *Spell)
{
    const bool bLearned = Spells.LearnSpell(Spell);
    if (bLearned)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return bLearned;
}

bool UInventoryComponent::UnlearnSpell(USpellData *Spell)
{
    const bool bUnlearned = Spells.UnlearnSpell(Spell);
    if (bUnlearned)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return bUnlearned;
}

bool UInventoryComponent::HasSpell(USpellData *Spell) const
{
    return Spells.HasSpell(Spell);
}

TArray<USpellData *> UInventoryComponent::GetSpellsByElement(ESpellElement Element) const
{
    return Spells.GetSpellsByElement(Element);
}

// ==================== ABILITY OPERATIONS ====================

bool UInventoryComponent::LearnAbility(UAbilityData *Ability)
{
    const bool bLearned = Abilities.LearnAbility(Ability);
    if (bLearned)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return bLearned;
}

bool UInventoryComponent::UnlearnAbility(UAbilityData *Ability)
{
    const bool bUnlearned = Abilities.UnlearnAbility(Ability);
    if (bUnlearned)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return bUnlearned;
}

bool UInventoryComponent::HasAbility(UAbilityData *Ability) const
{
    return Abilities.HasAbility(Ability);
}

TArray<UAbilityData *> UInventoryComponent::GetAbilitiesForWeaponType(EWeaponType WeaponType) const
{
    return Abilities.GetAbilitiesForWeaponType(WeaponType);
}

// ==================== INSTANCE-TIER RESOLUTION (spell-instance arc, ii-b/c) ====================

EItemTier UInventoryComponent::ResolveSpellTier(const AActor *Caster, const USpellData *Spell)
{
    if (!Spell)
    {
        return EItemTier::F_Tier;
    }
    if (Caster)
    {
        if (const UInventoryComponent *Inv = Caster->FindComponentByClass<UInventoryComponent>())
        {
            EItemTier InstanceTier;
            if (Inv->Spells.TryGetSpellTier(Spell, InstanceTier))
            {
                return InstanceTier; // owned → the caster's leveled instance tier
            }
        }
    }
    return Spell->Tier; // not owned (enemy / authored loadout) → asset tier
}

EItemTier UInventoryComponent::ResolveAbilityTier(const AActor *Caster, const USkillDataBase *Skill)
{
    if (!Skill)
    {
        return EItemTier::F_Tier;
    }
    if (Caster)
    {
        if (const UInventoryComponent *Inv = Caster->FindComponentByClass<UInventoryComponent>())
        {
            EItemTier InstanceTier;
            // Basic attacks aren't learned (LearnAbility rejects them), so Cast→TryGet returns false
            // and we asset-fall-back below — exactly the intended behaviour.
            if (Inv->Abilities.TryGetAbilityTier(Cast<UAbilityData>(Skill), InstanceTier))
            {
                return InstanceTier;
            }
        }
    }
    return Skill->Tier; // not owned / basic attack → asset tier
}

// ==================== WEAPON OPERATIONS ====================

bool UInventoryComponent::AddWeapon(UWeaponData *Weapon)
{
    if (!Weapon)
    {
        return false;
    }

    // Check capacity
    if (!CanAddWeapon(Weapon))
    {
        return false;
    }

    FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
    // Acquisition is the ONE mint point for the persistent owned-instance guid —
    // CreateFromWeapon deliberately leaves it invalid (loadout inflation reuses
    // that factory and must not mint).
    Entry.PersistentID = FGuid::NewGuid();
    // Tier/Quality are seeded in CreateFromWeapon (the factory, called above) so loadout-inflated
    // entries get them too — see cluster 2a. Acquisition only mints the persistent guid here.
    if (Weapon->bRandomGenerateOnPickup)
    {
        ApplyPickupRoll(Weapon, Entry.StatBonus, Entry.ResistanceBonus, Entry.StatMaxPool, Entry.ResistanceMaxPool);
        // Fresh pickup → roll a per-instance Quality (§11), Luck-biased. Toggle-OFF acquisitions
        // (purchases, authored gear) skip this and keep the CreateFromWeapon C_Quality placeholder.
        Entry.Quality = EconomyYield::RollQuality(ResolveInventoryOwnerLuck(this));
    }
    Weapons.Add(Entry);
    BroadcastInventoryChanged(EInventoryChangeType::Added);
    return true;
}

bool UInventoryComponent::RemoveWeapon(int32 WeaponIndex)
{
    if (!Weapons.IsValidIndex(WeaponIndex))
    {
        return false;
    }

    Weapons.RemoveAt(WeaponIndex);
    // RemoveWeaponByPersistentID delegates here, so this single broadcast covers
    // both entry points (no double-fire).
    BroadcastInventoryChanged(EInventoryChangeType::Removed);
    return true;
}

bool UInventoryComponent::RemoveWeaponByPersistentID(FGuid PersistentID)
{
    const int32 Index = Weapons.IndexOfByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &Entry) { return Entry.PersistentID == PersistentID; });
    if (Index == INDEX_NONE)
    {
        return false;
    }
    return RemoveWeapon(Index);
}

FWeaponInventoryEntry UInventoryComponent::GetWeaponAt(int32 Index) const
{
    if (Weapons.IsValidIndex(Index))
    {
        return Weapons[Index];
    }
    return FWeaponInventoryEntry();
}

int32 UInventoryComponent::GetWeaponSlotCostTotal() const
{
    int32 Total = 0;
    for (const FWeaponInventoryEntry &Entry : Weapons)
    {
        Total += Entry.GetSlotCost();
    }
    return Total;
}

int32 UInventoryComponent::GetRemainingWeaponCapacity() const
{
    return InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS - GetWeaponSlotCostTotal();
}

bool UInventoryComponent::CanAddWeapon(UWeaponData *Weapon) const
{
    if (!Weapon)
    {
        return false;
    }

    // Base weapon costs 1 slot
    int32 Cost = InventoryConstants::WEAPON_BASE_SLOT_COST;
    return GetRemainingWeaponCapacity() >= Cost;
}

// ==================== WEAPON CRYSTAL OPERATIONS ====================

bool UInventoryComponent::RemoveCrystalFromWeapon(int32 WeaponIndex)
{
    return RemoveCrystalFromGear(Weapons, WeaponIndex);
}

// ==================== RING OPERATIONS ====================

bool UInventoryComponent::AddRing(URingData *Ring)
{
    if (!Ring)
    {
        return false;
    }

    // Check capacity
    if (!CanAddRing(Ring))
    {
        return false;
    }

    FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
    // Acquisition mint — see AddWeapon; CreateFromRing leaves the guid invalid.
    Entry.PersistentID = FGuid::NewGuid();
    // Tier/Quality are seeded in CreateFromRing (the factory, called above); see cluster 2a.
    if (Ring->bRandomGenerateOnPickup)
    {
        ApplyPickupRoll(Ring, Entry.StatBonus, Entry.ResistanceBonus, Entry.StatMaxPool, Entry.ResistanceMaxPool);
        // Fresh pickup → roll a per-instance Quality (§11), Luck-biased. Toggle-OFF acquisitions
        // keep the CreateFromRing C_Quality placeholder.
        Entry.Quality = EconomyYield::RollQuality(ResolveInventoryOwnerLuck(this));
    }
    Rings.Add(Entry);
    BroadcastInventoryChanged(EInventoryChangeType::Added);
    return true;
}

bool UInventoryComponent::RemoveRing(int32 RingIndex)
{
    if (!Rings.IsValidIndex(RingIndex))
    {
        return false;
    }

    Rings.RemoveAt(RingIndex);
    // RemoveRingByPersistentID delegates here — single broadcast covers both.
    BroadcastInventoryChanged(EInventoryChangeType::Removed);
    return true;
}

bool UInventoryComponent::RemoveRingByPersistentID(FGuid PersistentID)
{
    const int32 Index = Rings.IndexOfByPredicate(
        [&PersistentID](const FRingInventoryEntry &Entry) { return Entry.PersistentID == PersistentID; });
    if (Index == INDEX_NONE)
    {
        return false;
    }
    return RemoveRing(Index);
}

TArray<int32> UInventoryComponent::GetLoadoutsUsing(const FGuid &PersistentID) const
{
    TArray<int32> Result;
    for (int32 Index = 0; Index < SavedLoadouts.Num(); ++Index)
    {
        FCombatLoadout Copy = SavedLoadouts[Index];
        if (StripFromLoadout(Copy, PersistentID))
        {
            Result.Add(Index);
        }
    }
    return Result;
}

int32 UInventoryComponent::RemoveFromSavedLoadouts(const FGuid &PersistentID)
{
    int32 Changed = 0;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        Changed += StripFromLoadout(Loadout, PersistentID) ? 1 : 0;
    }
    if (Changed > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: took instance %s out of %d saved loadout(s)"),
               *GetNameSafe(GetOwner()), *PersistentID.ToString(), Changed);
    }
    return Changed;
}

bool UInventoryComponent::StripFromLoadout(FCombatLoadout &Loadout, const FGuid &PersistentID)
{
    if (!PersistentID.IsValid())
    {
        return false;
    }
    bool bRemoved = false;
    if (Loadout.PrimaryWeapon.WeaponEntry.PersistentID == PersistentID)
    {
        Loadout.PrimaryWeapon.Clear();
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            Loadout.PrimarySlotType = EPrimarySlotType::None;
        }
        bRemoved = true;
    }
    if (Loadout.PrimaryRing.RingEntry.PersistentID == PersistentID)
    {
        Loadout.PrimaryRing.Clear();
        if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            Loadout.PrimarySlotType = EPrimarySlotType::None;
        }
        bRemoved = true;
    }
    if (Loadout.SecondaryWeapon.WeaponEntry.PersistentID == PersistentID)
    {
        Loadout.SecondaryWeapon.Clear();
        Loadout.SecondarySlotType = ESecondarySlotType::None;
        bRemoved = true;
    }
    for (int32 RingIndex = Loadout.RingLoadout.Num() - 1; RingIndex >= 0; --RingIndex)
    {
        if (Loadout.RingLoadout[RingIndex].RingEntry.PersistentID != PersistentID)
        {
            continue;
        }
        Loadout.RingLoadout.RemoveAt(RingIndex);
        // Keep the same ring active when an earlier slot goes; clamp when the active one goes.
        if (RingIndex < Loadout.ActiveRingIndex)
        {
            --Loadout.ActiveRingIndex;
        }
        bRemoved = true;
    }
    Loadout.ActiveRingIndex = FMath::Clamp(Loadout.ActiveRingIndex, 0, FMath::Max(0, Loadout.RingLoadout.Num() - 1));
    return bRemoved;
}

bool UInventoryComponent::LoadoutHasGear(const FCombatLoadout &Loadout)
{
    const bool bPrimary = (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid()) ||
                          (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid()) ||
                          (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item);
    const bool bSecondary = Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid();
    // The active ring, as ABattleGameMode::CanEnterCombat reads it (GetActiveRing).
    const bool bRings = Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex) && Loadout.RingLoadout[Loadout.ActiveRingIndex].IsValid();
    return bPrimary || bSecondary || bRings;
}

// ==================== OWNED GEAR SYNC (B-085, Q-085) ====================

namespace InventoryGearSync
{
    const UObject *GetGearAsset(const FWeaponInventoryEntry &Entry) { return Entry.Weapon; }
    const UObject *GetGearAsset(const FRingInventoryEntry &Entry) { return Entry.Ring; }

    /** The owned weapon or ring with ID (const or not, as Owned is), or null. */
    template <typename TArrayType>
    auto FindOwned(TArrayType &Owned, const FGuid &ID)
    {
        return ID.IsValid() ? Owned.FindByPredicate([&ID](const auto &Entry) { return Entry.PersistentID == ID; })
                            : nullptr;
    }

    /** Calls WeaponFunc / RingFunc on each weapon and ring entry the loadout holds. */
    template <typename TLoadout, typename FWeaponFunc, typename FRingFunc>
    void ForEachGearSlot(TLoadout &Loadout, FWeaponFunc &&WeaponFunc, FRingFunc &&RingFunc)
    {
        WeaponFunc(Loadout.PrimaryWeapon.WeaponEntry);
        WeaponFunc(Loadout.SecondaryWeapon.WeaponEntry);
        RingFunc(Loadout.PrimaryRing.RingEntry);
        for (auto &Ring : Loadout.RingLoadout)
        {
            RingFunc(Ring.RingEntry);
        }
    }

    /** The owned entry onto the slot; the slot keeps its own spell picks (a Resonator ring slot's). */
    template <typename TEntry>
    void CopyOwned(TEntry &Slot, const TEntry &Owned)
    {
        TArray<USpellData *> Picks = MoveTemp(Slot.AssignedSpells);
        Slot = Owned;
        Slot.AssignedSpells = MoveTemp(Picks);
    }

    void CopyOwnedEvolution(FEvolutionAttachment &Slot, const FEvolutionInventoryEntry &Owned)
    {
        Slot.Tier = Owned.Tier;
        Slot.CurrentDurability = Owned.CurrentDurability;
        Slot.GeneratedStatBonus = Owned.GeneratedStatBonus;
        Slot.GeneratedResistance = Owned.GeneratedResistance;
        Slot.StatPool = Owned.StatPool;
        Slot.StatMaxPool = Owned.StatMaxPool;
        Slot.ResistancePool = Owned.ResistancePool;
        Slot.ResistanceMaxPool = Owned.ResistanceMaxPool;
    }

    /** Links an unlinked slot to the first owned entry of its asset this loadout doesn't use yet. */
    template <typename TEntry>
    bool LinkSlot(TEntry &Slot, const TArray<TEntry> &Owned, TSet<FGuid> &UsedHere)
    {
        if (!GetGearAsset(Slot) || FindOwned(Owned, Slot.PersistentID))
        {
            return false;
        }
        const TEntry *Match = Owned.FindByPredicate([&Slot, &UsedHere](const TEntry &Entry)
        {
            return Entry.PersistentID.IsValid() && GetGearAsset(Entry) == GetGearAsset(Slot) &&
                   !UsedHere.Contains(Entry.PersistentID);
        });
        if (!Match)
        {
            return false;
        }
        UsedHere.Add(Match->PersistentID);
        CopyOwned(Slot, *Match);
        return true;
    }

    template <typename TEntry, typename TArrayType>
    FString DescribeSlot(const TCHAR *Label, const TEntry &Slot, const TArrayType &Owned)
    {
        if (!GetGearAsset(Slot))
        {
            return FString();
        }
        const auto *Match = FindOwned(Owned, Slot.PersistentID);
        if (!Match)
        {
            return FString::Printf(TEXT("\n    %s %s: not linked (own copy)"), Label, *GetNameSafe(GetGearAsset(Slot)));
        }
        const int32 SlotDurability = Slot.AttachedItem.GetCurrentDurability();
        const int32 OwnedDurability = Match->AttachedItem.GetCurrentDurability();
        const bool bInSync = Match->Tier == Slot.Tier && Match->AttachedItem.Kind == Slot.AttachedItem.Kind &&
                             OwnedDurability == SlotDurability;
        return FString::Printf(TEXT("\n    %s %s -> owned %s: %s"), Label, *GetNameSafe(GetGearAsset(Slot)),
                               *Slot.PersistentID.ToString(EGuidFormats::Short),
                               bInSync ? TEXT("in sync")
                                       : *FString::Printf(TEXT("DIFFERS (tier %d / owned %d, durability %d / owned %d)"),
                                                          static_cast<int32>(Slot.Tier), static_cast<int32>(Match->Tier),
                                                          SlotDurability, OwnedDurability));
    }
}

int32 UInventoryComponent::LinkLoadoutsToOwned()
{
    using namespace InventoryGearSync;
    UEvolutionInventoryComponent *EvoInv = GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    int32 Linked = 0;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        // Slots already linked claim their entries first, so a second copy of a ring links to another one.
        TSet<FGuid> UsedHere;
        ForEachGearSlot(Loadout,
                        [this, &UsedHere](const FWeaponInventoryEntry &Slot) { if (FindOwned(Weapons, Slot.PersistentID)) { UsedHere.Add(Slot.PersistentID); } },
                        [this, &UsedHere](const FRingInventoryEntry &Slot) { if (FindOwned(Rings, Slot.PersistentID)) { UsedHere.Add(Slot.PersistentID); } });
        ForEachGearSlot(Loadout,
                        [this, &UsedHere, &Linked](FWeaponInventoryEntry &Slot) { Linked += LinkSlot(Slot, Weapons, UsedHere) ? 1 : 0; },
                        [this, &UsedHere, &Linked](FRingInventoryEntry &Slot) { Linked += LinkSlot(Slot, Rings, UsedHere) ? 1 : 0; });

        const bool bEvolutionSlot = Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item;
        if (!EvoInv || !bEvolutionSlot || FindOwnedEvolution(GetOwner(), Loadout.PrimaryEvolutionInstance))
        {
            continue;
        }
        const FEvolutionInventoryEntry *Match = EvoInv->Entries.FindByPredicate([this, &Loadout](const FEvolutionInventoryEntry &Entry)
        {
            return Entry.Item == Loadout.PrimaryEvolution.Item && !IsEvolutionSlottedAnywhere(*this, Entry.InstanceID);
        });
        if (Match)
        {
            Loadout.PrimaryEvolutionInstance = Match->InstanceID;
            CopyOwnedEvolution(Loadout.PrimaryEvolution, *Match);
            ++Linked;
        }
    }
    if (Linked > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: linked %d loadout slot(s) to owned gear"), *GetNameSafe(GetOwner()), Linked);
    }
    return Linked;
}

int32 UInventoryComponent::RefreshLoadoutsFromOwned(const FGuid &GearID)
{
    using namespace InventoryGearSync;
    const FWeaponInventoryEntry *OwnedWeapon = FindOwned(Weapons, GearID);
    const FRingInventoryEntry *OwnedRing = FindOwned(Rings, GearID);
    if (!OwnedWeapon && !OwnedRing)
    {
        return RemoveFromSavedLoadouts(GearID);
    }
    int32 Changed = 0;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        ForEachGearSlot(Loadout,
                        [&GearID, OwnedWeapon, &Changed](FWeaponInventoryEntry &Slot)
                        {
                            if (OwnedWeapon && Slot.PersistentID == GearID)
                            {
                                CopyOwned(Slot, *OwnedWeapon);
                                ++Changed;
                            }
                        },
                        [&GearID, OwnedRing, &Changed](FRingInventoryEntry &Slot)
                        {
                            if (OwnedRing && Slot.PersistentID == GearID)
                            {
                                CopyOwned(Slot, *OwnedRing);
                                ++Changed;
                            }
                        });
    }
    return Changed;
}

int32 UInventoryComponent::RefreshEvolutionInLoadouts(const FGuid &EvoInstanceID)
{
    if (!EvoInstanceID.IsValid())
    {
        return 0;
    }
    const FEvolutionInventoryEntry *Owned = FindOwnedEvolution(GetOwner(), EvoInstanceID);
    int32 Changed = 0;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        if (Loadout.PrimaryEvolutionInstance != EvoInstanceID)
        {
            continue;
        }
        if (Owned)
        {
            InventoryGearSync::CopyOwnedEvolution(Loadout.PrimaryEvolution, *Owned);
        }
        else
        {
            Loadout.PrimaryEvolution = FEvolutionAttachment();
            Loadout.PrimaryEvolutionInstance = FGuid();
        }
        ++Changed;
    }
    if (!Owned && Changed > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: evolution %s no longer owned, taken out of %d loadout(s)"),
               *GetNameSafe(GetOwner()), *EvoInstanceID.ToString(), Changed);
    }
    return Changed;
}

int32 UInventoryComponent::WriteLoadoutToOwned(int32 LoadoutIndex)
{
    using namespace InventoryGearSync;
    if (!SavedLoadouts.IsValidIndex(LoadoutIndex))
    {
        return 0;
    }
    const FCombatLoadout &Loadout = SavedLoadouts[LoadoutIndex];
    TArray<FGuid> Written;
    ForEachGearSlot(Loadout,
                    [this, &Written](const FWeaponInventoryEntry &Slot)
                    {
                        if (FWeaponInventoryEntry *Owned = FindOwned(Weapons, Slot.PersistentID))
                        {
                            Owned->AttachedItem = Slot.AttachedItem;
                            Written.Add(Slot.PersistentID);
                        }
                    },
                    [this, &Written](const FRingInventoryEntry &Slot)
                    {
                        if (FRingInventoryEntry *Owned = FindOwned(Rings, Slot.PersistentID))
                        {
                            Owned->AttachedItem = Slot.AttachedItem;
                            Written.Add(Slot.PersistentID);
                        }
                    });
    int32 WrittenCount = Written.Num();
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        if (FEvolutionInventoryEntry *Owned = FindOwnedEvolution(GetOwner(), Loadout.PrimaryEvolutionInstance))
        {
            Owned->CurrentDurability = Loadout.PrimaryEvolution.CurrentDurability;
            ++WrittenCount;
        }
    }
    // The other loadouts holding the same gear (this one already matches).
    for (const FGuid &GearID : Written)
    {
        RefreshLoadoutsFromOwned(GearID);
    }
    UE_LOG(LogTemp, Verbose, TEXT("[Inventory] %s: loadout %d wrote %d owned entr(ies)"), *GetNameSafe(GetOwner()),
           LoadoutIndex, WrittenCount);
    return WrittenCount;
}

FString UInventoryComponent::GetOwnedGearSyncString() const
{
    using namespace InventoryGearSync;
    FString Out = FString::Printf(TEXT("Owned gear sync (%s): %d loadout(s), active %d"), *GetNameSafe(GetOwner()),
                                  SavedLoadouts.Num(), ActiveLoadoutIndex);
    for (int32 Index = 0; Index < SavedLoadouts.Num(); ++Index)
    {
        const FCombatLoadout &Loadout = SavedLoadouts[Index];
        Out += FString::Printf(TEXT("\n  [%d] %s"), Index, *Loadout.LoadoutName);
        Out += DescribeSlot(TEXT("Primary weapon"), Loadout.PrimaryWeapon.WeaponEntry, Weapons);
        Out += DescribeSlot(TEXT("Secondary weapon"), Loadout.SecondaryWeapon.WeaponEntry, Weapons);
        Out += DescribeSlot(TEXT("Primary ring"), Loadout.PrimaryRing.RingEntry, Rings);
        for (const FRingLoadoutEntry &Ring : Loadout.RingLoadout)
        {
            Out += DescribeSlot(TEXT("Ring"), Ring.RingEntry, Rings);
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
        {
            const FEvolutionInventoryEntry *Owned =
                FindOwnedEvolution(GetOwner(), Loadout.PrimaryEvolutionInstance);
            Out += Owned ? FString::Printf(TEXT("\n    Evolution %s -> owned %s: %s"), *GetNameSafe(Loadout.PrimaryEvolution.Item),
                                           *Loadout.PrimaryEvolutionInstance.ToString(EGuidFormats::Short),
                                           Owned->Tier == Loadout.PrimaryEvolution.Tier &&
                                                   Owned->CurrentDurability == Loadout.PrimaryEvolution.CurrentDurability
                                               ? TEXT("in sync")
                                               : TEXT("DIFFERS (written back at the fight's end)"))
                         : FString::Printf(TEXT("\n    Evolution %s: not linked (own copy)"), *GetNameSafe(Loadout.PrimaryEvolution.Item));
        }
    }
    return Out;
}

FRingInventoryEntry UInventoryComponent::GetRingAt(int32 Index) const
{
    if (Rings.IsValidIndex(Index))
    {
        return Rings[Index];
    }
    return FRingInventoryEntry();
}

int32 UInventoryComponent::GetRingSlotCostTotal() const
{
    int32 Total = 0;
    for (const FRingInventoryEntry &Entry : Rings)
    {
        Total += Entry.GetSlotCost();
    }
    return Total;
}

int32 UInventoryComponent::GetRemainingRingCapacity() const
{
    return InventoryConstants::MAX_RING_INVENTORY_SLOTS - GetRingSlotCostTotal();
}

bool UInventoryComponent::AttachEvolutionToWeapon(FGuid WeaponPersistentID, FGuid EvoInstanceID)
{
    return AttachEvolutionToGear(Weapons, WeaponPersistentID, EvoInstanceID, TEXT("AttachEvolutionToWeapon"));
}

bool UInventoryComponent::AttachEvolutionToRing(FGuid RingPersistentID, FGuid EvoInstanceID)
{
    return AttachEvolutionToGear(Rings, RingPersistentID, EvoInstanceID, TEXT("AttachEvolutionToRing"));
}

// ==================== CRYSTAL / FUSION ATTACH (debit model) ====================

bool UInventoryComponent::AttachCrystalToWeapon(FGuid WeaponPersistentID, FCrystalId Id)
{
    return AttachCrystalToGear(Weapons, WeaponPersistentID, Id, TEXT("AttachCrystalToWeapon"));
}

bool UInventoryComponent::AttachCrystalToRing(FGuid RingPersistentID, FCrystalId Id)
{
    return AttachCrystalToGear(Rings, RingPersistentID, Id, TEXT("AttachCrystalToRing"));
}

bool UInventoryComponent::CommitRemoveCrystals(const TArray<FCrystalId> &Ids)
{
    if (Ids.Num() == 0)
    {
        return true; // vacuous — nothing requested, nothing changed (no broadcast at the wrapper)
    }

    UCrystalInventoryComponent *CrystalInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    if (!CrystalInv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] RemoveCrystals: no UCrystalInventoryComponent — nothing removed"));
        return false;
    }

    // Tally required count per distinct Id — so duplicates in the set (incl. identical fusion halves,
    // HalfA == HalfB) demand that many, not one each. The component dispatches gem/stone internally.
    TMap<FCrystalId, int32> Required;
    for (const FCrystalId &Id : Ids)
    {
        Required.FindOrAdd(Id)++;
    }

    // VERIFY: every Id's pool must hold at least the required count. Any shortfall aborts BEFORE any
    // removal — atomic, no partial consume.
    for (const TPair<FCrystalId, int32> &Pair : Required)
    {
        const int32 Have = CrystalInv->GetCount(Pair.Key);
        if (Have < Pair.Value)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] RemoveCrystals: %s needs %d, have %d — atomic remove rejected, nothing removed"),
                   *ItemIdentity::GetDisplayName(Pair.Key), Pair.Value, Have);
            return false;
        }
    }

    // COMMIT: all verified — debit through the RAW SILENT unified method (no per-item broadcast). The
    // single change signal is the caller's responsibility (RemoveCrystals fires one Removed; the
    // attach-ops fire one Equipped instead).
    for (const TPair<FCrystalId, int32> &Pair : Required)
    {
        CrystalInv->RemoveCount(Pair.Key, Pair.Value);
    }
    return true;
}

bool UInventoryComponent::RemoveCrystals(const TArray<FCrystalId> &Ids)
{
    if (!CommitRemoveCrystals(Ids))
    {
        return false;
    }
    // ONE signal for the whole atomic remove (not per-item). Skip on an empty set (no mutation).
    if (Ids.Num() > 0)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return true;
}

bool UInventoryComponent::CommitAddCrystals(const TArray<FCrystalId> &Ids)
{
    if (Ids.Num() == 0)
    {
        return true; // vacuous — nothing requested, nothing changed
    }

    UCrystalInventoryComponent *CrystalInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    if (!CrystalInv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AddCrystals: no UCrystalInventoryComponent — nothing added"));
        return false;
    }

    // Tally per Id — same-Id duplicates sum so the cap check sees the real demand.
    TMap<FCrystalId, int32> Required;
    for (const FCrystalId &Id : Ids)
    {
        Required.FindOrAdd(Id)++;
    }

    // VERIFY: adds CAN fail on the per-tier cap (CRYSTAL_PER_TIER_CAP) — confirm every tally fits its
    // pool BEFORE adding any, so the op is all-or-nothing (no partial add).
    for (const TPair<FCrystalId, int32> &Pair : Required)
    {
        if (!CrystalInv->CanAddCount(Pair.Key, Pair.Value))
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AddCrystals: %s x%d exceeds the per-tier cap — atomic add rejected, nothing added"),
                   *ItemIdentity::GetDisplayName(Pair.Key), Pair.Value);
            return false;
        }
    }

    // COMMIT: all verified to fit — add via the RAW SILENT unified method (no per-item broadcast; the
    // AddCrystals wrapper fires one Added).
    for (const TPair<FCrystalId, int32> &Pair : Required)
    {
        CrystalInv->AddCount(Pair.Key, Pair.Value);
    }
    return true;
}

bool UInventoryComponent::AddCrystals(const TArray<FCrystalId> &Ids)
{
    if (!CommitAddCrystals(Ids))
    {
        return false;
    }
    if (Ids.Num() > 0)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return true;
}

bool UInventoryComponent::AttachFusionToWeapon(FGuid WeaponPersistentID, FFusionId FusionId)
{
    return AttachFusionToGear(Weapons, WeaponPersistentID, FusionId, TEXT("AttachFusionToWeapon"));
}

bool UInventoryComponent::AttachFusionToRing(FGuid RingPersistentID, FFusionId FusionId)
{
    return AttachFusionToGear(Rings, RingPersistentID, FusionId, TEXT("AttachFusionToRing"));
}

bool UInventoryComponent::RemoveEvolutionFromWeapon(FGuid WeaponPersistentID)
{
    return RemoveEvolutionFromGear(Weapons, WeaponPersistentID, TEXT("RemoveEvolutionFromWeapon"));
}

bool UInventoryComponent::RemoveEvolutionFromRing(FGuid RingPersistentID)
{
    return RemoveEvolutionFromGear(Rings, RingPersistentID, TEXT("RemoveEvolutionFromRing"));
}

bool UInventoryComponent::CanAddRing(URingData *Ring) const
{
    if (!Ring)
    {
        return false;
    }

    // Base ring costs 1 slot
    int32 Cost = InventoryConstants::RING_BASE_SLOT_COST;
    return GetRemainingRingCapacity() >= Cost;
}

// ==================== RING CRYSTAL OPERATIONS ====================

bool UInventoryComponent::RemoveCrystalFromRing(int32 RingIndex)
{
    // Ring attachments do not feed the turn-speed read path today (CalculateSpeedRatios reads the
    // active WEAPON attachment only), so the speed notify inside is defensive symmetry.
    return RemoveCrystalFromGear(Rings, RingIndex);
}

// ==================== EVOLUTION HELPERS ====================

TArray<UEvolutionItemData *> UInventoryComponent::GetEvolutionCrystals() const
{
    TArray<UEvolutionItemData *> Result;

    const UEvolutionInventoryComponent *EvolutionInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    if (EvolutionInv)
    {
        for (const FEvolutionInventoryEntry &Entry : EvolutionInv->Entries)
        {
            if (Entry.Item)
            {
                Result.Add(Entry.Item);
            }
        }
    }
    return Result;
}

// ==================== CRYSTAL / EVOLUTION FACADE ====================

bool UInventoryComponent::AddCrystal(FCrystalId Id, int32 Count)
{
    UCrystalInventoryComponent *CrystalInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    if (!CrystalInv)
    {
        return false;
    }
    const bool bAdded = CrystalInv->AddCount(Id, Count);
    if (bAdded)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return bAdded;
}

int32 UInventoryComponent::RemoveCrystal(FCrystalId Id, int32 Count)
{
    UCrystalInventoryComponent *CrystalInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    if (!CrystalInv)
    {
        return 0;
    }
    const int32 Removed = CrystalInv->RemoveCount(Id, Count);
    if (Removed > 0)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return Removed;
}

bool UInventoryComponent::AddEvolutionInstance(UEvolutionItemData *Item)
{
    UEvolutionInventoryComponent *EvolutionInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    if (!EvolutionInv)
    {
        return false;
    }
    const bool bAdded = EvolutionInv->AddInstance(Item);
    if (bAdded)
    {
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return bAdded;
}

bool UInventoryComponent::RemoveEvolutionInstance(FGuid InstanceID)
{
    UEvolutionInventoryComponent *EvolutionInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    if (!EvolutionInv)
    {
        return false;
    }
    const bool bRemoved = EvolutionInv->RemoveInstance(InstanceID);
    if (bRemoved)
    {
        RefreshEvolutionInLoadouts(InstanceID);
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return bRemoved;
}

// ==================== UTILITY ====================

FString UInventoryComponent::GetInventorySummary() const
{
    const int32 EvolutionCount = GetEvolutionCrystals().Num();

    // Item count from the new count-based pool (item + refined). Evolution is
    // reported on its own line below, so it is not folded into the item total.
    const UCrystalInventoryComponent *CrystalInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    const int32 ItemCount = CrystalInv ? CrystalInv->GetTotalCount() : 0;

    return FString::Printf(
        TEXT("Inventory Summary:\n")
            TEXT("  Spells: %d/%d\n")
                TEXT("  Abilities: %d/%d\n")
                    TEXT("  Weapons: %d (cost %d/%d)\n")
                        TEXT("  Rings: %d (cost %d/%d)\n")
                            TEXT("  Items: %d/%d\n")
                                TEXT("  Evolution Crystals: %d"),
        Spells.GetCount(), InventoryConstants::MAX_LEARNED_SPELLS,
        Abilities.GetCount(), InventoryConstants::MAX_LEARNED_ABILITIES,
        Weapons.Num(), GetWeaponSlotCostTotal(), InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS,
        Rings.Num(), GetRingSlotCostTotal(), InventoryConstants::MAX_RING_INVENTORY_SLOTS,
        ItemCount, InventoryConstants::ITEM_CAPACITY_TOTAL,
        EvolutionCount);
}

void UInventoryComponent::InitializeFromCharacterData(UCharacterData *CharacterData)
{
    if (!CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeFromCharacterData: Null CharacterData"));
        return;
    }

    if (!CharacterData->Inventory)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] Character '%s' has no Inventory asset - inventory will be empty"),
               *CharacterData->Name);
        return;
    }

    InitializeFromInventoryAsset(CharacterData);
}

void UInventoryComponent::InitializeFromInventoryAsset(UCharacterData *CharacterData)
{
    UInventoryData *InventoryAsset = CharacterData ? CharacterData->Inventory : nullptr;
    if (!InventoryAsset)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromInventoryAsset: null Inventory asset (CharacterData=%s)"),
               CharacterData ? *CharacterData->Name : TEXT("null"));
        return;
    }

    // Validate before population — surfaces ownership/loadout cross-check
    // failures alongside the init log rather than as downstream symptoms.
    // B-028: BD rules follow the character.
    const TOptional<bool> bCharacterIsBD = CharacterData ? TOptional<bool>(CharacterData->bBrokenDarknessInnate) : TOptional<bool>();
    // Authored levels only (live stats aren't seeded yet): a head-start can make the fight
    // accept what this log flags.
    const int32 AuthoredDiscount = CharacterData ? CharacterData->GetAuthoredSpellDiscount() : 0;
    for (const FString &Error : InventoryAsset->GetValidationErrors(bCharacterIsBD, AuthoredDiscount))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryData] '%s' validation: %s"),
               *InventoryAsset->GetName(), *Error);
    }

    // Clear ownership lists.
    Spells.LearnedSpells.Empty();
    Abilities.LearnedAbilities.Empty();
    Weapons.Empty();
    Rings.Empty();

    // Resolve the new inventory components once. Either may be null on
    // misconfigured actors — the per-block guards below warn when the
    // required sibling is missing; we log here so the missing component
    // surfaces alongside init rather than as a downstream symptom.
    AActor *Owner = GetOwner();
    UCrystalInventoryComponent *CrystalInv = Owner
        ? Owner->FindComponentByClass<UCrystalInventoryComponent>()
        : nullptr;
    UEvolutionInventoryComponent *EvolutionInv = Owner
        ? Owner->FindComponentByClass<UEvolutionInventoryComponent>()
        : nullptr;
    if (!CrystalInv || !EvolutionInv)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): inventory components missing on owner (CrystalInv=%s, EvolutionInv=%s) — items routed to missing components will be dropped"),
               *CharacterData->Name,
               CrystalInv ? TEXT("present") : TEXT("MISSING"),
               EvolutionInv ? TEXT("present") : TEXT("MISSING"));
    }

    // Clear the new pools so re-init doesn't accumulate.
    if (CrystalInv)
    {
        CrystalInv->ClearAll();
    }
    if (EvolutionInv)
    {
        EvolutionInv->Entries.Empty();
    }

    // ---------- Weapons ----------
    for (UWeaponData *Weapon : InventoryAsset->Weapons)
    {
        if (Weapon)
        {
            AddWeapon(Weapon);
        }
    }

    // ---------- Rings ----------
    for (URingData *Ring : InventoryAsset->Rings)
    {
        if (Ring)
        {
            AddRing(Ring);
        }
    }

    // ---------- Crystals / evolution ----------
    // CrystalStock → CrystalInv->AddCount per (Id, Count); the component splits gem→Crystals /
    // stone→Stones by IsGemType. (The asset's PostLoad already folded the deprecated
    // ItemCrystals + RefinedCrystals into CrystalStock, so one loop reads the merged stock.)
    // EvolutionEquipment → EvolutionInv->AddInstance per entry. Sibling components are warned
    // about above when missing; per-block guards surface the misconfig with the dropped data.
    if (CrystalInv)
    {
        for (const TPair<FCrystalId, int32> &Pair : InventoryAsset->CrystalStock)
        {
            if (Pair.Value > 0 && !CrystalInv->AddCount(Pair.Key, Pair.Value))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): CrystalStock (Type=%d, Tier=%d) count %d rejected (per-tier cap)"),
                       *CharacterData->Name,
                       static_cast<int32>(Pair.Key.Type),
                       static_cast<int32>(Pair.Key.Tier),
                       Pair.Value);
            }
        }
    }
    else if (InventoryAsset->CrystalStock.Num() > 0)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): CrystalStock authored but UCrystalInventoryComponent missing — data dropped"),
               *CharacterData->Name);
    }

    if (EvolutionInv)
    {
        for (UEvolutionItemData *Item : InventoryAsset->EvolutionEquipment)
        {
            if (Item && !EvolutionInv->AddInstance(Item))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): evolution item '%s' rejected (MAX_EVOLUTION_ITEMS reached)"),
                       *CharacterData->Name, *Item->ItemName);
            }
        }
    }
    else if (InventoryAsset->EvolutionEquipment.Num() > 0)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): EvolutionEquipment authored but UEvolutionInventoryComponent missing — data dropped"),
               *CharacterData->Name);
    }

    // ---------- Spells ----------
    // LearnSpell short-circuits on null/capacity/duplicate; HasSpell
    // discriminates dedup (silent) from capacity drop (warn).
    for (USpellData *Spell : InventoryAsset->Spells)
    {
        if (Spell && !LearnSpell(Spell) && !HasSpell(Spell))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): spell capacity full, dropping %s"),
                   *CharacterData->Name, *Spell->Name);
        }
    }

    // ---------- Abilities ----------
    // Explicit-only — no implicit seeding from weapon PresetAbilities per
    // locked decision. Designer must list every ability in
    // UInventoryData::Abilities.
    for (UAbilityData *Ability : InventoryAsset->Abilities)
    {
        if (Ability && !LearnAbility(Ability) && !HasAbility(Ability))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): ability capacity full, dropping %s"),
                   *CharacterData->Name, *Ability->Name);
        }
    }

    // ---------- SavedLoadouts ----------
    // Inflate each FSavedLoadout into a runtime FCombatLoadout. Stance
    // overrides flow through CreateFromSavedLoadout (commit 1.5).
    // Shape-B context: this component's owned Weapons/Rings plus the sibling
    // evolution inventory let valid instance refs resolve to owned entries
    // (unset/unfound refs fall back to the asset build — pre-shape-B path).
    SavedLoadouts.Empty();
    const UEvolutionInventoryComponent *OwnedEvolutions =
        GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    for (const FSavedLoadout &SavedLoadout : InventoryAsset->SavedLoadouts)
    {
        SavedLoadouts.Add(FCombatLoadout::CreateFromSavedLoadout(SavedLoadout, this, OwnedEvolutions));
    }

    // Active index — clamp to valid range. Empty SavedLoadouts is soft-fail
    // (legacy parity); LoadoutComponent will seed an empty default downstream.
    if (SavedLoadouts.Num() > 0)
    {
        ActiveLoadoutIndex = FMath::Clamp(InventoryAsset->DefaultActiveLoadoutIndex, 0, SavedLoadouts.Num() - 1);
    }
    else
    {
        ActiveLoadoutIndex = 0;
    }

    const int32 CrystalStacks = CrystalInv ? CrystalInv->GetStackCount() : 0;
    const int32 EvolutionCount = EvolutionInv ? EvolutionInv->Num() : 0;

    UE_LOG(LogTemp, Display,
           TEXT("[InventoryComponent] Initialized inventory from %s: %d weapons, %d rings, pool-entries=[crystal-stacks:%d evolution:%d], %d spells, %d abilities, %d loadouts (active=%d)"),
           *InventoryAsset->GetName(),
           Weapons.Num(),
           Rings.Num(),
           CrystalStacks,
           EvolutionCount,
           Spells.GetCount(),
           Abilities.GetCount(),
           SavedLoadouts.Num(),
           ActiveLoadoutIndex);

    LinkLoadoutsToOwned();

    // One signal for the whole bulk (re)load — NOT per entry. InitializeFromCharacterData
    // delegates here, so this single broadcast covers the seed path too (no double-fire).
    BroadcastInventoryChanged(EInventoryChangeType::Loaded);
}

void UInventoryComponent::InitializeFromPool(UPoolSubsystem *Pool)
{
    if (!Pool)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] InitializeFromPool: null Pool subsystem"));
        return;
    }

    // Clear run ownership lists — same as the authored path.
    Spells.LearnedSpells.Empty();
    Abilities.LearnedAbilities.Empty();
    Weapons.Empty();
    Rings.Empty();

    // Resolve sibling inventory components once (crystal + evolution pools).
    AActor *Owner = GetOwner();
    UCrystalInventoryComponent *CrystalInv = Owner
        ? Owner->FindComponentByClass<UCrystalInventoryComponent>()
        : nullptr;
    UEvolutionInventoryComponent *EvolutionInv = Owner
        ? Owner->FindComponentByClass<UEvolutionInventoryComponent>()
        : nullptr;
    if (!CrystalInv || !EvolutionInv)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromPool: inventory components missing on owner (CrystalInv=%s, EvolutionInv=%s) — pool data routed to missing components will be dropped"),
               CrystalInv ? TEXT("present") : TEXT("MISSING"),
               EvolutionInv ? TEXT("present") : TEXT("MISSING"));
    }
    if (CrystalInv)
    {
        CrystalInv->ClearAll();
    }
    if (EvolutionInv)
    {
        EvolutionInv->Entries.Empty();
    }

    // ---------- Whole-entry draw from the pool ----------
    // CRITICAL: copy the pool's OWNED INSTANCES verbatim (NOT via the asset factories),
    // so PersistentID / Tier / Quality / InstanceID survive the draw. Re-running
    // AddWeapon/AddRing/LearnSpell would mint fresh GUIDs and reset tier to asset-base,
    // destroying leveled/rolled state. Each owned store is structurally identical to its
    // pool counterpart, so a plain Append (arrays) / assignment (maps) is the faithful
    // whole-store transfer.
    Weapons.Append(Pool->GetOwnedWeapons());
    Rings.Append(Pool->GetOwnedRings());
    Spells.LearnedSpells.Append(Pool->GetOwnedSpells());
    Abilities.LearnedAbilities.Append(Pool->GetOwnedAbilities());

    if (CrystalInv)
    {
        // Crystal pools are TMap<FCrystalId,int32> on both sides — structural parity, so a
        // direct map copy (assignment) is the whole-store transfer. ClearAll() above means
        // assignment (not merge) is correct. Two pools now (gem-merge): Crystals + Stones.
        CrystalInv->Crystals = Pool->GetCrystals();
        CrystalInv->Stones = Pool->GetStones();
    }
    if (EvolutionInv)
    {
        EvolutionInv->Entries.Append(Pool->GetOwnedEvolutions());
    }

    // ---------- SavedLoadouts stay AUTHORED ----------
    // The pool holds owned INSTANCES, not loadout PRESETS. Inflate the authored
    // SavedLoadouts (owner's CharacterData->Inventory) against the freshly pool-drawn
    // owned inventory — identical to the authored tail, just resolved against drawn
    // entries. Valid instance refs resolve to the drawn owned entries; unset/unfound
    // refs fall back to the asset build (pre-shape-B path).
    UCharacterDataComponent *CharComp = Owner
        ? Owner->FindComponentByClass<UCharacterDataComponent>()
        : nullptr;
    UCharacterData *CharData = CharComp ? CharComp->CharacterData : nullptr;
    UInventoryData *InventoryAsset = CharData ? CharData->Inventory : nullptr;

    SavedLoadouts.Empty();
    if (InventoryAsset)
    {
        for (const FSavedLoadout &SavedLoadout : InventoryAsset->SavedLoadouts)
        {
            SavedLoadouts.Add(FCombatLoadout::CreateFromSavedLoadout(SavedLoadout, this, EvolutionInv));
        }
        ActiveLoadoutIndex = SavedLoadouts.Num() > 0
            ? FMath::Clamp(InventoryAsset->DefaultActiveLoadoutIndex, 0, SavedLoadouts.Num() - 1)
            : 0;
    }
    else
    {
        ActiveLoadoutIndex = 0;
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromPool: no authored CharacterData->Inventory resolved on owner — SavedLoadouts left empty (owned inventory still drawn from pool)"));
    }

    const int32 CrystalStacks = CrystalInv ? CrystalInv->GetStackCount() : 0;
    const int32 EvolutionCount = EvolutionInv ? EvolutionInv->Num() : 0;

    UE_LOG(LogTemp, Display,
           TEXT("[InventoryComponent] Drew inventory FROM POOL: %d weapons, %d rings, pool-entries=[crystal-stacks:%d evolution:%d], %d spells, %d abilities, %d loadouts (active=%d)"),
           Weapons.Num(),
           Rings.Num(),
           CrystalStacks,
           EvolutionCount,
           Spells.GetCount(),
           Abilities.GetCount(),
           SavedLoadouts.Num(),
           ActiveLoadoutIndex);

    LinkLoadoutsToOwned();

    // One signal for the whole pool draw — NOT per entry.
    BroadcastInventoryChanged(EInventoryChangeType::Loaded);
}

FString UInventoryComponent::GetInventoryInstanceString() const
{
    const UEnum *CrystalEnum = StaticEnum<ECrystalType>();

    auto QualityName = [](EItemQuality Q) -> const TCHAR *
    {
        switch (Q)
        {
        case EItemQuality::F_Quality: return TEXT("F");
        case EItemQuality::E_Quality: return TEXT("E");
        case EItemQuality::D_Quality: return TEXT("D");
        case EItemQuality::C_Quality: return TEXT("C");
        case EItemQuality::B_Quality: return TEXT("B");
        case EItemQuality::A_Quality: return TEXT("A");
        case EItemQuality::S_Quality: return TEXT("S");
        default: return TEXT("?");
        }
    };

    // Short, stable identity tag — first 8 hex of the GUID, or "invalid" for an
    // unminted (ephemeral / loadout-inflated) entry. Enough to eyeball survival.
    auto ShortGuid = [](const FGuid &Id) -> FString
    {
        return Id.IsValid() ? Id.ToString(EGuidFormats::Digits).Left(8) : FString(TEXT("invalid"));
    };

    auto DescribeAttachment = [&](const FRuntimeAttachedItem &A) -> FString
    {
        if (A.IsEmpty())
        {
            return TEXT("none");
        }
        if (A.IsCrystal() || A.IsAugmentStone())
        {
            return FString::Printf(TEXT("%s %s %s"),
                                   A.IsAugmentStone() ? TEXT("stone") : TEXT("crystal"),
                                   *CrystalEnum->GetAuthoredNameStringByValue(static_cast<int64>(A.Crystal.Id.Type)),
                                   *TierHelpers::GetTierName(A.Crystal.Id.Tier));
        }
        if (A.IsEvolution())
        {
            return FString::Printf(TEXT("evolution %s"),
                                   A.Evolution.Item ? *A.Evolution.Item->GetFullItemName() : TEXT("(null)"));
        }
        if (A.IsFusion())
        {
            return TEXT("fusion");
        }
        return TEXT("unknown");
    };

    FString Out = TEXT("[Inventory]");

    // ---------- Weapons ----------
    Out += FString::Printf(TEXT("\n  Weapons (%d):"), Weapons.Num());
    for (const FWeaponInventoryEntry &W : Weapons)
    {
        Out += FString::Printf(TEXT("\n    %s  PID=%s Tier=%s Q=%s  crystal=%s"),
                               W.Weapon ? *W.Weapon->Name : TEXT("(null)"),
                               *ShortGuid(W.PersistentID),
                               *TierHelpers::GetTierName(W.Tier),
                               QualityName(W.Quality),
                               *DescribeAttachment(W.AttachedItem));
    }

    // ---------- Rings ----------
    Out += FString::Printf(TEXT("\n  Rings (%d):"), Rings.Num());
    for (const FRingInventoryEntry &R : Rings)
    {
        Out += FString::Printf(TEXT("\n    %s  PID=%s Tier=%s Q=%s  crystal=%s"),
                               R.Ring ? *R.Ring->Name : TEXT("(null)"),
                               *ShortGuid(R.PersistentID),
                               *TierHelpers::GetTierName(R.Tier),
                               QualityName(R.Quality),
                               *DescribeAttachment(R.AttachedItem));
    }

    // ---------- Spells ----------
    Out += FString::Printf(TEXT("\n  Spells (%d):"), Spells.LearnedSpells.Num());
    for (const FSpellInstance &S : Spells.LearnedSpells)
    {
        Out += FString::Printf(TEXT("\n    %s  IID=%s Tier=%s Q=%s"),
                               S.Spell ? *S.Spell->Name : TEXT("(null)"),
                               *ShortGuid(S.InstanceID),
                               *TierHelpers::GetTierName(S.Tier),
                               QualityName(S.Quality));
    }

    // ---------- Abilities ----------
    Out += FString::Printf(TEXT("\n  Abilities (%d):"), Abilities.LearnedAbilities.Num());
    for (const FAbilityInstance &A : Abilities.LearnedAbilities)
    {
        Out += FString::Printf(TEXT("\n    %s  IID=%s Tier=%s Q=%s"),
                               A.Ability ? *A.Ability->Name : TEXT("(null)"),
                               *ShortGuid(A.InstanceID),
                               *TierHelpers::GetTierName(A.Tier),
                               QualityName(A.Quality));
    }

    // Sibling components hold the evolution + crystal stores — resolve read-only.
    AActor *Owner = GetOwner();
    UEvolutionInventoryComponent *EvolutionInv = Owner
        ? Owner->FindComponentByClass<UEvolutionInventoryComponent>()
        : nullptr;
    UCrystalInventoryComponent *CrystalInv = Owner
        ? Owner->FindComponentByClass<UCrystalInventoryComponent>()
        : nullptr;

    // ---------- Evolutions ----------
    if (EvolutionInv)
    {
        Out += FString::Printf(TEXT("\n  Evolutions (%d):"), EvolutionInv->Entries.Num());
        for (const FEvolutionInventoryEntry &E : EvolutionInv->Entries)
        {
            Out += FString::Printf(TEXT("\n    %s  IID=%s Tier=%s Q=%s  dur=%d"),
                                   E.Item ? *E.Item->GetFullItemName() : TEXT("(null)"),
                                   *ShortGuid(E.InstanceID),
                                   *TierHelpers::GetTierName(E.Tier),
                                   QualityName(E.Quality),
                                   E.CurrentDurability);
        }
    }
    else
    {
        Out += TEXT("\n  Evolutions: (no UEvolutionInventoryComponent)");
    }

    // ---------- Crystals (stacks — same shape as PrintPoolState for direct diff) ----------
    if (CrystalInv)
    {
        Out += TEXT("\n  Crystals:");
        auto EmitPool = [&](const TMap<FCrystalId, int32> &Pool, const TCHAR *Tag)
        {
            for (const TPair<FCrystalId, int32> &Pair : Pool)
            {
                Out += FString::Printf(TEXT("\n      [%s] %s %s x%d"), Tag,
                                       *CrystalEnum->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key.Type)),
                                       *TierHelpers::GetTierName(Pair.Key.Tier), Pair.Value);
            }
        };
        EmitPool(CrystalInv->Crystals, TEXT("crystal"));
        EmitPool(CrystalInv->Stones, TEXT("stone"));
    }
    else
    {
        Out += TEXT("\n  Crystals: (no UCrystalInventoryComponent)");
    }

    Out += TEXT("\n") + GetOwnedGearSyncString();
    return Out;
}

#if WITH_EDITOR
void UInventoryComponent::DebugLogInventory()
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *GetInventorySummary());

    // Log weapons
    UE_LOG(LogTemp, Display, TEXT("--- Weapons ---"));
    for (int32 i = 0; i < Weapons.Num(); ++i)
    {
        const FWeaponInventoryEntry &W = Weapons[i];
        FString WeaponName = W.Weapon ? W.Weapon->Name : TEXT("NULL");
        FString CrystalStr = W.HasCrystal() ? TEXT("Crystal") : TEXT("None");
        FString EvoStr = W.IsEvolved() ? TEXT("Evolved") : TEXT("");
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%s %s) Cost:%d"),
               i, *WeaponName, *CrystalStr, *EvoStr, W.GetSlotCost());
    }

    // Log rings
    UE_LOG(LogTemp, Display, TEXT("--- Rings ---"));
    for (int32 i = 0; i < Rings.Num(); ++i)
    {
        const FRingInventoryEntry &R = Rings[i];
        FString RingName = R.Ring ? R.Ring->Name : TEXT("NULL");
        FString CrystalStr = R.HasCrystal() ? TEXT("Crystal") : TEXT("None");
        FString EvoStr = R.IsEvolved() ? TEXT("Evolved") : TEXT("");
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%s %s) Cost:%d"),
               i, *RingName, *CrystalStr, *EvoStr, R.GetSlotCost());
    }
}
#endif
