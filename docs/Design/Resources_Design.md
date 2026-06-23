# World of Refraction — Resource Economy Design

**Status:** Design locked; most numbers locked (PIE-tunable drafts flagged). Nothing built. Locked-decision reference, not an implementation spec.
**Scope:** The roguelite resource economy — Prisms, Gold, Dust, Essence, World Stat Points — the round-table run loop, and the persistent account-perk sink.
**Suggested repo path:** `docs/Design/Resources_Design.md`

> **BUILD-STATE NOTE (per CC survey, 2026-06).** This doc is DESIGN. Code reality differs in three ways the survey caught: (1) **no reward system exists** — World Stat Points / Prisms / Gold are not awarded anywhere in C++; the post-defeat reward hook is greenfield at `CombatOrchestrator.cpp:576`. (2) **Tier lives only on the data asset** — there is no per-instance Tier; the loot + leveling model below REQUIRES building tier-on-instance first. (3) **Quality does not exist in code** — but under the revised model (§11.2) it's just an authored asset field, not a roll, so it's trivial to add. The instance layer (`UInventoryComponent` + per-type entry structs) IS already built, despite `InventorySystem_Design.md` claiming otherwise.

---

## 1. The five resources

| Resource                                | Type        | Lives                          | Earned by                                               | Spent on                                                  |
| --------------------------------------- | ----------- | ------------------------------ | ------------------------------------------------------- | --------------------------------------------------------- |
| **Prisms**                              | run economy | volatile (in-run)              | per-encounter reward                                    | in-run temp buffs / consumables only                      |
| **Gold**                                | hub economy | persistent (banked)            | per-encounter reward (banks on exit)                    | **hub buy-currency** — equipment + spell floors           |
| **Dust** (14 types)                     | acquisition | persistent                     | dismantling crystals/stones + combat (Reality/wildcard) | spell purchase surcharge (typed)                          |
| **Essence** (2 pools: Weapon / Crystal) | growth      | persistent                     | combat actions (see §3)                                 | tier-leveling your kit + account perks                    |
| **World Stat Points**                   | run power   | **volatile (reset every run)** | per-encounter (= avg enemy stat-total)                  | Mind / Body / Spirit allocation, for the current run only |

**The split:** the **run** carries only volatile spend (Prisms) + volatile power (WSP). All **acquisition and permanent buying happens at the hub** (Gold + Dust). Essence is earned in-run and spent in-run to level your kit (immediate + permanent).

Prisms ≠ Gold. (Earlier drafts called Prisms "= gold"; that's retired — they're now two distinct currencies, the Dead Cells *Gold (run) / Cells (meta)* split.)

---

## 2. Tier scale — the spine everything references

Keyed on `EItemTier` (F…S):

| Tier                                     | F    | E    | D    | C    | B    | A    | S    |
| ---------------------------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| **Dust weight** (linear ref)             | 1    | 2    | 3    | 4    | 5    | 6    | 7    |
| **TIER_POWER** (×, the real power curve) | 1.00 | 1.30 | 1.70 | 2.20 | 2.85 | 3.70 | 4.80 |

`TIER_POWER` already exists in code (`TierPowerConstants.h`) and drives combat power. Dust pricing and enemy-threat both ride it, so reward never drifts from real power.

---

## 3. Essence — growth (permanent)

Two pools, split by **what they level**:

| Pool                | Earned by                                                    | Levels                        |
| ------------------- | ------------------------------------------------------------ | ----------------------------- |
| **Weapon essence**  | weapon actions (attack, weapon-crit, defend, perfect defend) | weapons                       |
| **Crystal essence** | casting spells / using crystal-channelled abilities          | crystals (+ spells/abilities) |

**Faucet (per action):**

| Action         | Essence | Pool                                          |
| -------------- | ------- | --------------------------------------------- |
| Cast (spell)   | 1       | Crystal                                       |
| Crit           | 1       | follows source (spell→Crystal, weapon→Weapon) |
| Defend         | 1       | Weapon                                        |
| Perfect defend | 2       | Weapon                                        |
| **Parry**      | splits  | **both** pools                                |

