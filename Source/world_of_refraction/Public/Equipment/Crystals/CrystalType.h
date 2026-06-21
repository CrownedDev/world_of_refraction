// CrystalType.h
// Defines the crystal + augment-stone types for the Items System

#pragma once

#include "CoreMinimal.h"
#include "CrystalType.generated.h"

/**
 * Enum representing the crystal and augment-stone types in World of Refraction
 * Each crystal has a unique effect and color theme
 * Raw crystals = consumable items
 * Refined crystals = slot into weapons/rings
 */
UENUM(BlueprintType)
enum class ECrystalType : uint8
{
    None = 0 UMETA(Hidden),
    Garnet UMETA(DisplayName = "Garnet (Fire - Damage)"),
    Sapphire UMETA(DisplayName = "Sapphire (Water - Healing)"),
    Citrine UMETA(DisplayName = "Citrine (Lightning - Energy)"),
    Emerald UMETA(DisplayName = "Emerald (Wind - Bonus Turn)"),
    Amber UMETA(DisplayName = "Amber (Earth - Defense)"),
    Opal UMETA(DisplayName = "Opal (Light - Crit/Info)"),
    Onyx UMETA(DisplayName = "Onyx (Darkness - Silence)"),
    Amethyst UMETA(DisplayName = "Amethyst (Void - Gambling)"),
    Iolite UMETA(DisplayName = "Iolite (Reality - Cleanse)"),
    Quartz UMETA(DisplayName = "Quartz (None - Status Clear)"),
    DamageStone UMETA(DisplayName = "Damage Stone"),
    // Appended after DamageStone (=12); augment-stone sub-type that grants ability
    // slots. Attach-only (never a consumable). Do NOT reorder/insert above —
    // value is the serialized .uasset/SaveGame identity.
    AbilityStone UMETA(DisplayName = "Ability Stone"),
    // Appended after AbilityStone (=13); augment-stone sub-type that buffs Defense.
    // Same append-only serialized-identity rule — do NOT reorder/insert above.
    DefenseStone UMETA(DisplayName = "Defense Stone"),
    // Appended after DefenseStone (=14); stat-stone sub-types (Crit=15, TurnSpeed=16,
    // Status=17, Efficiency=18). Same append-only serialized-identity rule — do NOT
    // reorder/insert above.
    CritStone UMETA(DisplayName = "Crit Stone"),
    TurnSpeedStone UMETA(DisplayName = "Turn Speed Stone"),
    StatusStone UMETA(DisplayName = "Status Stone"),
    EfficiencyStone UMETA(DisplayName = "Efficiency Stone"),
    // Appended after EfficiencyStone (=18); pool stones (MaxHP=19, MaxEP=20) — raise the
    // MaxHP/MaxEP ceiling via RecomputeMaxPools (Option B; not ESubStat-backed). Same
    // append-only serialized-identity rule — do NOT reorder/insert above.
    MaxHPStone UMETA(DisplayName = "Max HP Stone"),
    MaxEPStone UMETA(DisplayName = "Max EP Stone"),
    // Appended after MaxEPStone; stat-stone sub-type — the magical-damage mirror of
    // DamageStone (DamageStone = physical via RawDamage; SpellDamageStone = magical
    // via SpellDamage). Same append-only serialized-identity rule — do NOT
    // reorder/insert above.
    SpellDamageStone UMETA(DisplayName = "Spell Damage Stone"),
    // Appended after SpellDamageStone; stat-stone sub-type — defender-side status
    // resistance. Attached = blanket resist; consumable = directional (ally raises
    // resist, enemy's goes negative -> amplified status buildup). Same append-only
    // serialized-identity rule — do NOT reorder/insert above.
    ResistanceStone UMETA(DisplayName = "Resistance Stone"),
    // Appended after ResistanceStone; the final two stat-stones — SpellSpeed and
    // ActionSpeed. They speed the visible montage PlayRate today; their COMBAT effect
    // (shrinking the defender's reaction window) is deferred to the Real-Time Defense
    // Rework (docs/Design/RealTimeDefenseRework.md). Append-only — do NOT reorder above.
    SpellSpeedStone UMETA(DisplayName = "Spell Speed Stone"),
    ActionSpeedStone UMETA(DisplayName = "Action Speed Stone"),
    // Appended after ActionSpeedStone; a no-substat, attach-only stone. Its ONLY
    // effect is adding flat durability to an ELEMENTAL fusion it is the stone half
    // of (stacks on the matrix bonus). Contributes 0 to every stat (StoneTargetStat
    // -> None, like the pool stones). Inert alone or in a stone-only fusion. Same
    // append-only serialized-identity rule — do NOT reorder/insert above.
    DurabilityStone UMETA(DisplayName = "Durability Stone"),
    // Appended after DurabilityStone (=26); stat-stone sub-type -> ESubStat::Luck (cluster 5f-B).
    // Luck drives crit chance + crystal-wear break-skip (+ future dodge/drops), so a LuckStone lifts
    // ALL luck consumers uniformly via GetEquipmentModifiedLuck. Same append-only serialized-identity
    // rule — do NOT reorder/insert above.
    LuckStone UMETA(DisplayName = "Luck Stone"),
    // Appended after LuckStone (=28); stat-stone sub-type -> ESubStat::Reflex (Cluster B-3). Reflex
    // widens the defender's input window in the real-time defense duel, so an attached ReflexStone
    // makes a character harder to land hits on. Same append-only serialized-identity rule — do NOT
    // reorder/insert above.
    ReflexStone UMETA(DisplayName = "Reflex Stone")
};