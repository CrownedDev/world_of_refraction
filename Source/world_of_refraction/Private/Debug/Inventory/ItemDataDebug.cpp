// ItemDataDebug.cpp
// Implementation of item system debug utilities

#include "Debug/Inventory/ItemDataDebug.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/ItemIdentity.h"

bool UItemDataDebug::ValidateAllItemCombinations()
{
    UE_LOG(LogTemp, Display, TEXT("========== VALIDATING ALL 70 ITEM COMBINATIONS =========="));

    bool bAllValid = true;
    int32 ValidCount = 0;
    int32 InvalidCount = 0;

    // Iterate all crystal types
    for (int32 CrystalIndex = static_cast<int32>(ECrystalType::Garnet); CrystalIndex <= static_cast<int32>(ECrystalType::Quartz); ++CrystalIndex)
    {
        ECrystalType Crystal = static_cast<ECrystalType>(CrystalIndex);
        if (Crystal == ECrystalType::Quartz) continue; // consumable-only, not a valid UEvolutionItemData asset

        // Iterate all tiers
        for (int32 TierIndex = 0; TierIndex <= static_cast<int32>(EItemTier::S_Tier); ++TierIndex)
        {
            EItemTier Tier = static_cast<EItemTier>(TierIndex);

            // Create test item
            UEvolutionItemData *TestItem = CreateTestItem(Crystal, Tier);
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

bool UItemDataDebug::ValidateItem(const UEvolutionItemData *Item)
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

    // Check element is valid. Only Quartz is non-elemental (None) — any OTHER crystal that
    // resolves to None is a data bug. (Was == Generic before the Generic->None sentinel
    // migration; Generic now means "inherit at cast", None is the non-elemental sentinel.)
    ESpellElement Element = Item->GetAssociatedElement();
    if (Element == ESpellElement::None && Item->CrystalType != ECrystalType::Quartz)
    {
        Errors.Add(TEXT("Unexpected non-elemental (None) element"));
        bValid = false;
    }

    // Check tier bonuses are positive
    if (CrystalEffectTable::GetBrokenDarknessEnergyPercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
    {
        Errors.Add(TEXT("Zero or negative BD energy bonus"));
        bValid = false;
    }

    // Crystal-specific validation
    switch (Item->CrystalType)
    {
    case ECrystalType::Garnet:
        if (CrystalEffectTable::GetDOTDamagePercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero DOT damage percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Sapphire:
        // Sapphire is now defy-death (Last Stand / revive). Validate its tier-scaled ward window
        // (must be > 0 or the ward never arms), not a heal percent.
        if (CrystalEffectTable::GetLastStandWindow(FCrystalId{Item->CrystalType, Item->Tier}) <= 0)
        {
            Errors.Add(TEXT("Zero Last Stand window"));
            bValid = false;
        }
        break;

    case ECrystalType::Citrine:
        if (CrystalEffectTable::GetEPRestorePercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero EP restore percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Emerald:
        // Emerald (reworked) grants a bonus turn after EMERALD_BONUS_TURN_DELAY turns; S=0 is a
        // valid (immediate) delay, so there is no "zero is invalid" stat to validate here.
        break;

    case ECrystalType::Amber:
        if (CrystalEffectTable::GetAmberDefensePercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero buff percentage"));
            bValid = false;
        }
        break;

    case ECrystalType::Opal:
        if (CrystalEffectTable::GetCritBuffPercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero crit buff percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Onyx:
        if (CrystalEffectTable::GetSilencePercentage(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero silence percentage"));
            bValid = false;
        }
        break;

    case ECrystalType::Amethyst:
        if (CrystalEffectTable::GetBuffChancePercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero buff chance percent"));
            bValid = false;
        }
        break;

    case ECrystalType::Iolite:
        if (CrystalEffectTable::GetEffectsToRemoveCount(FCrystalId{Item->CrystalType, Item->Tier}) <= 0)
        {
            Errors.Add(TEXT("Zero effects to remove count"));
            bValid = false;
        }
        break;

    case ECrystalType::Quartz:
        Errors.Add(TEXT("Quartz cannot exist as a UEvolutionItemData asset — consumable only"));
        bValid = false;
        if (CrystalEffectTable::GetStatusClearPercent(FCrystalId{Item->CrystalType, Item->Tier}) <= 0.0f)
        {
            Errors.Add(TEXT("Zero status clear percent"));
            bValid = false;
        }
        break;

    default:
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

void UItemDataDebug::LogItemValues(const UEvolutionItemData *Item)
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
    UE_LOG(LogTemp, Display, TEXT("Effect Type: %d"), static_cast<int32>(ItemIdentity::GetItemEffectType(FCrystalId{Item->CrystalType, Item->Tier})));
    UE_LOG(LogTemp, Display, TEXT(""));

    // ADD: Crystal state
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("--- Crystal State ---"));
    UE_LOG(LogTemp, Display, TEXT("Category: Evolution"));

    // ADD: Evolution details
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("--- Evolution Details ---"));
    UE_LOG(LogTemp, Display, TEXT("Evolution Name: %s"), *Item->ItemName);
    UE_LOG(LogTemp, Display, TEXT("Evolution Element: %s"), *UEnum::GetValueAsString(Item->GetAssociatedElement()));

    // Effect values (Phase 2 percentage-based getters)
    UE_LOG(LogTemp, Display, TEXT("--- Effect Values ---"));
    UE_LOG(LogTemp, Display, TEXT("DOT Damage %%: %.1f for %d turns"), CrystalEffectTable::GetDOTDamagePercent(FCrystalId{Item->CrystalType, Item->Tier}), CrystalEffectTable::GetDOTDuration(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Heal %%: %.1f"), CrystalEffectTable::GetHealPercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("EP Restore %%: %.1f"), CrystalEffectTable::GetEPRestorePercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Speed Buff %%: %.1f"), CrystalEffectTable::GetSpeedBuffPercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Emerald Bonus-Turn Delay: %d turns (0=immediate; 0 also for non-Emerald)"), CrystalEffectTable::GetEmeraldBonusTurnDelay(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Buff %%: %.1f"), CrystalEffectTable::GetAmberDefensePercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Crit Buff %%: %.1f"), CrystalEffectTable::GetCritBuffPercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Crystal Buff Duration: %d turns"), CrystalEffectTable::GetCrystalDuration(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Silence %%: %.1f (one-shot drain on use)"), CrystalEffectTable::GetSilencePercentage(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Buff Chance %%: %.1f"), CrystalEffectTable::GetBuffChancePercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Gamble Magnitude %%: %.1f for %d turns"), CrystalEffectTable::GetGambleMagnitudePercent(FCrystalId{Item->CrystalType, Item->Tier}), CrystalEffectTable::GetGambleDuration(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Effects To Remove: %d (IOLITE_REMOVE_ALL sentinel = all)"), CrystalEffectTable::GetEffectsToRemoveCount(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Status Clear %%: %.1f, Resistance: %d turns"), CrystalEffectTable::GetStatusClearPercent(FCrystalId{Item->CrystalType, Item->Tier}), CrystalEffectTable::GetResistanceDuration(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Elemental Buildup %%: %.1f"), CrystalEffectTable::GetElementalBuildupPercent(FCrystalId{Item->CrystalType, Item->Tier}));
    UE_LOG(LogTemp, Display, TEXT("Reveals HP/Stats: %s"), CrystalEffectTable::GetRevealsHP(FCrystalId{Item->CrystalType, Item->Tier}) ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // Bonuses
    UE_LOG(LogTemp, Display, TEXT("--- Tier Bonuses ---"));
    UE_LOG(LogTemp, Display, TEXT("BD Energy Bonus: %.0f%% of target MaxEP"),
           CrystalEffectTable::GetBrokenDarknessEnergyPercent(FCrystalId{Item->CrystalType, Item->Tier}) * 100.0f);
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
        UEvolutionItemData *TestItem = CreateTestItem(CrystalType, Tier);

        if (TestItem)
        {
            FString ValueStr;

            switch (CrystalType)
            {
            case ECrystalType::Garnet:
                ValueStr = FString::Printf(TEXT("%.0f%% burn/turn x%d turns"),
                                           CrystalEffectTable::GetDOTDamagePercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}), CrystalEffectTable::GetDOTDuration(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            case ECrystalType::Sapphire:
                ValueStr = FString::Printf(TEXT("Last Stand: %d-turn ward (survive at 50%%)"), CrystalEffectTable::GetLastStandWindow(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            case ECrystalType::Citrine:
                ValueStr = FString::Printf(TEXT("+%.0f%% EP restored"), CrystalEffectTable::GetEPRestorePercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            case ECrystalType::Emerald:
            {
                const int32 BonusTurnDelay = CrystalEffectTable::GetEmeraldBonusTurnDelay(FCrystalId{TestItem->CrystalType, TestItem->Tier});
                ValueStr = (BonusTurnDelay == 0)
                               ? FString(TEXT("bonus turn (immediate)"))
                               : FString::Printf(TEXT("bonus turn after %d turns"), BonusTurnDelay);
                break;
            }

            case ECrystalType::Amber:
                ValueStr = FString::Printf(TEXT("%.0f%% defense (ally buff / enemy weaken) for %d turns"),
                                           CrystalEffectTable::GetAmberDefensePercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}), CrystalEffectTable::GetCrystalDuration(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            case ECrystalType::Opal:
                ValueStr = FString::Printf(TEXT("+%.0f%% crit for %d turns"),
                                           CrystalEffectTable::GetCritBuffPercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}), CrystalEffectTable::GetCrystalDuration(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                if (CrystalEffectTable::GetRevealsHP(FCrystalId{TestItem->CrystalType, TestItem->Tier}))
                {
                    ValueStr += TEXT(" + reveals");
                }
                break;

            case ECrystalType::Onyx:
                ValueStr = FString::Printf(TEXT("%.0f%% energy drained on use"),
                                           CrystalEffectTable::GetSilencePercentage(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            case ECrystalType::Amethyst:
                ValueStr = FString::Printf(TEXT("%.0f%% buff chance, %.0f%% magnitude for %d turns"),
                                           CrystalEffectTable::GetBuffChancePercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}), CrystalEffectTable::GetGambleMagnitudePercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}),
                                           CrystalEffectTable::GetGambleDuration(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            case ECrystalType::Iolite:
                if (CrystalEffectTable::GetEffectsToRemoveCount(FCrystalId{TestItem->CrystalType, TestItem->Tier}) >= 99)
                {
                    ValueStr = TEXT("Remove ALL effects");
                }
                else
                {
                    ValueStr = FString::Printf(TEXT("Remove %d effect(s)"), CrystalEffectTable::GetEffectsToRemoveCount(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                }
                break;

            case ECrystalType::Quartz:
                ValueStr = FString::Printf(TEXT("Clear %.0f%% status, %d turns resistance"),
                                           CrystalEffectTable::GetStatusClearPercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}), CrystalEffectTable::GetResistanceDuration(FCrystalId{TestItem->CrystalType, TestItem->Tier}));
                break;

            default:
                break;
            }

            UE_LOG(LogTemp, Display, TEXT("  %s: %s"), *GetTierName(Tier), *ValueStr);
        }
    }

    UE_LOG(LogTemp, Display, TEXT(""));
}

void UItemDataDebug::LogCrystalState(const UEvolutionItemData *Item)
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

        UE_LOG(LogTemp, Display, TEXT("Special: EVOLUTION CRYSTAL"));
        UE_LOG(LogTemp, Display, TEXT("  Evolution: %s"), *Item->ItemName);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Usage: Unrefined — in inventory awaiting refinement"));
        UE_LOG(LogTemp, Display, TEXT("Description: %s"), *Item->Description);
    }

    UE_LOG(LogTemp, Display, TEXT("====================================="));
}

void UItemDataDebug::LogAllItems()
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("##########################################"));
    UE_LOG(LogTemp, Display, TEXT("#       COMPLETE ITEM MATRIX (70)        #"));
    UE_LOG(LogTemp, Display, TEXT("##########################################"));

    for (int32 CrystalIndex = static_cast<int32>(ECrystalType::Garnet); CrystalIndex <= static_cast<int32>(ECrystalType::Quartz); ++CrystalIndex)
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
        UEvolutionItemData *TestItem = CreateTestItem(ECrystalType::Garnet, Tier);

        if (TestItem)
        {
            UE_LOG(LogTemp, Display, TEXT("  %s: %.0f%% of MaxEP"),
                   *GetTierName(Tier),
                   CrystalEffectTable::GetBrokenDarknessEnergyPercent(FCrystalId{TestItem->CrystalType, TestItem->Tier}) * 100.0f);
        }
    }

    UE_LOG(LogTemp, Display, TEXT(""));
}

UEvolutionItemData *UItemDataDebug::CreateTestItem(ECrystalType CrystalType, EItemTier Tier)
{
    UEvolutionItemData *TestItem = NewObject<UEvolutionItemData>();
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
        UEvolutionItemData *TestItem = CreateTestItem(CrystalType, Tier);

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
    for (int32 CrystalIndex = static_cast<int32>(ECrystalType::Garnet); CrystalIndex <= static_cast<int32>(ECrystalType::Quartz); ++CrystalIndex)
    {
        ECrystalType Crystal = static_cast<ECrystalType>(CrystalIndex);
        if (Crystal == ECrystalType::Quartz) continue; // consumable-only, not a valid UEvolutionItemData asset
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

float UItemDataDebug::GetPrimaryValue(const UEvolutionItemData *Item)
{
    if (!Item)
        return 0.0f;

    switch (Item->CrystalType)
    {
    case ECrystalType::Garnet:
        return CrystalEffectTable::GetDOTDamagePercent(FCrystalId{Item->CrystalType, Item->Tier});

    case ECrystalType::Sapphire:
        // Defy-death: primary value is the Last Stand ward window (turns), like Emerald's delay.
        return static_cast<float>(CrystalEffectTable::GetLastStandWindow(FCrystalId{Item->CrystalType, Item->Tier}));

    case ECrystalType::Citrine:
        return CrystalEffectTable::GetEPRestorePercent(FCrystalId{Item->CrystalType, Item->Tier});

    case ECrystalType::Emerald:
        // Primary "effect value" is now the bonus-turn delay N (turns), not a speed %.
        return static_cast<float>(CrystalEffectTable::GetEmeraldBonusTurnDelay(FCrystalId{Item->CrystalType, Item->Tier}));

    case ECrystalType::Amber:
        return CrystalEffectTable::GetAmberDefensePercent(FCrystalId{Item->CrystalType, Item->Tier});

    case ECrystalType::Opal:
        return CrystalEffectTable::GetCritBuffPercent(FCrystalId{Item->CrystalType, Item->Tier});

    case ECrystalType::Onyx:
        return CrystalEffectTable::GetSilencePercentage(FCrystalId{Item->CrystalType, Item->Tier});

    case ECrystalType::Amethyst:
        return CrystalEffectTable::GetBuffChancePercent(FCrystalId{Item->CrystalType, Item->Tier});

    case ECrystalType::Iolite:
        return static_cast<float>(CrystalEffectTable::GetEffectsToRemoveCount(FCrystalId{Item->CrystalType, Item->Tier}));

    case ECrystalType::Quartz:
        return CrystalEffectTable::GetStatusClearPercent(FCrystalId{Item->CrystalType, Item->Tier});

    default:
        return 0.0f;
    }
}

void UItemDataDebug::LogCrystalDurability(const UEvolutionItemData *Item)
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
    // Live per-instance state lives on the runtime attachment
    // (FRuntimeAttachedItem) — query the actor's LoadoutComponent for that.
    UE_LOG(LogTemp, Display, TEXT("Max Durability: %d"), Item->MaxDurability);
    UE_LOG(LogTemp, Display, TEXT("Breakability: %s"),
           *GetBreakabilityString(Item->Breakability));
    UE_LOG(LogTemp, Display, TEXT(""));
}

FString UItemDataDebug::GetDurabilityString(const UEvolutionItemData *Item)
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
    // Live per-instance state lives on the runtime attachment (FRuntimeAttachedItem).
    FString State = FString::Printf(TEXT("[%s] Max %d"),
                                    *Item->GetFullItemName(),
                                    Item->MaxDurability);

    State += FString::Printf(TEXT(" [%s]"), *GetBreakabilityString(Item->Breakability));

    return State;
}