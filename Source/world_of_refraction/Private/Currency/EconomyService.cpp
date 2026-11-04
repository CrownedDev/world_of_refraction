// EconomyService.cpp

#include "Currency/EconomyService.h"
#include "Equipment/Crystals/FFusionId.h" // FusionRules (B-352)
#include "Currency/EconomyYield.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "WoRAuthority.h"
#include "Character/CharacterDataComponent.h" // BuyWorldStat reach target (§7 C4)
#include "Character/CharacterData.h"          // class + name for the dismantle warning
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
#include "Combat/TurnManager.h"                     // gear dismantle waits until the fight is over
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
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryUnlock.h"
#include "Pool/PoolSubsystem.h" // B-314: home pieces on the run can't be sold or dismantled
#include "Inventory/GearGive.h"  // B-316: buy-for hands the bought gear over
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

namespace
{
    /** B-209 (D-100 part 2): a gear upgrade is an event that checks memory unlock conditions. Item
     *  for a weapon or ring, Evolution for an evolution (Q-209d). */
    void CheckUpgradeMemories(const AActor *Owner, const UEquipmentDataBase *Item, const UEvolutionItemData *Evolution, EItemTier Tier)
    {
        UGameInstance *GameInstance = Owner->GetGameInstance();
        FMemoryEvent Event = MemoryConditions::MakeEvent(GameInstance, EMemorySource::BoundGear);
        Event.UpgradedItem = Item;
        Event.UpgradedEvolution = Evolution;
        Event.UpgradedTier = Tier;
        const UCharacterDataComponent *OwnerData = Owner->FindComponentByClass<UCharacterDataComponent>();
        Event.UpgradedBy = OwnerData ? OwnerData->CharacterData : nullptr;
        MemoryConditions::CheckEvent(GameInstance, Event);
    }

    /** Null + server-authority gate every mutating entry point starts with (WoRAuthority);
     *  logs only the authority refusal, a null owner is refused silently. */
    bool EconomyHasAuthority(AActor *Owner, const TCHAR *Op)
    {
        if (!WoRAuthority::HasAuthority(Owner))
        {
            if (Owner)
            {
                UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: no authority on %s — ignored"), Op, *Owner->GetName());
            }
            return false;
        }
        return true;
    }

    template <class TComponent>
    TComponent *EconomyFindComponent(AActor *Owner)
    {
        return Owner->FindComponentByClass<TComponent>();
    }

    /** D-046 (B-151): currency comes from the player's one wallet, not the character. */
    template <>
    UCurrencyComponent *EconomyFindComponent<UCurrencyComponent>(AActor *Owner)
    {
        return CurrencyWallet::FindFor(Owner);
    }

    /** Finds each required component on Owner (the service couples them; they don't couple
     *  to each other). False, logging the missing ones, when any is absent. */
    template <class... TComponents>
    bool EconomyFindComponents(AActor *Owner, const TCHAR *Op, TComponents *&...OutComponents)
    {
        ((OutComponents = EconomyFindComponent<TComponents>(Owner)), ...);
        if ((OutComponents && ...))
        {
            return true;
        }
        FString Missing;
        ((Missing += OutComponents ? FString() : TComponents::StaticClass()->GetName() + TEXT(" ")), ...);
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: %s missing %s"), Op, *Owner->GetName(), *Missing.TrimEnd());
        return false;
    }

    /** How long the "not during a fight" refusal stays on screen. */
    constexpr float ECONOMY_REFUSAL_MESSAGE_SECONDS = 3.0f;

    /** Players count loadouts from 1; SavedLoadouts from 0. */
    constexpr int32 LOADOUT_DISPLAY_OFFSET = 1;

    /** B-252: the gate every economy action passes: the authority, then out of combat (Q-252). */
    bool EconomyMayAct(AActor *Owner, const TCHAR *Op)
    {
        return EconomyHasAuthority(Owner, Op) && UEconomyService::IsOutOfCombat(Owner, Op);
    }

    /** D-116: a saved loadout's primary can't be dismantled until another primary is swapped in. */
    bool EconomyRefusesPrimary(const UInventoryComponent &Inv, const FGuid &PersistentID, const TCHAR *Op)
    {
        if (!Inv.IsPrimaryInAnyLoadout(PersistentID))
        {
            return false;
        }
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s refused: GUID %s is a loadout's primary - swap in another first (D-116)"),
               Op, *PersistentID.ToString());
        return true;
    }

    /** B-314 (Q-314b) / B-347: while a run is open, only run-only gear can be sold or dismantled. A home piece, on the
     *  run or left home (D-137 takes only the worn loadout), waits for the run's end (it comes back restored, D-111). */
    bool EconomyRefusesOnRun(const AActor *Owner, const FGuid &GearID, const TCHAR *Op)
    {
        const UWorld *World = Owner ? Owner->GetWorld() : nullptr;
        const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
        if (!Pool || !Pool->IsRunLayerActive() || Pool->IsRunOnly(GearID))
        {
            return false;
        }
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s refused: GUID %s is a home piece and a run is under way - it can go once the run ends (Q-314b)"),
               Op, *GearID.ToString());
        return true;
    }

    /** D-016: take the instance out of every saved loadout; tell listeners when the active one changed. */
    void EconomyStripFromLoadouts(AActor *Owner, UInventoryComponent *Inv, const FGuid &PersistentID)
    {
        const bool bActiveHeldIt = Inv->GetLoadoutsUsing(PersistentID).Contains(Inv->ActiveLoadoutIndex);
        const bool bHadPrimary = Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex) &&
                                 UInventoryComponent::LoadoutHasPrimary(Inv->SavedLoadouts[Inv->ActiveLoadoutIndex]);
        Inv->RemoveFromSavedLoadouts(PersistentID);
        const bool bRefilled = !bHadPrimary && Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex) &&
                               UInventoryComponent::LoadoutHasPrimary(Inv->SavedLoadouts[Inv->ActiveLoadoutIndex]);
        if (bActiveHeldIt || bRefilled)
        {
            if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
            {
                Loadout->OnLoadoutChanged.Broadcast(Inv->ActiveLoadoutIndex);
                Loadout->NotifyActiveWeaponChanged(); // the strip edits SavedLoadouts directly (B-138)
            }
        }
    }

    /** B-085: an owned evolution's new tier reaches the loadouts holding it (an owner with no
     *  inventory facade has no loadouts). */
    void EconomyRefreshEvolutionInLoadouts(AActor *Owner, const FGuid &InstanceID)
    {
        if (UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>())
        {
            Inv->RefreshEvolutionInLoadouts(InstanceID);
        }
    }
}


/** Q-252 (Crown): every economy action waits until the fight is over (dismantle, merge, level-up, revert,
 *  buy, repair); gear changes would also break B-081's armed effects mid-fight. The refusal is said on
 *  screen too (placeholder until the real UI, D-024). */
