// FCrystalInventoryEntry.cpp
// Runtime crystal inventory entry implementation

#include "FCrystalInventoryEntry.h"
#include "ItemData.h"

FCrystalInventoryEntry FCrystalInventoryEntry::CreateFromCrystal(UItemData *InCrystal)
{
    FCrystalInventoryEntry Entry;
    Entry.Crystal = InCrystal;
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

bool FCrystalInventoryEntry::CanProvideSpells() const
{
    return Crystal && Crystal->CanHaveSpells() && !Crystal->IsBroken();
}

bool FCrystalInventoryEntry::IsRefined() const
{
    return Crystal && Crystal->IsRefined();
}

bool FCrystalInventoryEntry::Validate() const
{
    return true; // Crystal-only entry - no spell constraints
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
