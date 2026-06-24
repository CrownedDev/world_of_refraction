// EconomyService.cpp

#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/Crystals/EvolutionItemData.h" // GetAssociatedElement for the dismantle yield
#include "Loadout/LoadoutComponent.h"              // ClearPrimaryEvolution for the primary-remove action
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "GameFramework/Actor.h"

bool UEconomyService::DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count, bool bRefined)
{
    if (!Owner || Count <= 0)
    {
        return false;
    }

    // Server-authoritative action. A standalone (PIE) actor already reports authority, so this
    // single check mirrors the HasServerAuthority pattern (NM_Standalone || HasAuthority).
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    // Resolve the two components off the owner — the service couples them, they don't couple to
    // each other.
    UCrystalInventoryComponent *CrystalInv = Owner->FindComponentByClass<UCrystalInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!CrystalInv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: %s missing %s%s"),
               *Owner->GetName(),
               CrystalInv ? TEXT("") : TEXT("CrystalInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Availability in the chosen pool (Item vs Refined).
    const int32 Available = bRefined ? CrystalInv->GetRefinedCount(Id) : CrystalInv->GetItemCount(Id);
    if (Available < Count)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: %s has %d/%d %s (%s pool) — insufficient"),
               *Owner->GetName(), Available, Count, *ItemIdentity::GetDisplayName(Id),
               bRefined ? TEXT("refined") : TEXT("item"));
        return false;
    }

    const EEssenceType EssenceType = EconomyYield::ResolveEssenceType(Id);

    // REMOVE FIRST — a failed/partial removal must never grant phantom essence.
    const int32 Removed = bRefined ? CrystalInv->RemoveRefinedCount(Id, Count)
                                   : CrystalInv->RemoveItemCount(Id, Count);
    if (Removed <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: removal returned %d for %s — no grant"),
               Removed, *ItemIdentity::GetDisplayName(Id));
        return false;
    }

    // Grant essence sized to what was ACTUALLY removed (== Count after the availability check;
    // using Removed keeps the grant honest if removal ever clamps below Count).
    const int32 Yield = EconomyYield::GetTypedEssenceYieldForTier(Id.Tier) * Removed;
    Currency->AddEssenceType(EssenceType, Yield);

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled %dx %s (%s pool) -> %d essence (type %s)"),
           Removed, *ItemIdentity::GetDisplayName(Id), bRefined ? TEXT("refined") : TEXT("item"),
           Yield, *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(EssenceType)));
    return true;
}