bool UEconomyService::IsOutOfCombat(const AActor *Owner, const TCHAR *Op)
{
    const UWorld *World = Owner ? Owner->GetWorld() : nullptr;
    const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
    const UTurnManager *TurnManager = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
    if (TurnManager && TurnManager->IsCombatActive())
    {
        const FText Refusal = NSLOCTEXT("EconomyService", "RefusedInCombat", "Not during a fight.");
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: refused during combat"), Op);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, ECONOMY_REFUSAL_MESSAGE_SECONDS, FColor::Yellow, Refusal.ToString());
        }
        return false;
    }
    return true;
}

bool UEconomyService::DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count)
{
    if (Count <= 0 || !EconomyMayAct(Owner, TEXT("DismantleCrystal")))
    {
        return false;
    }

    UCrystalInventoryComponent *CrystalInv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    // Route the count mutation through the UInventoryComponent facade so the dismantle
    // emits OnInventoryChanged; CrystalInv is retained for the read-only availability query.
    // (The facade owns the signal; in well-formed actors it coexists with the sibling pools.)
    UInventoryComponent *Inv = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DismantleCrystal"), CrystalInv, Currency, Inv))
    {
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

    // B-333 (D-125): a gem pays Elemental, a stat stone Pillar, an ability stone Skill.
    const ECurrencyType Essence = EconomyYield::GetCrystalEssence(Id.Type);

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
    Currency->Add(Essence, Yield);

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled %dx %s -> %d %s"),
           Removed, *ItemIdentity::GetDisplayName(Id), Yield,
           *StaticEnum<ECurrencyType>()->GetAuthoredNameStringByValue(static_cast<int64>(Essence)));
    return true;
}

bool UEconomyService::MergeCrystals(AActor *Owner, ECrystalType Type, EItemTier TargetTier)
{
    if (!EconomyMayAct(Owner, TEXT("MergeCrystals")))
    {
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

    UCrystalInventoryComponent *CrystalInv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    // Mutations route through the facade (emit OnInventoryChanged); CrystalInv kept for reads/caps.
    UInventoryComponent *Inv = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("MergeCrystals"), CrystalInv, Currency, Inv))
    {
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
    // (minimal-meets-or-exceeds); the overshoot comes back as change below (ER-28).
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

    // ER-28: the overshoot comes back as the fewest same-Type crystals below the target. Added with
    // the output in one call, so change that doesn't fit (a full tier) refunds the whole merge.
    TArray<FCrystalId> Outputs = {OutputId};
    for (const EItemTier ChangeTier : EconomyYield::GetMergeChange(Accumulated - TargetValue, TargetTier))
    {
        Outputs.Add(FCrystalId(Type, ChangeTier));
    }

    // ---- Produce 1 of the target tier (same Type, same pool); refund EVERYTHING on add-failure ----
    const bool bAdded = Inv->AddCrystals(Outputs); // ONE Added
    if (!bAdded)
    {
        Inv->AddCrystals(Inputs); // ONE Added — re-add the exact consumed set (atomic; space just freed)
        Currency->Add(ECurrencyType::Prisms, PrismsCost);
        UE_LOG(LogTemp, Warning,
               TEXT("[EconomyService] MergeCrystals: add of %s + %d change crystal(s) failed (output or change tier at cap, Q-177) — refunded inputs + %d Prisms"),
               *ItemIdentity::GetDisplayName(OutputId), Outputs.Num() - 1, PrismsCost);
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

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Merged %s -> 1x %s (+%d change crystal(s), %d value back) for %d Prisms"),
           *ConsumedStr, *ItemIdentity::GetDisplayName(OutputId), Outputs.Num() - 1, Accumulated - TargetValue, PrismsCost);
    return true;
}

// ==================== OWNED WEAPON / ADAPTER VERBS (B-252b) ====================
// Weapons and rings (adapters) are dismantled, levelled and reverted the same way; only the owned list, the asset
// field and the word in the log differ (OwnedGearOps).

namespace
{
    template <class TEntry>
    struct OwnedGearOps;

    template <>
    struct OwnedGearOps<FWeaponInventoryEntry>
    {
        static constexpr const TCHAR *Noun = TEXT("weapon");
        static FWeaponInventoryEntry *Find(UInventoryComponent &Inv, const FGuid &ID) { return Inv.FindOwnedWeapon(ID); }
        static bool Remove(UInventoryComponent &Inv, const FGuid &ID) { return Inv.RemoveWeaponByPersistentID(ID); }
        static const UWeaponData *Asset(const FWeaponInventoryEntry &Entry) { return Entry.Weapon; }
    };

    template <>
    struct OwnedGearOps<FRingInventoryEntry>
    {
        static constexpr const TCHAR *Noun = TEXT("ring");
        static FRingInventoryEntry *Find(UInventoryComponent &Inv, const FGuid &ID) { return Inv.FindOwnedRing(ID); }
        static bool Remove(UInventoryComponent &Inv, const FGuid &ID) { return Inv.RemoveRingByPersistentID(ID); }
        static const URingData *Asset(const FRingInventoryEntry &Entry) { return Entry.Ring; }
    };

    /** The owned instance with this GUID and an asset, or null with a warning naming the verb. */
    template <class TEntry>
    TEntry *FindOwnedGearForVerb(AActor *Owner, UInventoryComponent &Inv, const FGuid &PersistentID, const TCHAR *Verb)
    {
        TEntry *Entry = OwnedGearOps<TEntry>::Find(Inv, PersistentID);
        if (!Entry || !OwnedGearOps<TEntry>::Asset(*Entry))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: %s has no %s instance for GUID %s (or null asset)"), Verb,
                   *Owner->GetName(), OwnedGearOps<TEntry>::Noun, *PersistentID.ToString());
            return nullptr;
        }
        return Entry;
    }
}

template <class TEntry>
bool UEconomyService::DismantleOwnedGear(AActor *Owner, FGuid PersistentID, const TCHAR *Verb)
{
    if (!EconomyMayAct(Owner, Verb))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, Verb, Inv, Currency))
    {
        return false;
    }

    // Find the owned instance and capture its tier BEFORE removal - the entry pointer dangles once
    // the array is mutated, so nothing past the remove dereferences it. Reads the INSTANCE Tier
    // (cluster 2b read-flip): a leveled piece dismantles for its CURRENT tier, not its base.
    const TEntry *Entry = FindOwnedGearForVerb<TEntry>(Owner, *Inv, PersistentID, Verb);
    if (!Entry)
    {
        return false;
    }
    if (EconomyRefusesPrimary(*Inv, PersistentID, Verb) || EconomyRefusesOnRun(Owner, PersistentID, Verb))
    {
        return false;
    }
    const EItemTier Tier = Entry->Tier;
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // D-016: no ghost copy left in a saved loadout. Doesn't touch the owned list, so Entry stays valid.
    EconomyStripFromLoadouts(Owner, Inv, PersistentID);

    // REMOVE FIRST - a failed removal must never grant phantom essence.
    if (!OwnedGearOps<TEntry>::Remove(*Inv, PersistentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: removal failed for GUID %s - no grant"), Verb, *PersistentID.ToString());
        return false;
    }

    Currency->AddGearEssence(Yield); // weapons/rings -> Gear essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled %s (GUID %s, tier %d) -> %d Gear essence"), OwnedGearOps<TEntry>::Noun,
           *PersistentID.ToString(), static_cast<int32>(Tier), Yield);
    return true;
}

