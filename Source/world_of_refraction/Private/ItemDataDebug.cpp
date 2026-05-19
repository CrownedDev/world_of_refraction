// ItemDataDebug.cpp
// Implementation of item system debug utilities

#include "ItemDataDebug.h"

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
    if (Item->GetBrokenDarknessEnergyBonus() <= 0)
    {
        Errors.Add(TEXT("Zero or negative BD energy bonus"));
        bValid = false;
    }

    // Crystal-specific validation
    switch (Item->CrystalType)
    {
    case ECrystalType::Garnet:
        if (Item->GetDOTDamagePercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero DOT damage percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Sapphire:
        if (Item->GetHealPercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero heal percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Citrine:
        if (Item->GetEPRestorePercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero EP restore percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Emerald:
        if (Item->GetSpeedBuffPercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero speed buff percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Amber:
        if (Item->GetBuffPercentage() <= 0.0f)
        {
            Errors.Add(TEXT("Zero buff percentage"));
            bValid = false;
        }
        break;

    case ECrystalType::Opal:
        if (Item->GetCritBuffPercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero crit buff percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Onyx:
        if (Item->GetSilencePercentage() <= 0.0f)
        {
            Errors.Add(TEXT("Zero silence percentage"));
            bValid = false;
        }
        break;

    case ECrystalType::Amethyst:
        if (Item->GetBuffChancePercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero buff chance percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Iolite:
        if (Item->GetEffectsToRemoveCount() <= 0)
        {
            Errors.Add(TEXT("Zero effects to remove count"));
            bValid = false;
        }
        break;

    case ECrystalType::Quartz:
        if (Item->bIsRefined || Item->bIsEvolutionCrystal)
        {
            Errors.Add(TEXT("Quartz must be consumable-only (not refined or evolution)"));
            bValid = false;
        }
        if (Item->GetStatusClearPercent() <= 0.0f)
        {
            Errors.Add(TEXT("Zero status clear percent"));
            bValid = false;
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
    if (Item->bIsEvolutionCrystal)
    {
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("--- Evolution Details ---"));
        if (Item->GrantsEvolution())
        {
            UE_LOG(LogTemp, Display, TEXT("Evolution Name: %s"), *Item->ItemName);
            UE_LOG(LogTemp, Display, TEXT("Evolution Element: %s"), *UEnum::GetValueAsString(Item->GetAssociatedElement()));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Evolution: NOT ASSIGNED (INVALID)"));
        }
    }

    // Effect values (Phase 2 percentage-based getters)
    UE_LOG(LogTemp, Display, TEXT("--- Effect Values ---"));
    UE_LOG(LogTemp, Display, TEXT("DOT Damage %%: %.1f for %d turns"), Item->GetDOTDamagePercent(), Item->GetDOTDuration());
    UE_LOG(LogTemp, Display, TEXT("Heal %%: %.1f"), Item->GetHealPercent());
    UE_LOG(LogTemp, Display, TEXT("EP Restore %%: %.1f"), Item->GetEPRestorePercent());
    UE_LOG(LogTemp, Display, TEXT("Speed Buff %%: %.1f"), Item->GetSpeedBuffPercent());
    UE_LOG(LogTemp, Display, TEXT("Buff %%: %.1f"), Item->GetBuffPercentage());
    UE_LOG(LogTemp, Display, TEXT("Crit Buff %%: %.1f"), Item->GetCritBuffPercent());
    UE_LOG(LogTemp, Display, TEXT("Crystal Buff Duration: %d turns"), Item->GetCrystalDuration());
    UE_LOG(LogTemp, Display, TEXT("Silence %%: %.1f (one-shot drain on use)"), Item->GetSilencePercentage());
    UE_LOG(LogTemp, Display, TEXT("Buff Chance %%: %.1f"), Item->GetBuffChancePercent());
    UE_LOG(LogTemp, Display, TEXT("Gamble Magnitude %%: %.1f for %d turns"), Item->GetGambleMagnitudePercent(), Item->GetGambleDuration());
    UE_LOG(LogTemp, Display, TEXT("Effects To Remove: %d (99=all)"), Item->GetEffectsToRemoveCount());
    UE_LOG(LogTemp, Display, TEXT("Status Clear %%: %.1f, Resistance: %d turns"), Item->GetStatusClearPercent(), Item->GetResistanceDuration());
    UE_LOG(LogTemp, Display, TEXT("Elemental Buildup %%: %.1f"), Item->GetElementalBuildupPercent());
    UE_LOG(LogTemp, Display, TEXT("Reveals HP/Stats: %s"), Item->GetRevealsHP() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // Bonuses
    UE_LOG(LogTemp, Display, TEXT("--- Tier Bonuses ---"));
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
                ValueStr = FString::Printf(TEXT("%.0f%% burn/turn x%d turns"),
                                           TestItem->GetDOTDamagePercent(), TestItem->GetDOTDuration());
                break;

            case ECrystalType::Sapphire:
                ValueStr = FString::Printf(TEXT("%.0f%% HP healed"), TestItem->GetHealPercent());
                break;

            case ECrystalType::Citrine:
                ValueStr = FString::Printf(TEXT("+%.0f%% EP restored"), TestItem->GetEPRestorePercent());
                break;

            case ECrystalType::Emerald:
                ValueStr = FString::Printf(TEXT("+%.0f%% speed for %d turns"),
                                           TestItem->GetSpeedBuffPercent(), TestItem->GetCrystalDuration());
                break;

            case ECrystalType::Amber:
                ValueStr = FString::Printf(TEXT("-%.0f%% damage taken for %d turns"),
                                           TestItem->GetBuffPercentage(), TestItem->GetCrystalDuration());
                break;

            case ECrystalType::Opal:
                ValueStr = FString::Printf(TEXT("+%.0f%% crit for %d turns"),
                                           TestItem->GetCritBuffPercent(), TestItem->GetCrystalDuration());
                if (TestItem->GetRevealsHP())
                {
                    ValueStr += TEXT(" + reveals");
                }
                break;

            case ECrystalType::Onyx:
                ValueStr = FString::Printf(TEXT("%.0f%% energy drained on use"),
                                           TestItem->GetSilencePercentage());
                break;

            case ECrystalType::Amethyst:
                ValueStr = FString::Printf(TEXT("%.0f%% buff chance, %.0f%% magnitude for %d turns"),
                                           TestItem->GetBuffChancePercent(), TestItem->GetGambleMagnitudePercent(),
                                           TestItem->GetGambleDuration());
                break;

            case ECrystalType::Iolite:
                if (TestItem->GetEffectsToRemoveCount() >= 99)
                {
                    ValueStr = TEXT("Remove ALL effects");
                }
                else
                {
                    ValueStr = FString::Printf(TEXT("Remove %d effect(s)"), TestItem->GetEffectsToRemoveCount());
                }
                break;

            case ECrystalType::Quartz:
                ValueStr = FString::Printf(TEXT("Clear %.0f%% status, %d turns resistance"),
                                           TestItem->GetStatusClearPercent(), TestItem->GetResistanceDuration());
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

        if (Item->bIsEvolutionCrystal)
        {
            UE_LOG(LogTemp, Display, TEXT("Special: EVOLUTION CRYSTAL"));
            if (Item->GrantsEvolution())
            {
                UE_LOG(LogTemp, Display, TEXT("  Evolution: %s"), *Item->ItemName);
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("  WARNING: No Evolution assigned!"));
            }
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
        return Item->GetDOTDamagePercent();

    case ECrystalType::Sapphire:
        return Item->GetHealPercent();

    case ECrystalType::Citrine:
        return Item->GetEPRestorePercent();

    case ECrystalType::Emerald:
        return Item->GetSpeedBuffPercent();

    case ECrystalType::Amber:
        return Item->GetBuffPercentage();

    case ECrystalType::Opal:
        return Item->GetCritBuffPercent();

    case ECrystalType::Onyx:
        return Item->GetSilencePercentage();

    case ECrystalType::Amethyst:
        return Item->GetBuffChancePercent();

    case ECrystalType::Iolite:
        return static_cast<float>(Item->GetEffectsToRemoveCount());

    case ECrystalType::Quartz:
        return Item->GetStatusClearPercent();

    default:
        return 0.0f;
    }
}

void UItemDataDebug::LogCrystalDurability(const UItemData *Item)
{
    if (!Item)
    {
        UE_LOG(LogTemp, Error, TEXT("LogCrystalDurability: Null item!"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("===== CRYSTAL DURABILITY: %s ====="), *Item->GetFullItemName());
    UE_LOG(LogTemp, Display, TEXT("Refined: %s"), Item->bIsRefined ? TEXT("YES") : TEXT("NO"));

    if (!Item->bIsRefined)
    {
        UE_LOG(LogTemp, Display, TEXT("(Unrefined consumable - no durability state)"));
        UE_LOG(LogTemp, Display, TEXT(""));
        return;
    }

    // Design-time inspection only: this asset has no runtime durability state.
    // Live per-instance state lives on FCrystalInventoryEntry — query the
    // actor's LoadoutComponent for that.
    UE_LOG(LogTemp, Display, TEXT("Max Durability: %d"), Item->MaxDurability);
    UE_LOG(LogTemp, Display, TEXT("Immune to breaking: %s"),
           Item->bImmuneToBreaking ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Display, TEXT(""));
}

FString UItemDataDebug::GetDurabilityString(const UItemData *Item)
{
    if (!Item)
    {
        return TEXT("[Null crystal]");
    }

    if (!Item->bIsRefined)
    {
        return FString::Printf(TEXT("[%s] Unrefined - no durability"),
                               *Item->GetFullItemName());
    }

    // Design-time inspection only: this asset has no runtime durability state.
    // Live per-instance state lives on FCrystalInventoryEntry.
    FString State = FString::Printf(TEXT("[%s] Max %d"),
                                    *Item->GetFullItemName(),
                                    Item->MaxDurability);

    if (Item->bImmuneToBreaking)
    {
        State += TEXT(" [Immune]");
    }

    return State;
}