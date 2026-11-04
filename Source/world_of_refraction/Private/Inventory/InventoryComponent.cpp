// InventoryComponent.cpp
// Character inventory storage and management implementation

#include "Inventory/InventoryComponent.h"
#include "Inventory/GearFind.h"
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
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/Crystals/FFusionId.h"            // fusion attach identity
#include "Equipment/Crystals/CrystalTypeHelpers.h"   // IsGemType / IsValidFusionPair / IsElementalFusion
#include "Equipment/Crystals/ItemIdentity.h"         // GetMaxDurability + GetDisplayName for attach
#include "Pool/PoolSubsystem.h" // InitializeFromPool draw source (step 2)
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h" // B-091: party members draw the shared armoury
#include "Equipment/EquipmentDataBase.h" // B-091: gear display names
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
    /** OBSOLETE (B-335, D-127; delete in the D-124 sweep): per-instance random rolls retire;
     *  gear keeps tier + quality. Reached only when an asset opts in (default off).
     *  U2 pickup roll — when the asset opts in (bRandomGenerateOnPickup), a fresh
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
        Id = StatStone::Normalize(Id); // B-353 (D-142): an old single-stat stone attaches as a StatStone
        FRuntimeAttachedItem Runtime;
        Runtime.Kind = bIsStone ? EAttachedItemKind::AugmentStone : EAttachedItemKind::Crystal;
        Runtime.Crystal.Id = Id;
        Runtime.Crystal.CurrentDurability = bIsStone ? 0 : ItemIdentity::GetMaxDurability(Id);
        return Runtime;
    }

    /** Build a runtime fusion attachment from the two-half identity (fuse-and-socket — fusions have
     *  no loose-inventory home; the caller consumes the two halves from the crystal stacks). Seeds
     *  whole-fusion durability via GetMaxDurability (gem-seeded; 0 for an augmented fusion). */
    [[maybe_unused]] FRuntimeAttachedItem MakeFusionAttachment(const FFusionId &FusionId) // OBSOLETE (B-352): only the discarded branch calls it
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
        const int32 Index = WoRGearFind::IndexOf(Gear, GearID);
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
    HandleGearSocketChanged(GearID);
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

    // B-351 (D-141): a gem goes in the element socket, a stone in the stone socket, on a weapon or an adapter alike
    // (the weapon-only stone guard of D-009 retires). Each socket holds one: a second gem or a second stone is refused.
    const bool bIsStone = !CrystalTypeHelpers::IsGemType(Id.Type);
    const int32 Index = bIsStone ? FindGearIndex(Gear, GearID, Op) : FindEmptyGearSlot(Gear, GearID, Op);
    if (Index == INDEX_NONE)
    {
        return false;
    }
    if (bIsStone && !Gear[Index].StoneSocket.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: %s stone socket already occupied"), Op, GearLabel<TEntry>());
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

    FRuntimeAttachedItem &Socket = bIsStone ? Gear[Index].StoneSocket : Gear[Index].AttachedItem;
    Socket = MakeCrystalAttachment(Id, bIsStone);
    HandleGearSocketChanged(GearID);
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
    if constexpr (FusionRules::RETIRED)
    {
        // B-352 (D-141): fusions retire; nothing is fused or debited. OBSOLETE body below, deleted in the D-124 sweep.
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] %s: fusions are retired (D-141) - refused, nothing debited"), Op);
        return false;
    }
    else
    {
        // OBSOLETE (B-352, D-141; delete in the D-124 sweep): the fuse. Discarded while FusionRules::RETIRED.

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
        HandleGearSocketChanged(GearID);
        UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Fused %s + %s onto %s %s"),
               *ItemIdentity::GetDisplayName(FusionId.HalfA), *ItemIdentity::GetDisplayName(FusionId.HalfB),
               GearLabel<TEntry>(), *GearID.ToString());
        BroadcastInventoryChanged(EInventoryChangeType::Equipped);
        return true;
    }
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
    HandleGearSocketChanged(GearID);

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
    if (bHadAttachment)
    {
        HandleGearSocketChanged(Gear[Index].PersistentID);
    }

    // Notify TurnManager so the stone factor re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (bWasSpeedStone)
    {
        if (UWorld *World = GetWorld())
        {
            if (UGameInstance *GI = World->GetGameInstance())
            {
                if (UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(GI))
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

namespace
{
    /** B-347b (Q-347): Saved names a home piece the open run left at home (so the member's copy can't hold it). */
    bool HoldsPieceLeftHome(const UPoolSubsystem &Pool, const FSavedLoadout &Saved)
    {
        const auto LeftHome = [&Pool](const FGuid &ID) { return ID.IsValid() && !Pool.IsInRunView(ID); };
        if (LeftHome(Saved.PrimaryWeaponInstance) || LeftHome(Saved.PrimaryRingInstance) ||
            LeftHome(Saved.PrimaryEvolutionInstance) || LeftHome(Saved.SecondaryWeaponInstance))
        {
            return true;
        }
        return Saved.EquippedRings.ContainsByPredicate([&LeftHome](const FResonatorRingSlot &Slot) { return LeftHome(Slot.RingInstance); });
    }
}

void UInventoryComponent::PushCustomLoadoutsToPool()
{
    UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool || !bKeepsCustomLoadouts)
    {
        return;
    }
    const UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    const ECharacterClass Class = CharComp && CharComp->CharacterData ? CharComp->CharacterData->CharacterClass : ECharacterClass::Generic;
    // B-347b (Q-347): a loadout drawn during a run naming a piece left at home is written back as stored (the copy had
    // that slot emptied); also after the layer closes, as ExitTrial's save and EndPlay push after EndRunLayer.
    // Edits to such a loadout during the run are not kept (it can't be equipped there anyway).
    TArray<FSavedLoadout> Custom;
    int32 Kept = 0;
    for (const FCombatLoadout &Loadout : SavedLoadouts)
    {
        if (Loadout.bLore)
        {
            continue;
        }
        if (Loadout.HeldHomeLoadout.IsValid())
        {
            Custom.Add(*Loadout.HeldHomeLoadout);
            ++Kept;
        }
        else
        {
            Custom.Add(Loadout.ToSavedLoadout(Class));
        }
    }
    if (Kept > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: %d loadout(s) name gear left at home - kept as stored until the run ends"),
               *GetNameSafe(GetOwner()), Kept);
    }
    Pool->SetCustomLoadouts(SharedGearKey, Custom);
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    PushCustomLoadoutsToPool();
    if (UPoolSubsystem *Pool = SharedPool.Get())
    {
        Pool->OnPoolGearChanged.RemoveDynamic(this, &UInventoryComponent::HandlePoolGearChanged);
        Pool->OnPoolSkillsChanged.RemoveDynamic(this, &UInventoryComponent::HandlePoolSkillsChanged);
    }
    SharedPool.Reset();
    bSkillsInStore = false;
    Super::EndPlay(EndPlayReason);
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
        PushSkillsToPool(); // B-316b: a learned skill is the member's, kept
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return bLearned;
}

bool UInventoryComponent::UnlearnSpell(USpellData *Spell)
{
    const bool bUnlearned = Spells.UnlearnSpell(Spell);
    if (bUnlearned)
    {
        PushSkillsToPool();
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
        PushSkillsToPool(); // B-316b
        BroadcastInventoryChanged(EInventoryChangeType::Added);
    }
    return bLearned;
}

bool UInventoryComponent::UnlearnAbility(UAbilityData *Ability)
{
    const bool bUnlearned = Abilities.UnlearnAbility(Ability);
    if (bUnlearned)
    {
        PushSkillsToPool();
        BroadcastInventoryChanged(EInventoryChangeType::Removed);
    }
    return bUnlearned;
}

// ==================== THE MEMBER'S SKILLS IN THE STORE (B-316b, Q-316) ====================

void UInventoryComponent::PullSkillsFromPool()
{
    const UPoolSubsystem *Pool = SharedPool.Get();
    const FPoolSkills *Skills = Pool && bSkillsInStore ? Pool->FindSkills(SharedGearKey) : nullptr;
    if (!Skills)
    {
        return;
    }
    Spells.LearnedSpells = Skills->Spells;
    Abilities.LearnedAbilities = Skills->Abilities;
}

void UInventoryComponent::PushSkillsToPool()
{
    UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool || !bSkillsInStore)
    {
        return;
    }
    TGuardValue<bool> Pushing(bPushingSkills, true);
    Pool->SetSkills(SharedGearKey, Spells.LearnedSpells, Abilities.LearnedAbilities);
}

