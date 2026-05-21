// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "EvolutionItemData.h"
#include "LoadoutComponent.h"
#include "FCrystalInventoryEntry.h"
#include "BreakCalculator.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CombatConstants.h"
#include "TurnManager.h"
#include "WeaponData.h"
#include "Engine/GameInstance.h"

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

// ========================================
// DEBUG
// ========================================

void UCrystalManager::DebugBreakActiveCrystal()
{
    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug.BreakActiveCrystal] No TurnManager subsystem."));
        return;
    }

    AActor *Actor = TurnMgr->GetCurrentActor();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug.BreakActiveCrystal] No active character (combat not active?)."));
        return;
    }

    ULoadoutComponent *LC = GetLoadoutComponent(Actor);
    if (!LC)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] No LoadoutComponent on %s."),
               *Actor->GetName());
        return;
    }

    UWeaponData *Weapon = LC->GetPrimaryWeapon();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s has no primary weapon equipped."),
               *Actor->GetName());
        return;
    }

    const FCrystalInventoryEntry Entry = LC->GetCrystalEntryByHolder(Weapon);
    if (!Entry.Crystal)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon has no attached crystal."),
               *Actor->GetName());
        return;
    }
    if (Entry.IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon crystal '%s' is already broken."),
               *Actor->GetName(), *Entry.Crystal->GetFullItemName());
        return;
    }
    if (!Entry.Crystal->bIsRefined)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon crystal '%s' is unrefined (consumables do not wear/break)."),
               *Actor->GetName(), *Entry.Crystal->GetFullItemName());
        return;
    }
    if (Entry.Crystal->bImmuneToBreaking)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon crystal '%s' is immune to breaking (likely evolution crystal)."),
               *Actor->GetName(), *Entry.Crystal->GetFullItemName());
        return;
    }

    // Drain durability down to 1 so the next non-skipped wear breaks it.
    // ApplyWearToCrystalEntryByHolder does not broadcast OnCrystalBroken on
    // partial wear (non-zero remaining) — the unified broadcast lives in
    // ProcessPostCastWear's bBroke branch, so this drain stays silent.
    const int32 DrainAmount = Entry.CurrentDurability - 1;
    if (DrainAmount > 0)
    {
        LC->ApplyWearToCrystalEntryByHolder(Weapon, DrainAmount);
    }

    // Route through the real pipeline. Luck-skip is probabilistic, so retry
    // until the entry transitions to broken. S-Tier + L2 + spell parameters
    // produce the worst-case wear, guaranteeing >0 wear on any non-skipped roll.
    static constexpr int32 MAX_ATTEMPTS = 8;
    for (int32 Attempt = 1; Attempt <= MAX_ATTEMPTS; ++Attempt)
    {
        ProcessPostCastWear(Actor, Entry.Crystal, Weapon,
                            EItemTier::S_Tier, /*InfusionLevel*/ 2, /*bIsSpell*/ true);

        if (LC->GetCrystalEntryByHolder(Weapon).IsBroken())
        {
            UE_LOG(LogTemp, Display,
                   TEXT("[Debug.BreakActiveCrystal] Broke %s's primary weapon crystal '%s' on attempt %d/%d."),
                   *Actor->GetName(), *Entry.Crystal->GetFullItemName(), Attempt, MAX_ATTEMPTS);
            return;
        }
    }

    // All attempts luck-skipped. Surface the luck stat so we know whether the
    // cap needs raising for high-luck actors.
    float RawLuck = -1.0f;
    if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
    {
        RawLuck = CharComp->GetEquipmentModifiedLuck();
    }
    UE_LOG(LogTemp, Warning,
           TEXT("[Debug.BreakActiveCrystal] All %d attempts luck-skipped on %s's crystal '%s' (EquipmentModifiedLuck=%.2f). Try again, or raise MAX_ATTEMPTS if this recurs."),
           MAX_ATTEMPTS, *Actor->GetName(), *Entry.Crystal->GetFullItemName(), RawLuck);
}
