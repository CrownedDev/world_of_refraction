// InventoryDebug.cpp
// Debug utilities implementation

#include "InventoryDebug.h"
#include "InventoryComponent.h"
#include "LoadoutComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "WeaponData.h"
#include "RingData.h"
#include "ItemData.h"
#include "EvolutionData.h"

void UInventoryDebug::LogSeparator(const FString &Title)
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========== %s =========="), *Title);
}

// ==================== INVENTORY LOGGING ====================

void UInventoryDebug::LogInventory(UInventoryComponent *Inventory)
{
    if (!Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("LogInventory: Null inventory!"));
        return;
    }

    LogSeparator(TEXT("INVENTORY SUMMARY"));
    UE_LOG(LogTemp, Display, TEXT("%s"), *Inventory->GetInventorySummary());

    LogSpells(Inventory);
    LogAbilities(Inventory);
    LogWeapons(Inventory);
    LogRings(Inventory);
    LogItems(Inventory);
}

void UInventoryDebug::LogSpells(UInventoryComponent *Inventory)
{
    if (!Inventory)
        return;

    LogSeparator(TEXT("SPELLS"));
    UE_LOG(LogTemp, Display, TEXT("Count: %d/%d"),
           Inventory->Spells.GetCount(),
           Inventory->Spells.GetMaxCapacity());

    for (USpellData *Spell : Inventory->Spells.LearnedSpells)
    {
        if (Spell)
        {
            UE_LOG(LogTemp, Display, TEXT("  - %s [%s] (School: %d, Tier: %d)"),
                   *Spell->SpellName,
                   *UEnum::GetValueAsString(Spell->Element),
                   static_cast<int32>(Spell->School),
                   static_cast<int32>(Spell->Tier));
        }
    }
}

void UInventoryDebug::LogAbilities(UInventoryComponent *Inventory)
{
    if (!Inventory)
        return;

    LogSeparator(TEXT("ABILITIES"));
    UE_LOG(LogTemp, Display, TEXT("Count: %d/%d"),
           Inventory->Abilities.GetCount(),
           Inventory->Abilities.GetMaxCapacity());

    for (UAbilityData *Ability : Inventory->Abilities.LearnedAbilities)
    {
        if (Ability)
        {
            UE_LOG(LogTemp, Display, TEXT("  - %s [Weapon: %d]"),
                   *Ability->AbilityName,
                   static_cast<int32>(Ability->RequiredWeaponType));
        }
    }
}

void UInventoryDebug::LogWeapons(UInventoryComponent *Inventory)
{
    if (!Inventory)
        return;

    LogSeparator(TEXT("WEAPONS"));
    UE_LOG(LogTemp, Display, TEXT("Count: %d (Cost: %d/%d)"),
           Inventory->GetWeaponCount(),
           Inventory->GetWeaponSlotCostTotal(),
           InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS);

    for (int32 i = 0; i < Inventory->Weapons.Num(); ++i)
    {
        const FWeaponInventoryEntry &Entry = Inventory->Weapons[i];
        if (Entry.IsValid())
        {
            FString CrystalStr = Entry.HasCrystal() ? TEXT("Crystal") : TEXT("None");
            FString EvoStr = Entry.IsEvolved() ? TEXT(", Evolved") : TEXT("");

            UE_LOG(LogTemp, Display, TEXT("  [%d] %s (Type: %d, %s%s) Cost: %d"),
                   i,
                   *Entry.Weapon->WeaponName,
                   static_cast<int32>(Entry.Weapon->WeaponType),
                   *CrystalStr,
                   *EvoStr,
                   Entry.GetSlotCost());
        }
    }
}