Account perks can raise these rates (§7).

**Tier-up cost (per step), partial-spend allowed:**

| Step              | F→E | E→D | D→C | C→B | B→A | A→S |
| ----------------- | --- | --- | --- | --- | --- | --- |
| **Essence**       | 10  | 20  | 30  | 40  | 50  | 70  |
| **Cumulative F→** | 10  | 30  | 60  | 100 | 150 | 220 |

**In-run leveling:** essence is spent **during a committed run** to tier-up the kit you brought. The upgrade is **immediate** (you wield the stronger version for the rest of the run) and **permanent** (banks, survives death). The constraint is **scope, not timing** — you can only level *what you brought*, never acquire or swap to something new mid-run.

**Essence sinks (both permanent):** tier-leveling a weapon/crystal/spell/ability · account perks (§7).

---

## 4. Dust — spell acquisition surcharge (permanent)

### 4.1 The 14 wallets

| Family  | Count | Members                                                                                     | Source                 |
| ------- | ----- | ------------------------------------------------------------------------------------------- | ---------------------- |
| Element | 10    | Fire, Water, Lightning, Wind, Earth, Light, Darkness, Void, Reality, **Generic (= Quartz)** | gem dismantle          |
| Pillar  | 3     | Mind, Body, Spirit                                                                          | stat-stone dismantle   |
| Ability | 1     | Ability                                                                                     | AbilityStone dismantle |

- **Quartz = Generic** in the existing `CrystalTypeHelpers::GetElement` map → Generic spells pay in Quartz/Generic dust automatically, no special rule.
- **Reality dust = wildcard substitute** (covers any element line at a premium) and drops from combat directly, so it flows without Iolite.

### 4.2 The unit — steep curve, buy/sell spread

Dust rides `TIER_POWER` (×10), with a **2:1 buy/sell spread** (dismantle pays half of purchase cost — the vendor margin that makes it an economy, not a passthrough):

| Tier                       | F   | E   | D   | C   | B   | A   | S   |
| -------------------------- | --- | --- | --- | --- | --- | --- | --- |
| **Dismantle yield** (sell) | 5   | 7   | 9   | 11  | 15  | 19  | 24  |
| **Purchase cost** (buy)    | 10  | 13  | 17  | 22  | 29  | 37  | 48  |

### 4.3 The two conversions (both ride the purchase-cost row)

**Spell-tier conversion** — the spell's own tier → its identity (element) dust:

| Spell tier   | F   | E   | D   | C   | B   | A   | S   |
| ------------ | --- | --- | --- | --- | --- | --- | --- |
| Element dust | 10  | 13  | 17  | 22  | 29  | 37  | 48  |

**Scaling-tier conversion** — each scaling stat's grade → that pillar's dust:

| Scaling grade | F   | E   | D   | C   | B   | A   | S   |
| ------------- | --- | --- | --- | --- | --- | --- | --- |
| Pillar dust   | 10  | 13  | 17  | 22  | 29  | 37  | 48  |

(Same numbers — dust value is dust value. Spell-tier applies it to the element line; scaling-grade applies it to each stat line.)

### 4.4 Full spell price = Gold floor + Dust

> **Spell price = Gold floor (by spell tier, §5) + element dust (spell tier) + Σ pillar dust (each scaling grade).**

Identity = element (spells, by tier) · ability (abilities + AbilityStone) · the stat itself (stat-stones). No separate ×tier-weight multiplier — `TIER_POWER` is already baked into the dust values.

**Worked examples (dust portion):**

| Spell                               | Tier | Scaling | Components                                | Dust total |
| ----------------------------------- | ---- | ------- | ----------------------------------------- | ---------- |
| Fire `{RawDmg@F}`                   | F    | F       | 10 Fire + 10 Body                         | **20**     |
| Fire `{SpellDmg@C, Luck@D}`         | C    | C, D    | 22 Fire + 22 Mind + 17 Spirit             | **61**     |
| Void `{SpellDmg@A, Luck@D}`         | A    | A, D    | 37 Void + 37 Mind + 17 Spirit             | **91**     |
| Void `{SpellDmg@S, Luck@A, Crit@B}` | S    | S, A, B | 48 Void + 48 Mind + 37 Spirit + 29 Spirit | **162**    |

