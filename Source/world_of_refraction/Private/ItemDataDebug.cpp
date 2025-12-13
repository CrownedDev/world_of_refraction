// ItemDataDebug.cpp
// Implementation of item system debug utilities

#include "ItemDataDebug.h"
#include "EvolutionData.h"

bool UItemDataDebug::ValidateAllItemCombinations()
{
    UE_LOG(LogTemp, Display, TEXT("========== VALIDATING ALL 70 ITEM COMBINATIONS =========="));

    bool bAllValid = true;
    int32 ValidCount = 0;
    int32 InvalidCount = 0;

    // Iterate all crystal types
    for (int32 CrystalIndex = 0; CrystalIndex <= static_cast<int32>(ECrystalType::Quartz); ++CrystalIndex)
    {
        ECrystalType Crystal = static_cast<ECrystalType>(CrystalIndex);

        // Iterate all tiers
        for (int32 TierIndex = 0; TierIndex <= static_cast<int32>(EItemTier::S_Tier); ++TierIndex)
        {
            EItemTier Tier = static_cast<EItemTier>(TierIndex);

            // Create test item
            UItemData *TestItem = CreateTestItem(Crystal, Tier);
            if (TestItem)
            {
                if (ValidateItem(TestItem))
                {
                    ValidCount++;
                }
                else
                {
                    InvalidCount++;
                    bAllValid = false;
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create test item: %s %s"),
                       *GetCrystalTypeName(Crystal), *GetTierName(Tier));
                InvalidCount++;
                bAllValid = false;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("=========================================================="));
    UE_LOG(LogTemp, Display, TEXT("VALIDATION COMPLETE: %d Valid, %d Invalid"), ValidCount, InvalidCount);
    UE_LOG(LogTemp, Display, TEXT("=========================================================="));

    return bAllValid;
}

bool UItemDataDebug::ValidateItem(const UItemData *Item)
{
    if (!Item)
    {
        UE_LOG(LogTemp, Error, TEXT("ValidateItem: Null item!"));
        return false;
    }

    bool bValid = true;
    TArray<FString> Errors;

    // Check name generated
    if (Item->GetFullItemName().IsEmpty())
    {
        Errors.Add(TEXT("Empty item name"));
        bValid = false;
    }

    // Check element is valid
    ESpellElement Element = Item->GetAssociatedElement();
    if (Element == ESpellElement::Generic && Item->CrystalType != ECrystalType::Quartz)
    {
        Errors.Add(TEXT("Unexpected Generic element"));
        bValid = false;
    }

    // Check tier bonuses are positive
    if (Item->GetGenericResistanceBonus() <= 0.0f)
    {
        Errors.Add(TEXT("Zero or negative Generic resistance"));
        bValid = false;
    }

    if (Item->GetBrokenDarknessEnergyBonus() <= 0)
    {
        Errors.Add(TEXT("Zero or negative BD energy bonus"));
        bValid = false;
    }

    // Crystal-specific validation
    switch (Item->CrystalType)
    {
    case ECrystalType::Garnet:
    case ECrystalType::Sapphire:
        if (Item->GetDamageValue() <= 0.0f)
        {
            Errors.Add(TEXT("Zero damage/healing value"));
            bValid = false;
        }
        break;

    case ECrystalType::Citrine:
        if (Item->GetEnergyValue() <= 0)
        {
            Errors.Add(TEXT("Zero energy value"));
            bValid = false;
        }
        break;

    case ECrystalType::Emerald:
    case ECrystalType::Amber:
    case ECrystalType::Opal:
        if (Item->GetBuffPercentage() <= 0.0f)
        {
            Errors.Add(TEXT("Zero buff percentage"));
            bValid = false;
        }
        if (Item->GetBuffDuration() <= 0)
        {
            Errors.Add(TEXT("Zero buff duration"));
            bValid = false;
        }
        break;

    case ECrystalType::Onyx:
        if (Item->GetSilencePercentage() <= 0.0f)
        {
            Errors.Add(TEXT("Zero silence percentage"));
            bValid = false;
        }
        if (Item->GetSilenceDuration() <= 0)
        {
            Errors.Add(TEXT("Zero silence duration"));
            bValid = false;
        }
        break;

    case ECrystalType::Iolite:
        // Debuffs to remove can be 0 (means all)
        // Just check immunity makes sense at higher tiers
        if (Item->Tier >= EItemTier::B_Tier && !Item->GetGrantsImmunity())
        {
            Errors.Add(TEXT("B+ tier should grant immunity"));
            bValid = false;
        }
        break;

    case ECrystalType::Quartz:
        if (Item->GetTransformThreshold() <= 0)
        {
            Errors.Add(TEXT("Zero transform threshold"));
            bValid = false;
        }
        break;

    case ECrystalType::Amethyst:
        // Gamble - no specific values to check
        break;

    case ECrystalType::EvolutionCrystal:
        // Evolution crystals must have valid Evolution reference
        if (!Item->GrantsEvolution())
        {
            Errors.Add(TEXT("EvolutionCrystal missing Evolution data reference"));
            bValid = false;
        }
        else
        {
            // Validate evolution has element
            if (Item->GetAssociatedElement() == ESpellElement::Generic)
            {
                Errors.Add(TEXT("Evolution has Generic element (should be specific)"));
                bValid = false;
            }

            // Warn if not refined (can't be slotted)
            if (!Item->CanBeSlotted())
            {
                Errors.Add(TEXT("EvolutionCrystal should be refined for weapon/ring slotting"));
                // Not invalid, just a warning scenario
            }
        }
        break;
    }

    // Log errors if any
    if (!bValid)
    {
        UE_LOG(LogTemp, Warning, TEXT("INVALID: %s"), *Item->GetFullItemName());
        for (const FString &Error : Errors)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Error);
        }
    }

    return bValid;
}