void UInventoryDebug::LogRings(UInventoryComponent *Inventory)
{
    if (!Inventory)
        return;

    LogSeparator(TEXT("RINGS"));
    UE_LOG(LogTemp, Display, TEXT("Count: %d (Cost: %d/%d)"),
           Inventory->GetRingCount(),
           Inventory->GetRingSlotCostTotal(),
           InventoryConstants::MAX_RING_INVENTORY_SLOTS);

    for (int32 i = 0; i < Inventory->Rings.Num(); ++i)
    {
        const FRingInventoryEntry &Entry = Inventory->Rings[i];
        if (Entry.IsValid())
        {
            FString CrystalStr = Entry.HasCrystal() ? TEXT("Crystal") : TEXT("Empty");
            FString EvoStr = Entry.IsEvolved() ? TEXT(", Evolved") : TEXT("");
            FString EquipStr = Entry.CanBeEquipped() ? TEXT("") : TEXT(" [CANNOT EQUIP]");

            UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%s%s) Cost: %d%s"),
                   i,
                   *Entry.Ring->RingName,
                   *CrystalStr,
                   *EvoStr,
                   Entry.GetSlotCost(),
                   *EquipStr);
        }
    }
}

void UInventoryDebug::LogItems(UInventoryComponent *Inventory)
{
    if (!Inventory)
        return;

    LogSeparator(TEXT("ITEM CRYSTALS"));
    UE_LOG(LogTemp, Display, TEXT("Total: %d/%d"),
           Inventory->Items.GetTotalCount(),
           InventoryConstants::ITEM_CAPACITY_TOTAL);

    // Log by tier
    const EItemTier Tiers[] = {
        EItemTier::S_Tier, EItemTier::A_Tier, EItemTier::B_Tier,
        EItemTier::C_Tier, EItemTier::D_Tier, EItemTier::E_Tier, EItemTier::F_Tier};
    const TCHAR *TierNames[] = {TEXT("S"), TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F")};

    for (int32 i = 0; i < 7; ++i)
    {
        int32 Count = Inventory->Items.GetCountForTier(Tiers[i]);
        int32 Cap = Inventory->Items.GetCapacityForTier(Tiers[i]);
        if (Count > 0)
        {
            UE_LOG(LogTemp, Display, TEXT("  %s-Tier: %d/%d"), TierNames[i], Count, Cap);
        }
    }
}

// ==================== LOADOUT LOGGING ====================

void UInventoryDebug::LogAllLoadouts(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        UE_LOG(LogTemp, Error, TEXT("LogAllLoadouts: Null loadout component!"));
        return;
    }

    LogSeparator(TEXT("ALL LOADOUTS"));
    UE_LOG(LogTemp, Display, TEXT("Saved: %d/%d, Active: %d"),
           Loadout->GetLoadoutCount(),
           Loadout->MaxSavedLoadouts,
           Loadout->GetActiveLoadoutIndex());

    TArray<FString> Names = Loadout->GetLoadoutNames();
    for (int32 i = 0; i < Names.Num(); ++i)
    {
        FString ActiveStr = (i == Loadout->GetActiveLoadoutIndex()) ? TEXT(" [ACTIVE]") : TEXT("");
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s%s"), i, *Names[i], *ActiveStr);
    }
}

