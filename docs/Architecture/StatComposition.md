# Stat Composition & The Crit/Luck Model

**Status:** Complete — clusters 1–5g committed, and 5f-D editor content (LuckStone `.uasset` authoring)
confirmed shipped (editor-side, confirmed complete). This is the authoritative reference for how every
stat composes and how crit/Luck work.

Cross-reference: [`docs/Design/CombatEconomy_StatRedesign.md`](../Design/CombatEconomy_StatRedesign.md)
(economy + targets) and [`docs/Mechanics/TierGap.md`](../Mechanics/TierGap.md) (the working
tier-gap, untouched by this arc).

---

## 1. The decoupled stat base (Path A)

- `GetEffectiveMind/Body/Spirit` = `1.0 + WorldXLevel × WORLD_X_SCALING_BONUS` (0.07) — the **world
  multiplier ONLY**, range **1.0–1.49**. This dropped the old substat-sum term, which cross-amplified
  every stat in a pillar (the **"snowball"**: spending one stat buffed all siblings; a maxed character
  reached ~23,000 HP).
- Each stat now scales off its **OWN** points × this multiplier. True decoupling — no
  cross-contamination between sibling stats.
- The **formula shape is unchanged**, so the crystal mirror (`GetEvolutionModifiedX`) survives intact.
  `GetBaseX` is kept as **display-only** helpers — do **NOT** call them from the `EffectiveX` getters.
- **World level = the "777" tier** (3 pillars × levels 0–7); each level grants 3 points; max budget
  **93** points (`MAX_STAT_POINTS`).

---

## 2. THE UNIVERSAL RULE

> Every stat: the **stat-derived part caps at +50% ALONE**, then **gear/buffs MULTIPLY it past**,
> toward the **+100% gear ceiling**. No exceptions.

The one composition exception is **pools (HP/EP)**: they cap at a **1000 magnitude**, and gear there is
**ADDITIVE outside the clamp** — correct for pools (a flat pool, not a multiplier).

Per-point **DERIVES** from the cap — never a magic number:

```
per_point = (cap − base) / STAT_DERIVE_DENOM
STAT_DERIVE_DENOM = MAX_STAT_POINTS (93) × WORLD_MAX_MULT (1.49) = 138.57
```

The **cap is the single source of truth** — it is reused directly as the clamp bound. Shared constants:

| Constant | Value | Used by |
| --- | --- | --- |
| `UNIVERSAL_STAT_CAP` | 0.5 | %-stats (Defense, Resistance, Crit-chance contribution, Efficiency, Luck) |
| `STAT_MULT_CAP` | 1.5 | multiplier stats (Raw/Spell damage, StatusMultiplier, speeds) |
| `PCT_STAT_PER_POINT` | derived | %-stats per-point |
| `STAT_MULT_PER_POINT` | derived | multiplier-stats per-point |

---

## 3. Pattern P (the composition rule) + THE TRAP

- **Pattern P (correct):** clamp the **stat-derived part ALONE** at its cap, **THEN** gear/stone/buff
  multiply it past, toward the gear ceiling.
- **Pattern D (wrong — all fixed this arc):** stat + gear + stone + transient clamped **together** → gear
  was stuck *inside* the cap, so maxed-stat gear contributed ~0.
- **⚠️ THE TRAP:** the cap must live on the **LIVE getter the runtime calls** (`GetEvolutionModified*`),
  **NOT** the `Calculate*` display twin. Several `Calculate*` functions carry caps the live path never
  calls — editing them **compiles and does nothing**. Always verify the live path before/after a cap
  change.
- **Gear is multiplicative everywhere** — the existing `BonusX × per-point` magnitude is read as a
  fraction `(1 + term)`, so **no asset re-authoring** was needed.
  - *Inverted-twin nuance (speeds, cluster 5g):* for ActionSpeed/SpellSpeed the live composer is
    `GetEffectiveActionSpeed` / `GetEffectiveSpellSpeed`, which **wraps** the `Calculate*Speed()` stat
    term (here the "twin" IS the live stat input, capped ×1.5) and layers gear/stone/transient + the
    ×2.0 ceiling on top. There is **no** `GetEvolutionModified*Speed` — the new `GetEffective*` getter
    is the live path the runtime calls.

