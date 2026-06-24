# World of Refraction — Resource Economy Design

**Status:** Design locked; most numbers locked (PIE-tunable drafts flagged). Nothing built. Locked-decision reference, not an implementation spec.

---

### Global convention — always round DOWN

Any time a calculation produces a decimal (essence yields, prices, fractions, wear, fusion-break halves, etc.) → **round DOWN (floor)**. Never round up or to nearest. Rationale: flooring never over-rewards — it always favors the sink, never gives the player free value from rounding. Applies project-wide (`FMath::FloorToInt`). Examples already using it: fusion-break ½-yield, F-quality price half (13 = floor of 12.5).

### Two-hub economy — Main hub (Prisms) vs Run hub (Gold) [DEFERRED — shapes the Pool arc]

Two distinct hubs with different roles, currencies, and what they operate on:

|             | **Main hub** (persistent)                                       | **Run hub** (in-run)             |
| ----------- | --------------------------------------------------------------- | -------------------------------- |
| Currency    | **Prisms**                                                      | **Gold**                         |
| Shops       | hub shops — permanent gear/skill types                          | run shops — run gear/skill types |
| NPCs        | Blacksmith/Jeweler/Spiritualist (upgrade + maintain owned gear) | —                                |
| Operates on | owned pool                                                      | run inventory ↔ pool             |
| When        | between runs                                                    | during a run                     |

**Run hub flow (the in-run loop):**
1. **Draft** — offered ~3 inventories drawn from your own pool → **pick 1** → build your loadout from it. The draft is a curated SLICE, not your whole pool (you may own 100 of everything; a run takes ~10 — "take 10, get to the end"). Gives enough for a full loadout, but loadout SLOTS constrain what you keep:
   - **Base loadout caps:** 1 weapon, 2 rings, 5 spells. Raised by **persistent buffs** (account progression — early runs tight, capacity grows over time).
   - Surplus beyond your slots → **returned for Gold** (that's the "why return" — you're given more than fits).
   - **Themed inventories (designed, requirement-gated):** the 3 options are normally RANDOM, BUT if you meet all the requirements for a specific themed inventory (hand-designed builds), you have a CHANCE to be offered it as one of the options. Discovery/reward mechanic — assemble the requirements → chance to draft a curated themed build. *(Themed inventory definitions + their requirements = a design+content task, deferred with the pool arc.)*
2. **Build** — keep what you'll use this run.
3. **Return** — surplus → back to pool (you still OWN it — "I'll use it a different run") → **Gold**. NOT a loss; you keep the unlock, get Gold now.
4. **Shop** — buy gear/skills for the run with Gold (run shop).

**Two shops, same mechanism, different params:** hub shop (Prisms, permanent gear types) and run shop (Gold, run gear types) are the SAME rolled-stock shop system (§11 shop-roll), parameterized by currency + what they stock.

**Return-for-Gold ≠ break-for-essence (separate flows):**
- **Break (combat)** → pool + **essence** (forced, salvage value).
- **Voluntary return (run hub)** → pool + **Gold** (chosen, "bank for a later run"). NO essence.
Same destination (pool), different trigger + reward. Kept separate so the reward reason is legible.

**This is the Gold/Prisms split made physical:** Gold economy lives in the run hub (earned by returning surplus, spent in run shops), Prisms economy lives in the main hub (permanent purchases + upgrades). Matches the built currency model (Gold run-volatile, Prisms persistent). **Shapes the Pool arc** — the pool layer must support the draft (pool→run), the return valve (run→pool + Gold), and both shop types.

### Inventory architecture — Pool (persistent) vs Run (transient) [POOL DEFERRED]

**Two layers:**
| Layer             | Holds                                                                         | Persists                       | Cap                  | Status                                                       |
| ----------------- | ----------------------------------------------------------------------------- | ------------------------------ | -------------------- | ------------------------------------------------------------ |
| **Pool**          | everything unlocked/upgraded (weapons, spells, evolutions, currencies, items) | ✅ account-persistent           | **unlimited**        | **❌ UNBUILT — deferred arc**                                 |
| **Run inventory** | the drawn working set for one run                                             | ❌ run-scoped (drawn from pool) | **5 evolutions/run** | ✅ this is the CURRENT inventory (FWeaponInventoryEntry etc.) |

