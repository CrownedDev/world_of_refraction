// EconomyService.cpp

#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Currency/CurrencyComponent.h"
#include "Character/CharacterDataComponent.h" // BuyWorldStat reach target (§7 C4)
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h" // slot/cap constants for the Purchase pre-validation
#include "Templates/Function.h"           // TFunction undo-thunks for Purchase rollback
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
// --- Debug snapshot + wor.* console command ---
#include "Currency/CurrencyComponentDebug.h" // GetWalletString — reuse the wallet formatter
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h" // GEngine on-screen debug
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

bool UEconomyService::DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count)
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
    // Route the count mutation through the UInventoryComponent facade so the dismantle
    // emits OnInventoryChanged; CrystalInv is retained for the read-only availability query.
    // (The facade owns the signal; in well-formed actors it coexists with the sibling pools.)
    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    if (!CrystalInv || !Currency || !Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: %s missing %s%s%s"),
               *Owner->GetName(),
               CrystalInv ? TEXT("") : TEXT("CrystalInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent "),
               Inv ? TEXT("") : TEXT("InventoryComponent"));
        return false;
    }

    // Availability in Id's pool (gem/stone dispatched inside the component).
    const int32 Available = CrystalInv->GetCount(Id);
    if (Available < Count)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: %s has %d/%d %s — insufficient"),
               *Owner->GetName(), Available, Count, *ItemIdentity::GetDisplayName(Id));
        return false;
    }

    const EEssenceType EssenceType = EconomyYield::ResolveEssenceType(Id);

    // REMOVE FIRST — a failed/partial removal must never grant phantom essence.
    const int32 Removed = Inv->RemoveCrystal(Id, Count);
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

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled %dx %s -> %d essence (type %s)"),
           Removed, *ItemIdentity::GetDisplayName(Id), Yield,
           *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(EssenceType)));
    return true;
}

bool UEconomyService::MergeCrystals(AActor *Owner, ECrystalType Type, EItemTier TargetTier)
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
    // Mutations route through the facade (emit OnInventoryChanged); CrystalInv kept for reads/caps.
    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    if (!CrystalInv || !Currency || !Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s missing %s%s%s"),
               *Owner->GetName(),
               CrystalInv ? TEXT("") : TEXT("CrystalInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent "),
               Inv ? TEXT("") : TEXT("InventoryComponent"));
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
        return CrystalInv->GetCount(Key); // gem/stone dispatched inside the component
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
               TEXT("[EconomyService] MergeCrystals: %s has %d/%d %s value below %s — insufficient"),
               *Owner->GetName(), AvailableValue, TargetValue,
               *StaticEnum<ECrystalType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type)),
               *TierHelpers::GetTierName(TargetTier));
        return false;
    }
    if (!Currency->CanAfford(ECurrencyType::Prisms, PrismsCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] MergeCrystals: %s cannot afford %d Prisms for %s %s"),
               *Owner->GetName(), PrismsCost,
               *TierHelpers::GetTierName(TargetTier), *ItemIdentity::GetDisplayName(OutputId));
        return false;
    }
    // Output is at a DIFFERENT tier than the inputs, so removing inputs won't free its per-tier cap
    // — pre-check (the cap is per tier, so the lower-tier consumes don't open the target tier).
    const bool bCanAddOutput = CrystalInv->CanAddCount(OutputId, 1);
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

    // Flatten the consumed inputs into a flat FCrystalId set (one entry per consumed crystal; the
    // primitive tallies duplicates). The component dispatches gem/stone per Id. Reused verbatim for
    // the consume AND the refund.
    TArray<FCrystalId> Inputs;
    for (int32 t = 0; t < TargetTierIndex; ++t)
    {
        const FCrystalId Key(Type, TierHelpers::GetTierFromValue(t));
        for (int32 i = 0; i < ConsumeCounts[t]; ++i)
        {
            Inputs.Add(Key);
        }
    }

    // ---- Spend + REMOVE FIRST (a failed produce refunds below; never leave a phantom output) ----
    Currency->Spend(ECurrencyType::Prisms, PrismsCost);
    Inv->RemoveCrystals(Inputs); // ONE Removed (inputs are guaranteed present — counted above)

    // ---- Produce 1 of the target tier (same Type, same pool); refund EVERYTHING on add-failure ----
    const bool bAdded = Inv->AddCrystals({OutputId}); // ONE Added
    if (!bAdded)
    {
        Inv->AddCrystals(Inputs); // ONE Added — re-add the exact consumed set (atomic; space just freed)
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

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Merged %s -> 1x %s for %d Prisms"),
           *ConsumedStr, *ItemIdentity::GetDisplayName(OutputId), PrismsCost);
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
    // Removal routes through the facade (emit OnInventoryChanged); EvoInv kept for the read.
    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    if (!EvoInv || !Currency || !Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleEvolution: %s missing %s%s%s"),
               *Owner->GetName(),
               EvoInv ? TEXT("") : TEXT("EvolutionInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent "),
               Inv ? TEXT("") : TEXT("InventoryComponent"));
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
    if (!Inv->RemoveEvolutionInstance(InstanceID))
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

