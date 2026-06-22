# Durability Wear

How crystals, refined gems, elemental-fusion halves, and evolution items lose durability when used to infuse
an action, and how they break. **This documents the CURRENT, live mechanic — it is staying as-is** (confirmed
working). The infusion-charge rework (now shipped, 6-1..6-5) left this wear formula **unchanged**: the rework's
"durability cost" *is* this existing wear (only the infusion-charge **effect/cost amounts elsewhere** changed,
not this). See [`InfusionSystem.md`](../../Architecture/InfusionSystem.md) for the full infusion model.

> Path note: this doc lives under `docs/Mechanics/` (capital M), alongside `TierGap.md`.

## Overview

- An infusion sourced from a **crystal / ring / weapon-crystal / evolution** wears the catalyst's durability.
- **Deterministic wear**, not a probabilistic break-chance — the `% break chance` model was replaced
  (`DurabilityConstants.h:4-7`).
- At **0 durability the crystal breaks** (disabled; broken stat reads return 0) and `OnCrystalBroken`
  broadcasts (`CrystalManager.cpp:142, 156-177`).
- **`REPAIR_PER_BATTLE = +10`** durability is restored between combats (`DurabilityConstants.h:81`).
- A per-cast **Luck-skip** can negate a whole wear event before it applies (`CrystalManager.cpp:121-140`).

Source files: `Equipment/Durability/BreakCalculator.cpp`, `Equipment/Durability/DurabilityConstants.h`,
`Equipment/Crystals/CrystalManager.cpp`.

## The formula (three steps)

### Step A — base wear (`BreakCalculator::CalculateDurabilityWear`, `:11-39`)

An absolute integer:

```
TierGap   = GetTierGap(CrystalTier, ActionTier)        // = ActionTier − CrystalTier  (:22)
mismatch  = (TierGap > 0) ? TierGap × WEAR_PER_TIER_MISMATCH : 0     // 3 per gap tier (:23-26, const :33)
level     = (InfusionLevel == 1) ? (bIsSpell ? SPELL_L1_WEAR : ABILITY_L1_WEAR)      // 6 : 4  (:29-32)
          : (InfusionLevel >= 2) ? (bIsSpell ? SPELL_L2_WEAR : ABILITY_L2_WEAR) : 0  // 12 : 8 (:33-36)
BaseWear  = mismatch + level
```

- **Tier-match** (`TierGap ≤ 0` — crystal tier ≥ action tier) → `mismatch = 0`; base is just the level term.
- **Tier-mismatch** (crystal *below* the action) → `+3` per tier the action exceeds the crystal. Tier order
  `F < E < D < C < B < A < S` (gap is the integer difference).
- Spells wear more than abilities at the same level (6/12 vs 4/8 — *"spells create from nothing"*,
  `DurabilityConstants.h:41`).
- ⚠️ `CUSTOM_SPELL_WEAR = 2` (`DurabilityConstants.h:48`) is **defined but currently UNUSED** — no consumer
  adds it; `CalculateDurabilityWear` does not. (The header's worked-example comment mentions it, but the code
  does not apply it.)
- If `BaseWear == 0` (no infusion **and** matched tier) → **wear is 0**; substats cannot manufacture wear from
  nothing (`BreakCalculator.cpp:115-121`).

### Step B — substat power/control wrap (`CalculateDurabilityWearWithSubstatsDetailed`, `:98-151`)

Wear scales with the caster's stats — a powerful/buffed caster wears crystals **faster**; a disciplined
(efficient/resistant) caster wears them **slower**:

```
PowerFactor   = max( SUBSTAT_POWER_FACTOR_MIN , 1 + (SpellDamageFrac + StatusMultiplierFrac) × SUBSTAT_AMP )
              = max( 0.4 , 1 + (SpellDamageFrac + StatusMultiplierFrac) × 5 )        // :123-125
ControlFactor = clamp( 1 + (EfficiencyFrac + ResistanceFrac) × SUBSTAT_AMP , 0.5 , 3.0 )   // :127-130
Raw           = BaseWear × PowerFactor / ControlFactor                                      // :133
```

