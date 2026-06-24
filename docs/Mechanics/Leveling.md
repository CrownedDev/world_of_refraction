# Leveling, Downgrade & Dismantle

**Status:** [Built · No UI] — the backend (`UEconomyService`) is complete; the hub/NPC triggers
(Blacksmith / Jeweler / Spiritualist, §5.3b) are the UI layer on top. Backend detail:
[`../Architecture/EconomySystem.md`](../Architecture/EconomySystem.md),
[`../Architecture/TierOnInstance.md`](../Architecture/TierOnInstance.md).

Every owned weapon, ring, evolution, spell, and ability carries its **own tier** (a leveled copy
of the asset — see Tier-on-Instance). You raise it (level up), lower it for a refund (downgrade),
or scrap the item for essence (dismantle). All three read and write that **instance** tier, so a
leveled item is leveled everywhere — it hits harder, costs/wears differently, and scraps for more.

## Leveling up

Raise an owned item's tier **one step** (e.g. C → B). Both per-step cost components are paid:

- **Leveling essence** — the category currency: **Gear** for weapons/rings/evolution, **Skill** for
  spells/abilities. Cost per step (`GetTierUpCostForTier`, §5.3):

  | Step | F→E | E→D | D→C | C→B | B→A | A→S |
  |------|-----|-----|-----|-----|-----|-----|
  | Essence | 10 | 20 | 30 | 40 | 50 | 70 |

- **Reality essence** — **half** the step's leveling cost (5 / 10 / 15 / 20 / 25 / 35), the shared
  "reshape" co-cost. Both must be affordable or nothing is spent.

**S is the cap** — you can't level past S. No Gold cost. Leveling is immediate (the stronger version
is yours at once) and — once the Pool layer lands — permanent (banks across runs; today it's run-scoped).

Backend: `LevelUpWeapon`/`LevelUpRing`/`LevelUpEvolution` (Gear) · `LevelUpSpell`/`LevelUpAbility`
(Skill), all via the shared `TryLevelUpEntry` core.

## Downgrade (respec — "Face C")

Lower an owned item's tier **one step**, refunding part of what you paid:

- Refund = **half** the reverted step's leveling cost, in the **leveling essence only** (Gear/Skill).
  e.g. reverting B → C refunds ½ of the C→B cost (40 → **20 Gear**).
- The **½-Reality co-cost is NOT refunded** — reverting always costs the reshape currency.
- **Floored at the item's authored base tier** — you can only revert tiers you leveled *up* to,
  never below what the item shipped at.

So leveling is lossy to undo: full essence + Reality up, half essence back, no Reality back. Respec
is a real cost, not free tier-shuffling.

Backend: `DowngradeWeapon`/`Ring`/`Evolution` (Gear) · `DowngradeSpell`/`Ability` (Skill), via the
shared `TryDowngradeEntry` core (floor = the asset's authored `Tier`).

## Dismantle (scrap → essence)

Scrap an owned item for essence **at its current (leveled) tier** — a leveled item is worth more
scrapped. Routing by category:

| Scrapped | Essence | Amount curve |
|---|---|---|
| Weapon / Ring | **Gear** | `GetLevelingEssenceYieldForTier(tier)` — §3 (F5 E15 D30 C50 B75 A110 S145) |
| Spell / Ability | **Skill** | same §3 curve |
| Evolution | **element** essence (its element), at the **gear amount** | the §3 curve (hybrid: element *type*, gear *amount*) |
| Crystal / stone | **typed** essence (element / pillar / ability) | §4.2 crystal curve (F5…S24) × count |

Crystals don't level — they tier-up by **merging** (`MergeCrystals`): sum same-type crystals
lowest-first to a target's value, pay Prisms (half the buy price). See
[`Items/Crystals.md`](./Items/Crystals.md).

## How it reads instance tier (leveling-aware everywhere)

A leveled item's tier flows through: combat **damage/power** (`TIER_POWER`), action **EP cost**,
crystal/evolution **durability wear**, **tier-gap** scaling, loadout **slot budgets**, and
**dismantle** yield. Spells/abilities resolve the caster's leveled tier by asset (you own ≤1 copy
per asset); enemies and authored loadouts that don't own the instance read the asset's base tier.

## How to test
- Level a weapon up a step, then: check it hits harder in PIE, costs more EP, and dismantles for the
  higher §3 yield (Print Wallet before/after). Downgrade it back — confirm half-Gear refund, no Reality.
- Try to downgrade an item at its base tier → rejected (floored).
- Level a spell, cast it → leveled tier; have an enemy cast the same spell → base tier.

## Known Limitations / TODOs
- **No hub UI yet** — `LevelUp*`/`Downgrade*`/`Dismantle*` are authority-gated backend ops; the
  Blacksmith/Jeweler/Spiritualist triggers are unbuilt.
- **Run-scoped today** — leveled tiers persist only within a session until the Pool/save arc lands.
- **No currency faucet** beyond dismantle/break — nothing else awards Gear/Skill essence yet.
- Display of a spell's tier (`GetTierString`) shows the **asset** base tier (owner-less); instance-aware
  display is deferred.

## Related
- [`../Architecture/EconomySystem.md`](../Architecture/EconomySystem.md) — the service + yield curves.
- [`../Architecture/TierOnInstance.md`](../Architecture/TierOnInstance.md) — where the instance tier lives.
- [`Evolution.md`](./Items/EvolutionCrystals.md), [`Items/Crystals.md`](./Items/Crystals.md), [`Gear/PerInstanceRolls.md`](./Gear/PerInstanceRolls.md).