FPurchaseCost UEconomyService::BuildCartCost(const TArray<FMerchantStockEntry> &Items) const
{
    FPurchaseCost Cost;
    for (const FMerchantStockEntry &E : Items)
    {
        if (E.IsCrystalEntry())
        {
            const int32 Count = FMath::Max(1, E.Count);
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(E.Crystal.Tier) * Count;
            Cost.AddTyped(EconomyYield::ResolveEssenceType(E.Crystal),
                          EconomyYield::GetTypedEssencePurchaseCostForTier(E.Crystal.Tier) * Count);
            continue;
        }
        if (!E.IsAssetEntry())
        {
            continue; // empty entry prices at 0 — Purchase rejects it separately
        }

        UPrimaryDataAsset *Asset = E.Asset;
        if (UWeaponData *W = Cast<UWeaponData>(Asset))
        {
            // Equipment pricing: Prisms base by tier (quality = C placeholder until shop-roll).
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(W->Tier) * FMath::Max(1, E.Count);
        }
        else if (URingData *R = Cast<URingData>(Asset))
        {
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(R->Tier) * FMath::Max(1, E.Count);
        }
        else if (USpellData *S = Cast<USpellData>(Asset))
        {
            // Count clamps to 1 for all spells (a spell can't be owned twice).
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(S->Tier);
            if (S->Element == ESpellElement::Generic)
            {
                // GENERIC spells price like abilities: base + SkillEssence at tier.
                // No scaling surcharge, no element essence, no pillar essence —
                // Generic would otherwise charge "Quartz essence" (Crown, 3d).
                Cost.SkillEssence += EconomyYield::GetTypedEssencePurchaseCostForTier(S->Tier);
            }
            else
            {
                // ELEMENT-typed skill pricing (§4.4 + §5): base + surcharge + element
                // essence + pillar essence per scaling grade.
                Cost.AddTyped(EconomyYield::ElementToEssenceType(S->Element),
                              EconomyYield::GetTypedEssencePurchaseCostForTier(S->Tier));
                for (const FStatScaling &Sc : S->StatScaling)
                {
                    if (Sc.Stat == ESubStat::None)
                    {
                        continue;
                    }
                    Cost.AddTyped(EconomyYield::SubStatToPillarEssence(Sc.Stat),
                                  EconomyYield::GetTypedEssencePurchaseCostForTier(EconomyYield::ScalingGradeToItemTier(Sc.Tier)));
                    Cost.Prisms += EconomyYield::Constants::PRISMS_SCALING_SURCHARGE_PER_GRADE *
                                   EconomyYield::GetScalingGradeNumber(Sc.Tier);
                }
            }
        }
        else if (UAbilityData *A = Cast<UAbilityData>(Asset))
        {
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(A->Tier);
            Cost.SkillEssence += EconomyYield::GetTypedEssencePurchaseCostForTier(A->Tier);
        }
        else if (UEvolutionItemData *Ev = Cast<UEvolutionItemData>(Asset))
        {
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(Ev->Tier);
            const int32 ElementAmount = EconomyYield::GetTypedEssencePurchaseCostForTier(Ev->Tier);
            Cost.AddTyped(EconomyYield::ElementToEssenceType(Ev->GetAssociatedElement()), ElementAmount);
            Cost.AddTyped(EEssenceType::Reality, ElementAmount / 2); // ½ Reality co-cost (mirrors leveling)
        }
        // Unknown asset types price at 0 — Purchase rejects them separately.
    }
    return Cost;
}

