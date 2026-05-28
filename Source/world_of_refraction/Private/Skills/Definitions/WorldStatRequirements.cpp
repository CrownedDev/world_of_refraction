// WorldStatRequirements.cpp

#include "Skills/Definitions/WorldStatRequirements.h"
#include "Character/CharacterData.h"

bool FWorldStatRequirements::MeetsRequirements(const UCharacterData *Character) const
{
    if (!Character)
    {
        return false;
    }

    return Character->WorldMindLevel >= RequiredWorldMind &&
           Character->WorldBodyLevel >= RequiredWorldBody &&
           Character->WorldSpiritLevel >= RequiredWorldSpirit;
}

FString FWorldStatRequirements::GetRequirementsSummary(const UCharacterData *Character) const
{
    if (!HasRequirements())
    {
        return TEXT("No requirements");
    }

    FString Summary = TEXT("");

    if (RequiredWorldMind > 0)
    {
        int32 CharWorldMind = Character ? Character->WorldMindLevel : 0;
        FString Status = (Character && CharWorldMind >= RequiredWorldMind) ? TEXT("✓") : FString::Printf(TEXT("✗ -%d"), RequiredWorldMind - CharWorldMind);
        Summary += FString::Printf(TEXT("Mind Level: %d/%d %s\n"),
                                   CharWorldMind, RequiredWorldMind, *Status);
    }

    if (RequiredWorldBody > 0)
    {
        int32 CharWorldBody = Character ? Character->WorldBodyLevel : 0;
        FString Status = (Character && CharWorldBody >= RequiredWorldBody) ? TEXT("✓") : FString::Printf(TEXT("✗ -%d"), RequiredWorldBody - CharWorldBody);
        Summary += FString::Printf(TEXT("Body Level: %d/%d %s\n"),
                                   CharWorldBody, RequiredWorldBody, *Status);
    }

    if (RequiredWorldSpirit > 0)
    {
        int32 CharWorldSpirit = Character ? Character->WorldSpiritLevel : 0;
        FString Status = (Character && CharWorldSpirit >= RequiredWorldSpirit) ? TEXT("✓") : FString::Printf(TEXT("✗ -%d"), RequiredWorldSpirit - CharWorldSpirit);
        Summary += FString::Printf(TEXT("Spirit Level: %d/%d %s\n"),
                                   CharWorldSpirit, RequiredWorldSpirit, *Status);
    }

    return Summary;
}

FString FWorldStatRequirements::GetSimpleString() const
{
    if (!HasRequirements())
    {
        return TEXT("None");
    }

    TArray<FString> Reqs;

    if (RequiredWorldMind > 0)
    {
        Reqs.Add(FString::Printf(TEXT("Mind %d"), RequiredWorldMind));
    }

    if (RequiredWorldBody > 0)
    {
        Reqs.Add(FString::Printf(TEXT("Body %d"), RequiredWorldBody));
    }

    if (RequiredWorldSpirit > 0)
    {
        Reqs.Add(FString::Printf(TEXT("Spirit %d"), RequiredWorldSpirit));
    }

    return FString::Join(Reqs, TEXT(", "));
}

int32 FWorldStatRequirements::GetTotalDeficit(const UCharacterData *Character) const
{
    if (!Character)
        return 0;

    int32 MindDeficit = FMath::Max(0, RequiredWorldMind - Character->WorldMindLevel);
    int32 BodyDeficit = FMath::Max(0, RequiredWorldBody - Character->WorldBodyLevel);
    int32 SpiritDeficit = FMath::Max(0, RequiredWorldSpirit - Character->WorldSpiritLevel);

    return MindDeficit + BodyDeficit + SpiritDeficit;
}

float FWorldStatRequirements::CalculatePenalty(const UCharacterData *Character) const
{
    if (!Character)
        return 0.0f;

    int32 TotalDeficit = GetTotalDeficit(Character);
    if (TotalDeficit == 0)
        return 0.0f;

    // Penalty = sqrt(deficit) × 0.10, capped at 60%
    float Penalty = FMath::Sqrt(static_cast<float>(TotalDeficit)) * StatConstants::PENALTY_MULTIPLIER;
    return FMath::Min(Penalty, StatConstants::MAX_REQUIREMENT_PENALTY);
}