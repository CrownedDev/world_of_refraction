// WeaponAttackData.cpp
// Weapon attack data implementation

#include "WeaponAttackData.h"

FString UWeaponAttackData::GetDamageTypeName() const
{
    switch (PhysicalDamageType)
    {
    case EPhysicalDamageType::Slash:
        return TEXT("Slash");
    case EPhysicalDamageType::Pierce:
        return TEXT("Pierce");
    case EPhysicalDamageType::Blunt:
        return TEXT("Blunt");
    default:
        return TEXT("Unknown");
    }
}