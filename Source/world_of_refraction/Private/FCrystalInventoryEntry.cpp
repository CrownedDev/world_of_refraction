// FCrystalInventoryEntry.cpp
// Runtime crystal inventory entry implementation

#include "FCrystalInventoryEntry.h"
#include "EvolutionItemData.h"

FCrystalInventoryEntry FCrystalInventoryEntry::CreateFromCrystal(UEvolutionItemData *InCrystal)
{
    FCrystalInventoryEntry Entry;
    Entry.Crystal = InCrystal;

    if (InCrystal)
    {
        Entry.CurrentDurability = InCrystal->MaxDurability;
        Entry.InstanceID = FGuid::NewGuid();
    }

    return Entry;
}

// ==================== DURABILITY ====================

bool FCrystalInventoryEntry::IsBroken() const
{
    return Crystal && Crystal->bIsRefined && !Crystal->bImmuneToBreaking && CurrentDurability <= 0;
}

bool FCrystalInventoryEntry::ApplyWear(int32 Amount)
{
    if (Amount <= 0 || CurrentDurability <= 0)
    {
        return false;
    }

    const int32 Before = CurrentDurability;
    CurrentDurability = FMath::Max(0, CurrentDurability - Amount);

    return Before > 0 && CurrentDurability == 0;
}

int32 FCrystalInventoryEntry::RepairBetweenCombats(int32 Amount)
{
    if (!Crystal || Amount <= 0)
    {
        return 0;
    }

    const int32 MaxDur = Crystal->MaxDurability;
    const int32 Before = CurrentDurability;
    CurrentDurability = FMath::Min(MaxDur, CurrentDurability + Amount);

    return CurrentDurability - Before;
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
    return Crystal && Crystal->CanHaveSpells() && !IsBroken();
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
