// RequirementGapDebug.cpp

#include "Debug/Combat/RequirementGapDebug.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Engine/Engine.h"

namespace
{
    // One formatted line per pillar: name, Req, Char, Gap, ladder mult, pillar%.
    // PillarPct is passed in from the LIVE helper (GetRequirementGapPillarPercents),
    // NOT recomputed — so the displayed percent matches exactly what AddPillarPercent
    // receives. Gap and Mult are shown alongside as the breakdown that produced it.
    FString FormatPillarLine(const TCHAR *PillarName, int32 Required, int32 CharLevel, float PillarPct)
    {
        const int32 Gap = Required - CharLevel;
        const float Mult = TierGapDamage::GetRequirementGapMultiplier(Gap);
        return FString::Printf(TEXT("  %-7s Req %d  Char %d  Gap %+d  ->  x%.2f  =  %+.1f%%\n"),
                               PillarName, Required, CharLevel, Gap, Mult, PillarPct);
    }
}

FString URequirementGapDebug::GetRequirementGapString(const USkillDataBase *Skill, const UCharacterData *Character)
{
    if (!Skill || !Character)
    {
        return TEXT("ERROR: Invalid Skill or Character Data");
    }

    // Live percents — the SAME path the runtime feeds into AddPillarPercent. Displayed
    // verbatim so the readout can't drift from actual behaviour.
    float MindPct = 0.0f, BodyPct = 0.0f, SpiritPct = 0.0f;
    Skill->GetRequirementGapPillarPercents(Character, MindPct, BodyPct, SpiritPct);

    FString Output;
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("REQUIREMENT GAP: %s\n"), *Skill->Name);
    Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->Name);
    Output += TEXT("===================================\n");
    Output += FormatPillarLine(TEXT("Mind"), Skill->Requirements.RequiredWorldMind, Character->WorldMindLevel, MindPct);
    Output += FormatPillarLine(TEXT("Body"), Skill->Requirements.RequiredWorldBody, Character->WorldBodyLevel, BodyPct);
    Output += FormatPillarLine(TEXT("Spirit"), Skill->Requirements.RequiredWorldSpirit, Character->WorldSpiritLevel, SpiritPct);
    Output += TEXT("===================================\n");
    // The pillar% IS the substat delta: it applies to EVERY substat in that pillar
    // (membership lives inline in FActionStatModifiers::AddPillarPercent — not cleanly
    // enumerable from code, so the names are intentionally not listed here).
    Output += TEXT("  Pillar% = delta applied to every substat in that pillar\n");
    Output += TEXT("  (gap +ve = under-statted -> penalty; -ve = over-statted -> boost)\n");
    Output += TEXT("===================================\n");
    return Output;
}

void URequirementGapDebug::PrintRequirementGap(const USkillDataBase *Skill, const UCharacterData *Character, float Duration)
{
    if (!Skill || !Character)
    {
        UE_LOG(LogTemp, Error, TEXT("[RequirementGapDebug] Skill or Character is NULL"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Skill or Character is NULL"));
        }
        return;
    }

    const FString Output = GetRequirementGapString(Skill, Character);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);

    if (GEngine)
    {
        // Reverse order so the lines read top-down on screen (mirrors USpellDataDebug).
        TArray<FString> Lines;
        Output.ParseIntoArray(Lines, TEXT("\n"));
        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Cyan, Lines[i]);
        }
    }
}
