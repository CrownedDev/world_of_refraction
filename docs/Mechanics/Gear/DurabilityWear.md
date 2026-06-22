# Durability Wear

How crystals, refined gems, elemental-fusion halves, and evolution items lose durability when used to infuse
an action, and how they break. Wear is a **percent of the crystal's max durability for its tier** (the
**DurabilityWearPercentRework**, shipped 2026-06-22), so casts-to-break is consistent across tiers rather than
"more casts on a bigger crystal". The infusion-charge rework's "durability cost" axis *is* this wear. See
[`InfusionSystem.md`](../../Architecture/InfusionSystem.md) for the full infusion model.

> Path note: this doc lives under `docs/Mechanics/` (capital M), alongside `TierGap.md`.

## Overview

- An infusion sourced from a **crystal / ring / weapon-crystal / evolution** wears the catalyst's durability.
- **Deterministic wear**, not a probabilistic break-chance — the `% break chance` model was replaced.
- Wear is a **percent of the crystal's max durability** (`round(pct × MaxDurForTier)`), so the same action
  costs the same *fraction* on any tier — casts-to-break is tier-consistent.
- A **real tier mismatch always costs ≥ 1** (min-1 floor); matched / over-spec uninfused casts cost **0**.
- At **0 durability the crystal breaks** (disabled; broken stat reads return 0) and `OnCrystalBroken`
  broadcasts. There is **no wear ceiling** — a sufficiently over-tiered action can **shatter** a crystal in one
  cast (wear ≥ durability), straight from the percent math.
- **`REPAIR_PER_BATTLE = +10`** durability is restored between combats (`DurabilityConstants.h:81`).
- A per-cast **Luck-skip** can negate a whole wear event before it applies — on **both** the refined/fusion and
  the evolution paths.

Source files: `Equipment/Durability/BreakCalculator.cpp`, `Equipment/Durability/DurabilityConstants.h`,
`Equipment/Crystals/CrystalManager.cpp`.

## The formula (three steps)

### Step A — wear percent → base wear (`CalculateWearPercentOfMax` + `CalculateDurabilityWear`)

