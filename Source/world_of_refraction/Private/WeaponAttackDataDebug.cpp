// WeaponAttackDataDebug.cpp
// Debug utilities implementation

#include "WeaponAttackDataDebug.h"
#include "WeaponAttackData.h"
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
    Output += FString::Printf(TEXT("ATTACK: %s\n"), *Attack->AttackName);
    Output += TEXT("==========================================\n");

    if (!Attack->Description.IsEmpty())
    {
        Output += FString::Printf(TEXT("Description: %s\n"), *Attack->Description);
    }

    // Combat
    Output += TEXT("\nCOMBAT:\n");
    if (Attack->HitCount == 1)
    {
        Output += TEXT("  Hits: 1 (100%)\n");
    }
    else
    {
        Output += FString::Printf(TEXT("  Hits: %d (%.0f%% + %.0f%% = %.0f%%)\n"),
                                  Attack->HitCount,
                                  Attack->FirstHitPercent,
                                  Attack->SecondHitPercent,
                                  Attack->GetTotalDamagePercent());
    }
    Output += FString::Printf(TEXT("  Damage Type: %s\n"), *Attack->GetDamageTypeName());
    Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Attack->StatusBuildup);
    Output += FString::Printf(TEXT("  Infusion Cost: %.0f Energy\n"), Attack->InfusionEnergyCost);

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
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20s | %-20s"), TEXT("Property"), *Attack1->AttackName, *Attack2->AttackName);
    UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------"));
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20d | %-20d"), TEXT("Hit Count"), Attack1->HitCount, Attack2->HitCount);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20s | %-20s"), TEXT("Damage Type"), *Attack1->GetDamageTypeName(), *Attack2->GetDamageTypeName());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20d | %-20d"), TEXT("Status Buildup"), Attack1->StatusBuildup, Attack2->StatusBuildup);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.0f%% | %-20.0f%%"), TEXT("Total Damage"), Attack1->GetTotalDamagePercent(), Attack2->GetTotalDamagePercent());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.0f | %-20.0f"), TEXT("Infusion Cost"), Attack1->InfusionEnergyCost, Attack2->InfusionEnergyCost);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.2fx | %-20.2fx"), TEXT("Anim Speed"), Attack1->BaseAnimSpeed, Attack2->BaseAnimSpeed);
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}