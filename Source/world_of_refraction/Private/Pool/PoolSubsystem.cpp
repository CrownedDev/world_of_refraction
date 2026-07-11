// PoolSubsystem.cpp
// Phase 1c (all owned types, INERT) implementation. See PoolSubsystem.h.

#include "Pool/PoolSubsystem.h"

#include "Pool/PoolAccessors.h"             // the filter/translation layer
#include "Equipment/Crystals/CrystalType.h" // StaticEnum<ECrystalType> for the debug dump
#include "Equipment/Crystals/CrystalTypeHelpers.h" // IsGemType — item/refined gem/stone dispatch
#include "Inventory/ItemTier.h"             // TierHelpers::GetTierName
#include "Engine/Engine.h"                  // GEngine on-screen debug

// --- Populate (authored → pool): the asset + the factories/types the populate dereferences ---
#include "Inventory/InventoryData.h"               // UInventoryData (the authored source)
#include "Skills/Definitions/SpellData.h"          // USpellData::Tier
#include "Skills/Definitions/AbilityData.h"        // UAbilityData::Tier / IsAttack
#include "Equipment/Crystals/EvolutionItemData.h"  // UEvolutionItemData::Tier / MaxDurability

// --- Debug console (wor.* commands): world → game instance → subsystem, + the played-character source ---
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Character/CharacterData.h"            // UCharacterData::Inventory
#include "Character/CharacterDataComponent.h"   // UCharacterDataComponent::CharacterData
#include "Inventory/InventoryComponent.h"       // wor.DrawFromPool target (step 2)

namespace
{
    constexpr float POOL_DEBUG_SCREEN_DURATION = 12.0f;

    FString EnumName(const UEnum *EnumPtr, int64 Value)
    {
        return EnumPtr ? EnumPtr->GetAuthoredNameStringByValue(Value) : FString::FromInt(static_cast<int32>(Value));
    }
}

// ==================== ARMOURY ====================

void UPoolSubsystem::AddWeaponToPool(const FWeaponInventoryEntry &Entry)
{
    OwnedWeapons.Add(Entry);
}

FWeaponInventoryEntry *UPoolSubsystem::FindWeaponInPool(const FGuid &PersistentID)
{
    return OwnedWeapons.FindByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == PersistentID; });
}

const FWeaponInventoryEntry *UPoolSubsystem::FindWeaponInPool(const FGuid &PersistentID) const
{
    return OwnedWeapons.FindByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == PersistentID; });
}

void UPoolSubsystem::AddRingToPool(const FRingInventoryEntry &Entry)
{
    OwnedRings.Add(Entry);
}

FRingInventoryEntry *UPoolSubsystem::FindRingInPool(const FGuid &PersistentID)
{
    return OwnedRings.FindByPredicate(
        [&PersistentID](const FRingInventoryEntry &E) { return E.PersistentID == PersistentID; });
}

const FRingInventoryEntry *UPoolSubsystem::FindRingInPool(const FGuid &PersistentID) const
{
    return OwnedRings.FindByPredicate(
        [&PersistentID](const FRingInventoryEntry &E) { return E.PersistentID == PersistentID; });
}

void UPoolSubsystem::AddEvolutionToPool(const FEvolutionInventoryEntry &Entry)
{
    OwnedEvolutions.Add(Entry);
}

FEvolutionInventoryEntry *UPoolSubsystem::FindEvolutionInPool(const FGuid &InstanceID)
{
    return OwnedEvolutions.FindByPredicate(
        [&InstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == InstanceID; });
}

const FEvolutionInventoryEntry *UPoolSubsystem::FindEvolutionInPool(const FGuid &InstanceID) const
{
    return OwnedEvolutions.FindByPredicate(
        [&InstanceID](const FEvolutionInventoryEntry &E) { return E.InstanceID == InstanceID; });
}

// ==================== KNOWLEDGE ====================

void UPoolSubsystem::AddSpellToPool(const FSpellInstance &Instance)
{
    OwnedSpells.Add(Instance);
}

