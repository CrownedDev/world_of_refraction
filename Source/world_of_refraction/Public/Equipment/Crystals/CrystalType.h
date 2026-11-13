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
    // B-437 (D-175): the Ability Stone sat here and is removed, so every value below moved down one; the save
    // version rose with it (Q-437: older saves wiped) and assets store the name, not the number.
    // Appended after DamageStone; augment-stone sub-type that buffs Defense. Append-only: do NOT reorder or
    // insert above — the value is the serialized save identity.
    DefenseStone UMETA(DisplayName = "Defense Stone"),
    // Appended after DefenseStone; stat-stone sub-types (Crit, TurnSpeed, Status,
    // Efficiency). Same append-only serialized-identity rule — do NOT reorder/insert above.
    CritStone UMETA(DisplayName = "Crit Stone"),
    TurnSpeedStone UMETA(DisplayName = "Turn Speed Stone"),
    StatusStone UMETA(DisplayName = "Status Stone"),
    EfficiencyStone UMETA(DisplayName = "Efficiency Stone"),
    // Appended after EfficiencyStone; pool stones (MaxHP, MaxEP) — raise the
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
    // Appended after DurabilityStone; stat-stone sub-type -> ESubStat::Luck (cluster 5f-B).
    // Luck drives crit chance + crystal-wear break-skip (+ future dodge/drops), so a LuckStone lifts
    // ALL luck consumers uniformly via GetEquipmentModifiedLuck. Same append-only serialized-identity
    // rule — do NOT reorder/insert above.
    LuckStone UMETA(DisplayName = "Luck Stone"),
    // Appended after LuckStone; stat-stone sub-type -> ESubStat::Reflex (Cluster B-3). Reflex
    // widens the defender's input window in the real-time defense duel, so an attached ReflexStone
    // makes a character harder to land hits on. Same append-only serialized-identity rule — do NOT
    // reorder/insert above.
    ReflexStone UMETA(DisplayName = "Reflex Stone"),
    // Appended after ReflexStone; a CONSUME-ONLY effect stone — instant heal on any target (the
    // heal Sapphire vacated, relocated here; Sapphire is now the defy-death crystal). No attach
    // form: StoneTargetStat/GetStoneBasePercent deliberately omit it, so an attached HealingStone
    // is inert (contributes 0 to every stat). Same append-only serialized-identity rule — do NOT
    // reorder/insert above.
    HealingStone UMETA(DisplayName = "Healing Stone"),
    // B-353 (D-142): the one stat stone; FCrystalId::StoneStat says which sub-stat it amplifies. Appended (append-only).
    // RETIRED (B-353, D-142): the single-stat kinds DefenseStone, CritStone, TurnSpeedStone, StatusStone,
    // EfficiencyStone, SpellDamageStone, ResistanceStone, SpellSpeedStone, ActionSpeedStone, LuckStone and ReflexStone.
    // Their values stay (saves and assets store them); StatStone::Normalize maps them to StatStone on load.
    // Damage and Healing stones stay, and so do the pool stones (Max HP / Max EP, no sub-stat; Q-353).
    StatStone UMETA(DisplayName = "Stat Stone")
};