---

## 4. The full stat table

Reproduced from [`CombatEconomy_StatRedesign.md`](../Design/CombatEconomy_StatRedesign.md). Columns:
base value (no investment) → stat cap (maxed stat alone) → gear ceiling (gear/buff on top).

| Stat | Base | Stat cap (+50%) | Gear ceiling (+100%) | Family |
| --- | --- | --- | --- | --- |
| Max Health | 100 | 1000 | additive outside clamp | pool |
| Max Energy | 50 | 1000 | additive outside clamp | pool |
| Raw Damage | ×1.0 | ×1.5 | ×2.0 | multiplier |
| Spell Damage | ×1.0 | ×1.5 | ×2.0 | multiplier |
| Status Multiplier | ×1.0 | ×1.5 | ×2.0 | multiplier |
| Crit Damage | ×1.0 | ×1.5 | ×2.0 | multiplier |
| Action Speed (anim play-rate) | ×1.0 | ×1.5 | ×2.0 | multiplier |
| Spell Speed (cast play-rate) | ×1.0 | ×1.5 | ×2.0 | multiplier |
| Turn Speed | 10 | 15 | 20 | scalar |
| Defense | 0 | 0.5 | 1.0 | %-stat |
| Resistance | 0 | 0.5 | 1.0 | %-stat |
| Crit Chance | 0.05 | 0.50 | 1.00 | %-stat (Luck-sourced) |
| Efficiency (cost reduction) | 0 | 0.5 | 0.9 | %-stat (inverted) |
| Luck (normalized) | 1.0 | — | 2.0 | chance multiplier |

> The ActionSpeed/SpellSpeed ×2.0 gear ceiling (cluster 5g) is on the **animation/cast play-rate**
> channel. The Action-stat's **approach movement** (`CalculateActionSpeed`, ×400 units) is a separate
> channel, not covered by the 5g getters.

**Gear ceilings (constants):**

| Constant | Value |
| --- | --- |
| `STAT_MODIFIER_MAX` | 2.0 |
| `RESISTANCE_MAX` | 1.0 |
| `EFFICIENCY_GEAR_CEILING` | 0.9 |
| `TURN_SPEED_GEAR_CEILING` | 20 |
| `LUCK_GEAR_CEILING` | 2.0 |
| `CRIT_DAMAGE_GEAR_CEILING` | 2.0 |

---

## 5. Per-stat composition notes (the LIVE getter for each)

- **RawDamage / SpellDamage** — composed in `DamageCalculator` (Step 2.5 / 2.6): stat clamped ×1.5, gear
  multiplies toward ×2.0. (`CalculateRawDamage` is **display-only** — not the live path.)
- **ActionSpeed / SpellSpeed (animation/cast play-rate)** — `GetEffectiveActionSpeed` /
  `GetEffectiveSpellSpeed` (cluster 5g): stat `Min(Calculate{Animation|Spell}Speed(), ×1.5)` clamped
  ALONE, then gear (`BonusX`, additive→fraction) × stone × transient multiply toward ×2.0. The 3
  `ActionExecutor` sites (`PlayAttack`/`PlayAbility`/`PlaySpellAnimation`) do
  `PlayRate = BaseAnimSpeed × GetEffective*Speed()`; `BaseAnimSpeed` (montage authoring) and per-action
  `ActionMods` stay at the call site. **Turn order is NOT affected** — it reads raw `GetTotalActionSpeed()`
  / the ×1.5-capped `CalculateSpellSpeed()`, never these getters. Replaced a triplicated, gear-UNCAPPED
  inline compose (was only floored at 0.1).
- **Defense** — `GetDefenderFlatDefense` (converted to a **%** this arc): stat 0.5, stone/buff multiply
  toward 1.0. Was **secretly 0** before the conversion (the re-derived per-point was `RoundToInt`-floored
  to zero) — **revived** by cluster 4.
- **Efficiency** — `GetEffectiveEfficiencyMultiplier`: **inverted** (reduces cost). Reduction caps at
  0.5, gear pushes toward 0.9.
