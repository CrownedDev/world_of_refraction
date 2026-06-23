# Augment Stones

**Status:** Live. Player-facing reference.

## What they are

**Augment stones** grant a **mechanical bonus** — more damage, more defense, faster turns, a
bigger HP pool, and so on. Unlike crystals, **a stone carries no element** — it boosts a stat, it
doesn't give you Fire or Water.

**Most stones have two uses:**

- **Attach** the stone into a weapon for a **permanent** bonus while it stays equipped.
- **Use** the stone as a **consumable** for a **temporary** buff (a few turns), then it's gone.

That's the **dual-form** stone. Like crystals, stones come in tiers **F → E → D → C → B → A → S**,
and higher tiers are stronger.

## ⚠️ The correction: most stones are dual-form

A common misconception is that augment stones are attach-only "gear parts" you can't use as items.
**That's wrong.** **Most augment stones are dual-form** — attach OR consume. **Two stones are
attach-only** (no consumable use): the **Ability Stone** and the **Durability Stone**. And **one
stone is consume-only** — the **Healing Stone** (an instant heal with no attach effect).

## The roster

### Dual-form stat stones (attach permanent / consume temporary)

These attach for a permanent bonus or are used as a temporary consumable buff. The consumable is
usually **directional** — used on an **ally it buffs**, used on an **enemy it debuffs** the same
stat.

| Stone | Boosts |
|---|---|
| **Damage Stone** | physical damage |
| **Spell Damage Stone** | magical (spell) damage |
| **Defense Stone** | defense |
| **Crit Stone** | critical-hit damage |
| **Turn Speed Stone** | how soon your turn comes up |
| **Status Stone** | how hard your hits push the status bar |
| **Efficiency Stone** | energy efficiency (cheaper actions) |
| **Max HP Stone** | maximum HP ceiling |
| **Max EP Stone** | maximum energy ceiling |
| **Resistance Stone** | resistance to status buildup (used on an enemy, it makes status land *harder* on them) |
| **Spell Speed Stone** | spell animation speed |
| **Action Speed Stone** | action animation speed |
| **Luck Stone** | luck (crit chance, plus the odds your equipment survives wear) |
| **Reflex Stone** | reflex (a wider window to defend against incoming hits) |

> **Spell Speed / Action Speed — note:** today these speed up the visible animation. Their
> **combat teeth** — shrinking the *defender's* reaction window when you attack — arrive with the
> **real-time defense rework**. Until then, treat them as animation-speed (and the buildup of the
> other stats) rather than a defensive edge.

**By tier** — every stat stone in the table above shares **one** curve. The number is the **bonus
percent** to its stat, and it's the **same whether the stone is attached or consumed** (see
[Permanent vs temporary](#permanent-vs-temporary)):

| | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| **Stat bonus %** | 3 | 5 | 7 | 9 | 11 | 13 | 15 |

### Attach-only stones (no consumable use)

| Stone | What it does |
|---|---|
| **Ability Stone** | attached to a weapon, it **grants extra ability slots** so the weapon can hold more abilities. There's no "use" form — it only works slotted in. |
| **Durability Stone** | adds **flat durability** to a **fusion** it is part of (see [Fusion Stones](./FusionStones.md)). On its own it does **nothing** — no stat, no consumable use. |

**By tier:**

| Stone | Grants | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|---|
| **Ability Stone** | ability slots | 1 | 2 | 3 | 4 | 5 | 6 | 6 |
| **Durability Stone** | flat durability (to its fusion) | 8 | 15 | 22 | 29 | 36 | 43 | 50 |

> **Ability Stone — A and S both give 6.** S doesn't add a slot over A; the S advantage shows up
> elsewhere (S leads on power, not slot count).

### Consume-only effect stone (no attach use)

| Stone | What it does |
|---|---|
| **Healing Stone** | **Use** to instantly **heal any target** — ally *or* enemy — for a tier-scaled % of their max HP. This is the plain heal **moved off Sapphire** (which is now *defy death* — see [Crystals](./Crystals.md)). Consume-only: it carries no stat, so **attaching it to a weapon does nothing**. |

**By tier:**

| | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| **Healing Stone** — heal (% of target max HP) | 15 | 20 | 25 | 30 | 35 | 45 | 60 |

## Permanent vs temporary

- **Attached:** the bonus lasts **as long as the stone stays equipped**.
- **Consumed:** the buff is **temporary** — it lasts a few turns, then the stone is spent.

The strength is the **same percentage either way** for a given tier; the difference is permanent-
while-equipped versus a short, movable buff (or debuff) you can throw at any combatant.

## Stones and Broken Darkness

Because a stone is **non-elemental**, a stone consumable used on a [Broken Darkness](../Archetypes/BrokenDarkness.md)
fighter behaves like a non-elemental crystal: per the
[three-way crystal rule](./Crystals.md#using-crystals-on-a-broken-darkness-character), a
non-elemental item **does nothing to a BD's absorption** — no energy fed, no pool rotated, no
drain. Only **elemental** crystals feed-and-rotate, and only **Reality (Iolite)** drains-and-resets.
A stone is neither, so it leaves the BD's pool untouched.

## See also

- [Crystals](./Crystals.md) — the consumable elemental gems.
- [Fusion Stones](./FusionStones.md) — combine two attachables into one stone.
- [Archetypes](../Archetypes/) — the character forms these bonuses support.
