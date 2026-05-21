// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "EvolutionItemData.h"
#include "LoadoutComponent.h"
#include "FCrystalInventoryEntry.h"
#include "BreakCalculator.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CombatConstants.h"

void UCrystalManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CrystalManager] Initialized"));
}

void UCrystalManager::Deinitialize()
{
    Super::Deinitialize();
}

// ========================================
// WEAR
// ========================================

int32 UCrystalManager::ProcessPostCastWear(
    AActor *Actor,
    UEvolutionItemData *Crystal,
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
    // GetEquipmentModifiedLuck folds in active-loadout BonusLuck and null-guards
    // the CharacterData asset internally.
    if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
    {
        const float RawLuck = CharComp->GetEquipmentModifiedLuck();
        const float SkipChance = (RawLuck / CombatConstants::LUCK_RAW_MAX) * CombatConstants::LUCK_BREAK_SKIP_MAX;
        if (FMath::FRand() < SkipChance)
        {
            UE_LOG(LogTemp, Log,
                   TEXT("[CrystalManager] %s LUCKY break skip on crystal '%s' (would have applied %d wear, skip chance %.2f)"),
                   *Actor->GetName(), *Crystal->GetFullItemName(), Wear, SkipChance);
            return 0;
        }
    }

    // Resolve the per-instance entry from the holder. The Crystal* parameter is
    // the template (UEvolutionItemData); the FCrystalInventoryEntry is the per-instance
    // state we want to mutate. Phase B 2/5 routes all wear through the entry.
    ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
    if (!LoadoutComp)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear: no LoadoutComponent for %s"),
               *Actor->GetName());
        return 0;
    }

    // Pre-read for defensive broken check and as the no-match guard
    // (empty entry => Crystal == nullptr).
    const FCrystalInventoryEntry PreEntry = LoadoutComp->GetCrystalEntryByHolder(Holder);
    if (!PreEntry.Crystal)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear: could not resolve crystal entry for %s on %s"),
               *Crystal->GetFullItemName(), *Actor->GetName());
        return 0;
    }

    // Defensive: don't double-process an already-broken entry. Commit-time
    // gate should have excluded broken crystals from infusion options.
    if (PreEntry.IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear called on already-broken crystal '%s' for %s"),
               *Crystal->GetFullItemName(), *Actor->GetName());
        return 0;
    }

    const bool bBroke = LoadoutComp->ApplyWearToCrystalEntryByHolder(Holder, Wear);
    const FCrystalInventoryEntry PostEntry = LoadoutComp->GetCrystalEntryByHolder(Holder);

    UE_LOG(LogTemp, Verbose,
           TEXT("[CrystalManager] %s applies %d wear to crystal '%s' (%d/%d) [ActionTier=%d L%d bIsSpell=%d]"),
           *Actor->GetName(), Wear,
           *Crystal->GetFullItemName(),
           PostEntry.CurrentDurability, Crystal->MaxDurability,
           static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

    // Broadcast post-wear durability for real-time UI updates.
    OnCrystalDurabilityChanged.Broadcast(Actor, Holder, PostEntry.CurrentDurability, Crystal->MaxDurability);

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
