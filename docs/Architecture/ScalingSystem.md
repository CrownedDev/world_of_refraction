# Scaling System

## Overview

**Unified stat scaling** is the Souls-style per-skill scaling model: a skill authors a
list of `(stat, grade)` entries, and each one adds an additive bonus to that skill's
attacker damage multiplier, proportional to how far the attacker's stat exceeds neutral.
Abilities and spells share the **same** path — there is no separate spell scaling.

The core principle: **a skill declares what it scales off, and how steeply.** An empty
scaling list means flat base damage (no scaling) — byte-identical to the pre-arc behavior.
This replaced the old `bOverrideStatScaling` hybrid toggle (a binary Raw↔Spell stat swap);
cross-stat scaling is now authored explicitly via tiers, not a flag.

Key types (`Skills/Definitions/EScalingTier.h`): `EScalingTier` (the S..F grades),
`FStatScaling` (an authored entry), `GetScalingTierCoefficient` (grade → coefficient),
`GetScalingFraction` (stat → normalized fraction). Authored field:
`USkillDataBase::StatScaling`. Consume point: `UDamageCalculator::CalculateDamage` (Step 1,
see `DamageCalculator.md`).

## The grades — `EScalingTier`

`Public/Skills/Definitions/EScalingTier.h` — 7 grades, `uint8`, explicit values 0..6,
**append-only** (never reorder/remove):

`S=0, A=1, B=2, C=3, D=4, E=5, F=6`. There is **no `None` and no `Max`** member — a stat
simply **absent** from a skill's array is the true no-scaling case; **F is the shallowest
real scaling, not zero.**

**Coefficient ladder — `GetScalingTierCoefficient(EScalingTier)`** (inline):

| Tier | S | A | B | C | D | E | F | (default) |
|---|---|---|---|---|---|---|---|---|
| Coeff | 1.0 | 0.8 | 0.65 | 0.5 | 0.35 | 0.2 | 0.05 | 0.0 |

⚠️ **PLACEHOLDER ladder** — the source comment flags this explicitly: *"PLACEHOLDER ladder
(Crown tunes later): S 1.0 -> F 0.05."* Do not treat these as final values.

## The authored entry — `FStatScaling`

`EScalingTier.h`:

```cpp
struct FStatScaling
{
    ESubStat      Stat = ESubStat::None;   // which stat drives this contribution
    EScalingTier  Tier = EScalingTier::C;  // how steeply (S steepest -> F shallowest)
};
```

`ESubStat` is the stat identifier (from `Combat/Actions/ActionStatModifiers.h`). Authored
per-skill as `TArray<FStatScaling> StatScaling` on `USkillDataBase` (shared by abilities +
spells). Empty array = flat base.

## The formula (`DamageCalculator` Step 1)

The attacker multiplier composes the baseline **plus** the authored tier sum:

```
AttackerMult = GetAttackerDamageMultiplier(Attacker, ActionType)   // baseline axis
             + ActionMods.ApplyTo(...)                              // per-action modifiers
             + equipment stat bonus                                 // GetActiveStatBonus
             + Σ over StatScaling:
                 GetScalingTierCoefficient(Entry.Tier)
                 × GetScalingFraction(Entry.Stat, GetEffectiveStatForScaling(Attacker, Entry.Stat))
```

`None`-stat entries are skipped. **Empty `StatScaling` → the sum is 0 → `AttackerMult`
unchanged → prior behavior** (the no-op guard the debug harness verifies).

**The baseline stat axis comes directly from `Input.ActionType`** — `Spell` → `SpellDamage`,
else (`Ability`/`Attack`/`None`) → `RawDamage`. There is **no stat-swap toggle**: the old
`bOverrideStatScaling` / `ScalingType` Raw↔Spell swap was retired, since cross-stat scaling
is now authored via the tiers above (e.g. a "force slash" authors a `RawDamage` tier on a
spell instead of flipping a flag).

## Per-stat normalization — `GetScalingFraction` (Model Y)

Each stat is read in its **own** normalized units, so the coefficient ladder means the same
thing across stats. **Any stat is authorable — there is no allowlist** (only `None`/unmapped
returns 0). The buckets (`EScalingTier.h`):