void UInventoryComponent::HandlePoolSkillsChanged(FName CharacterKey)
{
    if (CharacterKey != SharedGearKey || !bSkillsInStore || bPushingSkills)
    {
        return;
    }
    PullSkillsFromPool();
    UE_LOG(LogTemp, Log, TEXT("[Inventory] %s's skills reloaded from the store: %d spell(s), %d ability(ies)"),
           *GetNameSafe(GetOwner()), Spells.LearnedSpells.Num(), Abilities.LearnedAbilities.Num());
    BroadcastInventoryChanged(EInventoryChangeType::Loaded);
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

// ==================== GEAR ACQUISITION (B-251) ====================

namespace
{
    template <class TEntry>
    const UEquipmentDataBase *GearEntryAsset(const TEntry &Entry)
    {
        if constexpr (std::is_same_v<TEntry, FWeaponInventoryEntry>)
        {
            return Entry.Weapon;
        }
        else
        {
            return Entry.Ring;
        }
    }
}

template <class TEntry>
bool UInventoryComponent::AddGearEntry(TArray<TEntry> &Gear, TEntry Entry, const UEquipmentDataBase &Asset)
{
    // Acquisition is the ONE mint point for the persistent owned-instance guid — the CreateFrom* factories
    // deliberately leave it invalid (loadout inflation reuses them and must not mint). Tier/Quality are
    // seeded in the factory, so loadout-inflated entries get them too (cluster 2a).
    Entry.PersistentID = FGuid::NewGuid();
    if (Asset.bRandomGenerateOnPickup)
    {
        // OBSOLETE (B-335, D-127; delete in the D-124 sweep): the stat / resistance roll. The Quality roll below stays (Q-335).
        ApplyPickupRoll(&Asset, Entry.StatBonus, Entry.ResistanceBonus, Entry.StatMaxPool, Entry.ResistanceMaxPool);
        // Fresh pickup → roll a per-instance Quality (§11), Luck-biased. Toggle-OFF acquisitions
        // (purchases, authored gear) skip this and keep the factory's C_Quality placeholder.
        Entry.Quality = EconomyYield::RollQuality(ResolveInventoryOwnerLuck(this));
    }
    const TEntry *Owned = Gear.FindByPredicate([&Asset](const TEntry &Other) { return GearEntryAsset(Other) == &Asset; });
    const FGuid OwnedID = Owned ? Owned->PersistentID : FGuid();
    Gear.Add(Entry);
    PushGearToPool(Entry.PersistentID);
    BroadcastInventoryChanged(EInventoryChangeType::Added);
    if (OwnedID.IsValid() && !bSuppressDuplicateNotice)
    {
        // D-115: one copy per character; the choice (Gold or dismantle) is the player's.
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: received a second %s - choose Gold or dismantle (wor.UI.Duplicates)"),
               *GetNameSafe(GetOwner()), *Asset.GetName());
        OnDuplicateGearReceived.Broadcast(Entry.PersistentID, OwnedID);
    }
    return true;
}

template <class TEntry>
bool UInventoryComponent::RemoveGearAt(TArray<TEntry> &Gear, int32 Index)
{
    if (!Gear.IsValidIndex(Index))
    {
        return false;
    }
    const FGuid RemovedID = Gear[Index].PersistentID;
    Gear.RemoveAt(Index);
    PushGearToPool(RemovedID);
    // Remove*ByPersistentID delegate here, so this single broadcast covers both entry points (no double-fire).
    BroadcastInventoryChanged(EInventoryChangeType::Removed);
    return true;
}

// ==================== WEAPON OPERATIONS ====================

bool UInventoryComponent::AddWeapon(UWeaponData *Weapon)
{
    if (!Weapon || !CanAddWeapon(Weapon))
    {
        return false;
    }
    return AddGearEntry(Weapons, FWeaponInventoryEntry::CreateFromWeapon(Weapon), *Weapon);
}

bool UInventoryComponent::RemoveWeapon(int32 WeaponIndex)
{
    return RemoveGearAt(Weapons, WeaponIndex);
}

bool UInventoryComponent::RemoveWeaponByPersistentID(FGuid PersistentID)
{
    const int32 Index = WoRGearFind::IndexOf(Weapons, PersistentID);
    return Index != INDEX_NONE && RemoveWeapon(Index);
}

FWeaponInventoryEntry *UInventoryComponent::FindOwnedWeapon(const FGuid &PersistentID)
{
    return WoRGearFind::Find(Weapons, PersistentID);
}

const FWeaponInventoryEntry *UInventoryComponent::FindOwnedWeapon(const FGuid &PersistentID) const
{
    return const_cast<UInventoryComponent *>(this)->FindOwnedWeapon(PersistentID);
}

FRingInventoryEntry *UInventoryComponent::FindOwnedRing(const FGuid &PersistentID)
{
    return WoRGearFind::Find(Rings, PersistentID);
}

const FRingInventoryEntry *UInventoryComponent::FindOwnedRing(const FGuid &PersistentID) const
{
    return const_cast<UInventoryComponent *>(this)->FindOwnedRing(PersistentID);
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
    if (!Ring || !CanAddRing(Ring))
    {
        return false;
    }
    return AddGearEntry(Rings, FRingInventoryEntry::CreateFromRing(Ring), *Ring);
}

bool UInventoryComponent::RemoveRing(int32 RingIndex)
{
    return RemoveGearAt(Rings, RingIndex);
}

