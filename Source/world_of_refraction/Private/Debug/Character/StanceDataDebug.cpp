// StanceDataDebug.cpp
// Debug utilities implementation

#include "Debug/Character/StanceDataDebug.h"
#include "Character/StanceData.h"

void UStanceDataDebug::LogStanceStats(UStanceData *Stance)
{
    if (!Stance)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Stance is NULL"));
        return;
    }

    FString Output = GetStanceStatsString(Stance);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
}

FString UStanceDataDebug::GetStanceStatsString(UStanceData *Stance)
{
    if (!Stance)
    {
        return TEXT("ERROR: Stance is NULL");
    }

    FString Output;

    Output += TEXT("==========================================\n");
    Output += FString::Printf(TEXT("STANCE: %s\n"), *Stance->StanceName);
    Output += TEXT("==========================================\n");

    if (!Stance->Description.IsEmpty())
    {
        Output += FString::Printf(TEXT("Description: %s\n"), *Stance->Description);
    }

    Output += FString::Printf(TEXT("Animation: %s\n"),
                              Stance->IdleAnimMontage ? *Stance->IdleAnimMontage->GetName() : TEXT("None"));

    Output += TEXT("==========================================\n");

    return Output;
}