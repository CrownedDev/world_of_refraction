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
    if (!RingEntry.Ring)
    {
        return 0;
    }
    // Use ring's locked status — if spells are locked, all preset spells are locked.
    // Mirrors FWeaponLoadoutEntry::GetLockedAbilityCount.
    if (RingEntry.Ring->bSpellsLocked)
    {
        return RingEntry.Ring->PresetSpells.Num();
    }
    return 0;
}

int32 FRingLoadoutEntry::GetCustomizableSpellCount() const
{
    return FMath::Max(0,
                      CrystalEffectTable::ResolveSpellSlotCap(
                          RingEntry.GetAttachedItem(),
                          RingEntry.Ring ? CrystalEffectTable::SlotsForContainerTier(RingEntry.Tier)
                                         : LoadoutConstants::MAX_RING_SPELLS)
                          - GetLockedSpellCount());
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpells(bool bAsIfIntact) const
{
    TArray<USpellData *> Result = GetAllSpellsUncapped(bAsIfIntact);
    // Locked rings (conjured) keep their presets whole.
    const bool bLocked = RingEntry.Ring && RingEntry.Ring->bSpellsLocked;
    if (!bLocked && Result.Num() > GetSpellSlotCap())
    {
        Result.SetNum(GetSpellSlotCap());
    }
    return Result;
}

int32 FRingLoadoutEntry::GetSpellSlotCap() const
{
    // Gem crystals and evolutions (Q-147) gate spell slots by their own tier; otherwise
    // the cap falls through to the ring's own tier (SlotsForContainerTier), MAX_RING_SPELLS only
    // as the no-ring ceiling.
    return CrystalEffectTable::ResolveSpellSlotCap(
        RingEntry.GetAttachedItem(),
        RingEntry.Ring ? CrystalEffectTable::SlotsForContainerTier(RingEntry.Tier)
                       : LoadoutConstants::MAX_RING_SPELLS);
}

int32 FRingLoadoutEntry::GetSpellsOverCap() const
{
    if (RingEntry.Ring && RingEntry.Ring->bSpellsLocked)
    {
        return 0;
    }
    return FMath::Max(0, GetAllSpellsUncapped().Num() - GetSpellSlotCap());
}

TArray<USpellData *> FRingLoadoutEntry::GetSpellSlots(bool bAsIfIntact) const
{
    const TArray<USpellData *> Presets = RingEntry.Ring ? RingEntry.Ring->PresetSpells : TArray<USpellData *>();
    if (RingEntry.Ring && RingEntry.Ring->bSpellsLocked)
    {
        return Presets;
    }
    // A broken crystal gives no picks; the ring's own presets stay (as in the old merge).
    const bool bPicksPlay = RingEntry.AttachedItem.IsSpellSource() && (bAsIfIntact || RingEntry.AttachedItem.CanProvideSpells());
    return SkillSlotList::Resolve(Presets, bPicksPlay ? RingEntry.SpellPicks : TArray<USpellData *>(), GetSpellSlotCap());
}

bool FRingLoadoutEntry::ReplaceSpell(int32 Slot, USpellData *Spell)
{
    // Review: picks need a spell-giving crystal or evolution in the element socket (a broken one keeps its list).
    if ((RingEntry.Ring && RingEntry.Ring->bSpellsLocked) || !RingEntry.AttachedItem.IsSpellSource() || Slot >= GetSpellSlotCap() ||
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
    const TArray<USpellData *> Presets = RingEntry.Ring ? RingEntry.Ring->PresetSpells : TArray<USpellData *>();
    FString Out = FString::Printf(TEXT("%s spell slots: %d (%s)"), *GetNameSafe(RingEntry.Ring), Slots.Num(),
                                  RingEntry.bSpellSlotsActive ? TEXT("one list, active") : TEXT("one list not edited yet: the old merge plays"));
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        const USpellData *BuiltIn = Presets.IsValidIndex(Slot) ? Presets[Slot] : nullptr;
        const bool bReplaced = SkillSlotList::IsReplaced(RingEntry.SpellPicks, Slot);
        Out += FString::Printf(TEXT("\n  [%d] %s%s"), Slot, *GetNameSafe(Slots[Slot]),
                               bReplaced ? (BuiltIn ? *FString::Printf(TEXT(" (pick; preset %s restorable)"), *GetNameSafe(BuiltIn)) : TEXT(" (pick)"))
                                         : (BuiltIn ? TEXT(" (preset)") : TEXT("")));
    }
    return Out;
}

TArray<USpellData *> FRingLoadoutEntry::GetAllSpellsUncapped(bool bAsIfIntact) const
{
    // Preset spells from the ring asset.
    const TArray<USpellData *> Presets = RingEntry.Ring
                                            ? RingEntry.Ring->PresetSpells
                                            : TArray<USpellData *>();

    // Locked rings (conjured) ignore overrides entirely — presets only.
    if (RingEntry.Ring && RingEntry.Ring->bSpellsLocked)
    {
        return Presets;
    }

    // B-351d (D-145): once the player has edited the one list, it is the list (the position merge below is OBSOLETE).
    if (RingEntry.bSpellSlotsActive)
    {
        TArray<USpellData *> Spells = GetSpellSlots(bAsIfIntact);
        Spells.RemoveAll([](const USpellData *Spell) { return Spell == nullptr; });
        return Spells;
    }

    // Sequential override merge: customisable spells replace preset slots in
    // order; non-null entries beyond the preset count are appended.
    const TArray<USpellData *> Overrides = bAsIfIntact ? RingEntry.AssignedSpells : RingEntry.GetSpells();

    TArray<USpellData *> Result;
    int32 OverrideIndex = 0;

    for (int32 i = 0; i < Presets.Num(); ++i)
    {
        if (OverrideIndex < Overrides.Num() && Overrides[OverrideIndex])
        {
            Result.Add(Overrides[OverrideIndex]);
            ++OverrideIndex;
        }
        else
        {
            Result.Add(Presets[i]);
        }
    }

    while (OverrideIndex < Overrides.Num())
    {
        if (Overrides[OverrideIndex])
        {
            Result.Add(Overrides[OverrideIndex]);
        }
        ++OverrideIndex;
    }

    return Result;
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
