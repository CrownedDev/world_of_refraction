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
#include "Combat/CombatConstants.h"
#include "Combat/TurnManager.h" // speed-notify on speed-relevant crystal detach
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Character/FPillarWeights.h"
#include "Character/CharacterDataComponent.h" // owner Luck for the pickup quality roll
#include "Currency/EconomyYield.h"            // RollQuality (§11 weighted drop curve)

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

bool UInventoryComponent::LearnSpell(USpellData *Spell)
{
    return Spells.LearnSpell(Spell);
}

bool UInventoryComponent::UnlearnSpell(USpellData *Spell)
{
    return Spells.UnlearnSpell(Spell);
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
    return Abilities.LearnAbility(Ability);
}

bool UInventoryComponent::UnlearnAbility(UAbilityData *Ability)
{
    return Abilities.UnlearnAbility(Ability);
}

bool UInventoryComponent::HasAbility(UAbilityData *Ability) const
{
    return Abilities.HasAbility(Ability);
}

TArray<UAbilityData *> UInventoryComponent::GetAbilitiesForWeaponType(EWeaponType WeaponType) const
{
    return Abilities.GetAbilitiesForWeaponType(WeaponType);
}

// ==================== WEAPON OPERATIONS ====================

bool UInventoryComponent::AddWeapon(UWeaponData *Weapon, bool bCopyDefaultCrystal)
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

    FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon, bCopyDefaultCrystal);
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
    return true;
}

bool UInventoryComponent::RemoveWeapon(int32 WeaponIndex)
{
    if (!Weapons.IsValidIndex(WeaponIndex))
    {
        return false;
    }

    Weapons.RemoveAt(WeaponIndex);
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
    if (!Weapons.IsValidIndex(WeaponIndex))
    {
        return false;
    }

    const bool bHadAttachment = !Weapons[WeaponIndex].AttachedItem.IsEmpty();
    // Speed-relevance gate, read BEFORE the detach: a TurnSpeed-contributing stone
    // (plain TurnSpeedStone or a fusion with a TurnSpeed half/bonus — same
    // GetAttachedStonePercent read CalculateSpeedRatios uses) feeds the stone factor,
    // so its removal must recalc turn order. Non-speed stones skip the notify.
    const bool bWasSpeedStone =
        CrystalEffectTable::GetAttachedStonePercent(Weapons[WeaponIndex].AttachedItem, ESubStat::TurnSpeed) > 0.0f;

    Weapons[WeaponIndex].RemoveCrystal();

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

    return bHadAttachment;
}

// ==================== RING OPERATIONS ====================

bool UInventoryComponent::AddRing(URingData *Ring, bool bCopyDefaultCrystal)
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

    FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring, bCopyDefaultCrystal);
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
    return true;
}

bool UInventoryComponent::RemoveRing(int32 RingIndex)
{
    if (!Rings.IsValidIndex(RingIndex))
    {
        return false;
    }

    Rings.RemoveAt(RingIndex);
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
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToWeapon: no authority — ignored"));
        return false;
    }
    if (!EvoInstanceID.IsValid())
    {
        return false;
    }

    // Resolve the owned evolution entry being attached (the reference target — it PERSISTS in EvoInv).
    UEvolutionInventoryComponent *EvoInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    const FEvolutionInventoryEntry *Entry = EvoInv
        ? EvoInv->Entries.FindByPredicate(
              [&EvoInstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == EvoInstanceID; })
        : nullptr;
    if (!Entry || !Entry->Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToWeapon: no owned evolution for GUID %s"),
               *EvoInstanceID.ToString());
        return false;
    }

    // One evo, one slot: reject if it is already referenced by any primary or gear slot.
    if (IsEvolutionSlottedAnywhere(*this, EvoInstanceID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToWeapon: evolution %s already slotted elsewhere"),
               *EvoInstanceID.ToString());
        return false;
    }

    // Resolve the target weapon; its crystal slot must be empty (don't overwrite an existing attachment).
    const int32 Index = Weapons.IndexOfByPredicate(
        [&WeaponPersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == WeaponPersistentID; });
    if (Index == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToWeapon: no weapon for GUID %s"),
               *WeaponPersistentID.ToString());
        return false;
    }
    if (!Weapons[Index].AttachedItem.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToWeapon: weapon slot already occupied"));
        return false;
    }

    // Reference the owned entry (entry stays in EvoInv->Entries — reference model, not move).
    Weapons[Index].AttachedItem = MakeEvolutionAttachment(*Entry, EvoInstanceID);
    UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Attached evolution %s (tier %d) to weapon %s"),
           *EvoInstanceID.ToString(), static_cast<int32>(Entry->Tier), *WeaponPersistentID.ToString());
    return true;
}