void UItemDataDebug::LogItemValues(const UItemData *Item)
{
    if (!Item)
    {
        UE_LOG(LogTemp, Error, TEXT("LogItemValues: Null item!"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== %s =========="), *Item->GetFullItemName());
    UE_LOG(LogTemp, Display, TEXT("Crystal Type: %s"), *GetCrystalTypeName(Item->CrystalType));
    UE_LOG(LogTemp, Display, TEXT("Tier: %s"), *Item->GetTierName());
    UE_LOG(LogTemp, Display, TEXT("Element: %d"), static_cast<int32>(Item->GetAssociatedElement()));
    UE_LOG(LogTemp, Display, TEXT("Effect Type: %d"), static_cast<int32>(Item->GetPrimaryEffectType()));
    UE_LOG(LogTemp, Display, TEXT(""));

    // ADD: Crystal state
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("--- Crystal State ---"));
    UE_LOG(LogTemp, Display, TEXT("Category: %s"), Item->GrantsEvolution() ? TEXT("Evolution") : (Item->CanBeSlotted() ? TEXT("Refined") : TEXT("Item")));

    // ADD: Evolution details if applicable
    if (Item->CrystalType == ECrystalType::EvolutionCrystal)
    {
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("--- Evolution Details ---"));
        if (Item->GrantsEvolution())
        {
            UE_LOG(LogTemp, Display, TEXT("Evolution Name: %s"), *Item->ItemName);
            UE_LOG(LogTemp, Display, TEXT("Evolution Element: %s"), *UEnum::GetValueAsString(Item->GetAssociatedElement()));
            UE_LOG(LogTemp, Display, TEXT("Locked Spells: %d"), Item->GetLockedSpellCount());
            UE_LOG(LogTemp, Display, TEXT("Total Spells: %d"), Item->GetSpells().Num());
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Evolution: NOT ASSIGNED (INVALID)"));
        }
    }

    // Effect values
    UE_LOG(LogTemp, Display, TEXT("--- Effect Values ---"));
    UE_LOG(LogTemp, Display, TEXT("Damage/Healing: %.1f"), Item->GetDamageValue());
    UE_LOG(LogTemp, Display, TEXT("Energy Value: %d"), Item->GetEnergyValue());
    UE_LOG(LogTemp, Display, TEXT("Self Damage: %d"), Item->GetSelfDamage());
    UE_LOG(LogTemp, Display, TEXT("Buff %%: %.1f"), Item->GetBuffPercentage());
    UE_LOG(LogTemp, Display, TEXT("Buff Duration: %d turns"), Item->GetBuffDuration());
    UE_LOG(LogTemp, Display, TEXT("Silence %%: %.1f"), Item->GetSilencePercentage());
    UE_LOG(LogTemp, Display, TEXT("Silence Duration: %d turns"), Item->GetSilenceDuration());
    UE_LOG(LogTemp, Display, TEXT("Debuffs Removed: %d (0=all)"), Item->GetDebuffsToRemove());
    UE_LOG(LogTemp, Display, TEXT("Grants Immunity: %s"), Item->GetGrantsImmunity() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Display, TEXT("Immunity Duration: %d turns"), Item->GetImmunityDuration());
    UE_LOG(LogTemp, Display, TEXT("Transform Threshold: %d"), Item->GetTransformThreshold());
    UE_LOG(LogTemp, Display, TEXT(""));

    // Secondary effects
    UE_LOG(LogTemp, Display, TEXT("--- Secondary Effects ---"));
    UE_LOG(LogTemp, Display, TEXT("Has Secondary: %s"), Item->HasSecondaryEffect() ? TEXT("Yes") : TEXT("No"));
    if (Item->HasSecondaryEffect())
    {
        UE_LOG(LogTemp, Display, TEXT("Secondary Damage: %d/turn"), Item->GetSecondaryDamagePerTurn());
        UE_LOG(LogTemp, Display, TEXT("Secondary Duration: %d turns"), Item->GetSecondaryDuration());
    }
    UE_LOG(LogTemp, Display, TEXT(""));

    // Bonuses
    UE_LOG(LogTemp, Display, TEXT("--- Tier Bonuses ---"));
    UE_LOG(LogTemp, Display, TEXT("Generic Resistance: %.1f%% for %d turns"),
           Item->GetGenericResistanceBonus(), Item->GetGenericResistanceDuration());
    UE_LOG(LogTemp, Display, TEXT("BD Energy Bonus: +%d"), Item->GetBrokenDarknessEnergyBonus());
    UE_LOG(LogTemp, Display, TEXT(""));
}

void UItemDataDebug::LogCrystalTierProgression(ECrystalType CrystalType)
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== %s TIER PROGRESSION =========="), *GetCrystalTypeName(CrystalType));
    UE_LOG(LogTemp, Display, TEXT(""));

    for (int32 TierIndex = 0; TierIndex <= static_cast<int32>(EItemTier::S_Tier); ++TierIndex)
    {
        EItemTier Tier = static_cast<EItemTier>(TierIndex);
        UItemData *TestItem = CreateTestItem(CrystalType, Tier);

        if (TestItem)
        {
            FString ValueStr;

            switch (CrystalType)
            {
            case ECrystalType::Garnet:
                ValueStr = FString::Printf(TEXT("%.0f damage"), TestItem->GetDamageValue());
                if (TestItem->HasSecondaryEffect())
                {
                    ValueStr += FString::Printf(TEXT(" + %d burn/turn x%d"),
                                                TestItem->GetSecondaryDamagePerTurn(), TestItem->GetSecondaryDuration());
                }
                break;

            case ECrystalType::Sapphire:
                ValueStr = FString::Printf(TEXT("%.0f healing"), TestItem->GetDamageValue());
                break;

            case ECrystalType::Citrine:
                ValueStr = FString::Printf(TEXT("+%d energy (-%d HP)"),
                                           TestItem->GetEnergyValue(), TestItem->GetSelfDamage());
                break;

            case ECrystalType::Emerald:
                ValueStr = FString::Printf(TEXT("+%.0f%% speed for %d turns"),
                                           TestItem->GetBuffPercentage(), TestItem->GetBuffDuration());
                break;

            case ECrystalType::Amber:
                ValueStr = FString::Printf(TEXT("-%.0f%% damage taken for %d turns"),
                                           TestItem->GetBuffPercentage(), TestItem->GetBuffDuration());
                break;

            case ECrystalType::Opal:
                ValueStr = FString::Printf(TEXT("+%.0f%% crit for %d turns"),
                                           TestItem->GetBuffPercentage(), TestItem->GetBuffDuration());
                if (TestItem->GetRevealsHP())
                {
                    ValueStr += TEXT(" + reveals");
                }
                break;

            case ECrystalType::Onyx:
                ValueStr = FString::Printf(TEXT("%.0f%% energy locked for %d turns"),
                                           TestItem->GetSilencePercentage(), TestItem->GetSilenceDuration());
                break;

            case ECrystalType::Amethyst:
                ValueStr = TEXT("Random effect");
                break;

            case ECrystalType::Iolite:
                if (TestItem->GetDebuffsToRemove() == 0)
                {
                    ValueStr = TEXT("Remove ALL debuffs");
                }
                else
                {
                    ValueStr = FString::Printf(TEXT("Remove %d debuffs"), TestItem->GetDebuffsToRemove());
                }
                if (TestItem->GetGrantsImmunity())
                {
                    ValueStr += FString::Printf(TEXT(" + %d turn immunity"), TestItem->GetImmunityDuration());
                }
                break;

            case ECrystalType::Quartz:
                ValueStr = FString::Printf(TEXT("%d absorption threshold"), TestItem->GetTransformThreshold());
                break;
            }

            UE_LOG(LogTemp, Display, TEXT("  %s: %s"), *GetTierName(Tier), *ValueStr);
        }
    }

    UE_LOG(LogTemp, Display, TEXT(""));
}