bool UInventoryComponent::RemoveRingByPersistentID(FGuid PersistentID)
{
    const int32 Index = WoRGearFind::IndexOf(Rings, PersistentID);
    return Index != INDEX_NONE && RemoveRing(Index);
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
    // D-116: also fills an active loadout that was already empty; counts as a change so listeners hear it.
    Changed += RefillActivePrimary(PersistentID) ? 1 : 0;
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

bool UInventoryComponent::LoadoutHasPrimary(const FCombatLoadout &Loadout)
{
    // A weapon counts with a Weapon or None type (AutoPopulateLoadout leaves None), as the fight reads it.
    const bool bWeaponType = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None;
    return (bWeaponType && Loadout.PrimaryWeapon.IsValid()) ||
           (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid()) ||
           (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item);
}

bool UInventoryComponent::IsPrimaryInAnyLoadout(FGuid PersistentID) const
{
    return PersistentID.IsValid() &&
           SavedLoadouts.ContainsByPredicate([&PersistentID](const FCombatLoadout &Loadout)
           {
               const bool bWeaponType = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None;
               return (bWeaponType && Loadout.PrimaryWeapon.WeaponEntry.PersistentID == PersistentID) ||
                      (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.RingEntry.PersistentID == PersistentID) ||
                      (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolutionInstance == PersistentID);
           });
}

bool UInventoryComponent::LoadoutHasGear(const FCombatLoadout &Loadout)
{
    const bool bPrimary = LoadoutHasPrimary(Loadout);
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
        return ID.IsValid() ? WoRGearFind::Find(Owned, ID) : nullptr;
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
        if constexpr (std::is_same_v<TEntry, FWeaponInventoryEntry>)
        {
            // B-351 (D-145): the loadout copy's skill picks lead until WriteLoadoutToOwned, like its spells;
            // an unedited copy takes the owned item's (saved) list.
            if (Slot.bSkillSlotsActive)
            {
                TArray<UAbilityData *> SkillPicks = MoveTemp(Slot.SkillPicks);
                Slot = Owned;
                Slot.AssignedSpells = MoveTemp(Picks);
                Slot.SkillPicks = MoveTemp(SkillPicks);
                Slot.bSkillSlotsActive = true;
                return;
            }
        }
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
        // B-351: the stone socket must match too.
        const bool bInSync = Match->Tier == Slot.Tier && Match->AttachedItem.Kind == Slot.AttachedItem.Kind &&
                             OwnedDurability == SlotDurability && Match->StoneSocket == Slot.StoneSocket;
        return FString::Printf(TEXT("\n    %s %s (stone socket %s) -> owned %s: %s"), Label, *GetNameSafe(GetGearAsset(Slot)),
                               Slot.StoneSocket.IsEmpty() ? TEXT("empty") : *ItemIdentity::GetDisplayName(Slot.StoneSocket.Crystal.Id),
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
        else
        {
            // One evolution, one slot: a second loadout naming the same owned evolution keeps its own copy.
            UE_LOG(LogTemp, Warning, TEXT("[Inventory] %s: loadout '%s' evolution %s matches no free owned evolution - kept as its own copy"),
                   *GetNameSafe(GetOwner()), *Loadout.LoadoutName, *GetNameSafe(Loadout.PrimaryEvolution.Item));
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
    PushGearToPool(GearID);
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

void UInventoryComponent::HandleGearSocketChanged(const FGuid &GearID)
{
    RefreshLoadoutsFromOwned(GearID);
    OnGearSocketChanged.Broadcast(GearID);
}

int32 UInventoryComponent::RefreshEvolutionInLoadouts(const FGuid &EvoInstanceID)
{
    if (!EvoInstanceID.IsValid())
    {
        return 0;
    }
    const FEvolutionInventoryEntry *Owned = FindOwnedEvolution(GetOwner(), EvoInstanceID);
    PushGearToPool(EvoInstanceID);
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

int32 UInventoryComponent::DetachEvolutionFromOwnedGear(const FGuid &EvoInstanceID)
{
    if (!EvoInstanceID.IsValid())
    {
        return 0; // built-in evolutions have no instance id and never leave
    }
    TArray<FGuid> Detached;
    const auto DetachFrom = [&EvoInstanceID, &Detached](auto &Gear)
    {
        for (auto &Entry : Gear)
        {
            if (Entry.AttachedItem.IsEvolution() && Entry.AttachedItem.Evolution.InstanceID == EvoInstanceID)
            {
                Entry.RemoveCrystal();
                Detached.Add(Entry.PersistentID);
            }
        }
    };
    DetachFrom(Weapons);
    DetachFrom(Rings);
    for (const FGuid &GearID : Detached)
    {
        HandleGearSocketChanged(GearID);
    }
    if (Detached.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: evolution %s no longer owned, taken off %d weapon(s) / ring(s)"),
               *GetNameSafe(GetOwner()), *EvoInstanceID.ToString(), Detached.Num());
    }
    return Detached.Num();
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
                            Owned->StoneSocket = Slot.StoneSocket; // B-351
                            Owned->SkillPicks = Slot.SkillPicks;   // B-351 (D-145): the picks live on the item
                            Owned->bSkillSlotsActive = Slot.bSkillSlotsActive;
                            Written.Add(Slot.PersistentID);
                        }
                    },
                    [this, &Written](const FRingInventoryEntry &Slot)
                    {
                        if (FRingInventoryEntry *Owned = FindOwned(Rings, Slot.PersistentID))
                        {
                            Owned->AttachedItem = Slot.AttachedItem;
                            Owned->StoneSocket = Slot.StoneSocket; // B-351
                            Written.Add(Slot.PersistentID);
                        }
                    });
    int32 WrittenCount = Written.Num();
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        if (FEvolutionInventoryEntry *Owned = FindOwnedEvolution(GetOwner(), Loadout.PrimaryEvolutionInstance))
        {
            Owned->CurrentDurability = Loadout.PrimaryEvolution.CurrentDurability;
            PushGearToPool(Loadout.PrimaryEvolutionInstance);
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
        Out += DescribeSlot(TEXT("Primary adapter"), Loadout.PrimaryRing.RingEntry, Rings);
        for (const FRingLoadoutEntry &Ring : Loadout.RingLoadout)
        {
            Out += DescribeSlot(TEXT("Adapter"), Ring.RingEntry, Rings);
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

// ==================== SHARED GEAR (B-091, D-020, Q-091) ====================

#define LOCTEXT_NAMESPACE "InventorySharedGear"

namespace InventoryGearSync
{
    /** The pool's copy of a weapon or ring into this member's owned list (added when new). */
    template <typename TEntry>
    void MirrorShared(TArray<TEntry> &Owned, const TEntry &Shared)
    {
        if (TEntry *Mine = FindOwned(Owned, Shared.PersistentID))
        {
            *Mine = Shared;
        }
        else
        {
            Owned.Add(Shared);
        }
    }

    FText GetGearDisplayName(const UObject *Asset)
    {
        if (const UEquipmentDataBase *Equipment = Cast<UEquipmentDataBase>(Asset))
        {
            return FText::FromString(Equipment->Name);
        }
        if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(Asset))
        {
            return FText::FromString(Evolution->ItemName);
        }
        return FText::FromString(GetNameSafe(Asset));
    }
}

namespace InventoryGearSync
{
    const TCHAR *const RUN_PRESET_NAME = TEXT("Run gear");

    /** B-076 (Q-076): a run-only member's one loadout. Its authored preset keeps its spells and items;
     *  the gear slots hold only the armoury pieces it was given. A Resonator wears the ring in its ring
     *  loadout; a Generic or Caster wears it as the primary only when given no weapon. */
    FCombatLoadout MakeRunPreset(const FPartyMember &Member, ECharacterClass Class, const FCombatLoadout *Authored,
                                 const TArray<FWeaponInventoryEntry> &Weapons, const TArray<FRingInventoryEntry> &Rings)
    {
        FCombatLoadout Preset;
        Preset.InitializeForClass(Class);
        if (Authored)
        {
            Preset.InnateSpells = Authored->InnateSpells;
            Preset.BDSpellPools = Authored->BDSpellPools;
            if (Authored->ItemSlots.Num() > 0)
            {
                Preset.ItemSlots = Authored->ItemSlots;
            }
            Preset.bAutoEquipItemsOnCombatStart = Authored->bAutoEquipItemsOnCombatStart;
            Preset.bShowPrimary = Authored->bShowPrimary;
        }
        Preset.LoadoutName = RUN_PRESET_NAME;
        const FWeaponInventoryEntry *Weapon = FindOwned(Weapons, Member.RunWeapon);
        if (Weapon)
        {
            Preset.PrimarySlotType = EPrimarySlotType::Weapon;
            Preset.PrimaryWeapon.WeaponEntry = *Weapon;
        }
        if (const FRingInventoryEntry *Ring = FindOwned(Rings, Member.RunRing))
        {
            if (Class == ECharacterClass::Resonator)
            {
                Preset.RingLoadout.AddDefaulted_GetRef().RingEntry = *Ring;
                Preset.ActiveRingIndex = 0;
            }
            else if (!Weapon)
            {
                Preset.PrimarySlotType = EPrimarySlotType::Ring;
                Preset.PrimaryRing.RingEntry = *Ring;
            }
        }
        return Preset;
    }

    /** B-321: the local party leader's character key (the first permanent member; a run-only recruit leaves), else Fallback. */
    FName GetLocalLeaderKey(const UGameInstance *GI, FName Fallback)
    {
        const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
        if (Party)
        {
            for (const FPartyMember &Member : Party->Members)
            {
                if (!Member.bRunOnly && Member.CharacterData)
                {
                    return UPoolSubsystem::GetCharacterKey(Member);
                }
            }
        }
        return Fallback;
    }

    /** The party member a pawn plays (B-360b: by the member id the battle stamped, so a copy is its own member). */
    const FPartyMember *FindLocalPartyMember(const UGameInstance *GI, const UCharacterDataComponent *Comp)
    {
        const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
        const int32 Slot = Party && Comp ? PartySession->GetMemberSlotOf(Comp) : INDEX_NONE;
        return Party && Party->Members.IsValidIndex(Slot) ? &Party->Members[Slot] : nullptr;
    }
}

TMap<FName, const UCharacterData *> UInventoryComponent::GetPartyMembersByKey() const
{
    TMap<FName, const UCharacterData *> Members;
    const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
    if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
    {
        for (const FPartyMember &Member : Party->Members)
        {
            if (Member.CharacterData)
            {
                Members.Add(UPoolSubsystem::GetCharacterKey(Member), Member.CharacterData);
            }
        }
    }
    return Members;
}

bool UInventoryComponent::DrawSharedGear()
{
    const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
    const UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    const FName Key = UPoolSubsystem::GetCharacterKey(CharComp);
    const TMap<FName, const UCharacterData *> Party = GetPartyMembersByKey();
    if (!Pool || !Party.Contains(Key))
    {
        return false;
    }
    UEvolutionInventoryComponent *EvoInv = GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>();
    // B-076 (D-026): a character found in a trial brings no gear; it uses what the party gives it.
    const FPartyMember *RunMember = InventoryGearSync::FindLocalPartyMember(GI, CharComp);
    const bool bRunOnly = RunMember && RunMember->bRunOnly;
    if (!bRunOnly && !Pool->HasBankedGear(Key))
    {
        AdoptPresetOnlyGear(*Pool);
    }
    if (!bRunOnly)
    {
        Pool->BankStartingGear(Key, Weapons, Rings, EvoInv ? EvoInv->Entries : TArray<FEvolutionInventoryEntry>());
    }
    // B-321 (D-117): each member draws only its own gear. A piece from before D-117 goes to whoever wears it,
    // else the party leader. A run-only recruit brings no gear and draws the whole armoury (TODO: B-323's give).
    Pool->AssignUnownedGear(InventoryGearSync::GetLocalLeaderKey(GI, Key));
    bDrawsWholeArmoury = bRunOnly;
    // B-324: the chest's gear is nobody's to wear; a recruit's whole-armoury draw leaves it in the chest.
    // Key, not SharedGearKey: that is set below, once the draw is done (IsMineInStore is the same rule after that).
    // B-347b (Q-347): during a run only what the run brought or found (IsMineInStore is the same rule).
    const auto IsMine = [this, Pool, Key](const FGuid &ID)
    { return (bDrawsWholeArmoury ? !Pool->IsInChest(ID) : Pool->GetGearOwner(ID) == Key) && Pool->IsInRunView(ID); };

    // The whole armoury: this member's pieces first so its presets link to them, then free pieces,
    // then pieces another member wears.
    const auto Rank = [Pool, Key, &Party](const FGuid &ID)
    {
        const FName Wearer = Pool->GetWearer(ID);
        return Wearer == Key ? 0 : (Wearer.IsNone() || !Party.Contains(Wearer)) ? 1 : 2;
    };
    Weapons = Pool->GetOwnedWeapons().FilterByPredicate([&IsMine](const FWeaponInventoryEntry &E) { return IsMine(E.PersistentID); });
    Weapons.StableSort([&Rank](const FWeaponInventoryEntry &A, const FWeaponInventoryEntry &B) { return Rank(A.PersistentID) < Rank(B.PersistentID); });
    Rings = Pool->GetOwnedRings().FilterByPredicate([&IsMine](const FRingInventoryEntry &E) { return IsMine(E.PersistentID); });
    Rings.StableSort([&Rank](const FRingInventoryEntry &A, const FRingInventoryEntry &B) { return Rank(A.PersistentID) < Rank(B.PersistentID); });
    if (EvoInv)
    {
        EvoInv->Entries = Pool->GetOwnedEvolutions().FilterByPredicate([&IsMine](const FEvolutionInventoryEntry &E) { return IsMine(E.InstanceID); });
        EvoInv->Entries.StableSort([&Rank](const FEvolutionInventoryEntry &A, const FEvolutionInventoryEntry &B) { return Rank(A.InstanceID) < Rank(B.InstanceID); });
    }
    if (bRunOnly)
    {
        const FCombatLoadout *Authored = SavedLoadouts.IsValidIndex(ActiveLoadoutIndex) ? &SavedLoadouts[ActiveLoadoutIndex] : nullptr;
        SavedLoadouts = {InventoryGearSync::MakeRunPreset(*RunMember, CharComp->CharacterData->CharacterClass, Authored, Weapons, Rings)};
        ActiveLoadoutIndex = 0;
    }
    else if (const TArray<FSavedLoadout> *Custom = bKeepsCustomLoadouts ? nullptr : Pool->GetCustomLoadouts(Key))
    {
        // B-311 (D-111): the player's own loadouts after the lore ones; a piece that's gone leaves its slot empty
        // (StripUnownedGear below).
        for (const FSavedLoadout &Saved : *Custom)
        {
            FCombatLoadout &Added = SavedLoadouts.Add_GetRef(FCombatLoadout::CreateFromSavedLoadout(Saved, this, EvoInv));
            if (HoldsPieceLeftHome(*Pool, Saved)) // B-347b: StripUnownedGear empties its left-home slot in this copy
            {
                Added.HeldHomeLoadout = MakeShared<const FSavedLoadout>(Saved);
            }
        }
    }
    bKeepsCustomLoadouts = !bRunOnly;

    LinkLoadoutsToOwned();
    const int32 Stripped = StripUnownedGear();
    SharedPool = Pool;
    SharedGearKey = Key;
    RefillActivePrimary(); // after the pool is set, so gear another member wears is skipped
    Pool->OnPoolGearChanged.AddUniqueDynamic(this, &UInventoryComponent::HandlePoolGearChanged);
    // B-316b (Q-316, Q-316b): learned skills are the member's own and kept: its asset's go in once, the store's are
    // loaded after. B-361 (D-152): a run-only recruit's too; they go to the host when it leaves (DismissRunMembers).
    bSkillsInStore = true;
    if (!Pool->BankSkills(Key, Spells.LearnedSpells, Abilities.LearnedAbilities))
    {
        PullSkillsFromPool();
    }
    Pool->OnPoolSkillsChanged.AddUniqueDynamic(this, &UInventoryComponent::HandlePoolSkillsChanged);
    // B-203 / B-322 (D-097, D-117): each member carries its own pouch; its starting items go in once.
    // B-361 (D-152, Q-361c): a run-only recruit carries the host's pouch and brings no items of its own.
    if (UCrystalInventoryComponent *CrystalInv = GetOwner()->FindComponentByClass<UCrystalInventoryComponent>())
    {
        if (!bRunOnly && !Pool->HasBankedItems(Key))
        {
            Pool->BankStartingItems(Key, CrystalInv->GetHeldStacks()); // its authored counts: not bound yet
        }
        const UPartySessionSubsystem *PartySession = GI->GetSubsystem<UPartySessionSubsystem>();
        CrystalInv->BindSharedItems(Pool, PartySession ? PartySession->GetPouchKey(Key) : Key);
    }
    ClaimActiveSharedGear();

    UE_LOG(LogTemp, Log, TEXT("[Inventory] %s draws the shared armoury: %d weapon(s), %d ring(s), %d evolution(s); %d preset slot(s) emptied"),
           *GetNameSafe(GetOwner()), Weapons.Num(), Rings.Num(), EvoInv ? EvoInv->Num() : 0, Stripped);
    BroadcastInventoryChanged(EInventoryChangeType::Loaded);
    return true;
}

namespace
{
    /** One side's pieces by ID, with what the compare looks at. */
    struct FStoreCompareEntry
    {
        FString Label;
        EItemTier Tier = EItemTier::F_Tier;
        EItemQuality Quality = EItemQuality::C_Quality;
        EAttachedItemKind Attached = EAttachedItemKind::None;
    };

    template <class TEntry>
    FStoreCompareEntry StoreCompareEntryOf(const TEntry &Entry, const TCHAR *Kind)
    {
        FStoreCompareEntry Out;
        Out.Label = FString::Printf(TEXT("%s %s"), Kind, *GetNameSafe(InventoryGearSync::GetGearAsset(Entry)));
        Out.Tier = Entry.Tier;
        Out.Quality = Entry.Quality;
        Out.Attached = Entry.AttachedItem.Kind;
        return Out;
    }

    FStoreCompareEntry StoreCompareEntryOf(const FEvolutionInventoryEntry &Entry)
    {
        FStoreCompareEntry Out;
        Out.Label = FString::Printf(TEXT("evolution %s"), *GetNameSafe(Entry.Item));
        Out.Tier = Entry.Tier;
        Out.Quality = Entry.Quality;
        return Out;
    }
}

bool UInventoryComponent::IsMineInStore(const UPoolSubsystem &Pool, const FGuid &GearID) const
{
    // B-347b (Q-347): during a run a home piece left at home isn't this member's to equip.
    return (bDrawsWholeArmoury ? !Pool.IsInChest(GearID) : Pool.GetGearOwner(GearID) == SharedGearKey) &&
           Pool.IsInRunView(GearID);
}

bool UInventoryComponent::OwnsWeaponAsset(const UWeaponData *Weapon) const
{
    if (!Weapon)
    {
        return false;
    }
    const UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool)
    {
        return Weapons.ContainsByPredicate([Weapon](const FWeaponInventoryEntry &Entry) { return Entry.Weapon == Weapon; });
    }
    return Pool->GetOwnedWeapons().ContainsByPredicate(
        [this, Pool, Weapon](const FWeaponInventoryEntry &Entry) { return Entry.Weapon == Weapon && IsMineInStore(*Pool, Entry.PersistentID); });
}

bool UInventoryComponent::OwnsRingAsset(const URingData *Ring) const
{
    if (!Ring)
    {
        return false;
    }
    const UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool)
    {
        return Rings.ContainsByPredicate([Ring](const FRingInventoryEntry &Entry) { return Entry.Ring == Ring; });
    }
    return Pool->GetOwnedRings().ContainsByPredicate(
        [this, Pool, Ring](const FRingInventoryEntry &Entry) { return Entry.Ring == Ring && IsMineInStore(*Pool, Entry.PersistentID); });
}

FString UInventoryComponent::CompareWithStore() const
{
    const UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool)
    {
        return FString();
    }
    TMap<FGuid, FStoreCompareEntry> Mine;
    for (const FWeaponInventoryEntry &Entry : Weapons)
    {
        Mine.Add(Entry.PersistentID, StoreCompareEntryOf(Entry, TEXT("weapon")));
    }
    for (const FRingInventoryEntry &Entry : Rings)
    {
        Mine.Add(Entry.PersistentID, StoreCompareEntryOf(Entry, TEXT("adapter")));
    }
    if (const UEvolutionInventoryComponent *EvoInv = GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr)
    {
        for (const FEvolutionInventoryEntry &Entry : EvoInv->Entries)
        {
            Mine.Add(Entry.InstanceID, StoreCompareEntryOf(Entry));
        }
    }
    TMap<FGuid, FStoreCompareEntry> Store;
    if (bDrawsWholeArmoury)
    {
        // A run-only recruit draws the whole armoury but the chest (Q-321): no owner view covers that.
        for (const FWeaponInventoryEntry &Entry : Pool->GetOwnedWeapons())
        {
            if (!Pool->IsInChest(Entry.PersistentID))
            {
                Store.Add(Entry.PersistentID, StoreCompareEntryOf(Entry, TEXT("weapon")));
            }
        }
        for (const FRingInventoryEntry &Entry : Pool->GetOwnedRings())
        {
            if (!Pool->IsInChest(Entry.PersistentID))
            {
                Store.Add(Entry.PersistentID, StoreCompareEntryOf(Entry, TEXT("adapter")));
            }
        }
        for (const FEvolutionInventoryEntry &Entry : Pool->GetOwnedEvolutions())
        {
            if (!Pool->IsInChest(Entry.InstanceID))
            {
                Store.Add(Entry.InstanceID, StoreCompareEntryOf(Entry));
            }
        }
    }
    else
    {
        // The very view B-312b will read, so silence proves that view.
        const FPoolGearView View = Pool->GetOwnedGearOf(SharedGearKey);
        for (const FWeaponInventoryEntry *Entry : View.Weapons)
        {
            Store.Add(Entry->PersistentID, StoreCompareEntryOf(*Entry, TEXT("weapon")));
        }
        for (const FRingInventoryEntry *Entry : View.Adapters)
        {
            Store.Add(Entry->PersistentID, StoreCompareEntryOf(*Entry, TEXT("adapter")));
        }
        for (const FEvolutionInventoryEntry *Entry : View.Evolutions)
        {
            Store.Add(Entry->InstanceID, StoreCompareEntryOf(*Entry));
        }
    }
    // B-347b: during a run the member's view is what the run brought and found.
    for (auto It = Store.CreateIterator(); It; ++It)
    {
        if (!Pool->IsInRunView(It->Key))
        {
            It.RemoveCurrent();
        }
    }
    FString Out;
    if (Mine.Contains(FGuid()))
    {
        Out += TEXT("a copy with no ID (unlinked): it can never match the store\n");
        Mine.Remove(FGuid());
    }
    for (const TPair<FGuid, FStoreCompareEntry> &Pair : Mine)
    {
        const FStoreCompareEntry *InStore = Store.Find(Pair.Key);
        if (!InStore)
        {
            Out += FString::Printf(TEXT("%s %s: a copy here, not in this member's store view\n"), *Pair.Value.Label, *Pair.Key.ToString());
        }
        else if (InStore->Tier != Pair.Value.Tier || InStore->Quality != Pair.Value.Quality || InStore->Attached != Pair.Value.Attached)
        {
            Out += FString::Printf(TEXT("%s %s: the copy differs from the store (tier, quality or attachment)\n"), *Pair.Value.Label, *Pair.Key.ToString());
        }
    }
    for (const TPair<FGuid, FStoreCompareEntry> &Pair : Store)
    {
        if (!Mine.Contains(Pair.Key))
        {
            Out += FString::Printf(TEXT("%s %s: in the store view, no copy here\n"), *Pair.Value.Label, *Pair.Key.ToString());
        }
    }
    return Out;
}

