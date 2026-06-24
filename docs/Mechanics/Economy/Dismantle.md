# Dismantle (salvage → essence)

**Status:** [Built · No UI] — `UEconomyService::Dismantle*` are complete, authority-gated ops; the
hub trigger that calls them is unbuilt. Backend: [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md).
The earn/spend picture: [`Economy.md`](./Economy.md).

Scrap an owned item to get essence back. Dismantle reads the item's **current (upgraded) instance
tier**, so an upgraded item is worth **more** scrapped than a base one — what you put into upgrading
is partly recoverable as raw essence (see [Tier-on-Instance](../../Architecture/TierOnInstance.md)).

## Routing by category

| Scrapped | Essence you get | Amount curve |
|---|---|---|
| **Weapon / Ring** | **Gear** essence | upgrade-yield curve (below) |
| **Spell / Ability** | **Skill** essence | same upgrade-yield curve |
| **Evolution** | **element** essence (the evo's element) — *at the gear amount* | upgrade-yield curve (hybrid: element *type*, gear *amount*) |
| **Crystal / Stone** | **typed** essence (element / pillar / ability) | crystal curve (below) × count |

Evolution is the one **hybrid**: it pays out as its *element* essence (like a crystal) but at the
*gear* upgrade amount (like a weapon). A dismantled gem yields its element essence, an AbilityStone
yields Ability essence, a stat stone yields its pillar essence (`ResolveEssenceType`).

## The amounts

**Upgrade-essence yield** (weapons / rings / spells / abilities / evolution-amount) — the §3
½-cumulative curve, read at the item's upgraded tier:

| Tier | F | E | D | C | B | A | S |
|------|---|---|---|---|---|---|---|
| Essence | 5 | 15 | 30 | 50 | 75 | 110 | 145 |

**Crystal / stone yield** (typed essence) — the §4.2 curve, **× the count** dismantled:

| Tier | F | E | D | C | B | A | S |
|------|---|---|---|---|---|---|---|
| Essence | 5 | 7 | 9 | 11 | 15 | 19 | 24 |

## How it behaves
- **Reads the upgraded tier** — upgrade a weapon C→B, then dismantle it for the **B** yield (75 Gear), not C (50).
- **Crystals don't upgrade** — they don't dismantle for "more" by upgrading; they **tier up by merging**
  instead ([`Merging.md`](./Merging.md)), then dismantle at the merged tier.
- **Remove-then-grant** — the item is removed first; a failed removal never grants phantom essence.
- **Combat-break is dismantle's involuntary cousin** — an evolution that breaks in combat
  *forced-dismantles* on the between-combat sweep (same yield path); a broken crystal/fusion grants
  essence directly. See [`Items/EvolutionCrystals.md`](../Items/EvolutionCrystals.md).

## How to test
- Print Wallet, dismantle a weapon, Print Wallet → Gear essence rose by the §3 amount **at its tier**.
- Upgrade it a step first → confirm the dismantle yield rises to the new tier's row.
- Dismantle 3 F-gems → typed essence rises by 5 × 3 = 15 of that element.

## Known Limitations / TODOs
- **No hub UI** — backend op only.
- **Run-scoped** — granted essence is session-only until the Pool/save arc.

## Related
- [`Economy.md`](./Economy.md) — the loop · [`Upgrading.md`](./Upgrading.md) — the matching sink (essence → tier).
- [`Merging.md`](./Merging.md) — how crystals tier up (they don't upgrade).
- [`Currency.md`](./Currency.md) — what Gear/Skill/typed essence are.
- [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md) — `Dismantle*` + `EconomyYield` curves.
