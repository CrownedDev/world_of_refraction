# Substat Decoupling + World-Level Rework — Planned

**Status:** Planned, own dedicated session. Decided this session (feature/realtime-defense), deferred
to finish the reactive-defense work. A 2-file code edit but a full combat re-tune + playtest.

## The problem (discovered this session)

`GetEffectiveMind/Body/Spirit` currently = (SUM of all substats in the pillar) × (1 + WorldXLevel × 0.01).
The substat-SUM causes CROSS-AMPLIFICATION (a "snowball"): spending one substat (e.g. Defense) raises
the shared pillar multiplier, which buffs EVERY substat in that pillar (e.g. RawDamage gets stronger
even though you only added Defense). Confirmed in code: adding 10 Defense raised RawDamage's multiplier
3.66× → 3.90× without touching RawDamage.

Crown's intent: substats should ONLY affect themselves. "Why should damage affect defense?"

## The fix (two changes)

1. **KILL THE SNOWBALL:** `GetEffectiveX()` returns ONLY the world multiplier (remove the substat sum):
   `GetEffectiveBody() = 1.0f + WorldBodyLevel * WORLD_BODY_SCALING_BONUS`
   Each substat then scales off world level + its OWN points only — no cross-amplification.
2. **MAKE WORLD LEVEL MEANINGFUL:** bump `WORLD_BODY_SCALING_BONUS` (and Mind/Spirit equivalents) from
   `0.01` (+1%/level, max +7%) to `0.07` (+7%/level, max +49%). Currently world level barely scales
   anything; Crown wants it to be a real progression lever. (`0.07` is the starting target — tune in
   playtest. Could land +5%/+7%/+10% — feel it out.)

## The unavoidable cost — re-tuning 16 constants (this is why it's a real session)

Removing the snowball GUTS the multiplier the formulas rely on. The substat sum (~111) was doing the
heavy lifting; the world multiplier alone (max ~1.49) is ~75× smaller. So every PER_POINT constant —
currently tuned against the ~111 multiplier — must be re-tuned UP (≈ × the reference sum ~100-111) or
every stat collapses to near-zero. This is NOT optional — it's arithmetic. CC can calculate behavior-
PRESERVING values (so the game still works post-change); Crown then playtests to dial the actual feel.

The 16 constants (`CombatConstants.h`):

- **Mind (5):** `EFFICIENCY_PER_POINT`, `EFFICIENCY_RING_BREAK_PER_POINT`, `SPELL_DAMAGE_PER_POINT`,
  `CRIT_CHANCE_PER_POINT`, `SPELL_SPEED_PER_POINT`
- **Body (6):** `DEFENSE_PER_POINT`, `MOVEMENT_SPEED_PER_POINT`, `ANIMATION_SPEED_PER_POINT`,
  `RAW_DAMAGE_PER_POINT`, `MAX_HEALTH_PER_POINT`, `REFLEX_WINDOW_PER_POINT` (Reflex — see below)
- **Spirit (5):** `STATUS_MULTIPLIER_PER_POINT`, `MAX_ENERGY_PER_POINT`, `RESISTANCE_PER_POINT`,
  `TURN_SPEED_PER_POINT`, `LUCK_PER_POINT`

(The `*_BASE` constants don't change mechanically but their felt weight shifts — playtest-check.)

## Scope / blast radius

- **CODE:** 2 files (`CharacterData.h` — 3 `GetEffectiveX` bodies; `CombatConstants.h` — ~16 constants +
  world scaling). Crystal/gear layer UNAFFECTED (pure percent — `ApplyEvolutionPillarModifier` returns
  `BaseValue × Modifier` where Modifier is a product of `(1 + percent/100)` factors; proportional, so it
  absorbs the magnitude shift with no re-tune).
- **BALANCE:** HIGH — every combat number shifts at once (damage, defense, HP, EP, crit, speed, turn
  order, status, luck). Concentrated builds NERFED (snowball gone), spread builds BUFFED. AI threat
  scoring (reads composed RawDamage) shifts too. Needs a real playtest pass, not a drive-by.
- **World level keeps BOTH jobs:** 3 points/level (`POINTS_PER_WORLD_STAT_LEVEL` — unchanged) + scaling
  (now exclusive + bumped to +7%).

## Consumer map (who inherits the change for free)

`GetEffectiveX` is read only by the 13 substat `CalculateX` formulas (all in `CharacterData.h`) plus the
runtime crystal-aware mirror `CharacterDataComponent::GetEvolutionModified{Mind,Body,Spirit}`. That mirror
feeds DamageCalculator, StatusBuildupManager, and TurnManager — all of which read the same pillar value
and so inherit the change automatically (no per-consumer edits). `GetBaseMind/Body/Spirit` are consumed
ONLY by `GetEffectiveX` (no `.cpp` callers; BlueprintPure so a BP panel may display the pillar total —
keep them as a display helper, just stop feeding `Effective` from them). `GetTotalSpent()` (budget) is
separate and unaffected.

## Reflex — current state on the defense branch

Reflex is IN the Body pillar (`GetBaseBody`) like the other 4 Body substats — consistent with the current
system. All 13 substats (including Reflex) decouple TOGETHER in this rework.

## Approach when doing it

1. **CC:** change the 3 `GetEffectiveX` formulas + bump world scaling to `0.07` + calculate behavior-
   preserving re-tune values for the 16 constants (so nothing collapses on day one).
2. **Crown:** playtest, adjust by feel (CC values are the safe starting point, not the final answer).
   Watch build diversity (concentrated vs spread), world-level power curve, combat pacing.

## Sequencing

Own session, ideally own branch (a combat rebalance shouldn't ride a feature branch). Do AFTER the
reactive-defense work is committed.