bool UEconomyService::MergeCrystals(AActor *Owner, ECrystalType Type, EItemTier TargetTier, bool bRefined)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    // Scope guard: item-crystals + stones only. Evolution crystals are structurally unrepresentable
    // as FCrystalId (no ECrystalType::Evolution — they live in the separate UEvolutionItemData
    // system), so they can NEVER reach this pool. The only invalid Type here is None.
    if (Type == ECrystalType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s passed Type None — rejected"),
               *Owner->GetName());
        return false;
    }
    // F is the floor — nothing below it to merge from, so it can't be a merge TARGET.
    if (TargetTier == EItemTier::F_Tier)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s F_Tier cannot be a merge target"),
               *Owner->GetName());
        return false;
    }

    UCrystalInventoryComponent *CrystalInv = Owner->FindComponentByClass<UCrystalInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!CrystalInv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s missing %s%s"),
               *Owner->GetName(),
               CrystalInv ? TEXT("") : TEXT("CrystalInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const int32 TargetValue = EconomyYield::GetCrystalValue(TargetTier);
    const int32 PrismsCost = EconomyYield::GetMergeCostForTier(TargetTier);
    const int32 TargetTierIndex = TierHelpers::GetTierValue(TargetTier); // tiers [0, TargetTierIndex) are mergeable
    const FCrystalId OutputId(Type, TargetTier);

    // Per-tier owned counts below the target (synthesize each {Type, tier} key — the pool is keyed
    // by exact FCrystalId, so we query per tier rather than enumerate). Indexed by tier value.
    auto CountAt = [&](int32 TierIndex) -> int32
    {
        const FCrystalId Key(Type, TierHelpers::GetTierFromValue(TierIndex));
        return bRefined ? CrystalInv->GetRefinedCount(Key) : CrystalInv->GetItemCount(Key);
    };

    // GATHER: total available value across tiers strictly BELOW the target (this Type, this pool).
    int32 AvailableValue = 0;
    for (int32 t = 0; t < TargetTierIndex; ++t)
    {
        AvailableValue += CountAt(t) * EconomyYield::GetCrystalValue(TierHelpers::GetTierFromValue(t));
    }
    if (AvailableValue < TargetValue)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[EconomyService] MergeCrystals: %s has %d/%d %s value below %s (%s pool) — insufficient"),
               *Owner->GetName(), AvailableValue, TargetValue,
               *StaticEnum<ECrystalType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type)),
               *TierHelpers::GetTierName(TargetTier), bRefined ? TEXT("refined") : TEXT("item"));
        return false;
    }
    if (!Currency->CanAfford(ECurrencyType::Prisms, PrismsCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s cannot afford %d Prisms for %s %s"),
               *Owner->GetName(), PrismsCost,
               *TierHelpers::GetTierName(TargetTier), *ItemIdentity::GetDisplayName(OutputId));
        return false;
    }
    // Output goes to a DIFFERENT per-tier bucket, so removing inputs won't free its cap — pre-check.
    const bool bCanAddOutput = bRefined ? CrystalInv->CanAddRefinedCount(OutputId, 1)
                                        : CrystalInv->CanAddItemCount(OutputId, 1);
    if (!bCanAddOutput)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s target %s at cap — rejected"),
               *Owner->GetName(), *ItemIdentity::GetDisplayName(OutputId));
        return false;
    }

    // SELECT lowest-first: take whole crystals from the bottom up until running value >= target
    // (minimal-meets-or-exceeds; overshoot from the smallest crystals is accepted per §4.5).
    int32 ConsumeCounts[7] = {0, 0, 0, 0, 0, 0, 0}; // by tier value (F..S); only [0, TargetTierIndex) used
    int32 Accumulated = 0;
    for (int32 t = 0; t < TargetTierIndex && Accumulated < TargetValue; ++t)
    {
        const int32 Count = CountAt(t);
        const int32 Value = EconomyYield::GetCrystalValue(TierHelpers::GetTierFromValue(t));
        for (int32 i = 0; i < Count && Accumulated < TargetValue; ++i)
        {
            ++ConsumeCounts[t];
            Accumulated += Value;
        }
    }
    // AvailableValue >= TargetValue (checked) guarantees Accumulated reached the target here.

    // ---- Spend + REMOVE FIRST (a failed produce refunds below; never leave a phantom output) ----
    Currency->Spend(ECurrencyType::Prisms, PrismsCost);
    for (int32 t = 0; t < TargetTierIndex; ++t)
    {
        if (ConsumeCounts[t] > 0)
        {
            const FCrystalId Key(Type, TierHelpers::GetTierFromValue(t));
            bRefined ? CrystalInv->RemoveRefinedCount(Key, ConsumeCounts[t])
                     : CrystalInv->RemoveItemCount(Key, ConsumeCounts[t]);
        }
    }

    // ---- Produce 1 of the target tier (same Type, same pool); refund EVERYTHING on add-failure ----
    const bool bAdded = bRefined ? CrystalInv->AddRefinedCount(OutputId, 1)
                                 : CrystalInv->AddItemCount(OutputId, 1);
    if (!bAdded)
    {
        for (int32 t = 0; t < TargetTierIndex; ++t)
        {
            if (ConsumeCounts[t] > 0)
            {
                const FCrystalId Key(Type, TierHelpers::GetTierFromValue(t));
                bRefined ? CrystalInv->AddRefinedCount(Key, ConsumeCounts[t])
                         : CrystalInv->AddItemCount(Key, ConsumeCounts[t]);
            }
        }
        Currency->Add(ECurrencyType::Prisms, PrismsCost);
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: add of %s failed — refunded inputs + %d Prisms"),
               *ItemIdentity::GetDisplayName(OutputId), PrismsCost);
        return false;
    }

    // Build the consumed-set string, e.g. "2xD + 1xE".
    FString ConsumedStr;
    for (int32 t = 0; t < TargetTierIndex; ++t)
    {
        if (ConsumeCounts[t] > 0)
        {
            if (!ConsumedStr.IsEmpty())
            {
                ConsumedStr += TEXT(" + ");
            }
            ConsumedStr += FString::Printf(TEXT("%dx%s"), ConsumeCounts[t],
                                           *TierHelpers::GetTierName(TierHelpers::GetTierFromValue(t)));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Merged %s -> 1x %s (%s pool) for %d Prisms"),
           *ConsumedStr, *ItemIdentity::GetDisplayName(OutputId),
           bRefined ? TEXT("refined") : TEXT("item"), PrismsCost);
    return true;
}

