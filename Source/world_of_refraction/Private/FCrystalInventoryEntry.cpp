// FCrystalInventoryEntry.cpp
// Runtime crystal inventory entry implementation

#include "FCrystalInventoryEntry.h"
#include "ItemData.h"
#include "SpellData.h"

FCrystalInventoryEntry FCrystalInventoryEntry::CreateFromCrystal(UItemData *InCrystal)
{
    FCrystalInventoryEntry Entry;
    Entry.Crystal = InCrystal;
    Entry.CustomSpells.Empty();
    return Entry;
}

// ==================== VALIDATION ====================

bool FCrystalInventoryEntry::GrantsEvolution() const
{
    return Crystal && Crystal->GrantsEvolution();
}

bool FCrystalInventoryEntry::CanHaveSpells() const
{
    return Crystal && Crystal->CanHaveSpells();
}

bool FCrystalInventoryEntry::IsRefined() const
{
    return Crystal && Crystal->IsRefined();
}

bool FCrystalInventoryEntry::Validate() const
{
    if (!Crystal)
    {
        return true; // Empty entry is valid
    }

    if (!CanHaveSpells())
    {
        // Item category should have no custom spells
        return CustomSpells.Num() == 0;
    }

    // Check custom spell count doesn't exceed available slots
    return CustomSpells.Num() <= GetCustomSlotCount();
}

// ==================== ELEMENT ACCESS ====================

ESpellElement FCrystalInventoryEntry::GetElement() const
{
    if (!Crystal)
    {
        return ESpellElement::Generic;
    }
    return Crystal->GetAssociatedElement();
}

// ==================== SPELL ACCESS ====================

TArray<USpellData *> FCrystalInventoryEntry::GetLockedSpells() const
{
    TArray<USpellData *> Result;

    if (!Crystal || !GrantsEvolution())
    {
        return Result; // Only Evolution has locked spells
    }

    int32 LockedCount = Crystal->GetLockedSpellCount();
    const TArray<USpellData *> &CrystalSpells = Crystal->GetSpells();

    for (int32 i = 0; i < LockedCount && i < CrystalSpells.Num(); ++i)
    {
        if (CrystalSpells[i])
        {
            Result.Add(CrystalSpells[i]);
        }
    }

    return Result;
}

TArray<USpellData *> FCrystalInventoryEntry::GetAllSpells() const
{
    TArray<USpellData *> Result;

    if (!Crystal || !CanHaveSpells())
    {
        return Result;
    }

    // Evolution: locked spells first
    if (GrantsEvolution())
    {
        TArray<USpellData *> Locked = GetLockedSpells();
        Result.Append(Locked);
    }
    // Refined: use asset spells as defaults
    else if (Crystal->CanHaveSpells())
    {
        const TArray<USpellData *> &AssetSpells = Crystal->GetSpells();
        for (USpellData *Spell : AssetSpells)
        {
            if (Spell && Result.Num() < CrystalSpellConstants::MAX_SPELL_SLOTS)
            {
                Result.Add(Spell);
            }
        }
    }

    // Add custom spells (player-assigned, fills remaining slots)
    for (USpellData *Spell : CustomSpells)
    {
        if (Spell && Result.Num() < CrystalSpellConstants::MAX_SPELL_SLOTS)
        {
            Result.Add(Spell);
        }
    }

    return Result;
}

int32 FCrystalInventoryEntry::GetLockedSpellCount() const
{
    if (!Crystal || !GrantsEvolution())
    {
        return 0;
    }
    return Crystal->GetLockedSpellCount();
}

int32 FCrystalInventoryEntry::GetCustomSlotCount() const
{
    if (!Crystal || !CanHaveSpells())
    {
        return 0;
    }

    // Refined: all 6 slots are customizable
    // Evolution: 6 - locked count
    return CrystalSpellConstants::MAX_SPELL_SLOTS - GetLockedSpellCount();
}

int32 FCrystalInventoryEntry::GetTotalSpellSlots() const
{
    if (!Crystal || !CanHaveSpells())
    {
        return 0;
    }
    return CrystalSpellConstants::MAX_SPELL_SLOTS;
}

// ==================== SPELL MANAGEMENT ====================

bool FCrystalInventoryEntry::AddCustomSpell(USpellData *Spell)
{
    if (!Spell || !CanHaveSpells())
    {
        return false;
    }

    if (CustomSpells.Num() >= GetCustomSlotCount())
    {
        return false; // At capacity
    }

    CustomSpells.Add(Spell);
    return true;
}

bool FCrystalInventoryEntry::RemoveCustomSpell(USpellData *Spell)
{
    return CustomSpells.Remove(Spell) > 0;
}

void FCrystalInventoryEntry::ClearCustomSpells()
{
    CustomSpells.Empty();
}

bool FCrystalInventoryEntry::SetCustomSpells(const TArray<USpellData *> &Spells)
{
    if (!CanHaveSpells())
    {
        return Spells.Num() == 0;
    }

    if (Spells.Num() > GetCustomSlotCount())
    {
        return false; // Too many spells
    }

    CustomSpells = Spells;
    return true;
}

// ==================== STAT MODIFIERS ====================

bool FCrystalInventoryEntry::HasStatModifiers() const
{
    if (!Crystal || !GrantsEvolution())
    {
        return false;
    }
    return Crystal->HasStatModifiers();
}

FString FCrystalInventoryEntry::GetStatModifierSummary() const
{
    if (!Crystal)
    {
        return TEXT("");
    }
    return Crystal->GetStatModifierSummary();
}

float FCrystalInventoryEntry::GetMindModifierPercent() const
{
    if (!Crystal || !GrantsEvolution())
    {
        return 0.0f;
    }
    return Crystal->GetMindModifierPercent();
}

float FCrystalInventoryEntry::GetBodyModifierPercent() const
{
    if (!Crystal || !GrantsEvolution())
    {
        return 0.0f;
    }
    return Crystal->GetBodyModifierPercent();
}

float FCrystalInventoryEntry::GetSpiritModifierPercent() const
{
    if (!Crystal || !GrantsEvolution())
    {
        return 0.0f;
    }
    return Crystal->GetSpiritModifierPercent();
}