void UItemDataDebug::LogCrystalState(const UItemData *Item)
{
    if (!Item)
    {
        UE_LOG(LogTemp, Error, TEXT("LogCrystalState: Null item!"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("===== CRYSTAL STATE: %s ====="), *Item->GetFullItemName());
    UE_LOG(LogTemp, Display, TEXT("Type: %s"), *GetCrystalTypeName(Item->CrystalType));
    UE_LOG(LogTemp, Display, TEXT("Tier: %s"), *Item->GetTierName());
    UE_LOG(LogTemp, Display, TEXT("Slottable: %s"), Item->CanBeSlotted() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Display, TEXT(""));

    if (Item->CanBeSlotted())
    {
        UE_LOG(LogTemp, Display, TEXT("Usage: Can be slotted into Weapons or Rings"));
        UE_LOG(LogTemp, Display, TEXT("Element Provided: %s"), *UEnum::GetValueAsString(Item->GetAssociatedElement()));

        if (Item->CrystalType == ECrystalType::EvolutionCrystal)
        {
            UE_LOG(LogTemp, Display, TEXT("Special: EVOLUTION CRYSTAL"));
            if (Item->GrantsEvolution())
            {
                UE_LOG(LogTemp, Display, TEXT("  Evolution: %s"), *Item->ItemName);
                UE_LOG(LogTemp, Display, TEXT("  Spells: %d (Locked: %d)"), Item->GetSpells().Num(), Item->GetLockedSpellCount());
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("  WARNING: No Evolution assigned!"));
            }
        }
        else if (Item->CrystalType == ECrystalType::Quartz)
        {
            UE_LOG(LogTemp, Display, TEXT("Special: QUARTZ (absorbs element from attacks)"));
            UE_LOG(LogTemp, Display, TEXT("  Transform Threshold: %d damage"), Item->GetTransformThreshold());
        }
        else if (Item->CrystalType == ECrystalType::Iolite)
        {
            UE_LOG(LogTemp, Display, TEXT("Special: IOLITE (Reality element, physical enhancement)"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Usage: Consumable in combat"));
        UE_LOG(LogTemp, Display, TEXT("Effect: %s"), *Item->Description);
    }

    UE_LOG(LogTemp, Display, TEXT("====================================="));
}

void UItemDataDebug::LogAllItems()
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("##########################################"));
    UE_LOG(LogTemp, Display, TEXT("#       COMPLETE ITEM MATRIX (70)        #"));
    UE_LOG(LogTemp, Display, TEXT("##########################################"));

    for (int32 CrystalIndex = 0; CrystalIndex <= static_cast<int32>(ECrystalType::Quartz); ++CrystalIndex)
    {
        ECrystalType Crystal = static_cast<ECrystalType>(CrystalIndex);
        LogCrystalTierProgression(Crystal);
    }

    UE_LOG(LogTemp, Display, TEXT("##########################################"));
    UE_LOG(LogTemp, Display, TEXT("#            END ITEM MATRIX             #"));
    UE_LOG(LogTemp, Display, TEXT("##########################################"));
}

void UItemDataDebug::LogTierBonuses()
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== TIER-BASED BONUSES =========="));
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("--- Generic Character Resistance Bonus ---"));

    for (int32 TierIndex = 0; TierIndex <= static_cast<int32>(EItemTier::S_Tier); ++TierIndex)
    {
        EItemTier Tier = static_cast<EItemTier>(TierIndex);
        UItemData *TestItem = CreateTestItem(ECrystalType::Garnet, Tier);

        if (TestItem)
        {
            UE_LOG(LogTemp, Display, TEXT("  %s: %.0f%% resistance for %d turns"),
                   *GetTierName(Tier),
                   TestItem->GetGenericResistanceBonus(),
                   TestItem->GetGenericResistanceDuration());
        }
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("--- Broken Darkness Energy Bonus ---"));

    for (int32 TierIndex = 0; TierIndex <= static_cast<int32>(EItemTier::S_Tier); ++TierIndex)
    {
        EItemTier Tier = static_cast<EItemTier>(TierIndex);
        UItemData *TestItem = CreateTestItem(ECrystalType::Garnet, Tier);

        if (TestItem)
        {
            UE_LOG(LogTemp, Display, TEXT("  %s: +%d energy"),
                   *GetTierName(Tier),
                   TestItem->GetBrokenDarknessEnergyBonus());
        }
    }

    UE_LOG(LogTemp, Display, TEXT(""));
}

