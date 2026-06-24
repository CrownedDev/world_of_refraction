// FCombatLoadout.cpp
// Full combat loadout implementation

#include "Loadout/FCombatLoadout.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/SpellPoolConstants.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Loadout/Entries/FAbilityCollection.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "GameFramework/Actor.h"

// ==================== VALIDATION ====================
// Live validation path is ULoadoutComponent::GetValidationErrors — see header
// note. The struct-side Validate / ValidateGeneric / ValidateCaster /
// ValidateResonator methods were dead code (zero callers) and were removed
// in feature/integration-gaps-sweep-2.

// ==================== BROKEN DARKNESS VALIDATION ====================

TArray<FString> FCombatLoadout::ValidateBDSpellLoadout(
    const TArray<USpellData *> &InnateSpells,
    const TArray<FBDElementSpellPool> &BDSpellPools,
    int32 Discount,
    bool bCheckWeight)
{
    TArray<FString> Errors;

    // Darkness pool (InnateSpells) — capped, every entry must be Darkness.
    if (InnateSpells.Num() > SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL)
    {
        Errors.Add(FString::Printf(TEXT("Broken Darkness: too many Darkness spells (%d/%d)"),
                                   InnateSpells.Num(), SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL));
    }
    for (const USpellData *Spell : InnateSpells)
    {
        if (Spell && !ElementHelpers::SpellElementMatchesHost(Spell->Element, ESpellElement::Darkness))
        {
            Errors.Add(FString::Printf(
                TEXT("Broken Darkness: Darkness-pool spell '%s' is not a Darkness element spell"),
                *Spell->Name));
        }
    }

    // Element pools — capped count, each pool capped, every spell matches element.
    if (BDSpellPools.Num() > LoadoutConstants::MAX_BD_ELEMENT_POOLS)
    {
        Errors.Add(FString::Printf(TEXT("Broken Darkness: too many element pools (%d/%d)"),
                                   BDSpellPools.Num(), LoadoutConstants::MAX_BD_ELEMENT_POOLS));
    }
    for (const FBDElementSpellPool &Pool : BDSpellPools)
    {
        if (Pool.Spells.Num() > SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL)
        {
            Errors.Add(FString::Printf(TEXT("Broken Darkness: %s pool has too many spells (%d/%d)"),
                                       *UEnum::GetValueAsString(Pool.Element),
                                       Pool.Spells.Num(), SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL));
        }
        for (const USpellData *Spell : Pool.Spells)
        {
            if (Spell && !ElementHelpers::SpellElementMatchesHost(Spell->Element, Pool.Element))
            {
                Errors.Add(FString::Printf(
                    TEXT("Broken Darkness: spell '%s' in %s pool does not match the pool element"),
                    *Spell->Name, *UEnum::GetValueAsString(Pool.Element)));
            }
        }
    }

    // Weight budget (runtime only — needs the character's discount): ONE shared point
    // budget across the Darkness pool + EVERY element pool. Σ effective spell cost
    // (tier - discount, floored) must fit BD_SPELL_BUDGET. The absorption gate already
    // limits WHICH element pools exist; this caps total power across them.
    if (bCheckWeight)
    {
        int32 Used = 0;
        for (const USpellData *Spell : InnateSpells)
        {
            if (Spell)
            {
                Used += SpellPoolConstants::SpellSlotEffectiveCost(Spell->Tier, Discount);
            }
        }
        for (const FBDElementSpellPool &Pool : BDSpellPools)
        {
            for (const USpellData *Spell : Pool.Spells)
            {
                if (Spell)
                {
                    Used += SpellPoolConstants::SpellSlotEffectiveCost(Spell->Tier, Discount);
                }
            }
        }
        if (Used > SpellPoolConstants::BD_SPELL_BUDGET)
        {
            Errors.Add(FString::Printf(
                TEXT("Broken Darkness spell budget exceeded: %d/%d points used (discount -%d)"),
                Used, SpellPoolConstants::BD_SPELL_BUDGET, Discount));
        }
    }

    return Errors;
}

// ==================== ACCESSORS ====================

int32 FCombatLoadout::GetInnateSpellCountForSchool(ESpellSchool School) const
{
    int32 Count = 0;
    for (const USpellData *Spell : InnateSpells)
    {
        if (Spell && Spell->School == School)
        {
            Count++;
        }
    }
    return Count;
}

bool FCombatLoadout::HasDuplicateItemTypes() const
{
    TSet<ECrystalType> SeenTypes;

    for (const FItemLoadoutSlot &Slot : ItemSlots)
    {
        if (Slot.IsEmpty())
        {
            continue;
        }

        ECrystalType Type = Slot.CrystalId.Type;
        if (SeenTypes.Contains(Type))
        {
            return true; // Duplicate found
        }
        SeenTypes.Add(Type);
    }

    return false;
}