FPurchaseCost UEconomyService::PreviewCartCost(const TArray<FMerchantStockEntry> &Items) const
{
    return BuildCartCost(Items);
}

bool UEconomyService::Purchase(AActor *Owner, const TArray<FMerchantStockEntry> &Items)
{
    if (!Owner || Items.Num() == 0)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: no authority on %s — ignored"), *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }
    // Sibling pools — only REQUIRED when the cart carries their goods (checked in validation).
    UCrystalInventoryComponent *CrystalInv = Owner->FindComponentByClass<UCrystalInventoryComponent>();
    UEvolutionInventoryComponent *EvolutionInv = Owner->FindComponentByClass<UEvolutionInventoryComponent>();

    // ============ VALIDATION PASS — cost + cumulative-per-type demand, spend nothing ============
    // Cost comes from the shared builder (also behind PreviewCartCost), so the charged numbers
    // can never drift from what the shop UI displayed.
    const FPurchaseCost Cost = BuildCartCost(Items);

    int32 WeaponDemand = 0, RingDemand = 0, SpellDemand = 0, AbilityDemand = 0, EvolutionDemand = 0;
    TMap<FCrystalId, int32> CrystalDemand;
    // Spells/abilities can't be owned twice — reject already-owned or in-cart duplicates so the
    // grant loop can never no-op, keeping the charged cost matched to what is actually granted.
    TSet<const USpellData *> CartSpells;
    TSet<const UAbilityData *> CartAbilities;

    for (int32 i = 0; i < Items.Num(); ++i)
    {
        const FMerchantStockEntry &E = Items[i];

        if (E.IsCrystalEntry())
        {
            CrystalDemand.FindOrAdd(E.Crystal) += FMath::Max(1, E.Count);
            continue;
        }
        if (!E.IsAssetEntry())
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: Items[%d] is empty (no Asset or Crystal) — aborting."), i);
            return false;
        }

        UPrimaryDataAsset *Asset = E.Asset;
        if (Cast<UWeaponData>(Asset))
        {
            WeaponDemand += FMath::Max(1, E.Count);
        }
        else if (Cast<URingData>(Asset))
        {
            RingDemand += FMath::Max(1, E.Count);
        }
        else if (USpellData *S = Cast<USpellData>(Asset))
        {
            if (Inv->Spells.HasSpell(S) || CartSpells.Contains(S))
            {
                UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: spell %s already owned or duplicated in cart — aborting."), *S->GetName());
                return false;
            }
            CartSpells.Add(S);
            SpellDemand += 1;
        }
        else if (UAbilityData *A = Cast<UAbilityData>(Asset))
        {
            if (Inv->Abilities.HasAbility(A) || CartAbilities.Contains(A))
            {
                UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: ability %s already owned or duplicated in cart — aborting."), *A->GetName());
                return false;
            }
            CartAbilities.Add(A);
            AbilityDemand += 1;
        }
        else if (Cast<UEvolutionItemData>(Asset))
        {
            EvolutionDemand += 1;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: Items[%d] asset %s is not a sellable type — aborting."), i, *Asset->GetName());
            return false;
        }
    }

    // ---- Cumulative capacity: fresh purchases are bare, so each costs its BASE slot ----
    if (WeaponDemand > 0 && Inv->GetRemainingWeaponCapacity() < WeaponDemand * InventoryConstants::WEAPON_BASE_SLOT_COST)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s weapon capacity short (need %d slots) — aborting."), *Owner->GetName(), WeaponDemand);
        return false;
    }
    if (RingDemand > 0 && Inv->GetRemainingRingCapacity() < RingDemand * InventoryConstants::RING_BASE_SLOT_COST)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s ring capacity short (need %d slots) — aborting."), *Owner->GetName(), RingDemand);
        return false;
    }
    if (SpellDemand > 0 && Inv->Spells.GetRemainingCapacity() < SpellDemand)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s spell capacity short (need %d) — aborting."), *Owner->GetName(), SpellDemand);
        return false;
    }
    if (AbilityDemand > 0 && Inv->Abilities.GetRemainingCapacity() < AbilityDemand)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s ability capacity short (need %d) — aborting."), *Owner->GetName(), AbilityDemand);
        return false;
    }
    if (EvolutionDemand > 0)
    {
        if (!EvolutionInv)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s missing EvolutionInventoryComponent for evolution stock — aborting."), *Owner->GetName());
            return false;
        }
        if (InventoryConstants::MAX_EVOLUTION_ITEMS - EvolutionInv->CountRunEvolutions() < EvolutionDemand)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s evolution cap short (need %d) — aborting."), *Owner->GetName(), EvolutionDemand);
            return false;
        }
    }
    if (CrystalDemand.Num() > 0)
    {
        if (!CrystalInv)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s missing CrystalInventoryComponent for crystal stock — aborting."), *Owner->GetName());
            return false;
        }
        for (const TPair<FCrystalId, int32> &Pair : CrystalDemand)
        {
            if (!CrystalInv->CanAddCount(Pair.Key, Pair.Value))
            {
                UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s crystal cap short for %s (need %d) — aborting."),
                       *Owner->GetName(), *ItemIdentity::GetDisplayName(Pair.Key), Pair.Value);
                return false;
            }
        }
    }

    // ============ AFFORDABILITY — every currency once, spend nothing if any is short ============
    if (!Currency->CanAfford(ECurrencyType::Prisms, Cost.Prisms))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s cannot afford %d Prisms — aborting."), *Owner->GetName(), Cost.Prisms);
        return false;
    }
    if (Cost.SkillEssence > 0 && !Currency->CanAfford(ECurrencyType::SkillEssence, Cost.SkillEssence))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s cannot afford %d SkillEssence — aborting."), *Owner->GetName(), Cost.SkillEssence);
        return false;
    }
    for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
    {
        if (!Currency->CanAfford(ECurrencyType::EssenceTyped, Pair.Value, static_cast<uint8>(Pair.Key)))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s cannot afford %d %s essence — aborting."),
                   *Owner->GetName(), Pair.Value,
                   *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key)));
            return false;
        }
    }

    // ============ SPEND — every currency once (all pre-cleared above) ============
    Currency->Spend(ECurrencyType::Prisms, Cost.Prisms);
    if (Cost.SkillEssence > 0)
    {
        Currency->Spend(ECurrencyType::SkillEssence, Cost.SkillEssence);
    }
    for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
    {
        Currency->SpendEssenceType(Pair.Key, Pair.Value);
    }

    auto RefundAll = [&]()
    {
        Currency->Add(ECurrencyType::Prisms, Cost.Prisms);
        if (Cost.SkillEssence > 0)
        {
            Currency->Add(ECurrencyType::SkillEssence, Cost.SkillEssence);
        }
        for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
        {
            Currency->AddEssenceType(Pair.Key, Pair.Value);
        }
    };

    // ============ GRANT — fully pre-validated, so this cannot fail on valid stock ============
    // Undo thunks unwind LIFO on the (unreachable) defensive failure. Evolutions grant LAST:
    // AddInstance mints an id we don't capture, so keeping them last means a rollback never has
    // to remove one (pre-validation already guarantees every evolution in the cart fits).
    TArray<TFunction<void()>> Undo;
    bool bGrantOk = true;
    FString FailName;

    for (const FMerchantStockEntry &E : Items)
    {
        if (!bGrantOk)
        {
            break;
        }
        if (E.IsCrystalEntry())
        {
            const int32 Count = FMath::Max(1, E.Count);
            const FCrystalId Id = E.Crystal;
            if (Inv->AddCrystal(Id, Count))
            {
                Undo.Add([Inv, Id, Count]() { TArray<FCrystalId> R; R.Init(Id, Count); Inv->RemoveCrystals(R); });
            }
            else
            {
                bGrantOk = false;
                FailName = ItemIdentity::GetDisplayName(Id);
            }
            continue;
        }

        UPrimaryDataAsset *Asset = E.Asset;
        if (UWeaponData *W = Cast<UWeaponData>(Asset))
        {
            const int32 Count = FMath::Max(1, E.Count);
            for (int32 c = 0; c < Count && bGrantOk; ++c)
            {
                if (Inv->AddWeapon(W))
                {
                    Undo.Add([Inv]() { Inv->RemoveWeapon(Inv->GetWeaponCount() - 1); });
                }
                else
                {
                    bGrantOk = false;
                    FailName = W->GetName();
                }
            }
        }
        else if (URingData *R = Cast<URingData>(Asset))
        {
            const int32 Count = FMath::Max(1, E.Count);
            for (int32 c = 0; c < Count && bGrantOk; ++c)
            {
                if (Inv->AddRing(R))
                {
                    Undo.Add([Inv]() { Inv->RemoveRing(Inv->GetRingCount() - 1); });
                }
                else
                {
                    bGrantOk = false;
                    FailName = R->GetName();
                }
            }
        }
        else if (USpellData *S = Cast<USpellData>(Asset))
        {
            if (Inv->LearnSpell(S))
            {
                Undo.Add([Inv, S]() { Inv->UnlearnSpell(S); });
            }
            else
            {
                bGrantOk = false;
                FailName = S->GetName();
            }
        }
        else if (UAbilityData *A = Cast<UAbilityData>(Asset))
        {
            if (Inv->LearnAbility(A))
            {
                Undo.Add([Inv, A]() { Inv->UnlearnAbility(A); });
            }
            else
            {
                bGrantOk = false;
                FailName = A->GetName();
            }
        }
        // Evolutions handled in the trailing pass below (granted last).
    }

    if (bGrantOk)
    {
        for (const FMerchantStockEntry &E : Items)
        {
            if (!bGrantOk)
            {
                break;
            }
            if (E.IsAssetEntry())
            {
                if (UEvolutionItemData *Ev = Cast<UEvolutionItemData>(E.Asset))
                {
                    if (!EvolutionInv->AddInstance(Ev)) // EvolutionInv non-null: guaranteed by the demand check
                    {
                        bGrantOk = false;
                        FailName = Ev->GetName();
                    }
                }
            }
        }
    }

    if (!bGrantOk)
    {
        for (int32 u = Undo.Num() - 1; u >= 0; --u)
        {
            Undo[u]();
        }
        RefundAll();
        UE_LOG(LogTemp, Error,
               TEXT("[EconomyService] Purchase: grant failed for %s AFTER full pre-validation — rolled back %d grants + refunded everything. Indicates a pre-check gap."),
               *FailName, Undo.Num());
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Purchase: granted %d cart entries to %s for %d Prisms (+ essence)."),
           Items.Num(), *Owner->GetName(), Cost.Prisms);
    return true;
}

