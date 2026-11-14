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
#include "Equipment/Apparel/ApparelData.h"
#include "Pool/PoolSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
        if (Spell && Spell->GetSchool() == School)
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
        // B-351 (D-145): the weapon's one skill-slot list once active (else the old merge), so every reader of
        // the loadout's abilities (CanUseAbility, the AI, combat tier, memory conditions) sees the same list.
        Result.Append(PrimaryWeapon.GetSkillSlotAbilities());
    }

    // Secondary weapon abilities (Generic only)
    if (SecondarySlotType == ESecondarySlotType::Weapon && SecondaryWeapon.IsValid())
    {
        Result.Append(SecondaryWeapon.GetSkillSlotAbilities());
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

namespace
{
    const UObject *OwnedGearAsset(const FWeaponInventoryEntry &Entry) { return Entry.Weapon; }
    const UObject *OwnedGearAsset(const FRingInventoryEntry &Entry) { return Entry.Ring; }

    /** Shape-B resolver (B-251: one copy for weapon and ring slots). A slot resolves to its OWNED entry (wholesale
     *  copy: roll, pools, PersistentID and the instance's crystal / spell state) only when the ref is valid, the
     *  context exists, the guid is found AND the found entry's asset matches the slot's. Anything else builds
     *  from the asset. Logging: Verbose for an unset ref / no context (the normal cases), Warning only for a
     *  SET ref that fails to resolve (a real dangling or mismatched reference). */
    template <class TEntry, class TAsset>
    TEntry ResolveOwnedGearEntry(const TArray<TEntry> *Owned, TAsset *Asset, const FGuid &Ref, const TCHAR *SlotName,
                                 const TCHAR *Kind, TEntry (*CreateFromAsset)(TAsset *))
    {
        if (Ref.IsValid())
        {
            if (Owned)
            {
                for (const TEntry &Entry : *Owned)
                {
                    if (Entry.PersistentID != Ref)
                    {
                        continue;
                    }
                    if (OwnedGearAsset(Entry) != Asset)
                    {
                        UE_LOG(LogTemp, Warning,
                               TEXT("[FCombatLoadout] %s instance ref %s resolves to a DIFFERENT asset ('%s' != slot '%s') — falling back to asset build"),
                               SlotName, *Ref.ToString(), *GetNameSafe(OwnedGearAsset(Entry)), *GetNameSafe(Asset));
                        break;
                    }
                    UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] %s resolved owned %s instance %s"), SlotName, Kind, *Ref.ToString());
                    return Entry;
                }
                UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] %s instance ref %s set but NOT FOUND in owned %ss — falling back to asset build"),
                       SlotName, *Ref.ToString(), Kind);
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] %s instance ref set but no inventory context — asset build"), SlotName);
            }
        }
        return CreateFromAsset(Asset);
    }
}

FCombatLoadout FCombatLoadout::CreateFromSavedLoadout(const FSavedLoadout &SavedLoadout)
{
    // Zero-context form — null inventories mean every instance ref takes the
    // asset fallback below, so this is byte-identical to the pre-shape-B path.
    return CreateFromSavedLoadout(SavedLoadout, nullptr, nullptr);
}

