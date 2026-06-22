# Combat Grid

**Status:** Live — reference doc (created 2026-06-22). Describes shipped behaviour; do not change the implementation from this doc.

> **Related:** [`README.md`](../README.md) (mechanics index), [`../Architecture/CombatOrchestrator.md`](../../Architecture/CombatOrchestrator.md) (startup places actors on the grid).

## Concept

Each team fights on its own **3×3 grid** (3 rows × 3 columns). A combatant's **row** applies a passive
damage/defence modifier; **column** is positional identity (preserved by movement). Position is set at
startup (auto-formation) and changed in-combat by row movement (push / pull / reposition).

## The grid

- `ECombatRow` — `Back = 0`, `Middle = 1`, `Front = 2` (`Combat/Grid/ECombatRow.h`).
- `FCombatGridPosition` — `{ TeamIndex, Row, Column }` with inline `GetDamageModifier()` / `GetDefenseModifier()` (`Combat/Grid/FCombatGridPosition.h`).
- `EMovementCategory` — movement intent (Approach / Return / Dodge / Parry / Block / Reposition).
- Dimensions: `CombatGridConstants::GRID_ROWS = 3`, `GRID_COLUMNS = 3`.

## Row modifiers (passive, ±5%)

`CombatGridConstants` (`Combat/Grid/CombatGridConstants.h`):

| Row | Damage | Defence |
|---|---|---|
| **Front** | ×1.05 (`FRONT_ROW_DAMAGE_MODIFIER`) | ×0.95 (`FRONT_ROW_DEFENSE_MODIFIER`) |
| **Middle** | ×1.00 | ×1.00 |
| **Back** | ×0.95 (`BACK_ROW_DAMAGE_MODIFIER`) | ×1.05 (`BACK_ROW_DEFENSE_MODIFIER`) |

Front trades defence for offence; Back the reverse; Middle is neutral. The defender modifier is applied
as a **divisor** in the damage pipeline (see [`../Architecture/DamageCalculator.md`](../../Architecture/DamageCalculator.md)),
the attacker modifier as a multiplier.

## Entry points (`UCombatGridSubsystem`, `Combat/Grid/CombatGridSubsystem.cpp`)

- `AssignPosition` — place an actor in a grid slot (auto-formation drops one actor per column, middle row).
- `GetDamageModifier(Actor)` / `GetDefenseModifier(Actor)` — resolve the row modifier for damage calc.
- `MoveActorToRow` — change an actor's **row within the same column** (column identity preserved).
- `UpdateAllActorFacing` — orient actors toward their target enemy (same-column front first, else closest).

## Integration points

- **Startup** — `CombatOrchestrator` auto-assigns teams and places/faces actors during `StartCombat`.
- **Damage** — `DamageCalculator::CalculateDamage` reads attacker `GetDamageModifier` (step ~6) and divides by defender `GetDefenseModifier`.
- World transforms are derived from grid slot + arena centre.

## Known TODOs

- No dedicated grid design doc existed before this; row constants are tuning placeholders (±5%) and not yet PIE-balanced against tier-power/tier-gap swings.
- Column identity is preserved by movement, but cross-column movement is not currently modelled (row-only moves).