UItemData *UItemDataDebug::CreateTestItem(ECrystalType CrystalType, EItemTier Tier)
{
    UItemData *TestItem = NewObject<UItemData>();
    if (TestItem)
    {
        TestItem->CrystalType = CrystalType;
        TestItem->Tier = Tier;

        // Manually trigger what PostEditChangeProperty does
        TestItem->ItemName = TestItem->GetFullItemName();
    }
    return TestItem;
}

bool UItemDataDebug::TestTierProgression(ECrystalType CrystalType)
{
    UE_LOG(LogTemp, Display, TEXT("Testing tier progression for %s..."), *GetCrystalTypeName(CrystalType));

    float PreviousValue = 0.0f;
    bool bValid = true;

    for (int32 TierIndex = 0; TierIndex <= static_cast<int32>(EItemTier::S_Tier); ++TierIndex)
    {
        EItemTier Tier = static_cast<EItemTier>(TierIndex);
        UItemData *TestItem = CreateTestItem(CrystalType, Tier);

        if (TestItem)
        {
            float CurrentValue = GetPrimaryValue(TestItem);

            // Skip Amethyst (random) and check that values increase
            if (CrystalType != ECrystalType::Amethyst && TierIndex > 0)
            {
                if (CurrentValue < PreviousValue)
                {
                    UE_LOG(LogTemp, Warning, TEXT("  FAIL: %s (%.1f) < previous tier (%.1f)"),
                           *GetTierName(Tier), CurrentValue, PreviousValue);
                    bValid = false;
                }
            }

            PreviousValue = CurrentValue;
        }
    }

    if (bValid)
    {
        UE_LOG(LogTemp, Display, TEXT("  PASS: All tiers progress correctly"));
    }

    return bValid;
}