template <class TEntry>
bool UEconomyService::LevelUpOwnedGear(AActor *Owner, FGuid PersistentID, const TCHAR *Verb)
{
    if (!EconomyMayAct(Owner, Verb))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, Verb, Inv, Currency))
    {
        return false;
    }

    // MUTABLE entry - the core writes Entry.Tier in place. Nothing mutates the owned list here, so
    // the pointer stays valid across the currency spend inside TryLevelUpEntry.
    TEntry *Entry = FindOwnedGearForVerb<TEntry>(Owner, *Inv, PersistentID, Verb);
    if (!Entry || !TryLevelUpEntry(Currency, Entry->Tier))
    {
        return false;
    }
    Inv->RefreshLoadoutsFromOwned(PersistentID); // B-085: the loadouts show the new tier
    // B-209 (D-100): a bound piece's memories unlock bit by bit as it is levelled.
    MemoryUnlock::UnlockBoundOnLevelUp(Owner->GetGameInstance(), OwnedGearOps<TEntry>::Asset(*Entry), Entry->Tier);
    CheckUpgradeMemories(Owner, OwnedGearOps<TEntry>::Asset(*Entry), nullptr, Entry->Tier);
    return true;
}

template <class TEntry>
bool UEconomyService::DowngradeOwnedGear(AActor *Owner, FGuid PersistentID, const TCHAR *Verb)
{
    if (!EconomyMayAct(Owner, Verb))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, Verb, Inv, Currency))
    {
        return false;
    }

    // Floor = the asset's authored Tier (leveling only mutates the instance .Tier).
    TEntry *Entry = FindOwnedGearForVerb<TEntry>(Owner, *Inv, PersistentID, Verb);
    if (!Entry || !TryDowngradeEntry(Currency, Entry->Tier, OwnedGearOps<TEntry>::Asset(*Entry)->Tier, ECurrencyType::GearEssence))
    {
        return false;
    }
    Inv->RefreshLoadoutsFromOwned(PersistentID); // B-085
    return true;
}

bool UEconomyService::DismantleWeapon(AActor *Owner, FGuid PersistentID)
{
    return DismantleOwnedGear<FWeaponInventoryEntry>(Owner, PersistentID, TEXT("DismantleWeapon"));
}

bool UEconomyService::DismantleRing(AActor *Owner, FGuid PersistentID)
{
    return DismantleOwnedGear<FRingInventoryEntry>(Owner, PersistentID, TEXT("DismantleRing"));
}

namespace
{
    /** Q-319: a duplicate converts for half its tier's buy price (the merge-cost rule's half). */
    constexpr int32 DUPLICATE_GOLD_DIVISOR = 2;

    /** B-319: the weaker copy out of the loadouts and the owned list, then Gold. */
    template <class TEntry>
    bool ConvertOwnedGearToGold(AActor *Owner, UInventoryComponent &Inv, UCurrencyComponent &Currency, const FGuid &PersistentID)
    {
        const TCHAR *Verb = TEXT("ResolveDuplicate");
        const TEntry *Entry = FindOwnedGearForVerb<TEntry>(Owner, Inv, PersistentID, Verb);
        if (!Entry || EconomyRefusesPrimary(Inv, PersistentID, Verb) || EconomyRefusesOnRun(Owner, PersistentID, Verb))
        {
            return false;
        }
        const int32 Gold = UEconomyService::GetDuplicateGoldForTier(Entry->Tier);
        EconomyStripFromLoadouts(Owner, &Inv, PersistentID);
        if (!OwnedGearOps<TEntry>::Remove(Inv, PersistentID))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: removal failed for GUID %s - no Gold"), Verb, *PersistentID.ToString());
            return false;
        }
        Currency.Add(ECurrencyType::Gold, Gold);
        UE_LOG(LogTemp, Log, TEXT("[EconomyService] Duplicate %s (GUID %s) -> %d Gold"), OwnedGearOps<TEntry>::Noun, *PersistentID.ToString(), Gold);
        return true;
    }
}

int32 UEconomyService::GetDuplicateGoldForTier(EItemTier Tier)
{
    return EconomyYield::GetPrismsBaseForTier(Tier) / DUPLICATE_GOLD_DIVISOR;
}

bool UEconomyService::ResolveDuplicate(AActor *Owner, FGuid WeakerID, bool bConvertToGold)
{
    const TCHAR *Verb = TEXT("ResolveDuplicate");
    if (!EconomyMayAct(Owner, Verb))
    {
        return false;
    }
    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, Verb, Inv, Currency))
    {
        return false;
    }
    const TArray<FDuplicateGearPair> Pairs = Inv->FindDuplicateGear();
    const FDuplicateGearPair *Pair = Pairs.FindByPredicate([&WeakerID](const FDuplicateGearPair &Candidate) { return Candidate.WeakerID == WeakerID; });
    if (!Pair)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] %s: GUID %s is no weaker duplicate"), Verb, *WeakerID.ToString());
        return false;
    }
    const FDuplicateGearPair Chosen = *Pair;
    // The kept copy takes the weaker one's place, so a primary is never left empty (D-116). The move
    // stands even if the removal below fails: the kept copy is a valid piece.
    const bool bActiveHeldWeaker = Inv->GetLoadoutsUsing(Chosen.WeakerID).Contains(Inv->ActiveLoadoutIndex);
    if (Inv->MoveLoadoutSlots(Chosen.WeakerID, Chosen.KeptID) > 0 && bActiveHeldWeaker)
    {
        if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
        {
            Loadout->OnLoadoutChanged.Broadcast(Inv->ActiveLoadoutIndex);
            Loadout->NotifyActiveWeaponChanged(); // the move edits SavedLoadouts directly (B-138)
        }
    }
    if (bConvertToGold)
    {
        return Chosen.bRing ? ConvertOwnedGearToGold<FRingInventoryEntry>(Owner, *Inv, *Currency, Chosen.WeakerID)
                            : ConvertOwnedGearToGold<FWeaponInventoryEntry>(Owner, *Inv, *Currency, Chosen.WeakerID);
    }
    return Chosen.bRing ? DismantleRing(Owner, Chosen.WeakerID) : DismantleWeapon(Owner, Chosen.WeakerID);
}

#define LOCTEXT_NAMESPACE "EconomyService"