Wear is accumulated as a **percent of max durability** by the shared helper
`CalculateWearPercentOfMax` (the single source — base amount, detailed results, and all UI/debug derive from it
so they can't drift), then multiplied by the crystal's max durability **once**:

```
TierGap   = GetTierGap(CrystalTier, ActionTier)        // = ActionTier − CrystalTier
mismatch% = (TierGap > 0) ? TierGap × (bIsSpell ? SPELL_WEAR_PCT_PER_GAP : ABILITY_WEAR_PCT_PER_GAP) : 0
          //                 spell 0.07 (7%) / ability 0.03 (3%) per gap tier
level%    = (InfusionLevel == 1) ? (bIsSpell ? SPELL_L1_INFUSION_PCT : ABILITY_L1_INFUSION_PCT)   // 0.05 : 0.0
          : (InfusionLevel >= 2) ? (bIsSpell ? SPELL_L2_INFUSION_PCT : ABILITY_L2_INFUSION_PCT) : 0 // 0.10 : 0.0
TotalPct  = mismatch% + level%
BaseWear  = round( TotalPct × MaxDurabilityForTier(CrystalTier) )
```

- **Tier-match** (`TierGap ≤ 0` — crystal tier ≥ action tier) → `mismatch% = 0`; only the infusion term applies.
- **Tier-mismatch** (crystal *below* the action) → `+7%` (spell) / `+3%` (ability) of max per tier the action
  exceeds the crystal. Tier order `F < E < D < C < B < A < S` (gap is the integer difference).
- **Abilities have no infusion wear add-on** (`ABILITY_L*_INFUSION_PCT = 0`) — only spells gain infusion wear
  (5% / 10%). Spells "create from nothing", so they cost more.
- ⚠️ `CUSTOM_SPELL_WEAR = 2` (`DurabilityConstants.h`) is still a **flat int, defined but UNUSED** by the live
  caller.
- If `BaseWear == 0` (no infusion **and** matched/over-spec tier) → **wear is 0**; substats can't manufacture
  wear from nothing. **Exception:** a real mismatch (`TierGap > 0`) whose base rounds to 0 is floored to **1**
  (see Step C).

### Step B — substat power/control wrap (`CalculateDurabilityWearWithSubstatsDetailed`, `:98-151`)

Wear scales with the caster's stats — a powerful/buffed caster wears crystals **faster**; a disciplined
(efficient/resistant) caster wears them **slower**:

```
PowerFactor   = clamp( 1 + (SpellDamageFrac + StatusMultiplierFrac) × SUBSTAT_AMP , 0.4 , 3.0 )
              //  clamped BOTH ends — PF_MIN 0.4, PF_MAX 3.0 (a geared caster amplifies ≤ ×3, not unbounded)
ControlFactor = clamp( 1 + (EfficiencyFrac + ResistanceFrac) × SUBSTAT_AMP , 0.5 , 3.0 )
Raw           = BaseWear × PowerFactor / ControlFactor
```

The fractions are read from `UCharacterDataComponent::GetEffectiveStats()` (`FEffectiveStats`), the FULL
composed values (innate + equipment + stone + transient) — `CrystalManager.cpp:92-96, 228-232`:

| Frac                   | Source                       | Direction                   |
| ---------------------- | ---------------------------- | --------------------------- |
| `SpellDamageFrac`      | `SpellDamage − 1.0`          | **power** (↑ ⇒ more wear)   |
| `StatusMultiplierFrac` | `StatusMultiplier − 1.0`     | **power** (↑ ⇒ more wear)   |
| `EfficiencyFrac`       | `1.0 − EfficiencyMultiplier` | **control** (↑ ⇒ less wear) |
| `ResistanceFrac`       | `Resistance`                 | **control** (↑ ⇒ less wear) |

Constants: `SUBSTAT_AMP = 5.0`, `SUBSTAT_POWER_FACTOR_MIN = 0.4`, **`SUBSTAT_POWER_FACTOR_MAX = 3.0`**,
`SUBSTAT_CONTROL_FACTOR_MIN = 0.5`, `SUBSTAT_CONTROL_FACTOR_MAX = 3.0`.

### Step C — floor + min-1, no ceiling

The 45%-of-max **ceiling was removed** — shatter now emerges naturally from the percent math (a large mismatch
produces wear ≥ durability). With no ceiling the old `TierGap ≥ 4` "one-shot" branch became identical to the
other arm and **collapsed to a single path**:

```
Floor     = SUBSTAT_FLOOR_FRAC × BaseWear   = 0.25 × BaseWear        // % of BASE WEAR
FinalWear = round( max(Floor, Raw) )                                 // no ceiling
if (TierGap > 0 && FinalWear < 1): FinalWear = 1                     // min-1 mismatch floor
```

- **Floor** = 25% **of the base wear** — control can never reduce wear below ¼ of base.
- **Min-1 mismatch floor** — any real mismatch (`TierGap > 0`) costs **≥ 1** durability even if the modulated
  wear would round to 0, so a control-stacked caster can't grind small mismatches to free casts. This is robust
  even when base wear itself rounds to 0 (the `BaseWear == 0` early return applies the same `TierGap > 0 → 1`
  rule). Matched / over-spec (`TierGap ≤ 0`) stays exactly 0 — the floor is the *mismatch* guarantee, not an
  infusion guarantee (an infused-matched spell whose wear rounds to 0 stays 0).
- **No ceiling** — a big over-reach shatters in one cast. `SUBSTAT_CEIL_FRAC` and `SUBSTAT_ONE_SHOT_GAP` are now
  unreferenced constants (kept in the header, flagged for a dead-constant sweep).
- `bMinFloored` on `FDurabilityWearWithSubstatsResult` is `true` when the min-1 floor fired (debug/preview), and
  `WearPercentOfMax` carries the authored 0–1 fraction for UI / `WouldBreakCrystal` preview.

`WouldBreakCrystal` is the predictive check: `CurrentDurability > 0 && (CurrentDurability − ProposedWear) <= 0`.

## Tier → max durability (`DurabilityConstants.h:20-26`, `GetMaxDurabilityForTier :91-112`)

| Tier           | F   | E   | D   | C   | B   | A   | S   |
| -------------- | --- | --- | --- | --- | --- | --- | --- |
| Max durability | 30  | 40  | 50  | 60  | 70  | 80  | 100 |

HUD shows a low-durability warning below 25% (`LOW_DURABILITY_WARNING_THRESHOLD = 0.25`, `:86`).

## Worked examples

1. **Matched tier, neutral stats, L2 spell, C-tier crystal (max 60).** `TierGap ≤ 0` → mismatch 0%; spell L2
   = 10% → `TotalPct 0.10` → `BaseWear = round(0.10×60) = 6`. Power = Control = 1 → `Raw = 6`. Floor = 1.5 →
   `max(1.5, 6) = 6` → **FinalWear = 6 = 10% of max**. ~10 casts break it.

2. **Geared caster, same cast.** `SpellDamage = 1.4`, `StatusMultiplier = 1.2` → power frac `0.4 + 0.2 = 0.6`,
   `PowerFactor = clamp(1 + 0.6×5, 0.4, 3.0) = 3.0` (**capped** — was ×4.0 pre-rework); control = 1 → `Raw =
   6 × 3 = 18` → **FinalWear = 18 = 30% of max**. ~4 casts. The new `PF_MAX` holds amplification to ×3.

3. **Tier-mismatch, F-tier crystal (max 30), C-tier L2 spell, neutral stats.** `TierGap = C(3) − F(0) = 3` →
   mismatch `3×7% = 21%`; spell L2 10% → `TotalPct 0.31` → `BaseWear = round(0.31×30) = 9`. `Raw = 9`, Floor =
   2.25 → **FinalWear = 9 = 30% of max**. ~4 casts.

4. **Shatter (no ceiling).** S-tier L2 spell (gap 6) on F-tier crystal (max 30): mismatch `6×7% = 42%` + 10%
   = `52%` → `BaseWear = round(0.52×30) = 16`. At neutral stats that's ~half the crystal; a high-power caster
   (`PowerFactor 3.0`) → `Raw = 16 × 3 = 48 > 30` → **shatters in one cast**. Nothing caps it anymore — shatter
   scales with gap **and** power rather than a fixed 45% wall.

5. **Min-1 floor.** 1-tier mismatch **ability** on an F-tier crystal: mismatch `1×3% = 3%` → `BaseWear =
   round(0.03×30) = 1`. A control-stacked caster (`ControlFactor 3.0`, `PowerFactor 0.4`) → `Raw = 1×0.4/3.0 =
   0.13` → rounds to 0, but `TierGap > 0` floors it to **1** (`bMinFloored = true`). A real mismatch always
   costs something.

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

### Luck-skip (both paths)

Before applying wear, the wielder rolls `GetLuckModifiedChance(0, LUCK_BREAK_SKIP_MAX)`. On success the
**entire wear event is skipped** (durability unchanged, no broadcast). Negative ("cursed") luck never skips.
This sits **above** the wear amount — it's all-or-nothing, formula-agnostic. Both `ProcessPostCastWear`
(refined / fusion) **and** `ProcessPostCastEvolutionWear` roll it, using the same `LUCK_BREAK_SKIP_MAX` ceiling,
so Luck protects every crystal type consistently. See [Luck](../Combat/Luck.md).

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

The infusion-charge rework (6-1..6-5) shipped on `feature/realtime-defense`; the rework's "durability cost" axis
**is** this wear. It left the wear *formula* alone — the later **DurabilityWearPercentRework**
(`feature/durability-percent-wear`, 2026-06-22, see changelog) is what converted wear from flat integers to
percent-of-max, capped PowerFactor, removed the ceiling, and added the min-1 mismatch floor. The authoritative
infusion design is [`docs/Architecture/InfusionSystem.md`](../../Architecture/InfusionSystem.md) (durability is
one of its three cost axes); the deep wear architecture is
[`docs/Architecture/CrystalWear.md`](../../Architecture/CrystalWear.md).

## Changelog

| Date       | Change                                                                                                                                                                                                     | Branch                   |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------ |
| 2026-06-17 | Initial documentation of the existing deterministic wear mechanic (formula, tier table, substat power/control wrap, floor/ceiling, consume paths, break/repair). No code change — documents live behavior. | feature/realtime-defense |
| 2026-06-18 | Status update: the infusion-charge rework shipped (6-1..6-5) and left this wear formula unchanged. Reworded the "parked" references; cross-linked the new `InfusionSystem.md` and the retired planning note. | feature/realtime-defense |
| 2026-06-22 | **DurabilityWearPercentRework** (4 commits). Wear converted from flat integers to **percent-of-max** (spell 7%/gap, ability 3%/gap, spell infusion +5/+10, abilities none) via the shared `CalculateWearPercentOfMax` helper → tier-consistent casts-to-break. PowerFactor capped at 3.0 (`SUBSTAT_POWER_FACTOR_MAX`). 45%-of-max **ceiling removed** (branches collapsed; `SUBSTAT_CEIL_FRAC` + `SUBSTAT_ONE_SHOT_GAP` now unused). Added **min-1 mismatch floor** (robust on the `base==0` early return). Luck wear-skip **extended to the evolution path**. Surfaced `WearPercentOfMax` + `bMinFloored` for UI/preview; debug suite regenerated to percent. Rewrote formula Steps A–C, examples, and overview here. | feature/durability-percent-wear |