(Plus the Gold floor on each, §5.)

### 4.5 Anti-exploit (the "F-farm" guard)

Bypass to prevent: shredding common F-crystals to fake high-tier purchases. Two structural deterrents:

1. **Steep curve + spread** — S line costs 48, F crystal yields 5 → ~10 F-crystals per S line; a full S spell ≈ 30+ F-crystals across types.
2. **Type-matching** — an S Void spell needs **Void + Mind + Spirit dust specifically**; an F Garnet can't supply them.

**Substitution (only softener):** pillar→specific or Reality→element at **1.5 : 1**. Keep modest so it doesn't reopen the bypass.

---

## 5. Gold (hub) + Prisms (run)

### 5.1 Gold — the hub buy-currency

Gold buys **everything permanent at the round table**: equipment, and the **floor on every spell purchase** (spells = Gold floor + Dust). Banked between runs.

**Cost table (kill/encounter-anchored — inherits the earlier anchor, now denominated in Gold):**

| Buy               | Gold |
| ----------------- | ---- |
| F crystal / stone | 15   |
| C crystal / stone | 50   |
| S crystal / stone | 100  |
| C spell floor     | 40   |
| S spell floor     | 140  |
| Gear reroll       | 20   |

> *Light reconcile pending:* these numbers were set when the hub currency was called "Prisms." They carry over as the **Gold** table unchanged. Equipment Gold prices (weapons/rings by tier) still to be added.

### 5.2 Prisms — the run currency

Earned per encounter, **spent in-run on temporary buffs / consumables only** (no permanent purchases inside a run). The volatile run-spend.

---

## 6. Per-encounter reward (the faucet)

> **NOT YET IN CODE.** `CalculateThreatLevel()` today reads three character-stat scalars (RawDamage / StatusMult / SpellDamage × weights), **not** `StatTotal × AvgKitPower`. The formula below is the design target; building it means replacing the current threat calc AND adding the (currently nonexistent) reward-award path. Treat §6 as spec, not present behaviour.

**Encounter = one battle** (you vs an enemy team), Pokémon / Expedition-33 style — not a room of separate kills. Reward computed per battle, **averaged across the team** (team-size-independent):

> **Encounter Threat = AvgStatTotal × AvgKitPower**
> - **AvgStatTotal** = mean of the team's stat-totals (e.g. 5/5/5 → 15)
> - **AvgKitPower** = mean `TIER_POWER` across the team's equipped kits (F ×1.0 → S ×4.8)
>
> **Reward = Encounter Threat × rate(=1), split:**
> - **World Stat Points** ← AvgStatTotal (you learn from the *caliber* of foe)
> - **Gold + Prisms** ← AvgStatTotal × AvgKitPower (full danger)
>
> **Named boss:** × BossToughness (2–3) on top.

**Worked:**

| Enemy team (avg) | AvgStatTotal | AvgKitPower | Encounter Threat |
| ---------------- | ------------ | ----------- | ---------------- |
| 5/5/5, B kits    | 15           | ×2.85       | **~43**          |
| 7/7/7, S kits    | 21           | ×4.80       | **~101**         |
| 5/5/5, F kits    | 15           | ×1.00       | **15**           |

**Why average, not sum:** judges a fight by enemy *strength*, not headcount — a 1v3 boss and a 1v1 elite of equal caliber pay the same; no "farm big trash teams for volume" exploit.

**Boss minion-drag:** a boss flanked by weak minions has its average dragged down. Fix (pick later): weighted-average by individual threat, **or** rely on BossToughness ×2–3 on top (leaning the latter — keeps the average clean, boss premium is a deliberate knob).

**Two faucets, one number:** WSP = AvgStatTotal alone; Gold/Prisms = the full product. Stat-weak/S-geared teams pay currency but little WSP; stat-strong/F-geared is the reverse — players pick targets by which they need.

