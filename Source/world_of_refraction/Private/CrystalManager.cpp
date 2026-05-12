// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "ItemData.h"
#include "LoadoutComponent.h"

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
// WEAR — STUB
// ========================================
// Real implementation lands in Commit 3 (weapon-side migration). For now
// returns 0 so any unintended caller is a no-op.

int32 UCrystalManager::ProcessPostCastWear(
    AActor *Actor,
    UItemData *Crystal,
    UObject *Holder,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    return 0;
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