bool UItemDataDebug::RunAllTests()
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("##################################################"));
    UE_LOG(LogTemp, Display, TEXT("#          RUNNING ALL ITEM SYSTEM TESTS         #"));
    UE_LOG(LogTemp, Display, TEXT("##################################################"));
    UE_LOG(LogTemp, Display, TEXT(""));

    bool bAllPassed = true;

    // Test 1: Validate all items
    UE_LOG(LogTemp, Display, TEXT("=== TEST 1: Validate All Item Combinations ==="));
    if (!ValidateAllItemCombinations())
    {
        bAllPassed = false;
    }

    // Test 2: Tier progression for each crystal
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("=== TEST 2: Tier Progression ==="));
    for (int32 CrystalIndex = 0; CrystalIndex <= static_cast<int32>(ECrystalType::Quartz); ++CrystalIndex)
    {
        ECrystalType Crystal = static_cast<ECrystalType>(CrystalIndex);
        if (!TestTierProgression(Crystal))
        {
            bAllPassed = false;
        }
    }

    // Log tier bonuses for reference
    LogTierBonuses();

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("##################################################"));
    if (bAllPassed)
    {
        UE_LOG(LogTemp, Display, TEXT("#              ALL TESTS PASSED!                 #"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("#              SOME TESTS FAILED!                #"));
    }
    UE_LOG(LogTemp, Display, TEXT("##################################################"));
    UE_LOG(LogTemp, Display, TEXT(""));

    return bAllPassed;
}

