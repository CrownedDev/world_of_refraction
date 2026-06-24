// InventoryDebug.cpp
// Debug utilities implementation

#include "Debug/Inventory/InventoryDebug.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Inventory/ItemTier.h"
#include "Loadout/SpellPoolConstants.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"

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

    for (const FSpellInstance &Instance : Inventory->Spells.LearnedSpells)
    {
        if (USpellData *Spell = Instance.Spell)
        {
            UE_LOG(LogTemp, Display, TEXT("  - %s [%s] (School: %d, Tier: %d)"),
                   *Spell->Name,
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

    for (const FAbilityInstance &Instance : Inventory->Abilities.LearnedAbilities)
    {
        if (UAbilityData *Ability = Instance.Ability)
        {
            UE_LOG(LogTemp, Display, TEXT("  - %s [Weapon: %d]"),
                   *Ability->Name,
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
                   *Entry.Weapon->Name,
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
                   *Entry.Ring->Name,
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

    // Counts come from the new count-based pool (item + refined). Capacity
    // denominators still reflect the legacy tiered model (S=3..F=25); the new
    // component uses a flat per-tier/per-pool cap, so these are dev-reference
    // figures pending a debug-display follow-up.
    const UCrystalInventoryComponent *CrystalInv =
        Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;

    LogSeparator(TEXT("ITEM CRYSTALS"));
    UE_LOG(LogTemp, Display, TEXT("Total: %d/%d"),
           CrystalInv ? CrystalInv->GetTotalCount() : 0,
           InventoryConstants::ITEM_CAPACITY_TOTAL);

    // Log by tier
    const EItemTier Tiers[] = {
        EItemTier::S_Tier, EItemTier::A_Tier, EItemTier::B_Tier,
        EItemTier::C_Tier, EItemTier::D_Tier, EItemTier::E_Tier, EItemTier::F_Tier};
    const TCHAR *TierNames[] = {TEXT("S"), TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F")};

    for (int32 i = 0; i < 7; ++i)
    {
        const int32 Count = CrystalInv
                                ? CrystalInv->GetItemCountForTier(Tiers[i]) + CrystalInv->GetRefinedCountForTier(Tiers[i])
                                : 0;
        const int32 Cap = InventoryConstants::GetItemCapacityForTier(static_cast<int32>(Tiers[i]));
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

    UInventoryComponent *Inv = Loadout->GetOwner()
        ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>()
        : nullptr;
    const int32 MaxLoadouts = Inv ? Inv->MaxSavedLoadouts : 0;

    LogSeparator(TEXT("ALL LOADOUTS"));
    UE_LOG(LogTemp, Display, TEXT("Saved: %d/%d, Active: %d"),
           Loadout->GetLoadoutCount(),
           MaxLoadouts,
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
    switch (Active.PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (Active.PrimaryWeapon.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Weapon: %s"),
                   *Active.PrimaryWeapon.WeaponEntry.Weapon->Name);
            UE_LOG(LogTemp, Display, TEXT("  Abilities: %d"),
                   Active.PrimaryWeapon.GetAllAbilities().Num());
            UE_LOG(LogTemp, Display, TEXT("  Spells: %d"),
                   Active.PrimaryWeapon.GetAllSpells().Num());
            // Slot capacity by container tier (ability cap = weapon tier; spell cap =
            // effective ResolveSpellSlotCap, i.e. gem tier when a gem is attached, else weapon tier).
            if (const UWeaponData *W = Active.PrimaryWeapon.WeaponEntry.Weapon)
            {
                // Display the INSTANCE (leveled) tier from the entry, not the base asset (W->Tier).
                const EItemTier WTier = Active.PrimaryWeapon.WeaponEntry.Tier;
                const int32 AbilityCap = CrystalEffectTable::SlotsForContainerTier(WTier);
                const int32 SpellCap = CrystalEffectTable::ResolveSpellSlotCap(
                    Active.PrimaryWeapon.WeaponEntry.GetAttachedItem(),
                    CrystalEffectTable::SlotsForContainerTier(WTier));
                UE_LOG(LogTemp, Display, TEXT("  Slots [%s]: abilities %d/%d, spells %d/%d"),
                       *TierHelpers::GetTierDisplayString(WTier),
                       Active.PrimaryWeapon.GetAllAbilities().Num(), AbilityCap,
                       Active.PrimaryWeapon.GetAllSpells().Num(), SpellCap);
            }
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Weapon: NONE"));
        }
        break;

    case EPrimarySlotType::Ring:
        if (Active.PrimaryRing.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Ring: %s"),
                   *Active.PrimaryRing.RingEntry.Ring->Name);
            UE_LOG(LogTemp, Display, TEXT("  Spells: %d"),
                   Active.PrimaryRing.GetAllSpells().Num());
            // Spell slot capacity by ring tier (effective cap = customisable + locked;
            // gem tier overrides as the no-gem fall-through inside GetCustomizableSpellCount).
            if (const URingData *Rg = Active.PrimaryRing.RingEntry.Ring)
            {
                const int32 SpellCap = Active.PrimaryRing.GetCustomizableSpellCount()
                                       + Active.PrimaryRing.GetLockedSpellCount();
                UE_LOG(LogTemp, Display, TEXT("  Slots [%s]: spells %d/%d"),
                       *TierHelpers::GetTierDisplayString(Active.PrimaryRing.RingEntry.Tier), // INSTANCE tier, not Rg->Tier (asset)
                       Active.PrimaryRing.GetAllSpells().Num(), SpellCap);
            }
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Ring: NONE"));
        }
        break;

    case EPrimarySlotType::Evolution:
        if (Active.PrimaryEvolution.Item)
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Evolution: %s"),
                   *Active.PrimaryEvolution.Item->ItemName);
            UE_LOG(LogTemp, Display, TEXT("  Evolution Spells: %d"),
                   Active.EvolutionSpells.Num());
            // Spell slot capacity by evolution tier (no attachment crystal — straight tier cap).
            const int32 SpellCap = CrystalEffectTable::SlotsForContainerTier(
                Active.PrimaryEvolution.Tier);
            UE_LOG(LogTemp, Display, TEXT("  Slots [%s]: spells %d/%d"),
                   *TierHelpers::GetTierDisplayString(Active.PrimaryEvolution.Tier),
                   Active.EvolutionSpells.Num(), SpellCap);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Primary Evolution: NONE"));
        }
        break;

    case EPrimarySlotType::None:
        UE_LOG(LogTemp, Display, TEXT("Primary: (none)"));
        break;
    }

    // Secondary (Generic only - Weapon only now)
    if (Active.SecondarySlotType == ESecondarySlotType::Weapon && Active.SecondaryWeapon.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("Secondary Weapon: %s"),
               *Active.SecondaryWeapon.WeaponEntry.Weapon->Name);
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
                       i, *R.RingEntry.Ring->Name, R.GetSpellCount());
                if (const URingData *Rg = R.RingEntry.Ring)
                {
                    const int32 SpellCap = R.GetCustomizableSpellCount() + R.GetLockedSpellCount();
                    UE_LOG(LogTemp, Display, TEXT("      Slots [%s]: spells %d/%d"),
                           *TierHelpers::GetTierDisplayString(R.RingEntry.Tier), // INSTANCE tier, not Rg->Tier (asset)
                           R.GetAllSpells().Num(), SpellCap);
                }
            }
        }
    }

    // Innate spells (Caster)
    if (Active.InnateSpells.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Innate Spells: %d"), Active.InnateSpells.Num());
    }

    // Spell-pool budget readout (Cluster 2/3 — innate + BD weighted budget). Discount
    // from the owner's raw world-pillar levels; no CharacterData -> discount 0, flagged.
    {
        UCharacterData *PoolCharData = Loadout->GetOwnerCharacterData();
        UCharacterDataComponent *PoolCharComp = Loadout->GetOwner()
            ? Loadout->GetOwner()->FindComponentByClass<UCharacterDataComponent>()
            : nullptr;
        const bool bIsBD = PoolCharComp && PoolCharComp->IsBrokenDarkness();
        const int32 Discount = PoolCharData
            ? SpellPoolConstants::SpellSlotDiscount(
                  PoolCharData->WorldMindLevel, PoolCharData->WorldBodyLevel, PoolCharData->WorldSpiritLevel)
            : 0;
        const TCHAR *DiscountNote = PoolCharData ? TEXT("") : TEXT(" [discount unknown — no CharacterData]");

        if (bIsBD)
        {
            // Darkness pool reuses InnateSpells; element pools are BDSpellPools.
            int32 Used = 0;
            UE_LOG(LogTemp, Display, TEXT("BD [Darkness]: %d/%d spells"),
                   Active.InnateSpells.Num(), SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL);
            for (const USpellData *S : Active.InnateSpells)
            {
                if (S) Used += SpellPoolConstants::SpellSlotEffectiveCost(S->Tier, Discount);
            }
            for (const FBDElementSpellPool &Pool : Active.BDSpellPools)
            {
                UE_LOG(LogTemp, Display, TEXT("BD [%s]: %d/%d spells"),
                       *UEnum::GetValueAsString(Pool.Element),
                       Pool.Spells.Num(), SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL);
                for (const USpellData *S : Pool.Spells)
                {
                    if (S) Used += SpellPoolConstants::SpellSlotEffectiveCost(S->Tier, Discount);
                }
            }
            UE_LOG(LogTemp, Display, TEXT("BD budget: %d/%d pts (discount -%d)%s"),
                   Used, SpellPoolConstants::BD_SPELL_BUDGET, Discount, DiscountNote);
        }
        else if (Active.InnateSpells.Num() > 0)
        {
            // Normal Caster: per-school counts + total innate weight.
            TMap<ESpellSchool, int32> PerSchool;
            for (const USpellData *S : Active.InnateSpells)
            {
                if (S) PerSchool.FindOrAdd(S->School)++;
            }
            for (const TPair<ESpellSchool, int32> &P : PerSchool)
            {
                UE_LOG(LogTemp, Display, TEXT("Innate [%s]: %d/%d spells"),
                       *UEnum::GetValueAsString(P.Key), P.Value, SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL);
            }
            int32 Used = 0;
            for (const USpellData *S : Active.InnateSpells)
            {
                if (S) Used += SpellPoolConstants::SpellSlotEffectiveCost(S->Tier, Discount);
            }
            UE_LOG(LogTemp, Display, TEXT("Innate budget: %d/%d pts (discount -%d)%s"),
                   Used, SpellPoolConstants::INNATE_SPELL_BUDGET, Discount, DiscountNote);
        }
    }

    // Items
    int32 UsableItems = 0;
    int32 TotalUses = 0;
    for (const FItemLoadoutSlot &Slot : Active.ItemSlots)
    {
        if (!Slot.IsEmpty())
        {
            UsableItems++;
            TotalUses += Slot.Quantity;
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
    const UCrystalInventoryComponent *CrystalInv =
        Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    UE_LOG(LogTemp, Display, TEXT("Item Capacity (Tiered):"));
    UE_LOG(LogTemp, Display, TEXT("  Total: %d/%d"),
           CrystalInv ? CrystalInv->GetTotalCount() : 0, InventoryConstants::ITEM_CAPACITY_TOTAL);
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

    const UCrystalInventoryComponent *CrystalInv =
        Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    UE_LOG(LogTemp, Display, TEXT("Items:     %3d / %3d"),
           CrystalInv ? CrystalInv->GetTotalCount() : 0,
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
    for (const FSpellInstance &Instance : Inventory->Spells.LearnedSpells)
    {
        if (!Instance.Spell)
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
