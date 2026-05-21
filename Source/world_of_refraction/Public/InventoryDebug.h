// InventoryDebug.h
// Debug utilities for inventory and loadout systems
//
// Provides context menu actions and logging for testing

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryDebug.generated.h"

class UInventoryComponent;
class ULoadoutComponent;
class USpellData;
class UAbilityData;
class UWeaponData;
class URingData;
class UEvolutionItemData;
class UEvolutionData;

/**
 * UInventoryDebug
 * Static debug functions for inventory and loadout testing
 */
UCLASS()
class WORLD_OF_REFRACTION_API UInventoryDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ==================== INVENTORY LOGGING ====================

    /** Log full inventory contents */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogInventory(UInventoryComponent* Inventory);

    /** Log spell collection details */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogSpells(UInventoryComponent* Inventory);

    /** Log ability collection details */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogAbilities(UInventoryComponent* Inventory);

    /** Log weapon inventory details */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogWeapons(UInventoryComponent* Inventory);

    /** Log ring inventory details */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogRings(UInventoryComponent* Inventory);

    /** Log item crystal inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogItems(UInventoryComponent* Inventory);

    // ==================== LOADOUT LOGGING ====================

    /** Log all saved loadouts */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Debug")
    static void LogAllLoadouts(ULoadoutComponent* Loadout);

    /** Log active loadout details */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Debug")
    static void LogActiveLoadout(ULoadoutComponent* Loadout);

    /** Log loadout validation results */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Debug")
    static void LogValidation(ULoadoutComponent* Loadout, UInventoryComponent* Inventory);

    // ==================== TEST DATA GENERATION ====================

    /** Populate inventory with test data (requires test assets) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug|Test")
    static void PopulateTestInventory(UInventoryComponent* Inventory);

    /** Create a test loadout from inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Debug|Test")
    static void CreateTestLoadout(ULoadoutComponent* Loadout, UInventoryComponent* Inventory);

    // ==================== CAPACITY TESTING ====================

    /** Test inventory capacity limits */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug|Test")
    static void TestCapacityLimits(UInventoryComponent* Inventory);

    /** Log capacity status for all collections */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    static void LogCapacityStatus(UInventoryComponent* Inventory);

    // ==================== VALIDATION TESTING ====================

    /** Run full validation suite */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug|Test")
    static bool RunValidationSuite(UInventoryComponent* Inventory, ULoadoutComponent* Loadout);

private:
    static void LogSeparator(const FString& Title);
};