bool UEconomyService::BuyWorldStat(AActor *Owner, EWorldPillar Pillar)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] BuyWorldStat: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!CharComp || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] BuyWorldStat: %s missing %s%s"),
               *Owner->GetName(),
               CharComp ? TEXT("") : TEXT("CharacterDataComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Cap PRE-CHECK — don't charge for a stat that can't rise (cleaner than spend-then-refund).
    if (CharComp->GetWorldStat(Pillar) >= WorldStatConstants::LIVE_MAX)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] BuyWorldStat: %s pillar already at cap (%d) — rejected"),
               *Owner->GetName(), WorldStatConstants::LIVE_MAX);
        return false;
    }

    // Escalating Gold cost: base + (this-run purchase count * step). The count resets per run via
    // UCharacterDataComponent::ResetRunWorldStats, so the ramp is automatically per-run.
    const int32 Cost = EconomyYield::Constants::WORLDSTAT_BUY_BASE_COST +
                       CharComp->GetWorldStatPurchaseCount() * EconomyYield::Constants::WORLDSTAT_BUY_COST_STEP;
    if (!Currency->CanAfford(ECurrencyType::Gold, Cost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] BuyWorldStat: %s cannot afford %d Gold"),
               *Owner->GetName(), Cost);
        return false;
    }

    // Spend → grant (cap pre-checked, so AddEarnedWorldStat won't fail on cap) → escalate the next buy.
    Currency->Spend(ECurrencyType::Gold, Cost);
    if (!CharComp->AddEarnedWorldStat(Pillar, 1))
    {
        // Defensive (shouldn't happen post pre-check): never take Gold for a failed grant.
        Currency->Add(ECurrencyType::Gold, Cost);
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] BuyWorldStat: grant failed post-spend — refunded %d Gold"), Cost);
        return false;
    }
    CharComp->IncrementPurchaseCount();

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] %s bought +1 world stat for %d Gold (buy #%d this run)"),
           *Owner->GetName(), Cost, CharComp->GetWorldStatPurchaseCount());
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

