// FAbilityCollection.cpp
// Ability inventory collection implementation

#include "FAbilityCollection.h"
#include "AbilityData.h"

TArray<UAbilityData*> FAbilityCollection::GetAbilitiesForWeaponType(EWeaponType WeaponType) const
{
    TArray<UAbilityData*> Result;
    for (UAbilityData* Ability : LearnedAbilities)
    {
        if (Ability && Ability->RequiredWeaponType == WeaponType)
        {
            Result.Add(Ability);
        }
    }
    return Result;
}

int32 FAbilityCollection::GetAbilityCountForWeaponType(EWeaponType WeaponType) const
{
    int32 Count = 0;
    for (UAbilityData* Ability : LearnedAbilities)
    {
        if (Ability && Ability->RequiredWeaponType == WeaponType)
        {
            Count++;
        }
    }
    return Count;
}
