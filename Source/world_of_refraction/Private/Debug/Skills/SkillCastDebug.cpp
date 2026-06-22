// SkillCastDebug.cpp
// Debug utilities for the skill Cast array (D6)

#include "Debug/Skills/SkillCastDebug.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Engine/Engine.h"

void USkillCastDebug::PrintCastArray(const USkillDataBase *Skill, float Duration)
{
    if (!Skill)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Skill is NULL"));
        }
        return;
    }

    const FString CastString = GetCastArrayString(Skill);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *CastString);

    if (GEngine)
    {
        TArray<FString> Lines;
        CastString.ParseIntoArray(Lines, TEXT("\n"));
        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Cyan, Lines[i]);
        }
    }
}

FString USkillCastDebug::GetCastArrayString(const USkillDataBase *Skill)
{
    if (!Skill)
    {
        return TEXT("ERROR: Invalid Skill Data");
    }

    FString Output = FString::Printf(TEXT("CAST ARRAY: %s (%d entries)\n"),
                                     *Skill->GetName(), Skill->CastArray.Num());

    if (Skill->CastArray.IsEmpty())
    {
        Output += TEXT("  (empty — loose delivery fields authoritative)\n");
        return Output;
    }

    for (int32 i = 0; i < Skill->CastArray.Num(); ++i)
    {
        const FSkillCastEntry &Entry = Skill->CastArray[i];

        FString DeliveryName = UEnum::GetValueAsString(Entry.DeliveryType);
        DeliveryName.RemoveFromStart(TEXT("ESpellDeliveryType::"));

        const FString TrailName = Entry.Trail.IsNull() ? TEXT("None") : Entry.Trail.GetAssetName();

        FString Line = FString::Printf(
            TEXT("[%d] %s '%s' Size=%.2f VisualScale=%.2f Speed=%.1f Trail=%s Count=%d"),
            i, *DeliveryName, *Entry.Label, Entry.Size, Entry.VisualScale,
            Entry.ProjectileSpeed, *TrailName, Entry.Count);

        Output += Line + TEXT("\n");
    }

    return Output;
}