bool UInventoryComponent::AttachEvolutionToRing(FGuid RingPersistentID, FGuid EvoInstanceID)
{
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToRing: no authority — ignored"));
        return false;
    }
    if (!EvoInstanceID.IsValid())
    {
        return false;
    }

    UEvolutionInventoryComponent *EvoInv =
        GetOwner() ? GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
    const FEvolutionInventoryEntry *Entry = EvoInv
        ? EvoInv->Entries.FindByPredicate(
              [&EvoInstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == EvoInstanceID; })
        : nullptr;
    if (!Entry || !Entry->Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToRing: no owned evolution for GUID %s"),
               *EvoInstanceID.ToString());
        return false;
    }

    if (IsEvolutionSlottedAnywhere(*this, EvoInstanceID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToRing: evolution %s already slotted elsewhere"),
               *EvoInstanceID.ToString());
        return false;
    }

    const int32 Index = Rings.IndexOfByPredicate(
        [&RingPersistentID](const FRingInventoryEntry &E) { return E.PersistentID == RingPersistentID; });
    if (Index == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToRing: no ring for GUID %s"),
               *RingPersistentID.ToString());
        return false;
    }
    if (!Rings[Index].AttachedItem.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AttachEvolutionToRing: ring slot already occupied"));
        return false;
    }

    Rings[Index].AttachedItem = MakeEvolutionAttachment(*Entry, EvoInstanceID);
    UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Attached evolution %s (tier %d) to ring %s"),
           *EvoInstanceID.ToString(), static_cast<int32>(Entry->Tier), *RingPersistentID.ToString());
    return true;
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
    if (!Rings.IsValidIndex(RingIndex))
    {
        return false;
    }

    const bool bHadAttachment = !Rings[RingIndex].AttachedItem.IsEmpty();
    // Same speed-relevance gate as RemoveCrystalFromWeapon. NOTE: ring attachments do
    // not feed the turn-speed read path today (CalculateSpeedRatios reads the active
    // WEAPON attachment only) — this notify is defensive symmetry; the gated recalc is
    // a no-op until rings join that read path.
    const bool bWasSpeedStone =
        CrystalEffectTable::GetAttachedStonePercent(Rings[RingIndex].AttachedItem, ESubStat::TurnSpeed) > 0.0f;

    Rings[RingIndex].RemoveCrystal();

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

    return bHadAttachment;
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
    for (const FString &Error : InventoryAsset->GetValidationErrors())
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
        CrystalInv->ItemCrystals.Empty();
        CrystalInv->RefinedCrystals.Empty();
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
            AddWeapon(Weapon, true);
        }
    }

    // ---------- Rings ----------
    for (URingData *Ring : InventoryAsset->Rings)
    {
        if (Ring)
        {
            AddRing(Ring, true);
        }
    }

    // ---------- Crystals (item / refined / evolution) ----------
    // ItemCrystals → CrystalInv->AddItemCount per (Id, Count).
    // RefinedCrystals → CrystalInv->AddRefinedCount per (Id, Count).
    // EvolutionEquipment → EvolutionInv->AddInstance per entry.
    // Sibling components are warned about above when missing; per-block
    // guards surface the misconfig with the specific dropped data.
    if (CrystalInv)
    {
        for (const TPair<FCrystalId, int32> &Pair : InventoryAsset->ItemCrystals)
        {
            if (Pair.Value > 0 && !CrystalInv->AddItemCount(Pair.Key, Pair.Value))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): ItemCrystals (Type=%d, Tier=%d) count %d rejected (per-tier cap)"),
                       *CharacterData->Name,
                       static_cast<int32>(Pair.Key.Type),
                       static_cast<int32>(Pair.Key.Tier),
                       Pair.Value);
            }
        }
        for (const TPair<FCrystalId, int32> &Pair : InventoryAsset->RefinedCrystals)
        {
            if (Pair.Value > 0 && !CrystalInv->AddRefinedCount(Pair.Key, Pair.Value))
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): RefinedCrystals (Type=%d, Tier=%d) count %d rejected (per-tier cap)"),
                       *CharacterData->Name,
                       static_cast<int32>(Pair.Key.Type),
                       static_cast<int32>(Pair.Key.Tier),
                       Pair.Value);
            }
        }
    }
    else if (InventoryAsset->ItemCrystals.Num() > 0 || InventoryAsset->RefinedCrystals.Num() > 0)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[InventoryComponent] InitializeFromInventoryAsset(%s): ItemCrystals/RefinedCrystals authored but UCrystalInventoryComponent missing — data dropped"),
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

    const int32 ItemCount = CrystalInv ? CrystalInv->ItemCrystals.Num() : 0;
    const int32 RefinedCount = CrystalInv ? CrystalInv->RefinedCrystals.Num() : 0;
    const int32 EvolutionCount = EvolutionInv ? EvolutionInv->Num() : 0;

    UE_LOG(LogTemp, Display,
           TEXT("[InventoryComponent] Initialized inventory from %s: %d weapons, %d rings, pool-entries=[item:%d refined:%d evolution:%d], %d spells, %d abilities, %d loadouts (active=%d)"),
           *InventoryAsset->GetName(),
           Weapons.Num(),
           Rings.Num(),
           ItemCount,
           RefinedCount,
           EvolutionCount,
           Spells.GetCount(),
           Abilities.GetCount(),
           SavedLoadouts.Num(),
           ActiveLoadoutIndex);
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
