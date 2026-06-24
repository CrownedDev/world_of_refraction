# Economy — the earn → spend loop

**Status:** [Built · No UI] — every operation below is a complete, authority-gated backend op
(`UEconomyService`); the hub/NPC triggers that *call* them (Blacksmith / Jeweler / Spiritualist,
§5.3b) are the UI layer on top. Backend detail: [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md).
Design: [`../../Design/Resources_Design.md`](../../Design/Resources_Design.md).

This is the **map**. It shows how value flows; the linked docs hold the detail. The currencies
themselves are in [`Currency.md`](./Currency.md).

## The loop

```
        FAUCETS  (value in)                         SINKS  (value out)
   ┌──────────────────────────┐              ┌──────────────────────────────┐
   │ Dismantle  item → essence │   essence    │ Level up   raise instance tier │
   │ Combat-break broken→essence│ ──────────▶ │ Purchase   buy spell/weapon    │
   └──────────────────────────┘   Prisms      │ Merge      crystals up a tier  │
                                               │ Reroll     re-roll a drop      │
                                               └──────────────────────────────┘
```

You **earn essence** by scrapping or breaking items, and **spend** it (plus Prisms) to make your
kept items stronger. Nothing else awards economy currency yet — dismantle and combat-break are the
only faucets wired today (see [`../../Architecture/CurrencySystem.md`](../../Architecture/CurrencySystem.md)
"No reward/faucet sources").

### Faucets (earn)
- **Dismantle** — scrap an owned item for essence at its **current (leveled) tier**. Routes by
  category (gear→Gear essence, skill→Skill essence, crystal→typed essence). See [`Dismantle.md`](./Dismantle.md).
- **Combat-break** — a crystal/fusion/evolution that wears to 0 in combat is destroyed on the
  between-combat sweep and yields essence (a broken evolution forced-dismantles; a fusion yields a
  lossy half). See [`Items/EvolutionCrystals.md`](../Items/EvolutionCrystals.md) and
  [`Gear/DurabilityWear.md`](../Gear/DurabilityWear.md).

### Sinks (spend)
- **Upgrade** (a.k.a. level up) — raise an owned item's tier one step; costs upgrade essence + ½ Reality. See [`Upgrading.md`](./Upgrading.md).
- **Purchase** — buy a spell (Prisms + typed essence) or weapon (Prisms) at its **asset** tier. Backend: `PurchaseSpell`/`PurchaseWeapon`.
- **Merge** — fuse same-type crystals up a tier (value-based, costs Prisms). See [`Merging.md`](./Merging.md).
- **Reroll** — re-roll a drop's stats/tier ([Built · No UI], reward-fill not wired). See [`Gear/RerollEconomy.md`](../Gear/RerollEconomy.md).

## The Gear / Skill essence split (self-balancing)

Upgrade essence comes in **two non-fungible scalars**, and a faucet feeds the **same category** it
sinks into:

| Category | Faucet (dismantle) | Sink (level) | Essence |
|---|---|---|---|
| Weapons · rings · evolution | scrap a weapon/ring/evo | level a weapon/ring/evo | **Gear essence** |
| Spells · abilities | scrap a spell/ability | level a spell/ability | **Skill essence** |

The two **never merge**. So scrapping gear funds gear progression and scrapping skills funds skill
progression — each lane balances itself; you can't strip-mine spells to over-level your weapon. The
two share **one cost/yield curve** (`GetLevelingEssenceYieldForTier` / `GetTierUpCostForTier`); only
the destination scalar differs.

## Reality — the cross-cutting co-cost

`EEssenceType::Reality` (one of the 14 typed essences) is the connective "reshape" currency.

- **Built today:** every upgrade also costs **½ the upgrade-essence amount in Reality**
  (`TryLevelUpEntry`, `EconomyService.cpp`). A 100-essence tier-up also costs 50 Reality.
  Downgrade refunds **half the upgrade essence only — the Reality is never refunded**, so reshaping
  is always a real, lossy cost (no free tier-shuffling). See [`Upgrading.md`](./Upgrading.md).
- **Design-planned, NOT wired yet:** Reality as the **roll/reroll currency** (§14) and as a
  **1.5:1 wildcard substitute** for any element line (§4.1). These are design-locked in
  `Resources_Design.md` but no code spends Reality on rolls or substitutes it — only the level-up
  co-cost is live. (Flagged so the doc doesn't overstate what's built.)

## Known Limitations / TODOs
- **No hub UI** — all ops are backend `UFUNCTION`s; the NPC triggers are unbuilt.
- **Run-scoped** — earned essence and leveled tiers persist only within a session until the Pool/save arc lands.
- **Only two faucets** — dismantle + combat-break. No post-defeat reward / drop faucet yet (greenfield).
- **Reality's roll/reroll + wildcard-substitute roles are design-only** (see above).

## Related
- [`Currency.md`](./Currency.md) — the tokens (what each currency is + where it comes from).
- [`Dismantle.md`](./Dismantle.md) · [`Merging.md`](./Merging.md) · [`Upgrading.md`](./Upgrading.md) · [`Quality.md`](../Gear/Quality.md)
- [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md) — `UEconomyService` + `EconomyYield` curves.
- [`../../Design/Resources_Design.md`](../../Design/Resources_Design.md) — the design spec the build follows.