FText UEconomyService::GetDismantleWarning(const AActor *Owner, FGuid PersistentID)
{
    const UInventoryComponent *Inv = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
    const TArray<int32> Using = Inv ? Inv->GetLoadoutsUsing(PersistentID) : TArray<int32>();
    if (Using.IsEmpty())
    {
        return FText::GetEmpty();
    }
    if (Inv->IsPrimaryInAnyLoadout(PersistentID))
    {
        return LOCTEXT("DismantlePrimary", "This is a loadout's primary. Swap in another primary before dismantling it.");
    }

    TArray<FText> LoadoutNumbers;
    for (const int32 Index : Using)
    {
        LoadoutNumbers.Add(FText::AsNumber(Index + LOADOUT_DISPLAY_OFFSET));
    }
    TArray<FText> Lines;
    Lines.Add(FText::Format(LOCTEXT("DismantleEquipped", "This is equipped in {0}|plural(one=loadout,other=loadouts) {1}. Dismantle anyway?"),
                            Using.Num(), FText::Join(LOCTEXT("LoadoutListSeparator", ", "), LoadoutNumbers)));
    Lines.Add(LOCTEXT("DismantleLosesUpgrades", "You'll lose its upgrades."));

    // The fight starts from the active loadout; a Caster fights on innate spells without gear.
    const UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>();
    const UCharacterData *Data = CharComp ? CharComp->CharacterData : nullptr;
    if (Data && Data->CharacterClass != ECharacterClass::Caster && Using.Contains(Inv->ActiveLoadoutIndex))
    {
        FCombatLoadout After = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        UInventoryComponent::StripFromLoadout(After, PersistentID);
        if (!UInventoryComponent::LoadoutHasGear(After))
        {
            Lines.Add(FText::Format(LOCTEXT("DismantleLeavesGearless", "{0} will have no gear and can't fight until you equip something."),
                                    FText::FromString(Data->Name)));
        }
    }
    return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#undef LOCTEXT_NAMESPACE

bool UEconomyService::DismantleEvolution(AActor *Owner, FGuid InstanceID)
{
    if (!EconomyMayAct(Owner, TEXT("DismantleEvolution")))
    {
        return false;
    }

    UEvolutionInventoryComponent *EvoInv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    // Removal routes through the facade (emit OnInventoryChanged); EvoInv kept for the read.
    UInventoryComponent *Inv = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DismantleEvolution"), EvoInv, Currency, Inv))
    {
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
    if (EconomyRefusesPrimary(*Inv, InstanceID, TEXT("DismantleEvolution")) || EconomyRefusesOnRun(Owner, InstanceID, TEXT("DismantleEvolution")))
    {
        return false;
    }
    const EItemTier Tier = Entry->Tier; // INSTANCE tier (leveled), not Entry->Item->Tier (asset/base)
    // HYBRID yield at the GEAR leveling AMOUNT (§3 gear curve, NOT the §4.2 crystal yield): Elemental
    // Essence for an evolution with an element (B-333, Q-333), Gear Essence for an element-agnostic one.
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);
    const ECurrencyType Essence = EconomyYield::HasElement(Entry->Item->GetAssociatedElement())
                                      ? ECurrencyType::ElementalEssence
                                      : ECurrencyType::GearEssence;

    // REMOVE FIRST — a failed removal must never grant phantom essence.
    if (!Inv->RemoveEvolutionInstance(InstanceID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleEvolution: removal failed for GUID %s — no grant"),
               *InstanceID.ToString());
        return false;
    }

    Currency->Add(Essence, Yield); // hybrid: Elemental (or Gear), gear amount
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled evolution (GUID %s, tier %d) -> %d %s"),
           *InstanceID.ToString(), static_cast<int32>(Tier), Yield,
           *StaticEnum<ECurrencyType>()->GetAuthoredNameStringByValue(static_cast<int64>(Essence)));
    return true;
}

bool UEconomyService::RemovePrimaryEvolution(AActor *Owner)
{
    if (!EconomyMayAct(Owner, TEXT("RemovePrimaryEvolution")))
    {
        return false;
    }

    ULoadoutComponent *LC = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("RemovePrimaryEvolution"), LC))
    {
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

    // D-116: this always empties the primary, so it waits for another primary to be swapped in.
    // B-314: refuse a home piece on the run here, before the slot is cleared, so the refusal is whole.
    const UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    if ((Inv && EconomyRefusesPrimary(*Inv, InstanceID, TEXT("RemovePrimaryEvolution"))) ||
        EconomyRefusesOnRun(Owner, InstanceID, TEXT("RemovePrimaryEvolution")))
    {
        return false;
    }

    // 2. Clear the primary-slot reference so nothing points at the entry we're about to destroy.
    LC->ClearPrimaryEvolution();

    // 3. Destroy the owned entry + yield the dust + free a cap slot. Authoritative result.
    return DismantleEvolution(Owner, InstanceID);
}

