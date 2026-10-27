// InfusionDisplayData.cpp

#include "Infusion/InfusionDisplayData.h"
#include "Debug/Infusion/InfusionDisplayDataDebug.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UInfusionDisplayData::PrintInfusionDisplayData()
{
    UInfusionDisplayDataDebug::LogInfusionDisplayStats(this);
}

#if WITH_EDITOR
EDataValidationResult UInfusionDisplayData::IsDataValid(FDataValidationContext &Context) const
{
    const EDataValidationResult Result = Super::IsDataValid(Context);

    if (!VFXSystem)
    {
        Context.AddWarning(FText::FromString(TEXT("No VFXSystem - the infusion shows nothing")));
    }
    if (DisplayName == GetDefault<UInfusionDisplayData>()->DisplayName)
    {
        Context.AddWarning(FText::FromString(TEXT("DisplayName is still the default")));
    }
    return Result;
}
#endif