int32 UInventoryComponent::AdoptPresetOnlyGear(const UPoolSubsystem &Pool)
{
    using namespace InventoryGearSync;
    (void)Pool; // B-321 (D-117): another member's copy in the pool is not this member's, so it no longer blocks adoption
    int32 Adopted = 0;
    TSet<FGuid> AdoptedIDs;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        // A piece adopted for an earlier preset is reused (two copies only when one preset holds two).
        TSet<FGuid> UsedHere;
        const auto Adopt = [&Adopted, &AdoptedIDs, &UsedHere](auto &Slot, auto &Owned)
        {
            const UObject *Asset = GetGearAsset(Slot);
            if (!Asset || FindOwned(Owned, Slot.PersistentID))
            {
                return;
            }
            const auto *Earlier = Owned.FindByPredicate([Asset, &AdoptedIDs, &UsedHere](const auto &Entry)
            {
                return GetGearAsset(Entry) == Asset && AdoptedIDs.Contains(Entry.PersistentID) && !UsedHere.Contains(Entry.PersistentID);
            });
            if (Earlier)
            {
                CopyOwned(Slot, *Earlier);
            }
            else
            {
                Slot.PersistentID = FGuid::NewGuid();
                Owned.Add(Slot);
                AdoptedIDs.Add(Slot.PersistentID);
                ++Adopted;
            }
            UsedHere.Add(Slot.PersistentID);
        };
        ForEachGearSlot(Loadout,
                        [this, &Adopt](FWeaponInventoryEntry &Slot) { Adopt(Slot, Weapons); },
                        [this, &Adopt](FRingInventoryEntry &Slot) { Adopt(Slot, Rings); });
    }
    if (Adopted > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: %d preset piece(s) not in the authored inventory join the starting gear"),
               *GetNameSafe(GetOwner()), Adopted);
    }
    return Adopted;
}