---

## 7. The roguelite loop (round table)

### 7.1 Structure

**Continuous escalating climb.** No fixed round count.

> **Round table (hub):** spend Gold + Dust on equipment + spells (you **own** them permanently); dismantle crystals → Dust; **pick your target boss → commit** (changing target loses run progress). →
> **Run (committed):** continuous escalating encounters; draft your loadout from owned inventory (weighted by tier + Luck, element-filtered); fight as long as you live; earn per-encounter reward; spend Essence to level **what you brought** (immediate + permanent); spend Prisms on temp buffs. →
> **Exit (three ways, below).** →
> **Back at the round table:** restock with banked Gold/Dust; deeper next run.

### 7.2 The three exits (the bank rule)

| Exit            | Earnings    | Run progress |
| --------------- | ----------- | ------------ |
| **Beat boss**   | **kept**    | win          |
| **Die**         | **kept**    | reset        |
| **Leave early** | **forfeit** | reset        |

**The inversion:** dying *banks* your earnings; **leaving early forfeits them.** This is deliberate — if leaving banked your haul, the optimal play would be farm-trivial-encounters-and-quit. Forcing leave = forfeit means the only ways to keep earnings are **win** or **die trying**, so players are always pushed toward real risk. The run's tension is **commitment**, not loss of loot.

**What a death actually costs:** only **World Stat Points + run progress/position** (always volatile). The economic haul (Gold, Dust, Essence, levels) survives death. Death = "I went as far as I could," not "I lost my loot."

### 7.3 Item ownership loop

Buy an item at the hub (Gold) → **own it permanently** → if it drafts into a run, **upgrade it there** (Essence, immediate + permanent). Acquire at the hub, deepen in the run. Owned items appear in the draft **at their leveled tier** (meta rides into the run). A bigger/leveled inventory = a richer, stronger draft pool — the reward for grinding.

### 7.4 Genre

Turn-based **roguelite** (meta-progression persists). Gear cushions low world stats — pre-leveled gear gives a floor so a fresh run isn't helpless; world stats are the per-run earn. (Gear-vs-world-stat scaling split is a tuning pass — lean ~60–70% self-tier / 30–40% world-stat.)

---

## 8. Persistent buffs — account perks (essence sink, permanent)

Permanent run-start advantages bought with Essence, surviving death. They **raise the floor**, they do not skip the climb. Lean toward *perceivable* perks (felt immediately) over invisible % boosts.

**Guard-rail:** keep floor-raisers small — especially World Stat head-start — or the run-axis (which resets on death) gets hollowed out by stacked perks.

| Perk                    | Effect                                                           | Cap                     | Scope     |
| ----------------------- | ---------------------------------------------------------------- | ----------------------- | --------- |
| **Head Start**          | begin each run with +N World Stat Points pre-allocated           | ≤ ~half max (≤10 of 21) | permanent |
| **Starting Purse**      | begin each run with +X Prisms                                    | small                   | permanent |
| **Signature Weapon**    | one owned high-tier weapon guaranteed into the run draft         | 1                       | permanent |
| **Signature Spell**     | one owned high-tier spell guaranteed into the run draft          | 1                       | permanent |
| **Roll Budget**         | gear bonus-stat / resistance rolls generate from a larger budget | modest                  | permanent |
| **Baseline Resistance** | begin with a small flat resistance pool                          | small                   | permanent |
| **Extra Draft**         | +1 inventory option at run-start draft                           | 1                       | permanent |

**Why essence:** it's the long-tail currency — once a core loadout is maxed, account perks give players something to keep pouring essence into.

**Open:** exact essence cost per perk; per-perk caps (table is structure + guard-rails, not final tuning).

---

## 9. Open items

**Genuinely open (need a pass):**
- **Account perk costs & caps** (§8) — numbers only.
- **Equipment Gold prices** (§5.1) — weapon/ring cost-by-tier, to add to the Gold table.
- **Gear vs world-stat scaling split** (§7.4) — the dial making both "pre-leveling helps" and "world stats matter" true.