void UInventoryDebug::LogActiveLoadout(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        UE_LOG(LogTemp, Error, TEXT("LogActiveLoadout: Null loadout component!"));
        return;
    }

    FCombatLoadout Active = Loadout->GetActiveLoadout();

    LogSeparator(FString::Printf(TEXT("ACTIVE LOADOUT: %s"), *Active.LoadoutName));
    UE_LOG(LogTemp, Display, TEXT("Ready for Battle: %s"),
           Loadout->IsReadyForBattle() ? TEXT("Yes") : TEXT("No"));

    // Primary
    if (!Active.bPrimaryIsRing)
    {
        if (Active.PrimaryWeapon.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Weapon: %s"),
                   *Active.PrimaryWeapon.WeaponEntry.Weapon->WeaponName);
            UE_LOG(LogTemp, Display, TEXT("  Abilities: %d"),
                   Active.PrimaryWeapon.GetAllAbilities().Num());
            UE_LOG(LogTemp, Display, TEXT("  Spells: %d"),
                   Active.PrimaryWeapon.GetAllSpells().Num());
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Weapon: NONE"));
        }
    }
    else
    {
        if (Active.PrimaryRing.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Ring: %s"),
                   *Active.PrimaryRing.RingEntry.Ring->RingName);
            UE_LOG(LogTemp, Display, TEXT("  Spells: %d"),
                   Active.PrimaryRing.GetAllSpells().Num());
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Ring: NONE"));
        }
    }

    // Secondary (Generic)
    if (!Active.bSecondaryIsRing && Active.SecondaryWeapon.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("Secondary Weapon: %s"),
               *Active.SecondaryWeapon.WeaponEntry.Weapon->WeaponName);
    }
    else if (Active.bSecondaryIsRing && Active.SecondaryRing.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("Secondary Ring: %s"),
               *Active.SecondaryRing.RingEntry.Ring->RingName);
    }

    // Ring loadout (Resonator)
    if (Active.RingLoadout.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Ring Loadout: %d rings"), Active.RingLoadout.Num());
        for (int32 i = 0; i < Active.RingLoadout.Num(); ++i)
        {
            const FRingLoadoutEntry &R = Active.RingLoadout[i];
            if (R.IsValid())
            {
                UE_LOG(LogTemp, Display, TEXT("  [%d] %s (%d spells)"),
                       i, *R.RingEntry.Ring->RingName, R.GetSpellCount());
            }
        }
    }

    // Innate spells (Caster)
    if (Active.InnateSpells.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Innate Spells: %d"), Active.InnateSpells.Num());
    }

    // Items
    int32 UsableItems = 0;
    int32 TotalUses = 0;
    for (const FItemLoadoutSlot &Slot : Active.ItemSlots)
    {
        if (Slot.CanUse())
        {
            UsableItems++;
            TotalUses += Slot.GetRemainingUses();
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Items: %d slots, %d uses remaining"), UsableItems, TotalUses);

    // Totals
    UE_LOG(LogTemp, Display, TEXT("---"));
    UE_LOG(LogTemp, Display, TEXT("Total Abilities: %d"), Active.GetAllAbilities().Num());
    UE_LOG(LogTemp, Display, TEXT("Total Spells: %d"), Active.GetAllSpells().Num());
}

void UInventoryDebug::LogValidation(ULoadoutComponent *Loadout, UInventoryComponent *Inventory)
{
    if (!Loadout || !Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("LogValidation: Null component!"));
        return;
    }

    LogSeparator(TEXT("LOADOUT VALIDATION"));

    for (int32 i = 0; i < Loadout->GetLoadoutCount(); ++i)
    {
        TArray<FString> Errors = Loadout->GetValidationErrors(i, Inventory);

        FString StatusStr = Errors.Num() == 0 ? TEXT("VALID") : TEXT("INVALID");
        UE_LOG(LogTemp, Display, TEXT("[%d] %s: %s"),
               i, *Loadout->GetLoadoutNames()[i], *StatusStr);

        for (const FString &Error : Errors)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Error);
        }
    }
}

// ==================== TEST DATA GENERATION ====================

void UInventoryDebug::PopulateTestInventory(UInventoryComponent *Inventory)
{
    if (!Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("PopulateTestInventory: Null inventory!"));
        return;
    }

    LogSeparator(TEXT("POPULATE TEST INVENTORY"));
    UE_LOG(LogTemp, Display, TEXT("Note: This requires test data assets to be created."));
    UE_LOG(LogTemp, Display, TEXT("Use Content Browser to create SpellData, AbilityData, etc."));
    UE_LOG(LogTemp, Display, TEXT("Then call AddWeapon(), LearnSpell(), etc. with those assets."));

    // Log current state
    UE_LOG(LogTemp, Display, TEXT("Current inventory state:"));
    UE_LOG(LogTemp, Display, TEXT("%s"), *Inventory->GetInventorySummary());
}

