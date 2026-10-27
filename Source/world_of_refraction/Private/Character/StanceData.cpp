// StanceData.cpp

#include "Character/StanceData.h"
#include "Debug/Character/StanceDataDebug.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UStanceData::PrintStanceData()
{
    UStanceDataDebug::LogStanceStats(this);
}

#if WITH_EDITOR
EDataValidationResult UStanceData::IsDataValid(FDataValidationContext &Context) const
{
    const EDataValidationResult Result = Super::IsDataValid(Context);

    if (!IdleAnimMontage)
    {
        Context.AddWarning(FText::FromString(TEXT("No IdleAnimMontage - the character holds no idle pose")));
    }
    return Result;
}
#endif
