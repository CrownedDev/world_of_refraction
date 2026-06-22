// DamageSplitDebug.cpp

#include "Debug/Combat/DamageSplitDebug.h"
#include "Skills/Definitions/SkillDataBase.h"

namespace
{
    bool IsHitAuthored(const USkillDataBase *Skill, int32 HitNumber)
    {
        for (const FDamageSplitEntry &Entry : Skill->DamageSplit)
        {
            if (Entry.HitNumber == HitNumber && Entry.Percent > 0.0f)
            {
                return true;
            }
        }
        return false;
    }
}

void UDamageSplitDebug::PrintDamageSplitTable(const USkillDataBase *Skill)
{
    if (!Skill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DamageSplitDebug] PrintDamageSplitTable: null skill"));
        return;
    }

    const TArray<float> Table = ResolveDamageSplit(Skill->HitCount, Skill->DamageSplit);

    UE_LOG(LogTemp, Display, TEXT("=== DamageSplit: %s (HitCount %d, %d authored entr%s) ==="),
           *Skill->Name, Skill->HitCount, Skill->DamageSplit.Num(),
           Skill->DamageSplit.Num() == 1 ? TEXT("y") : TEXT("ies"));

    for (int32 i = 0; i < Table.Num(); ++i)
    {
        UE_LOG(LogTemp, Display, TEXT("  Hit %d: %6.2f%% (%s)"),
               i + 1, Table[i],
               IsHitAuthored(Skill, i + 1) ? TEXT("authored") : TEXT("derived"));
    }
}

FString UDamageSplitDebug::GetDamageSplitString(const USkillDataBase *Skill)
{
    if (!Skill)
    {
        return TEXT("(null skill)");
    }

    const TArray<float> Table = ResolveDamageSplit(Skill->HitCount, Skill->DamageSplit);

    FString Result = FString::Printf(TEXT("%s [%d hits]:"), *Skill->Name, Table.Num());
    for (int32 i = 0; i < Table.Num(); ++i)
    {
        Result += FString::Printf(TEXT(" %.1f%%"), Table[i]);
        if (i < Table.Num() - 1)
        {
            Result += TEXT(" /");
        }
    }
    return Result;
}