**PIE-tunable drafts (non-blocking):**
- Dust substitution rate — **1.5 : 1**
- Dust buy/sell spread — **2 : 1**
- Boss toughness — **×2–3**
- Reward rate — **1** (Reward = Encounter Threat)
- Boss minion-drag fix — weighted-avg vs BossToughness-on-top (leaning latter)

---

## 10. Quick reference — the loop

> **Round table:** buy equipment + spells with Gold (+ Dust on spells), own them; dismantle crystals → Dust; commit to a target boss. **Run:** continuous escalating battles; per-encounter reward (AvgStatTotal × AvgKitPower) → World Stat Points + Gold/Prisms; spend Essence to level what you brought (immediate, permanent); Prisms buy temp buffs. **Exit:** beat boss or die → keep earnings; leave early → forfeit. **Death** only costs World Stat Points + run position. Back to the table → restock → deeper.

---

## 11. Loot — tiers, hidden quality, drop chance

**Status:** Design locked (model + numbers). Depends on **tier-on-instance** (below). Validated against Path of Exile's itemization model (hidden item-level + tiered affixes), simplified onto the one F→S spine.

### 11.1 Tier-on-instance (the enabler)

`Tier` (and `Quality`) move **off the asset, onto the instance.** One tier-agnostic asset per spell/weapon (identity: element, base, scaling array, effects); loot stamps the grades per drop.

- **Why it works:** unified scaling makes power formulaic (`Base × TIER_POWER × …`), so tier is a per-instance multiplier, not an authored stat block. One asset replaces 7 (DA_Fireball_F…_S).
- **Bonus unification:** the instance `Tier` is **both** the loot tier **and** the essence-leveled tier — leveling just increments the field; the draft reads it. One source of truth.
- **Caveat — discrete tier *behaviour*** (e.g. S-rank crystal specials, which are bespoke not formulaic): handle via a small per-tier override map on the asset (`TMap<EItemTier, FTierOverride>` — author only the deltas), or keep behaviour-changing items as separate assets. Pure-numeric scaling needs neither.

### 11.2 Two axes — Quality (asset) × Tier (instance)

**Revised model (supersedes the earlier hidden-roll version).** Quality is **authored on the asset**, Tier is **rolled per drop on the instance.**

| Axis        | Lives on                                                  | Graded | Drives                                                                                                                                                |
| ----------- | --------------------------------------------------------- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Quality** | **asset** (authored, fixed per item)                      | F→S    | the item's grade/specialness — one Fireball asset is F-quality, a better one is S-quality. Visible; **may affect price** (it's authored, not hidden). |
| **Tier**    | **instance** (rolled on drop, raised by essence-leveling) | F→S    | core power (`TIER_POWER`), dust price, requirements, the draft                                                                                        |

- **Quality = which item this is** (authored identity). Not a roll — you build distinct assets at distinct Qualities.
- **Tier = how leveled this copy is.** Rolled at drop (§11.3), and essence-leveling raises it F→S. This is why **Tier must be per-instance.**
- A single asset can drop/exist at **any Tier**; its Quality is fixed by the asset. A C-tier plain-sword and a C-tier high-quality-sword hit similarly (same Tier) but differ by authored Quality (better rolls/effects).

### 11.3 Tier drop weights (the flat roll)

The **Tier** of a drop is rolled from this curve — **flat, regardless of enemy strength**:

| Grade       | F   | E   | D   | C   | B   | A   | S   |
| ----------- | --- | --- | --- | --- | --- | --- | --- |
| Tier drop % | 30  | 24  | 18  | 13  | 9   | 4   | 2   |

(Quality is **not** rolled — it comes from the dropped asset.)

### 11.4 Outcome space — Quality (from asset) × Tier (rolled)