- **TurnSpeed** — `CacheActorStats` uses the **geared/buffed Spirit** (SpiritBuff / pillar % affect it
  like sibling Spirit stats, inside the 15 cap); a dedicated `BonusTurnSpeed` multiplies toward 20.
- **StatusMultiplier** — `GetEffectiveStatusMultiplier` (`CharacterDataComponent.cpp:1203`, clamp at
  `:1240`): a cap was **INTRODUCED this arc** (it was previously uncapped / runaway). Stat ×1.5, gear
  (`BonusStatusMultiplier` + attached `StatusStone`) toward ×2.0, additive transient buff/debuff. The
  **T3 consolidation retired `GetSourceStatusMultiplierFactor`** — `AddStatusBuildup`, the BD overload
  bake, and crystal-wear all read this one getter now (lockstep by construction).
- **Pools (HP/EP)** — `RecomputeMaxPools`: stat clamped to 1000 **alone**, gear **ADDITIVE outside** (the
  composition exception).

---

## 6. The Crit / Luck model

- **Crit CHANCE ← Luck:** `GetEvolutionModifiedCritChance` = `GetLuckModifiedChance(0.05,
  CRIT_CHANCE_LUCK_BONUS 0.45)`. Ramps **5% → 50%** (maxed Luck stat) **→ 100%** (gear).
- **Crit DAMAGE ← the renamed CritDamage stat:** `GetCritDamageMultiplier`, base `CRIT_DMG_BASE` 1.0
  (an un-invested crit deals **NORMAL** damage), stat ramps to 1.5, gear multiplies toward 2.0. The old
  fixed ×1.5 `CRIT_MULTIPLIER` was **removed**.
- A **full crit build needs BOTH**: chance (Spirit/Luck) **and** damage (Mind/CritDamage) —
  **cross-pillar by design**.
- **Luck = the general chance system:** `GetEquipmentModifiedLuck` returns a **NORMALIZED** value (stat
  1.0, gear to 2.0). `GetLuckModifiedChance(base, max)` and `RollLuckChance(base, max)` on
  `UCharacterDataComponent` let **any roll plug in**. Live consumers: **crit chance**, **crystal
  break-skip**. Greenfield (constants exist, consumers not yet built): **dodge**, **drops**.
- **The rename (cluster 5e):** `BonusCritChance → BonusCritDamage`, `ESubStat::CritChance → CritDamage`,
  `CharacterData CritChance → CritDamage` (**16 files, atomic**), with `CoreRedirects` in
  `DefaultEngine.ini`. Crit-chance **CONCEPTS were kept** (`GetCriticalChance`,
  `CritChanceBuff/Debuff/ModifyCritChance` — these are **source-agnostic**).
- **AI parity:** `AIDecisionManager` crit-estimate sites read `GetCritDamageMultiplier(Attacker)`; crit
  chance auto-aligned (the AI calls `GetCriticalChance`, now Luck-sourced).

---

## 7. Gotchas (carry forward)

- **The `Calculate*`-twin trap (§3)** — cap the **LIVE** getter, not the display twin. A cap on the twin
  compiles green and changes nothing.
- **Stale-build-artifact cascade** — a flood of `"StaticStruct is not a member"` errors across the whole
  module is a **stale UHT artifact**, not a code error; a **clean rebuild** fixes it.
  `CombatConstants.h` / `CharacterData.h` constants are **not UHT-processed** and cannot cause it.
- **Gear adds OUTSIDE the clamp** (the +100% path) — **never lower a compose-layer ceiling** thinking
  it's the stat cap. They are different layers.
- **Enums are append-only** (`ESubStat` / `ECrystalType`) — never reorder or reuse a value; rename
  values via `CoreRedirect`.

---

## 8. Cluster 5f — the two-stone system (BUILT, committed)

Two distinct stones — a **Crit Damage stone** (boosts the renamed CritDamage stat) and a **Luck stone**
(boosts Luck, which drives crit chance + break-skip). Committed per-chunk:

