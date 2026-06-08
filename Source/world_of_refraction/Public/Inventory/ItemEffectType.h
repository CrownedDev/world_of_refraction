// ItemEffectType.h
// Defines the different types of effects items can have

#pragma once

#include "CoreMinimal.h"
#include "ItemEffectType.generated.h"

/**
 * Enum representing the different types of effects an item can produce
 * Used for categorization and effect application
 */
UENUM(BlueprintType)
enum class EItemEffectType : uint8
{
    Damage UMETA(DisplayName = "Damage (Direct HP damage)"),
    Healing UMETA(DisplayName = "Healing (HP restore)"),
    EnergyRestore UMETA(DisplayName = "Energy Restore (Energy gain)"),
    BuffRawDamage UMETA(DisplayName = "Buff Raw Damage (increase outgoing attack/physical damage)"),
    BuffDefense UMETA(DisplayName = "Buff Defense (Reduce incoming damage)"),
    BuffSpeed UMETA(DisplayName = "Buff Speed (Increase turn speed)"),
    BuffCrit UMETA(DisplayName = "Buff Crit (Increase crit chance)"),
    Silence UMETA(DisplayName = "Silence (Prevent energy gain)"),
    Cleanse UMETA(DisplayName = "Cleanse (Remove debuffs)"),
    Gamble UMETA(DisplayName = "Gamble (Random effects)"),
    // StatusClear — Quartz consumable: clears a portion of the target's status
    // bar. Renamed in place from the removed Transform value (item-system-redesign);
    // see the EItemEffectType Transform->StatusClear redirect in DefaultEngine.ini.
    StatusClear UMETA(DisplayName = "Status Clear (Quartz bar clear)"),
    Repair UMETA(DisplayName = "Weapon Repair"),
    // Appended (=12). No consumable effect — used by attach-only crystals like
    // AbilityStone so UseItem's dispatch finds no case and falls to the default
    // "not a usable consumable" arm. Runtime-derived only (never serialized),
    // so the append needs no CoreRedirect.
    None UMETA(DisplayName = "None (no consumable effect)"),
    // Appended (=13). Directional TurnSpeedStone consumable — pure-stat turn-speed
    // buff/debuff (no turn mechanic; distinct from Emerald's BuffSpeed). Runtime-
    // derived only (never serialized), so the append needs no CoreRedirect.
    BuffTurnSpeed UMETA(DisplayName = "Buff Turn Speed (stone turn-speed buff/debuff)"),
    // Appended (=14). Directional StatusStone consumable — transient StatusMultiplier
    // buff/debuff (Step-5b layer, separate from the attached form's base-stat getter).
    // Runtime-derived only (never serialized), so the append needs no CoreRedirect.
    BuffStatusMultiplier UMETA(DisplayName = "Buff Status Multiplier (stone status buff/debuff)"),
    // Appended (=15). Directional EfficiencyStone consumable — transient Efficiency
    // buff/debuff (folded into GetEffectiveEfficiencyMultiplier; reaches BD/EP/durability).
    // Runtime-derived only (never serialized), so the append needs no CoreRedirect.
    BuffEfficiency UMETA(DisplayName = "Buff Efficiency (stone efficiency buff/debuff)"),
    // Appended (=16/=17). Directional pool-stone consumables — transient MaxHP/MaxEnergy
    // buff/debuff (folded into RecomputeMaxPools via the P2b subscribe trigger). Runtime-
    // derived only (never serialized), so the appends need no CoreRedirect.
    BuffMaxHP UMETA(DisplayName = "Buff Max HP (stone max-HP buff/debuff)"),
    BuffMaxEP UMETA(DisplayName = "Buff Max EP (stone max-EP buff/debuff)"),
    // Appended (=18). Directional SpellDamageStone consumable — transient SpellDamage
    // buff/debuff, read SPELL-only in GetStatusEffectDamageModifier (the magical mirror
    // of BuffRawDamage). Runtime-derived only (never serialized), so the append needs
    // no CoreRedirect.
    BuffSpellDamage UMETA(DisplayName = "Buff Spell Damage (stone spell-damage buff/debuff)"),
    // Appended (=19). Directional ResistanceStone consumable — blanket ModifyStatusResist
    // (ally +mag = more status resist; enemy -mag = vulnerability/amplified buildup).
    // Runtime-derived only (never serialized), so the append needs no CoreRedirect.
    BuffResistance UMETA(DisplayName = "Buff Resistance (stone status-resist buff/debuff)"),
    // Appended. Directional SpellSpeedStone / ActionSpeedStone consumables —
    // SpellSpeedBuff/Debuff (cast montage) and ActionSpeedBuff/Debuff (ability/attack
    // montage) PlayRate. Runtime-derived only, so the appends need no CoreRedirect.
    BuffSpellSpeed UMETA(DisplayName = "Buff Spell Speed (stone cast-speed buff/debuff)"),
    BuffActionSpeed UMETA(DisplayName = "Buff Action Speed (stone action-speed buff/debuff)")
};