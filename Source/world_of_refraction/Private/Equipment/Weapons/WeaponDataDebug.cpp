// WeaponDataDebug.cpp
// Debug utilities implementation

#include "Equipment/Weapons/WeaponDataDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Character/StanceData.h"
#include "Skills/Definitions/SkillDataBase.h"

void UWeaponDataDebug::LogWeaponStats(UWeaponData *Weapon)
{
    if (!Weapon)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Weapon is NULL"));
        return;
    }

    FString Output = GetWeaponStatsString(Weapon);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
}

FString UWeaponDataDebug::GetWeaponStatsString(UWeaponData *Weapon)
{
    if (!Weapon)
    {
        return TEXT("ERROR: Weapon is NULL");
    }

    FString Output;

    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("WEAPON: %s\n"), *Weapon->Name);
    Output += TEXT("==========================================\n");

    // Identity
    Output += FString::Printf(TEXT("Type: %s\n"), *Weapon->GetWeaponTypeName());
    if (!Weapon->Description.IsEmpty())
    {
        Output += FString::Printf(TEXT("Description: %s\n"), *Weapon->Description);
    }

    // Attack
    Output += TEXT("\nATTACK:\n");
    if (Weapon->WeaponAttack)
    {
        Output += FString::Printf(TEXT("  %s (%s)\n"),
                                  *Weapon->WeaponAttack->Name,
                                  *Weapon->WeaponAttack->GetAttackSummary());
    }
    else
    {
        Output += TEXT("  None assigned\n");
    }

    // Abilities
    Output += FString::Printf(TEXT("\nABILITIES (%d/6):\n"), Weapon->GetAbilityCount());
    if (Weapon->PresetAbilities.Num() > 0)
    {
        for (int32 i = 0; i < Weapon->PresetAbilities.Num(); ++i)
        {
            if (Weapon->PresetAbilities[i])
            {
                Output += FString::Printf(TEXT("  %d. %s\n"), i + 1, *Weapon->PresetAbilities[i]->Name);
            }
            else
            {
                Output += FString::Printf(TEXT("  %d. (empty)\n"), i + 1);
            }
        }
    }
    else
    {
        Output += TEXT("  None assigned\n");
    }

    // Stance
    Output += TEXT("\nSTANCE:\n");
    if (Weapon->WeaponStance)
    {
        Output += FString::Printf(TEXT("  %s\n"), *Weapon->WeaponStance->StanceName);
    }
    else
    {
        Output += TEXT("  None assigned\n");
    }

    Output += TEXT("==========================================\n");

    return Output;
}

void UWeaponDataDebug::CompareWeapons(UWeaponData *Weapon1, UWeaponData *Weapon2)
{
    if (!Weapon1 || !Weapon2)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: One or both weapons are NULL"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== WEAPON COMPARISON =========="));
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Property"), *Weapon1->Name, *Weapon2->Name);
    UE_LOG(LogTemp, Display, TEXT("----------------------------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Type"), *Weapon1->GetWeaponTypeName(), *Weapon2->GetWeaponTypeName());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Attack"),
           Weapon1->WeaponAttack ? *Weapon1->WeaponAttack->Name : TEXT("None"),
           Weapon2->WeaponAttack ? *Weapon2->WeaponAttack->Name : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25d | %-25d"), TEXT("Abilities"), Weapon1->GetAbilityCount(), Weapon2->GetAbilityCount());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-25s | %-25s"), TEXT("Stance"),
           Weapon1->WeaponStance ? *Weapon1->WeaponStance->StanceName : TEXT("None"),
           Weapon2->WeaponStance ? *Weapon2->WeaponStance->StanceName : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}