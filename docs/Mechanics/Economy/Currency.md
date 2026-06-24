# Currencies

**Status:** [Built · No UI] — the wallet (`UCurrencyComponent`) holds all of these and the economy
ops move them; no shop/HUD surfaces them yet. Code detail: [`../../Architecture/CurrencySystem.md`](../../Architecture/CurrencySystem.md).
How they flow as a loop: [`Economy.md`](./Economy.md).

A player-facing reference for every currency: what it is, and what it's **for**. (The earn/spend
loop is in [`Economy.md`](./Economy.md) — this is the token glossary.)

## The currencies

| Currency | What it is | Earned by | Spent on |
|---|---|---|---|
| **Gold** | Run-volatile pocket money. Never banked — gone at run end. | (no faucet wired yet) | run consumables / temporary buffs (design) |
| **Prisms** | The **hub** buy-currency. Persistent (banks across runs once save lands). | (no faucet wired yet) | purchase (spell/weapon), merge, reroll |
| **Diamond** | Account-wide premium currency. | (none) | premium sinks (design) |
| **Gear Essence** | Upgrades **weapons / rings / evolution**. | dismantle a weapon/ring/evo; gear-evo break | upgrade + downgrade of gear |
| **Skill Essence** | Upgrades **spells / abilities**. | dismantle a spell/ability | upgrade + downgrade of skills |
| **Typed Essence** (×14) | Acquisition essence keyed by element / pillar / ability. | dismantle a crystal/stone; combat-break | spell-purchase surcharge; **Reality** = upgrade co-cost |

> **"Upgrade essence"** is the collective doc-term for **Gear + Skill** essence (the two scalars
> spent to upgrade an item's tier). The backend currency fields are `GearEssence`/`SkillEssence`;
> older code/comments (e.g. `GetLevelingEssenceYieldForTier`) call this "leveling essence". The C++
> upgrade ops keep the **`LevelUp*`** name (`LevelUpWeapon`, `TryLevelUpEntry`, …) — the concept is
> "upgrading," the symbol is "LevelUp." Code references are not renamed.

### Typed essence — the 14 keys
`EEssenceType` (14): **10 element** (Fire, Water, Lightning, Wind, Earth, Light, Darkness, Void,
**Reality**, Generic) · **3 pillar** (Mind, Body, Spirit) · **1 Ability**.

- A dismantled **gem** yields its **element** essence; an **AbilityStone** yields **Ability**
  essence; a **stat stone** yields its **pillar** essence (Mind/Body/Spirit).
- **Reality** is the standout: it's the **½-cost co-currency on every upgrade** (and, in design,
  the roll/reroll + wildcard currency — not yet wired; see [`Economy.md`](./Economy.md)).
- **Generic** = "Quartz's element" (non-elemental).

## The Gear vs Skill split
`Gear` and `Skill` essence are **separate scalars that never merge** — scrapping gear funds gear
upgrades, scrapping skills funds skill upgrades. This is the self-balancing lane split; see
[`Economy.md`](./Economy.md#the-gear--skill-essence-split-self-balancing).

## Scope notes (today)
- **Gold** is replicated-only (run-volatile, never saved). Everything else is `SaveGame`-tagged for a
  future save system — **latent today** (session-only, no save system exists).
- **Prisms** (account-shareable) and **Diamond** (account-wide) currently store **per-character**;
  account-vs-character routing awaits the `APlayerState` wiring.
- **World Stat Points are NOT a currency here** — run power lives on the character component (separate build).

## Known Limitations / TODOs
- **No faucet sources** beyond dismantle + combat-break — nothing awards Gold/Prisms/Diamond in C++ yet.
- **No save system** — persistence tags are dormant.
- **Reality's roll/reroll + wildcard roles are design-only** (only the upgrade co-cost is wired).

## Related
- [`Economy.md`](./Economy.md) — the earn/spend loop these tokens move through.
- [`../../Architecture/CurrencySystem.md`](../../Architecture/CurrencySystem.md) — `UCurrencyComponent` (the code).
- [`Dismantle.md`](./Dismantle.md) · [`Upgrading.md`](./Upgrading.md) · [`Merging.md`](./Merging.md)