int32 FCombatLoadout::GetTotalItemUses() const
{
    int32 Total = 0;

    for (const FItemLoadoutSlot &Slot : ItemSlots)
    {
        Total += Slot.Quantity;
    }

    return Total;
}

TArray<UAbilityData *> FCombatLoadout::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    // Primary weapon abilities (only if primary is weapon)
    if (PrimarySlotType == EPrimarySlotType::Weapon && PrimaryWeapon.IsValid())
    {
        Result.Append(PrimaryWeapon.GetAllAbilities());
    }

    // Secondary weapon abilities (Generic only)
    if (SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetAllAbilities());
    }

    // Note: Evolution abilities would come from PrimaryEvolution
    // but abilities from evolution are handled separately

    return Result;
}

TArray<USpellData *> FCombatLoadout::GetAllSpells() const
{
    TArray<USpellData *> Result;

    // Evolution spells
    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result.Append(EvolutionSpells);
    }

    // Primary weapon spells
    if (PrimarySlotType == EPrimarySlotType::Weapon && PrimaryWeapon.IsValid())
    {
        Result.Append(PrimaryWeapon.GetAllSpells());
    }

    // Primary ring spells
    if (PrimarySlotType == EPrimarySlotType::Ring && PrimaryRing.IsValid())
    {
        Result.Append(PrimaryRing.GetAllSpells());
    }

    // Secondary weapon spells (Generic only)
    if (SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetAllSpells());
    }

    // Innate spells (Caster)
    Result.Append(InnateSpells);

    // Ring loadout spells (Resonator)
    for (const FRingLoadoutEntry &Entry : RingLoadout)
    {
        if (Entry.IsValid())
        {
            Result.Append(Entry.GetAllSpells());
        }
    }

    return Result;
}

TArray<FItemLoadoutSlot> FCombatLoadout::GetUsableItemSlots() const
{
    TArray<FItemLoadoutSlot> Result;

    for (const FItemLoadoutSlot &Slot : ItemSlots)
    {
        if (!Slot.IsEmpty())
        {
            Result.Add(Slot);
        }
    }

    return Result;
}

// ==================== HELPERS ====================

void FCombatLoadout::Clear()
{
    LoadoutName = TEXT("Default Loadout");

    PrimarySlotType = EPrimarySlotType::None;
    PrimaryWeapon.Clear();
    PrimaryRing.Clear();
    PrimaryEvolution = FEvolutionAttachment();
    EvolutionSpells.Empty();

    SecondarySlotType = ESecondarySlotType::None;
    SecondaryWeapon.Clear();

    RingLoadout.Empty();
    InnateSpells.Empty();
    BDSpellPools.Empty();
    ItemSlots.Empty();

    bShowPrimary = true;
    ActiveRingIndex = 0;
}

void FCombatLoadout::InitializeForClass(ECharacterClass CharClass)
{
    Clear();

    switch (CharClass)
    {
    case ECharacterClass::Generic:
        // Generic: Primary weapon/ring/evolution, optional secondary weapon
        PrimarySlotType = EPrimarySlotType::None;
        SecondarySlotType = ESecondarySlotType::None;
        break;

    case ECharacterClass::Caster:
        // Caster: Primary weapon/ring/evolution, innate spells
        PrimarySlotType = EPrimarySlotType::None;
        InnateSpells.SetNum(0); // Empty, to be filled
        break;

    case ECharacterClass::Resonator:
        // Resonator: Primary weapon/evolution, ring loadout
        PrimarySlotType = EPrimarySlotType::None;
        RingLoadout.SetNum(0); // Empty, to be filled
        break;
    }

    // Initialize item slots (empty). SetNum default-constructs each slot
    // (default CrystalId, Quantity 0) — no per-slot reset needed.
    ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
}

// ==================== FACTORY ====================

FCombatLoadout FCombatLoadout::CreateFromSavedLoadout(const FSavedLoadout &SavedLoadout)
{
    // Zero-context form — null inventories mean every instance ref takes the
    // asset fallback below, so this is byte-identical to the pre-shape-B path.
    return CreateFromSavedLoadout(SavedLoadout, nullptr, nullptr);
}

