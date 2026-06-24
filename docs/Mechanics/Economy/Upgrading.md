# Upgrading, Downgrade & Dismantle

**Status:** [Built · No UI] — the backend (`UEconomyService`) is complete; the hub/NPC triggers
(Blacksmith / Jeweler / Spiritualist, §5.3b) are the UI layer on top. Backend detail:
[`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md),
[`../../Architecture/TierOnInstance.md`](../../Architecture/TierOnInstance.md).

> **Upgrading = ITEM tier progression** (this doc). Distinct from **character progression** (pillar
> stats growing) — for that see [`Leveling.md`](../Character/Leveling.md). The two were once both called
> "leveling"; the item system is now **upgrading**.
>
> **Naming note:** the *concept* is "upgrading," but the C++ symbols keep the **`LevelUp*`** name
> (`LevelUpWeapon`, `TryLevelUpEntry`, …) — code references below are kept verbatim, not renamed.

Every owned weapon, ring, evolution, spell, and ability carries its **own tier** (an upgraded copy
of the asset — see Tier-on-Instance). You raise it (upgrade), lower it for a refund (downgrade),
or scrap the item for essence (dismantle). All three read and write that **instance** tier, so an
upgraded item is upgraded everywhere — it hits harder, costs/wears differently, and scraps for more.

## Upgrading

Raise an owned item's tier **one step** (e.g. C → B). Both per-step cost components are paid:

- **Upgrade essence** — the category currency: **Gear** for weapons/rings/evolution, **Skill** for
  spells/abilities. Cost per step (`GetTierUpCostForTier`, §5.3):

  | Step | F→E | E→D | D→C | C→B | B→A | A→S |
  |------|-----|-----|-----|-----|-----|-----|
  | Essence | 10 | 20 | 30 | 40 | 50 | 70 |

  *(The backend currency is `GearEssence`/`SkillEssence`; older code/comments — e.g.
  `GetLevelingEssenceYieldForTier` — call this "leveling essence".)*

- **Reality essence** — **half** the step's upgrade cost (5 / 10 / 15 / 20 / 25 / 35), the shared
  "reshape" co-cost. Both must be affordable or nothing is spent.

**S is the cap** — you can't upgrade past S. No Gold cost. Upgrading is immediate (the stronger
version is yours at once) and — once the Pool layer lands — permanent (banks across runs; today
it's run-scoped).

Backend: `LevelUpWeapon`/`LevelUpRing`/`LevelUpEvolution` (Gear) · `LevelUpSpell`/`LevelUpAbility`
(Skill), all via the shared `TryLevelUpEntry` core. *(Backend retains the `LevelUp*` name — the
player-facing concept is "upgrade.")*

## Downgrade (respec — "Face C")

Lower an owned item's tier **one step**, refunding part of what you paid:

- Refund = **half** the reverted step's upgrade cost, in the **upgrade essence only** (Gear/Skill).
  e.g. reverting B → C refunds ½ of the C→B cost (40 → **20 Gear**).
- The **½-Reality co-cost is NOT refunded** — reverting always costs the reshape currency.
- **Floored at the item's authored base tier** — you can only revert tiers you upgraded *up* to,
  never below what the item shipped at.

So upgrading is lossy to undo: full essence + Reality up, half essence back, no Reality back. Respec
is a real cost, not free tier-shuffling.

Backend: `DowngradeWeapon`/`Ring`/`Evolution` (Gear) · `DowngradeSpell`/`Ability` (Skill), via the
shared `TryDowngradeEntry` core (floor = the asset's authored `Tier`).

## Dismantle (scrap → essence)

Scrap an owned item for essence **at its current (upgraded) tier** — an upgraded item is worth more
scrapped. The full routing table + amounts now live in **[`Dismantle.md`](./Dismantle.md)** (it's a
faucet, not an upgrade step). The short version: weapon/ring → Gear, spell/ability → Skill,
evolution → element essence at the gear amount, crystal/stone → typed essence.

Crystals don't upgrade — they tier-up by **merging** (`MergeCrystals`): sum same-type crystals
lowest-first to a target's value, pay Prisms (half the buy price). See [`Merging.md`](./Merging.md).

## How it reads instance tier (upgrade-aware everywhere)

An upgraded item's tier flows through: combat **damage/power** (`TIER_POWER`), action **EP cost**,
crystal/evolution **durability wear**, **tier-gap** scaling, loadout **slot budgets**, and
**dismantle** yield. Spells/abilities resolve the caster's upgraded tier by asset (you own ≤1 copy
per asset); enemies and authored loadouts that don't own the instance read the asset's base tier.

## How to test
- Upgrade a weapon a step, then: check it hits harder in PIE, costs more EP, and dismantles for the
  higher §3 yield (Print Wallet before/after). Downgrade it back — confirm half-Gear refund, no Reality.
- Try to downgrade an item at its base tier → rejected (floored).
- Upgrade a spell, cast it → upgraded tier; have an enemy cast the same spell → base tier.

## Known Limitations / TODOs
- **No hub UI yet** — `LevelUp*`/`Downgrade*`/`Dismantle*` are authority-gated backend ops; the
  Blacksmith/Jeweler/Spiritualist triggers are unbuilt.
- **Run-scoped today** — upgraded tiers persist only within a session until the Pool/save arc lands.
- **No currency faucet** beyond dismantle/break — nothing else awards Gear/Skill essence yet.
- Display of a spell's tier (`GetTierString`) shows the **asset** base tier (owner-less); instance-aware
  display is deferred.

## Related
- [`Economy.md`](./Economy.md) — the earn/spend loop this is a sink of · [`Dismantle.md`](./Dismantle.md) — the matching faucet · [`Merging.md`](./Merging.md) — the crystal-lane equivalent.
- [`Leveling.md`](../Character/Leveling.md) — **character** progression (pillar stats), a different system.
- [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md) — the service + yield curves.
- [`../../Architecture/TierOnInstance.md`](../../Architecture/TierOnInstance.md) — where the instance tier lives.
- [`Items/EvolutionCrystals.md`](../Items/EvolutionCrystals.md), [`Items/Crystals.md`](../Items/Crystals.md), [`Gear/PerInstanceRolls.md`](../Gear/PerInstanceRolls.md), [`Quality.md`](../Gear/Quality.md).