**Key rules:**
- **Pool = unlimited owned**; **Run = draw from pool**, max **5 evolutions per run** (the cap is per-RUN, not total-owned).
- **Leveled tiers persist in the POOL** (you keep upgrades across runs). *(⚠️ Today leveling writes to the RUN inventory — until the pool exists, upgrades are effectively run-scoped. The pool arc makes them persist.)*
- **Evolutions are gear** (3rd gear type alongside weapons/rings) — pool-persistent, drawn per-run, same rules.
- **Break splits by item type:**
  - **Gear + skills (weapons/rings/spells/abilities + EVOLUTIONS)** → **return to pool (damaged, repairable) + essence.** Durable layer — not destroyed; goes back to the pool to be fixed (repair = new mechanic, NPC, for a cost).
  - **Items (crystals/stones)** → **CONSUMED.** The burn-fuel layer — broken (or run-end) = gone (→ essence). No pool-return, no repair. You just lose them.
  *(⚠️ INTERIM: clusters iv + B currently DESTROY + essence for everything, because the pool doesn't exist to return to. The essence yield is correct + final; the "gear/skills return to pool instead of destroy" half layers on with the pool arc. Crystals are ALREADY correct (consumed + essence — no change needed for them).)*
- **Locked-on-gear evolutions:** some weapons/rings come with a built-in evolution crystal that CAN'T be removed — it **counts toward the 5/run** (bringing that weapon uses an evo slot).
- **Run draw** (#3): weapons/spells/evolutions are SELECTED by the player from their unlocked pool each run (loadout choice from what you've unlocked/upgraded), not random.

**Gear-attached evolution combat-wear/break — DEFERRED (inert until gear-wear exists):** gear-attached evolutions take NO combat wear today — the evolution wear path (`ProcessPostCastEvolutionWear`) is primary-slot ONLY (the crystal-wear path early-returns for evolution). So a gear-attached evo never wears to 0 in combat → never combat-breaks. **Removal-driven break works** (gear-detach's 10% wear → 0 → DismantleEvolution); only **combat-driven** gear-evo break is missing. Two ordered clusters when ready: **(gear-wear)** a weapon/ring-socket wear path (mirror ProcessPostCastEvolutionWear for the active gear's attached evo), then **(gear-break)** the combat-end sweep handler (keystone + sweep detection already in place — small once gear-wear lands).

**Pool features (design):**
- **The pool = reference of what you own** — view all unlocked gear/spells/items/currencies. The "look at what I own + plan" space.
- **Item loadouts (saved crystal/stone presets):** a quick way to prepare your item RESOURCES — save a set of crystals/stat-stones you want available, so you don't re-gather each run. Pure convenience preset over the stable item pool. *(Gear/spell loadout presets were considered + CUT — you draft + set those up per-run anyway, so pre-planning them is duplicated effort. Items earn a preset because they're a stable owned pool, not a random draft.)*
- **Saved weapon/ring crystal pairings (pre-attach):** you can pre-attach a crystal to a weapon/ring IN THE POOL ("I like this crystal with this weapon") → the weapon **spawns with it equipped** at run start, **deducting the crystal from inventory on spawn**. Convenience (saves manual socketing), but still COSTS the crystal. **Weapons/rings only — NOT evolution crystals** (evolutions must be acquired IN the run, can't be pre-committed from the pool).

**⚠️ Crystals are CONSUMED per run (the burn resource).** When a run ends, two things happen:
1. **Weapons/rings revert to their original bare (un-socketed) state** — back to the pool, ready to re-equip.
2. **Whatever was slotted is LOST** — socketed crystals are consumed/gone (whether pre-attached or socketed mid-run). NOT returned.

So crystals are a **per-run expenditure**: gear is the durable frame (reverts to bare, persists), crystals are the fuel (slot → use → consumed). This ties the crystal economy together — merging (produce higher crystals) + dismantle→essence (recycle) feed the per-run burn. Pre-attach saves clicks, not crystals (still deducted on spawn).

**Design principle — meta-progression health (validated against roguelite discourse):** Player sentiment strongly favors meta-progression that **expands OPTIONS/variety** (the praised pattern: bigger pool, more builds) over **permanent STAT/power inflation** (the criticized pattern: early runs feel weak, progression = grind-gate). WoR sits mostly on the GOOD side (the pool = option-expansion; per-run draft + crystal-burn = fresh build each run, the "temporary layer" that keeps runs distinct — cf. Rogue Core, Slay the Spire). **The watch-point: the LEVELING system is persistent power growth — the criticized axis.** Counterbalances that keep it healthy (all already designed): draft constraints (1 weapon / 2 rings / 5 spells), 5-evo-per-run cap, crystal consumption (re-build item layer each run), themed inventories, and difficulty scaling. **Build rule: leveling should make MORE options viable, not crown ONE** (the tier×quality split + draft-as-slice help — a leveled "unique F" competes with a base S; you don't always draw your favorite). If leveling ever makes one loadout strictly best → the draft degrades to "always pick that" → the criticized grind. Keep leveling broadening, not dominating.

**⚠️ POOL — DISCOVERY SURVEY FINDINGS (2026-06, scope correction):** The pool is MORE than a repointing pass — three things the design assumed exist DON'T:
1. **No persistent layer** — every owned thing (weapons/rings/spells/crystals/CURRENCY) lives on the run-scoped Blueprint character. Nothing survives PIE/app restart. No home above the character.
2. **No save system** — zero USaveGame, zero save/load. The SaveGame tags we've added are DORMANT (nothing serializes them).
3. **No run concept** — no run/campaign object. Only per-battle combat-end hooks. **Combat-end ≠ run-end** (a run spans many combats); nothing represents that boundary.

**So the pool needs SCAFFOLDING built first, THEN the repointing pass.** Recommended home: `UPoolSubsystem : UGameInstanceSubsystem` (idiomatic — ~17 GI subsystems already exist; survives level transitions hub↔combat) + `UPoolSaveGame : USaveGame` (cross-session/disk, activates the dormant tags).

**Three design gaps the code revealed (resolve during build):**
- **Currency needs the pool home too** — UCurrencyComponent (account-persistent per design) also lives on the run character. The pool holds currency, not just items.
- **Dual-write/sync (the subtle one):** leveling is "immediate AND permanent" — a mid-run level-up must update BOTH the run copy (wield now) AND the pool (bank). Current "leveling writes the run entry" can't persist. Decide the sync rule (dual-write, or write-pool-then-resync-run).
- **Combatant lifetime is BP-defined** — can't confirm from C++ whether owned components survive combat-to-combat or respawn per battle. Verify in-editor (or discover at the draw build).

**Build skeleton (dependency order, smallest-safe-first):**
1. **Persistent home (in-memory)** — UPoolSubsystem holding the owned pool (mirrors entry structs + currency). INERT (nothing draws yet). The safe foundation.
2. **Save system** — UPoolSaveGame + load-on-start/save-on-change (activates SaveGame tags). Cross-session.
3. **Run-state object** — run boundary (GameMode or RunSubsystem) exposing run-start/run-end events. The hook layer for draw/return.
4. **The draw** — at run-start, populate run inventory from the pool draft (wrap InitializeFromCharacterData's source: asset → pool). Slot-capped, instance-state copied.
5. **The repointing pass** — redirect persistent writes (LevelUp*/Downgrade*, acquisition, dismantle, currency) run→pool; keep combat-transient (durability/break) run-only. Resolve dual-write.
6. **The return** — run-end: gear reverts-to-bare + persists, surplus → Gold, crystals consumed.
7. **(Parallel)** draft UI + two-hub level structure (BP).

**The draw/return hooks (found):** draw wraps `InventoryComponent::InitializeFromCharacterData` (currently seeds run inventory from authored UInventoryData asset → swap source to pool). Return needs a run-end hook that doesn't exist yet (depends on phase 3 run-state).

**⚠️ The Pool is the next major foundational arc** — it's the persistent/account layer (tied to the save system + PlayerState, all deferred together). It's mostly a REPOINTING pass (redirect persistence + add the run-draw), not net-new — so it's done AFTER the run-layer economy actions are complete (build the full surface, then migrate persistence in one pass, like the tier-on-instance migration). Repair mechanic + break→pool-return land with it.

### Hub NPCs — service registry (outline as we go)

The hub round-table has service NPCs gating maintenance/upgrade/removal. **Players can ATTACH (self-service), but UPGRADING and REMOVING go through the relevant NPC.** This makes the hub meaningful and gives each action a *place*.

| NPC              | Handles   | Services                                                                                              |
| ---------------- | --------- | ----------------------------------------------------------------------------------------------------- |
| **Blacksmith**   | weapons   | upgrade (tier-up), remove crystals/attachments                                                        |
| **Jeweler**      | rings     | upgrade (tier-up), remove crystals/attachments                                                        |
| **Spiritualist** | evolution | upgrade (tier-up) evolution, remove evolution (from primary OR gear), attach evolution to weapon/ring |

**Self-service (no NPC):** attaching an evolution to the **primary slot** (player clicks the evolution → confirmation → slotted). Everything else (level, remove, attach-to-gear) is NPC-gated.

**Backend note:** the `LevelUp*` / dismantle / attach / detach methods (UEconomyService / loadout) are the same regardless of caller — the NPC-vs-direct distinction is a UI/trigger layer on top. C++ ops built first; NPC interaction wiring is a later UI layer.

*(More NPCs to be outlined as systems are designed — this is a living registry.)*

### Deferred — auto-dedupe: keep stronger, weaker → essence

**Idea:** gaining a duplicate of something you already own → automatically **keep the stronger copy, convert the weaker to essence** (its dismantle yield). Keeps the pool to one best-copy-of-each; surplus auto-converts. Fits the take-or-scrap philosophy, just automatic for things you own a better version of.

**Open questions (settle when building):**
- **Scope:** all items (weapons/rings/evolution/spells/crystals) or spells/abilities specifically? (Likely all — a general QoL auto-merge.)
- **"Stronger" measure:** combined tier+quality value (§5.1b model)? strictly tier (power)? tier-first, quality tiebreaker? (Wrinkle: a dupe higher-tier-but-lower-quality vs lower-tier-but-higher-quality — combined value resolves it.)
- **Yield:** the weaker converts to its normal deconstruct essence at its tier (auto-dismantle on dupe-pickup).
- **Where:** matters most at the POOL (unlimited owned — don't want 50 Fireballs). Likely a pool-layer mechanic → build with the pool arc, or when dupe-pickup volume makes it needed.

**Status: NOTED — build when the time comes** (good fit alongside the pool arc or the spell-instance leveling, since both involve instances + dismantle).

### ⚠️ DEFERRED ARC — Spell/Ability instance-tier (the big one)

Spell/ability leveling + leveled-tier dismantle/pricing are **deferred** behind a materially large arc. Survey finding (key): unlike weapons/rings (equipped by INSTANCE), spells are **equipped by VALUE (bare USpellData* everywhere)** — AssignedSpells, InnateSpells, BDSpellPools, EvolutionSpells, FSavedLoadout, FCombatCapabilities, the command menu DataRef, and Action.SpellData. A spell's instance tier **cannot reach a cast** without rebuilding the entire spell equip pipeline to be instance-based.

**Scope (chosen shape (b), FSpellInstance wrapper):**
- (i) FSpellInstance{ Spell, Tier, Quality, InstanceID } replacing the bare arrays — ~12 mechanical .Spell rewrites. SMALL but **INERT** (combat still reads asset).
- (ii) Thread instance identity through equip chain + FAction — FSavedLoadout spell InstanceID (**+ save migration**), FCombatLoadout resolution, capabilities, menu, FAction tier field. **15-20+ sites, the bulk + the risk.**
- (iii) Repoint the ~34 skill Tier reads to the threaded instance tier.
- (iv) LevelUpSpell/Ability (reuses TryLevelUpEntry) + dismantle/purchase repoint.

**⚠️ The trap:** cluster (i) alone LOOKS like it works (tier in inventory, leveling writes it) but does NOTHING in combat — the value is gated entirely behind (ii). No partial-value path. All-or-nothing arc.

**Recommendation:** own dedicated session. Bigger than the entire weapon/ring migration; carries save-migration risk. Survey complete (the plan exists) — execute when fresh, not as a session tail. Until then: spells/abilities use asset tier everywhere (dismantle/purchase/combat) — correct + parity-safe, just not leveling-aware.
**Scope:** The roguelite resource economy — Gold, Prisms, Essence, Essence, World Stat Points, Diamond — the round-table run loop, and the persistent account-perk sink.
**Suggested repo path:** `docs/Design/Resources_Design.md`

> **BUILD-STATE NOTE (per CC survey, 2026-06).** This doc is DESIGN. Code reality differs in three ways the survey caught: (1) **no reward system exists** — World Stat Points / Gold / Prisms are not awarded anywhere in C++; the post-defeat reward hook is greenfield at `CombatOrchestrator.cpp:576`. (2) **Tier lives only on the data asset** — there is no per-instance Tier; the loot + leveling model below REQUIRES building tier-on-instance first. (3) **Quality does not exist in code** — but under the revised model (§11.2) it's just an authored asset field, not a roll, so it's trivial to add. The instance layer (`UInventoryComponent` + per-type entry structs) IS already built, despite `InventorySystem_Design.md` claiming otherwise.

---

## 1. The five resources

| Resource               | Type               | Lives                                    | Earned by                                               | Spent on                                                  |
| ---------------------- | ------------------ | ---------------------------------------- | ------------------------------------------------------- | --------------------------------------------------------- |
| **Gold**               | run economy        | **always lost at run end** (win or lose) | per-encounter reward                                    | in-run upgrades, buffs, consumables                       |
| **Prisms**             | hub economy        | persistent (banked)                      | per-encounter reward (banks on exit)                    | **hub buy-currency** — equipment + spell floors           |
| **Essence** (14 types) | acquisition        | persistent                               | dismantling crystals/stones + combat (Reality/wildcard) | spell purchase surcharge (typed)                          |
| **Gear Essence**       | growth (equipment) | persistent                               | dismantling weapons + rings                             | tier-leveling weapons/rings + account perks               |
| **Skill Essence**      | growth (skills)    | persistent                               | dismantling abilities + spells                          | tier-leveling abilities/spells                            |
| **World Stat Points**  | run power          | **volatile (reset every run)**           | per-encounter (= avg enemy stat-total)                  | Mind / Body / Spirit allocation, for the current run only |

**The split:** the **run** carries only volatile spend (Gold — always lost at run end) + volatile power (WSP). All **acquisition and permanent buying happens at the hub** (Prisms + Essence). Prisms is **shared account-wide (partial)** as a veteran catch-up mechanic (a maxed character's surplus helps new characters skip early grind). Essence is earned by **dismantling** equipment/skills (take-or-scrap on rewards + dupes) and spent to level your kit (immediate + permanent).

**Gold (run) vs Prisms (hub)** are two distinct currencies — the Dead Cells *Gold (run, volatile) / Cells (meta, banked)* split. Gold = the fleeting in-run currency (always lost at run end); Prisms = the lasting hub buy-currency.

---

## 2. Tier scale — the spine everything references

Keyed on `EItemTier` (F…S):

| Tier                                     | F    | E    | D    | C    | B    | A    | S    |
| ---------------------------------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| **Essence weight** (linear ref)          | 1    | 2    | 3    | 4    | 5    | 6    | 7    |
| **TIER_POWER** (×, the real power curve) | 1.00 | 1.30 | 1.70 | 2.20 | 2.85 | 3.70 | 4.80 |

`TIER_POWER` already exists in code (`TierPowerConstants.h`) and drives combat power. Essence pricing and enemy-threat both ride it, so reward never drifts from real power.

---

## 3. Gear & Skill Essence — the growth currencies (permanent)

Two growth currencies, **split by category** (both single scalars), each a per-character persistent currency in `UCurrencyComponent`:

| Currency          | Source (dismantle)                 | Levels             |
| ----------------- | ---------------------------------- | ------------------ |
| **Gear Essence**  | dismantling **weapons + rings**    | weapons + rings    |
| **Skill Essence** | dismantling **abilities + spells** | abilities + spells |

**Self-balancing by design:** you dismantle the *same category* you level — scrap a weapon → Gear Essence → level a weapon; scrap a spell → Skill Essence → level a spell. Source and sink are the same category, so there's no cross-category unfairness. (This is why a category split works here but a *use-based* faucet wouldn't: you earn by scrapping the same kind of item you spend on, not by using things. A weapon-user scrapping a duplicate spell gets Skill Essence — which is correct, because that's what spells cost; nothing is "locked to a kit they don't use" since they chose to scrap a spell.)

**Single source — dismantling (NO per-action faucet).** Both essences come *only* from **dismantling** their category. There is **no** earn-by-using-things faucet — the old per-action model (cast/crit/defend/parry grants) is **removed**.

**The reward fork:** every reward is a choice — **take it** (own it) *or* **dismantle it** (essence). Duplicates in inventory can be dismantled anytime. This is the standard salvage loop (Diablo/Destiny): every drop is a meaningful take-or-scrap decision, and dupes have an obvious purpose.

> **MP win:** dismantling is a deliberate hub/menu action → trivially server-authoritative. This *removes* the real-time local-input reconciliation problem the per-action faucet had (parry/perfect-defend fire on local input → would double-grant under MP). Dismantle-only essence is dramatically simpler to build correctly networked.

**Yield** applies to ALL dismantlable items (weapons/rings/abilities/spells), derived from essence's OWN curve (essence ≠ essence). Yield = **½ × the cumulative essence to build that item to its current Tier** (the cumulative column below), so breaking < building:

| Skill Tier | Cumulative essence to build | Deconstruct yield (½) |
| ---------- | --------------------------- | --------------------- |
| F          | 10                          | 5                     |
| E          | 30                          | 15                    |
| D          | 60                          | 30                    |
| C          | 100                         | 50                    |
| B          | 150                         | 75                    |
| A          | 220                         | 110                   |
| S          | ~290                        | ~145                  |

**Routing — by category:** weapons + rings → `AddGearEssence(...)`; abilities + spells → `AddSkillEssence(...)`. **Evolution dismantle → typed ELEMENT essence** (not Gear — evolutions yield element essence by their element, like crystals). What you break feeds the currency that levels the same category.

**Rule:** `Add{Gear|Skill}Essence(dismantleYield[item.currentTier])` — **flat by current Tier** (a leveled dupe yields by where it sits now; no separate invested-essence refund, matching the crystal model).

- **Build dependency:** the deconstruct *action* needs tier-on-instance (to read a dupe's current tier) + the skill collections — a **step-7 spend-layer feature**, not part of the currency-wallet build. The wallet only needs `AddEssence(pool, n)` (which it has); deconstruct calls it later.
- **Guard:** the ½ spread keeps acquiring-and-keeping worthwhile vs scrap-everything — dismantling returns less than building, so you can't farm-scrap-rebuild for free value.

### 3.1 Effect Transfer (deferred — own design thread)

**Idea:** dismantling shouldn't *only* give raw essence — it could optionally **extract the item's aspect/effect** as a transferable buff instead of (or alongside) the essence. You break a Fire weapon → choose: take its essence value, OR harvest its "fire aspect" as a buff you bank. The buff can land as:
- a **persistent buff** (account-level, permanent — lives in §8 perks), or
- a **gear buff** (grafted onto a specific item you own).

This is the Destiny-2/PoE-imprint model — salvage gives currency *and* lets you transfer an item's identity onto something you keep. Makes dismantle a richer choice: scrap for essence, or harvest the special property.

**Status: DEFERRED — own design pass.** This is a meatier system than the essence-source change (needs: what counts as an "aspect," how many buff slots gear has, can aspects stack, extraction cost). Not scoped here. Flagged so it isn't lost — revisit as the **Effect Transfer** arc after the core essence/dismantle loop is built.
- **PIE-tunable:** the ½ spread (vs ⅓ for costlier scrapping); cumulative-to-tier (fairer) vs per-step (stingier) — locked at ½ × cumulative.

**Tier-up cost (per step), partial-spend allowed:**

| Step              | F→E | E→D | D→C | C→B | B→A | A→S |
| ----------------- | --- | --- | --- | --- | --- | --- |
| **Essence**       | 10  | 20  | 30  | 40  | 50  | 70  |
| **Cumulative F→** | 10  | 30  | 60  | 100 | 150 | 220 |

**In-run leveling:** essence is spent **during a committed run** to tier-up the kit you brought. The upgrade is **immediate** (you wield the stronger version for the rest of the run) and **permanent** (banks, survives death). The constraint is **scope, not timing** — you can only level *what you brought*, never acquire or swap to something new mid-run.

**Essence sinks (both permanent):** tier-leveling a weapon/crystal/spell/ability · account perks (§7).

---

## 4. Essence (typed, 14) — spell acquisition surcharge (permanent)

> **Naming note:** "Essence" now covers TWO distinct currency families that do NOT merge: (1) this section's **14-type typed essence** (Fire/Mind/etc — from dismantling crystals/stones, spent acquiring spells/stones), and (2) the single **gear-leveling essence** (§3 — from dismantling equipment/skills, spent tier-leveling your kit). Different currencies, different sources, different sinks; both spoken of as "essence," distinguished in code by type (`EEssenceType` for the 14 vs the single gear-essence field).

### 4.1 The 14 wallets

| Family  | Count | Members                                                                                     | Source                 |
| ------- | ----- | ------------------------------------------------------------------------------------------- | ---------------------- |
| Element | 10    | Fire, Water, Lightning, Wind, Earth, Light, Darkness, Void, Reality, **Generic (= Quartz)** | gem dismantle          |
| Pillar  | 3     | Mind, Body, Spirit                                                                          | stat-stone dismantle   |
| Ability | 1     | Ability                                                                                     | AbilityStone dismantle |

- **Quartz = Generic** in the existing `CrystalTypeHelpers::GetElement` map → Generic spells pay in Quartz/Generic essence automatically, no special rule.
- **Reality essence = wildcard substitute** (covers any element line at a premium) and drops from combat directly, so it flows without Iolite.

### 4.2 The unit — steep curve, buy/sell spread

Essence rides `TIER_POWER` (×10), with a **2:1 buy/sell spread** (dismantle pays half of purchase cost — the vendor margin that makes it an economy, not a passthrough):

| Tier                       | F   | E   | D   | C   | B   | A   | S   |
| -------------------------- | --- | --- | --- | --- | --- | --- | --- |
| **Dismantle yield** (sell) | 5   | 7   | 9   | 11  | 15  | 19  | 24  |
| **Purchase cost** (buy)    | 10  | 13  | 17  | 22  | 29  | 37  | 48  |

### 4.3 The two conversions (both ride the purchase-cost row)

**Spell-tier conversion** — the spell's own tier → its identity (element) essence:

| Spell tier      | F   | E   | D   | C   | B   | A   | S   |
| --------------- | --- | --- | --- | --- | --- | --- | --- |
| Element essence | 10  | 13  | 17  | 22  | 29  | 37  | 48  |

**Scaling-tier conversion** — each scaling stat's grade → that pillar's essence:

| Scaling grade  | F   | E   | D   | C   | B   | A   | S   |
| -------------- | --- | --- | --- | --- | --- | --- | --- |
| Pillar essence | 10  | 13  | 17  | 22  | 29  | 37  | 48  |

(Same numbers — essence value is essence value. Spell-tier applies it to the element line; scaling-grade applies it to each stat line.)

### 4.4 Full spell price = Prisms floor + Essence

> **Spell price (full) = element essence (spell tier) + Σ pillar essence (each scaling grade) + Prisms base (tier-double) + Prisms scaling surcharge (50 × each grade).** This section (§4) covers the typed-essence components; §5.2 has the full three-component breakdown + worked totals.

Identity = element (spells, by tier) · ability (abilities + AbilityStone) · the stat itself (stat-stones). No separate ×tier-weight multiplier — `TIER_POWER` is already baked into the essence values.

**Worked examples (essence portion):**

| Spell                               | Tier | Scaling | Components                                | Essence total |
| ----------------------------------- | ---- | ------- | ----------------------------------------- | ------------- |
| Fire `{RawDmg@F}`                   | F    | F       | 10 Fire + 10 Body                         | **20**        |
| Fire `{SpellDmg@C, Luck@D}`         | C    | C, D    | 22 Fire + 22 Mind + 17 Spirit             | **61**        |
| Void `{SpellDmg@A, Luck@D}`         | A    | A, D    | 37 Void + 37 Mind + 17 Spirit             | **91**        |
| Void `{SpellDmg@S, Luck@A, Crit@B}` | S    | S, A, B | 48 Void + 48 Mind + 37 Spirit + 29 Spirit | **162**       |

(Plus the Prisms floor on each, §5.)

### 4.5 Anti-exploit (the "F-farm" guard)

Bypass to prevent: shredding common F-crystals to fake high-tier purchases. Two structural deterrents:

1. **Steep curve + spread** — S line costs 48, F crystal yields 5 → ~10 F-crystals per S line; a full S spell ≈ 30+ F-crystals across types.
2. **Type-matching** — an S Void spell needs **Void + Mind + Spirit essence specifically**; an F Garnet can't supply them.

**Substitution (only softener):** pillar→specific or Reality→element at **1.5 : 1**. Keep modest so it doesn't reopen the bypass.

---

## NPC Registry — service gateways (living list)

NPCs are the **service gateways** for item operations — attaching/unslotting, and likely more as systems are designed. Pattern: each domain has an NPC; services attach to NPCs as they're designed. **All slotting/unslotting is NPC-mediated** (no self-service).

| NPC              | Domain    | Services                                                                               |
| ---------------- | --------- | -------------------------------------------------------------------------------------- |
| **Blacksmith**   | Weapons   | weapon services (TBD — repair? reforge? crystal-socketing onto weapons?)               |
| **Jeweler**      | Rings     | ring services (TBD)                                                                    |
| **Spiritualist** | Evolution | slot/unslot evolution (primary slot AND onto weapons/rings); evolution is their domain |

**Evolution slotting/unslotting:**
- **Slot to primary** → **SELF-SERVICE** (player clicks the evolution → confirmation → slotted; no NPC).
- **Slot onto weapon/ring** → **Spiritualist** (NPC-gated).
- **Upgrade (tier-up) evolution** (primary or inventory; NOT gear-attached) → **Spiritualist**.
- **Remove from primary** → **Spiritualist** → **destroys the entry** + dust (dismantle yield at leveled tier) → **frees a slot** (now ≤4 of 5).
- **Remove from weapon/ring** → **Spiritualist** → un-references (entry was always owned — just stops being referenced by the gear) + 10% wear. Still 1 of 5. (NOT a re-creation — the entry never left under the reference model.)

(Players ATTACH to primary themselves; ALL upgrading + removal is NPC-gated.)

**Locked mechanics (the tier/durability/essence flow):**
- **Attach = REFERENCE, not move.** The owned `FEvolutionInventoryEntry` PERSISTS when slotted (primary OR gear); the slot just references it (via `PrimaryEvolutionInstance` GUID / the gear attachment). A slotted evolution STILL COUNTS toward the owned cap wherever it is.
- **5-evolution cap (owned total).** Max **5** evolution crystals owned — bag + primary-slotted + gear-slotted ALL count. You cannot exceed 5 by parking them on gear (reference, not move — slotting doesn't free a slot). Evolution crystals are precious; committing one to a weapon is a real opportunity cost (1 of your 5, locked there until the Spiritualist removes it).
- **Cap enforced at ACQUISITION** (can't acquire a 6th while at 5) — with **escape valves: sell or deconstruct** an owned evolution to free a slot, then acquire. Manage which 5 you keep.
  - **Deconstruct** = the evolution dismantle (DismantleEvolution → element essence, gear amount, at tier). Available from inventory, not just on primary-removal — same mechanic.
  - **Sell** = a NEW vendor path → currency (Prisms?) instead of essence. *(Open: sell yield + whether sell is evolution-specific or a general sell-to-vendor mechanic for all items — flag as its own small design thread.)*
- **Gear-removal wear hits DURABILITY, tier preserved.** Remove an S-evo from a weapon → returns to inventory at **S** (tier kept) with **10% durability gone**. Wear never touches tier.
- **Break = forced dismantle (the item's normal yield at current tier).** A crystal/evo worn to 0 → breaks → yields exactly what dismantling it would: a broken S-evo = S evolution-dismantle yield (element essence, gear amount); a broken item crystal = its crystal-curve typed essence. **The two different essence values apply** — evolutions yield gear-amount element essence, item crystals yield crystal-curve typed essence (per §3/§4.5).

*(NPCs are the UI/trigger layer; the backend ops are UEconomyService/loadout methods. NPC interaction wiring is deferred — the C++ ops are built first, NPC triggers layer on top.)*

## 4.5 Crystal / Stone merging — tier-up by combining (item crystals + stones ONLY)

Low-tier item crystals (gems) + stones (stat/ability) climb tiers by **merging** — the fungible-crystal upsink that makes low-tier drops useful. **Scope: item crystals + stones ONLY. Does NOT touch evolution crystals** (those tier-up via essence leveling, the separate innate-leveling path, §5.3b).

**Value-based merge (pick-the-target).** Each crystal tier has a **value** (F-unit ladder, the compounding from the 2:1/3:1 design): **F1 · E2 · D4 · C8 · B16 · A32 · S96.** To make a target tier, your same-type crystals must **sum to ≥ the target's value**; if so, the merge consumes exactly that value (+ Prisms) and produces 1 of the target. If not, you can't make it yet.

**Rules:**
- **Pick the target** (e.g. "make a C" = value 8). Have ≥8 worth of that type → consume exactly 8 + Prisms → get 1 C. Not enough → can't make it (no partial).
- **Like-into-like (element/type):** same `Type` (encodes element+variant — there is no separate Variant field; `FCrystalId` is `{Type, Tier}`). 2 Fire-D → 1 Fire-C; you cannot pour Fire + Water together.
- **Consume lowest-tier-first**, and only crystals **≤ the target tier**. You build *up* from smaller pieces (burns the chaff, preserves your high crystals); you never shatter a higher crystal to make a lower one (no breaking a B to make a C).
- **No waste:** consumes exactly the target's value. (If the lowest-first walk can't hit the value *exactly* with available crystals — e.g. only D's [value 4] toward a C [8] = 2 D's, exact ✓ — it takes the minimal set that meets-or-exceeds, see note.) *(Open: if exact value isn't achievable with the available denominations, take the minimal set that meets-or-exceeds the target — small overshoot consumed; the alternative is reject-unless-exact. Default: minimal-meets-or-exceeds. PIE-tunable.)*

**The value model generalizes the old fixed ratio:** 2 D's = value 8 = a C (the old 2:1 step); 3 A's = 96 = S (the old 3:1 final). The fixed ladder is the special case where you use only the directly-lower tier — but now you can mix lower tiers freely to reach the target.

**+ Prisms cost** (hub activity): scaled by produced tier at **½ the §5.1 buy price** — E25/D50/C100/B200/A400/S800 (`GetPrismsBaseForTier(produced)/2`). Producing S = 96-value of crystals + 800 Prisms.

**Why merging is the crystal tier-up path:** the three-way progression split — item crystals/stones tier up by **merging**; evolution crystals + weapons/rings/skills tier up by **essence leveling**. No overlap.

## 5. Prisms (hub) + Gold (run) — pricing

### 5.1 Prisms — the hub buy-currency

Prisms buys **everything permanent at the round table**: equipment and spells. Banked between runs. Spell purchases cost **typed Essence + Prisms** (both — the Essence is the acquisition ingredient, the Prisms is the money).

**Prisms base cost — doubles per tier** (start 25 at F; steep 64× spread, so S is a serious sink):

| Tier            | F   | E   | D   | C   | B   | A   | S    |
| --------------- | --- | --- | --- | --- | --- | --- | ---- |
| **Prisms base** | 25  | 50  | 100 | 200 | 400 | 800 | 1600 |

This base applies to **any purchase** keyed on the item's tier (equipment, spells, crystals/stones).

### 5.1b Equipment price = Tier-half + Quality-half (50/50 split)

Equipment price factors **both** Tier and Quality, split 50/50 — each axis contributes its own doubling half-scale (maxing at 800), summed. An **SS** item (S-tier × S-quality) = 800 + 800 = 1600 (the old full tier-double cap).

**Each axis — doubling half-scale:**

| Grade      | F   | E   | D   | C   | B   | A   | S   |
| ---------- | --- | --- | --- | --- | --- | --- | --- |
| Half-value | 13* | 25  | 50  | 100 | 200 | 400 | 800 |

**Price = Tier-half + Quality-half.** Examples:
| Item (Tier × Quality) | =       | Total |
| --------------------- | ------- | ----- |
| F × F                 | 13+13   | 25    |
| C × C                 | 100+100 | 200   |
| S × S                 | 800+800 | 1600  |
| S × F                 | 800+13  | 813   |
| F × S                 | 13+800  | 813   |
| C × S                 | 100+800 | 900   |
| A × C                 | 400+100 | 500   |

**Why the off-diagonals matter (the key design point):** Tier and Quality are INDEPENDENT axes of value. **Tier** = raw power ceiling (scales stats/damage). **Quality** = the roll's excellence (affixes/properties — the "is this a *good* version"). So an **F×S** is a "unique F" — a low-tier item with an EXCEPTIONAL roll, genuinely valuable (≈813, not 25) because its quality is special. An **S×F** is high-ceiling but poorly-rolled. Each is worth ≈half-max because it's excellent on ONE axis. **You pay for excellence on either axis** — which makes high-quality low-tier items a real chase, and makes the drop-quality roll matter as much as the tier roll.

**Backward-compatible:** the matched diagonal (F×F, C×C, S×S = 25/200/1600) reproduces the old tier-only doubling table exactly — this just lets the axes diverge.

*F-half = 13 (½ of 25, rounded). *(Open: 50/50 weight confirmed — tier and quality equally valuable. Could shift to tier-weighted later if quality proves too dominant. PIE-tunable.)*

### 5.2 Spell purchase = Essence + Prisms (three components)

A spell's full price stacks:

1. **Typed Essence** (§4.3): element essence at spell tier + Σ pillar essence at each scaling grade — the acquisition ingredients.
2. **Prisms base** (§5.1 table): the tier-doubling money floor.
3. **Prisms scaling surcharge**: **50 × each scaling entry's grade** (F=1 … S=7). A heavily-scaling spell costs more because each scaling stat, at its grade, taxes the price.

> **Worked — `{SpellDmg@C, Luck@D}`, C-tier Fire spell:**
> - Typed Essence: 22 Fire + 22 Mind + 17 Spirit = **61 essence**
> - Prisms base (C): **200**
> - Prisms surcharge: 50 × (C=4 + D=3) = **350**
> - **Total: 61 typed Essence + 550 Prisms**

> **Worked — `{SpellDmg@S, Luck@A, Crit@B}`, S-tier:**
> - Prisms base (S): 1600 + surcharge 50×(7+6+5)=900 = **2500 Prisms** + the typed Essence (§4.3).

Equipment (weapons/rings) = **Prisms base by tier** (no scaling surcharge — equipment scaling is authored differently; surcharge is a spell-pricing tool). Crystals/stones = Prisms base by tier + their typed-Essence purchase cost (§4.2).

### 5.3 Ranking up (tier-up) = Gear/Skill essence + ½ Reality essence

Tier-leveling an item costs **two currencies** (NOT Gold):

- **The item's leveling essence, full** — Gear Essence for weapons/rings, Skill Essence for abilities/spells (§3, the category split).
- **+ Reality essence = HALF the leveling-essence amount** — a Reality companion cost, half the size of the main cost. (Reality = the reshape/meta currency; ranking up reshapes the item's power.)

So a tier-up that costs 100 Gear essence *also* costs 50 Reality essence. **No Gold** — upgrading is funded by the dismantle-fed essence economy + Reality, not the run currency.

> *Open:* the exact leveling-essence base per tier-step (scales — a C→B costs more than F→E, riding the §3 curve). The Reality half follows automatically (½ the Gear/Skill amount). PIE-tunable.

### 5.x — DEFERRED: full item pricing pass (its own session)

**Not yet priced — needs a dedicated pass.** §5 prices spells, equipment (by the Prisms tier-double), upgrades, and rerolls — but the **per-item price list is not enumerated.** Every buyable item needs a tier-keyed price. Locked rules for that pass:

- **Crystals > Stones** — a gem (crystal) costs MORE than a stat-stone of the same tier (crystals are the more valuable attach).
- **Evolution at the top** — evolution (3rd gear type) is premium, base 500 @ F (§5.3b), far above normal items.
- Everything rides tier scaling (`TIER_POWER` / the Prisms tier-double, §5.1).
- Ordering, roughly cheapest→dearest: stones < crystals < weapons/rings < spells (3-component) < evolution.

**Status: DEFERRED.** Too many items to price inline; do it as a focused session AFTER the rings + spells migration work. Flagged here so it isn't lost.

### 5.3b Evolution — third gear type (premium, persistent attach)

Evolution is a **third gear type** alongside weapons and rings — an **evolution crystal** you attach (to the primary slot, or onto a weapon/ring). Attaching it gives the host the evolved state (spell-holding slots, higher loadout-slot cost). It is NOT a transform *action* with a cost — it's a **piece of gear you acquire and attach.**

**Premium price.** Evolution costs far more than a normal item — **base 500 at F tier**, scaling up by tier. (Compare: a normal F item is 25 Prisms — evolution is ~20× pricier, fitting its power as the spell-unlocking gear.) *(Open: the per-tier curve from 500 — doubling [500/1k/2k/4k/8k/16k/32k] is steep; confirm the top, or use a gentler scale. Currency: Prisms-based like other gear, OR the Gear+Skill essence premium discussed — confirm.)*

**Dismantle yield — hybrid (element TYPE, gear AMOUNT):** dismantling an evolution crystal yields **element essence** (the crystal path — it carries an element), but in the **gear/skill leveling AMOUNT** (the §3 curve F5/E15/D30/C50/B75/A110/S145), NOT the smaller crystal curve. Rationale: evolution is premium gear, so scrapping it returns gear-level value — just denominated in the element's typed essence rather than Gear essence. (A C-tier Fire evolution → 50 Fire essence, not 11.) *(Open: element-typed evolutions yield their element; if evolution crystals can be element-agnostic, confirm which essence — likely Reality/wildcard.)*

**Evolution tiers up ("leveling your innate").** Evolutions level like weapons/rings — same Gear + ½ Reality essence cost (§5.3), via the shared tier-up core. Think of it as leveling your character's innate power.

**Evolution lifecycle — where it can level + how removal behaves (corrected):**

**Levelable ANYWHERE EXCEPT gear-attached:**
- In **inventory** (unslotted) → levelable ✓
- In the **primary slot** → levelable ✓
- **Attached to a weapon/ring** → **LOCKED** (frozen, can't level) ✗

So both the inventory form (`FEvolutionInventoryEntry`) and the primary-slot form need a mutable, targetable `.Tier`; the gear-attached form is frozen.

**Tier flows across the lifecycle (must be preserved):**
| Action                                              | Behavior                                                                                                                                 |
| --------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| **Slot** (→ primary or → weapon/ring)               | copy inventory `.Tier` → attachment `.Tier` (an A stays A)                                                                               |
| **Level** (inventory OR primary, NOT gear-attached) | write `.Tier` via shared `TryLevelUpEntry` (Gear + ½ Reality, §5.3)                                                                      |
| **Remove from PRIMARY slot**                        | **DESTROYED → yields dust** (the evolution dismantle: element essence at gear amount, at current leveled tier). No wear (it's consumed). |
| **Remove from WEAPON/RING**                         | **returns to inventory INTACT** (tier preserved) **+ 10% wear** (see below)                                                              |

**⚠️ Design reversal to action:** the CODE currently assumes "slotted evolution = destroyed on removal" (`FEvolutionAttachment` has no GUID). §5.3b's "remove from weapon/ring → return to inventory intact + wear" REVERSES that. The gear-attached form must gain enough identity to be re-created as an inventory entry on removal. Knowingly overriding the old invariant.

**⚠️ Architectural keystone — runtime loadout must carry the slotted evolution's instance GUID.** Currently the `PrimaryEvolutionInstance` GUID exists only on the SAVED loadout (read once at inflation, dropped at runtime). But under the reference model, a slotted evolution maps to an OWNED entry that must be destroyed→essence when it breaks (in combat) or is removed (Spiritualist). **Combat breaks are automatic (no UI to pass a GUID)** — so the runtime `FCombatLoadout` must RETAIN the instance GUID, making it the single source of "which evolution is slotted." Both combat-break (tell inventory: destroy + convert + free slot) AND Spiritualist-removal resolve the owned entry from this runtime GUID. This is a foundation cluster before the removal/wear/break paths.

**Build sequencing (survey-scoped):**
- **Cluster (i) — DOABLE NOW (clean repoint):** add `.Tier` to `FEvolutionAttachment`, copy it at the primary-slot inflation (instance infra already exists — `PrimaryEvolutionInstance` GUID), repoint the deferred reads (2c budgets ×4, 2d combat, display ×3, + CrystalManager:218). Closes the last tier-migration gaps. Low risk — evolution is already instance-threaded (unlike spells).
- **Clusters (ii)-(iv) — DEFERRED (new mechanics):** (ii) LevelUpEvolution (reuses TryLevelUpEntry), (iii) the two unslot paths (primary→DismantleEvolution+dust, gear→inventory+wear — needs the design reversal above + a NEW DismantleEvolution + a NEW runtime gear-attach op that doesn't exist), (iv) wear-on-removal wiring + 0-durability break→essence (the wear primitive `ApplyWear` exists; break→essence is new). These are a separate evolution-mechanics feature arc.

**Evolution breakability — `Breakable` by default (decision).** Evolutions default to **`Breakable`** (break under ANY wear), changed from `BDBreakable` (only BD/Reality wear). Knowingly making evolutions break like normal crystals (not premium-tough). This + a gear combat-wear path = gear-attached evolutions wear and break in combat like primary-slot ones do.

**Wear on gear-removal (crystals + evolutions, NOT stones):**
- Removing a crystal from a weapon/ring applies **flat 10% durability wear to the CRYSTAL** (the thing pried out), all tiers. Applies to **item crystals (gems) + evolution crystals** — **stat/ability stones are EXCLUDED** (no durability, pull out freely).
- Only on the **gear→inventory** path (primary removal destroys→dust, so no wear there).
- **At 0% durability the crystal BREAKS → turns into essence** (its dismantle yield). So a crystal survives ~10 removals, then auto-converts to essence — heavy swapping has a real attrition cost but isn't pure loss. Hooks into the existing durability/wear system.
- **Fusion break → HALF its components' essence.** A broken fusion yields its gem components' typed essence (ResolveEssenceType + crystal curve, like gem-break) but at **½** — a reclamation discount (you consumed crystals into the fusion; disassembly returns part of that value, not all). Socketed = direct grant, no pool-removal.


> **One consolidated future cluster — "evolution-attachment instance tier":** the slotted/primary evolution form lacks an instance `.Tier`, which blocks THREE things at once: (1) slotted-evolution loadout budgets (2c left 4 reads on asset tier), (2) combat slotted-evolution tier read (2d :460), (3) primary-slot evolution leveling. Build one cluster that adds `.Tier` (+ identity/resolver) to the primary-slot evolution form, and all three resolve together. `LevelUpEvolution` is gated on this — NOT a quick wrapper. Weapon/ring leveling (built) is independent and complete.

**⚠️ Persistence — evolution is PERMANENT, account-level.** Once attached, an evolution crystal **persists on the character in AND out of runs** — it is NOT run-volatile. It stays until manually removed. This makes evolution the most *permanent* upgrade a character can hold (justifying the premium price — you're buying a forever-upgrade).

> **Build implication:** attached-evolution state is **persistent save-state** (SaveGame-tagged, eventually account/PlayerState-level — like the persistent currencies), NOT run-scoped pawn state. The attach/detach + its persistence is its own build, tied to the save-system stage.

### 5.4 Gold — the run currency

Earned per encounter, spent in-run on **temporary buffs / consumables** only. Always lost at run end. The volatile run-spend. *(Tier-ups no longer cost Gold — they're funded by essence + Reality, §5.3.)*

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
> - **Prisms + Gold** ← AvgStatTotal × AvgKitPower (full danger)
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

**Two faucets, one number:** WSP = AvgStatTotal alone; Prisms/Gold = the full product. Stat-weak/S-geared teams pay currency but little WSP; stat-strong/F-geared is the reverse — players pick targets by which they need.

---

## 7. The roguelite loop (round table)

### 7.1 Structure

**Continuous escalating climb.** No fixed round count.

> **Round table (hub):** spend Prisms + Essence on equipment + spells (you **own** them permanently); dismantle crystals → Essence; **pick your target boss → commit** (changing target loses run progress). →
> **Run (committed):** continuous escalating encounters; draft your loadout from owned inventory (weighted by tier + Luck, element-filtered); fight as long as you live; earn per-encounter reward; spend Essence to level **what you brought** (immediate + permanent); spend Gold on temp buffs. →
> **Exit (three ways, below).** →
> **Back at the round table:** restock with banked Prisms/Essence; deeper next run.

### 7.2 The three exits (the bank rule)

| Exit            | Earnings    | Run progress |
| --------------- | ----------- | ------------ |
| **Beat boss**   | **kept**    | win          |
| **Die**         | **kept**    | reset        |
| **Leave early** | **forfeit** | reset        |

**The inversion:** dying *banks* your earnings; **leaving early forfeits them.** This is deliberate — if leaving banked your haul, the optimal play would be farm-trivial-encounters-and-quit. Forcing leave = forfeit means the only ways to keep earnings are **win** or **die trying**, so players are always pushed toward real risk. The run's tension is **commitment**, not loss of loot.

**What a death actually costs:** **World Stat Points + run progress/position + any unspent Gold** (all volatile). The persistent haul (Prisms, Essence, Essence, levels) survives death. **Gold are always lost at run end regardless of outcome** — you have them only for the run; spend them on upgrades/buffs as you go or waste them. Death = "I went as far as I could," not "I lost my permanent loot."

### 7.3 Item ownership loop

Buy an item at the hub (Prisms) → **own it permanently** → if it drafts into a run, **upgrade it there** (Essence, immediate + permanent). Acquire at the hub, deepen in the run. Owned items appear in the draft **at their leveled tier** (meta rides into the run). A bigger/leveled inventory = a richer, stronger draft pool — the reward for grinding.

### 7.4 Genre

Turn-based **roguelite** (meta-progression persists). Gear cushions low world stats — pre-leveled gear gives a floor so a fresh run isn't helpless; world stats are the per-run earn. (Gear-vs-world-stat scaling split is a tuning pass — lean ~60–70% self-tier / 30–40% world-stat.)

---

## 8. Persistent buffs — account perks (essence sink, permanent)

Permanent run-start advantages bought with Essence, surviving death. They **raise the floor**, they do not skip the climb. Lean toward *perceivable* perks (felt immediately) over invisible % boosts.

**Guard-rail:** keep floor-raisers small — especially World Stat head-start — or the run-axis (which resets on death) gets hollowed out by stacked perks.

| Perk                    | Effect                                                                              | Cap                     | Scope     |
| ----------------------- | ----------------------------------------------------------------------------------- | ----------------------- | --------- |
| **Head Start**          | begin each run with +N World Stat Points pre-allocated                              | ≤ ~half max (≤10 of 21) | permanent |
| **Starting Purse**      | begin each run with +X Gold                                                         | small                   | permanent |
| **Signature Weapon**    | one owned high-tier weapon guaranteed into the run draft                            | 1                       | permanent |
| **Signature Spell**     | one owned high-tier spell guaranteed into the run draft                             | 1                       | permanent |
| **Roll Budget**         | gear bonus-stat / resistance rolls generate from a larger budget                    | modest                  | permanent |
| **Baseline Resistance** | begin with a small flat resistance pool                                             | small                   | permanent |
| **Extra Draft**         | +1 inventory option at run-start draft                                              | 1                       | permanent |
| **Drop-Grade Shift**    | every drop rolls one grade better (curve shifts up a step; S-odds apply at A, etc.) | —                       | permanent |

**Why essence:** it's the long-tail currency — once a core loadout is maxed, account perks give players something to keep pouring essence into.

**Open:** exact essence cost per perk; per-perk caps (table is structure + guard-rails, not final tuning).

---

## 9. Open items

**Genuinely open (need a pass):**
- **Account perk costs & caps** (§8) — numbers only.
- **Equipment Prisms prices** (§5.1) — weapon/ring cost-by-tier, to add to the Prisms table.
- **Gear vs world-stat scaling split** (§7.4) — the dial making both "pre-leveling helps" and "world stats matter" true.

**PIE-tunable drafts (non-blocking):**
- Essence substitution rate — **1.5 : 1**
- Essence buy/sell spread — **2 : 1**
- Boss toughness — **×2–3**
- Reward rate — **1** (Reward = Encounter Threat)
- Boss minion-drag fix — weighted-avg vs BossToughness-on-top (leaning latter)

---

## 10. Quick reference — the loop

> **Round table:** buy equipment + spells with Prisms (+ Essence on spells), own them; dismantle crystals → Essence; commit to a target boss. **Run:** continuous escalating battles; per-encounter reward (AvgStatTotal × AvgKitPower) → World Stat Points + Prisms/Gold; spend Essence to level what you brought (immediate, permanent); Gold buy temp buffs. **Exit:** beat boss or die → keep earnings; leave early → forfeit. **Death** only costs World Stat Points + run position. Back to the table → restock → deeper.

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
| **Tier**    | **instance** (rolled on drop, raised by essence-leveling) | F→S    | core power (`TIER_POWER`), essence price, requirements, the draft                                                                                     |

- **Quality = which item this is** (authored identity). Not a roll — you build distinct assets at distinct Qualities.
- **Tier = how leveled this copy is.** Rolled at drop (§11.3), and essence-leveling raises it F→S. This is why **Tier must be per-instance.**
- A single asset can drop/exist at **any Tier**; its Quality is fixed by the asset. A C-tier plain-sword and a C-tier high-quality-sword hit similarly (same Tier) but differ by authored Quality (better rolls/effects).

### 11.3 Tier drop weights (the flat roll)

The **Tier** of a drop is rolled from this curve — **flat, regardless of enemy strength**:

| Grade       | F   | E   | D   | C   | B   | A   | S   |
| ----------- | --- | --- | --- | --- | --- | --- | --- |
| Tier drop % | 26  | 22  | 18  | 14  | 10  | 6   | 4   |

(Sums to 100. **S = 4%** ≈ the shipped "legendary" benchmark for a *tier-every-drop* system — every drop IS a tier, so the top sits near legendary's ~5%, NOT the 2% chase-rarity of games where legendaries drop *on top of* normal loot. A+S = 10%, so high-tier feels reachable.)

(Quality is **not** rolled here — it comes from the dropped asset, OR is rolled on its own weighted curve if Quality-on-instance is used; the drop-better perk + Luck shift these weights — see §11.5 / §12.3.)

**Break-yield consistency (all socketed items yield essence on break):**
- **Gems** → typed essence, crystal curve (§4.2). Direct grant (socketed, no pool-removal).
- **Evolutions** → DismantleEvolution (element essence, gear amount, at tier) + remove owned entry + free cap slot.
- **Fusions** → each gem COMPONENT yields its typed essence at **HALF** (½ × GetTypedEssenceYieldForTier per component, floored — lossy). Half-per-component makes fusion a commitment (fuse-then-break is lossy vs breaking gems individually — anti-exploit). Direct grant per component.

**Quality roll — where it happens (locked):**
- **Drops / random pickups** (`bRandomGenerateOnPickup`-gated mints) → **roll Quality** on the §11 weighted curve (F26/E22/D18/C14/B10/A6/S4), **Luck-biased** (linear tilt around C, maxed Luck stat takes S 4%→~7% — modest). Sits beside the existing stat-roll (`ApplyPickupRoll`).
- **Shop items** → **rolled at shelf-population** (the shop stocks pre-rolled tier+quality items, rerollable per §11.5). **Purchase carries the shelf-rolled quality through** — NO roll at point-of-sale, NOT a fixed grade. *(DEFERRED: gated on the loot/shop generator, which doesn't exist yet. Until built, purchases get a C placeholder.)*
- **Authored starting gear / non-rolled** → fixed **C** placeholder.
- **Re-hydration / re-equip** → never re-rolls (the factory keeps the C-placeholder; rolls only at the GUID-minting acquisition point).

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

Each **row's Tier distribution is identical** (the Tier roll is flat, enemy-independent). *(The matrix above uses illustrative weights; the LIVE Tier weights are §11.3's 26/22/18/14/10/6/4.)* The variety between drops comes from **which-Quality asset the enemy carried**, not from a Quality roll. A high-Quality enemy item dropping at a high rolled Tier is the jackpot; both factors are independent of enemy strength (Quality = what they had; Tier = flat luck).

### 11.5 Modifiers & guards

- **Tier roll is FLAT — enemy strength does NOT bias it.** (Supersedes the earlier "encounter threat shifts Tier weights" line.) A weak enemy and a boss have identical per-item Tier odds; reward scales by **what's in their kit** (higher-Quality items, more items) and **how much drops**, not better Tier luck.
- **Luck** can still bend the Tier curve up (a player-side run/perk modifier on the drop roll, §12.3) — that's player-earned, not enemy-derived.
- **Pity floor** — guarantee a high-Tier roll within ~N drops so the curve doesn't flatline on a streak.
- **S Tier dial** — 2% is the rarest roll; raise it (S = 3%) if too rare.

### 11.6 Deferred — unique-effect layer

A future "unique" rarity (PoE-style: fixed *special effect*, build-defining, not raw-stat king) is the home for the parked "legendary-only unique effect." Not needed to ship; bolt on later if playtesting wants a build-defining chase beyond rolls.

### 11.7 Does Quality affect cost?

**It may — Quality is authored & visible, so pricing on it is fair** (unlike the old hidden-roll model). Buy price stays primarily **Tier-keyed** (§4–5: identity essence + Σ stat essence + Prisms floor). Quality, being a known authored grade, can layer a multiplier on top (a higher-Quality asset costs more — you can see why before buying). Open number: the Quality price multiplier, if any. Tier remains the dominant price axis; Quality is a known premium, not hidden luck.


---

## 12. Shop & reroll (the round table)

**Status:** Design locked; numbers (reroll cost, cap N) PIE-tunable. The shop reuses the loot generator — no second system.

### 12.1 Shop = a per-run rolled drop table

Each run, the round-table shop **rolls fresh stock** from the same generator as combat loot (§11): it surfaces assets (each carrying its authored **Quality**) and rolls a **Tier** per item. You buy a **rolled instance** off the shelf — same as if it dropped. This is the PoE/Diablo vendor model: refreshing rolled stock, one generator powering both drops and shop.

- Stock differs every run (items, Tiers, hidden Qualities all re-rolled).
- Buy price is still **Tier-keyed** (§4–5: essence + Prisms floor). Quality rides along free — you might buy a C item that secretly rolled A-quality.

### 12.2 Reroll

| Reroll           | Cost                               | Effect                                                       |
| ---------------- | ---------------------------------- | ------------------------------------------------------------ |
| **Stock reroll** | Reality essence + Prisms (cheaper) | re-rolls the whole shop selection                            |
| **Item reroll**  | Reality essence + Prisms (dearer)  | re-rolls one item's **Tier** (Quality is fixed by the asset) |

- **Currency: Reality essence + Prisms** — rerolls cost both: Reality essence (the roll/reshape currency, §14) plus Prisms (the hub buy-currency). The round table is the hub, so rerolls draw persistent currency, not run Gold.
- **Guard: per-run cap** — N rerolls per run, then locked. Flat, legible ("3 rerolls left"). Preferred over escalating cost.
- The cap doubles as a **perk hook** — a persistent buff (§8) can raise it (+1 reroll/run).

### 12.3 Luck + "better drop" perk = one curve-shift, two sources

Both inputs do the **same thing**: shift the §11.3 **Tier** weight curve toward high grades (better Tier odds), for combat drops **and** shop rolls. (Quality is authored on the asset, not rolled, so Luck/perks bias **Tier only**.)

| Source                              | Scope                                                    |
| ----------------------------------- | -------------------------------------------------------- |
| **Luck** (`ESubStat::Luck`)         | run-scoped — built this run via World Stat Points / gear |
| **"Better drop" account perk** (§8) | permanent — essence-bought                               |

One mechanic (curve-shift), two sources (temporary Luck, permanent perk) — same "one curve, multiple sources" discipline as essence/essence. Mirrors PoE's Increased-Item-Rarity stat fed by gear/perks.

### 12.4 Open numbers (PIE-tunable)

- Stock-reroll cost · item-reroll cost (dearer) — both in Reality essence + Prisms
- Per-run reroll cap **N** (and the perk that raises it)
- Luck → curve-shift magnitude (how much Luck bends the weights)


---

## 13. Authored vs runtime — the asset/component split

**Status:** Locked classification. Determines what gets a runtime home vs stays on the asset. Getting this right once is cheaper than retrofitting.

**The rule:** anything a *player* mutates at runtime, while the authored baseline must survive as a template (for enemies, for player-built Lords-as-enemies, for death-reset), needs the split — **asset = authored baseline, component/instance = runtime copy seeded from it.** Things that are authored-and-fixed stay asset-only. Pure-runtime resources need no asset baseline.

| System                                       | Lives where                                            | Why                                                                                                  |
| -------------------------------------------- | ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| **Sub-stats (13 DNA)**                       | **asset only**                                         | player authors at character creation, then fixed — character identity, never mutated at runtime      |
| **World stat levels** (Mind/Body/Spirit)     | asset baseline → **`UCharacterDataComponent` runtime** | run-allocated/rolled; resets on death. Asset holds authored start; component holds live value        |
| **Tier**                                     | asset baseline → **item instance runtime**             | loot rolls it; essence raises it. Asset Tier = authored floor                                        |
| **Quality**                                  | **asset only**                                         | authored item identity, never mutated. NOT split (do not build as instance state by analogy to Tier) |
| **Currencies** (Gold/Prisms/Essence/Essence) | **`UCurrencyComponent`**                               | pure runtime resource, no asset baseline                                                             |
| **HP/EP, alive, BD flag**                    | already split (`CurrentHP` on component)               | the template for this whole pattern                                                                  |
| **Items/weapons/rings/crystals**             | already per-instance (`FWeaponInventoryEntry` etc.)    | the instance layer's purpose                                                                         |
| **Bonus-stat / resistance rolls**            | already per-instance (`StatBonus` on entry)            | reroll rework must target the INSTANCE copy, not the asset layer                                     |
| **Durability / wear**                        | per-instance (inert today)                             | when built: asset authors max, instance tracks current — same split                                  |

**Only two NEW splits:** world-stat levels (→ character component) and Tier (→ item instance). Everything else is already split, asset-only, or pure-runtime. No sprawling "make everything runtime" migration.

**Enemy authoring is unchanged:** enemies are authored on the asset exactly as today (a "Fire Lord" asset with WorldLevels 5/5/6, sub-stat DNA, kit). The component seeds from the asset at spawn; since enemies don't allocate/roll, their runtime copy stays at the authored value. Only players diverge, only at runtime. This is also what makes player-built Lords-as-enemies work — the asset baseline IS the snapshot faced.

---

## 13.5 Reality revert — un-leveling as Reality's signature [DEFERRED — design locked]

**Concept:** Reality is the reshape/meta element (rolls, wildcard, ½-cost on every level-up). **Reverting / un-leveling gear is Reality's thematic inverse** — Reality un-shapes power. One mechanic, several faces, all owned by Reality.

**Leveling is OPT-IN (the meta-progression escape valve):** players who dislike the power growth simply don't upgrade, or vary their spells. Revert extends this — it makes leveling *reversible*, not a permanent commitment (the Griftlands "optional + non-intrusive" pattern the discourse praises).

**Three enemy types (context for Face A):**
1. **Actual AI** — dev-authored enemies (not players).
2. **Players** — live PvP (real player characters).
3. **AI players** — AI MIMICS of real players' characters (Lord/Contender system — a copy/snapshot that behaves like the player's build).

### Face A — offensive de-level (combat)
A Reality ability that **reverts/de-levels a target's gear**. Behaviour by target type (the key asymmetry that makes permanence safe):
- **vs AI players (mimics):** **PERMANENT** — it's a copy, not the real person. You permanently weaken the mimic you face; the real player's Lord is untouched. Safe permanence = real reward, no griefing.
- **vs actual players (live PvP):** **PERMANENT IF IT CONNECTS** — same permanent de-level as a mimic, but gated behind counterplay so it's never a cheap grief: a **long telegraph (~5-turn windup before it executes)** + **cleanseable** (target can remove it during the windup). If they fail to cleanse in time → it lands → **permanent**. Real, permanent PvP stakes, but fully telegraphed + answerable ("you had 5 turns + a cleanse to stop this"). The telegraph/cleanse is the COUNTERPLAY WINDOW, not a permanence-downgrade — high stakes, fair warning, eat it if you don't respond.
- **vs actual AI:** applies as the combat effect (no persistent-character grief concern).

### Face B — inverse-tier weapons (a build archetype, not just an ability)
A **weapon archetype that gets STRONGER the LOWER its tier.** Inverts the standard curve (normal: higher tier = stronger; these: lower tier = stronger). A player using one *wants* to revert it — Reality powers it *down* to power it *up*. Reality's revert becomes CONSTRUCTIVE for this build (a core power loop, not just respec/counter). Thematically perfect (Reality rewards un-shaping). *(Lives in the weapon/item design space — an inverse-tier weapon archetype enabled by the revert mechanic.)*

### Face C — hub respec (out of combat) [BUILDABLE NOW]
De-level your own gear in the hub → **partial essence refund.** Refund rule (LOCKED):
- **½ the step cost back** (revert B→C refunds ½ the Gear/Skill essence that the C→B level-up cost — matches the ½-dismantle / ½-cumulative conventions, round down).
- **Main essence only — the ½-Reality is NOT refunded** (spent/gone). Reality being non-refundable makes reverting a REAL cost: you eat the reshape currency every re-shape, so level/revert is never a free economy-gaming loop. Consistent with Reality as the connective sink.

The meta-progression respec valve: over-invested, or one loadout dominant? Revert it, reclaim ½ the main essence, rebalance. **Build shape:** `UEconomyService::DowngradeWeapon/Ring/Spell/Ability` — the inverse of LevelUp*, reusing a shared revert core (lower instance .Tier one step, refund ½ step cost in the leveling essence type; no Reality refund). The matching pair to the level-up system.

**Status: DESIGN LOCKED, DEFERRED.** A meaty mechanic (combat ability + weapon archetype + hub action + the per-target-type permanence rules + telegraph/cleanse). Build after the pool arc. Strengthens Reality's identity as the meta/reshape element across BOTH economy (level cost, rolls) AND combat (de-leveling).

## 14. Rolling stats — Reality essence as the roll currency

**Status:** Design locked. Build slots at the spend/reroll layer (step 7), reworks the resistance generator + adds a cost gate.

**Three separate roll surfaces** (NOT unified into one roll): **roll world stats · roll stats · roll resistance.** Each its own action; they share the cost model below but stay distinct.

### 14.1 Reality essence = the roll currency

**Reality essence + Prisms are the currency for stat rolls.** Reality essence is the thematic core — Reality is the meta/wildcard element (already the wildcard substitute, §4.1), so "spending Reality to reshape your stats" reads as lore. Prisms (the hub buy-currency) is paid alongside it as the gold cost. Rolling/rerolling world stats, bonus stats, and resistance all spend **Reality essence + Prisms**.

### 14.2 Cost matches the scaling-tier cost

Roll cost rides the **same tier curve as everything else** (`TIER_POWER`): rolling at a higher tier costs proportionally more Reality essence, because a higher-tier roll is worth more (tier sets per-point VALUE — §3 gear model). Cost = value, one curve.

- **Bonus-stat / resistance roll** — cost scales by the **item's Tier** (`TIER_POWER(itemTier)` × base Reality-essence cost). An S-item roll costs ~4.8× an F-item roll.
- **World-stat roll** — cost scales by the **World Point level** being rolled at (same curve shape).

### 14.3 Resistance generator reworked to match the stats model

Currently resistance uses a **per-tier budget** (`GetResistanceBudget(Tier)` — "various pool values"); stats use a **flat budget** with tier scaling per-point VALUE. **Rework resistance to mirror the stats model:** flat budget + tier-scales-per-point-value (the same change the tier-power arc made to stats, which resistance never received). Keeps the three rolls *consistent in model* while staying *separate as actions*.

### 14.4 Interface: resource-gated, not points-allocated

Move from "here's N points, distribute" to "pay Reality essence → get a rolled result → pay to reroll." **Keep the internal zero-sum budget as the balance guard** (a roll can't go all-max) — only the *interface* changes from manual point-allocation to resource-cost-gated rolling.

**Open numbers (PIE-tunable):** base Reality-essence cost per roll (before the TIER_POWER multiplier); whether a per-run roll cap applies (mirrors §12.2 reroll cap).


---

## 15. Diamond — premium currency

**Status:** Design locked. Field built with the wallet (§1 / `UCurrencyComponent`); trading faucet deferred. Modelled on Warframe Platinum.

**What it is:** the **premium currency**, on a different plane from the gameplay economy. Sits beside the other five in the wallet but is sourced and spent entirely separately.

|                       | Diamond                                                     | Gameplay currencies (Gold / Prisms / Essence / Essence / WSP) |
| --------------------- | ----------------------------------------------------------- | ------------------------------------------------------------- |
| Source                | **real money** (+ in-game **trade**, deferred)              | gameplay                                                      |
| Buys                  | cosmetics, bundles, account slots, **grind-speed boosters** | power, items, rolls, progression                              |
| Touches combat power? | **NEVER**                                                   | yes                                                           |
| Lives on              | `UCurrencyComponent` (a field)                              | same component                                                |

### 15.1 The hard rule — Diamond never buys power

The single non-negotiable: **a hard wall between Diamond and the power economy.** Diamond buys cosmetics, bundles, account/loadout slots, renames, and convenience — **never** Tier, Quality, Essence, Essence, stat rolls, or anything that wins a fight. (Warframe's discipline: the strongest items can't be bought with Platinum; the only way to get power is to earn it. This is what keeps the loot/roguelite chase meaningful and the game non-pay-to-win.)

### 15.2 Boosters — grind-speed only

Convenience boosters are allowed, held to the Warframe line:
- **Allowed:** speed up *earning* you'd do anyway — e.g. 2× Essence gain, 2× Prisms, 2× resource drop *rate*. Raises throughput, not ceilings.
- **Forbidden:** anything touching power or loot *quality* — e.g. better drop odds, higher Tier rolls, fatter Quality. These cross into power and break the chase.

The test: does the booster let you reach something you *couldn't* otherwise, or just reach it *faster*? Faster = OK; couldn't-otherwise = forbidden.

### 15.3 Trade faucet (deferred)

Ideally Diamond is **earnable in-game via player trade** (Warframe model): grind valuable items, sell to paying players for Diamond; spending it sinks it from the economy, real money is the only way to add more. This makes Diamond a gameplay sink, not just a paywall, and lets free players earn it with time. **Deferred** — needs player-to-player trading infra that doesn't exist yet. Build the currency field now knowing trade is the eventual faucet.

### 15.4 Now six currencies

Wallet total: **Gold, Prisms, Essence (14), Essence, World Stat Points, Diamond.** Diamond is the only one isolated from the power economy.


---

## 16. Ownership & multiplayer-readiness

**Status:** Locked (per MP-readiness survey, 2026-06). The codebase is single-player/PIE-local today with clean asset/runtime separation — this is a tagging-and-ownership problem, not a structural one. Author the resource build replication-aware NOW (near-zero cost); do NOT block on a full RPC/networking implementation.

### 16.1 Ownership model — PlayerState, per-character profiles, hybrid sharing

Per-player persistent state lives on **PlayerState** (the UE-idiomatic home; survives the disposable run-body pawn). Run-transient state stays on the **pawn/character component**. This maps directly onto the two-axis death rule: the pawn IS the run (resets), the PlayerState IS the account (persists).

```
PlayerState (account)
 └─ Characters[] : TArray<FCharacterProfile>
     ├─ Char A → { gameplay currencies, owned inventory, levels, CharacterId }
     ├─ Char B → { ... }
 └─ Account-wide: Diamond, account perks (§8)
```

| State                                                     | Scope                                                                          | Lives on                                       |
| --------------------------------------------------------- | ------------------------------------------------------------------------------ | ---------------------------------------------- |
| Prisms, Essence (14), Essence                             | **per-character**                                                              | PlayerState → character profile                |
| Owned inventory (weapons/rings/spells/abilities/crystals) | **per-character** (items gain a `CharacterId` owner + existing `PersistentID`) | PlayerState → character profile                |
| **Diamond**                                               | **account-wide** (paid once, all characters)                                   | PlayerState (account)                          |
| **Account perks** (§8)                                    | **account-wide** (unlock once, applies to all)                                 | PlayerState (account)                          |
| Gold                                                      | run-volatile                                                                   | pawn/run context                               |
| **World Stat Points** (run-earned)                        | run-volatile, character state                                                  | **pawn/character component** (resets with run) |
| CurrentHP/EP, alive, active loadout body                  | run-transient                                                                  | **pawn/character component**                   |

`UCurrencyComponent` is therefore **the active character's wallet**, drawn from the per-character persistent store on PlayerState — not one flat global wallet.

### 16.2 The four author-now hygiene rules (cheap now, expensive retrofit)

These cost near-nothing at authoring time and avoid a painful rewrite. None require the RPC layer to exist yet.

1. **FastArray over TMap for replicable counts.** `TMap<Enum,int32>` cannot replicate natively. Essence (14) + Essence are exactly that shape — author as `TArray<FCurrencyEntry>` (enum + count) with a `FastArraySerializer` from the start. (Also the fix for the existing `CrystalInventoryComponent` TMap blocker when touched.)
2. **Every resource mutation behind a server-authority check.** Grant/spend gated even though no RPC transport exists yet — the gate is a harmless no-op in PIE (HasServerAuthority() returns true in NM_Standalone) and the correct shape for later. Gate the GRANT, not the delegate subscription.
3. **Dual-tag SaveGame + Replicated** on persistent-and-shared fields (currencies, instance Tier, Quality). Only `bIsBrokenDarkness` does this today — establish the pattern deliberately. NOTE: no `USaveGame` subclass exists yet, so the persist half is also unbuilt (a real build item, not wiring).
4. **Runtime world-stats off the asset.** Writing run-allocated World levels onto the shared `UCharacterData` template is both an MP break and an aliasing bug. They live on the runtime character component (§13), seeded from the asset baseline.

### 16.3 What NOT to build yet (follows incrementally)

The RPC transport layer, authority-gating the combat orchestrator/subsystems, and BD-manager replication are NOT prerequisites for the resource build — they follow once the authority model is implemented. Authoring replication-aware *shapes* now (16.2) is what prevents the corner-paint.

### 16.4 Known MP gaps flagged by the survey (not resource-blocking)

- **No RPC layer exists** — zero Server/Client/Multicast UFUNCTIONs. Combat is client-local; `Server*` functions on CharacterDataComponent are plain C++ self-gated by HasServerAuthority (no-op in PIE).
- **Combat orchestration is ungated** (CombatOrchestrator/TurnManager/ActionExecutor run wherever invoked). Two combat subsystems are `UGameInstanceSubsystem` (don't replicate) — server-authoritative combat will eventually need them server-owned or moved to a replicated actor/GameState.
- **Combat delegates broadcast ungated** → under MP with client-side execution they fire per-client → earn-layer would double-grant. Every grant must sit behind server authority (16.2 rule 2). Defense events (parry/perfect-defend) are real-time *local* input → need server reconciliation, not just a gate.
- **`CrystalInventoryComponent` + `UInventoryData`** key crystal counts as `TMap<FCrystalId,int32>` — same FastArray treatment needed (16.2 rule 1).