bool UEconomyService::DismantleSpell(AActor *Owner, USpellData *Spell)
{
    if (!Spell || !EconomyMayAct(Owner, TEXT("DismantleSpell")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DismantleSpell"), Inv, Currency))
    {
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
    if (!Ability || !EconomyMayAct(Owner, TEXT("DismantleAbility")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DismantleAbility"), Inv, Currency))
    {
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

namespace
{
    /** Prisms surcharge for whatever is authored in the equipment's attachment slot
     *  (3j): crystal / augment stone → its tier base; evolution → 2× the evolution's
     *  tier base (premium attachment); fusion → 1.5× the summed half bases. */
    int32 AttachmentPrisms(const UEquipmentDataBase *Eq)
    {
        if (!Eq->HasCrystal())
        {
            return 0;
        }
        const FAttachedItem &Attached = Eq->AttachedItem;
        switch (Attached.Kind)
        {
        case EAttachedItemKind::Crystal:
        case EAttachedItemKind::AugmentStone:
            return EconomyYield::GetPrismsBaseForTier(Attached.CrystalTier);
        case EAttachedItemKind::Evolution:
            // Null-guarded: a mis-authored Evolution slot with no asset prices at 0
            // (IsDataValid flags it upstream).
            return Attached.Evolution ? 2 * EconomyYield::GetPrismsBaseForTier(Attached.Evolution->Tier) : 0;
        case EAttachedItemKind::Fusion:
            if constexpr (FusionRules::RETIRED)
            {
                return 0; // B-352 (D-141): an authored fusion no longer attaches, so it adds nothing to the price
            }
            else
            {
                // OBSOLETE (B-352, D-141; delete in the D-124 sweep): fusion pricing.
                return FMath::RoundToInt(1.5f * (EconomyYield::GetPrismsBaseForTier(Attached.FusionHalfATier) +
                                                 EconomyYield::GetPrismsBaseForTier(Attached.FusionHalfBTier)));
            }
        default:
            return 0;
        }
    }

    /** Prisms surcharge for skills bundled on equipment (3l): each non-null skill
     *  prices at a third of its tier base + 10 flat (integer floor per the project
     *  rounding rule). Prisms-only — bundled skills never charge essence. Template
     *  because DefaultSpells / PresetAbilities are arrays of different derived
     *  pointers; Tier reads through the shared USkillDataBase. */
    template <typename TSkill>
    int32 BundledSkillsPrisms(const TArray<TSkill *> &Skills)
    {
        int32 Prisms = 0;
        for (const TSkill *Skill : Skills)
        {
            if (Skill)
            {
                Prisms += EconomyYield::GetPrismsBaseForTier(Skill->Tier) / 3 + 10;
            }
        }
        return Prisms;
    }

    /** True when the attachment can grant augment-stone abilities: any augment
     *  stone, or a fusion carrying an AbilityStone half (half A is stone-only by
     *  authoring convention; half B only when authored as a stone). Pricing-side
     *  mirror of FWeaponLoadoutEntry::GetAugmentStoneAbilities, but tighter — the
     *  runtime gate passes any fusion and lets the slot cap resolve to 0, while
     *  pricing must not charge for abilities the attachment cannot grant. */
    bool HasAbilityStoneAttachment(const UEquipmentDataBase *Eq)
    {
        const FAttachedItem &Attached = Eq->AttachedItem;
        switch (Attached.Kind)
        {
        case EAttachedItemKind::AugmentStone:
            return true;
        case EAttachedItemKind::Fusion:
            if constexpr (FusionRules::RETIRED)
            {
                return false; // B-352 (D-141): no fusion attaches
            }
            else
            {
                return Attached.FusionHalfAType == ECrystalType::AbilityStone || // OBSOLETE (B-352)
                       (!Attached.bFusionHalfBIsCrystal && Attached.FusionHalfBType == ECrystalType::AbilityStone);
            }
        default:
            return false;
        }
    }
}

FPurchaseCost UEconomyService::BuildCartCost(const TArray<FMerchantStockEntry> &Items) const
{
    FPurchaseCost Cost;
    for (const FMerchantStockEntry &E : Items)
    {
        if (E.IsCrystalEntry())
        {
            // B-333 (D-125, Q-333): Prisms + its own essence at tier, per unit — a gem (Quartz too)
            // Elemental, a stat stone Pillar, an ability stone Skill.
            const int32 Units = FMath::Max(1, E.Count);
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(E.Crystal.Tier) * Units;
            Cost.AddEssence(EconomyYield::GetCrystalEssence(E.Crystal.Type),
                            EconomyYield::GetTypedEssencePurchaseCostForTier(E.Crystal.Tier) * Units);
            continue;
        }
        if (!E.IsAssetEntry())
        {
            continue; // empty entry prices at 0 — Purchase rejects it separately
        }

        UPrimaryDataAsset *Asset = E.Asset;
        if (UWeaponData *W = Cast<UWeaponData>(Asset))
        {
            // Equipment pricing: Prisms base by tier + attachment surcharge + bundled
            // skills (3l), all per-unit (quality = C placeholder until shop-roll).
            // PresetAbilities always price — they're baked into what you're buying.
            // Gem-side DefaultSpells / stone-side DefaultAbilities price only when the
            // attachment actually grants them. WeaponAttack is the weapon's identity,
            // never priced.
            int32 PerUnit = EconomyYield::GetPrismsBaseForTier(W->Tier) + AttachmentPrisms(W) +
                            BundledSkillsPrisms(W->PresetAbilities);
            if (W->AttachedItem.Kind == EAttachedItemKind::Crystal)
            {
                PerUnit += BundledSkillsPrisms(W->DefaultSpells);
            }
            if (HasAbilityStoneAttachment(W))
            {
                PerUnit += BundledSkillsPrisms(W->DefaultAbilities);
            }
            Cost.Prisms += PerUnit * FMath::Max(1, E.Count);
        }
        else if (URingData *R = Cast<URingData>(Asset))
        {
            // Gem-attached rings price their bundled DefaultSpells (3l — the themed
            // rings); stone/evolution/fusion attachments don't gem-seed spells.
            int32 PerUnit = EconomyYield::GetPrismsBaseForTier(R->Tier) + AttachmentPrisms(R);
            if (R->AttachedItem.Kind == EAttachedItemKind::Crystal)
            {
                PerUnit += BundledSkillsPrisms(R->DefaultSpells);
            }
            Cost.Prisms += PerUnit * FMath::Max(1, E.Count);
        }
        else if (USpellData *S = Cast<USpellData>(Asset))
        {
            // Count clamps to 1 for all spells (a spell can't be owned twice).
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(S->Tier);
            // B-333 (D-125): a spell costs Skill + Elemental at its tier; a None (non-elemental) spell
            // Skill only (Q-333). Generic counts as elemental (Quartz folded into Elemental, changes D-105).
            // The +50 Prisms per scaling grade stays; the pillar essence per grade retired.
            const int32 EssenceAtTier = EconomyYield::GetTypedEssencePurchaseCostForTier(S->Tier);
            Cost.SkillEssence += EssenceAtTier;
            if (EconomyYield::SpellHasElement(S->Element))
            {
                Cost.ElementalEssence += EssenceAtTier;
            }
            for (const FStatScaling &Sc : S->StatScaling)
            {
                if (Sc.Stat == ESubStat::None)
                {
                    continue;
                }
                Cost.Prisms += EconomyYield::Constants::PRISMS_SCALING_SURCHARGE_PER_GRADE *
                               EconomyYield::GetScalingGradeNumber(Sc.Tier);
            }
        }
        else if (UAbilityData *A = Cast<UAbilityData>(Asset))
        {
            Cost.Prisms += EconomyYield::GetPrismsBaseForTier(A->Tier);
            Cost.SkillEssence += EconomyYield::GetTypedEssencePurchaseCostForTier(A->Tier);
        }
        else if (UEvolutionItemData *Ev = Cast<UEvolutionItemData>(Asset))
        {
            // 2× base — evolutions are the premium item class (3j). The multiplier sits
            // HERE, not in a shared helper, so a future merge-cost reuse of the tier
            // base can't accidentally double it.
            Cost.Prisms += 2 * EconomyYield::GetPrismsBaseForTier(Ev->Tier);
            // B-333 (Q-333): an evolution with an element adds Elemental at tier + the ½ co-cost (was
            // ½ Reality), mirroring leveling; an element-agnostic one is Prisms only.
            if (EconomyYield::HasElement(Ev->GetAssociatedElement()))
            {
                const int32 ElementAmount = EconomyYield::GetTypedEssencePurchaseCostForTier(Ev->Tier);
                Cost.ElementalEssence += ElementAmount + EconomyYield::GetElementalCoCost(ElementAmount);
            }
        }
        // Unknown asset types price at 0 — Purchase rejects them separately.
    }
    return Cost;
}

namespace
{
    /** B-316: the local party member with Key, or null. */
    const FPartyMember *BuyForMemberOf(const AActor *Payer, FName Key)
    {
        const UWorld *World = Payer ? Payer->GetWorld() : nullptr;
        const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
        return Party ? Party->Members.FindByPredicate([Key](const FPartyMember &Member)
                                                      { return Member.CharacterData && UPoolSubsystem::GetCharacterKey(Member.CharacterData) == Key; })
                     : nullptr;
    }

    /** B-316: the receiver's armoury and pouch can take the whole cart (the purchase's own checks read the payer). Logs
     *  the first shortfall. */
    bool BuyForReceiverHasRoom(const UPoolSubsystem &Pool, FName ForKey, const TArray<FMerchantStockEntry> &Items)
    {
        int32 WeaponCost = 0;
        int32 AdapterCost = 0;
        TMap<FCrystalId, int32> ItemDemand;
        for (const FMerchantStockEntry &Entry : Items)
        {
            const int32 Count = FMath::Max(1, Entry.Count);
            if (Entry.IsCrystalEntry())
            {
                ItemDemand.FindOrAdd(Entry.Crystal) += Count;
            }
            else if (Cast<UWeaponData>(Entry.Asset.Get()))
            {
                WeaponCost += Count * InventoryConstants::WEAPON_BASE_SLOT_COST;
            }
            else if (Cast<URingData>(Entry.Asset.Get()))
            {
                AdapterCost += Count * InventoryConstants::RING_BASE_SLOT_COST;
            }
        }
        const FPoolGearView Owned = Pool.GetOwnedGearOf(ForKey);
        for (const FWeaponInventoryEntry *Weapon : Owned.Weapons)
        {
            WeaponCost += Weapon->GetSlotCost();
        }
        for (const FRingInventoryEntry *Adapter : Owned.Adapters)
        {
            AdapterCost += Adapter->GetSlotCost();
        }
        if (WeaponCost > InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS || AdapterCost > InventoryConstants::MAX_RING_INVENTORY_SLOTS)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s has no room for the gear - refused, nothing spent"), *ForKey.ToString());
            return false;
        }
        // One item type per slot (D-097): each new type needs a free slot, and each type's stack its room.
        int32 NewSlots = 0;
        for (const TPair<FCrystalId, int32> &Demand : ItemDemand)
        {
            NewSlots += Pool.GetCarriedCount(ForKey, Demand.Key) > 0 ? 0 : 1;
            if (Pool.GetCarryRoom(ForKey, Demand.Key) < Demand.Value)
            {
                UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s's pouch can't take %d %s - refused, nothing spent"),
                       *ForKey.ToString(), Demand.Value, *ItemIdentity::GetDisplayName(Demand.Key));
                return false;
            }
        }
        if (Pool.GetUsedItemSlots(ForKey) + NewSlots > Pool.GetItemSlotCount(ForKey))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s's pouch has too few free slots - refused, nothing spent"), *ForKey.ToString());
            return false;
        }
        return true;
    }

    /** B-316: every weapon and adapter ID in the store. */
    TSet<FGuid> BuyForGearIDs(const UPoolSubsystem &Pool)
    {
        TSet<FGuid> IDs;
        for (const FWeaponInventoryEntry &Entry : Pool.GetOwnedWeapons())
        {
            IDs.Add(Entry.PersistentID);
        }
        for (const FRingInventoryEntry &Entry : Pool.GetOwnedRings())
        {
            IDs.Add(Entry.PersistentID);
        }
        return IDs;
    }
}

bool UEconomyService::PurchaseFor(AActor *Payer, FName ForKey, const TArray<FMerchantStockEntry> &Items)
{
    const UCharacterDataComponent *PayerChar = Payer ? Payer->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    const FName PayerKey = UPoolSubsystem::GetCharacterKey(PayerChar ? PayerChar->CharacterData : nullptr);
    if (ForKey.IsNone() || ForKey == PayerKey)
    {
        return Purchase(Payer, Items);
    }
    UWorld *World = Payer ? Payer->GetWorld() : nullptr;
    const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
    UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
    const FPartyMember *Receiver = BuyForMemberOf(Payer, ForKey);
    if (PayerKey.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s has no character (no UCharacterDataComponent), so it can't hand goods to another member - refused"),
               *GetNameSafe(Payer));
        return false;
    }
    if (!Pool || !Receiver)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s is not a party member the payer can buy for - refused"), *ForKey.ToString());
        return false;
    }
    for (const FMerchantStockEntry &Entry : Items)
    {
        const UPrimaryDataAsset *Asset = Entry.IsAssetEntry() ? Entry.Asset.Get() : nullptr;
        if (Cast<USpellData>(Asset) || Cast<UAbilityData>(Asset) || Cast<UEvolutionItemData>(Asset))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s can't be bought for another character yet (B-316b) - refused"),
                   *GetNameSafe(Asset));
            return false;
        }
        if (Receiver->bRunOnly && (Cast<UWeaponData>(Asset) || Cast<URingData>(Asset)))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s is only here for the run and can't own gear (Q-321) - refused"),
                   *ForKey.ToString());
            return false;
        }
    }

    if (!BuyForReceiverHasRoom(*Pool, ForKey, Items))
    {
        return false;
    }

    const TSet<FGuid> GearBefore = BuyForGearIDs(*Pool);
    const TMap<FCrystalId, int32> CarriedBefore = Pool->GetCarriedItems(PayerKey);
    // The goods pass through the payer: its own copy of one is no duplicate for it to resolve (the receiver's is).
    UInventoryComponent *PayerInv = Payer->FindComponentByClass<UInventoryComponent>();
    const bool bWasQuiet = PayerInv && PayerInv->bSuppressDuplicateNotice;
    if (PayerInv)
    {
        PayerInv->bSuppressDuplicateNotice = true;
    }
    const bool bBought = Purchase(Payer, Items);
    if (PayerInv)
    {
        PayerInv->bSuppressDuplicateNotice = bWasQuiet;
    }
    if (!bBought)
    {
        return false;
    }

    // The new pieces are the payer's (PushGearToPool): each becomes the receiver's.
    int32 GearMoved = 0;
    for (const FGuid &ID : BuyForGearIDs(*Pool).Difference(GearBefore))
    {
        const FText Refusal = WoRGive::TryGiveGear(World, Pool->GetGearOwner(ID), ForKey, ID);
        if (Refusal.IsEmpty())
        {
            ++GearMoved;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %s stays with the payer: %s"), *ID.ToString(), *Refusal.ToString());
        }
    }
    // Crystals and stones the purchase put in the payer's pouch move to the receiver's, as many as fit.
    int32 ItemsMoved = 0;
    for (const TPair<FCrystalId, int32> &Stack : TMap<FCrystalId, int32>(Pool->GetCarriedItems(PayerKey)))
    {
        const int32 *Before = CarriedBefore.Find(Stack.Key);
        const int32 Bought = Stack.Value - (Before ? *Before : 0);
        const int32 Moving = FMath::Min(Bought, Pool->GetCarryRoom(ForKey, Stack.Key));
        if (Moving > 0 && Pool->GiveCarried(PayerKey, ForKey, Stack.Key, Moving))
        {
            ItemsMoved += Moving;
        }
        if (Bought > Moving)
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseFor: %d %s didn't fit %s's pouch (or went to storage) - left with the payer"),
                   Bought - Moving, *ItemIdentity::GetDisplayName(Stack.Key), *ForKey.ToString());
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] %s bought for %s: %d piece(s) of gear and %d item(s) handed over"), *PayerKey.ToString(),
           *ForKey.ToString(), GearMoved, ItemsMoved);
    return true;
}