The fractions are read from `UCharacterDataComponent::GetEffectiveStats()` (`FEffectiveStats`), the FULL
composed values (innate + equipment + stone + transient) — `CrystalManager.cpp:92-96, 228-232`:

| Frac                   | Source                       | Direction                   |
| ---------------------- | ---------------------------- | --------------------------- |
| `SpellDamageFrac`      | `SpellDamage − 1.0`          | **power** (↑ ⇒ more wear)   |
| `StatusMultiplierFrac` | `StatusMultiplier − 1.0`     | **power** (↑ ⇒ more wear)   |
| `EfficiencyFrac`       | `1.0 − EfficiencyMultiplier` | **control** (↑ ⇒ less wear) |
| `ResistanceFrac`       | `Resistance`                 | **control** (↑ ⇒ less wear) |

Constants: `SUBSTAT_AMP = 5.0`, `SUBSTAT_POWER_FACTOR_MIN = 0.4`, `SUBSTAT_CONTROL_FACTOR_MIN = 0.5`,
`SUBSTAT_CONTROL_FACTOR_MAX = 3.0` (`DurabilityConstants.h:58-67`).

### Step C — floor / ceiling clamp (`:134-149`)

⚠️ **The floor and ceiling use two DIFFERENT denominators:**

```
Floor   = SUBSTAT_FLOOR_FRAC × BaseWear              = 0.25 × BaseWear            // % of BASE WEAR     (:134)
Ceiling = SUBSTAT_CEIL_FRAC  × MaxDurabilityForTier  = 0.45 × MaxDurability(tier) // % of MAX DURABILITY(:144)

if (TierGap >= SUBSTAT_ONE_SHOT_GAP /* 4 */):
    FinalWear = round( max(Floor, Raw) )                       // ceiling LIFTED — shatter allowed   (:137-141)
else:
    FinalWear = round( max(Floor, min(Raw, Ceiling)) )                                              // (:144-147)
```

- **Floor** = 25% **of the base wear** — control can never reduce wear below ¼ of base.
- **Ceiling** = 45% **of max durability** — a single infusion normally wears at most 45% of the crystal's max
  (so ≥3 infusions to break a matched crystal), **unless** the action out-tiers the crystal by **≥ 4 tiers**
  (`SUBSTAT_ONE_SHOT_GAP`), where the ceiling lifts and a single overreach can **shatter** the crystal.

`WouldBreakCrystal` (`BreakCalculator.cpp:73-76`) is the predictive check: `CurrentDurability > 0 &&
(CurrentDurability − ProposedWear) <= 0`.

## Tier → max durability (`DurabilityConstants.h:20-26`, `GetMaxDurabilityForTier :91-112`)

| Tier           | F   | E   | D   | C   | B   | A   | S   |
| -------------- | --- | --- | --- | --- | --- | --- | --- |
| Max durability | 30  | 40  | 50  | 60  | 70  | 80  | 100 |

HUD shows a low-durability warning below 25% (`LOW_DURABILITY_WARNING_THRESHOLD = 0.25`, `:86`).

## Worked examples

1. **Matched tier, neutral stats, L2 spell, C-tier crystal (max 60).** `TierGap ≤ 0` → mismatch 0; level 12 →
   `BaseWear = 12`. Power = Control = 1 → `Raw = 12`. Floor = 3, Ceiling = 27 → `min(12,27)=12`, `max(3,12)=12`
   → **FinalWear = 12 = 20% of max**. ~5 such casts break it.

2. **Geared caster, same cast.** `SpellDamage = 1.4`, `StatusMultiplier = 1.2` → power frac `0.4 + 0.2 = 0.6`,
   `PowerFactor = 1 + 0.6×5 = 4.0`; neutral control = 1 → `Raw = 12 × 4 = 48`. Ceiling = 27 → clamped to
   **27 = 45% of max**. A powerful caster hits the ceiling; 2-3 casts break it.

3. **Tier-mismatch, F-tier crystal (max 30), C-tier L2 spell, neutral stats.** `TierGap = C(3) − F(0) = 3` →
   mismatch `3×3 = 9`; level 12 → `BaseWear = 21`. `Raw = 21`. Ceiling = `0.45 × 30 = 13.5`; `TierGap 3 < 4`
   so ceiling applies → `min(21,13.5)=13.5`, `max(5.25,13.5)=13.5` → **FinalWear ≈ 14 ≈ 47% of max**. ~2 casts.
   If the gap were ≥ 4 (e.g. S-tier action on an E-tier crystal), the ceiling lifts and a single cast can
   shatter it.