FSpellInstance *UPoolSubsystem::FindSpellInPool(const FGuid &InstanceID)
{
    return OwnedSpells.FindByPredicate(
        [&InstanceID](const FSpellInstance &I) { return I.InstanceID == InstanceID; });
}

const FSpellInstance *UPoolSubsystem::FindSpellInPool(const FGuid &InstanceID) const
{
    return OwnedSpells.FindByPredicate(
        [&InstanceID](const FSpellInstance &I) { return I.InstanceID == InstanceID; });
}

void UPoolSubsystem::AddAbilityToPool(const FAbilityInstance &Instance)
{
    OwnedAbilities.Add(Instance);
}

FAbilityInstance *UPoolSubsystem::FindAbilityInPool(const FGuid &InstanceID)
{
    return OwnedAbilities.FindByPredicate(
        [&InstanceID](const FAbilityInstance &I) { return I.InstanceID == InstanceID; });
}

const FAbilityInstance *UPoolSubsystem::FindAbilityInPool(const FGuid &InstanceID) const
{
    return OwnedAbilities.FindByPredicate(
        [&InstanceID](const FAbilityInstance &I) { return I.InstanceID == InstanceID; });
}

// ==================== ITEMS ====================

void UPoolSubsystem::AddCrystalToPool(const FCrystalId &Id, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }
    // gem/stone by IsGemType (mirrors the run component's PoolFor).
    (CrystalTypeHelpers::IsGemType(Id.Type) ? Crystals : Stones).FindOrAdd(Id) += Count;
}

// ==================== CATEGORY GETTERS ====================

FPoolKnowledgeView UPoolSubsystem::GetKnowledge(const FPoolFilter &Filter) const
{
    FPoolKnowledgeView View;
    for (const FSpellInstance &Spell : OwnedSpells)
    {
        if (PoolAccessors::MatchesFilter(Filter, Spell))
        {
            View.Spells.Add(Spell);
        }
    }
    for (const FAbilityInstance &Ability : OwnedAbilities)
    {
        if (PoolAccessors::MatchesFilter(Filter, Ability))
        {
            View.Abilities.Add(Ability);
        }
    }
    return View;
}

FPoolArmouryView UPoolSubsystem::GetArmoury(const FPoolFilter &Filter) const
{
    FPoolArmouryView View;
    for (const FWeaponInventoryEntry &Weapon : OwnedWeapons)
    {
        if (PoolAccessors::MatchesFilter(Filter, Weapon))
        {
            View.Weapons.Add(Weapon);
        }
    }
    for (const FRingInventoryEntry &Ring : OwnedRings)
    {
        if (PoolAccessors::MatchesFilter(Filter, Ring))
        {
            View.Rings.Add(Ring);
        }
    }
    for (const FEvolutionInventoryEntry &Evo : OwnedEvolutions)
    {
        if (PoolAccessors::MatchesFilter(Filter, Evo))
        {
            View.Evolutions.Add(Evo);
        }
    }
    return View;
}

TArray<FPoolItemStack> UPoolSubsystem::GetItems(const FPoolFilter &Filter) const
{
    TArray<FPoolItemStack> Out;

    // The item/refined axis is gone (gem-merge) — every stack tags bRefined=false. The dormant
    // FPoolFilter.bUseRefined / .bRefined and FPoolItemStack.bRefined fields are left in
    // PoolQueryTypes for a later cleanup; the browse path is still inert.
    auto Gather = [&](const TMap<FCrystalId, int32> &Pool)
    {
        for (const TPair<FCrystalId, int32> &Pair : Pool)
        {
            if (Filter.bUseBucket && PoolAccessors::GetBucket(Pair.Key) != Filter.Bucket)
            {
                continue; // browse-bucket narrowing (Crystal vs Augment Stone)
            }
            if (PoolAccessors::MatchesFilter(Filter, Pair.Key))
            {
                FPoolItemStack Stack;
                Stack.Id = Pair.Key;
                Stack.Count = Pair.Value;
                Stack.bRefined = false;
                Out.Add(Stack);
            }
        }
    };

    Gather(Crystals);
    Gather(Stones);
    return Out;
}

