// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "ItemData.h"
#include "LoadoutComponent.h"
#include "FCrystalInventoryEntry.h"
#include "BreakCalculator.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CombatConstants.h"
#include "UObject/UObjectIterator.h"

void UCrystalManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CrystalManager] Initialized"));
}

void UCrystalManager::Deinitialize()
{
    TrackedCrystals.Empty();
    Super::Deinitialize();
}

// ========================================
// LIFECYCLE
// ========================================

void UCrystalManager::RegisterCombatant(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
    if (!LoadoutComp)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] RegisterCombatant: %s has no LoadoutComponent"),
               *Actor->GetName());
        return;
    }

    // Walk every crystal-bearing slot on the actor's loadout. Subscribe to
    // each crystal's OnCrystalBroken delegate. Track the list so
    // UnregisterCombatant can detach symmetrically.
    TArray<TWeakObjectPtr<UItemData>> &CrystalsForActor = TrackedCrystals.FindOrAdd(Actor);
    CrystalsForActor.Empty();

    int32 SubscribedCount = 0;
    for (const FEquippedCrystalSlot &Slot : LoadoutComp->GetEquippedCrystals())
    {
        UItemData *Crystal = Slot.Crystal;
        if (!Crystal || !Crystal->bIsRefined || Crystal->bImmuneToBreaking)
        {
            continue;
        }

        if (!Crystal->OnCrystalBroken.IsAlreadyBound(this, &UCrystalManager::HandleCrystalBroken))
        {
            Crystal->OnCrystalBroken.AddDynamic(this, &UCrystalManager::HandleCrystalBroken);
        }
        CrystalsForActor.AddUnique(Crystal);
        ++SubscribedCount;
    }

    UE_LOG(LogTemp, Log,
           TEXT("[CrystalManager] Registered %s — subscribed to %d crystals"),
           *Actor->GetName(), SubscribedCount);
}

void UCrystalManager::UnregisterCombatant(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    TArray<TWeakObjectPtr<UItemData>> *CrystalsForActor = TrackedCrystals.Find(Actor);
    if (!CrystalsForActor)
    {
        return;
    }

    int32 UnsubscribedCount = 0;
    for (const TWeakObjectPtr<UItemData> &CrystalPtr : *CrystalsForActor)
    {
        UItemData *Crystal = CrystalPtr.Get();
        if (!Crystal)
        {
            continue;
        }

        // Phase A shared-asset footgun: two combatants slotting the same
        // UItemData asset share the subscription. Unregistering one removes
        // the only binding and the other actor stops getting break events.
        // Accepted limitation — Phase B's per-instance migration removes the
        // sharing concern entirely.
        if (Crystal->OnCrystalBroken.IsAlreadyBound(this, &UCrystalManager::HandleCrystalBroken))
        {
            Crystal->OnCrystalBroken.RemoveDynamic(this, &UCrystalManager::HandleCrystalBroken);
            ++UnsubscribedCount;
        }
    }

    TrackedCrystals.Remove(Actor);

    UE_LOG(LogTemp, Log,
           TEXT("[CrystalManager] Unregistered %s — unsubscribed from %d crystals"),
           *Actor->GetName(), UnsubscribedCount);
}

// ========================================
// WEAR
// ========================================

