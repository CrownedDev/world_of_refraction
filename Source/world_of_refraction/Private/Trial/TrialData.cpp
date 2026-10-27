// TrialData.cpp

#include "Trial/TrialData.h"
#include "Character/CharacterData.h"
#include "WoRLog.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FString UTrialData::GetTrialDataString() const
{
    FString Output = FString::Printf(TEXT("TRIAL: %s (%s)\n"), *Name.ToString(), *GetName());
    Output += FString::Printf(TEXT("  Element: %s\n"), *UEnum::GetValueAsString(Element));
    Output += FString::Printf(TEXT("  Level: %s\n"), Level.IsNull() ? TEXT("(none)") : *Level.ToString());
    Output += FString::Printf(TEXT("  Encounter level: %s\n"),
                              EncounterLevel.IsNull() ? TEXT("(none)") : *EncounterLevel.ToString());
    Output += FString::Printf(TEXT("  Variations: %d\n"), Variations.Num());
    for (int32 Index = 0; Index < Variations.Num(); ++Index)
    {
        const FTrialVariation &Variation = Variations[Index];
        Output += FString::Printf(TEXT("    [%d] %s - boss %s\n"), Index, *Variation.Name.ToString(),
                                  Variation.Boss ? *Variation.Boss->GetName() : TEXT("(none)"));
    }
    return Output;
}

void UTrialData::PrintTrialData() const
{
    UE_LOG(LogWoR, Log, TEXT("%s"), *GetTrialDataString());
}

#if WITH_EDITOR
EDataValidationResult UTrialData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Level.IsNull())
    {
        Context.AddError(FText::FromString(TEXT("No Level set - the trial door would open nothing")));
        Result = EDataValidationResult::Invalid;
    }
    if (Name.IsEmpty())
    {
        Context.AddWarning(FText::FromString(TEXT("No Name - the door and run screens show a blank trial")));
    }
    if (EncounterLevel.IsNull())
    {
        Context.AddWarning(FText::FromString(TEXT("No EncounterLevel - encounters in this trial log and do nothing")));
    }
    for (int32 Index = 0; Index < Variations.Num(); ++Index)
    {
        if (Variations[Index].Name.IsEmpty())
        {
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("Variation %d has no Name"), Index)));
        }
        if (!Variations[Index].Boss)
        {
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("Variation %d has no Boss - the arena uses the fallback boss"), Index)));
        }
    }
    return Result;
}
#endif