FCombatLoadout FCombatLoadout::CreateFromSavedLoadout(const FSavedLoadout &SavedLoadout,
                                                      const UInventoryComponent *OwnedInventory,
                                                      const UEvolutionInventoryComponent *OwnedEvolutions)
{
    // Shape-B resolvers. A slot resolves to its OWNED entry (wholesale copy —
    // carries roll, pools, PersistentID, and the instance's real crystal/spell
    // state) only when the ref is valid, the context exists, the guid is found,
    // AND the found entry's asset matches the slot's asset. Anything else falls
    // back to the asset build — today's path exactly. Logging: Verbose for
    // unset refs / no-context (the normal cases), Warning only for a SET ref
    // that fails to resolve (a real dangling or mismatched reference).
    const auto ResolveWeaponEntry = [OwnedInventory](UWeaponData *Asset, const FGuid &Ref, const TCHAR *SlotName) -> FWeaponInventoryEntry
    {
        if (Ref.IsValid())
        {
            if (OwnedInventory)
            {
                for (const FWeaponInventoryEntry &Owned : OwnedInventory->Weapons)
                {
                    if (Owned.PersistentID == Ref)
                    {
                        if (Owned.Weapon != Asset)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] %s instance ref %s resolves to a DIFFERENT asset ('%s' != slot '%s') — falling back to asset build"),
                                   SlotName, *Ref.ToString(), Owned.Weapon ? *Owned.Weapon->Name : TEXT("null"), Asset ? *Asset->Name : TEXT("null"));
                            break;
                        }
                        UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] %s resolved owned weapon instance %s"), SlotName, *Ref.ToString());
                        return Owned;
                    }
                }
                UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] %s instance ref %s set but NOT FOUND in owned weapons — falling back to asset build"),
                       SlotName, *Ref.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] %s instance ref set but no inventory context — asset build"), SlotName);
            }
        }
        return FWeaponInventoryEntry::CreateFromWeapon(Asset, true);
    };

    const auto ResolveRingEntry = [OwnedInventory](URingData *Asset, const FGuid &Ref, const TCHAR *SlotName) -> FRingInventoryEntry
    {
        if (Ref.IsValid())
        {
            if (OwnedInventory)
            {
                for (const FRingInventoryEntry &Owned : OwnedInventory->Rings)
                {
                    if (Owned.PersistentID == Ref)
                    {
                        if (Owned.Ring != Asset)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] %s instance ref %s resolves to a DIFFERENT asset ('%s' != slot '%s') — falling back to asset build"),
                                   SlotName, *Ref.ToString(), Owned.Ring ? *Owned.Ring->Name : TEXT("null"), Asset ? *Asset->Name : TEXT("null"));
                            break;
                        }
                        UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] %s resolved owned ring instance %s"), SlotName, *Ref.ToString());
                        return Owned;
                    }
                }
                UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] %s instance ref %s set but NOT FOUND in owned rings — falling back to asset build"),
                       SlotName, *Ref.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] %s instance ref set but no inventory context — asset build"), SlotName);
            }
        }
        return FRingInventoryEntry::CreateFromRing(Asset, true);
    };

    FCombatLoadout Result;

    Result.LoadoutName = SavedLoadout.LoadoutName;
    Result.PrimarySlotType = SavedLoadout.PrimarySlotType;

    // Resonator cannot have Ring primary - guard against bad asset data
    if (SavedLoadout.RequiredClass == ECharacterClass::Resonator &&
        Result.PrimarySlotType == EPrimarySlotType::Ring)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] Resonator saved loadout '%s' has invalid Ring primary - clearing to None"),
               *SavedLoadout.LoadoutName);
        Result.PrimarySlotType = EPrimarySlotType::None;
    }

    // ==================== PRIMARY EQUIPMENT ====================

    switch (SavedLoadout.PrimarySlotType)
    {
    case EPrimarySlotType::Weapon:
        if (SavedLoadout.PrimaryWeapon)
        {
            Result.PrimaryWeapon.WeaponEntry = ResolveWeaponEntry(
                SavedLoadout.PrimaryWeapon, SavedLoadout.PrimaryWeaponInstance, TEXT("PrimaryWeapon"));
            Result.PrimaryWeapon.InitializeFromWeapon();
            Result.PrimaryWeapon.AssignedAbilities = SavedLoadout.PrimaryWeaponAbilities;
            Result.PrimaryWeapon.AssignedAugmentStoneAbilities = SavedLoadout.PrimaryAugmentStoneAbilities;
            Result.PrimaryWeapon.StanceOverride = SavedLoadout.PrimaryWeaponStanceOverride;

            FString CrystalDesc = TEXT("none");
            if (Result.PrimaryWeapon.WeaponEntry.HasCrystal())
            {
                const FRuntimeAttachedItem &Att = Result.PrimaryWeapon.WeaponEntry.AttachedItem;
                if (Att.IsCrystal())
                {
                    CrystalDesc = ItemIdentity::GetDisplayName(Att.Crystal.Id);
                }
                else if (Att.IsEvolution() && Att.Evolution.Item)
                {
                    CrystalDesc = Att.Evolution.Item->ItemName;
                }
            }
            UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] Weapon '%s' HasCrystal=%d Crystal=%s"),
                   *SavedLoadout.PrimaryWeapon->Name,
                   Result.PrimaryWeapon.WeaponEntry.HasCrystal(),
                   *CrystalDesc);
        }
        break;

    case EPrimarySlotType::Ring:
        if (SavedLoadout.PrimaryRing)
        {
            Result.PrimaryRing.RingEntry = ResolveRingEntry(
                SavedLoadout.PrimaryRing, SavedLoadout.PrimaryRingInstance, TEXT("PrimaryRing"));
            Result.PrimaryRing.InitializeFromRing();
        }
        break;

    case EPrimarySlotType::Evolution:
        Result.PrimaryEvolution.Item = SavedLoadout.PrimaryEvolution;
        Result.PrimaryEvolution.CurrentDurability =
            SavedLoadout.PrimaryEvolution ? SavedLoadout.PrimaryEvolution->MaxDurability : 0;
        // Asset-tier fallback: always initialize .Tier from the asset. The instance-resolved
        // branch below overwrites it with the owned entry's leveled Tier when a valid ref resolves.
        Result.PrimaryEvolution.Tier =
            SavedLoadout.PrimaryEvolution ? SavedLoadout.PrimaryEvolution->Tier : EItemTier::F_Tier;

        // (iii-b) Retain the owned-entry identity instead of dropping it after the resolve below —
        // runtime removal/break paths read this to dismantle the right owned FEvolutionInventoryEntry.
        Result.PrimaryEvolutionInstance = SavedLoadout.PrimaryEvolutionInstance;

        // Shape-B: a valid + found evolution instance ref carries the OWNED
        // entry's rolled state (Generated stats/resistance + pools) onto the
        // attachment. Item/durability/spells handling above and below is
        // identical on both branches — only the rolled state is sourced.
        if (SavedLoadout.PrimaryEvolutionInstance.IsValid())
        {
            const FEvolutionInventoryEntry *Found = nullptr;
            if (OwnedEvolutions)
            {
                for (const FEvolutionInventoryEntry &Owned : OwnedEvolutions->Entries)
                {
                    if (Owned.InstanceID == SavedLoadout.PrimaryEvolutionInstance)
                    {
                        Found = &Owned;
                        break;
                    }
                }
            }
            if (Found && Found->Item != SavedLoadout.PrimaryEvolution)
            {
                UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] PrimaryEvolution instance ref %s resolves to a DIFFERENT asset — ignoring instance roll"),
                       *SavedLoadout.PrimaryEvolutionInstance.ToString());
                Found = nullptr;
            }
            if (Found)
            {
                Result.PrimaryEvolution.Tier = Found->Tier; // instance (leveled) Tier — overrides the asset fallback
                Result.PrimaryEvolution.GeneratedStatBonus = Found->GeneratedStatBonus;
                Result.PrimaryEvolution.GeneratedResistance = Found->GeneratedResistance;
                Result.PrimaryEvolution.StatPool = Found->StatPool;
                Result.PrimaryEvolution.StatMaxPool = Found->StatMaxPool;
                Result.PrimaryEvolution.ResistancePool = Found->ResistancePool;
                Result.PrimaryEvolution.ResistanceMaxPool = Found->ResistanceMaxPool;
                UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] PrimaryEvolution resolved owned instance %s"),
                       *SavedLoadout.PrimaryEvolutionInstance.ToString());
            }
            else if (OwnedEvolutions)
            {
                UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] PrimaryEvolution instance ref %s set but NOT FOUND in owned evolutions — asset state only"),
                       *SavedLoadout.PrimaryEvolutionInstance.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] PrimaryEvolution instance ref set but no evolution-inventory context — asset state only"));
            }
        }

        Result.EvolutionSpells = SavedLoadout.EvolutionSpells;
        break;

    case EPrimarySlotType::None:
        // Empty primary — equip nothing; PrimaryWeapon/Ring/Evolution stay default.
        break;
    }

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    if (SavedLoadout.RequiredClass == ECharacterClass::Generic)
    {
        Result.SecondarySlotType = SavedLoadout.SecondarySlotType;

        if (SavedLoadout.SecondarySlotType == ESecondarySlotType::Weapon && SavedLoadout.SecondaryWeapon)
        {
            Result.SecondaryWeapon.WeaponEntry = ResolveWeaponEntry(
                SavedLoadout.SecondaryWeapon, SavedLoadout.SecondaryWeaponInstance, TEXT("SecondaryWeapon"));
            Result.SecondaryWeapon.InitializeFromWeapon();
            Result.SecondaryWeapon.AssignedAbilities = SavedLoadout.SecondaryWeaponAbilities;
            Result.SecondaryWeapon.AssignedAugmentStoneAbilities = SavedLoadout.SecondaryAugmentStoneAbilities;
            Result.SecondaryWeapon.StanceOverride = SavedLoadout.SecondaryWeaponStanceOverride;
        }
    }

    // ==================== RESONATOR RINGS ====================

    if (SavedLoadout.RequiredClass == ECharacterClass::Resonator)
    {
        for (const FResonatorRingSlot &Slot : SavedLoadout.EquippedRings)
        {
            if (Slot.Ring)
            {
                FRingLoadoutEntry RingEntry;
                RingEntry.RingEntry = ResolveRingEntry(Slot.Ring, Slot.RingInstance, TEXT("ResonatorRingSlot"));
                RingEntry.InitializeFromRing();
                // Per-loadout spell overrides flow through to the inventory
                // entry's AssignedSpells override list (empty list = use the
                // ring's DefaultSpells, set in CreateFromRing above).
                if (Slot.AssignedSpells.Num() > 0)
                {
                    RingEntry.RingEntry.AssignedSpells = Slot.AssignedSpells;
                }
                Result.RingLoadout.Add(RingEntry);
            }
        }
    }

    // ==================== CASTER INNATE SPELLS ====================

    if (SavedLoadout.RequiredClass == ECharacterClass::Caster)
    {
        Result.InnateSpells = SavedLoadout.InnateSpells;
        Result.BDSpellPools = SavedLoadout.BDSpellPools;
    }

    // ==================== ITEMS ====================
    // Slot gets the designer's authored CrystalId + Quantity directly. The
    // auto-equip flag is propagated so ULoadoutComponent::PrepareForBattle
    // can call FCombatLoadout::ApplyAutoEquip, which refills each slot from
    // UCrystalInventoryComponent at combat start when the flag is true.
    Result.bAutoEquipItemsOnCombatStart = SavedLoadout.bAutoEquipItemsOnCombatStart;
    Result.ItemSlots.SetNum(InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
    for (int32 i = 0; i < SavedLoadout.EquippedItems.Num() && i < Result.ItemSlots.Num(); ++i)
    {
        Result.ItemSlots[i].CrystalId = SavedLoadout.EquippedItems[i].CrystalId;
        Result.ItemSlots[i].Quantity = SavedLoadout.EquippedItems[i].Quantity;
    }

    // ==================== COSMETICS & DEFENSE ====================

    Result.bShowPrimary = SavedLoadout.bShowPrimary;

    UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] Created from SavedLoadout '%s' (PrimarySlotType: %d)"),
           *SavedLoadout.LoadoutName, static_cast<int32>(SavedLoadout.PrimarySlotType));
    return Result;
}

