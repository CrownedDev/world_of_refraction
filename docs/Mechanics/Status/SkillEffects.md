# Skill Effects — Authoring & Rules

**Status:** Live and working as designed (branch `feature/dynamic-skill-effects`). This is the rules / designer view — for code internals see `docs/Architecture/SkillEffectSystem.md`.

## Concept

A **skill effect** is anything that happens to a combatant beyond raw damage: a buff, a debuff, a damage-over-time, a heal, a shield, a stun. Effects are **authored once** as reusable bundles and **referenced** by the weapons, rings, evolutions, and skills that grant them. The same buff coming from two of your items doesn't double up — it **merges** into one (unless you mark it stackable).

Effects can be **always-on** (a flat bonus while equipped), **conditional** (fire only when something happens), or **timed consequences** (apply for N turns then expire).

## Authoring model — bundles, referenced

You don't write effects inline on a weapon any more. Instead:

1. Author an **Effect Definition** (a data asset) with a display name, a price (for the future shop), and a list of effects.
2. **Reference** that definition from any number of weapons / rings / evolutions / skills.

Edit the definition once and every item that references it updates. The same definition referenced by your weapon *and* your ring is understood to be the **same effect** — so it merges on whoever it lands on, rather than applying twice.

### One effect = conditions + payloads

Each effect inside a bundle has two halves:

- **Conditions** — *when* it fires (the gate). A list of entries; leave it empty for "always".
- **Payloads** — *what happens* (one or more). Each payload is one applied thing: a type (e.g. DamageBuff, DOT, HealthRestore), a strength, a duration, and **who it lands on** (target type + count).

One effect can carry several payloads — e.g. "on parry: buff myself **and** debuff the attacker" is one effect, two payloads with different targets.

### Authoring limits (and why)

- **≤ 10 effects per definition.**
- **≤ 9 payloads per effect.**

These aren't arbitrary — they're the budget of the stable-identity numbering that lets the same effect merge correctly across sources. The editor flags a definition that exceeds either. If you need more, split into a second definition.

## Conditions — when an effect fires

Each condition entry has four parts:

| Part | Meaning |
|---|---|
| **Trigger** | The event/state: `Always`, HP/Energy thresholds, `OnHit`/`OnCrit`/`OnKill`, turn events, and the defense outcomes below. |
| **Threshold** | For HP/Energy triggers — the % cutoff (e.g. HP below 30%). |
| **Subject** | *Whose* state/outcome to check: **Self**, **Self Team** (any ally), **Target**, **Target Team** (any enemy). |
| **Combine** | How this entry joins the previous one: **And** / **Or**. |

**Subject** is the universal "whose" axis — it applies to *every* trigger, not just defense ones. "When **my** HP is low" vs "when **the target's** HP is low" is just a Subject change. **Team scopes fire if ANY member qualifies** — "Self Team, HP below 25%" means "if any ally is below 25%".

Mix **And**/**Or** across entries to build compound gates, e.g. *"OnPerfectParry AND (Self) HP below 30%"*.

### Defense-outcome triggers

Effects can fire on how a defense resolved, **at the moment of impact**:

| Trigger | Fires on |
|---|---|
| `OnParry` / `OnPerfectParry` | any parry / only a perfect parry |
| `OnBlock` / `OnPerfectBlock` | any block / only a perfect block |
| `OnDodge` / `OnPerfectDodge` | any dodge / only a perfect dodge |
| `OnTakeDamage` | the hit landed (no defense) |

**Superset rule:** a *perfect* outcome fires **both** the perfect trigger **and** its base. So an effect authored `OnParry` triggers on *every* parry (perfect or not); author `OnPerfectParry` if you want the perfect tier only. (There is deliberately no "miss" trigger.)

Defense triggers resolve from **both sides** of the exchange via Subject:
- **Defender's** gear with `Subject = Self`, `OnParry` → "when **I** parry".
- **Attacker's** gear with `Subject = Target`, `OnDodge` → "when **my target** dodges me".

Conditional (triggered) effects are a **gear** concept. Skills fire all of their effects when cast; the conditional-trigger machinery is for equipped items reacting to combat.

## Payloads — what happens, and to whom

Each payload says **who** it lands on:

| Target type | Count | Result |
|---|---|---|
| Self | — | you |
| Enemy | Single / Double / All | the foe you were fighting / 2 random / all foes |
| Ally | Single / Double / All | yourself / self + 1 / whole team |
| Anyone | Single / Double / All | the exchange target / 2 random / everyone |

So *"on perfect parry, heal **all allies**"* is: condition `OnPerfectParry, Self`; payload `HealthRestore → Ally (All)`. The trigger checks the defender; the payload's own targeting decides who benefits.

## Merging, stacking, and fires-once

- **Merge (default).** The same effect (same definition) re-applied to a target refreshes its duration instead of duplicating. Two items granting the same buff = one buff.
- **Stackable (opt-in).** Mark an effect stackable with a max-stacks count and re-applications add stacks (strength scales with stacks) instead of merging.
- **Fires once per match (opt-in).** The effect can apply at most once per combat — re-applications are rejected. Good for "comeback" effects you don't want spamming.

## Worked examples

**"Blessing of the Goddess" (an always-on bundle).**
One Effect Definition, referenced by a ring and a relic. Effect: empty conditions (Always); payload `MaxHPBuff +15% → Self`. Equipped via either item it applies once at combat start; equipped via *both*, it **merges** to a single +15% (not +30%) — because both reference the same definition.

**"Aegis Riposte" (conditional, multi-payload).**
Condition: `OnPerfectParry (Self) AND HP below 30% (Self)`. Payloads: (1) `Shield → Self Team (All)` for 2 turns; (2) `DamageBuff +20% → Self` for 2 turns. Authored on a shield's referenced bundle. At combat start it's armed; when you land a perfect parry *while* under 30% HP, every ally gets shielded and you get the damage buff. A normal (non-perfect) parry doesn't qualify; neither does a perfect parry at full HP.

**"Counter-sting" (attacker reacting to the defender).**
On a weapon, condition `OnDodge (Target)`; payload `EnergyDrain → Enemy (Single)`. When the enemy you attacked dodges you, they lose energy — the *attacker's* gear reacting to the *target's* outcome, expressed purely through Subject = Target.

## How to test (PIE)

Author a gear bundle "on parry, heal all allies" (`OnParry, Self` → `HealthRestore → Ally (All)`), reference it from an equipped item, enter combat, and parry an incoming attack. On the parry, the log shows `[DefenseTrigger] FIRE: …` and allies' HP rises. A pure-threshold conditional (no defense trigger) does **not** fire on the parry — it's correctly skipped by the impact path.
