// PoolAccessors.h
// The accessor/TRANSLATOR layer — the architectural crux of the structured pool.
//
// Each filterable attribute lives in a different place per type: ELEMENT is on the spell
// ASSET, on the weapon/ring INSTANCE (via the slotted crystal), the evolution ASSET, or the
// crystal KEY; WEAPON-TYPE + SCHOOL are on assets; TIER/QUALITY are on the instance (key for
// crystals). These overloaded accessors hide all of that so a filter loop reads uniformly —
// "element of X" regardless of where X keeps it.
//
// Free-function namespace (not UFUNCTIONs) so BOTH the pool subsystem AND the later run-inventory
// facade share ONE implementation. Bodies live in PoolAccessors.cpp (they deref always-loaded
// data assets — cheap pointer chases, no I/O; pools are hundreds-sized so linear filtering is fine).
//
// Axis applicability: an entry TYPE that lacks an axis (e.g. a weapon has no school; a crystal
// has no quality) is EXCLUDED when that axis is active in the filter — encoded per MatchesFilter
// overload below. INERT: additive, nothing existing calls these.

#pragma once

#include "CoreMinimal.h"
#include "Pool/PoolQueryTypes.h" // FPoolFilter + the entry/crystal types (complete)

namespace PoolAccessors
{
    // ── ELEMENT (unified ESpellElement; resolved from wherever each type keeps it) ──
    ESpellElement GetElement(const FWeaponInventoryEntry &Entry);     // INSTANCE — slotted crystal
    ESpellElement GetElement(const FRingInventoryEntry &Entry);       // INSTANCE — slotted crystal
    ESpellElement GetElement(const FEvolutionInventoryEntry &Entry);  // ASSET — Item->GetAssociatedElement
    ESpellElement GetElement(const FSpellInstance &Instance);         // ASSET — Spell->Element
    ESpellElement GetElement(const FCrystalId &Id);                   // KEY — ItemIdentity::GetElement
    // (abilities have NO element — intentionally no overload)

    // ── TIER (instance for gear/skills, key for crystals) ──
    EItemTier GetTier(const FWeaponInventoryEntry &Entry);
    EItemTier GetTier(const FRingInventoryEntry &Entry);
    EItemTier GetTier(const FEvolutionInventoryEntry &Entry);
    EItemTier GetTier(const FSpellInstance &Instance);
    EItemTier GetTier(const FAbilityInstance &Instance);
    EItemTier GetTier(const FCrystalId &Id);

    // ── QUALITY (instance only — crystals are fungible, no quality) ──
    EItemQuality GetQuality(const FWeaponInventoryEntry &Entry);
    EItemQuality GetQuality(const FRingInventoryEntry &Entry);
    EItemQuality GetQuality(const FEvolutionInventoryEntry &Entry);
    EItemQuality GetQuality(const FSpellInstance &Instance);
    EItemQuality GetQuality(const FAbilityInstance &Instance);

    // ── WEAPON-TYPE (weapon asset; ability's required weapon asset) ──
    EWeaponType GetWeaponType(const FWeaponInventoryEntry &Entry);
    EWeaponType GetWeaponType(const FAbilityInstance &Instance);

    // ── SCHOOL (spell asset only) ──
    ESpellSchool GetSchool(const FSpellInstance &Instance);

    // ── ITEMS BUCKET (browse classification: gem → Crystal, else → AugmentStone) ──
    EItemBucket GetBucket(const FCrystalId &Id);

    // ── MATCH PREDICATES (the consumable API — encode each type's axis applicability) ──
    bool MatchesFilter(const FPoolFilter &F, const FWeaponInventoryEntry &Entry);
    bool MatchesFilter(const FPoolFilter &F, const FRingInventoryEntry &Entry);
    bool MatchesFilter(const FPoolFilter &F, const FEvolutionInventoryEntry &Entry);
    bool MatchesFilter(const FPoolFilter &F, const FSpellInstance &Instance);
    bool MatchesFilter(const FPoolFilter &F, const FAbilityInstance &Instance);
    bool MatchesFilter(const FPoolFilter &F, const FCrystalId &Id);
}
