// FRingLoadoutEntry.cpp
// Ring loadout entry implementation

#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Loadout/SkillSlotList.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Equipment/Crystals/CrystalEffectTable.h"

int32 FRingLoadoutEntry::GetLockedSpellCount() const
{
    return 0; // B-518 (D-224, Q7): a locked (conjured) ring is an ordinary empty ring
}

int32 FRingLoadoutEntry::GetCustomizableSpellCount() const
{
    return FMath::Max(0, GetSpellSlotCap() - GetLockedSpellCount());
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpells(bool bAsIfIntact) const
{
    TArray<USpellData *> Result = GetAllSpellsUncapped(bAsIfIntact);
    if (Result.Num() > GetSpellSlotCap())
    {
        Result.SetNum(GetSpellSlotCap());
    }
    return Result;
}

int32 FRingLoadoutEntry::GetSpellSlotCap() const
{
    // D-226 (3): a ring's slots come from its socket's tier (a gem, a fusion's gem half or an evolution), not the built
    // average (skill power keeps that); an empty ring keeps its own tier's. MAX_RING_SPELLS only as the no-ring ceiling.
    if (RingEntry.Ring)
    {
        return CrystalEffectTable::SlotsForContainerTier(
            CrystalEffectTable::GetSocketSkillTier(RingEntry.GetAttachedItem()).Get(RingEntry.Tier));
    }
    return CrystalEffectTable::ResolveSpellSlotCap(
        RingEntry.GetAttachedItem(),
        RingEntry.Ring ? CrystalEffectTable::SlotsForContainerTier(RingEntry.Tier)
                       : LoadoutConstants::MAX_RING_SPELLS);
}

int32 FRingLoadoutEntry::GetSpellsOverCap() const
{
    return FMath::Max(0, GetAllSpellsUncapped().Num() - GetSpellSlotCap());
}

TArray<USpellData *> FRingLoadoutEntry::GetSpellSlots(bool bAsIfIntact) const
{
    // B-518 (D-224): the slots hold only the picks; a broken crystal gives none.
    const bool bPicksPlay = RingEntry.AttachedItem.IsSpellSource() && (bAsIfIntact || RingEntry.AttachedItem.CanProvideSpells());
    return SkillSlotList::Resolve(TArray<USpellData *>(), bPicksPlay ? RingEntry.SpellPicks : TArray<USpellData *>(), GetSpellSlotCap());
}

bool FRingLoadoutEntry::ReplaceSpell(int32 Slot, USpellData *Spell)
{
    // Review: picks need a spell-giving crystal or evolution in the element socket (a broken one keeps its list).
    if (!RingEntry.AttachedItem.IsSpellSource() || Slot >= GetSpellSlotCap() ||
        !SkillSlotList::Replace(RingEntry.SpellPicks, Slot, Spell))
    {
        return false;
    }
    RingEntry.bSpellSlotsActive = true;
    return true;
}

bool FRingLoadoutEntry::RestoreSpell(int32 Slot)
{
    if (!SkillSlotList::Restore(RingEntry.SpellPicks, Slot))
    {
        return false;
    }
    RingEntry.bSpellSlotsActive = true;
    return true;
}

FString FRingLoadoutEntry::GetSpellSlotString() const
{
    const TArray<USpellData *> Slots = GetSpellSlots();
    FString Out = FString::Printf(TEXT("%s spell slots: %d (picks only, B-518)"), *GetNameSafe(RingEntry.Ring), Slots.Num());
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        Out += FString::Printf(TEXT("\n  [%d] %s"), Slot, *GetNameSafe(Slots[Slot]));
    }
    return Out;
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpellsUncapped(bool bAsIfIntact) const
{
    // B-518: the one list is the list (the preset and AssignedSpells merge went).
    TArray<USpellData *> Spells = GetSpellSlots(bAsIfIntact);
    Spells.RemoveAll([](const USpellData *Spell) { return Spell == nullptr; });
    return Spells;
}

TArray<USpellData *> FRingLoadoutEntry::GetLockedSpells() const
{
    TArray<USpellData *> Result;

    if (!RingEntry.Ring)
    {
        return Result;
    }

    const int32 LockedCount = GetLockedSpellCount();
    for (int32 i = 0; i < LockedCount && i < RingEntry.Ring->PresetSpells.Num(); ++i)
    {
        Result.Add(RingEntry.Ring->PresetSpells[i]);
    }

    return Result;
}

TArray<USpellData *> FRingLoadoutEntry::GetCustomizableSpells() const
{
    return RingEntry.GetSpells();
}

bool FRingLoadoutEntry::ValidateSpells(const FSpellCollection &OwnedSpells) const
{
    if (!IsValid())
    {
        return true; // Invalid ring = nothing to validate
    }

    const ESpellElement RingElement = RingEntry.GetElement();

    for (USpellData *Spell : RingEntry.AssignedSpells)
    {
        if (!Spell)
            continue;
        if (!OwnedSpells.HasSpell(Spell))
            return false;
        if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, RingElement))
            return false;
    }

    // Customisable count must fit in the unlocked partition.
    if (RingEntry.AssignedSpells.Num() > GetCustomizableSpellCount())
        return false;
    return true;
}

void FRingLoadoutEntry::InitializeFromRing()
{
    // RingEntry.AssignedSpells is a pure override list — it starts empty.
    // Preset spells are merged in at query time by GetAllSpells().
    RingEntry.AssignedSpells.Empty();
}