| Bucket | Stats | Normalization |
|---|---|---|
| Multiplier-family | `RawDamage`, `SpellDamage`, `CritDamage`, `StatusMultiplier`, `SpellSpeed`, `ActionSpeed` | `max(0, v − 1)` (`StatFraction`) |
| Fraction-family | `Defense`, `Resistance` | `max(0, v / UNIVERSAL_STAT_CAP)` (0..0.5 → 0..1) |
| Luck | `Luck` | `max(0, v)` — pre-normalized by `GetEquipmentModifiedLuck` |
| Efficiency (inverted) | `Efficiency` | `max(0, 1 − v)` |
| Turn-economy | `TurnSpeed` | `max(0, (v − TURN_SPEED_BASE) / (TURN_SPEED_CAP − TURN_SPEED_BASE))` |
| Turn-economy | `Reflex` | `max(0, v / WINDOW_CAP_SECONDS)` |
| (none/unmapped) | — | `0.0` |

(`UNIVERSAL_STAT_CAP`, `TURN_SPEED_BASE`/`CAP`, `WINDOW_CAP_SECONDS` are `CombatConstants`.)

## Skill-data changes

On `USkillDataBase` (`Public/Skills/Definitions/SkillDataBase.h`):

- **NEW** — `TArray<FStatScaling> StatScaling` (Category "Combat"). The authored scaling
  entries. ⚠️ The in-source field comment still reads *"NOT yet consumed … authored-but-inert"*
  — that comment is **stale**; the field is **live** (consumed by the Step 1 loop and
  populated by `ActionExecutor`). Trust this doc, not the trailing comment.
- **REMOVED** — `bOverrideStatScaling` (the hybrid stat toggle). Retired with the swap.
- **REMOVED** — `FSkillCastEntry::Damage` (the per-cast base override). Its value migrated to
  `USpellData::BaseDamage` in `PostLoad`. Spells now resolve damage via the **same
  scale-then-split path as abilities**: the scaled `BaseDamage` sliced by the shared
  `DamageSplit` %-table (`ResolveDamageSplit` — empty split → even 100/N per hit).

Also retired but inert: `EStatScalingType.h` (`None/Body/Spirit/Mind`) — the residue of the
old `ScalingType` swap, now referenced by zero source files (see Known Limitations).

## Debug

- **`WoR.ScalingSnapshot`** (console command, `ScalingDebug.cpp`) — snapshots the
  attacker-side scaling for the current-turn actor's available abilities vs the first living
  enemy: resolved ActionType, `StatScaling` entry count, `BaseDamage` in, `AttackerMultiplier`,
  and `FinalDamage`.
- Backed by **`UScalingDebug`** (`UBlueprintFunctionLibrary`) — `CompareScalingSnapshot` /
  `GetScalingSnapshotString` run the real `UDamageCalculator::CalculateDamage` with
  defense/resistance/crit neutralized so only the attacker scaling chain shows.
- Built as a **baseline-capture / regression harness**: snapshot before wiring the tier sum,
  re-run with an empty `StatScaling` array, and the numbers must match (the empty-array no-op
  guard).

## Known Limitations / TODOs

- **PLACEHOLDER coefficients.** The `S 1.0 … F 0.05` ladder is a first pass; Crown tunes the
  values later. Treat them as provisional.
- **Orphaned dead enum.** `EStatScalingType.h` (`None/Body/Spirit/Mind`) is referenced by no
  source file — leftover from the retired `ScalingType` swap. Safe to delete in a later
  cleanup; not part of the shipped scaling system.

## Cross-links

- `DamageCalculator.md` — Step 1 is the consume point (baseline multiplier + the tier sum).
- `StatComposition.md` — the effective-stat accessors the fractions read.
- `InfusionSystem.md` — charge effects compose on top of this attacker multiplier.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-18 | Initial documentation of the unified scaling-tiers arc (stages a..d2c): `EScalingTier` grades + `GetScalingTierCoefficient` (placeholder ladder); `FStatScaling`; the additive tier sum in `DamageCalculator` Step 1 (empty-array no-op); `GetScalingFraction` per-bucket normalization (Model Y, no allowlist); the `USkillDataBase::StatScaling` field; the retired `bOverrideStatScaling` swap + removed per-cast `FSkillCastEntry::Damage` (spells unified onto scaled-base + `DamageSplit`); the `WoR.ScalingSnapshot` / `UScalingDebug` harness. | feature/unified-scaling-tiers |
