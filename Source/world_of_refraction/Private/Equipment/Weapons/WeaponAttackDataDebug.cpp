// WeaponAttackDataDebug.cpp
// Debug utilities implementation

#include "Equipment/Weapons/WeaponAttackDataDebug.h"
#include "Equipment/Weapons/WeaponAttackData.h"
#include "Engine/Engine.h"

void UWeaponAttackDataDebug::PrintAttackStats(UWeaponAttackData *Attack, float Duration)
{
    if (!Attack)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Attack is NULL"));
        }
        return;
    }

    FString Output = GetAttackStatsString(Attack);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Cyan, Output);
    }
}

void UWeaponAttackDataDebug::LogAttackStats(UWeaponAttackData *Attack)
{
    if (!Attack)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Attack is NULL"));
        return;
    }

    FString Output = GetAttackStatsString(Attack);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
}

FString UWeaponAttackDataDebug::GetAttackStatsString(UWeaponAttackData *Attack)
{
    if (!Attack)
    {
        return TEXT("ERROR: Attack is NULL");
    }

    FString Output;

    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("ATTACK: %s [%s]\n"), *Attack->Name, *Attack->GetTierString());
    Output += TEXT("==========================================\n");

    if (!Attack->Description.IsEmpty())
    {
        Output += FString::Printf(TEXT("Description: %s\n"), *Attack->Description);
    }

    // Targeting
    const UEnum *TargetEnum = StaticEnum<ETargetType>();
    const FString TargetTypeName = TargetEnum
                                       ? TargetEnum->GetNameStringByValue(static_cast<int64>(Attack->TargetType))
                                       : TEXT("Unknown");
    Output += FString::Printf(TEXT("Target: %s\n"), *TargetTypeName);

    // Combat
    Output += TEXT("\nCOMBAT:\n");
    Output += FString::Printf(TEXT("  Base Damage:    %d\n"), Attack->BaseDamage);
    Output += FString::Printf(TEXT("  Energy Cost:    %d\n"), Attack->BaseEnergyCost);
    // Per-hit distribution is authored via DamageSplit on the base (D1) —
    // inspect it with UDamageSplitDebug::PrintDamageSplitTable.
    Output += FString::Printf(TEXT("  Hits: %d\n"), Attack->HitCount);
    Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Attack->StatusBuildup);
    Output += FString::Printf(TEXT("  Raw Mode:       %s\n"), Attack->bIsRawMode ? TEXT("Yes") : TEXT("No"));
    Output += FString::Printf(TEXT("  Immune Infuse:  %s\n"), Attack->bImmuneToInfusion ? TEXT("Yes") : TEXT("No"));

    // Requirements
    Output += TEXT("\nREQUIREMENTS:\n");
    if (Attack->Requirements.HasRequirements())
    {
        Output += Attack->Requirements.GetSimpleString();
        Output += TEXT("\n");
    }
    else
    {
        Output += TEXT("  None\n");
    }

    // Animation
    Output += TEXT("\nANIMATION:\n");
    Output += FString::Printf(TEXT("  Base Speed: %.2fx\n"), Attack->BaseAnimSpeed);
    Output += FString::Printf(TEXT("  Montage: %s\n"),
                              Attack->AttackMontage ? *Attack->AttackMontage->GetName() : TEXT("None"));

    Output += TEXT("==========================================\n");

    return Output;
}

void UWeaponAttackDataDebug::CompareAttacks(UWeaponAttackData *Attack1, UWeaponAttackData *Attack2)
{
    if (!Attack1 || !Attack2)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: One or both attacks are NULL"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== ATTACK COMPARISON =========="));
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20s | %-20s"), TEXT("Property"), *Attack1->Name, *Attack2->Name);
    UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20s | %-20s"), TEXT("Tier"), *Attack1->GetTierString(), *Attack2->GetTierString());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20d | %-20d"), TEXT("Base Damage"), Attack1->BaseDamage, Attack2->BaseDamage);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20d | %-20d"), TEXT("Energy Cost"), Attack1->BaseEnergyCost, Attack2->BaseEnergyCost);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20d | %-20d"), TEXT("Hit Count"), Attack1->HitCount, Attack2->HitCount);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20d | %-20d"), TEXT("Status Buildup"), Attack1->StatusBuildup, Attack2->StatusBuildup);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.2fx | %-20.2fx"), TEXT("Anim Speed"), Attack1->BaseAnimSpeed, Attack2->BaseAnimSpeed);
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}
