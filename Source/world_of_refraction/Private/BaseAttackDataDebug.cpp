// BaseAttackDataDebug.cpp
// Debug utilities implementation

#include "BaseAttackDataDebug.h"
#include "BaseAttackData.h"
#include "Engine/Engine.h"

void UBaseAttackDataDebug::PrintAttackStats(UBaseAttackData* Attack, float Duration)
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

void UBaseAttackDataDebug::LogAttackStats(UBaseAttackData* Attack)
{
    if (!Attack)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Attack is NULL"));
        return;
    }

    FString Output = GetAttackStatsString(Attack);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
}

FString UBaseAttackDataDebug::GetAttackStatsString(UBaseAttackData* Attack)
{
    if (!Attack)
    {
        return TEXT("ERROR: Attack is NULL");
    }

    FString Output;

    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("ATTACK: %s\n"), *Attack->AttackName);
    Output += TEXT("==========================================\n");

    // Description
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
    Output += FString::Printf(TEXT("  Infusion Cost: %.0f Energy\n"), Attack->InfusionEnergyCost);

    // Animation
    Output += TEXT("\nANIMATION:\n");
    Output += FString::Printf(TEXT("  Base Speed: %.2fx\n"), Attack->BaseAnimSpeed);
    Output += FString::Printf(TEXT("  Montage: %s\n"),
        Attack->AttackMontage ? *Attack->AttackMontage->GetName() : TEXT("None"));

    Output += TEXT("==========================================\n");

    return Output;
}

void UBaseAttackDataDebug::CompareAttacks(UBaseAttackData* Attack1, UBaseAttackData* Attack2)
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
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.0f%% | %-20.0f%%"), TEXT("Total Damage"), Attack1->GetTotalDamagePercent(), Attack2->GetTotalDamagePercent());
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.0f | %-20.0f"), TEXT("Infusion Cost"), Attack1->InfusionEnergyCost, Attack2->InfusionEnergyCost);
    UE_LOG(LogTemp, Display, TEXT("%-20s | %-20.2fx | %-20.2fx"), TEXT("Anim Speed"), Attack1->BaseAnimSpeed, Attack2->BaseAnimSpeed);
    UE_LOG(LogTemp, Display, TEXT("=========================================="));
}