int32 UCrystalManager::ProcessPostCastWear(
    AActor *Actor,
    UItemData *Crystal,
    UObject *Holder,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    if (!Actor || !Crystal)
    {
        return 0;
    }

    if (!Crystal->bIsRefined || Crystal->bImmuneToBreaking)
    {
        // Unrefined consumables and immune (evolution) crystals never wear.
        return 0;
    }

    // Defensive broken-check moved post-entry-resolution below — the entry
    // (not the asset) is the per-instance source of truth post-Phase-B 2/5.

    const int32 Wear = UBreakCalculator::CalculateDurabilityWear(
        Crystal->Tier,
        ActionTier,
        InfusionLevel,
        bIsSpell);

    if (Wear <= 0)
    {
        return 0;
    }

    // Luck-driven break skip. Roll the wielder's Luck before applying wear.
    // On success, skip the wear entirely (durability unchanged, no broadcast).
    if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
    {
        if (UCharacterData *CharData = CharComp->CharacterData)
        {
            const float RawLuck = CharData->CalculateLuck();
            const float SkipChance = (RawLuck / CombatConstants::LUCK_RAW_MAX) * CombatConstants::LUCK_BREAK_SKIP_MAX;
            if (FMath::FRand() < SkipChance)
            {
                UE_LOG(LogTemp, Log,
                       TEXT("[CrystalManager] %s LUCKY break skip on crystal '%s' (would have applied %d wear, skip chance %.2f)"),
                       *Actor->GetName(), *Crystal->GetFullItemName(), Wear, SkipChance);
                return 0;
            }
        }
    }

    // Resolve the per-instance entry from the holder. The Crystal* parameter is
    // the template (UItemData); the FCrystalInventoryEntry is the per-instance
    // state we want to mutate. Phase B 2/5 routes all wear through the entry.
    ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
    FCrystalInventoryEntry *Entry = LoadoutComp ? LoadoutComp->FindCrystalEntryByHolder(Holder) : nullptr;
    if (!Entry)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear: could not resolve crystal entry for %s on %s"),
               *Crystal->GetFullItemName(), *Actor->GetName());
        return 0;
    }

    // Defensive: don't double-process an already-broken entry. Commit-time
    // gate should have excluded broken crystals from infusion options.
    if (Entry->IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear called on already-broken crystal '%s' for %s"),
               *Crystal->GetFullItemName(), *Actor->GetName());
        return 0;
    }

    const bool bBroke = Entry->ApplyWear(Wear);

    UE_LOG(LogTemp, Verbose,
           TEXT("[CrystalManager] %s applies %d wear to crystal '%s' (%d/%d) [ActionTier=%d L%d bIsSpell=%d]"),
           *Actor->GetName(), Wear,
           *Crystal->GetFullItemName(),
           Entry->CurrentDurability, Crystal->MaxDurability,
           static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

    // Broadcast post-wear durability for real-time UI updates.
    OnCrystalDurabilityChanged.Broadcast(Actor, Holder, Entry->CurrentDurability, Crystal->MaxDurability);

    // If the wear broke the crystal, broadcast unified OnCrystalBroken directly.
    // The asset-side Crystal->ApplyWear path (and its UItemData::OnCrystalBroken
    // delegate, plus the RegisterCombatant subscription that listens for it)
    // is no longer invoked. Both delegate and subscription become dead-code
    // after this commit and are removed in commit 5.
    if (bBroke)
    {
        UE_LOG(LogTemp, Log,
               TEXT("[CrystalManager] Crystal '%s' broke on %s (holder: %s)"),
               *Crystal->GetFullItemName(),
               *Actor->GetName(),
               Holder ? *Holder->GetName() : TEXT("Unknown"));

        OnCrystalBroken.Broadcast(Actor, Holder, Crystal);
    }

    return Wear;
}

// ========================================
// HANDLERS
// ========================================

void UCrystalManager::HandleCrystalBroken(UItemData *BrokenCrystal)
{
    if (!BrokenCrystal)
    {
        return;
    }

    // Resolve owner + holder by walking every live LoadoutComponent.
    // TrackedCrystals alone gives us the actor, but not the holder asset
    // (UWeaponData / URingData) — the broadcast needs both.
    AActor *OwnerActor = nullptr;
    UObject *OwnerHolder = nullptr;

    for (TObjectIterator<ULoadoutComponent> It; It; ++It)
    {
        ULoadoutComponent *Loadout = *It;
        if (!Loadout || !IsValid(Loadout) || Loadout->HasAnyFlags(RF_ClassDefaultObject))
        {
            continue;
        }

        for (const FEquippedCrystalSlot &Slot : Loadout->GetEquippedCrystals())
        {
            if (Slot.Crystal == BrokenCrystal)
            {
                OwnerActor = Loadout->GetOwner();
                OwnerHolder = Slot.Holder;
                break;
            }
        }

        if (OwnerActor)
        {
            break;
        }
    }

    if (!OwnerActor)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] HandleCrystalBroken: no owner found for crystal '%s'"),
               *BrokenCrystal->GetFullItemName());
        return;
    }

    UE_LOG(LogTemp, Log,
           TEXT("[CrystalManager] Crystal '%s' broke on %s (holder: %s)"),
           *BrokenCrystal->GetFullItemName(),
           *OwnerActor->GetName(),
           OwnerHolder ? *OwnerHolder->GetName() : TEXT("Unknown"));

    OnCrystalBroken.Broadcast(OwnerActor, OwnerHolder, BrokenCrystal);
}

// ========================================
// HELPERS
// ========================================

ULoadoutComponent *UCrystalManager::GetLoadoutComponent(AActor *Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }
    return Actor->FindComponentByClass<ULoadoutComponent>();
}