- **5f-A — attached Crit Damage stone** (`8dfe29c1`, 1 file): `GetCritDamageMultiplier` now reads
  `GetAttachedStonePercent(.., CritDamage)` (mirrors `GetStoneRawDamageFactor`). The mapping existed but
  was **inert** (no reader); now an attached CritStone multiplies the capped stat toward ×2.0. Inert when
  no CritStone attached.
- **5f-B — Luck stone** (`e85ef7d5`, 5 files): appended `ECrystalType::LuckStone (=26)` →
  `ESubStat::Luck`, plus `IsAugmentStoneType` + `StoneTargetStat` + `GetStoneBasePercent` (shared curve) +
  description. `GetEquipmentModifiedLuck` multiplies the **normalized** luck by the attached LuckStone
  factor (gear-beyond the stat cap) — so one LuckStone lifts **all** luck consumers uniformly (crit
  chance, break-skip).
- **5f-C — consumable split + directional crit-damage consumable** (`63329348`, 8 files): split the stone
  consumables — CritStone → `BuffCritDamage`, LuckStone → `BuffLuck` (routed in
  `ItemIdentity::GetItemEffectType`); **Opal stays crit-chance** (`CritChanceBuff`). So a stone behaves
  the same attached or consumed. The crit-damage consumable is **DIRECTIONAL** like the rest: a paired
  `CritDamageDebuff` effect type was added (mirroring `CritChanceBuff/Debuff`, registered in the enum +
  `IsDebuff` + apply-logic + display names). `GetCritDamageMultiplier` subtracts the debuff (unfloored on
  the debuff side) and **clamps the result to `[CRIT_DMG_BASE 1.0, CRIT_DAMAGE_GEAR_CEILING 2.0]`** — a
  crit-damage debuff drags crits toward ×1.0 (cancels the bonus) but **never below a normal hit**.
- **Enum-value name kept:** `ECrystalType::CritStone` was **not** renamed to `CritDamageStone` (just
  wired) — lower-risk (no enum rename / redirect).

**5f-D — editor content (DONE — editor-side, confirmed complete):** the LuckStone item asset(s) + tiers +
icon and the per-item effect-type assignments are authored. (Stones are **not** roll-generated, so LuckStones
are obtainable only because these were authored.) No code remained.

---

### Changelog

| Date | Change | Branch |
| --- | --- | --- |
| 2026-06-16 | Created — captures the stat decoupling (Path A), the universal +50% stat / +100% gear rule, Pattern P composition + the `Calculate*`-twin trap, the full stat table, per-stat live getters, and the crit/Luck split (clusters 1–5e). 5f stones scoped at the end. | feature/realtime-defense |
| 2026-06-16 | §8 updated to BUILT — two-stone system committed (5f-A `8dfe29c1` attached Crit Damage stone, 5f-B `e85ef7d5` LuckStone, 5f-C `63329348` consumable split + directional crit-damage consumable with the ×1.0 floor). Status line → clusters 1–5f complete; only 5f-D editor content (LuckStone `.uasset`) remains. | feature/realtime-defense |
| 2026-06-16 | Cluster 5g (`5100c0ad`): ActionSpeed + SpellSpeed converted to Pattern P — extracted `GetEffectiveActionSpeed` / `GetEffectiveSpellSpeed` getters (×1.5 stat / ×2.0 gear ceiling), routed the 3 `ActionExecutor` play-rate sites through them, retired the triplicated gear-UNCAPPED inline compose. §4 table speed rows → ×2.0; §5 speed getter note + §3 inverted-twin nuance added. Status → 1–5g. | feature/realtime-defense |
| 2026-06-16 | Doc-sync: §5 StatusMultiplier live getter corrected — `GetSourceStatusMultiplierFactor` was **retired** in the T3 consolidation; the live composer is `UCharacterDataComponent::GetEffectiveStatusMultiplier` (`CharacterDataComponent.cpp:1203`, clamp `:1240`), which `AddStatusBuildup` / BD overload bake / crystal-wear all read. | feature/realtime-defense |
| 2026-06-16 | Status migration: **5f-D editor content marked DONE** (editor-side, confirmed complete) — LuckStone `.uasset` authoring shipped. Status line now reads the full arc complete (no "remains" qualifier). | feature/realtime-defense |