FPoolQueryResult UPoolSubsystem::QueryAll(const FPoolFilter &Filter) const
{
    FPoolQueryResult Result;
    Result.Knowledge = GetKnowledge(Filter);
    Result.Armoury = GetArmoury(Filter);
    Result.Items = GetItems(Filter);
    return Result;
}

// ==================== POPULATE (authored → pool) ====================

void UPoolSubsystem::PopulateFromInventoryAsset(UInventoryData *Asset, AActor *OwnerContext)
{
    // One-time per session (GI-scoped). A flag, not an is-empty check, so a legitimately-empty
    // authored asset can't re-trigger and double-bank.
    if (bPopulated)
    {
        UE_LOG(LogTemp, Log, TEXT("[Pool] PopulateFromInventoryAsset: already populated this session — skipping."));
        return;
    }
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pool] PopulateFromInventoryAsset: null Asset — nothing to populate."));
        return;
    }

    // ---------- Weapons ----------
    // Reuse the SAME factory the run uses: CreateFromWeapon copies the default crystal into
    // AttachedItem + seeds Tier/Quality; the acquisition mint adds a fresh PersistentID (the factory
    // deliberately leaves it invalid). NO pickup roll — authored banking is deterministic (the
    // toggle-off path keeps the factory's C_Quality placeholder).
    for (UWeaponData *Weapon : Asset->Weapons)
    {
        if (!Weapon)
        {
            continue;
        }
        FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
        Entry.PersistentID = FGuid::NewGuid();
        AddWeaponToPool(Entry);
    }

    // ---------- Rings ----------
    for (URingData *Ring : Asset->Rings)
    {
        if (!Ring)
        {
            continue;
        }
        FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
        Entry.PersistentID = FGuid::NewGuid();
        AddRingToPool(Entry);
    }

    // ---------- Spells ----------
    // Learn-time mint (mirrors FSpellCollection::LearnSpell): ctor mints InstanceID; seed Tier from
    // the asset + the C_Quality placeholder.
    for (USpellData *Spell : Asset->Spells)
    {
        if (!Spell)
        {
            continue;
        }
        FSpellInstance Instance(Spell);
        Instance.Tier = Spell->Tier;
        Instance.Quality = EItemQuality::C_Quality;
        AddSpellToPool(Instance);
    }

    // ---------- Abilities ----------
    // Mirror FAbilityCollection::LearnAbility: basic attacks live on the weapon, never the bar — skip.
    for (UAbilityData *Ability : Asset->Abilities)
    {
        if (!Ability || Ability->IsAttack())
        {
            continue;
        }
        FAbilityInstance Instance(Ability);
        Instance.Tier = Ability->Tier;
        Instance.Quality = EItemQuality::C_Quality;
        AddAbilityToPool(Instance);
    }

    // ---------- Crystals ----------
    // Seed from the authored CrystalStock (mixed gem+stone); AddCrystalToPool routes gem→Crystals /
    // stone→Stones by IsGemType. The asset's PostLoad already folded the deprecated ItemCrystals +
    // RefinedCrystals into CrystalStock (and emptied them), so this reads the merged stock — the same
    // source the run bulk-load reads, keeping pool-seed and run-seed consistent.
    for (const TPair<FCrystalId, int32> &Pair : Asset->CrystalStock)
    {
        AddCrystalToPool(Pair.Key, Pair.Value);
    }

    // ---------- Evolutions ----------
    // Mint an owned entry per authored item (mirrors UEvolutionInventoryComponent::AddInstance's seed:
    // InstanceID via ctor, Tier from asset, C_Quality placeholder, full starting durability). No pickup
    // roll (deterministic banking) and no run-cap gate — the pool is raw bank storage.
    for (UEvolutionItemData *Item : Asset->EvolutionEquipment)
    {
        if (!Item)
        {
            continue;
        }
        FEvolutionInventoryEntry Entry(Item);
        Entry.Tier = Item->Tier;
        Entry.Quality = EItemQuality::C_Quality;
        Entry.CurrentDurability = Item->MaxDurability;
        AddEvolutionToPool(Entry);
    }

    bPopulated = true;

    UE_LOG(LogTemp, Display,
           TEXT("[Pool] Populated from '%s' (owner=%s): Weapons=%d Rings=%d Spells=%d Abilities=%d Evolutions=%d"),
           *Asset->GetName(),
           OwnerContext ? *OwnerContext->GetName() : TEXT("none"),
           OwnedWeapons.Num(), OwnedRings.Num(), OwnedSpells.Num(), OwnedAbilities.Num(), OwnedEvolutions.Num());
}