int32 UInventoryComponent::StripUnownedGear()
{
    using namespace InventoryGearSync;
    int32 Stripped = 0;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        // An unlinked slot can carry no ID (an asset-built copy): tag it so it strips like the rest.
        TArray<FGuid> Unowned;
        const auto Collect = [&Unowned](auto &Slot, const auto &Owned)
        {
            if (GetGearAsset(Slot) && !FindOwned(Owned, Slot.PersistentID))
            {
                if (!Slot.PersistentID.IsValid())
                {
                    Slot.PersistentID = FGuid::NewGuid();
                }
                Unowned.Add(Slot.PersistentID);
            }
        };
        ForEachGearSlot(Loadout,
                        [this, &Collect](FWeaponInventoryEntry &Slot) { Collect(Slot, Weapons); },
                        [this, &Collect](FRingInventoryEntry &Slot) { Collect(Slot, Rings); });
        for (const FGuid &ID : Unowned)
        {
            Stripped += StripFromLoadout(Loadout, ID) ? 1 : 0;
        }
        const bool bEvolutionSlot = Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item;
        if (bEvolutionSlot && !FindOwnedEvolution(GetOwner(), Loadout.PrimaryEvolutionInstance))
        {
            Loadout.PrimaryEvolution = FEvolutionAttachment();
            Loadout.PrimaryEvolutionInstance = FGuid();
            ++Stripped;
        }
    }
    return Stripped;
}

