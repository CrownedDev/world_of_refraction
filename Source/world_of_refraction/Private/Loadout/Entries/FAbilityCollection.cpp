// FAbilityCollection.cpp
// Ability inventory collection implementation

#include "Loadout/Entries/FAbilityCollection.h"
#include "Skills/Definitions/AbilityData.h"

// PRIMARY slotting gate (step 6a): basic attacks (bIsAttack) never enter the learned-ability pool, so
// they can't be slotted as abilities. Post attack/ability merge they're UAbilityData, so without this
// gate a designer-listed attack could be learned and appear slottable. The body lives here (not inline
// in the header) because IsAttack() needs the full UAbilityData type, included in this .cpp.
bool FAbilityCollection::LearnAbility(UAbilityData* Ability)
{
    if (!Ability || !CanLearn() || HasAbility(Ability))
    {
        return false;
    }
    if (Ability->IsAttack())
    {
        return false; // basic attacks live on the weapon (WeaponData::WeaponAttack), not the ability bar
    }
    LearnedAbilities.Add(Ability);
    return true;
}

TArray<UAbilityData*> FAbilityCollection::GetAbilitiesForWeaponType(EWeaponType WeaponType) const
{
    TArray<UAbilityData*> Result;
    for (UAbilityData* Ability : LearnedAbilities)
    {
        // Belt: exclude basic attacks from the assignable list (redundant with the LearnAbility gate).
        if (Ability && !Ability->IsAttack() && Ability->RequiredWeaponType == WeaponType)
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
