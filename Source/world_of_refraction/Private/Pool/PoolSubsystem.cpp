// PoolSubsystem.cpp
// Phase 1c (all owned types, INERT) implementation. See PoolSubsystem.h.

#include "Pool/PoolSubsystem.h"

#include "Pool/PoolAccessors.h"             // the filter/translation layer
#include "Equipment/Crystals/CrystalType.h" // StaticEnum<ECrystalType> for the debug dump
#include "Equipment/Crystals/CrystalTypeHelpers.h" // IsGemType — item/refined gem/stone dispatch
#include "Inventory/ItemTier.h"             // TierHelpers::GetTierName
#include "Engine/Engine.h"                  // GEngine on-screen debug

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

void UPoolSubsystem::AddItemCrystalToPool(const FCrystalId &Id, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }
    // Item axis → gem/stone by IsGemType (mirrors the run component's ItemPoolFor).
    (CrystalTypeHelpers::IsGemType(Id.Type) ? GemItem : StoneItem).FindOrAdd(Id) += Count;
}

void UPoolSubsystem::AddRefinedCrystalToPool(const FCrystalId &Id, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }
    if (!CrystalTypeHelpers::IsGemType(Id.Type))
    {
        return; // stones have no refined form — reject (mirrors RefinedPoolFor == nullptr)
    }
    GemRefined.FindOrAdd(Id) += Count;
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

    // bUseRefined narrows to one pool; otherwise both are returned, each tagged bRefined.
    auto Gather = [&](const TMap<FCrystalId, int32> &Pool, bool bRefined)
    {
        if (Filter.bUseRefined && Filter.bRefined != bRefined)
        {
            return;
        }
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
                Stack.bRefined = bRefined;
                Out.Add(Stack);
            }
        }
    };

    Gather(GemItem, false);
    Gather(GemRefined, true);
    Gather(StoneItem, false); // stones are item-only (no refined form)
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
    Out += FString::Printf(TEXT("\n  Items    : GemItem=%d (%d), GemRefined=%d (%d), StoneItem=%d (%d)  [stacks (total)]"),
                           GemItem.Num(), SumStacks(GemItem),
                           GemRefined.Num(), SumStacks(GemRefined),
                           StoneItem.Num(), SumStacks(StoneItem));

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
        EmitPool(GemItem, TEXT("item"));
        EmitPool(GemRefined, TEXT("refined"));
        EmitPool(StoneItem, TEXT("item"));
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
