// PoolSubsystem.cpp
// Phase 1 (weapons-only, INERT) implementation. See PoolSubsystem.h.

#include "Pool/PoolSubsystem.h"

#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "Engine/Engine.h"

namespace
{
    constexpr float POOL_DEBUG_SCREEN_DURATION = 12.0f;

    FString QualityName(EItemQuality Quality)
    {
        if (const UEnum *EnumPtr = StaticEnum<EItemQuality>())
        {
            return EnumPtr->GetAuthoredNameStringByValue(static_cast<int64>(Quality));
        }
        return FString::FromInt(static_cast<int32>(Quality));
    }
}

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

FString UPoolSubsystem::GetPoolString() const
{
    FString Out = FString::Printf(TEXT("[Pool] OwnedWeapons: %d"), OwnedWeapons.Num());
    for (int32 i = 0; i < OwnedWeapons.Num(); ++i)
    {
        const FWeaponInventoryEntry &E = OwnedWeapons[i];
        Out += FString::Printf(TEXT("\n  [%d] %s | Tier=%s Quality=%s | GUID=%s"),
                               i,
                               *GetNameSafe(E.Weapon),
                               *TierHelpers::GetTierName(E.Tier),
                               *QualityName(E.Quality),
                               *E.PersistentID.ToString());
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