bool UInventoryComponent::RefillActivePrimary(const FGuid &Excluded)
{
    using namespace InventoryGearSync;
    if (!SavedLoadouts.IsValidIndex(ActiveLoadoutIndex) || LoadoutHasPrimary(SavedLoadouts[ActiveLoadoutIndex]))
    {
        return false;
    }
    const UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    const bool bTakesAdapterPrimary = !(CharComp && CharComp->CharacterData && CharComp->CharacterData->CharacterClass == ECharacterClass::Resonator);
    const TMap<FName, const UCharacterData *> Party = GetPartyMembersByKey();
    FCombatLoadout &Loadout = SavedLoadouts[ActiveLoadoutIndex];
    {
        TSet<FGuid> InLoadout;
        ForEachGearSlot(Loadout,
                        [&InLoadout](const FWeaponInventoryEntry &Slot) { InLoadout.Add(Slot.PersistentID); },
                        [&InLoadout](const FRingInventoryEntry &Slot) { InLoadout.Add(Slot.PersistentID); });
        const auto Usable = [this, &Excluded, &InLoadout, &Party](const auto &Entry)
        {
            return GetGearAsset(Entry) && Entry.PersistentID.IsValid() && Entry.PersistentID != Excluded &&
                   !InLoadout.Contains(Entry.PersistentID) && !IsWornByOtherMemberOf(Entry.PersistentID, Party);
        };
        FString Filler;
        if (const FWeaponInventoryEntry *Weapon = Weapons.FindByPredicate(Usable))
        {
            Loadout.PrimaryRing.Clear();
            Loadout.PrimaryEvolution = FEvolutionAttachment();
            Loadout.PrimaryEvolutionInstance = FGuid();
            Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
            Loadout.PrimaryWeapon.Clear(); // no picks carried over from an old slot
            Loadout.PrimaryWeapon.WeaponEntry = *Weapon;
            Loadout.PrimaryWeapon.InitializeFromWeapon();
            Filler = GetNameSafe(Weapon->Weapon);
        }
        else if (const FRingInventoryEntry *Ring = bTakesAdapterPrimary ? Rings.FindByPredicate(Usable) : nullptr)
        {
            Loadout.PrimaryWeapon.Clear();
            Loadout.PrimaryEvolution = FEvolutionAttachment();
            Loadout.PrimaryEvolutionInstance = FGuid();
            Loadout.PrimarySlotType = EPrimarySlotType::Ring;
            Loadout.PrimaryRing.Clear();
            Loadout.PrimaryRing.RingEntry = *Ring;
            Loadout.PrimaryRing.InitializeFromRing();
            Filler = GetNameSafe(Ring->Ring);
        }
        else
        {
            return false; // nothing usable owned: the empty-primary fallback (Q-304) stays the guard
        }
        UE_LOG(LogTemp, Warning, TEXT("[Inventory] %s: loadout '%s' had no primary - filled with %s (D-116)"),
               *GetNameSafe(GetOwner()), *Loadout.LoadoutName, *Filler);
    }
    return true;
}

TArray<FDuplicateGearPair> UInventoryComponent::FindDuplicateGear() const
{
    TArray<FDuplicateGearPair> Pairs;
    const auto Collect = [&Pairs](const auto &Gear, bool bRing)
    {
        // Stronger = higher instance tier, then quality; a tie keeps the copy owned first (lower index).
        const auto Stronger = [](const auto &A, const auto &B) { return A.Tier != B.Tier ? A.Tier > B.Tier : A.Quality > B.Quality; };
        TMap<const UEquipmentDataBase *, int32> KeptIndex;
        for (int32 Index = 0; Index < Gear.Num(); ++Index)
        {
            const UEquipmentDataBase *Asset = GearEntryAsset(Gear[Index]);
            if (!Asset || !Gear[Index].PersistentID.IsValid())
            {
                continue;
            }
            int32 &Kept = KeptIndex.FindOrAdd(Asset, Index);
            if (Kept != Index && Stronger(Gear[Index], Gear[Kept]))
            {
                Kept = Index;
            }
        }
        for (int32 Index = 0; Index < Gear.Num(); ++Index)
        {
            const int32 *Kept = KeptIndex.Find(GearEntryAsset(Gear[Index]));
            if (Kept && *Kept != Index && Gear[Index].PersistentID.IsValid())
            {
                FDuplicateGearPair &Pair = Pairs.AddDefaulted_GetRef();
                Pair.KeptID = Gear[*Kept].PersistentID;
                Pair.WeakerID = Gear[Index].PersistentID;
                Pair.bRing = bRing;
            }
        }
    };
    Collect(Weapons, false);
    Collect(Rings, true);
    return Pairs;
}

int32 UInventoryComponent::MoveLoadoutSlots(const FGuid &From, const FGuid &To)
{
    using namespace InventoryGearSync;
    if (!From.IsValid() || !To.IsValid() || From == To)
    {
        return 0;
    }
    int32 Moved = 0;
    for (FCombatLoadout &Loadout : SavedLoadouts)
    {
        bool bHoldsTo = false;
        ForEachGearSlot(Loadout,
                        [&To, &bHoldsTo](const FWeaponInventoryEntry &Slot) { bHoldsTo |= Slot.PersistentID == To; },
                        [&To, &bHoldsTo](const FRingInventoryEntry &Slot) { bHoldsTo |= Slot.PersistentID == To; });
        const bool bWeaponPrimary = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None;
        const bool bFromIsPrimary = (bWeaponPrimary && Loadout.PrimaryWeapon.WeaponEntry.PersistentID == From) ||
                                    (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.RingEntry.PersistentID == From);
        if (bHoldsTo && !bFromIsPrimary)
        {
            continue; // the kept copy is already in this loadout; From's slot empties when it goes
        }
        if (bHoldsTo)
        {
            StripFromLoadout(Loadout, To); // the kept copy leaves its other slot to become the primary (D-116)
        }
        const auto Repoint = [&From, &To, &Moved](auto &Slot)
        {
            if (Slot.PersistentID == From)
            {
                Slot.PersistentID = To;
                ++Moved;
            }
        };
        ForEachGearSlot(Loadout, [&Repoint](FWeaponInventoryEntry &Slot) { Repoint(Slot); }, [&Repoint](FRingInventoryEntry &Slot) { Repoint(Slot); });
    }
    if (Moved > 0)
    {
        RefreshLoadoutsFromOwned(To); // the moved slots take the kept copy's tier, rolls and socket
        UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: %d loadout slot(s) moved from %s to %s"), *GetNameSafe(GetOwner()), Moved,
               *From.ToString(), *To.ToString());
    }
    return Moved;
}

bool UInventoryComponent::IsWornByOtherMember(const FGuid &PersistentID) const
{
    return IsWornByOtherMemberOf(PersistentID, GetPartyMembersByKey());
}