// ==================== DEBUG ====================

FString UEconomyService::GetEconomyString(AActor *Owner) const
{
    if (!Owner)
    {
        return TEXT("[Economy] Owner=<null>");
    }

    // Read-only resolution of the same components the dismantle / level / downgrade paths require.
    // A missing component is the dominant cause of those calls returning false, so the checklist is
    // the economy-specific diagnostic the wallet line alone does not give.
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    UCrystalInventoryComponent *CrystalInv = Owner->FindComponentByClass<UCrystalInventoryComponent>();
    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UEvolutionInventoryComponent *EvoInv = Owner->FindComponentByClass<UEvolutionInventoryComponent>();
    ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>();

    const TCHAR *Y = TEXT("Y");
    const TCHAR *N = TEXT("N");

    return FString::Printf(
        TEXT("[Economy] Owner=%s | Authority=%s\n")
        TEXT("  Components: Currency=%s CrystalInv=%s Inventory=%s EvolutionInv=%s Loadout=%s\n")
        TEXT("  Wallet: %s"),
        *Owner->GetName(), Owner->HasAuthority() ? Y : N,
        Currency ? Y : N, CrystalInv ? Y : N, Inv ? Y : N, EvoInv ? Y : N, Loadout ? Y : N,
        *UCurrencyComponentDebug::GetWalletString(Currency));
}