bool UEconomyService::DismantleWeapon(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Find the owned instance and capture its tier BEFORE removal — the entry pointer dangles once
    // the array is mutated, so nothing past the remove dereferences it. Reads the INSTANCE Tier
    // (cluster 2b read-flip): a leveled weapon now dismantles for its CURRENT tier, not its base.
    const FWeaponInventoryEntry *Entry = Inv->Weapons.FindByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: %s has no weapon instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }
    const EItemTier Tier = Entry->Tier; // INSTANCE tier (leveled), not Entry->Weapon->Tier (asset/base)
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — a failed removal must never grant phantom essence.
    if (!Inv->RemoveWeaponByPersistentID(PersistentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: removal failed for GUID %s — no grant"),
               *PersistentID.ToString());
        return false;
    }

    Currency->AddGearEssence(Yield); // weapons/rings → Gear essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled weapon (GUID %s, tier %d) -> %d Gear essence"),
           *PersistentID.ToString(), static_cast<int32>(Tier), Yield);
    return true;
}

bool UEconomyService::DismantleRing(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const FRingInventoryEntry *Entry = Inv->Rings.FindByPredicate(
        [&PersistentID](const FRingInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Ring)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: %s has no ring instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }
    const EItemTier Tier = Entry->Tier; // INSTANCE tier (leveled), not Entry->Ring->Tier (asset/base)
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    if (!Inv->RemoveRingByPersistentID(PersistentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: removal failed for GUID %s — no grant"),
               *PersistentID.ToString());
        return false;
    }

    Currency->AddGearEssence(Yield); // weapons/rings → Gear essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled ring (GUID %s, tier %d) -> %d Gear essence"),
           *PersistentID.ToString(), static_cast<int32>(Tier), Yield);
    return true;
}

bool UEconomyService::DismantleEvolution(AActor *Owner, FGuid InstanceID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleEvolution: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UEvolutionInventoryComponent *EvoInv = Owner->FindComponentByClass<UEvolutionInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!EvoInv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleEvolution: %s missing %s%s"),
               *Owner->GetName(),
               EvoInv ? TEXT("") : TEXT("EvolutionInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Capture type + tier BEFORE removal — the entry pointer dangles once Entries mutates. Reads the
    // INSTANCE Tier (leveled), so a leveled evolution dismantles for its CURRENT tier, not its base.
    const FEvolutionInventoryEntry *Entry = EvoInv->Entries.FindByPredicate(
        [&InstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == InstanceID; });
    if (!Entry || !Entry->Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleEvolution: %s has no evolution instance for GUID %s (or null asset)"),
               *Owner->GetName(), *InstanceID.ToString());
        return false;
    }
    const EItemTier Tier = Entry->Tier; // INSTANCE tier (leveled), not Entry->Item->Tier (asset/base)
    // HYBRID yield: the evolution's ELEMENT essence TYPE at the GEAR leveling AMOUNT (§3 gear curve,
    // NOT the §4.2 crystal yield). An element-agnostic evolution (Quartz/None) maps to Generic.
    const EEssenceType EssenceType = EconomyYield::ElementToEssenceType(Entry->Item->GetAssociatedElement());
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — a failed removal must never grant phantom essence.
    if (!EvoInv->RemoveInstance(InstanceID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleEvolution: removal failed for GUID %s — no grant"),
               *InstanceID.ToString());
        return false;
    }

    Currency->AddEssenceType(EssenceType, Yield); // hybrid: element type, gear amount
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled evolution (GUID %s, tier %d) -> %d %s essence"),
           *InstanceID.ToString(), static_cast<int32>(Tier), Yield,
           *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(EssenceType)));
    return true;
}