bool UInventoryComponent::IsWornByOtherMemberOf(const FGuid &PersistentID, const TMap<FName, const UCharacterData *> &Party) const
{
    const UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool || !PersistentID.IsValid())
    {
        return false;
    }
    const FName Wearer = Pool->GetWearer(PersistentID);
    // A wearer who left the party has released it (UPartySessionSubsystem); only a current member counts.
    return !Wearer.IsNone() && Wearer != SharedGearKey && Party.Contains(Wearer);
}

TArray<FWeaponInventoryEntry> UInventoryComponent::GetSelectableWeapons() const
{
    const TMap<FName, const UCharacterData *> Party = GetPartyMembersByKey();
    return Weapons.FilterByPredicate([this, &Party](const FWeaponInventoryEntry &Entry) { return !IsWornByOtherMemberOf(Entry.PersistentID, Party); });
}

TArray<FRingInventoryEntry> UInventoryComponent::GetSelectableRings() const
{
    const TMap<FName, const UCharacterData *> Party = GetPartyMembersByKey();
    return Rings.FilterByPredicate([this, &Party](const FRingInventoryEntry &Entry) { return !IsWornByOtherMemberOf(Entry.PersistentID, Party); });
}

// TODO: OBSOLETE after Crown's PIE of B-221b (Q-091 changed by Crown): with worn gear out of the other members'
// lists, the clash step below is only reached by a saved loadout; B-221b keeps that slot empty-while-worn
// instead of stripping it.
TArray<FText> UInventoryComponent::ClaimActiveSharedGear()
{
    using namespace InventoryGearSync;
    TArray<FText> Taken;
    UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool || !SavedLoadouts.IsValidIndex(ActiveLoadoutIndex))
    {
        return Taken;
    }
    const TMap<FName, const UCharacterData *> Party = GetPartyMembersByKey();
    FCombatLoadout &Active = SavedLoadouts[ActiveLoadoutIndex];
    TArray<TPair<FGuid, const UObject *>> Worn;
    ForEachGearSlot(Active,
                    [&Worn](const FWeaponInventoryEntry &Slot) { if (Slot.Weapon && Slot.PersistentID.IsValid()) { Worn.Emplace(Slot.PersistentID, Slot.Weapon); } },
                    [&Worn](const FRingInventoryEntry &Slot) { if (Slot.Ring && Slot.PersistentID.IsValid()) { Worn.Emplace(Slot.PersistentID, Slot.Ring); } });
    const bool bEvolutionWorn = Active.PrimarySlotType == EPrimarySlotType::Evolution && Active.PrimaryEvolutionInstance.IsValid();
    if (bEvolutionWorn)
    {
        Worn.Emplace(Active.PrimaryEvolutionInstance, Active.PrimaryEvolution.Item);
    }

    const UCharacterData *const *Self = Party.Find(SharedGearKey);
    // TODO: character and gear names are FString assets; FText with B-127 (D-061).
    const FText SelfName = FText::FromString(Self && *Self ? (*Self)->Name : SharedGearKey.ToString());
    TArray<FPoolGearClash> Clashes;
    TSet<FGuid> Kept;
    for (const TPair<FGuid, const UObject *> &Piece : Worn)
    {
        const FName Wearer = Pool->GetWearer(Piece.Key);
        const UCharacterData *const *Other = Wearer != SharedGearKey ? Party.Find(Wearer) : nullptr;
        if (!Other)
        {
            Pool->SetWearer(Piece.Key, SharedGearKey);
            Kept.Add(Piece.Key);
            continue;
        }
        if (bEvolutionWorn && Piece.Key == Active.PrimaryEvolutionInstance)
        {
            Active.PrimaryEvolution = FEvolutionAttachment();
            Active.PrimaryEvolutionInstance = FGuid();
        }
        else
        {
            StripFromLoadout(Active, Piece.Key);
        }
        FPoolGearClash &Clash = Clashes.AddDefaulted_GetRef();
        Clash.GearID = Piece.Key;
        Clash.ItemName = GetGearDisplayName(Piece.Value);
        Clash.WearerName = FText::FromString(*Other ? (*Other)->Name : Wearer.ToString());
        Clash.LoserKey = SharedGearKey;
        Clash.LoserName = SelfName;
        Taken.Add(FText::Format(LOCTEXT("WornByOther", "{0} (worn by {1})"), Clash.ItemName, Clash.WearerName));
    }

    // What this member wore before and no longer does is free for the others.
    TArray<FGuid> Released;
    for (const TPair<FGuid, FName> &Pair : Pool->GetWearers())
    {
        if (Pair.Value == SharedGearKey && !Kept.Contains(Pair.Key))
        {
            Released.Add(Pair.Key);
        }
    }
    for (const FGuid &ID : Released)
    {
        Pool->ReleaseWearer(ID);
    }
    Pool->NoteClashes(SharedGearKey, Clashes);
    for (const FText &Line : Taken)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Inventory] %s: taken out of the active loadout: %s"), *GetNameSafe(GetOwner()), *Line.ToString());
    }
    return Taken;
}

void UInventoryComponent::PushGearToPool(const FGuid &GearID)
{
    using namespace InventoryGearSync;
    UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool || bSyncingWithPool || !GearID.IsValid())
    {
        return;
    }
    TGuardValue<bool> Syncing(bSyncingWithPool, true);
    // B-314 (D-111, D-097): a new piece heard of while a run is open was found or bought in it: run-only, arriving at
    // the tier its definition reached in an earlier run (raised here first, so this copy and the store agree).
    const bool bFoundOnRun = Pool->IsRunLayerActive() && Pool->GetGearOwner(GearID).IsNone();
    // B-347b (Q-347): a home piece left out of the run isn't in this copy; hearing of it is not its removal (the
    // store keeps it), and it doesn't join the run.
    const bool bInCopy = FindOwned(Weapons, GearID) || FindOwned(Rings, GearID) || FindOwnedEvolution(GetOwner(), GearID);
    if (!bInCopy && !bFoundOnRun && !Pool->IsInRunView(GearID))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Inventory] %s: %s is a home piece left out of the run - store unchanged"),
               *GetNameSafe(GetOwner()), *GearID.ToString());
        return;
    }
    // B-321 (D-117): a new piece (loot, purchase) belongs to the member who got it; set before the update is heard.
    // A run-only recruit owns nothing, so what it gets goes to the party leader (B-323's give can move it).
    if (Pool->GetGearOwner(GearID).IsNone())
    {
        Pool->SetGearOwner(GearID, bDrawsWholeArmoury ? InventoryGearSync::GetLocalLeaderKey(GetWorld() ? GetWorld()->GetGameInstance() : nullptr, SharedGearKey) : SharedGearKey);
    }
    // B-347 (D-137): a home piece left out of the worn loadout but changed during the run joins the run now, with the
    // store's (still unchanged) entry as its home state, so run end puts it back too. No-op off a run or when marked.
    if (!bFoundOnRun && Pool->IsRunLayerActive())
    {
        Pool->TakeOnRun(GearID);
    }
    if (FWeaponInventoryEntry *Weapon = FindOwned(Weapons, GearID))
    {
        if (bFoundOnRun && Pool->MarkRunOnly(GearID))
        {
            Pool->ApplyUpgradeMemory(*Weapon);
        }
        Pool->UpdateWeapon(*Weapon);
    }
    else if (FRingInventoryEntry *Ring = FindOwned(Rings, GearID))
    {
        if (bFoundOnRun && Pool->MarkRunOnly(GearID))
        {
            Pool->ApplyUpgradeMemory(*Ring);
        }
        Pool->UpdateRing(*Ring);
    }
    else if (FEvolutionInventoryEntry *Evolution = FindOwnedEvolution(GetOwner(), GearID))
    {
        if (bFoundOnRun && Pool->MarkRunOnly(GearID))
        {
            Pool->ApplyUpgradeMemory(*Evolution);
        }
        Pool->UpdateEvolution(*Evolution);
    }
    else
    {
        Pool->RemoveGear(GearID);
    }
}

