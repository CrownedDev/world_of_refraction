// RequirementGapDebug.cpp

#include "Debug/Combat/RequirementGapDebug.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Tuning/CombatTuningData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Engine/Engine.h"

namespace
{
    /** B-326 (D-119): an unmet skill ignores the percents above; combat uses the flat factor and no scaling. */
    FString UnmetLine(const FWorldStatRequirements &Requirements, const FWorldPillarLevels &Levels)
    {
        return Requirements.IsMetBy(Levels)
                   ? FString(TEXT("  Requirement met: the percents above apply\n"))
                   : FString::Printf(TEXT("  UNMET (D-119): skill part x%.2f, no pillar scaling; the percents above don't apply\n"),
                                     WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR);
    }

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
    Output += UnmetLine(Skill->Requirements, FWorldPillarLevels::FromAsset(*Character));
    // The pillar% IS the substat delta: it applies to EVERY substat in that pillar
    // (membership lives inline in FActionStatModifiers::AddPillarPercent — not cleanly
    // enumerable from code, so the names are intentionally not listed here).
    Output += TEXT("  Pillar% = delta applied to every substat in that pillar\n");
    Output += TEXT("  (gap -ve = over-statted -> boost; under on any pillar -> the flat D-119 penalty instead)\n");
    Output += TEXT("===================================\n");
    return Output;
}

FString URequirementGapDebug::GetLiveRequirementGapString(const USkillDataBase *Skill, const UCharacterDataComponent *Combatant)
{
    if (!Skill || !Combatant)
    {
        return TEXT("ERROR: Invalid Skill or Combatant");
    }

    // The same call ComputeActionStatModifiers makes, so the readout matches combat.
    const FWorldPillarLevels Levels = FWorldPillarLevels::FromComponent(*Combatant);
    float MindPct = 0.0f, BodyPct = 0.0f, SpiritPct = 0.0f;
    Skill->Requirements.GetRequirementGapPillarPercents(Levels, MindPct, BodyPct, SpiritPct);

    FString Output = FString::Printf(TEXT("REQUIREMENT GAP (live run levels): %s\n"), *Skill->Name);
    Output += FormatPillarLine(TEXT("Mind"), Skill->Requirements.RequiredWorldMind, Levels.Mind, MindPct);
    Output += FormatPillarLine(TEXT("Body"), Skill->Requirements.RequiredWorldBody, Levels.Body, BodyPct);
    Output += FormatPillarLine(TEXT("Spirit"), Skill->Requirements.RequiredWorldSpirit, Levels.Spirit, SpiritPct);
    Output += UnmetLine(Skill->Requirements, Levels);
    return Output;
}

FString URequirementGapDebug::GetWeaponRequirementFactorString(const UWeaponData *Weapon, const UCharacterData *Character)
{
    if (!Weapon || !Character)
    {
        return TEXT("ERROR: Invalid Weapon or Character Data");
    }

    const FWorldStatRequirements &Req = Weapon->Requirements;
    // B-326 (D-119): each pillar is met or short; any shortfall gives the one flat penalty.
    auto PillarLine = [](const TCHAR *PillarName, int32 Required, int32 CharLevel)
    {
        const int32 Short = Required - CharLevel;
        return Short > 0 ? FString::Printf(TEXT("  %-7s Req %d  Char %d  short by %d\n"), PillarName, Required, CharLevel, Short)
                         : FString::Printf(TEXT("  %-7s Req %d  Char %d  met\n"), PillarName, Required, CharLevel);
    };

    FString Output = FString::Printf(TEXT("WEAPON REQUIREMENT: %s (CHARACTER: %s)\n"), *Weapon->GetName(), *Character->Name);
    Output += PillarLine(TEXT("Mind"), Req.RequiredWorldMind, Character->WorldMindLevel);
    Output += PillarLine(TEXT("Body"), Req.RequiredWorldBody, Character->WorldBodyLevel);
    Output += PillarLine(TEXT("Spirit"), Req.RequiredWorldSpirit, Character->WorldSpiritLevel);
    const float Factor = Req.GetWeaponRequirementFactor(Character);
    Output += FString::Printf(TEXT("  Factor: x%.2f on the weapon part of attacks and abilities (B-220)%s\n"), Factor,
                              WeaponRequirement::IsUnmetFactor(Factor) ? TEXT(", unmet: no pillar scaling (D-119)") : TEXT(""));
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