FPurchaseCost UEconomyService::PreviewCartCost(const TArray<FMerchantStockEntry> &Items) const
{
    return BuildCartCost(Items);
}

bool UEconomyService::Purchase(AActor *Owner, const TArray<FMerchantStockEntry> &Items)
{
    if (Items.Num() == 0 || !EconomyMayAct(Owner, TEXT("Purchase")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("Purchase"), Inv, Currency))
    {
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
    // B-333: Prisms always (a zero-Prisms cart is refused, as before); each essence only when charged.
    for (const ECurrencyType Balance : PurchaseCostBalances::CHARGED)
    {
        const int32 Amount = Cost.Get(Balance);
        if ((Balance == ECurrencyType::Prisms || Amount > 0) && !Currency->CanAfford(Balance, Amount))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] Purchase: %s cannot afford %d %s — aborting."), *Owner->GetName(),
                   Amount, *StaticEnum<ECurrencyType>()->GetAuthoredNameStringByValue(static_cast<int64>(Balance)));
            return false;
        }
    }

    // ============ SPEND — every currency once (all pre-cleared above) ============
    for (const ECurrencyType Balance : PurchaseCostBalances::CHARGED)
    {
        Currency->Spend(Balance, Cost.Get(Balance)); // a zero amount is a no-op
    }

    auto RefundAll = [&]()
    {
        for (const ECurrencyType Balance : PurchaseCostBalances::CHARGED)
        {
            Currency->Add(Balance, Cost.Get(Balance)); // a zero amount is a no-op
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
                Undo.Add([Inv, Id, Count]()
                {
                    TArray<FCrystalId> R;
                    R.Init(Id, Count);
                    if (!Inv->RemoveCrystals(R))
                    {
                        // B-203: part went to the party's storage (carried slots full); it stays there.
                        UE_LOG(LogTemp, Warning, TEXT("[Economy] Purchase undo: %s x%d not all carried, left where it landed"),
                               *ItemIdentity::GetDisplayName(Id), Count);
                    }
                });
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
                    // By instance, not last index: a duplicate listener may have reordered the list (B-319).
                    const FGuid AddedID = Inv->Weapons.Last().PersistentID;
                    Undo.Add([Inv, AddedID]() { Inv->RemoveWeaponByPersistentID(AddedID); });
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
                    const FGuid AddedID = Inv->Rings.Last().PersistentID;
                    Undo.Add([Inv, AddedID]() { Inv->RemoveRingByPersistentID(AddedID); });
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
                    // Through the inventory, so a party member's purchase reaches the shared armoury (B-091).
                    if (!Inv->AddEvolutionInstance(Ev)) // EvolutionInv non-null: guaranteed by the demand check
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
    if (!EconomyMayAct(Owner, TEXT("BuyWorldStat")))
    {
        return false;
    }

    UCharacterDataComponent *CharComp = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("BuyWorldStat"), CharComp, Currency))
    {
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
                                      ECurrencyType LevelingEssence, bool bHasElement) const
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

    // Cost (§5.3): full leveling essence to reach the next tier. No Gold. LevelingEssence is the §3
    // category currency — Gear (weapons/rings/evolution) or Skill (spells/abilities). B-333 (Q-333):
    // an item with an element also pays HALF that in Elemental (was ½ Reality for everything).
    const int32 LevelingCost = EconomyYield::GetTierUpCostForTier(CurrentTier);
    const int32 ElementalCost = bHasElement ? EconomyYield::GetElementalCoCost(LevelingCost) : 0;

    // CanAfford BOTH before spending anything — if either is short, spend nothing and bail.
    if (!Currency->CanAfford(LevelingEssence, LevelingCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] TryLevelUpEntry: cannot afford %d leveling essence (from %s)"),
               LevelingCost, *TierHelpers::GetTierName(CurrentTier));
        return false;
    }
    if (ElementalCost > 0 && !Currency->CanAfford(ECurrencyType::ElementalEssence, ElementalCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] TryLevelUpEntry: cannot afford %d Elemental essence (from %s)"),
               ElementalCost, *TierHelpers::GetTierName(CurrentTier));
        return false;
    }

    // Spend BOTH (the afford-checks above already cleared each component; a zero co-cost is a no-op).
    Currency->Spend(LevelingEssence, LevelingCost);
    Currency->Spend(ECurrencyType::ElementalEssence, ElementalCost);

    // WRITE the instance tier one step UP. EItemTier is forward-ordered (F_Tier=0 .. S_Tier=6),
    // so the next-stronger tier is value+1; GetTierFromValue clamps 0..6 so this never overruns S
    // (and the S-cap above already guarantees CurrentTier < S here). This in-place write through
    // the caller's tier-ref cannot fail, so no post-spend refund path is needed.
    const EItemTier NextTier = TierHelpers::GetTierFromValue(TierHelpers::GetTierValue(CurrentTier) + 1);
    InOutTier = NextTier;

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Tier-up: %s -> %s (spent %d leveling + %d Elemental essence)"),
           *TierHelpers::GetTierName(CurrentTier), *TierHelpers::GetTierName(NextTier), LevelingCost, ElementalCost);
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
    // essence ONLY — the ½-Elemental co-cost paid at level-up is gone (reverting costs the reshape
    // currency). Write the lowered tier in place; neither the write nor the Add can fail.
    const EItemTier DownTier = TierHelpers::GetTierFromValue(TierHelpers::GetTierValue(CurrentTier) - 1);
    const int32 StepCost = EconomyYield::GetTierUpCostForTier(DownTier);
    const int32 Refund = StepCost / 2;

    InOutTier = DownTier;
    Currency->Add(LevelingEssence, Refund);

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Tier-down: %s -> %s (refunded %d leveling essence; Elemental co-cost not refunded)"),
           *TierHelpers::GetTierName(CurrentTier), *TierHelpers::GetTierName(DownTier), Refund);
    return true;
}