FSavedLoadout FCombatLoadout::ToSavedLoadout(ECharacterClass CharacterClass) const
{
    const auto Refs = [](const TArray<USpellData *> &Spells)
    {
        TArray<FSpellRef> Out;
        for (USpellData *Spell : Spells)
        {
            Out.Emplace(Spell);
        }
        return Out;
    };
    FSavedLoadout Saved;
    Saved.LoadoutName = LoadoutName;
    Saved.RequiredClass = CharacterClass;
    Saved.bShowPrimary = bShowPrimary;
    Saved.PrimarySlotType = PrimarySlotType;
    Saved.PrimaryWeapon = PrimaryWeapon.WeaponEntry.Weapon;
    Saved.PrimaryWeaponInstance = PrimaryWeapon.WeaponEntry.PersistentID;
    Saved.PrimaryWeaponAbilities = PrimaryWeapon.AssignedAbilities;
    Saved.PrimaryRing = PrimaryRing.RingEntry.Ring;
    Saved.PrimaryRingInstance = PrimaryRing.RingEntry.PersistentID;
    Saved.PrimaryEvolution = PrimaryEvolution.Item;
    Saved.PrimaryEvolutionInstance = PrimaryEvolutionInstance;
    Saved.EvolutionSpells = Refs(EvolutionSpells);
    Saved.SecondarySlotType = SecondarySlotType;
    Saved.SecondaryWeapon = SecondaryWeapon.WeaponEntry.Weapon;
    Saved.SecondaryWeaponInstance = SecondaryWeapon.WeaponEntry.PersistentID;
    Saved.SecondaryWeaponAbilities = SecondaryWeapon.AssignedAbilities;
    // B-477 (D-209): the sets' off hands, by owned GUID like the main hands.
    Saved.PrimaryOffHandWeapon = PrimaryWeapon.OffHandEntry.Weapon;
    Saved.PrimaryOffHandInstance = PrimaryWeapon.OffHandEntry.PersistentID;
    Saved.SecondaryOffHandWeapon = SecondaryWeapon.OffHandEntry.Weapon;
    Saved.SecondaryOffHandInstance = SecondaryWeapon.OffHandEntry.PersistentID;
    for (const FRingLoadoutEntry &Ring : RingLoadout)
    {
        FResonatorRingSlot &Slot = Saved.EquippedRings.AddDefaulted_GetRef();
        Slot.Ring = Ring.RingEntry.Ring;
        Slot.RingInstance = Ring.RingEntry.PersistentID;
        Slot.AssignedSpells = Refs(Ring.RingEntry.AssignedSpells);
    }
    for (const FApparelEntry &Piece : Apparel) // B-380c
    {
        Saved.Apparel.Add(Piece.Apparel);
        Saved.ApparelInstances.Add(Piece.PersistentID);
    }
    Saved.InnateSpells = Refs(InnateSpells);
    for (const FBDElementSpellPool &Pool : BDSpellPools)
    {
        FSavedBDElementSpellPool &Out = Saved.BDSpellPools.AddDefaulted_GetRef();
        Out.Element = Pool.Element;
        Out.Spells = Refs(Pool.Spells);
    }
    return Saved;
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
    // B-251 (DRY audit A13): one resolver for weapon and ring slots (ResolveOwnedGearEntry).
    const auto ResolveWeaponEntry = [OwnedInventory](UWeaponData *Asset, const FGuid &Ref, const TCHAR *SlotName)
    {
        return ResolveOwnedGearEntry(OwnedInventory ? &OwnedInventory->Weapons : nullptr, Asset, Ref, SlotName, TEXT("weapon"),
                                     &FWeaponInventoryEntry::CreateFromWeapon);
    };
    const auto ResolveRingEntry = [OwnedInventory](URingData *Asset, const FGuid &Ref, const TCHAR *SlotName)
    {
        return ResolveOwnedGearEntry(OwnedInventory ? &OwnedInventory->Rings : nullptr, Asset, Ref, SlotName, TEXT("ring"),
                                     &FRingInventoryEntry::CreateFromRing);
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

        Result.EvolutionSpells = FSpellRef::ExtractSpells(SavedLoadout.EvolutionSpells); // bare runtime — InstanceID rides the saved side (ii-a)
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
            if (SavedLoadout.SecondaryOffHandWeapon) // B-477 (D-209): the secondary set's off hand
            {
                Result.SecondaryWeapon.OffHandEntry = ResolveWeaponEntry(
                    SavedLoadout.SecondaryOffHandWeapon, SavedLoadout.SecondaryOffHandInstance, TEXT("SecondaryOffHandWeapon"));
            }
        }

        // B-477 (D-209): the primary set's off hand (a Generic's sets only).
        if (Result.PrimaryWeapon.IsValid() && SavedLoadout.PrimaryOffHandWeapon &&
            (SavedLoadout.PrimarySlotType == EPrimarySlotType::Weapon || SavedLoadout.PrimarySlotType == EPrimarySlotType::None))
        {
            Result.PrimaryWeapon.OffHandEntry = ResolveWeaponEntry(
                SavedLoadout.PrimaryOffHandWeapon, SavedLoadout.PrimaryOffHandInstance, TEXT("PrimaryOffHandWeapon"));
        }
    }

    // ==================== RESONATOR RINGS ====================

    // B-480 (D-210): a Generic's adapter entry rides the same list (one at most; the loadout check drops more).
    if (SavedLoadout.RequiredClass == ECharacterClass::Resonator || SavedLoadout.RequiredClass == ECharacterClass::Generic)
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
                    RingEntry.RingEntry.AssignedSpells = FSpellRef::ExtractSpells(Slot.AssignedSpells); // bare runtime (ii-a)
                }
                Result.RingLoadout.Add(RingEntry);
            }
        }
    }

    // ==================== CASTER INNATE SPELLS ====================

    if (SavedLoadout.RequiredClass == ECharacterClass::Caster)
    {
        Result.InnateSpells = FSpellRef::ExtractSpells(SavedLoadout.InnateSpells); // bare runtime — InstanceID rides the saved side (ii-a)
        // BD pools are now split (ii-a2): convert each saved FSavedBDElementSpellPool to the bare
        // runtime FBDElementSpellPool (extracts .Spell; the runtime side + combat readers stay bare).
        Result.BDSpellPools.Reset();
        Result.BDSpellPools.Reserve(SavedLoadout.BDSpellPools.Num());
        for (const FSavedBDElementSpellPool &SavedPool : SavedLoadout.BDSpellPools)
        {
            Result.BDSpellPools.Add(SavedPool.ToRuntimePool());
        }
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

    // B-380c (D-164): apparel by owned id from the store (its socket and roll), else built from the asset.
    const UGameInstance *GI = OwnedInventory && OwnedInventory->GetWorld() ? OwnedInventory->GetWorld()->GetGameInstance() : nullptr;
    const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
    for (int32 Index = 0; Index < SavedLoadout.Apparel.Num(); ++Index)
    {
        UApparelData *Asset = SavedLoadout.Apparel[Index];
        const FGuid Ref = SavedLoadout.ApparelInstances.IsValidIndex(Index) ? SavedLoadout.ApparelInstances[Index] : FGuid();
        const FApparelEntry *Owned = Pool ? Pool->FindApparelInPool(Ref) : nullptr;
        if (Owned && Owned->Apparel == Asset)
        {
            ApparelRules::AddApparel(Result, *Owned);
        }
        else if (Asset)
        {
            if (Ref.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[FCombatLoadout] apparel %s: owned piece %s not found - built from the asset"), *Asset->GetName(),
                       *Ref.ToString(EGuidFormats::Short));
            }
            ApparelRules::AddApparel(Result, FApparelEntry::CreateFromApparel(Asset));
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[FCombatLoadout] Created from SavedLoadout '%s' (PrimarySlotType: %d)"),
           *SavedLoadout.LoadoutName, static_cast<int32>(SavedLoadout.PrimarySlotType));
    return Result;
}