## Consume paths

Both call the **same** `CalculateDurabilityWearWithSubstats` formula above — only the target and the
break-permission differ.

### `ProcessPostCastWear` — regular crystals, rings, weapon-crystals (`CrystalManager.cpp:32`)

- **Eligibility:** only **refined crystals** and **elemental fusions (the gem half)** wear; augment
  (stone+stone) fusions never wear; evolution items use the evolution path (`:45-53`). Already-broken
  attachments are skipped (`:55-62`).
- **Wear tier:** the refined crystal's `FCrystalId.Tier`, or for a fusion the **gem half's** tier
  (`:68-70`).
- Routed from `ActionExecutor::ApplyCommitCosts` for the `ActiveRing` / `PrimaryRing` / `WeaponCrystal`
  infusion sources.

### `ProcessPostCastEvolutionWear` — evolution-slot items (`CrystalManager.cpp:200`)

- Wears the active primary-slot **evolution** (`:212-218`), same substat formula.
- ⚠️ **`bForceWear` gates WHETHER wear applies, not the amount** (`:264-269`):
  `bForceWear = IsBrokenDarkness() || (InnateElement == ESpellElement::Reality)`. BD / Reality wielders force
  wear even on an opt-out crystal; everyone else respects the asset's **`bCanBreak`** flag
  (`EvolutionItemData.h:97`, default `false` — evolution crystals are unbreakable unless designers opt in).
  Applied via `LC->ApplyWearToActivePrimaryEvolution(Wear, bForceWear)`.

### Luck-skip (both paths, `CrystalManager.cpp:121-140`)

Before applying wear, the wielder rolls `GetLuckModifiedChance(0, LUCK_BREAK_SKIP_MAX)`. On success the
**entire wear event is skipped** (durability unchanged, no broadcast). Negative ("cursed") luck never skips.
This sits **above** the wear amount — it's all-or-nothing, formula-agnostic.

## Breaking + repair

- `Attachment.ApplyWear(Wear)` decrements durability and returns `bBroke` when it reaches ≤ 0
  (`CrystalManager.cpp:142`).
- On break (`:156-197`): `OnCrystalBroken.Broadcast(Actor, Holder, Payload)` (payload carries the crystal/
  fusion identity). For **fusions**, speed and max-pools recompute (`UTurnManager::OnActorSpeedChanged`,
  `UCharacterDataComponent::RecomputeMaxPools`, `:186-196`) since the broken fusion's stat percents drop to 0;
  plain gems carry no stat percents, so no recompute hook.
- `OnCrystalDurabilityChanged` broadcasts after every wear for live UI (`:154`).
- Auto-repair restores `REPAIR_PER_BATTLE = +10` between combats (`DurabilityConstants.h:81`).

## Relationship to the infusion-charge rework

The infusion-charge rework (6-1..6-5) **shipped** on `feature/realtime-defense` and did **not** change this wear
formula — Crown confirmed it works correctly. The rework's "durability cost" axis **is** this existing wear;
only the infusion-charge *effect* multipliers and the *HP/EP* cost amounts (elsewhere) changed. The original
planning note is retired to `docs/Design/Completed/InfusionChargeRework.md`; the authoritative current design is
[`docs/Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md) (durability is one of its three cost
axes). Any future change to the wear amounts would be documented here as a changelog entry.

## Changelog

| Date       | Change                                                                                                                                                                                                     | Branch                   |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------ |
| 2026-06-17 | Initial documentation of the existing deterministic wear mechanic (formula, tier table, substat power/control wrap, floor/ceiling, consume paths, break/repair). No code change — documents live behavior. | feature/realtime-defense |
| 2026-06-18 | Status update: the infusion-charge rework shipped (6-1..6-5) and left this wear formula unchanged. Reworded the "parked" references; cross-linked the new `InfusionSystem.md` and the retired planning note. | feature/realtime-defense |
