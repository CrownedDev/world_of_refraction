# Combat Economy + Stat Redesign — LOCKED REFERENCE

**Status:** Design locked. Implementation = Path A decoupling, clustered, built to the targets below.
Supersedes `StatDecouplingRework.md` / `StatSystemRedesign.md` (folded — those are the pre-decision
exploration record; this is the single source of truth).

---

## Part 1 — Stat model (Path A: decouple + universal 50% cap + gear-beyond)

**Problem (solved):** old `GetEffectiveX` = (sum of ALL pillar substats) × world mult. The substat SUM
cross-amplified — spending one stat buffed every stat in the pillar ("snowball"). Maxed character had
~23,000 HP, damage ×2 for everyone, fights ran dozens of hits. OP + incoherent.

**Fix — Path A** (chosen over ratio model "Path B", which re-introduced NEGATIVE coupling via the focus
denominator):

- `GetEffectiveMind/Body/Spirit` = `1.0 + WorldXLevel × WORLD_X_SCALING_BONUS` (world mult ONLY — drop
  the substat sum). EffectiveX: ~99 → **1.0–1.49**.
- Each stat scales off ITS OWN points × the small EffectiveX. TRUE decoupling — Defense does NOTHING to
  RawDamage. Formula SHAPE unchanged → crystal/gear mirror survives untouched (**the Path A win**).
- WORLD SCALING `0.01 → 0.07` (+7%/level, +49% at world 7) — the progression lever.

**UNIVERSAL RULE: every stat starts neutral, caps at +50% from stats, gear goes beyond. NO EXCEPTIONS.**
Pools (HP/EP) cap at a magnitude (1000); %/multiplier stats cap at +50%. Caps = `FMath::Min`/`Clamp` on
output. Gear is the only way past (gear deferred).

**World level = "777" tier:** 3 world pillars (Mind/Body/Spirit), each 0–7. Tier = 3 digits (777 max,
111 low). Each level grants 3 budget points. Max budget = `30 + (7×3×3)` = **93**.

**Research-backed:** infinite scaling breaks games; %-stats MUST be capped; hard caps over soft (gear
gives past-cap a purpose); start conservative, tune FEEL.

---

## Part 2 — Skill tiers = base power (E33 model; a GUIDE, not a code limit)

Damage = TWO layers:

```
final = SKILL_TIER_BASE_POWER (F→S, authored on the skill)
      × STAT_MULTIPLIER (RawDamage/SpellDamage, ×1→×1.5, gear beyond)
      × crit (1.5)
      − defense (%)
```

**IMPORTANT:** tier base power is an **AUTHORING GUIDE** (the table in Part 4b), **NOT a code-enforced
cap.** Skills author damage directly (as now); the table keeps numbers coherent without limiting design.
Tiers still gate USE via World Stat requirements (existing `FWorldStatRequirements`).

---

## Part 3 — Economy anchors (DESIGNED, known outcomes)

- **MAX HP = 1000** (base 100). **MAX EP = 1000** (base 50).
- **NO passive EP regen** — EP returns via items / effects / BD absorption (parry 100% refund, block
  50%). EP is a precious tactical resource.
- **Damage:** S-tier ≈ 150 final (base ~100 × ×1.5), ~225 crit. vs 1000 HP tank ≈ ~7 hits; vs ~545 HP
  balanced ≈ ~4 hits.
- **S-rank spell ≈ 375 EP → 2–3 casts** (a scarce FINISHER, NOT a rotation — nobody relies on S-rank).
  Low/mid tiers are the rotation. No regen = deliberate spending.

---

## Part 4a — FULL STAT REFERENCE TABLE (decoupled model, locked targets)

Max = 93 pts × world-7 (×1.49). Half ≈ 46 pts at world 7. Shows the curve base → mid → cap.