FString UItemDataDebug::GetCrystalTypeName(ECrystalType CrystalType)
{
    switch (CrystalType)
    {
    case ECrystalType::Garnet:
        return TEXT("Garnet");
    case ECrystalType::Sapphire:
        return TEXT("Sapphire");
    case ECrystalType::Citrine:
        return TEXT("Citrine");
    case ECrystalType::Emerald:
        return TEXT("Emerald");
    case ECrystalType::Amber:
        return TEXT("Amber");
    case ECrystalType::Opal:
        return TEXT("Opal");
    case ECrystalType::Onyx:
        return TEXT("Onyx");
    case ECrystalType::Amethyst:
        return TEXT("Amethyst");
    case ECrystalType::Iolite:
        return TEXT("Iolite");
    case ECrystalType::Quartz:
        return TEXT("Quartz");
    case ECrystalType::EvolutionCrystal:
        return TEXT("Evolution");
    default:
        return TEXT("Unknown");
    }
}

FString UItemDataDebug::GetTierName(EItemTier Tier)
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return TEXT("F");
    case EItemTier::E_Tier:
        return TEXT("E");
    case EItemTier::D_Tier:
        return TEXT("D");
    case EItemTier::C_Tier:
        return TEXT("C");
    case EItemTier::B_Tier:
        return TEXT("B");
    case EItemTier::A_Tier:
        return TEXT("A");
    case EItemTier::S_Tier:
        return TEXT("S");
    default:
        return TEXT("?");
    }
}

float UItemDataDebug::GetPrimaryValue(const UItemData *Item)
{
    if (!Item)
        return 0.0f;

    switch (Item->CrystalType)
    {
    case ECrystalType::Garnet:
    case ECrystalType::Sapphire:
        return Item->GetDamageValue();

    case ECrystalType::Citrine:
        return static_cast<float>(Item->GetEnergyValue());

    case ECrystalType::Emerald:
    case ECrystalType::Amber:
    case ECrystalType::Opal:
        return Item->GetBuffPercentage();

    case ECrystalType::Onyx:
        return Item->GetSilencePercentage();

    case ECrystalType::Amethyst:
        return static_cast<float>(Item->Tier); // Just tier index for gamble

    case ECrystalType::Iolite:
        // Higher tier = more debuffs OR immunity
        if (Item->GetGrantsImmunity())
        {
            return 100.0f + Item->GetImmunityDuration(); // Immunity is better
        }
        return static_cast<float>(Item->GetDebuffsToRemove());

    case ECrystalType::Quartz:
        return static_cast<float>(Item->GetTransformThreshold());
    case ECrystalType::EvolutionCrystal:
        // Evolution crystals don't have standard progression values
        return 0.0f;

    default:
        return 0.0f;
    }
}