bool UEconomyService::RemovePrimaryEvolution(AActor *Owner)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] RemovePrimaryEvolution: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    ULoadoutComponent *LC = Owner->FindComponentByClass<ULoadoutComponent>();
    if (!LC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] RemovePrimaryEvolution: %s missing LoadoutComponent"),
               *Owner->GetName());
        return false;
    }

    // 1. Self-resolve the slotted owned-entry identity from the runtime loadout (iii-b), BEFORE
    //    clearing. Invalid GUID = the primary slot is not an instance-resolved evolution → nothing
    //    to remove via this path.
    const FGuid InstanceID = LC->GetActivePrimaryEvolutionInstance();
    if (!InstanceID.IsValid())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[EconomyService] RemovePrimaryEvolution: %s has no instance-resolved primary evolution to remove"),
               *Owner->GetName());
        return false;
    }

    // 2. Clear the primary-slot reference so nothing points at the entry we're about to destroy.
    LC->ClearPrimaryEvolution();

    // 3. Destroy the owned entry + yield the dust + free a cap slot. Authoritative result.
    return DismantleEvolution(Owner, InstanceID);
}

bool UEconomyService::DismantleSpell(AActor *Owner, USpellData *Spell)
{
    if (!Owner || !Spell)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleSpell: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleSpell: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const EItemTier Tier = UInventoryComponent::ResolveSpellTier(Owner, Spell); // INSTANCE tier (leveled) — scrap a leveled spell for its current value; asset-fallback if somehow unowned
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — UnlearnSpell's bool return IS the success signal: false = the spell was not
    // known, so a not-owned spell never grants phantom essence (no HasSpell pre-check needed).
    if (!Inv->UnlearnSpell(Spell))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleSpell: %s did not know %s — no grant"),
               *Owner->GetName(), *Spell->GetName());
        return false;
    }

    Currency->AddSkillEssence(Yield); // spells/abilities → Skill essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled spell %s (tier %d) -> %d Skill essence"),
           *Spell->GetName(), static_cast<int32>(Tier), Yield);
    return true;
}

bool UEconomyService::DismantleAbility(AActor *Owner, UAbilityData *Ability)
{
    if (!Owner || !Ability)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleAbility: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleAbility: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const EItemTier Tier = UInventoryComponent::ResolveAbilityTier(Owner, Ability); // INSTANCE tier (leveled) — scrap a leveled ability for its current value; asset-fallback if somehow unowned
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — UnlearnAbility's bool return IS the success signal (false = not known).
    if (!Inv->UnlearnAbility(Ability))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleAbility: %s did not know %s — no grant"),
               *Owner->GetName(), *Ability->GetName());
        return false;
    }

    Currency->AddSkillEssence(Yield); // spells/abilities → Skill essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled ability %s (tier %d) -> %d Skill essence"),
           *Ability->GetName(), static_cast<int32>(Tier), Yield);
    return true;
}

// ==================== PURCHASE (spend-side) ====================

bool UEconomyService::PurchaseSpell(AActor *Owner, USpellData *Spell)
{
    if (!Owner || !Spell)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // ---- Compute cost (typed essence accumulated per type; Prisms = base + scaling surcharge) ----
    TMap<EEssenceType, int32> EssenceCost;
    // (a1) element essence at the spell's own tier.
    EssenceCost.FindOrAdd(EconomyYield::ElementToEssenceType(Spell->Element)) +=
        EconomyYield::GetTypedEssencePurchaseCostForTier(Spell->Tier);

    // (b) Prisms base by spell tier.
    int32 PrismsCost = EconomyYield::GetPrismsBaseForTier(Spell->Tier);

    for (const FStatScaling &Entry : Spell->StatScaling)
    {
        if (Entry.Stat == ESubStat::None)
        {
            continue;
        }
        // (a2) pillar essence at each scaling grade (same numbers as the tier buy row, §4.3) —
        //      entries sharing a pillar accumulate.
        EssenceCost.FindOrAdd(EconomyYield::SubStatToPillarEssence(Entry.Stat)) +=
            EconomyYield::GetTypedEssencePurchaseCostForTier(EconomyYield::ScalingGradeToItemTier(Entry.Tier));
        // (c) Prisms scaling surcharge: 50 × grade-number.
        PrismsCost += EconomyYield::Constants::PRISMS_SCALING_SURCHARGE_PER_GRADE *
                      EconomyYield::GetScalingGradeNumber(Entry.Tier);
    }

    // ---- CanAfford ALL components — spend nothing if any is short ----
    for (const TPair<EEssenceType, int32> &Pair : EssenceCost)
    {
        if (!Currency->CanAfford(ECurrencyType::EssenceTyped, Pair.Value, static_cast<uint8>(Pair.Key)))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: %s cannot afford %d %s essence for %s"),
                   *Owner->GetName(), Pair.Value,
                   *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key)),
                   *Spell->GetName());
            return false;
        }
    }
    if (!Currency->CanAfford(ECurrencyType::Prisms, PrismsCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: %s cannot afford %d Prisms for %s"),
               *Owner->GetName(), PrismsCost, *Spell->GetName());
        return false;
    }

    // ---- Spend ALL (CanAfford already cleared every component) ----
    for (const TPair<EEssenceType, int32> &Pair : EssenceCost)
    {
        Currency->SpendEssenceType(Pair.Key, Pair.Value);
    }
    Currency->Spend(ECurrencyType::Prisms, PrismsCost);

    // ---- Grant; refund EVERYTHING on grant-failure (e.g. at spell capacity) ----
    if (!Inv->LearnSpell(Spell))
    {
        for (const TPair<EEssenceType, int32> &Pair : EssenceCost)
        {
            Currency->AddEssenceType(Pair.Key, Pair.Value);
        }
        Currency->Add(ECurrencyType::Prisms, PrismsCost);
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: LearnSpell failed for %s — refunded %d Prisms + essence"),
               *Spell->GetName(), PrismsCost);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Purchased spell %s (tier %d) for %d Prisms + typed essence"),
           *Spell->GetName(), static_cast<int32>(Spell->Tier), PrismsCost);
    return true;
}

