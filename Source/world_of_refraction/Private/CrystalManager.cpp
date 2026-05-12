// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "ItemData.h"
#include "LoadoutComponent.h"
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
    TrackedCrystals.Empty();
    Super::Deinitialize();
}

// ========================================
// LIFECYCLE — STUB
// ========================================
// Implementations land in Commit 4 (weapon-side migration) and Commit 5
// (combat-init wiring). For now these are stubs so the subsystem compiles
// and registers without changing any runtime behaviour.

void UCrystalManager::RegisterCombatant(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }
    UE_LOG(LogTemp, Verbose, TEXT("[CrystalManager] RegisterCombatant stub for %s"),
           *Actor->GetName());
}

void UCrystalManager::UnregisterCombatant(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }
    UE_LOG(LogTemp, Verbose, TEXT("[CrystalManager] UnregisterCombatant stub for %s"),
           *Actor->GetName());
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

    if (Crystal->IsBroken())
    {
        // Commit-time gate should have excluded this from infusion options.
        // Defensive: don't double-process.
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear called on already-broken crystal '%s' for %s"),
               *Crystal->GetFullItemName(), *Actor->GetName());
        return 0;
    }

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

    UE_LOG(LogTemp, Verbose,
           TEXT("[CrystalManager] %s applies %d wear to crystal '%s' (%d/%d) [ActionTier=%d L%d bIsSpell=%d]"),
           *Actor->GetName(), Wear,
           *Crystal->GetFullItemName(),
           Crystal->CurrentDurability, Crystal->MaxDurability,
           static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

    Crystal->ApplyWear(Wear);
    // ApplyWear fires UItemData::OnCrystalBroken if durability hits 0.
    // Subscription pipeline (Commit 4) will register UCrystalManager as a
    // listener and rebroadcast as unified OnCrystalBroken.

    // Broadcast durability change for real-time UI updates. Fires whether
    // the crystal survived or just broke — UI updates either way.
    OnCrystalDurabilityChanged.Broadcast(Actor, Holder, Crystal->CurrentDurability, Crystal->MaxDurability);

    return Wear;
}

// ========================================
// HANDLERS — STUB
// ========================================

void UCrystalManager::HandleCrystalBroken(UItemData *BrokenCrystal)
{
    // Resolution + broadcast lands in Commit 4.
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