// ==================== AUTO-EQUIP ====================

void FCombatLoadout::ApplyAutoEquip(FCombatLoadout &Loadout, AActor *OwningActor)
{
    if (!OwningActor)
    {
        return;
    }
    if (!Loadout.bAutoEquipItemsOnCombatStart)
    {
        return;
    }

    UCrystalInventoryComponent *CrystalInv =
        OwningActor->FindComponentByClass<UCrystalInventoryComponent>();
    if (!CrystalInv)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[FCombatLoadout::ApplyAutoEquip] No UCrystalInventoryComponent on %s; skipping auto-equip"),
               *OwningActor->GetName());
        return;
    }

    // Top each non-full slot up to the per-slot cap, debiting the inventory
    // pool. Min(Available, Capacity) guarantees we never over-equip or pull
    // more than the inventory holds.
    for (FItemLoadoutSlot &Slot : Loadout.ItemSlots)
    {
        if (Slot.CrystalId.Type == ECrystalType::None)
        {
            continue;
        }
        if (Slot.Quantity >= InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT)
        {
            continue;
        }

        const int32 Available = CrystalInv->GetItemCount(Slot.CrystalId);
        const int32 Capacity = InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT - Slot.Quantity;
        const int32 ToEquip = FMath::Min(Available, Capacity);
        if (ToEquip > 0)
        {
            CrystalInv->RemoveItemCount(Slot.CrystalId, ToEquip);
            Slot.Quantity += ToEquip;
        }
    }
}