bool UEconomyService::PurchaseWeapon(AActor *Owner, UWeaponData *Weapon)
{
    if (!Owner || !Weapon)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Equipment pricing: Prisms base by tier only (no essence, no surcharge).
    const int32 PrismsCost = EconomyYield::GetPrismsBaseForTier(Weapon->Tier);
    if (!Currency->CanAfford(ECurrencyType::Prisms, PrismsCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: %s cannot afford %d Prisms for %s"),
               *Owner->GetName(), PrismsCost, *Weapon->GetName());
        return false;
    }

    // TODO(shop-roll): purchased items currently get the C_Quality placeholder.
    // The real design: the SHOP stocks pre-rolled items (tier+quality rolled at
    // shelf-population), and purchase CARRIES that shelf-rolled quality through —
    // no roll at point-of-sale, not a fixed C. Gated on the loot/shop generator
    // (does not exist yet). Until then, purchase = C placeholder.
    Currency->Spend(ECurrencyType::Prisms, PrismsCost);
    if (!Inv->AddWeapon(Weapon))
    {
        Currency->Add(ECurrencyType::Prisms, PrismsCost); // refund on grant-failure (e.g. at capacity)
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: AddWeapon failed for %s — refunded %d Prisms"),
               *Weapon->GetName(), PrismsCost);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Purchased weapon %s (tier %d) for %d Prisms"),
           *Weapon->GetName(), static_cast<int32>(Weapon->Tier), PrismsCost);
    return true;
}

// ==================== LEVELING (instance tier-up, spend-side) ====================

bool UEconomyService::TryLevelUpEntry(UCurrencyComponent *Currency, EItemTier &InOutTier,
                                      ECurrencyType LevelingEssence) const
{
    if (!Currency)
    {
        return false;
    }

    const EItemTier CurrentTier = InOutTier;

    // S-cap: S is the max tier — there is nothing above it to buy.
    if (CurrentTier == EItemTier::S_Tier)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] TryLevelUpEntry: already S_Tier — cannot level past max"));
        return false;
    }

    // Cost (§5.3): full leveling essence to reach the next tier + HALF that in Reality. No Gold.
    // LevelingEssence is the §3 category currency — Gear (weapons/rings/evolution) or Skill
    // (spells/abilities); the cost ladder + the ½-Reality co-cost are shared across both.
    const int32 LevelingCost = EconomyYield::GetTierUpCostForTier(CurrentTier);
    const int32 RealityCost = LevelingCost / 2;

    // CanAfford BOTH before spending anything — if either is short, spend nothing and bail.
    if (!Currency->CanAfford(LevelingEssence, LevelingCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] TryLevelUpEntry: cannot afford %d leveling essence (from %s)"),
               LevelingCost, *TierHelpers::GetTierName(CurrentTier));
        return false;
    }
    if (!Currency->CanAfford(ECurrencyType::EssenceTyped, RealityCost, static_cast<uint8>(EEssenceType::Reality)))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] TryLevelUpEntry: cannot afford %d Reality essence (from %s)"),
               RealityCost, *TierHelpers::GetTierName(CurrentTier));
        return false;
    }

    // Spend BOTH (the afford-checks above already cleared each component).
    Currency->Spend(LevelingEssence, LevelingCost);
    Currency->SpendEssenceType(EEssenceType::Reality, RealityCost);

    // WRITE the instance tier one step UP. EItemTier is forward-ordered (F_Tier=0 .. S_Tier=6),
    // so the next-stronger tier is value+1; GetTierFromValue clamps 0..6 so this never overruns S
    // (and the S-cap above already guarantees CurrentTier < S here). This in-place write through
    // the caller's tier-ref cannot fail, so no post-spend refund path is needed.
    const EItemTier NextTier = TierHelpers::GetTierFromValue(TierHelpers::GetTierValue(CurrentTier) + 1);
    InOutTier = NextTier;

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Tier-up: %s -> %s (spent %d leveling + %d Reality essence)"),
           *TierHelpers::GetTierName(CurrentTier), *TierHelpers::GetTierName(NextTier), LevelingCost, RealityCost);
    return true;
}