bool UEconomyService::LevelUpWeapon(AActor *Owner, FGuid PersistentID)
{
    return LevelUpOwnedGear<FWeaponInventoryEntry>(Owner, PersistentID, TEXT("LevelUpWeapon"));
}

bool UEconomyService::LevelUpRing(AActor *Owner, FGuid PersistentID)
{
    return LevelUpOwnedGear<FRingInventoryEntry>(Owner, PersistentID, TEXT("LevelUpRing"));
}

bool UEconomyService::LevelUpEvolution(AActor *Owner, FGuid InstanceID)
{
    if (!EconomyMayAct(Owner, TEXT("LevelUpEvolution")))
    {
        return false;
    }

    // Evolution instances live on the dedicated UEvolutionInventoryComponent, NOT UInventoryComponent.
    UEvolutionInventoryComponent *EvoInv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("LevelUpEvolution"), EvoInv, Currency))
    {
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

    // B-333 (Q-333): an evolution with an element also pays the ½ Elemental co-cost.
    if (!TryLevelUpEntry(Currency, Entry->Tier, ECurrencyType::GearEssence,
                         EconomyYield::HasElement(Entry->Item->GetAssociatedElement())))
    {
        return false;
    }
    EconomyRefreshEvolutionInLoadouts(Owner, InstanceID); // B-085
    CheckUpgradeMemories(Owner, nullptr, Entry->Item, Entry->Tier);
    return true;
}