void UInventoryDebug::CreateTestLoadout(ULoadoutComponent *Loadout, UInventoryComponent *Inventory)
{
    if (!Loadout || !Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTestLoadout: Null component!"));
        return;
    }

    LogSeparator(TEXT("CREATE TEST LOADOUT"));

    // Auto-populate if we have inventory
    if (Inventory->GetWeaponCount() > 0)
    {
        Loadout->AutoPopulateLoadout(Loadout->GetActiveLoadoutIndex(), Inventory);
        UE_LOG(LogTemp, Display, TEXT("Auto-populated loadout from inventory"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No weapons in inventory - cannot auto-populate"));
    }
}

// ==================== CAPACITY TESTING ====================

void UInventoryDebug::TestCapacityLimits(UInventoryComponent *Inventory)
{
    if (!Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("TestCapacityLimits: Null inventory!"));
        return;
    }

    LogSeparator(TEXT("CAPACITY LIMIT TEST"));

    // Test spell capacity
    UE_LOG(LogTemp, Display, TEXT("Spell Capacity:"));
    UE_LOG(LogTemp, Display, TEXT("  Current: %d/%d"),
           Inventory->Spells.GetCount(), InventoryConstants::MAX_LEARNED_SPELLS);
    UE_LOG(LogTemp, Display, TEXT("  Can Learn: %s"),
           Inventory->Spells.CanLearn() ? TEXT("Yes") : TEXT("No"));

    // Test ability capacity
    UE_LOG(LogTemp, Display, TEXT("Ability Capacity:"));
    UE_LOG(LogTemp, Display, TEXT("  Current: %d/%d"),
           Inventory->Abilities.GetCount(), InventoryConstants::MAX_LEARNED_ABILITIES);
    UE_LOG(LogTemp, Display, TEXT("  Can Learn: %s"),
           Inventory->Abilities.CanLearn() ? TEXT("Yes") : TEXT("No"));

    // Test weapon weighted capacity
    UE_LOG(LogTemp, Display, TEXT("Weapon Capacity (Weighted):"));
    UE_LOG(LogTemp, Display, TEXT("  Current Cost: %d/%d"),
           Inventory->GetWeaponSlotCostTotal(), InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS);
    UE_LOG(LogTemp, Display, TEXT("  Remaining: %d slots"),
           Inventory->GetRemainingWeaponCapacity());

    // Test ring weighted capacity
    UE_LOG(LogTemp, Display, TEXT("Ring Capacity (Weighted):"));
    UE_LOG(LogTemp, Display, TEXT("  Current Cost: %d/%d"),
           Inventory->GetRingSlotCostTotal(), InventoryConstants::MAX_RING_INVENTORY_SLOTS);
    UE_LOG(LogTemp, Display, TEXT("  Remaining: %d slots"),
           Inventory->GetRemainingRingCapacity());

    // Test item tiered capacity
    UE_LOG(LogTemp, Display, TEXT("Item Capacity (Tiered):"));
    UE_LOG(LogTemp, Display, TEXT("  Total: %d/%d"),
           Inventory->Items.GetTotalCount(), InventoryConstants::ITEM_CAPACITY_TOTAL);
}

void UInventoryDebug::LogCapacityStatus(UInventoryComponent *Inventory)
{
    if (!Inventory)
        return;

    LogSeparator(TEXT("CAPACITY STATUS"));

    // Quick overview
    UE_LOG(LogTemp, Display, TEXT("Spells:    %3d / %3d  (%d remaining)"),
           Inventory->Spells.GetCount(),
           InventoryConstants::MAX_LEARNED_SPELLS,
           Inventory->Spells.GetRemainingCapacity());

    UE_LOG(LogTemp, Display, TEXT("Abilities: %3d / %3d  (%d remaining)"),
           Inventory->Abilities.GetCount(),
           InventoryConstants::MAX_LEARNED_ABILITIES,
           Inventory->Abilities.GetRemainingCapacity());

    UE_LOG(LogTemp, Display, TEXT("Weapons:   %3d / %3d  (%d remaining) [weighted]"),
           Inventory->GetWeaponSlotCostTotal(),
           InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS,
           Inventory->GetRemainingWeaponCapacity());

    UE_LOG(LogTemp, Display, TEXT("Rings:     %3d / %3d  (%d remaining) [weighted]"),
           Inventory->GetRingSlotCostTotal(),
           InventoryConstants::MAX_RING_INVENTORY_SLOTS,
           Inventory->GetRemainingRingCapacity());

    UE_LOG(LogTemp, Display, TEXT("Items:     %3d / %3d"),
           Inventory->Items.GetTotalCount(),
           InventoryConstants::ITEM_CAPACITY_TOTAL);

    UE_LOG(LogTemp, Display, TEXT("Evolution Crystals: %3d"),
           Inventory->GetEvolutionCrystals().Num());
}

// ==================== VALIDATION TESTING ====================

bool UInventoryDebug::RunValidationSuite(UInventoryComponent *Inventory, ULoadoutComponent *Loadout)
{
    if (!Inventory || !Loadout)
    {
        UE_LOG(LogTemp, Error, TEXT("RunValidationSuite: Null component!"));
        return false;
    }

    LogSeparator(TEXT("VALIDATION SUITE"));

    bool bAllPassed = true;
    int32 TestsPassed = 0;
    int32 TestsFailed = 0;

    // Test 1: Inventory has valid data
    UE_LOG(LogTemp, Display, TEXT("Test 1: Inventory data integrity..."));
    bool bTest1 = true;
    for (USpellData *Spell : Inventory->Spells.LearnedSpells)
    {
        if (!Spell)
        {
            bTest1 = false;
            break;
        }
    }
    if (bTest1)
        TestsPassed++;
    else
        TestsFailed++;
    UE_LOG(LogTemp, Display, TEXT("  Result: %s"), bTest1 ? TEXT("PASS") : TEXT("FAIL"));

    // Test 2: Weapon slot costs are correct
    UE_LOG(LogTemp, Display, TEXT("Test 2: Weapon slot cost calculation..."));
    int32 ManualCost = 0;
    for (const FWeaponInventoryEntry &W : Inventory->Weapons)
    {
        ManualCost += W.GetSlotCost();
    }
    bool bTest2 = (ManualCost == Inventory->GetWeaponSlotCostTotal());
    if (bTest2)
        TestsPassed++;
    else
        TestsFailed++;
    UE_LOG(LogTemp, Display, TEXT("  Result: %s (Manual: %d, Reported: %d)"),
           bTest2 ? TEXT("PASS") : TEXT("FAIL"), ManualCost, Inventory->GetWeaponSlotCostTotal());

    // Test 3: Loadout validation
    UE_LOG(LogTemp, Display, TEXT("Test 3: Active loadout validation..."));
    bool bTest3 = Loadout->ValidateActiveLoadout(Inventory);
    if (bTest3)
        TestsPassed++;
    else
        TestsFailed++;
    UE_LOG(LogTemp, Display, TEXT("  Result: %s"), bTest3 ? TEXT("PASS") : TEXT("FAIL"));

    // Summary
    bAllPassed = (TestsFailed == 0);
    UE_LOG(LogTemp, Display, TEXT("---"));
    UE_LOG(LogTemp, Display, TEXT("Suite Result: %s (%d passed, %d failed)"),
           bAllPassed ? TEXT("ALL PASSED") : TEXT("FAILURES DETECTED"),
           TestsPassed, TestsFailed);

    return bAllPassed;
}
