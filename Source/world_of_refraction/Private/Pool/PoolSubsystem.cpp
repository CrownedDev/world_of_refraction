// PoolSubsystem.cpp
// Phase 1c (all owned types, INERT) implementation. See PoolSubsystem.h.

#include "Pool/PoolSubsystem.h"

#include "Equipment/Crystals/CrystalType.h" // StaticEnum<ECrystalType> for the debug dump
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
    ItemCrystals.FindOrAdd(Id) += Count;
}

void UPoolSubsystem::AddRefinedCrystalToPool(const FCrystalId &Id, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }
    RefinedCrystals.FindOrAdd(Id) += Count;
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
    Out += FString::Printf(TEXT("\n  Items    : ItemCrystals=%d stacks (%d total), RefinedCrystals=%d stacks (%d total)"),
                           ItemCrystals.Num(), SumStacks(ItemCrystals),
                           RefinedCrystals.Num(), SumStacks(RefinedCrystals));

    // Crystal stacks, both pools.
    for (const TPair<FCrystalId, int32> &Pair : ItemCrystals)
    {
        Out += FString::Printf(TEXT("\n      [item]    %s %s x%d"),
                               *EnumName(CrystalEnum, static_cast<int64>(Pair.Key.Type)),
                               *TierHelpers::GetTierName(Pair.Key.Tier), Pair.Value);
    }
    for (const TPair<FCrystalId, int32> &Pair : RefinedCrystals)
    {
        Out += FString::Printf(TEXT("\n      [refined] %s %s x%d"),
                               *EnumName(CrystalEnum, static_cast<int64>(Pair.Key.Type)),
                               *TierHelpers::GetTierName(Pair.Key.Tier), Pair.Value);
    }

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