Quality is set by **which asset** dropped (the enemy's item); Tier is the flat roll (§11.3). The 49-cell space below shows, **for an item of a given Quality**, the chance it rolls each Tier — i.e. each **row is a single fixed-Quality asset**, and the cells are that asset's Tier-roll distribution × its drop weight if you also weight assets by Quality. Held here as the reference space; in practice you read **one row** (the asset's authored Quality) and roll down it for Tier.

| Quality (asset) \ Tier rolled | F (30) | E (24) | D (18) | C (13) | B (9) | A (4) | S (2) |
| ----------------------------- | ------ | ------ | ------ | ------ | ----- | ----- | ----- |
| **F**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |
| **E**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |
| **D**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |
| **C**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |
| **B**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |
| **A**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |
| **S**                         | 30.0   | 24.0   | 18.0   | 13.0   | 9.0   | 4.0   | 2.0   |

Each **row's Tier distribution is identical** (the Tier roll is flat, enemy-independent). The variety between drops comes from **which-Quality asset the enemy carried**, not from a Quality roll. A high-Quality enemy item dropping at a high rolled Tier is the jackpot; both factors are independent of enemy strength (Quality = what they had; Tier = flat luck).

### 11.5 Modifiers & guards

- **Tier roll is FLAT — enemy strength does NOT bias it.** (Supersedes the earlier "encounter threat shifts Tier weights" line.) A weak enemy and a boss have identical per-item Tier odds; reward scales by **what's in their kit** (higher-Quality items, more items) and **how much drops**, not better Tier luck.
- **Luck** can still bend the Tier curve up (a player-side run/perk modifier on the drop roll, §12.3) — that's player-earned, not enemy-derived.
- **Pity floor** — guarantee a high-Tier roll within ~N drops so the curve doesn't flatline on a streak.
- **S Tier dial** — 2% is the rarest roll; raise it (S = 3%) if too rare.

### 11.6 Deferred — unique-effect layer

A future "unique" rarity (PoE-style: fixed *special effect*, build-defining, not raw-stat king) is the home for the parked "legendary-only unique effect." Not needed to ship; bolt on later if playtesting wants a build-defining chase beyond rolls.

### 11.7 Does Quality affect cost?

**It may — Quality is authored & visible, so pricing on it is fair** (unlike the old hidden-roll model). Buy price stays primarily **Tier-keyed** (§4–5: identity dust + Σ stat dust + Gold floor). Quality, being a known authored grade, can layer a multiplier on top (a higher-Quality asset costs more — you can see why before buying). Open number: the Quality price multiplier, if any. Tier remains the dominant price axis; Quality is a known premium, not hidden luck.


---

## 12. Shop & reroll (the round table)

**Status:** Design locked; numbers (reroll cost, cap N) PIE-tunable. The shop reuses the loot generator — no second system.

### 12.1 Shop = a per-run rolled drop table

Each run, the round-table shop **rolls fresh stock** from the same generator as combat loot (§11): it surfaces assets (each carrying its authored **Quality**) and rolls a **Tier** per item. You buy a **rolled instance** off the shelf — same as if it dropped. This is the PoE/Diablo vendor model: refreshing rolled stock, one generator powering both drops and shop.

- Stock differs every run (items, Tiers, hidden Qualities all re-rolled).
- Buy price is still **Tier-keyed** (§4–5: dust + Gold floor). Quality rides along free — you might buy a C item that secretly rolled A-quality.

### 12.2 Reroll

| Reroll           | Cost             | Effect                                                       |
| ---------------- | ---------------- | ------------------------------------------------------------ |
| **Stock reroll** | Prisms (cheaper) | re-rolls the whole shop selection                            |
| **Item reroll**  | Prisms (dearer)  | re-rolls one item's **Tier** (Quality is fixed by the asset) |

- **Currency: Prisms** (run-scoped) — gives Prisms a real in-run sink the round-table rule otherwise left thin.
- **Guard: per-run cap** — N rerolls per run, then locked. Flat, legible ("3 rerolls left"). Preferred over escalating cost.
- The cap doubles as a **perk hook** — a persistent buff (§8) can raise it (+1 reroll/run).

### 12.3 Luck + "better drop" perk = one curve-shift, two sources

Both inputs do the **same thing**: shift the §11.3 **Tier** weight curve toward high grades (better Tier odds), for combat drops **and** shop rolls. (Quality is authored on the asset, not rolled, so Luck/perks bias **Tier only**.)

| Source                              | Scope                                                    |
| ----------------------------------- | -------------------------------------------------------- |
| **Luck** (`ESubStat::Luck`)         | run-scoped — built this run via World Stat Points / gear |
| **"Better drop" account perk** (§8) | permanent — essence-bought                               |

One mechanic (curve-shift), two sources (temporary Luck, permanent perk) — same "one curve, multiple sources" discipline as dust/essence. Mirrors PoE's Increased-Item-Rarity stat fed by gear/perks.

### 12.4 Open numbers (PIE-tunable)

- Stock-reroll Prism cost · item-reroll Prism cost (dearer)
- Per-run reroll cap **N** (and the perk that raises it)
- Luck → curve-shift magnitude (how much Luck bends the weights)


---

## 13. Authored vs runtime — the asset/component split

**Status:** Locked classification. Determines what gets a runtime home vs stays on the asset. Getting this right once is cheaper than retrofitting.

**The rule:** anything a *player* mutates at runtime, while the authored baseline must survive as a template (for enemies, for player-built Lords-as-enemies, for death-reset), needs the split — **asset = authored baseline, component/instance = runtime copy seeded from it.** Things that are authored-and-fixed stay asset-only. Pure-runtime resources need no asset baseline.

| System                                    | Lives where                                            | Why                                                                                                  |
| ----------------------------------------- | ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| **Sub-stats (13 DNA)**                    | **asset only**                                         | player authors at character creation, then fixed — character identity, never mutated at runtime      |
| **World stat levels** (Mind/Body/Spirit)  | asset baseline → **`UCharacterDataComponent` runtime** | run-allocated/rolled; resets on death. Asset holds authored start; component holds live value        |
| **Tier**                                  | asset baseline → **item instance runtime**             | loot rolls it; essence raises it. Asset Tier = authored floor                                        |
| **Quality**                               | **asset only**                                         | authored item identity, never mutated. NOT split (do not build as instance state by analogy to Tier) |
| **Currencies** (Prisms/Gold/Dust/Essence) | **`UCurrencyComponent`**                               | pure runtime resource, no asset baseline                                                             |
| **HP/EP, alive, BD flag**                 | already split (`CurrentHP` on component)               | the template for this whole pattern                                                                  |
| **Items/weapons/rings/crystals**          | already per-instance (`FWeaponInventoryEntry` etc.)    | the instance layer's purpose                                                                         |
| **Bonus-stat / resistance rolls**         | already per-instance (`StatBonus` on entry)            | reroll rework must target the INSTANCE copy, not the asset layer                                     |
| **Durability / wear**                     | per-instance (inert today)                             | when built: asset authors max, instance tracks current — same split                                  |

**Only two NEW splits:** world-stat levels (→ character component) and Tier (→ item instance). Everything else is already split, asset-only, or pure-runtime. No sprawling "make everything runtime" migration.

**Enemy authoring is unchanged:** enemies are authored on the asset exactly as today (a "Fire Lord" asset with WorldLevels 5/5/6, sub-stat DNA, kit). The component seeds from the asset at spawn; since enemies don't allocate/roll, their runtime copy stays at the authored value. Only players diverge, only at runtime. This is also what makes player-built Lords-as-enemies work — the asset baseline IS the snapshot faced.

---

## 14. Rolling stats — Reality dust as the roll currency

**Status:** Design locked. Build slots at the spend/reroll layer (step 7), reworks the resistance generator + adds a cost gate.

**Three separate roll surfaces** (NOT unified into one roll): **roll world stats · roll stats · roll resistance.** Each its own action; they share the cost model below but stay distinct.

### 14.1 Reality dust = the roll currency

**Reality dust is the main currency for stat rolls.** It's the thematic fit — Reality is the meta/wildcard element (already the wildcard substitute, §4.1), so "spending Reality to reshape your stats" reads as lore, not an arbitrary cost. Rolling/rerolling world stats, bonus stats, and resistance all spend **Reality dust**.

### 14.2 Cost matches the scaling-tier cost

Roll cost rides the **same tier curve as everything else** (`TIER_POWER`): rolling at a higher tier costs proportionally more Reality dust, because a higher-tier roll is worth more (tier sets per-point VALUE — §3 gear model). Cost = value, one curve.

- **Bonus-stat / resistance roll** — cost scales by the **item's Tier** (`TIER_POWER(itemTier)` × base Reality-dust cost). An S-item roll costs ~4.8× an F-item roll.
- **World-stat roll** — cost scales by the **World Point level** being rolled at (same curve shape).

### 14.3 Resistance generator reworked to match the stats model

Currently resistance uses a **per-tier budget** (`GetResistanceBudget(Tier)` — "various pool values"); stats use a **flat budget** with tier scaling per-point VALUE. **Rework resistance to mirror the stats model:** flat budget + tier-scales-per-point-value (the same change the tier-power arc made to stats, which resistance never received). Keeps the three rolls *consistent in model* while staying *separate as actions*.

### 14.4 Interface: resource-gated, not points-allocated

Move from "here's N points, distribute" to "pay Reality dust → get a rolled result → pay to reroll." **Keep the internal zero-sum budget as the balance guard** (a roll can't go all-max) — only the *interface* changes from manual point-allocation to resource-cost-gated rolling.

**Open numbers (PIE-tunable):** base Reality-dust cost per roll (before the TIER_POWER multiplier); whether a per-run roll cap applies (mirrors §12.2 reroll cap).


---

## 15. Diamond — premium currency

**Status:** Design locked. Field built with the wallet (§1 / `UCurrencyComponent`); trading faucet deferred. Modelled on Warframe Platinum.

**What it is:** the **premium currency**, on a different plane from the gameplay economy. Sits beside the other five in the wallet but is sourced and spent entirely separately.

|                       | Diamond                                                     | Gameplay currencies (Prisms / Gold / Dust / Essence / WSP) |
| --------------------- | ----------------------------------------------------------- | ---------------------------------------------------------- |
| Source                | **real money** (+ in-game **trade**, deferred)              | gameplay                                                   |
| Buys                  | cosmetics, bundles, account slots, **grind-speed boosters** | power, items, rolls, progression                           |
| Touches combat power? | **NEVER**                                                   | yes                                                        |
| Lives on              | `UCurrencyComponent` (a field)                              | same component                                             |

### 15.1 The hard rule — Diamond never buys power

The single non-negotiable: **a hard wall between Diamond and the power economy.** Diamond buys cosmetics, bundles, account/loadout slots, renames, and convenience — **never** Tier, Quality, Dust, Essence, stat rolls, or anything that wins a fight. (Warframe's discipline: the strongest items can't be bought with Platinum; the only way to get power is to earn it. This is what keeps the loot/roguelite chase meaningful and the game non-pay-to-win.)

### 15.2 Boosters — grind-speed only

Convenience boosters are allowed, held to the Warframe line:
- **Allowed:** speed up *earning* you'd do anyway — e.g. 2× Essence gain, 2× Gold, 2× resource drop *rate*. Raises throughput, not ceilings.
- **Forbidden:** anything touching power or loot *quality* — e.g. better drop odds, higher Tier rolls, fatter Quality. These cross into power and break the chase.

The test: does the booster let you reach something you *couldn't* otherwise, or just reach it *faster*? Faster = OK; couldn't-otherwise = forbidden.

### 15.3 Trade faucet (deferred)

Ideally Diamond is **earnable in-game via player trade** (Warframe model): grind valuable items, sell to paying players for Diamond; spending it sinks it from the economy, real money is the only way to add more. This makes Diamond a gameplay sink, not just a paywall, and lets free players earn it with time. **Deferred** — needs player-to-player trading infra that doesn't exist yet. Build the currency field now knowing trade is the eventual faucet.

### 15.4 Now six currencies

Wallet total: **Prisms, Gold, Dust (14), Essence (2), World Stat Points, Diamond.** Diamond is the only one isolated from the power economy.