bool UEconomyService::TryDowngradeEntry(UCurrencyComponent *Currency, EItemTier &InOutTier,
                                        EItemTier FloorTier, ECurrencyType LevelingEssence) const
{
    if (!Currency)
    {
        return false;
    }

    const EItemTier CurrentTier = InOutTier;

    // Floor: can't revert below the item's AUTHORED base — you only refund tiers you leveled UP to,
    // never the tier it shipped at. (At/below base → nothing to downgrade.)
    if (TierHelpers::GetTierValue(CurrentTier) <= TierHelpers::GetTierValue(FloorTier))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] TryDowngradeEntry: %s is at/below its base %s — nothing to revert"),
               *TierHelpers::GetTierName(CurrentTier), *TierHelpers::GetTierName(FloorTier));
        return false;
    }

    // The step being reverted is DownTier -> CurrentTier; its level-up cost is
    // GetTierUpCostForTier(DownTier) (exact mirror of TryLevelUpEntry). Refund HALF in the leveling
    // essence ONLY — the ½-Reality co-cost paid at level-up is gone (reverting costs the reshape
    // currency). Write the lowered tier in place; neither the write nor the Add can fail.
    const EItemTier DownTier = TierHelpers::GetTierFromValue(TierHelpers::GetTierValue(CurrentTier) - 1);
    const int32 StepCost = EconomyYield::GetTierUpCostForTier(DownTier);
    const int32 Refund = StepCost / 2;

    InOutTier = DownTier;
    Currency->Add(LevelingEssence, Refund);

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Tier-down: %s -> %s (refunded %d leveling essence; Reality co-cost not refunded)"),
           *TierHelpers::GetTierName(CurrentTier), *TierHelpers::GetTierName(DownTier), Refund);
    return true;
}

bool UEconomyService::LevelUpWeapon(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpWeapon: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpWeapon: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // MUTABLE entry — the core writes Entry.Tier in place. Nothing mutates Inv->Weapons here, so
    // the pointer stays valid across the currency spend inside TryLevelUpEntry.
    FWeaponInventoryEntry *Entry = Inv->Weapons.FindByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpWeapon: %s has no weapon instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }

    return TryLevelUpEntry(Currency, Entry->Tier);
}

bool UEconomyService::LevelUpRing(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpRing: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpRing: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FRingInventoryEntry *Entry = Inv->Rings.FindByPredicate(
        [&PersistentID](const FRingInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Ring)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpRing: %s has no ring instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }

    return TryLevelUpEntry(Currency, Entry->Tier);
}

bool UEconomyService::LevelUpEvolution(AActor *Owner, FGuid InstanceID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpEvolution: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    // Evolution instances live on the dedicated UEvolutionInventoryComponent, NOT UInventoryComponent.
    UEvolutionInventoryComponent *EvoInv = Owner->FindComponentByClass<UEvolutionInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!EvoInv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpEvolution: %s missing %s%s"),
               *Owner->GetName(),
               EvoInv ? TEXT("") : TEXT("EvolutionInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // MUTABLE entry — identity is InstanceID (not PersistentID). The entry persists while
    // primary-slotted, so leveling it here also lifts the slot on the next loadout inflation.
    // Nothing mutates Entries here, so the pointer stays valid across the spend in TryLevelUpEntry.
    FEvolutionInventoryEntry *Entry = EvoInv->Entries.FindByPredicate(
        [&InstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == InstanceID; });
    if (!Entry || !Entry->Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpEvolution: %s has no evolution instance for GUID %s (or null asset)"),
               *Owner->GetName(), *InstanceID.ToString());
        return false;
    }

    return TryLevelUpEntry(Currency, Entry->Tier);
}