// ==================== UNIFIED SLOTS (D-110 mirror, B-302 / B-304) ====================

namespace
{
    /** The old gear fields the fight reads for the class, in slot order: the primary weapon or adapter, a Generic's
     *  secondary weapon, a Resonator's adapter loadout (a loadout authored for another class may carry the other
     *  fields; the getters ignore them, so does the mirror). Visit(Origin, LegacyIndex, WeaponEntry, RingEntry). */
    template <typename FVisit>
    void ForEachReadLegacySlot(const FCombatLoadout &Loadout, ECharacterClass CharacterClass, FVisit &&Visit)
    {
        // A weapon in the primary counts with a Weapon or None type (AutoPopulateLoadout leaves None), as GetActiveWeapon.
        if ((Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None) &&
            Loadout.PrimaryWeapon.IsValid())
        {
            Visit(ELegacySlotOrigin::PrimaryWeapon, INDEX_NONE, &Loadout.PrimaryWeapon, nullptr);
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
        {
            Visit(ELegacySlotOrigin::PrimaryRing, INDEX_NONE, nullptr, &Loadout.PrimaryRing);
        }
        if (CharacterClass == ECharacterClass::Generic && Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid())
        {
            Visit(ELegacySlotOrigin::SecondaryWeapon, INDEX_NONE, &Loadout.SecondaryWeapon, nullptr);
        }
        // B-480 (D-210): a Generic's adapter entry beside its two weapons sits in the adapter loadout too (one at most,
        // the loadout check drops more).
        if (CharacterClass == ECharacterClass::Resonator || CharacterClass == ECharacterClass::Generic)
        {
            for (int32 RingIndex = 0; RingIndex < Loadout.RingLoadout.Num(); ++RingIndex)
            {
                if (Loadout.RingLoadout[RingIndex].IsValid())
                {
                    Visit(ELegacySlotOrigin::RingLoadout, RingIndex, nullptr, &Loadout.RingLoadout[RingIndex]);
                }
            }
        }
    }

    int32 FindSlotByOrigin(const FCombatLoadout &Loadout, ELegacySlotOrigin Origin, int32 LegacyIndex = INDEX_NONE)
    {
        return Loadout.Slots.IndexOfByPredicate([Origin, LegacyIndex](const FLoadoutSlot &Slot)
                                                { return Slot.LegacyOrigin == Origin && Slot.LegacyIndex == LegacyIndex; });
    }

    /** The weapon in hand and the adapter in use, as today's getters pick them (GetActiveWeapon / GetActiveRing /
     *  GetPrimaryRing). An evolution or adapter primary leaves only a Generic's secondary weapon; a Generic with a
     *  weapon (or emptied, sold) primary uses bShowPrimary to choose; other classes use the primary. A Resonator's
     *  adapter is its active ring (none for a stale index); anyone else's is the primary ring. */
    void ComputeActiveSlotIndices(FCombatLoadout &Loadout, ECharacterClass CharacterClass)
    {
        const bool bGeneric = CharacterClass == ECharacterClass::Generic;
        const int32 PrimaryWeaponSlot = FindSlotByOrigin(Loadout, ELegacySlotOrigin::PrimaryWeapon);
        const int32 SecondaryWeaponSlot = FindSlotByOrigin(Loadout, ELegacySlotOrigin::SecondaryWeapon);
        Loadout.ActiveWeaponSlot = INDEX_NONE;
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None)
        {
            Loadout.ActiveWeaponSlot = (bGeneric && !Loadout.bShowPrimary) ? SecondaryWeaponSlot : PrimaryWeaponSlot;
        }
        else if (bGeneric)
        {
            Loadout.ActiveWeaponSlot = SecondaryWeaponSlot;
        }
        Loadout.ActiveAdapterSlot = CharacterClass == ECharacterClass::Resonator
                                        ? FindSlotByOrigin(Loadout, ELegacySlotOrigin::RingLoadout, Loadout.ActiveRingIndex)
                                        : FindSlotByOrigin(Loadout, ELegacySlotOrigin::PrimaryRing);
        if (bGeneric && Loadout.ActiveAdapterSlot == INDEX_NONE)
        {
            // B-480 (D-210): a Generic's adapter entry beside two weapons (the first one in the adapter loadout).
            Loadout.ActiveAdapterSlot = Loadout.Slots.IndexOfByPredicate([](const FLoadoutSlot &Slot) { return Slot.LegacyOrigin == ELegacySlotOrigin::RingLoadout; });
        }
    }
}

