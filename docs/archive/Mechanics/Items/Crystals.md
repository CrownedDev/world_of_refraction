# Items — Consumable Crystals

**Status:** Live. Player-facing reference.

## What they are

**Consumable crystals** are single-use items. Using one **spends your turn** and targets **any
living combatant** — ally *or* enemy (you pick from a list split into your side and theirs). Each
crystal comes in seven **tiers** — **F → E → D → C → B → A → S** — and higher tiers are stronger
(bigger numbers, longer or shorter durations as appropriate). **S-tier unlocks a special** beyond
just bigger numbers.

## The roster

| Crystal | Element | What it does | S-tier special |
|---|---|---|---|
| **Garnet** | Fire | Fire **damage over time** (a % of the target's max HP each turn) | One big **single-turn burst** |
| **Sapphire** | Water | **Defy death** — *Last Stand* ward on a living target / **revive** a dead one | **Longest** ward window |
| **Citrine** | Lightning | **Restore EP** | **Full** EP |
| **Emerald** | Wind | Grants a **bonus turn** — but **forfeits your current turn**; higher tiers shorten the delay before it fires | Bonus turn fires **immediately** |
| **Amber** | Earth | **Defense** buff (on an ally) / debuff (on an enemy) | Strongest buff/debuff |
| **Opal** | Light | **Crit** buff / debuff | Strongest buff/debuff |
| **Onyx** | Darkness | **Silence / energy-lock** (locks spellcasting) | **Full** silence |
| **Amethyst** | Void | **Gamble** — a random buff (ally) / debuff (enemy) | Best odds + magnitude |
| **Iolite** | Reality | **Cleanse** your debuffs / **strip** an enemy's buffs | Removes **ALL** debuffs/buffs |
| **Quartz** | *(non-elemental)* | **Clear your own status bar** + element protection | Full clear + element **immunity** |

> Emerald on an **enemy** is a gamble: you hand them a turn (their damage-over-time ticks *and*
> they act). On **yourself** it's tempo — you act again sooner. Either way you give up the turn
> you spent using it.

> **Sapphire is now "defy death," not a heal.** On a **living** target it grants **Last Stand** — a
> tier-scaled **ward**: if they would die within the window, they instead **survive at 50% max HP**
> (one death absorbed). On a **dead** target it **revives** them at **30% max HP**. Higher tiers
> give a **longer ward window**. The old plain **heal moved to the [Healing Stone](./AugmentStones.md)**.

## Numbers by tier

Tier order is **F → S** (worst → best). These are the live values straight from the combat code.

| Crystal | Metric | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|---|
| **Garnet** | DOT per turn (% of target max HP) | 5 | 7 | 9 | 12 | 16 | 20 | 30 |
| **Garnet** | DOT duration (turns) | 3 | 3 | 3 | 2 | 2 | 2 | 1 |
| **Sapphire** | Last Stand ward window (turns) | 2 | 2 | 3 | 3 | 4 | 4 | 5 |
| **Citrine** | EP restored (% of target max EP) | 30 | 40 | 50 | 60 | 70 | 85 | 100 |
| **Emerald** | Bonus-turn delay (turns until it fires) | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| **Amber** | Defense buff/debuff % | 6 | 10 | 14 | 18 | 22 | 26 | 30 |
| **Opal** | Crit buff/debuff % | 6 | 10 | 14 | 18 | 22 | 26 | 30 |
| **Amber / Opal** | Buff/debuff duration (turns) | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| **Onyx** | EP drained (% of target max EP) | 15 | 30 | 30 | 50 | 70 | 70 | 100 |
| **Amethyst** | Chance the roll is a buff (vs debuff) % | 10 | 20 | 30 | 40 | 50 | 60 | 70 |
| **Amethyst** | Buff/debuff magnitude % | 10 | 15 | 20 | 25 | 30 | 35 | 40 |
| **Amethyst** | Duration (turns) | 4 | 4 | 3 | 3 | 3 | 2 | 2 |
| **Iolite** | Effects removed | 1 | 1 | 2 | 2 | 3 | 3 | all |
| **Quartz** | Status bar cleared % | 25 | 35 | 45 | 55 | 65 | 80 | 100 |
| **Quartz** | Element-protection duration (turns) | 4 | 4 | 3 | 3 | 3 | 2 | 2 |

Shared effects (only the elemental crystals carry these):

| Effect | Applies to | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|---|
| Status-bar buildup % | Garnet · Citrine · Onyx · Amethyst | 10 | 15 | 20 | 30 | 40 | 50 | 60 |
| BD absorb (% of target max EP) | any **gem** crystal used on a Broken Darkness fighter | 10 | 20 | 30 | 40 | 50 | 60 | 70 |

A few values are **fixed, not tier-scaled** — only the *window* or *duration* above moves:

- **Sapphire** ward: if the warded target would die inside the window, they instead survive at
  **50% max HP**; used on a **dead** target it revives them at **30% max HP** (any tier).
- **Onyx**: F–A spend that % of the target's max EP **immediately** (a one-shot drain, no lingering
  lock). **S-tier** instead applies a **full Silence** — spellcasting blocked for **1 turn**.
- **Emerald**: every tier grants one full bonus turn; only the *delay* before it fires shortens. At
  **S** it fires immediately (and grants two back-to-back).

## Status buildup

The **elemental** crystals don't just deal their effect — they also push the target's **status
bar** toward their element, the same way a spell hit does. Garnet (Fire), Citrine (Lightning),
Onyx (Darkness) and Amethyst (Void) are the notable ones: an item hit can help fill — or fill —
the bar that triggers a status effect.

## Using crystals on a Broken Darkness character

This is the crystal's most important wrinkle. A [Broken Darkness](../Archetypes/BrokenDarkness.md)
fighter survives by **absorbing** elements — so a crystal used **on a BD** does one of **three**
things, by the crystal's element:

- **An elemental crystal** (Fire / Water / Earth / Wind / Light / Darkness / Lightning / Void) →
  **feeds their absorption energy AND rotates their active pool to that element.** You've handed
  them that element to wield ("Dark Fire", etc.). Powering up an enemy BD is usually a mistake.
- **Iolite (Reality)** → the **opposite**: it **drains** their absorption energy **and resets
  their active pool back to base Darkness** — the anti-Broken-Darkness **cleanse**. This is how
  you undo a BD that has built up a dangerous stolen element. See [Reality](../Archetypes/Reality.md).
- **Quartz (non-elemental)** → **does nothing to absorption.** No energy, no rotation, no drain —
  a Quartz crystal is inert against a BD's pool (it still does its normal status-bar clear).

So against a Broken Darkness enemy: **never feed them an elemental crystal**, and reach for
**Iolite** to strip them back to base.

## See also

- [Reality](../Archetypes/Reality.md) — the any-element Refractor; Iolite is its crystal.
- [Broken Darkness](../Archetypes/BrokenDarkness.md) — the form the three-way interaction targets.
- [Generic spells](../Magic/GenericSpells.md) — a crystal slotted into a weapon/ring turns a Generic spell into that element.

> **Note:** **augment stones** and **fusion stones** are a separate item family — they grant
> mechanical bonuses, not elements. See [Augment Stones](./AugmentStones.md) and
> [Fusion Stones](./FusionStones.md).
