// ItemDataDebug.h
// Debug utilities for testing the item system

#pragma once

#include "CoreMinimal.h"
#include "EvolutionItemData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemDataDebug.generated.h"

/**
 * Debug utilities for the Item System
 * Provides validation, logging, and testing functions
 */
UCLASS()
class UItemDataDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ==================== VALIDATION ====================

    /**
     * Validates all 70 crystal/tier combinations
     * Logs any issues found (zero values, missing data, etc.)
     * @return True if all items validate correctly
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static bool ValidateAllItemCombinations();

    /**
     * Validates a specific item's computed values
     * @param Item The item to validate
     * @return True if item is valid
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static bool ValidateItem(const UEvolutionItemData *Item);

    // ==================== LOGGING ====================

    /**
     * Logs all computed values for a specific item
     * @param Item The item to log
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static void LogItemValues(const UEvolutionItemData *Item);

    /**
     * Logs a summary table of all items for a specific crystal type
     * Shows tier progression at a glance
     * @param CrystalType The crystal type to log
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static void LogCrystalTierProgression(ECrystalType CrystalType);

    /**
     * Logs the complete item matrix (all crystals x all tiers)
     * Warning: Produces a lot of output!
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static void LogAllItems();

    /**
     * Logs tier-based bonuses (Generic resistance, BD energy)
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static void LogTierBonuses();

    /**
     * Logs crystal state info (raw vs refined, slotting status)
     * @param Item The item to check
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static void LogCrystalState(const UEvolutionItemData *Item);
    /**
     * Logs a crystal asset's design-time durability properties (MaxDurability,
     * refined, immune). Per-instance runtime state lives on
     * FCrystalInventoryEntry — query the actor's LoadoutComponent for that.
     * @param Item The crystal asset to inspect
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static void LogCrystalDurability(const UEvolutionItemData *Item);

    /**
     * Returns a single-line formatted design-time durability string for asset
     * inspection. Format: "[Refined Crystal Name] Max 40 [Immune]" or
     * "[Unrefined - no durability]". For live per-instance state use
     * FCrystalInventoryEntry via LoadoutComponent.
     */
    UFUNCTION(BlueprintPure, Category = "Item Debug")
    static FString GetDurabilityString(const UEvolutionItemData *Item);

    // ==================== TESTING ====================

    /**
     * Creates a temporary item with specified crystal/tier for testing
     * @param CrystalType The crystal type
     * @param Tier The tier level
     * @return Temporary item (not saved to disk)
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static UEvolutionItemData *CreateTestItem(ECrystalType CrystalType, EItemTier Tier);

    /**
     * Tests that tier progression is correct (higher tier = better values)
     * @param CrystalType The crystal type to test
     * @return True if progression is valid
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static bool TestTierProgression(ECrystalType CrystalType);

    /**
     * Runs all validation and progression tests
     * @return True if all tests pass
     */
    UFUNCTION(BlueprintCallable, Category = "Item Debug", meta = (DevelopmentOnly))
    static bool RunAllTests();

private:
    // Helper to get crystal type name for logging
    static FString GetCrystalTypeName(ECrystalType CrystalType);

    // Helper to get tier name for logging
    static FString GetTierName(EItemTier Tier);

    // Helper to get primary value for a crystal type (for progression testing)
    static float GetPrimaryValue(const UEvolutionItemData *Item);
};