bool UEconomyService::LevelUpSpell(AActor *Owner, const USpellData *Spell)
{
    if (!EconomyMayAct(Owner, TEXT("LevelUpSpell")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("LevelUpSpell"), Inv, Currency))
    {
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

    // Spells level on SKILL essence, §3 category split; + ½ Elemental when the spell has an element (Q-333).
    return TryLevelUpEntry(Currency, Instance->Tier, ECurrencyType::SkillEssence,
                           Spell && EconomyYield::SpellHasElement(Spell->Element));
}

bool UEconomyService::LevelUpAbility(AActor *Owner, const UAbilityData *Ability)
{
    if (!EconomyMayAct(Owner, TEXT("LevelUpAbility")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("LevelUpAbility"), Inv, Currency))
    {
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
    return DowngradeOwnedGear<FWeaponInventoryEntry>(Owner, PersistentID, TEXT("DowngradeWeapon"));
}

bool UEconomyService::DowngradeRing(AActor *Owner, FGuid PersistentID)
{
    return DowngradeOwnedGear<FRingInventoryEntry>(Owner, PersistentID, TEXT("DowngradeRing"));
}

bool UEconomyService::DowngradeEvolution(AActor *Owner, FGuid InstanceID)
{
    if (!EconomyMayAct(Owner, TEXT("DowngradeEvolution")))
    {
        return false;
    }

    UEvolutionInventoryComponent *EvoInv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DowngradeEvolution"), EvoInv, Currency))
    {
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

    if (!TryDowngradeEntry(Currency, Entry->Tier, Entry->Item->Tier, ECurrencyType::GearEssence))
    {
        return false;
    }
    EconomyRefreshEvolutionInLoadouts(Owner, InstanceID); // B-085
    return true;
}

bool UEconomyService::DowngradeSpell(AActor *Owner, const USpellData *Spell)
{
    if (!EconomyMayAct(Owner, TEXT("DowngradeSpell")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DowngradeSpell"), Inv, Currency))
    {
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
    if (!EconomyMayAct(Owner, TEXT("DowngradeAbility")))
    {
        return false;
    }

    UInventoryComponent *Inv = nullptr;
    UCurrencyComponent *Currency = nullptr;
    if (!EconomyFindComponents(Owner, TEXT("DowngradeAbility"), Inv, Currency))
    {
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
    UCurrencyComponent *Currency = CurrencyWallet::FindFor(Owner);
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
        *Owner->GetName(), WoRAuthority::HasAuthority(Owner) ? Y : N,
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
#if !UE_BUILD_SHIPPING
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

    /** A crystal type named by Args[Index] ("Garnet"), or INDEX_NONE (also for the hidden _MAX entry). */
    int64 EconomyDebugParseCrystalType(const TArray<FString> &Args, int32 Index)
    {
        const UEnum *Enum = StaticEnum<ECrystalType>();
        const int64 Value = Args.IsValidIndex(Index) ? Enum->GetValueByNameString(Args[Index]) : INDEX_NONE;
        return Value < Enum->GetMaxEnumValue() ? Value : INDEX_NONE;
    }

    static FAutoConsoleCommandWithWorldAndArgs GGiveCrystalCmd(
        TEXT("wor.Economy.GiveCrystal"),
        TEXT("DEBUG: give the played pawn <Count> crystals of <Type> <Tier> (e.g. Garnet D 2) to set up a merge check."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
            [](const TArray<FString> &Args, UWorld *World)
            {
                const int64 TypeValue = EconomyDebugParseCrystalType(Args, 0);
                const int64 TierValue = Args.Num() > 1 ? StaticEnum<EItemTier>()->GetValueByNameString(Args[1] + TEXT("_Tier")) : INDEX_NONE;
                const int32 Count = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 1;
                if (TypeValue == INDEX_NONE || TypeValue == static_cast<int64>(ECrystalType::None) || TierValue == INDEX_NONE || Count <= 0)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.Economy.GiveCrystal: usage <Type> <Tier> [Count], e.g. wor.Economy.GiveCrystal Garnet D 2"));
                    return;
                }

                APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UInventoryComponent *Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
                if (!Inv)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.Economy.GiveCrystal: no played pawn with an Inventory component."));
                    return;
                }
                TArray<FCrystalId> Ids;
                Ids.Init(FCrystalId(static_cast<ECrystalType>(TypeValue), static_cast<EItemTier>(TierValue)), Count);
                const bool bOK = Inv->AddCrystals(Ids);
                UE_LOG(LogTemp, Display, TEXT("[EconomyService] wor.Economy.GiveCrystal %d x %s -> %s"), Count,
                       *ItemIdentity::GetDisplayName(Ids[0]), bOK ? TEXT("OK") : TEXT("FAILED (tier at cap)"));
            }));

    static FAutoConsoleCommandWithWorldAndArgs GMergeCrystalsCmd(
        TEXT("wor.Economy.Merge"),
        TEXT("DEBUG: merge the played pawn's crystals up to one of <Type> <Tier> (e.g. Garnet C). Logs what was spent and the change given back (ER-28)."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
            [](const TArray<FString> &Args, UWorld *World)
            {
                const int64 TypeValue = EconomyDebugParseCrystalType(Args, 0);
                const int64 TierValue = Args.Num() > 1 ? StaticEnum<EItemTier>()->GetValueByNameString(Args[1] + TEXT("_Tier")) : INDEX_NONE;
                if (TypeValue == INDEX_NONE || TierValue == INDEX_NONE)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.Economy.Merge: usage <Type> <Tier>, e.g. wor.Economy.Merge Garnet C"));
                    return;
                }

                UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                UEconomyService *Economy = GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
                APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                if (!Economy || !Pawn)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[EconomyService] wor.Economy.Merge: no subsystem or played pawn."));
                    return;
                }
                const bool bOK = Economy->MergeCrystals(Pawn, static_cast<ECrystalType>(TypeValue), static_cast<EItemTier>(TierValue));
                UE_LOG(LogTemp, Display, TEXT("[EconomyService] wor.Economy.Merge %s %s -> %s"), *Args[0], *Args[1],
                       bOK ? TEXT("OK") : TEXT("FAILED (see the warning above)"));
            }));
}
#endif // !UE_BUILD_SHIPPING
