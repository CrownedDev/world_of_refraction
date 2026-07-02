// MerchantData.cpp

#include "Merchant/MerchantData.h"

#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
    const FName HubTagName(TEXT("Hub"));
    const FName TrialTagName(TEXT("Trial"));

    FString DescribeStockEntry(const FMerchantStockEntry &Entry)
    {
        if (Entry.IsAssetEntry())
        {
            return FString::Printf(TEXT("%s (%s) x%d"),
                                   *Entry.Asset->GetName(),
                                   *Entry.Asset->GetClass()->GetName(),
                                   Entry.Count);
        }
        if (Entry.IsCrystalEntry())
        {
            return FString::Printf(TEXT("%s %s x%d"),
                                   *UEnum::GetDisplayValueAsText(Entry.Crystal.Tier).ToString(),
                                   *UEnum::GetDisplayValueAsText(Entry.Crystal.Type).ToString(),
                                   Entry.Count);
        }
        return TEXT("<empty entry>");
    }

    /** The merchant map: which asset class each merchant type sells. */
    UClass *GetExpectedAssetClass(EMerchantType Type)
    {
        switch (Type)
        {
        case EMerchantType::Blacksmith:   return UWeaponData::StaticClass();
        case EMerchantType::Jeweler:      return URingData::StaticClass();
        case EMerchantType::CombatMaster: return UAbilityData::StaticClass();
        case EMerchantType::SpellShop:    return USpellData::StaticClass();
        case EMerchantType::Spiritualist: return UEvolutionItemData::StaticClass();
        default:                          return nullptr;
        }
    }

    /** Crystal-entry side of the map: Blacksmith owns augment stones,
     *  the Spiritualist owns gem crystals. No other merchant stocks FCrystalId goods. */
    bool CrystalBelongsToMerchant(EMerchantType Type, ECrystalType CrystalType)
    {
        switch (Type)
        {
        case EMerchantType::Blacksmith:   return CrystalTypeHelpers::IsAugmentStoneType(CrystalType);
        case EMerchantType::Spiritualist: return CrystalTypeHelpers::IsGemType(CrystalType);
        default:                          return false;
        }
    }
}

bool UMerchantData::AppearsInHub() const
{
    if (AvailabilityTags.IsEmpty())
    {
        return true;
    }
    const FGameplayTag HubTag = FGameplayTag::RequestGameplayTag(HubTagName, /*ErrorIfNotFound*/ false);
    return HubTag.IsValid() && AvailabilityTags.HasTag(HubTag);
}

bool UMerchantData::AppearsInTrial(FGameplayTag ContextTag) const
{
    // MatchesAny expands ContextTag's parents, so an authored Trial.Garnet
    // matches a Trial.Garnet.Floor1 context.
    return ContextTag.IsValid() && ContextTag.MatchesAny(AvailabilityTags);
}

FString UMerchantData::GetMerchantString() const
{
    FString Result = FString::Printf(TEXT("Merchant '%s' [%s]"),
                                     *GetName(),
                                     *UEnum::GetDisplayValueAsText(MerchantType).ToString());

    const FString Context = AvailabilityTags.IsEmpty()
                                ? TEXT("HUB only (default, no tags)")
                                : FString::Printf(TEXT("tags: %s%s"),
                                                  *AvailabilityTags.ToStringSimple(),
                                                  AppearsInHub() ? TEXT(" (hub + trial)") : TEXT(" (trial only)"));
    Result += FString::Printf(TEXT("\n  Availability: %s"), *Context);

    Result += FString::Printf(TEXT("\n  Stock (%d):"), Stock.Num());
    for (int32 Index = 0; Index < Stock.Num(); ++Index)
    {
        Result += FString::Printf(TEXT("\n    [%d] %s"), Index, *DescribeStockEntry(Stock[Index]));
    }
    return Result;
}

void UMerchantData::PrintMerchant()
{
    UE_LOG(LogTemp, Log, TEXT("%s"), *GetMerchantString());
}

void UMerchantData::ValidateStock()
{
    TArray<FString> Errors;
    TArray<FString> Warnings;
    CollectStockIssues(Errors, Warnings);

    for (const FString &Error : Errors)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] %s"), *GetName(), *Error);
    }
    for (const FString &Warning : Warnings)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] %s"), *GetName(), *Warning);
    }
    if (Errors.IsEmpty() && Warnings.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[%s] Stock valid — %d entries."), *GetName(), Stock.Num());
    }
}

void UMerchantData::CollectStockIssues(TArray<FString> &OutErrors, TArray<FString> &OutWarnings) const
{
    for (int32 Index = 0; Index < Stock.Num(); ++Index)
    {
        const FMerchantStockEntry &Entry = Stock[Index];

        if (Entry.Asset && Entry.Crystal.Type != ECrystalType::None)
        {
            OutErrors.Add(FString::Printf(
                TEXT("Stock[%d]: both Asset and Crystal are set — an entry identifies exactly one."), Index));
            continue;
        }
        if (!Entry.IsAssetEntry() && !Entry.IsCrystalEntry())
        {
            OutErrors.Add(FString::Printf(TEXT("Stock[%d]: empty — set Asset or Crystal."), Index));
            continue;
        }
        if (Entry.Count < 1)
        {
            OutErrors.Add(FString::Printf(TEXT("Stock[%d]: Count %d — must be >= 1."), Index, Entry.Count));
        }

        // Merchant-map mismatches are warnings, not errors — vendor tags may
        // deliberately override the automatic type->merchant mapping.
        if (Entry.IsAssetEntry())
        {
            const UClass *Expected = GetExpectedAssetClass(MerchantType);
            if (Expected && !Entry.Asset->IsA(Expected))
            {
                OutWarnings.Add(FString::Printf(
                    TEXT("Stock[%d]: %s is a %s — a %s normally sells %s."),
                    Index, *Entry.Asset->GetName(), *Entry.Asset->GetClass()->GetName(),
                    *UEnum::GetDisplayValueAsText(MerchantType).ToString(), *Expected->GetName()));
            }
        }
        else if (!CrystalBelongsToMerchant(MerchantType, Entry.Crystal.Type))
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Stock[%d]: %s — crystal stock normally belongs to the Blacksmith (stones) or Spiritualist (gems)."),
                Index, *DescribeStockEntry(Entry)));
        }
    }

    // Availability sanity: only Hub or Trial.* tags mean anything to the context filter.
    for (const FGameplayTag &Tag : AvailabilityTags)
    {
        const FString TagString = Tag.ToString();
        if (Tag.GetTagName() != HubTagName &&
            Tag.GetTagName() != TrialTagName &&
            !TagString.StartsWith(TEXT("Trial.")))
        {
            OutWarnings.Add(FString::Printf(
                TEXT("AvailabilityTags: '%s' is neither Hub nor Trial.* — the context filter ignores it."),
                *TagString));
        }
    }
}

#if WITH_EDITOR
EDataValidationResult UMerchantData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    TArray<FString> Errors;
    TArray<FString> Warnings;
    CollectStockIssues(Errors, Warnings);

    for (const FString &Error : Errors)
    {
        Context.AddError(FText::FromString(Error));
        Result = EDataValidationResult::Invalid;
    }
    for (const FString &Warning : Warnings)
    {
        Context.AddWarning(FText::FromString(Warning));
    }
    return Result;
}
#endif