void UInventoryComponent::HandlePoolGearChanged(FGuid GearID)
{
    using namespace InventoryGearSync;
    const UPoolSubsystem *Pool = SharedPool.Get();
    if (!Pool || bSyncingWithPool)
    {
        return;
    }
    TGuardValue<bool> Syncing(bSyncingWithPool, true);
    UEvolutionInventoryComponent *EvoInv = GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    // B-321 (D-117): another member's piece is not in this armoury (dropped if it was, e.g. after a give).
    const FName GearOwner = Pool->GetGearOwner(GearID);
    // B-324: chest gear is nobody's, not even a run-only recruit's whole-armoury draw.
    // B-347b (Q-347): a home piece left out of the run isn't in a member's view until the run ends.
    const bool bOthers = Pool->IsInChest(GearID) || !Pool->IsInRunView(GearID) ||
                         (!bDrawsWholeArmoury && !GearOwner.IsNone() && GearOwner != SharedGearKey);
    // B-323 (D-115): a piece given to this member that it already owns a copy of waits for the duplicate choice.
    // Only a piece now this member's own: a run-only recruit hears the whole armoury, and unowned gear isn't a gift.
    const bool bMineNow = !bDrawsWholeArmoury && !GearOwner.IsNone() && GearOwner == SharedGearKey;
    const auto NoteDuplicate = [this, &GearID, bMineNow](const FGuid &OwnedID, const FString &AssetName)
    {
        if (bMineNow && OwnedID.IsValid() && OwnedID != GearID)
        {
            UE_LOG(LogTemp, Log, TEXT("[Inventory] %s: received a second %s - choose Gold or dismantle (wor.UI.Duplicates)"),
                   *GetNameSafe(GetOwner()), *AssetName);
            OnDuplicateGearReceived.Broadcast(GearID, OwnedID);
        }
    };
    if (const FWeaponInventoryEntry *SharedWeapon = bOthers ? nullptr : Pool->FindWeaponInPool(GearID))
    {
        // The twin's id is read before the mirror: adding the new piece can reallocate Weapons.
        const bool bNew = FindOwned(Weapons, GearID) == nullptr;
        const FWeaponInventoryEntry *Twin = Weapons.FindByPredicate([SharedWeapon](const FWeaponInventoryEntry &Other)
            { return Other.PersistentID != SharedWeapon->PersistentID && Other.Weapon && Other.Weapon == SharedWeapon->Weapon; });
        const FGuid TwinID = Twin ? Twin->PersistentID : FGuid();
        MirrorShared(Weapons, *SharedWeapon);
        RefreshLoadoutsFromOwned(GearID);
        if (bNew)
        {
            NoteDuplicate(TwinID, GetNameSafe(SharedWeapon->Weapon));
        }
    }
    else if (const FRingInventoryEntry *SharedRing = bOthers ? nullptr : Pool->FindRingInPool(GearID))
    {
        const bool bNew = FindOwned(Rings, GearID) == nullptr;
        const FRingInventoryEntry *Twin = Rings.FindByPredicate([SharedRing](const FRingInventoryEntry &Other)
            { return Other.PersistentID != SharedRing->PersistentID && Other.Ring && Other.Ring == SharedRing->Ring; });
        const FGuid TwinID = Twin ? Twin->PersistentID : FGuid();
        MirrorShared(Rings, *SharedRing);
        RefreshLoadoutsFromOwned(GearID);
        if (bNew)
        {
            NoteDuplicate(TwinID, GetNameSafe(SharedRing->Ring));
        }
    }
    else if (const FEvolutionInventoryEntry *SharedEvolution = bOthers ? nullptr : Pool->FindEvolutionInPool(GearID))
    {
        if (EvoInv)
        {
            if (FEvolutionInventoryEntry *Mine = FindOwnedEvolution(GetOwner(), GearID))
            {
                *Mine = *SharedEvolution;
            }
            else
            {
                EvoInv->Entries.Add(*SharedEvolution);
            }
            RefreshEvolutionInLoadouts(GearID);
        }
    }
    else
    {
        const auto Matches = [&GearID](const auto &Entry) { return Entry.PersistentID == GearID; };
        if (Weapons.RemoveAll(Matches) + Rings.RemoveAll(Matches) > 0)
        {
            RefreshLoadoutsFromOwned(GearID);
        }
        if (EvoInv && EvoInv->RemoveInstance(GearID))
        {
            RefreshEvolutionInLoadouts(GearID);
            DetachEvolutionFromOwnedGear(GearID);
        }
    }
    BroadcastInventoryChanged(EInventoryChangeType::Loaded);
}

#undef LOCTEXT_NAMESPACE

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
        PushGearToPool(EvolutionInv->Entries.Last().InstanceID);
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
        DetachEvolutionFromOwnedGear(InstanceID);
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
    bSkillsInStore = false; // B-316b: the asset's skills seed this component; the next draw decides what is kept
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
        SavedLoadouts.Add_GetRef(FCombatLoadout::CreateFromSavedLoadout(SavedLoadout, this, OwnedEvolutions)).bLore = true;
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
        // B-203 / B-322 (D-097, D-117): the pool's Crystals / Stones are storage now; a member carries its
        // own pouch instead, so the draw binds to that rather than copying storage.
        const UCharacterDataComponent *PouchOwner = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
        const UGameInstance *PouchGI = Pool->GetGameInstance();
        const UPartySessionSubsystem *PartySession = PouchGI ? PouchGI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        const FName OwnKey = UPoolSubsystem::GetCharacterKey(PouchOwner);
        const FName PouchKey = PartySession ? PartySession->GetPouchKey(OwnKey) : OwnKey; // B-361: a recruit carries the host's
        if (PouchKey.IsNone())
        {
            UE_LOG(LogTemp, Warning, TEXT("[Inventory] %s has no character data: its items stay its own, not a pouch"), *GetNameSafe(GetOwner()));
        }
        else
        {
            CrystalInv->BindSharedItems(Pool, PouchKey);
        }
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
            SavedLoadouts.Add_GetRef(FCombatLoadout::CreateFromSavedLoadout(SavedLoadout, this, EvolutionInv)).bLore = true;
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
        // B-351: the stone socket (D-141) and the one skill-slot list's picks (D-145; '-' = the built-in).
        FString Picks;
        for (const UAbilityData *Pick : W.SkillPicks)
        {
            Picks += Pick ? Pick->GetName() + TEXT(" ") : FString(TEXT("- "));
        }
        Out += FString::Printf(TEXT("\n    %s  PID=%s Tier=%s Q=%s  crystal=%s  stone=%s  skills=%s%s"),
                               W.Weapon ? *W.Weapon->Name : TEXT("(null)"),
                               *ShortGuid(W.PersistentID),
                               *TierHelpers::GetTierName(W.Tier),
                               QualityName(W.Quality),
                               *DescribeAttachment(W.AttachedItem),
                               *DescribeAttachment(W.StoneSocket),
                               W.bSkillSlotsActive ? TEXT("one list ") : TEXT("old lists "),
                               *Picks);
    }

    // ---------- Rings ----------
    Out += FString::Printf(TEXT("\n  Rings (%d):"), Rings.Num());
    for (const FRingInventoryEntry &R : Rings)
    {
        Out += FString::Printf(TEXT("\n    %s  PID=%s Tier=%s Q=%s  crystal=%s  stone=%s"),
                               R.Ring ? *R.Ring->Name : TEXT("(null)"),
                               *ShortGuid(R.PersistentID),
                               *TierHelpers::GetTierName(R.Tier),
                               QualityName(R.Quality),
                               *DescribeAttachment(R.AttachedItem),
                               *DescribeAttachment(R.StoneSocket));
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
        // B-313b: a member's pouch while bound (not the mirror), else the component's own counts.
        Out += CrystalInv->IsSharingItems() ? TEXT("\n  Crystals (pouch):") : TEXT("\n  Crystals:");
        for (const TPair<FCrystalId, int32> &Pair : CrystalInv->GetHeldStacks())
        {
            Out += FString::Printf(TEXT("\n      [%s] %s %s x%d"),
                                   CrystalTypeHelpers::IsGemType(Pair.Key.Type) ? TEXT("crystal") : TEXT("stone"),
                                   *CrystalEnum->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key.Type)),
                                   *TierHelpers::GetTierName(Pair.Key.Tier), Pair.Value);
        }
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