| Stat | Base | Half (w7) | Max (w7) | Cap | Meaning |
|---|---|---|---|---|---|
| HP | 100 | 545 | 1000 | 1000 | hit points |
| EP | 50 | 520 | 1000 | 1000 | energy (no regen) |
| RawDamage mult | ×1.00 | ×1.25 | ×1.50 | ×1.5 | physical dmg mult |
| SpellDamage mult | ×1.00 | ×1.25 | ×1.50 | ×1.5 | spell dmg mult |
| Defense | 0% | 25% | 50% | 50% | damage reduction |
| Resistance | 0% | 25% | 50% | 50% | status reduction |
| Crit | 5% | 27% | 50% | 50% | crit chance |
| Efficiency | 0% | 25% | 50% | 50% | EP cost reduction |
| Luck | 0% | 25% | 50% | 50% | fortune (crit/dodge/drops) |
| Status Mult | ×1.00 | ×1.25 | ×1.50 | ×1.5 | status buildup scalar |
| Movement speed | ×1.00 | ×1.25 | ×1.50 | ×1.5 | approach speed |
| Spell speed | ×1.00 | ×1.25 | ×1.50 | ×1.5 | cast speed |
| Animation speed | ×1.00 | ×1.25 | ×1.50 | ×1.5 | anim play rate |
| Turn speed | 10 | 12 | 15 | 15 | turn-order value |
| Reflex window | +0% | +12% | +50% | +50% | defense window (+0.25s at max) |

(Half = 46 points in THAT stat at world 7. Per-point derived so max investment REACHES the cap.)

---

## Part 4b — DAMAGE-BY-TIER REFERENCE (tier base power → final; an authoring guide)

| Tier | Base power | No stats | Max stats (×1.5) | Max+crit (×2.25) | vs 1000HP tank |
|---|---|---|---|---|---|
| S | ~100 | 100 | 150 | 225 | ~7 hits |
| A | ~80 | 80 | 120 | 180 | ~8 hits |
| B | ~60 | 60 | 90 | 135 | ~11 hits |
| C | ~45 | 45 | 68 | 101 | ~15 hits |
| D | ~30 | 30 | 45 | 68 | ~22 hits |
| E | ~20 | 20 | 30 | 45 | ~33 hits |
| F | ~12 | 12 | 18 | 27 | ~56 hits |

**AWARENESS NOTE (not a constraint):** the spread is wide — F-tier vs a max tank ≈ 56 hits (use
appropriate-tier skills vs appropriate targets; F-tier is for weak enemies, S-tier for tanks/bosses).
These are guide numbers; author toward them, deviate when a skill needs it.

**ENERGY COST guide** (ladder, tunable): S ~375 (2–3 casts), A ~250, B ~180, C ~120, D ~75, E ~45, F ~25.

---

## Part 5 — Implementation plan (Path A, clustered, built to targets)

Set per-point constants so MAX investment HITS the targets (HP=1000, EP=1000, each %-stat=50% at max,
multipliers ×1.5, Turn 10→15). Derived per-point examples: HP ≈ `(1000−100)/(93×1.49)` ≈ **6.5**;
EP ≈ **6.85**; %-stats solve to their 0.50 target; etc. (Caps clamp anyway — per-point just needs to
REACH the cap at max, not exceed.)

- **Cluster 1 (atomic):** `GetEffectiveX` ×3 + world bump 0.07 + per-point constants derived to hit
  targets. — `CharacterData.h`, `CombatConstants.h`
- **Cluster 2:** cap clamps (HP 1000, EP 1000, %-stats 50% — consider a shared `UNIVERSAL_STAT_CAP=0.5`
  for the %-stats; multipliers ×1.5; movement ×1.5; Turn 15) + cap constants. Clamp the STAT term only;
  gear adds OUTSIDE the clamp (gear-beyond). — `CharacterData.h`, `CombatConstants.h`
- **Cluster 3:** Reflex / speed-penalty window caps (+50%, equal — preserve duel cancel). —
  `DefenseSystem.cpp`
- **Cluster 4:** Defense flat→% (structural — `DamageCalculator` subtraction → multiplication; defer the
  `BonusDefense` flat-int gear field). **Last, isolated.**

**Deferred (not this build):** gear layer (the past-cap path); skill base-power + cost asset authoring
toward the Part 4b guide; EP-regen-via-effects tuning. **TUNE ALL FEEL IN PIE** — these are starting
targets.