void FCombatLoadout::RebuildSlotsFromLegacy(ECharacterClass CharacterClass)
{
    Slots.Reset();
    ForEachReadLegacySlot(*this, CharacterClass,
                          [this](ELegacySlotOrigin Origin, int32 LegacyIndex, const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Ring)
                          {
                              FLoadoutSlot &Slot = Slots.Add_GetRef(Weapon ? FLoadoutSlot::FromWeaponLoadoutEntry(*Weapon)
                                                                           : FLoadoutSlot::FromRingLoadoutEntry(*Ring));
                              Slot.LegacyOrigin = Origin;
                              Slot.LegacyIndex = LegacyIndex;
                          });
    ComputeActiveSlotIndices(*this, CharacterClass);
}

void FCombatLoadout::EnsureSlotsInStep(ECharacterClass CharacterClass)
{
    int32 Walked = 0;
    bool bInStep = true;
    ForEachReadLegacySlot(*this, CharacterClass,
                          [this, &Walked, &bInStep](ELegacySlotOrigin Origin, int32 LegacyIndex, const FWeaponLoadoutEntry *Weapon,
                                                    const FRingLoadoutEntry *Ring)
                          {
                              const UObject *Asset = Weapon ? static_cast<const UObject *>(Weapon->WeaponEntry.Weapon)
                                                            : static_cast<const UObject *>(Ring->RingEntry.Ring);
                              const FGuid &GearID = Weapon ? Weapon->WeaponEntry.PersistentID : Ring->RingEntry.PersistentID;
                              const FLoadoutSlot *Slot = Slots.IsValidIndex(Walked) ? &Slots[Walked] : nullptr;
                              bInStep = bInStep && Slot && Slot->LegacyOrigin == Origin && Slot->LegacyIndex == LegacyIndex &&
                                        Slot->Gear.Asset == Asset && Slot->GearID == GearID;
                              ++Walked;
                          });
    if (!bInStep || Walked != Slots.Num())
    {
        RebuildSlotsFromLegacy(CharacterClass);
        return;
    }
    ComputeActiveSlotIndices(*this, CharacterClass);
}