// ==================== DEBUG ====================

FString UPoolSubsystem::GetPoolString() const
{
    const UEnum *CrystalEnum = StaticEnum<ECrystalType>();
    const UEnum *EssenceEnum = StaticEnum<EEssenceType>();

    auto SumStacks = [](const TMap<FCrystalId, int32> &Pool)
    {
        int32 Total = 0;
        for (const TPair<FCrystalId, int32> &Pair : Pool)
        {
            Total += Pair.Value;
        }
        return Total;
    };

    FString Out = TEXT("[Pool]");
    Out += FString::Printf(TEXT("\n  Armoury  : Weapons=%d Rings=%d Evolutions=%d"),
                           OwnedWeapons.Num(), OwnedRings.Num(), OwnedEvolutions.Num());
    Out += FString::Printf(TEXT("\n  Knowledge: Spells=%d Abilities=%d"),
                           OwnedSpells.Num(), OwnedAbilities.Num());
    Out += FString::Printf(TEXT("\n  Items    : Crystals=%d (%d), Stones=%d (%d)  [stacks (total)]"),
                           Crystals.Num(), SumStacks(Crystals),
                           Stones.Num(), SumStacks(Stones));

    // Crystal stacks grouped by BROWSE bucket (Crystal vs Augment Stone), each listing both pools.
    auto DumpBucket = [&](EItemBucket Bucket, const TCHAR *Label)
    {
        Out += FString::Printf(TEXT("\n    %s:"), Label);
        auto EmitPool = [&](const TMap<FCrystalId, int32> &Pool, const TCHAR *Tag)
        {
            for (const TPair<FCrystalId, int32> &Pair : Pool)
            {
                if (PoolAccessors::GetBucket(Pair.Key) != Bucket)
                {
                    continue;
                }
                Out += FString::Printf(TEXT("\n      [%s] %s %s x%d"), Tag,
                                       *EnumName(CrystalEnum, static_cast<int64>(Pair.Key.Type)),
                                       *TierHelpers::GetTierName(Pair.Key.Tier), Pair.Value);
            }
        };
        EmitPool(Crystals, TEXT("crystal"));
        EmitPool(Stones, TEXT("stone"));
    };
    DumpBucket(EItemBucket::Crystal, TEXT("Crystals"));
    DumpBucket(EItemBucket::AugmentStone, TEXT("Augment Stones"));

    // Wallet.
    Out += FString::Printf(TEXT("\n  Wallet   : Gold=%d Prisms=%d Diamond=%d Gear=%d Skill=%d"),
                           PoolGold, PoolPrisms, PoolDiamond, PoolGearEssence, PoolSkillEssence);
    for (const FCurrencyEntry &E : PoolEssence.Items)
    {
        if (E.Amount != 0)
        {
            Out += FString::Printf(TEXT("\n      Essence[%s]=%d"),
                                   *EnumName(EssenceEnum, static_cast<int64>(E.Key)), E.Amount);
        }
    }

    return Out;
}

void UPoolSubsystem::PrintPoolState() const
{
    const FString Dump = GetPoolString();
    UE_LOG(LogTemp, Log, TEXT("%s"), *Dump);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, POOL_DEBUG_SCREEN_DURATION, FColor::Cyan, Dump);
    }
}

// ==================== DEBUG CONSOLE COMMANDS ====================
// FAutoConsoleCommandWithWorld — NOT UFUNCTION(Exec). Exec only dispatches on PlayerController / Pawn /
// GameMode / GameInstance / CheatManager / HUD; a UGameInstanceSubsystem is NOT on that list, so an Exec
// here would silently no-op (the documented cause of "console commands never working" on the wrong class).
// Console commands work from any class. The "wor." prefix groups them and avoids engine-command clashes.
// Both resolve PIE-only state, so the whole world → game-instance → subsystem chain is null-checked.