void UEconomyService::PrintEconomy(AActor *Owner) const
{
    const FString Snapshot = GetEconomyString(Owner);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Snapshot);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, Snapshot);
    }
}

// FAutoConsoleCommandWithWorld — NOT UFUNCTION(Exec / CallInEditor). A UGameInstanceSubsystem is not
// on the engine's FExec chain and has no Details panel, so neither trigger fires. Console commands
// resolve from any class. "wor." prefix groups them. State is PIE-only, so the chain is null-checked.
namespace
{
    static FAutoConsoleCommandWithWorld GPrintEconomyCmd(
        TEXT("wor.PrintEconomy"),
        TEXT("Snapshot the played character's economy: authority, component checklist, wallet (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                UEconomyService *Economy = GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
                if (!Economy)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.PrintEconomy: no subsystem (no PIE world?)."));
                    return;
                }
                APlayerController *PC = World->GetFirstPlayerController();
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                if (!Pawn)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.PrintEconomy: no played pawn to inspect."));
                    return;
                }
                Economy->PrintEconomy(Pawn);
            }));

    static FAutoConsoleCommandWithWorldAndArgs GBuyWorldStatCmd(
        TEXT("wor.BuyWorldStat"),
        TEXT("DEBUG: buy +1 world stat for the played pawn (arg: Mind|Body|Spirit, default Mind). Tests the escalating Gold cost — run it 3x and watch the cost rise + the stat + count climb."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
            [](const TArray<FString> &Args, UWorld *World)
            {
                EWorldPillar Pillar = EWorldPillar::Mind;
                if (Args.Num() > 0)
                {
                    if (Args[0].Equals(TEXT("Body"), ESearchCase::IgnoreCase))
                    {
                        Pillar = EWorldPillar::Body;
                    }
                    else if (Args[0].Equals(TEXT("Spirit"), ESearchCase::IgnoreCase))
                    {
                        Pillar = EWorldPillar::Spirit;
                    }
                }

                UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                UEconomyService *Economy = GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
                APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                if (!Economy || !Pawn)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.BuyWorldStat: no subsystem or played pawn."));
                    return;
                }
                const bool bOK = Economy->BuyWorldStat(Pawn, Pillar);
                UE_LOG(LogTemp, Display, TEXT("[EconomyService] wor.BuyWorldStat %s -> %s"),
                       *UEnum::GetValueAsString(Pillar), bOK ? TEXT("OK") : TEXT("FAILED (cap / can't afford / missing component)"));
            }));
}