const FWeaponLoadoutEntry *FCombatLoadout::GetLegacyWeaponEntry(int32 SlotIndex) const
{
    if (!Slots.IsValidIndex(SlotIndex))
    {
        return nullptr;
    }
    switch (Slots[SlotIndex].LegacyOrigin)
    {
    case ELegacySlotOrigin::PrimaryWeapon:
        return &PrimaryWeapon;
    case ELegacySlotOrigin::SecondaryWeapon:
        return &SecondaryWeapon;
    default:
        return nullptr;
    }
}

const FRingLoadoutEntry *FCombatLoadout::GetLegacyRingEntry(int32 SlotIndex) const
{
    if (!Slots.IsValidIndex(SlotIndex))
    {
        return nullptr;
    }
    const FLoadoutSlot &Slot = Slots[SlotIndex];
    if (Slot.LegacyOrigin == ELegacySlotOrigin::PrimaryRing)
    {
        return &PrimaryRing;
    }
    return Slot.LegacyOrigin == ELegacySlotOrigin::RingLoadout && RingLoadout.IsValidIndex(Slot.LegacyIndex) ? &RingLoadout[Slot.LegacyIndex]
                                                                                                               : nullptr;
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
    // Debit routes through the inventory facade so auto-equip emits OnInventoryChanged;
    // CrystalInv is retained for the per-slot availability read below.
    UInventoryComponent *Inv = OwningActor->FindComponentByClass<UInventoryComponent>();
    if (!CrystalInv || !Inv)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[FCombatLoadout::ApplyAutoEquip] No UCrystalInventoryComponent/UInventoryComponent on %s; skipping auto-equip"),
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

        const int32 Available = CrystalInv->GetCount(Slot.CrystalId);
        const int32 Capacity = InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT - Slot.Quantity;
        const int32 ToEquip = FMath::Min(Available, Capacity);
        if (ToEquip > 0)
        {
            Inv->RemoveCrystal(Slot.CrystalId, ToEquip);
            Slot.Quantity += ToEquip;
        }
    }
}