namespace
{
    UPoolSubsystem *ResolvePoolFromWorld(UWorld *World)
    {
        if (!World)
        {
            return nullptr;
        }
        UGameInstance *GI = World->GetGameInstance();
        return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
    }

    static FAutoConsoleCommandWithWorld GPopulatePoolCmd(
        TEXT("wor.PopulatePool"),
        TEXT("Populate the account pool from the played character's authored inventory, then print it (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UPoolSubsystem *Pool = ResolvePoolFromWorld(World);
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.PopulatePool: no pool subsystem (no PIE world?)."));
                    return;
                }

                // Source the InventoryData from the PLAYED character: first player controller's pawn →
                // CharacterDataComponent → CharacterData->Inventory. Cleanest testing source — it banks
                // whatever character is being played, with no hardcoded asset path to keep in sync.
                APlayerController *PC = World->GetFirstPlayerController();
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UCharacterDataComponent *CDC = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
                UCharacterData *CharData = CDC ? CDC->CharacterData : nullptr;
                UInventoryData *Inventory = CharData ? CharData->Inventory : nullptr;
                if (!Inventory)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[Pool] wor.PopulatePool: could not resolve a played character's Inventory asset (pawn / CharacterData / Inventory missing)."));
                    return;
                }

                Pool->PopulateFromInventoryAsset(Inventory, Pawn);
                Pool->PrintPoolState();
            }));

    static FAutoConsoleCommandWithWorld GPrintPoolCmd(
        TEXT("wor.PrintPool"),
        TEXT("Dump the account pool (per-category counts, crystal stacks, wallet) to log + screen (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UPoolSubsystem *Pool = ResolvePoolFromWorld(World);
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.PrintPool: no pool subsystem (no PIE world?)."));
                    return;
                }
                Pool->PrintPoolState();
            }));

    // Step 2 — the first NON-INERT flip: DRAW the run inventory from the pool. Uncalled-first
    // (not wired into InitializeFromCharacterData), so this debug command is the ONLY way to
    // trigger a pool draw. PIE stays byte-identical until you type wor.DrawFromPool manually.
    static FAutoConsoleCommandWithWorld GDrawFromPoolCmd(
        TEXT("wor.DrawFromPool"),
        TEXT("Draw the played character's RUN inventory FROM the account pool (whole-entry, preserves instance identity), then print the pool (PIE debug). Owned inventory only — SavedLoadouts stay authored."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UPoolSubsystem *Pool = ResolvePoolFromWorld(World);
                if (!Pool)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.DrawFromPool: no pool subsystem (no PIE world?)."));
                    return;
                }

                APlayerController *PC = World->GetFirstPlayerController();
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UInventoryComponent *Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
                if (!Inv)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[Pool] wor.DrawFromPool: could not resolve a played character's InventoryComponent (pawn / component missing)."));
                    return;
                }

                Inv->InitializeFromPool(Pool);
                Pool->PrintPoolState();
            }));

    // The DIFF tool: dump the played character's RUN inventory per-INSTANCE (tiers / quality /
    // attached crystals). Run after wor.PopulatePool + wor.DrawFromPool and compare against
    // wor.PrintPool — matching tiers/crystals prove the whole-entry draw was lossless.
    static FAutoConsoleCommandWithWorld GPrintInventoryCmd(
        TEXT("wor.PrintInventory"),
        TEXT("Dump the played character's run inventory per-instance (PersistentID/InstanceID, tier, quality, attached crystal) to log + screen (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
                APawn *Pawn = PC ? PC->GetPawn() : nullptr;
                UInventoryComponent *Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
                if (!Inv)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[Pool] wor.PrintInventory: could not resolve a played character's InventoryComponent (pawn / component missing)."));
                    return;
                }

                const FString Dump = Inv->GetInventoryInstanceString();
                UE_LOG(LogTemp, Log, TEXT("%s"), *Dump);
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, POOL_DEBUG_SCREEN_DURATION, FColor::Yellow, Dump);
                }
            }));
}