bool UEconomyService::LevelUpSpell(AActor *Owner, const USpellData *Spell)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpSpell: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpSpell: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Asset-keyed mutable lookup (owners hold <=1 instance per asset). Can't level a spell you
    // don't own. Nothing mutates LearnedSpells here, so the pointer is valid across the spend.
    FSpellInstance *Instance = Inv->Spells.FindSpellInstanceMutable(Spell);
    if (!Instance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpSpell: %s does not own %s"),
               *Owner->GetName(), Spell ? *Spell->GetName() : TEXT("(null)"));
        return false;
    }

    // Spells level on SKILL essence (+ ½ Reality), §3 category split.
    return TryLevelUpEntry(Currency, Instance->Tier, ECurrencyType::SkillEssence);
}

bool UEconomyService::LevelUpAbility(AActor *Owner, const UAbilityData *Ability)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpAbility: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpAbility: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FAbilityInstance *Instance = Inv->Abilities.FindAbilityInstanceMutable(Ability);
    if (!Instance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] LevelUpAbility: %s does not own %s"),
               *Owner->GetName(), Ability ? *Ability->GetName() : TEXT("(null)"));
        return false;
    }

    return TryLevelUpEntry(Currency, Instance->Tier, ECurrencyType::SkillEssence);
}

bool UEconomyService::DowngradeWeapon(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeWeapon: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeWeapon: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FWeaponInventoryEntry *Entry = Inv->Weapons.FindByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeWeapon: %s has no weapon instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }

    // Floor = the weapon asset's authored Tier (leveling only mutates the instance .Tier).
    return TryDowngradeEntry(Currency, Entry->Tier, Entry->Weapon->Tier, ECurrencyType::GearEssence);
}

bool UEconomyService::DowngradeRing(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeRing: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeRing: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FRingInventoryEntry *Entry = Inv->Rings.FindByPredicate(
        [&PersistentID](const FRingInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Ring)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeRing: %s has no ring instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }

    return TryDowngradeEntry(Currency, Entry->Tier, Entry->Ring->Tier, ECurrencyType::GearEssence);
}

bool UEconomyService::DowngradeEvolution(AActor *Owner, FGuid InstanceID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeEvolution: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UEvolutionInventoryComponent *EvoInv = Owner->FindComponentByClass<UEvolutionInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!EvoInv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeEvolution: %s missing %s%s"),
               *Owner->GetName(),
               EvoInv ? TEXT("") : TEXT("EvolutionInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FEvolutionInventoryEntry *Entry = EvoInv->Entries.FindByPredicate(
        [&InstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == InstanceID; });
    if (!Entry || !Entry->Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeEvolution: %s has no evolution instance for GUID %s (or null asset)"),
               *Owner->GetName(), *InstanceID.ToString());
        return false;
    }

    return TryDowngradeEntry(Currency, Entry->Tier, Entry->Item->Tier, ECurrencyType::GearEssence);
}

bool UEconomyService::DowngradeSpell(AActor *Owner, const USpellData *Spell)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeSpell: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeSpell: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FSpellInstance *Instance = Inv->Spells.FindSpellInstanceMutable(Spell);
    if (!Instance || !Instance->Spell)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeSpell: %s does not own %s"),
               *Owner->GetName(), Spell ? *Spell->GetName() : TEXT("(null)"));
        return false;
    }

    // Floor = the spell asset's authored Tier; Skill essence (spells level on Skill, §3).
    return TryDowngradeEntry(Currency, Instance->Tier, Instance->Spell->Tier, ECurrencyType::SkillEssence);
}

bool UEconomyService::DowngradeAbility(AActor *Owner, const UAbilityData *Ability)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeAbility: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeAbility: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    FAbilityInstance *Instance = Inv->Abilities.FindAbilityInstanceMutable(Ability);
    if (!Instance || !Instance->Ability)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DowngradeAbility: %s does not own %s"),
               *Owner->GetName(), Ability ? *Ability->GetName() : TEXT("(null)"));
        return false;
    }

    return TryDowngradeEntry(Currency, Instance->Tier, Instance->Ability->Tier, ECurrencyType::SkillEssence);
}
