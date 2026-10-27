// CosmeticsData.cpp
// Data-driven (no behaviour) - see CosmeticsData.h. Debug dump and editor validation only.

#include "Character/CosmeticsData.h"
#include "Animation/AnimMontage.h"
#include "Character/StanceData.h"
#include "Infusion/InfusionDisplayData.h"
#include "WoRLog.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace CosmeticsDebug
{
    FString AssetName(const UObject *Asset)
    {
        return Asset ? Asset->GetName() : TEXT("(none)");
    }
}

FString UCosmeticsData::GetCosmeticsDataString() const
{
    using CosmeticsDebug::AssetName;
    FString Output = FString::Printf(TEXT("COSMETICS: %s\n"), *GetName());
    Output += FString::Printf(TEXT("  Dodge left / right: %s / %s\n"), *AssetName(DodgeLeftMontage), *AssetName(DodgeRightMontage));
    Output += FString::Printf(TEXT("  Block: %s\n"), *AssetName(BlockMontage));
    Output += FString::Printf(TEXT("  Perfect Guard flash / sound: %s / %s\n"), *PerfectGuardFlashVFX.ToString(),
                              *PerfectGuardSound.ToString());
    Output += FString::Printf(TEXT("  Unarmed stance: %s\n"), *AssetName(UnarmedStance));
    Output += FString::Printf(TEXT("  Ring switch: %s\n"), *AssetName(RingSwitchMontage));
    Output += FString::Printf(TEXT("  Infusion display: %s\n"), *AssetName(InfusionDisplay));
    Output += FString::Printf(TEXT("  Item use self / target: %s / %s\n"), *AssetName(ItemUseSelfMontage),
                              *AssetName(ItemUseTargetMontage));
    Output += FString::Printf(TEXT("  Weather variant: %s\n"), *AssetName(EquippedWeatherVariant));
    return Output;
}

void UCosmeticsData::PrintCosmeticsData() const
{
    UE_LOG(LogWoR, Log, TEXT("%s"), *GetCosmeticsDataString());
}

#if WITH_EDITOR
EDataValidationResult UCosmeticsData::IsDataValid(FDataValidationContext &Context) const
{
    const EDataValidationResult Result = Super::IsDataValid(Context);

    const TPair<const UObject *, const TCHAR *> Expected[] = {
        {DodgeLeftMontage, TEXT("DodgeLeftMontage")},
        {DodgeRightMontage, TEXT("DodgeRightMontage")},
        {BlockMontage, TEXT("BlockMontage")},
        {UnarmedStance, TEXT("UnarmedStance")},
        {ItemUseSelfMontage, TEXT("ItemUseSelfMontage")},
        {ItemUseTargetMontage, TEXT("ItemUseTargetMontage")},
    };
    for (const TPair<const UObject *, const TCHAR *> &Field : Expected)
    {
        if (!Field.Key)
        {
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("%s not set - that action plays no animation"), Field.Value)));
        }
    }
    return Result;
}
#endif
