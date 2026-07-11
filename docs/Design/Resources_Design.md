# World of Refraction — Resource Economy Design

**Status:** Design locked; most numbers locked (PIE-tunable drafts flagged). Nothing built. Locked-decision reference, not an implementation spec.

---

### Global convention — always round DOWN

Any time a calculation produces a decimal (essence yields, prices, fractions, wear, fusion-break halves, etc.) → **round DOWN (floor)**. Never round up or to nearest. Rationale: flooring never over-rewards — it always favors the sink, never gives the player free value from rounding. Applies project-wide (`FMath::FloorToInt`). Examples already using it: fusion-break ½-yield, F-quality price half (13 = floor of 12.5).

### ⚠️ SUPERSEDED — collapsed to ONE hub (Nightreign model, 2026-06-25)

**Decision:** the two-hub split (local hub + trial/run hub) is **collapsed to a SINGLE hub.** Validated against Elden Ring Nightreign's Roundtable Hold — the proven model for a co-op roguelite.

**The model:**
- **ONE hub = the between-runs space** (all prep: shop, upgrade, repair, merge, roll, respec, head-start allocation, character select, draft). Everything "outside battle" happens here.
- **Multiplayer matchmakes at TRIAL LAUNCH, not in the hub.** Per Nightreign: players are SOLO in the hub even when online; matchmaking happens when you commence the trial → drop into the shared run together. **The TRIAL is the shared multiplayer space, the hub is solo prep.** This dissolves the original "two hubs for multiplayer" reasoning — you don't need a shared hub; the run IS the shared space.
- **Boundary:** **hub = outside battle** (prep/shop/respec) · **trial = the run** (5 battles + named boss; Gold economy, World-Stat earn, in-run power).

**What this simplifies (overrides the two-hub text below):**
- No local-vs-run hub distinction — ONE hub does all services.
- **Currency:** persistent currencies (Prisms/essence/Reality) spend at the HUB; Gold is the in-TRIAL currency (earned + spent during the run). No "currency-follows-location" split (it collapses — location is just hub-vs-trial).
- The "currency follows the action" principle stays (services carry their own cost), but the hub-vs-hub routing is gone.
- Item shop-assignment flags (sold-in-main/sold-in-run) collapse to hub-stock vs trial-vendor-stock.

**The two-hub text below is RETAINED for reference (the economy reasoning — Gold-vs-Prisms, the service costs, the draft/return valve — all still holds) but READ IT AS ONE HUB: wherever it says "local hub" or "run hub," that's now the single hub (prep) vs the trial (the run). The Gold=in-trial / Prisms=at-hub split is the surviving core.**

---

### Two-hub economy — Main hub (Prisms) vs Run hub (Gold) [DEFERRED — shapes the Pool arc]

Two distinct hubs with different roles, currencies, and what they operate on:

|             | **Main hub** (persistent)                                       | **Run hub** (in-run)                                          |
| ----------- | --------------------------------------------------------------- | ------------------------------------------------------------- |
| Currency    | **Prisms**                                                      | **Gold**                                                      |
| Shops       | local shop (purchase, Prisms)                                   | run shop (purchase, Gold)                                     |
| Services    | downgrade (respec)                                              | upgrade, repair, merge, roll/reroll, downgrade, draft, return |
| NPCs        | Blacksmith/Jeweler/Spiritualist (upgrade + maintain owned gear) | —                                                             |
| Operates on | owned pool                                                      | run inventory ↔ pool                                          |
| When        | between runs                                                    | during a run                                                  |

**Run hub flow (the in-run loop):**
1. **Draft** — offered ~3 inventories drawn from your own pool → **pick 1** → build your loadout from it. The draft is a curated SLICE, not your whole pool (you may own 100 of everything; a run takes ~10 — "take 10, get to the end"). Gives enough for a full loadout, but loadout SLOTS constrain what you keep:
   - **Base loadout slots → max (account progression via §8 perks):** the 3 drafted loadouts each FILL these slots; the slot COUNT grows by spending essence on §8 account perks (account-wide — unlock once, applies to all characters). Base→max:

     | GEAR slot  | Base | Max |
     | ---------- | ---- | --- |
     | Weapons    | 1    | 5   |
     | Rings      | 2    | 5   |
     | Evolutions | 1    | 5   |

     ⚠️ **Account perks grow the GEAR COUNT only** (how many weapons/rings/evolutions you start a run with). They do NOT change skill caps.

     **Skill caps are SEPARATE + ALREADY BUILT (not account-perk-driven):** how many spells/abilities each gear piece holds is the existing TIER-GATED slot system — **6 per gear piece** (F=1…A/S=6, SlotsForContainerTier), **up to 12 abilities** on an ability-crystal'd weapon, **24 spells** total for a Caster (MAX_SPELL_SLOTS/MAX_RING_SPELLS = 6 each). These are EQUIP caps on gear, untouched by the draft.

     **The mechanic:** at run-start, **3 loadouts spawn** (the draft), each a **slice of your owned (account-wide) pool** — what you own shapes what can be drafted. You pick 1. Each loadout fills your starting GEAR slots (small early — 1 weapon/2 rings/1 evo — growing to 5/5/5 as you unlock §8 slot perks). The skills on that gear use the existing per-piece caps. **In-run pickups are usable THAT run but don't carry** — next run you draft fresh from base slots (roguelite reset; account progression = more GEAR options, NOT carried power — the healthy axis).

     **In-run skill growth (NEW — the within-run progression):** you START a run with **3 abilities + 3 spells** and **upgrade up to 12 each WITHIN the run** — the run itself is where your skill kit grows (the roguelite "start lean, get stronger" beat). The 12/12 are the run-MAX (not per-gear equip caps — those stay the existing tier-gated 6-per-piece). You work to gain skills as the run progresses.

**Draft generation uses the existing DROP-CHANCE weighting:** when the 3 loadouts spawn (pool slices), WHICH items surface is weighted by the existing Luck-biased drop/quality curve (F/E/D/C/B/A/S, §11) — rarer items appear less often, common more often. Reuses the built drop-weighting; no new roll system.

**Auto-dedupe — keep the stronger, BANKED AT RUN-END (the earlier deferred idea, now locked):** if you find something you ALREADY OWN, the **STRONGER copy goes into your run inventory** (you use it the rest of the run). The **permanent keep happens ONLY at run-END** — completing the run banks the stronger into your pool (weaker → essence); **dying/failing the run loses the found upgrade** (you keep your original pool copy). So: in-run = use the stronger immediately; permanent = a REWARD FOR FINISHING. Consistent with the death rule (§7 — run progress is volatile; the persistent haul survives, but in-run FINDS bank only on completion). Risk/reward: found great gear is yours to USE all run, but yours to KEEP only if you finish.

### Trial = the run (narrative + structure)

**The frame:** characters are trying to **get their memories back**; **trials** are how — fight through enemy encounters, succeed, recover memories. A **trial = a run = a sequence of enemy encounters** (the combat loop). Structure (fixed count / variable / escalating) **DEFERRED** — concept locked, shape TBD.

**Hubs map to this:** **village** = general/local hub (home base, between trials) · **trial hub** = the run hub (prep/draft/workshop for a trial).

**Succeed → keep what you earned in the trial** (the finish-to-bank rule). Death/fail → forfeit the at-risk gains.

**Partial bank via a VENDOR (mid-trial risk management):** instead of blanket all-or-nothing, a **mid-trial vendor lets you RETURN a few items** — banked safely even if you later die. The number you can return per trial **starts at 1, max 5** (account-perk-scaled, like gear slots). So partial-banking is PLAYER-CONTROLLED: "push deeper, or stop and lock in these items?" (This is the return-to-pool flow, available mid-trial, capped per trial.)

**World Stats — in-trial, run-scoped, passive-scaled start:** World Stat Points are **earned in-trial** (defeating enemies / completing the trial) and **spent to build THIS character's stats DURING the run** — run-scoped (reset each trial; consistent with the existing run-volatile WSP rule). **Passives raise the STARTING amount** (thinking ~4 max to start) — account progression lets you begin a trial with more WSP. So WSP is the in-trial character-building currency: earn fighting → spend to power up this trial → reset next trial; passives set the floor.

### World Stats — LOCKED MODEL (2026-06-25)

The full layered model (refines the paragraph above):

**1. Base 0/0/0 every run.** Mind/Body/Spirit start at ZERO at run start — not the authored asset value. The authored CharacterData stats become the asset baseline only; the live run value starts at 0/0/0.

**2. Persistent PER-CHARACTER head-start (≤4 points, allocatable).** A passive/unlock grants up to ~4 head-start points the player allocates across Mind/Body/Spirit. **PER-CHARACTER persistent (NOT account-wide)** — each character saves its OWN head-start allocation; it sticks across that character's runs.
- **Allocate at your convenience:** set the points whenever (hub/menu) OR in-run.
- **In-run allocation self-saves:** if you enter a run without pre-allocating, however you distribute the points DURING the run becomes your saved persistent allocation. No dead-end — the latest allocation (hub or in-run) persists per-character.
- ⚠️ **This is the ONE World-Stat piece needing SAVE** (per-character persistence — gated on the save system, audit-flagged ❌). Build the field/allocation now (session-volatile until save lands), persist later.

**3. In-run EARN (on top of head-start), run-scoped:**
- **Earn from enemies — PER-KILL pool, spent via a 5-PICK-3 DRAFT at COMBAT END (locked):**
  - **Per-kill: feed a pool (no interruption).** Each enemy death adds caliber-scaled WSP to a running "earned this encounter" pool. Caliber = the victim's `GetTotalPool()` (stat-total, ~30-93 range) / a divisor → ~1-3 WSP/kill (divisor = tuning). NO mid-fight screens — kills just grow the pool.
  - **Team-wide grant** (no killer attribution exists — `OnDied` carries victim only, `ServerTakeDamage` has no instigator; threading one through ~18 sites is a deferred enhancement). The winning team's pool fills; killer-specific is a later upgrade.
  - **At COMBAT END: a 5-options-pick-3 DRAFT.** The accumulated pool drives a draft — player is offered **5 stat options, picks 3** to apply (Hades/VS-style level-up pick). Bigger pool = better/more options (exact mapping = tuning). Fires once per encounter at combat end.
  - ⚠️ **The DRAFT UI is DEFERRED** (UI layer). The BACKEND (pool accumulation per-kill + the combat-end "draft ready" event + the `AddEarnedWorldStat` grant op) is buildable now as SCAFFOLDING — the pick-screen binds the event later. No wave-counter needed (caliber self-balances); wave-scaling layers on with trial-structure.
- **Buy from run vendors — ESCALATING COST (the proven Slay-the-Spire model, web-validated):** a World Stat point is ALWAYS buyable at ANY run vendor; **each purchase raises the price of the next** (`base + N×bought`). The rising cost IS the throttle — no roll-chance, no random appearance (research: random gating frustrates players for a CORE progression stat; deterministic escalation is plannable). Spends Gold (run currency). **The escalation RESETS each run** (fresh ramp every trial — fits run-scoped stats). Per-pillar or shared ramp = a tuning call.
- Both stack onto the head-start; both RESET each run.

**4. Run-scoped reset.** Earned + bought stats wipe each run; only the per-character head-start allocation persists. Live value = `0 + head-start(persistent) + earned/bought(run-scoped)`.

**5. Resettable.** The head-start allocation can be respec'd (reset → re-allocate the ≤4).

**Storage (build shape):** live World Stats on `UCharacterDataComponent` runtime (line ~911 — already the designated runtime home). The head-start allocation is a per-character persistent field (SaveGame-tagged, dormant until save). In-run earned is run-scoped (needs the run-state container OR a runtime component field until run-state lands). The enemy-drop earn hooks `OnCombatResultReady` (fires already, no consumer — audit 🟡); the vendor-buy is an economy op (run currency → stat increment).

**Earn model — REFINED (2026-06-25):**
- **Per-ENEMY** (each kill drops World Stat Points), NOT per-encounter. Hook the per-actor death signal (`OnDied`/per-actor), not `OnCombatResultReady` (which is per-win).
- **Scaled by enemy CALIBER** — a defeated enemy's stat-total (the `AvgStatTotal` / total-stat-points expression, CharacterData.h:315) determines the drop: tougher foe = more points. This is the §7.4 "earn from the caliber of foe" model, and it works NOW (caliber is an enemy property — no wave-counter needed).
- **Wave-scaling DEFERRED** — "harder each wave gives more" multiplies on top of caliber, but needs the trial/encounter-sequence (UNBUILT). Add the wave multiplier when trial-structure lands; caliber-scaling ships now and stands alone.

**Skill gating — CLARIFIED:** world stats are **IDEAL requirements (advisory), they do NOT gate skills** (already implemented this way). So the skill-requirement reader does NOT need repointing to live — it's not gating anything. The reader-repoint (C2) is ONLY the combat-SCALING readers (damage/defense scaling, turn speed, spell-slot discount, weather) — the mechanical consumers, not the advisory requirement check.

**Build-order note (per foundations audit):** the EARN/SPEND/reset (enemy drop + vendor buy + allocate + reset) is buildable NOW as runtime ops on a component — it does NOT need save (only the head-start PERSISTENCE does, which degrades gracefully to session-volatile until save lands). So this is buildable on existing foundations; the persistence half wires in with the save system later.

**Class shapes what's USABLE from a draft — off-class gear RETURNS for Gold (kept, not scrapped):** Generic uses TWO weapons; other classes (Caster/Resonator) may use no/fewer weapons (Resonator is ring-based). So a class won't use everything it drafts. Off-class drafted gear (e.g. a Resonator's weapon) → **RETURN to the account-wide pool for Gold** — the item is KEPT (a Generic can use it later; the pool is shared), NOT destroyed. **No essence from return.** You only get essence by deliberately DISMANTLING (which destroys the item). So off-class gear is never waste — return it (Gold now, kept for another character), don't scrap it. Reinforces the faucet split: RETURN = pool + Gold (item kept) · DISMANTLE = essence (item destroyed) · BREAK = pool + essence (forced salvage).

**Skill slotting + special-gear override (already built):** you fill a gear piece's skill slots with abilities/spells YOU OWN ("I have a weapon + abilities, let me use them together"). Special gear may come with skills PRE-ATTACHED — but it's TAKE-IT-OR-LEAVE-IT: you can use the attached skills OR replace them with your own (the existing AssignedSpells/AssignedAbilities sequential override). Not forced.
   - Surplus beyond your slots → **returned for Gold** (that's the "why return" — you're given more than fits).
   - **Themed inventories — NARRATIVE memory-recovery (designed):** a themed inventory is a **specific character's actual kit** — "this is the loadout [character] used." Ties to the core narrative (recovering memories): assembling a character's kit = **a recovered memory made playable**. The theme can be anything (element/playstyle/concept) but the FRAMING is narrative.
     - **Whose kits:** a MIX — **legendary figures, NPCs, and your own past selves**. Wide narrative net (lore legends to embody, world NPCs, recovered memories of who you were).
     - **Requirement = own the EXACT specific items** that character used (a collection/discovery hunt — find the precise gear to reconstruct their kit). Exact, not category — precise + collectible ("found the last piece of [Legend]'s loadout").
     - **Payoff = NARRATIVE UNLOCK** — assembling a kit recovers the memory (a story/lore beat). Reward is narrative, not (just) power — fits the memory-recovery core.
     - **Definitions = `UThemedInventoryData` asset per character** (their exact gear list + lore beat; the character can set loadouts in it).
     - **⭐ PLAYER-CREATED PRESETS (also draftable):** players save their own preset loadouts, which ALSO have a chance to appear as draft options (only if you OWN the preset's items — same ownership rule as authored themes). Makes the themed-draft pool INFINITE + personal (your favorite builds resurface), not just finite hand-designed themes.
     - **The 3-way draft option model:** each draft slot can be (a) a RANDOM pool slice, (b) an AUTHORED themed inventory (legend/NPC/past-self, if you own the exact kit), or (c) a PLAYER PRESET (your saved build, if you own its items).
     - **Chance model (anti-dilution — VALIDATED against roguelite discourse):** START = a chance that ONE themed/preset loadout appears among the 3 options per run. **Persistent effects INCREASE it** (the chance AND eventually the NUMBER of themed slots — a maxed account sees 2-3 themed options). ⚠️ The increase is the **ANTI-DILUTION mechanism**: as your eligible-theme pool GROWS (more kits assembled, more presets saved), the per-theme chance would otherwise SHRINK ("pool dilution" — the documented roguelite failure where unlocking more makes finding any one thing rarer). The persistent increase keeps themes FINDABLE as the pool grows. **Weighting:** which eligible theme fills the slot is weighted — boost a RECENTLY-COMPLETED kit so a freshly-assembled memory is likely to show up soon (the "I just finished [Legend]'s kit → now I get to play it!" payoff). *(Theme definitions + content = a design+content task; the SYSTEM is designed here.)*
2. **Build** — keep what you'll use this run.
3. **Return** — surplus → back to pool (you still OWN it — "I'll use it a different run") → **Gold**. NOT a loss; you keep the unlock, get Gold now.
4. **Shop** — buy gear/skills for the run with Gold (run shop).

**Hub composition — services + shops (LOCKED):**

| Service / Shop               | Local hub | Run hub | Cost                                                                                                                    |
| ---------------------------- | --------- | ------- | ----------------------------------------------------------------------------------------------------------------------- |
| **Purchase**                 | ✅ Prisms  | ✅ Gold  | currency differs BY HUB; stock by per-item flag (main / run / both)                                                     |
| **Upgrade** (item tier-up)   | —         | ✅       | upgrade essence + ½ Reality (NOT Gold) — build power *during* the run                                                   |
| **Downgrade** (respec)       | ✅         | ✅       | partial essence refund (½ step, no Reality)                                                                             |
| **Repair** (fix broken gear) | ✅         | ✅       | **flat by tier** (F cheap → S more); cheap upkeep. **Gold at run hub, Prisms at local hub** (currency-follows-location) |
| **Roll / Reroll** (stats)    | ✅         | ✅       | Reality + Prisms                                                                                                        |
| **Merge** (crystals)         | ✅         | ✅       | Prisms                                                                                                                  |
| **Draft / Return**           | —         | ✅       | Gold (return surplus → pool + Gold)                                                                                     |

**The rule:** the HUB determines **purchase** currency (Prisms local / Gold run); **services carry their own designed cost regardless of hub** (upgrade=essence anywhere, roll=Reality+Prisms anywhere, merge=Prisms anywhere). So Prisms IS spendable in the run hub — just for services (roll/merge), not for buying gear (that's Gold). Item-to-shop assignment is a per-item FLAG (sold-in-main / sold-in-run / both), so the same item type can appear in both shops at different currencies.

**Run-hub-only:** Upgrade (power-up is a run activity), Draft, Return. **Everything else is in BOTH** (convenience — downgrade/repair/roll/reroll/merge wherever you are).

**Hub composition (LOCKED) — run hub = active workshop, local hub = lean restock/respec:**

| Service              | Local hub              | Run hub                       |
| -------------------- | ---------------------- | ----------------------------- |
| Purchase             | local-flagged (Prisms) | run-flagged (Gold)            |
| Upgrade (level gear) | —                      | ✅ upgrade essence + ½ Reality |
| Repair (fix broken)  | —                      | ✅ run hub only                |
| Merge (crystals)     | —                      | ✅ (Prisms)                    |
| Roll/reroll (stats)  | —                      | ✅ Reality + Prisms            |
| Downgrade (respec)   | ✅ refund ½ essence     | ✅ refund ½ essence            |
| Draft / Return       | —                      | ✅ pool→run / run→pool+Gold    |

**Principle:** the **run hub is the active workshop** — you do almost everything *while assembling for the challenge* (upgrade/repair/merge/roll/buy). The **local hub is lean** — purchase (restock) + downgrade (respec). **Downgrade is in BOTH.** This resolves "upgrade is immediate AND permanent": all active power-shaping happens IN the run where you use it, and persists.

**Item-to-shop is a TAG, not a hard rule:** each gear/skill/item is flagged **local-hub / run-hub / both** — the same item type can appear in both shops at different currencies (Prisms locally, Gold in-run) per its flag. Shop stock = item-flag-filtered rolled stock.

**Currency follows the ACTION, not the location:** the run hub is **multi-currency** (Gold to purchase, essence+½Reality to upgrade, Prisms to merge, Reality+Prisms to roll) — Prisms is a carried wallet, not location-locked. Local hub is Prisms-only (purchase) + the essence refund (downgrade). *(Friction noted: merge/roll spend Prisms IN the run hub — coherent since currency follows the action, but flag if it should change.)*

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

  **Repair cost (RESOLVED):** **FLAT by tier** (F cheap → S more), regardless of how much durability is missing — a simple per-tier price, not damage-scaled. **Cheap — a light maintenance tax**: gear is the durable layer you KEEP, so repair shouldn't punish use (don't compete with the big sinks — essence/Reality). **Currency-follows-location: Gold at the run hub, Prisms at the local hub** (repair is in BOTH hubs; you pay the local currency). Numbers (the per-tier Gold/Prisms values) = a tuning pass.
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

### Pool scope — ACCOUNT-WIDE shared, skins the only per-character exception (LOCKED)

**The pool is ACCOUNT-WIDE** — all characters share ONE owned collection (weapons, rings, evolutions, spells, abilities, crystals, currency, essence) — **gear AND skills are account-wide** (confirmed). Plus the §8 slot-expansion perks are account-wide (unlock once, all characters). Everything account-wide EXCEPT skins. No per-character inventory ownership; every character draws from the same pool. Your account IS your collection; characters are just who you take into a run.

**Cosmetics are ACCOUNT-WIDE (shared) — EXCEPT skins.** Chosen styles (defense style, infusion style, etc.) apply across all characters → can live on the pool, shared like everything else. **Skins** are the character-specific exception — and when built ("when the time comes"), skins will likely be **an entry on the CHARACTER DATA** (not the pool), keyed per-character. So: cosmetic STYLES = account-wide pool; SKINS = per-character on character data, deferred.

**Cosmetic-loadout spec (SCOPED, DEFERRED — "when the time comes"):** Survey confirmed the clean shape:
- **Options on UCosmeticsData:** add `FCosmeticStyleSet` struct (StyleId + the overridable axes: dodge L/R, block, parry montages + InfusionDisplay) + `TArray<FCosmeticStyleSet> StyleLoadouts`. Bare fields = the default; per-field fallback (null field in a chosen set → bare default, so partial sets work).
- **Pick on the pool:** an ACCOUNT-WIDE style selection (SaveGame, alongside wallet) — styles are shared across characters, so a single account-level pick (not per-character). Store the FName StyleId (not asset ref) — sidesteps the disk-save soft-pointer caveat. ⚠️ (Survey assumed per-character keying because chars have different UCosmeticsData assets — revisit: if all characters share a style choice, a single FName works; if a char's UCosmeticsData lacks the chosen style, fall through to its default. SKINS, separately, will be per-character on character data — deferred.)
- **Resolver:** `ULoadoutComponent::ResolveStyleSet()` — pick-set-and-loadout-exists? → use; else nullptr. Every cosmetic read funnels through ~6 LoadoutComponent getters (ONE chokepoint — external callers DefenseSystem/InfusionVFX never change). Each getter: resolved field non-null? → use; else bare default. Parry keeps its weapon-override on top.
- **Parity-safe** (no pick → default → byte-identical). Cluster: (i) slots [struct+array+pool map], (ii) wiring [resolver+5 getter repoints]. Picker UI = later.
- **Precedent:** the parry-per-weapon override (LoadoutComponent GetParryMontage) is the exact pattern to copy.
- Alternatives don't exist yet — build the mechanism, ship with one (default) until style sets authored. (Character A's skin ≠ Character B's). The cosmetic/skin PICK is keyed per-character; everything else in the pool is shared. *(Cosmetic system deferred — "when the time comes." The pool itself is flat/account-wide now; the per-character cosmetic layer is a later addition.)*

**⚠️ This OVERRIDES the earlier §9 "items are per-character (CharacterId owner)" note** — the simpler account-wide-shared model is locked. No CharacterId keying on inventory; the pool is flat + shared. (The §9 PlayerState mapping still applies for the persistent-vs-run-volatile axis; just the per-character-inventory split is dropped in favor of account-wide.)

### Pool structure — 3 organizational categories + filter layer (LOCKED)

The pool is STRUCTURED (not a flat type-mirror) — organized how a player browses their collection. Categories are ORGANIZATIONAL (browse/filter grouping); typed storage stays distinct underneath.

| Category      | Contains                                               | Storage model                         | Display                        |
| ------------- | ------------------------------------------------------ | ------------------------------------- | ------------------------------ |
| **Knowledge** | spells (FSpellInstance) + abilities (FAbilityInstance) | instance lists                        | individual items               |
| **Armoury**   | weapons + rings + evolutions (all 3 gear types)        | instance lists                        | individual items               |
| **Items**     | crystals + stones                                      | **counted stacks** (FCrystalId→int32) | **stacks with ×N quantities**  |
| *(Wallet)*    | currency (5 scalars + 14 typed essence)                | value-block                           | separate from the 3 categories |

**Items sub-buckets (2, pool-level only — storage untouched):** **Crystals** (gems, IsGemType) + **Augment Stones** (stat stones + ability stones — everything non-gem). Split via the existing CrystalTypeHelpers classification at the POOL/browse layer; run-side storage stays one pile (deeper storage split deferred). Items are COUNTED STACKS (fungible, no quality, tier is the stack key) — shown as ×N, not expanded to individual rows.

**Filter design (fixed axes):** `FPoolFilter` (Element / WeaponType / School / Tier / Quality — optional per axis, BP-friendly bUseX+X pairs) + an **accessor/translator layer** (the architectural core — hides where each attribute lives: instance vs asset-deref vs key, per type) + per-category getters (GetKnowledge/GetArmoury/GetItems applying only valid axes) + `QueryAll` (cross-category, element is the natural cross-axis). Quality applies to Knowledge+Armoury only (crystals have none). Asset derefs (element-on-spell, weapon-type, school) are cheap (always-loaded). The accessor layer is SHARED by pool + run inventory.

**Attribute homes (the filter crux — element lives in 3 places):**
- Element: spells=asset (ESpellElement), weapons/rings=instance-via-slotted-crystal, crystals=key (FCrystalId). One unified enum (ESpellElement), three lookups → the accessor layer reconciles.
- Weapon-type: weapons/abilities=asset. School: spells=asset. Tier: instance (gear/skills) / key (crystals). Quality: instance (gear/skills) / N/A (crystals).

**Inventory convergence (deferred, small):** the run inventory already stores by-type distinct — so converging it to the 3-category model is a THIN category-accessor facade (GetKnowledge=Spells+Abilities, etc.) + sharing the accessor/filter layer. No storage refactor, no migration. Build the structured pool first; facade the run later.

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

| NPC              | Handles                | Services                                                                                                                                                                    |
| ---------------- | ---------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Blacksmith**   | weapons                | upgrade (tier-up), remove attachments; **weapon-crystals** (sell/refine/attach to weapons)                                                                                  |
| **Jeweler**      | rings                  | upgrade (tier-up), remove attachments; **ring-crystals** (sell/refine/attach to rings)                                                                                      |
| **Stone Smith**  | augment stones         | sell/buy, refine, **attach** augment stones (any gear — not split by target)                                                                                                |
| **Spiritualist** | evolution              | upgrade (tier-up) evolution, remove evolution (from primary OR gear), attach evolution to weapon/ring                                                                       |
| **Spiritualist** | evolution + **FUSION** | evolution services (attach/upgrade/unslot) + **fusing** (combine 2 attachables → fusion stone, attach to gear). The 'advanced augmentation' NPC — no separate Fusion Smith. |

**Attachment-service organizing principle:** crystals split **by target gear** (Blacksmith=weapon-crystals, Jeweler=ring-crystals — each gear-smith owns their gear's crystals); stones → **one specialist** (Stone Smith, any target); **fusion → the Spiritualist** (folded in — evolution + fusion are both 'advanced augmentation'; no separate Fusion Smith). Crystals piggyback on the gear-smith who owns that gear; stones/fusion are distinct enough for dedicated smiths.

**Crystal storage (UPDATED — dual-purpose, post gem-merge 2026-06-25):** "Refining" is **CUT** (§13.5). Crystals are **dual-purpose** — one `Crystals` pool (gem-family, throw OR slot; slotting consumes) + one `Stones` pool (augment stones, slot-only). **2 pools, not 3** — the old GemItem/GemRefined raw-vs-refined split is gone (there was no refining mechanic; the split was pure bookkeeping). Stones never had a refined form (correct — unchanged).
- **Throwable crystals** (gem-family) — one `Crystals` stack; throw-eligibility is `GetItemEffectType != None`.
- **Augment stones** (AbilityStone etc.) — `Stones` stack; slot-only (no item effect → can't throw).

**⚠️ UPDATED — attach machinery is now BUILT (2026-06-25):** runtime crystal + fusion attach onto gear **shipped this session** — `AttachCrystalToWeapon/ToRing` + `AttachFusionToWeapon/ToRing` debit the pool and socket onto the gear instance (mirror the built evolution-attach). The atomic `RemoveCrystals`/`AddCrystals` batch primitive backs them (verify-then-commit, one signal). So the "attach-from-pool onto gear" gap is **CLOSED**. Refining is **CUT** (no raw→refined step — crystals are dual-purpose).

**Smith services — UPDATED status:**
- ~~Crystal cut/refine~~ — **CUT** (dual-purpose crystals; no refining step).
- **Attach-from-pool onto gear** — ✅ **BUILT** (this session: crystal + fusion attach + atomic remove primitive). The smith's attach half is done at the backend; only the NPC/UI trigger remains.
- ~~Stone-smith refining~~ — **CUT** (no refined form; dual-purpose makes it moot).

**Spiritualist owns FUSION (folded in — decision):** the Spiritualist combines crystals/stones → a **fusion stone**, and attaches it (plus evolutions) to gear. Fusion stones are PLAYER-CREATED (you fuse 2 attachables you own), NOT looted. **The fusion MECHANIC is already built + Live** (FusionStones.md): valid pair = stat-stone + one contributor (crystal/stat-stone/ability-stone), never evolution; keeps both halves' effects + a tier-scaled bonus% to a chosen substat (formula (TierValue(A)+TierValue(B))/2); FFusionId identity, EAttachedItemKind::Fusion slot, half-essence break — all built. **Still open (the genuine gaps):** (1) the SPIRITUALIST as the fusion SERVICE/NPC interaction (the crafting action at a cost — "Phase 3 crafting"); (2) ring-mounted fusion stones (planned: a crystal-containing fusion stone allowed on rings; bare stat/ability stays weapon-only) — designed, not built.
- ✅ RESOLVED: a fusion stone IS the player-created product of fusing 2 attachables (already built + Live — FusionStones.md). The Spiritualist performs the fusion.

**⚠️ RUNTIME ATTACH-OP GAP (the connector NPC attach services need):** "If we had UI, is it plug-and-play?" — answer is MIXED, split by attachment type (verified):
- ✅ **Evolution attach/detach is BUILT** — `UInventoryComponent::AttachEvolutionToWeapon`/`AttachEvolutionToRing` + remove variants exist. UI plug-and-play. (So the Spiritualist's EVOLUTION half works.)
- ✅ **Crystal DETACH built** (`RemoveCrystalFromWeapon`/`Ring`); all loadout management built.
- ✅ **Crystal ATTACH = BUILT** (2026-06-25) — `AttachCrystalToWeapon/ToRing` debit the `Crystals` pool and socket the gear instance. Stone ring-guard enforced inline.
- ✅ **Fusion ATTACH = BUILT** (2026-06-25) — `AttachFusionToWeapon/ToRing` (fuse-and-socket: consume the two halves atomically, write the Fusion slot). Ring accepts elemental fusions, rejects augmented.
- ✅ **`OnInventoryChanged` delegate = BUILT** (2026-06-25) — the foundation mutation signal; every grant/remove/equip/load broadcasts (Added/Removed/Equipped/Loaded). UI/loot bind it instead of polling.
- **Authored pre-attach WORKS** (a weapon designed with a fusion/crystal baked in inflates via `FromAttachedItem`).
- **Backed by the atomic batch primitive:** `RemoveCrystals`/`AddCrystals` (verify-then-commit, one signal) underpin the attach debits + merge consume.
- **So:** the NPC attach SERVICES now have their full backend (evolution + crystal + fusion attach all built + `OnInventoryChanged`). Only the NPC/UI trigger layer remains.

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

### 3.1 Gear effects — AUTHORED ONLY (player effect-crafting CUT)

**Decision (reversed the earlier "Spiritualist Effect Crafting" design):** players do NOT add/craft/roll effects onto gear. **Effects are authored by the designer on the gear itself** — players work with what they're given. The gear IS the build; you don't assemble a buff bar.

**Why cut it:** authored gear effects ALREADY WORK (`UEquipmentDataBase::Effects` → `ApplyEquipmentEffects`, built) — zero new code needed. Cutting player-crafting removes a large system (AppliedBuffs per-instance storage + roll/reroll + effect-tier axis + the Spiritualist effect service) for a feature that added complexity + a balance risk (rolled effect-tiers on the 4.8× TIER_POWER curve could run hot). Designer-authored effects fit the curated/narrative identity (legendary kits with hand-crafted effects > gacha-rolled stat-soup), and let the designer control power directly. The long-tail sink it would've provided is covered by other sinks (leveling, perks, rerolls).

**What this CUTS (do not build):** `AppliedBuffs` (per-instance runtime effect storage, gap #8) · the Spiritualist effect-crafting service · effect-tier axis (effects gaining an F-S tier) · the roll/reroll (Gold/Reality) for effects.

**What stays:** authored gear effects (`UEquipmentDataBase::Effects`, already firing via the built gather→apply machinery). The Spiritualist keeps its OTHER roles (evolution + fusion); only effect-crafting is cut.

**Design rule — 4 effects max per gear piece:** a DESIGNER GUIDELINE (not a runtime cap) — don't author more than 4 effects on any one weapon/ring, to keep gear readable + bounded. No code enforcement needed (designer discipline).
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

### 5.1c Ring types — 4-type taxonomy (LOCKED 2026-07-11)

Every shop ring is one of four **cost-composed** types. Cost stacks per the
implemented rules (tier base §5.1; attachment + bundled-skill components per
the §5.x AS-IMPLEMENTED block):

| # | Type | Composition | Cost (Prisms) | Existing examples |
|---|------|-------------|---------------|-------------------|
| 1 | **Template Ring** | bare ring, no attachment — player buys + attaches a crystal after | tier base | `DA_Ring_Generic` |
| 2 | **Crystal Ring** | tier-matched attached gem crystal, empty `DefaultSpells` (player fills) | tier base + crystal tier base | the 9 gem rings (`DA_Ring_Garnet` … `DA_Ring_Iolite`) |
| 3 | **Themed Ring** | tier-matched attached gem + 1–2 bundled Generic-pool `DefaultSpells` themed by concept | tier base + crystal tier base + Σ per-spell (base/3 + 10) | the 27 themed rings (`DA_Ring_Emberflame` … `DA_Ring_Sundered`) |
| 4 | **Evolution Ring** | tier-matched attached evolution crystal | tier base + 2× evolution tier base | none yet — **banked as an intended type** |

Worked at E (base 50): Template **50** · Crystal **100** · Themed **126**
(1 spell) / **152** (2 spells) · Evolution **150**.

Player-facing summary: `Mechanics/Gear/Rings.md`. Shop stocking:
`Architecture/MerchantShopSystem.md` (Jeweler).

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

Equipment (weapons/rings) = **Prisms base by tier** (no scaling surcharge — equipment scaling is authored differently; surcharge is a spell-pricing tool). Crystals/stones = Prisms base by tier + their typed-Essence purchase cost (§4.2). *(⚠️ Superseded in implementation — crystals/stones are now **Prisms-only** and equipment adds an attached-item surcharge; see the AS-IMPLEMENTED block under §5.x.)*

### 5.3 Ranking up (tier-up) = Gear/Skill essence + ½ Reality essence

Tier-leveling an item costs **two currencies** (NOT Gold):

- **The item's leveling essence, full** — Gear Essence for weapons/rings, Skill Essence for abilities/spells (§3, the category split).
- **+ Reality essence = HALF the leveling-essence amount** — a Reality companion cost, half the size of the main cost. (Reality = the reshape/meta currency; ranking up reshapes the item's power.)

So a tier-up that costs 100 Gear essence *also* costs 50 Reality essence. **No Gold** — upgrading is funded by the dismantle-fed essence economy + Reality, not the run currency.

> *Open:* the exact leveling-essence base per tier-step (scales — a C→B costs more than F→E, riding the §3 curve). The Reality half follows automatically (½ the Gear/Skill amount). PIE-tunable.

### 5.x — Item pricing (RESOLVED — formulas locked, coefficients tunable)

Pricing is **fully specified by formula** — every buyable item's price is derivable from its tier (+ quality for gear). The structure is locked; only a few coefficients are PIE-tunable.

**Per-item-type pricing:**
| Item type                  | Price formula                                                                                                            |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| **Weapons / rings (gear)** | Tier-half + Quality-half (§5.1b, 50/50 — the only type that prices on Quality)                                           |
| **Crystals**               | Prisms base by tier (§5.1) + typed-Essence cost (§4.2). **Tier-only** (no quality).                                      |
| **Stat-stones**            | **⅔ × the same-tier crystal price** (LOCKED — crystals are the premium attach; a C stone ≈ ⅔ of a C crystal). Tier-only. |
| **Spells**                 | 3-component (§5.2): typed Essence + Prisms base + 50×scaling-grade surcharge. **Tier-only** (no quality).                |
| **Evolution**              | premium, base 500 @ F (§5.3b), rides the tier-double from there. Top of the price ladder.                                |

**LOCKED decisions (this pass):**
- **Non-equipment is TIER-ONLY** — only gear (weapons/rings) factors Quality (the 50/50). Crystals/stones/spells price on tier alone (quality is a gear concept; simplest, avoids quality-pricing complexity on non-gear).
- **Stones = ⅔ crystal price** (same tier) — crystals are the more valuable attach.
- **Ordering (cheapest→dearest):** stones < crystals < weapons/rings < spells (3-component) < evolution.

**Still PIE-tunable (coefficients, not structure):** the exact upgrade-essence base per tier-step (§5.3, rides the curve); evolution's exact curve beyond 500@F; the 50/50 gear weight (could shift tier-weighted if quality proves too dominant). All placeholder-then-tune — the pricing SYSTEM is complete.

**Status: DEFERRED.** Too many items to price inline; do it as a focused session AFTER the rings + spells migration work. Flagged here so it isn't lost.

**⚠️ AS-IMPLEMENTED (2026-07-11, `feature/hub-merchants`)** — the shipped
`UEconomyService::BuildCartCost` diverges from the table above; source of truth
is now [`../Architecture/EconomySystem.md`](../Architecture/EconomySystem.md):

- **Crystals/stones are Prisms-only** (Crown): NO typed-essence charge — essence
  stays a dismantle-side currency for crystal stock. Supersedes the table's
  "+ typed-Essence cost" row.
- **Stones price the SAME as crystals** (tier base) — the ⅔-crystal rule is NOT
  implemented.
- **Evolution = 2× Prisms base** (F=50 … S=3200) + element essence @ tier +
  ½ Reality (mirrors leveling). Resolves §5.3b's open curve question — the
  premium is a multiplier on the shared tier-double, not a separate 500@F base.
- **Generic-element spells price like abilities**: Prisms base + SkillEssence @
  tier — no scaling surcharge, no typed essence (Generic would otherwise charge
  "Quartz essence").
- **Weapons/rings add an attached-item surcharge** on top of tier base:
  authored crystal/stone → its tier base; evolution → 2× its tier base; fusion
  → 1.5× the summed half bases.
- **Quality-half (§5.1b) NOT implemented** — quality is a C placeholder until
  the shop-roll generator lands; equipment prices tier-only today.
- **Bundled-skill surcharge (3l, 2026-07-11):** each non-null skill bundled on
  equipment prices at **its tier base / 3 + 10** (integer floor), Prisms-only —
  no essence on bundled skills. Gates: weapon `PresetAbilities` always;
  weapon/ring `DefaultSpells` only when a **gem crystal** is attached
  (`Kind == Crystal`); weapon `DefaultAbilities` only when the attachment can
  grant stone abilities (augment stone, or a fusion carrying an AbilityStone
  half). `WeaponAttack` is never priced — weapon identity, part of the base
  package. Enables the Themed-Ring cost row in §5.1c.

### 5.3b Evolution — third gear type (premium, persistent attach)

Evolution is a **third gear type** alongside weapons and rings — an **evolution crystal** you attach (to the primary slot, or onto a weapon/ring). Attaching it gives the host the evolved state (spell-holding slots, higher loadout-slot cost). It is NOT a transform *action* with a cost — it's a **piece of gear you acquire and attach.**

**Premium price.** Evolution costs far more than a normal item — **base 500 at F tier**, scaling up by tier. (Compare: a normal F item is 25 Prisms — evolution is ~20× pricier, fitting its power as the spell-unlocking gear.) *(Open: the per-tier curve from 500 — doubling [500/1k/2k/4k/8k/16k/32k] is steep; confirm the top, or use a gentler scale. Currency: Prisms-based like other gear, OR the Gear+Skill essence premium discussed — confirm.)* **→ Resolved in implementation (2026-07-11): 2× the §5.1 tier base in Prisms (F=50 … S=3200, a gentler premium than 500@F) + element essence @ tier + ½ Reality — see the §5.x AS-IMPLEMENTED block.**

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

## 7.9 Enemy inventories — LEVEL POOL + bespoke (DESIGN THREAD, 2026-06-25)

**Concept:** enemies get loadouts the SAME way players do — drawn from a pool — but the pool is **per-level/area**, not per-character. Two tiers:

**1. Level pool (generic enemies).** Each level/area defines a **pool of items that can appear there** (an authored per-level item table — "these weapons/spells/crystals belong to Level N"). **Generic enemies roll a loadout from their level's pool** — so the same enemy type plays differently each encounter (variety), and the LEVEL sets the power band + theme. Low-authoring, scalable, varied.

**2. Bespoke (named/special enemies).** Bosses + special enemies get **authored or custom-randomized loadouts** — not bound to the generic level pool. Hand-crafted control where it matters (trash = table-driven, bosses = hand-built — the standard RPG split).

**Why it fits cleanly:**
- **Reuses the BUILT inventory/loadout machinery** (audit: inventory/loadout system is ✅ solid; pool-draw proven lossless this session). Enemies become "a character with an inventory drawn from a pool" — the same draw logic, pointed at a LEVEL pool instead of an account pool. Not a new system — a new pool SOURCE + an enemy-spawn draw hook.
- **Self-balances with the caliber-earn (§7).** A higher-level enemy draws from a higher-tier level pool → richer kit → higher `GetTotalPool()` caliber → more WSP earned. The level pool sets enemy power, which sets the reward. Deeper level = tougher enemy = bigger drop. The earn system (C3) reads whatever caliber the enemy has — authored OR pool-drawn — so it works regardless; pool-draw just makes caliber VARY by level.

**Independent of the World Stats build:** C3 (earn) reads enemy caliber from `GetTotalPool()` whether the kit is authored or pool-drawn. So enemy-inventories is its own arc — bank now, build after the World Stats faucets (C4 vendor + the draft UI).

**Source-tag model — LOCKED (2026-06-25, web-validated):**

- **Items carry `FGameplayTag` source tags** (UE-native hierarchical, engine-built for "where can this appear"): `Source.Floor.2`, `Source.Level.Volcano`, `Source.Vendor.Blacksmith`, etc. Multi-dimensional — one item can be tagged for many floors/levels/vendors at once. Designer-editable in the central tag registry, no code per new tag.
- **A "pool" = a TAG QUERY, not an authored list.** Floor-2 enemies draw from `query(Source.Floor.2)`; Volcano drops = `query(Source.Level.Volcano)`; Blacksmith stock = `query(Source.Vendor.Blacksmith)`. Add an item with the right tags → it AUTOMATICALLY appears in every matching pool. No pool-list editing.
- **Querying: OR to start** (item appears in ANY pool it's tagged for), `FGameplayTagQuery` AND/NOT available later for finer sub-pools — costs nothing now, upgrade path built-in.
- **TAG = filter, ROLL = selector.** The tag query produces the ELIGIBLE candidate set; the actual pick is a **weighted ROLL** over that set (the existing drop-chance / Luck-weighted draw — REUSES built machinery). Tags answer "what's eligible here?", the roll answers "what actually drops/spawns?" — the production pattern (PoE/Diablo: level-range + tags → eligible set → weighted roll).
- **Enemies, player-drops, and vendors all use the SAME mechanism** — same tag-query → weighted roll, different tag. Enemy kit, player loot, shop stock are one system viewed three ways.

- **Enemy-kit ↔ player-loot relationship — LOCKED: same query, two rolls (option a).** When you defeat an enemy, your drop rolls from the SAME level tag-query the enemy's kit drew from — but as a SEPARATE weighted roll (the enemy's specific kit and your specific drop are independent picks over the same eligible set). "You fight what you can win." Sub-tags (`Source.Floor.2.Enemy` vs `.Drop`) can DECOUPLE specific items later without rebuilding — the hierarchy gives that escape hatch for free. (Note: the shared-world Lord/Contender vision — player characters as AI enemies — could later use option (b) "win the enemy's actual kit" for NAMED/character enemies, taking their build; banked as a future flavour, not the generic default.)

**Crystal tagging — DECIDED: tag crystals too (location-gatable uniques).** Crystals are enum-keyed (`FCrystalId{Type,Tier}`), not assets — a UPROPERTY tag field has nowhere to live. So crystals get a SEPARATE tag mechanism: a tag table keyed by `ECrystalType` (crystal → its `FGameplayTagContainer` source tags). Most crystals = universal (tagged broadly / untagged = everywhere); rare/unique crystals = location-gated via their table entry. Built alongside the asset-tag layer (its own small piece).

**Build shape (mostly reuses built systems):** the weighted draw EXISTS (drop-chance/Luck draw, survey-confirmed). NEW pieces: (1) an `FGameplayTag` source-tag field on the item definitions, (2) a tag-query→candidate-set helper, (3) the enemy-spawn draw hook (roll a kit from the floor/level query) + generic-vs-bespoke enemy flag. Independent of the World Stats build; gated on run-state/trial-structure for the SEQUENCING, but the tag+query+roll layer is buildable on existing foundations.

**Open (design, when built):**
- The level-pool DATA structure (a `ULevelItemPool` data asset per level? a table keyed by level?).
- The enemy-spawn draw hook (where a generic enemy rolls its loadout from the level pool at spawn).
- Generic-vs-bespoke flag on the enemy (which path an enemy takes).
- ⚠️ Does the level pool ALSO feed player drops/rewards (one level table, two uses), or enemy-loadouts only? (Crown leaned enemy-loadouts; player-drop reuse is a possible extension — flag.)
- Tier-scaling within a level pool (does a level pool span tiers, or is it tier-locked to the level's band?).

## 8. Persistent buffs — account perks (essence sink, permanent)

Permanent run-start advantages bought with Essence, surviving death. They **raise the floor**, they do not skip the climb. Lean toward *perceivable* perks (felt immediately) over invisible % boosts. **Account-wide** — unlock once, applies to every character (consistent with the account-wide pool).

**Structure — TIERED TRACKS:** most perks are incremental tracks you buy up step-by-step (e.g. Weapon Slot 1→2→3→4→5), each step costing more essence than the last. A few are single one-off unlocks. This is the connective tissue — nearly every progression mechanic designed this session routes through here.

**Guard-rail:** keep floor-raisers small — especially the WSP head-start — or the run-axis (which resets on death) gets hollowed out by stacked perks.

### Slot-expansion tracks (the draft/gear capacity — this session)
| Track               | Steps | Effect                                                        |
| ------------------- | ----- | ------------------------------------------------------------- |
| **Weapon Slots**    | 1 → 5 | how many weapons each drafted loadout starts with             |
| **Ring Slots**      | 2 → 5 | how many rings each drafted loadout starts with               |
| **Evolution Slots** | 1 → 5 | how many evolutions each drafted loadout starts with          |
| **Vendor Return**   | 1 → 5 | items you can bank at the mid-trial vendor (partial-bank, §7) |

### Run-start floor tracks
| Track                   | Steps / Cap | Effect                                                                                                                               |
| ----------------------- | ----------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| **WSP Head Start**      | up to ~4    | begin each run with +N World Stat Points pre-allocated (CAP ~4 — small, keep the run-axis meaningful; reconciled from the older ≤10) |
| **Starting Purse**      | small       | begin each run with +X Gold                                                                                                          |
| **Baseline Resistance** | small       | begin with a small flat resistance pool                                                                                              |
| **Roll Budget**         | modest      | gear bonus-stat / resistance rolls generate from a larger budget                                                                     |

### Themed-inventory tracks (anti-dilution, §1 draft)
| Track             | Effect                                                                                                                                       |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| **Themed Chance** | raises the chance a themed/preset loadout appears in the draft — THE ANTI-DILUTION lever (keeps themes findable as your eligible pool grows) |
| **Themed Slots**  | raises the NUMBER of themed options that can appear (1 → 2-3 at a maxed account)                                                             |

### Single one-off unlocks
| Perk                 | Effect                                                                        | Cap        |
| -------------------- | ----------------------------------------------------------------------------- | ---------- |
| **Extra Draft**      | +1 inventory OPTION at the run-start draft (4 options to pick from, not 3)    | 1          |
| **Signature Weapon** | one owned high-tier weapon guaranteed into the run draft                      | 1          |
| **Signature Spell**  | one owned high-tier spell guaranteed into the run draft                       | 1          |
| **Drop-Grade Shift** | every drop rolls one grade better (curve shifts up a step; S-odds at A, etc.) | 1          |
| **Reroll Charge**    | +1 gear reroll/run (the §12.2 perk hook)                                      | per design |

**Why essence:** it's the long-tail currency — once a core loadout is maxed, account perks give players something to keep pouring essence into. Tiered tracks deepen the sink (each step costs more).

**Open (numbers only — structure is locked):** exact essence cost per track step; the cost CURVE per track (linear vs escalating); final per-track caps. The tables above are structure + guard-rails, not final tuning.

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

## 13.5 Crystals — GEMS DUAL-PURPOSE (gem item/refined split CUT)

**Decision:** **GEMS become dual-purpose** — one gem, **thrown (consumed in combat) OR slotted (attached to gear)**, player's choice at point of use. The gem **item-vs-refined** split is **CUT**. **STONES are unchanged** — they stay slot-only (you can't throw a stone), in their own bucket.

**Buckets: 3 → 2.**
- Today (runtime): `GemItem` (throw) + `GemRefined` (slot) + `StoneItem` (slot-only) = **3 pools**.
- After: **`Gems`** (one dual-purpose bucket — throw or slot) + **`Stones`** (slot-only, unchanged) = **2 pools**.
- The two GEM buckets collapse into one; the Stone bucket is left alone (stones genuinely aren't dual-purpose).

**Why:** there was **no refining mechanic** (PK-confirmed — no `RefineCrystal` op, no sink, no gate; the gem item/refined split was pure storage bookkeeping). So the split bought only the pre-sorting of gems into "ammo" vs "gear" at acquisition — needless friction. Collapsing it = one gem bucket, simpler merge (no `bRefined`), simpler model ("you have gems; throw or slot them").

**Core rule — SLOTTING CONSUMES (locked):** slotting a gem **debits it from the Gems bucket** (one gem = one use; throw it OR wear it, not both). Preserves scarcity; matches built attach behavior (attach already debits; detach destroys — fungible, no GUID).

**Cap:** each bucket keeps its own `CRYSTAL_PER_TIER_CAP` (20/tier). Because gems and stones stay SEPARATE buckets, a full gem shelf does NOT block stones (the 3→1 cap-collision worry doesn't apply — that was the everything-into-one model, not this). Net gem capacity goes 2×20→1×20 per tier (the two gem buckets merge); stones unchanged at 20/tier. The gem tightening (40→20 effective, since you no longer hold item+refined separately) is acceptable / intended — one bucket, one cap.

**What this CUTS:**
- The `GemRefined` pool — folded into the one `Gems` pool (was `GemItem` + `GemRefined`).
- `MergeCrystals`'s `bRefined` selector for gems — one gem bucket, no item/refined axis.
- The `ECrystalPool` pool-explicit machinery (just built, `a32d1bee`) — **mostly unwound**: with one gem bucket there's no gem item/refined ambiguity. **KEEP the atomic verify-then-commit BATCH** (fusion still consumes 2 halves atomically; merge still consumes a multi-crystal set) — revert to `FCrystalId`-only batch, drop the pool tag.

**Eligibility stays property-based (clean):** throw-eligibility is already `GetItemEffectType(Id) != None` (stones → None → can't throw); slot-eligibility already property-based. So merging the gem buckets loses NO enforcement — dual-purpose falls out naturally.

**Migration:** runtime is greenfield (run-scoped, no live saves) — just change the fields. The only persisted artifact is the authoring `UInventoryData` asset → `PostLoad` fold (LFS-safe, lossless, no re-authoring).

**Blast radius (bigger than the inventory component):** the account pool (`UPoolSubsystem`) mirrors the same gem item/refined split + the draw copies map-by-map, so they collapse too. Clusters: core storage → facade/primitives → economy/loadout → account pool → authoring+migration.

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

## Hub+Combat Character — LOCKED DESIGN (2026-06-25, Direction A, web-validated)

**Goal:** ONE character serves both the walkable HUB (third-person traversal) and fixed-position COMBAT, by swapping controllers + anim layers per mode. UPDATE THE EXISTING combat character (additive) — do NOT rebuild on a fresh UE base.

**Direction A (update existing) — chosen after MCP surveys revealed B was redundant.** Crown initially leaned B (fresh UE base) for a "cleaner movement foundation," but the surveys showed the existing character IS ALREADY that foundation: parent `BP_CharacterBase` → `ACharacter` with CapsuleComponent + CharacterMovementComponent + SkeletalMesh, AND already on the **Manny skeleton** (same as UE's third-person base — so B's anims would've needed no retarget *because they're already the same skeleton*), AND no camera/input to conflict with. B's cost (re-home every component, re-satisfy every combat dependency, risk breaking working combat) bought nothing A doesn't already have. A is ADDITIVE: switch on movement the character already has + add the 2 missing cosmetic pieces (camera + locomotion anim) + the hub controller. Combat wiring stays 100% intact — the orchestrator still finds the same character, no component moves, no anim-instance re-cast.

**Architecture (LOCKED):**
- **Base:** the EXISTING `BP_CombatCharacter` / `BP_CharacterBase` — UPDATED, not replaced. It's already an `ACharacter` on Manny with full movement. No duplicate-and-rehome; work additively on the real character.
- **Components:** UNCHANGED. All gameplay components (CharacterData, Inventory, Loadout, CrystalInventory, EvolutionInventory, Currency, BrokenDarknessManager, WeaponMesh, InfusionVFX, ElementColorDebug) stay exactly where they are. Nothing moves. Combat's FindComponentByClass lookups + orchestrator class-find all keep working unchanged.
- **Anim — ADD a Locomotion LINKED LAYER alongside the existing Combat anim (UE5-native, Lyra-proven):** keep the existing combat AnimBP as the base; ADD a **Locomotion layer** (UE third-person locomotion) as a linked anim layer, active in hub mode. Combat anim logic stays the base/Combat layer, untouched. Swap the active layer by mode (`bIsCombat`). ⚠️ Whether the existing combat AnimBP cleanly accepts an added locomotion layer vs needs light restructuring is the survey's key anim finding — but even worst case, A keeps the combat CHARACTER wiring intact (the expensive part).
- **Mode flag `bIsCombat`:** ⚠️ cached in the EventGraph (read from controller/character), READ in the AnimGraph — the thread-safe split (AnimGraph runs on the anim worker thread, must not call game-thread functions directly).
- **Hub mode:** third-person `APlayerController` possesses it → walks (movement already there) → Locomotion layer active. Needs the follow-camera (spring-arm + camera) — character has NONE today.
- **Combat mode:** `ACombatPlayerController` possesses it → no movement input, fixed-position → Combat layer active. Combat camera unaffected: `ACombatCameraManager` drives the view via PC `SetViewTargetWithBlend` pointed at its OWN camera actors — so a character-owned follow-camera is automatically DORMANT in combat (confirmed via MCP, zero conflict).
- **Mode toggle = GameMode-per-level:** combat level uses `BP_Combat_GameMode` (PlayerController=`ACombatPlayerController`, DefaultPawn=`BP_CombatCharacter_Fire` ⚠️). Hub gets its OWN GameMode (third-person controller + the new char as default pawn). Switching modes = level/GameMode transition. Combat GameMode stays UNTOUCHED.

**Build discipline (additive on the real character — combat-risk managed by branch + PIE gate):**
- Work additively on the existing character — add camera + locomotion layer + hub input WITHOUT touching combat wiring. The risk is lower than B (nothing re-homed), but it's still the live combat character, so: do it on a BRANCH, tag before starting, PIE-verify combat STILL works after each additive change.
- The combat path must remain unbroken at every step (combat anim still plays, orchestrator still finds the char, defense windows still fire). Test combat in PIE after adding the camera, and again after adding the locomotion layer.
- ⚠️ Stray duplicate to clean later: `/Game/Core/GameModes/BP_CombatGameMode` (unused; `LevelMechanics` uses `/Game/Level/BP_Combat_GameMode`).

**Build order (next session, after the survey):**
1. Branch + tag. Add follow-camera (spring-arm + camera) to the existing character. ⚠️ PIE-verify combat STILL works (camera dormant in combat, view-target unaffected).
2. Add the Locomotion linked anim layer alongside the existing combat anim; `bIsCombat` cached EventGraph→read AnimGraph. ⚠️ PIE-verify combat anim STILL plays.
3. Hub GameMode + third-person controller (its own IMC: move/look/jump, added in BeginPlay — separate from the combat controller's IMC, no clash). Prove walking in `TestLevel_Nav`.
4. The mode toggle (level/GameMode transition hub↔trial).
NOTE: no component re-homing, no orchestrator re-find, no anim-instance re-cast — combat keeps using the SAME character it always did. A is fewer clusters than B.

**Status:** DESIGN LOCKED + SURVEYED (2026-06-25). Build fresh next session.

### Re-integration survey findings (2026-06-25) — refine the anim approach + flag the skeleton

**KEY REFINEMENT — Path A (expand existing locomotion SM) beats Linked Layers here.** The survey found `ABP_CombatCharacter` is ALREADY `UCombatAnimInstance`-derived AND already contains a Locomotion state machine (just an Idle state today). Combat is 100% montage-slot-driven — combat montages overlay whatever the base graph does. So the simplest, lowest-risk path is to **expand the existing Locomotion SM to full walk/run/jump** — ONE AnimBP serves both modes, combat montages still overlay, zero combat code touched. Linked layers are NOT needed (and linked layers alone wouldn't satisfy combat anyway — see the cast requirement). Supersedes the earlier "Linked Anim Layers" plan: use Path A unless a reason to layer emerges.

**⚠️ THE hard break-risk — the AnimInstance cast (mandatory):** combat hard-casts `GetMesh()->GetAnimInstance()` to `UCombatAnimInstance` (ActionExecutor.cpp:4995, DefenseSystem.cpp:571, CombatNotify.cpp:41). If the main AnimInstance is NOT `UCombatAnimInstance`-derived, the notify/ended delegates don't fire → action-finalize + VFX/damage timing break (there's a degraded `PlayAnimMontage` fallback, but it loses the delegates). So the main AnimBP MUST stay `UCombatAnimInstance`-derived. ⚠️ Do NOT use the stock third-person AnimBP (plain `UAnimInstance` — would break combat). Expand the EXISTING combat AnimBP (already the right class).

**⚠️ SKELETON MISMATCH — the one real complication:** combat is on **UE5 `SK_Manny_HTS`**; the third-person template content in-project (`/Game/SoStylized/...`) is on **UE4 `UE4_Mannequin_Skeleton`** — DIFFERENT skeletons. Can't use the template's locomotion anims as-is. Recommendation: source UE5-Manny locomotion (the CombatMasterAnimBundle is Manny_UE5-based — CONFIRM it already carries locomotion on `SK_Manny_HTS` before authoring) OR do one UE4→UE5 IK-retarget pass. Confirm the exact USkeleton of any locomotion anims first.

**LOW-RISK confirmations:** (1) combat does NOT filter by character class — `StartCombat` takes a `TArray<AActor*>` team array; combatant-test is `FindComponentByClass<UCharacterDataComponent>`, not a class cast. The char just needs the components + to be in the team array. (2) All 10 gameplay components stay on `BP_CharacterBase` — keeping the char descended from it satisfies the full component contract for free, nothing moves. (3) Per-character knob that matters: `CharacterData.CharacterData = DA_Character_*` (each elemental variant). (4) Third-person camera/input pattern already in-project to copy (not its UE4 skeleton/AnimBP).

**SURVEYED CLUSTER PLAN (≤3 files each, additive, combat provable at each step):**
- **Cluster 1 — Hub controller + GameMode** (zero combat impact): new `BP_HubPlayerController` (third-person input + possession + view target) + `BP_Hub_GameMode` (DefaultPawn = the character, PlayerController = hub). BONUS: add `RemoveMappingContext` to `ACombatPlayerController::EndPlay` (Enhanced-Input hygiene). Prove walking in a hub level.
- **Cluster 2 — Camera rig** (additive): SpringArm + Camera on `BP_CharacterBase` (or hub-side subclass); dormant in combat (combat sets PC view target to its own camera actor — verified). PIE-verify combat still frames correctly.
- **Cluster 3 — Locomotion in the AnimBP** (the risky one, ISOLATED): on a DUPLICATE of `ABP_CombatCharacter`, expand the Locomotion SM to walk/run/jump (retarget locomotion anims to `SK_Manny_HTS` first if needed). Swap the char's AnimClass to the duplicate ONLY after combat montages (stance/action/notify/ended) re-verify in PIE. Keep `ABP_CombatCharacter` as the proven fallback until then.


### Hub camera — Expedition 33 feel (target for Cluster 2 tuning pass, web-researched 2026-06-25)

Expedition 33's exploration camera is a STANDARD dynamic third-person follow-cam (not exotic) — the spring-arm + camera rig already planned IS the base. Tune toward their feel in the Cluster 2 camera-rig pass (tuning only makes sense once the char walks — Cluster 1 first):
- **Longer arm** (~500–600 vs UE default 400) — pulled back for the wide, environment-showcasing frame (their environments are a showpiece; camera mods push it even higher/wider).
- **Higher pitch + slight downward look + Z/socket offset up (~50–80)** — the elevated "survey the world" angle.
- **Camera Lag enabled (low value)** on the spring arm — smooth, floaty cinematic follow vs rigid lock.
- **Sprint** wired to the existing `IA_Sprint` (already in the input set) with a high sprint speed — E33's "inhumanly fast, streamlined" traversal.
- **A few fixed-camera set-piece rooms** are a later tool (reuse placed-camera support from the combat camera system) — NOT the default.
- ⚠️ **Beat their weakness:** E33 was criticized for NO minimap → occasional disorientation. A minimap/compass is the easy win they left on the table — bank for the hub UI.
Sequencing: Cluster 1 = walking + basic follow-cam (don't tune feel against a non-moving char). Cluster 2 = finalize rig + this Expedition-33 tuning pass.

### Cluster 3 locomotion source — LOCKED (2026-06-25): MM defaults, shared AnimBP

**Decision (Crown):** use the STOCK UE5 Manny MM anims (the defaults), NOT Paragon. Rationale: taking *inspiration* from Expedition 33, not replicating it — the neutral stock anims are the baseline. MM keeps it un-locked-in (generic UE5 Manny; can branch/extend/swap later); Paragon would couple to that specific set. Keep it SIMPLE — walk/run/jump only, no 8-way (8-way × 14 characters = unmanageable; not needed).

**Scope:** walk, run, jump. That's it. Forward-only.

**Source assets (all on the Big_Pack SK_Mannequin the AnimBP already targets — ZERO retarget):**
- MM_Idle, MM_Walk_Fwd, MM_Walk_InPlace, MM_Run_Fwd, MM_Jump (+ MM_Fall_Loop, MM_Land available)
- BS_MM_WalkRun already built: BlendSpace1D, Speed 0–500 → MM_Walk_InPlace@0, MM_Walk_Fwd@230, MM_Run_Fwd@500 (idle is a SEPARATE state, not in the blendspace)

**Sharing across 14 characters: ONE shared locomotion AnimBP.** All 14 characters use the same `ABP_TestCombatCharacter` (→ becomes the locomotion-capable AnimBP). They all walk/run/jump identically — correct for generic traversal, scales trivially (one AnimBP, all characters point at it via BP_CharacterBase). NO per-character locomotion.

**⚠️ Avoid:** the stray `SKM_Skeleton_HTS.uasset` (a UE4-mannequin skeleton) — do NOT wire it into the locomotion path. Stick to the Big_Pack SK_Mannequin everything else uses.

**Build (Cluster 3, in ABP_TestCombatCharacter — the duplicate, original kept as fallback):**
1. Expand the Locomotion state machine: Idle state (MM_Idle) ↔ Move state (BS_MM_WalkRun, speed-driven) + a Jump state/path (MM_Jump). EventGraph caches Speed (from owner velocity) + bIsInAir thread-safely (cache in EventGraph, read in AnimGraph).
2. Combat montages overlay regardless (montage-slot driven; main AnimBP stays UCombatAnimInstance — already is).
3. Repoint BP_TestCharacterBase.AnimClass → ABP_TestCombatCharacter ONLY after combat montages (stance/action/notify/ended) re-verify in PIE. Keep ABP_CombatCharacter as fallback.
4. PIE: walk/run/jump animates in hub; combat still works.

### Hub movement speed driven by action-speed stat (LOCKED 2026-06-25) — Cluster 3 follow-up

**Decision (Crown):** action-speed stat DOES drive hub traversal speed (not just combat). Rationale: in shared LOCAL-HUB MULTIPLAYER, seeing other players move at different speeds based on their build is a readable expression of character identity — a small detail that makes the hub feel alive and makes stat choices visible to others. (Note: the action-speed stat is also SET/live in the hub — head-start allocation, presetting ≤4 world stats, substats via persistent buffs are all hub/outside-battle prep.)

**Stays simple — ONE shared locomotion AnimBP for all 14.** What varies is `MaxWalkSpeed` on the movement component (set from the action-speed stat), NOT the animation. The blend space (BS_MM_WalkRun) already matches anim to speed, so faster characters naturally show more run-blend — no per-character anim work.

**Build order:** locomotion state machine FIRST at flat 500 (prove walk/run/jump animates), THEN add stat-driven speed as a follow-up (don't conflate anim bugs with stat-wiring bugs).

**Follow-up step (after state machine works):** on hub spawn/possess, read action-speed stat (from CharacterData/stat component) → map to MaxWalkSpeed within a CLAMPED band → set CharMoveComp.MaxWalkSpeed. Sprint multiplier (875-style) applies on top. ⚠️ Foot-sliding: BS_MM_WalkRun tops out at 500 (Run_Fwd); if stat pushes speed well above 500 the run anim won't keep up. Fix: clamp hub speed to a sensible band (e.g. ~400–700) so it varies but stays in anim range. Play-rate-scale the blendspace at the top end only if needed later (this is the legit use of play-rate, which is otherwise unnecessary since walk+run are separate blend samples). Mapping (stat→speed curve, exact band) = TUNING, design when building the follow-up.

**REFINED SPEC (2026-06-25):** Stat: **ActionSpeed** (Body sub-stat, anim/attack pacing — NOT TurnSpeed/Reflex; matches the doc's literal "action-speed"). Model: **base run = FLAT 500 for everyone; SPRINT scales with ActionSpeed** (the stat shows in the burst — two players sprint, the fast-build one visibly pulls ahead = the readable multiplayer moment). Read **GetEffectiveActionSpeed()** (CharacterDataComponent.cpp:1023) — the effective MULTIPLIER, bounded 1.0–2.0, which ALREADY folds in world presets (WorldBodyLevel) + equipment BonusActionSpeed + ActionSpeedStone + skill ActionSpeedBuff/Debuff. Do NOT use raw GetTotalActionSpeed() (ignores buffs/world). Map [1.0,2.0] → sprint band **[700, 1050]** (Crown chose wider/more dramatic over the safe 700–900). ⚠️ Sprint anim (TwinBlast Sprint_Fwd in the embedded blendspace) tops out ~875 → above that, extend the blendspace sprint sample higher and/or play-rate-scale at the top to avoid foot-slide (accepted cost for the wider band). 
- **Wiring:** IA_Sprint Started = computed sprint speed (mapped from GetEffectiveActionSpeed, clamped to band); Completed = flat base 500. Replaces the current hardcoded 875/500.
- **HUB STAT SOURCES (no combat buffs in hub):** in the hub, ActionSpeed comes ONLY from (1) the points the player allocated into ActionSpeed (base stat) + (2) the ≤4 world-stat presets they set in the hub (Body-pillar scaling) + equipment. NO transient combat buffs (those are trial-only). So it's pure build-choice expression.
- **APPROACH = OPTION A (live read at sprint-press) — CHOSEN:** IA_Sprint Started reads GetEffectiveActionSpeed() LIVE via a BlueprintCallable helper, computes sprint speed fresh each press. This DISSOLVES the timing problem entirely (press always happens long after CharacterDataComponent::BeginPlay resolved base+world+equipment) AND auto-reflects any world-preset change made in the hub (re-allocate presets → sprint immediately faster — the build-choice feedback loop just works, no cache/re-read hook). Chosen over Option B (cache + deferred hook + re-read on change) which is more moving parts for no benefit since the value isn't needed elsewhere yet.
- **Home:** BP_HubPlayerController (already owns the IA_Sprint speed writes). Combat does NOT leak (fixed-position, MOVEMENT_SPEED_BASE/PER_POINT retired, warp is montage-driven — hub MaxWalkSpeed ignored by combat).

### Deferred: weapon-flag split (separate refactor, after hub-slide fix)
Crown's idea — the current single "change/show weapon" flag does too much. Split into TWO flags:
1. **Switch weapon** — swaps between weapons; only usable when a character has MULTIPLE weapons (mainly Generic, who carries dual/multiple).
2. **Show/hide weapon** — independently shows or holsters the weapon mesh.
Separate concerns (switching vs visibility). DEFERRED — do after the hub stance-montage slide fix. NOT a fix for the slide (the slide is the stance MONTAGE overriding locomotion, not weapon mesh visibility).

### ⚠️ ACTIVE BUG: hub locomotion slide = combat stance montage override (2026-06-25)
ROOT CAUSE FOUND: BP_TestCharacterBase with ABP_TestCombatCharacter slides in idle pose in the hub because the C++ parent UCombatAnimInstance::UpdateCombatState() calls PlayStanceMontage() every frame (the test char has CharacterData → logs "[CombatAnimInstance] Playing stance montage: Anim_Sword_1H_Attack_Idle"). The stance montage plays on a slot OVER the locomotion state machine → holds sword-idle pose while capsule slides. The locomotion state machine itself is built correctly — it's just being COVERED by the stance montage overlay that shouldn't run in hub mode.
- ⚠️ The "TryGetPawnOwner not valid" warnings in the log are the AnimBP PREVIEW actor (AnimationEditorPreviewActor), NOT the live PIE character — red herring; the live char IS possessed and moving.
- FIX (combat-vs-hub mode separation — the bIsCombat distinction finally surfacing): gate PlayStanceMontage/UpdateCombatState so the stance montage only plays IN COMBAT, not in the hub. Least-invasive: a bIsInCombat check in UpdateCombatState (don't touch combat behavior). Survey CombatAnimInstance.cpp for an existing combat-active signal (CombatOrchestrator running? a state bool?) to drive the gate.
- Once gated, the locomotion underneath should show through immediately (state machine work not wasted, just hidden).

### Deferred: sprint leg-cadence polish (play-rate scaling)
Stat-driven sprint works (band 800–1800, ActionSpeed-driven). At high speeds the legs don't fully keep up (some foot-slide) — cosmetic, deferred. The fix options (researched 2026-06-25):
- ⚠️ The Move state uses an EMBEDDED Blend Space GRAPH (plain "Blendspace" node), NOT a "Blendspace Player" — so there's NO Play Rate PIN to expose (only Blendspace Player nodes expose it).
- ⚠️ "Axis to Scale Animation" (the intended UE feature) is NOT available on 1D blend spaces — and BS_MM_WalkRun is 1D. So that path is out.
- **Option A (simplest, try first):** set a fixed Rate Scale on the Sprint_Fwd SAMPLE inside the embedded blend space (~1.5). One number, no node changes. Fixed (not speed-driven) but may be enough.
- **Option B (dynamic):** convert the node to a Blendspace Player (gets the Play Rate pin) OR convert the 1D blend space to 2D — then drive Play Rate with `max(1.0, Speed/SprintRef)` (SprintRef ~800; the max() floor keeps walk/run at rate 1.0 so only the sprint zone scales). More work.
- **Option C (AAA, if play-rate looks chipmunk at 1800):** Stride Warping (Animation Warping plugin) — lengthens stride instead of cranking cadence. Most setup.
Build order if pursued: try A → if not enough, B (dynamic play-rate) → C only if cadence looks frantic at the top.

### Hub→Trial door model + PvP options (CONCEPTUAL, banked 2026-06-25 — needs networked MP, far off)

**The door model (locked direction):**
- **Hub** = solo, walkable, prep/shop/menus ONLY — NO combat happens in the hub.
- **Trial** = the run, where ALL combat happens. Entered via a **DOOR**.
- **The door you pick determines the trial type:** PvE trials, or a **PvP trial door** (pick a level, with friends, randomized matchup).
- Fits the existing "hub solo, matchmake at trial launch, trial = the shared multiplayer space" design (Nightreign model).

**PvP architecture options (conceptual — pick when building):**
1. **Lobby/party then launch (recommended fit):** form a party in the hub / at the PvP door → pick level (or random) → launch together into a shared trial instance. The Nightreign/Helldivers model; matches "matchmake at trial launch."
2. **Door = matchmaking queue:** walk through PvP door → queue → matched → shared trial. More drop-in/quick-match.
3. **Host/join via door:** one player opens a PvP level → friends join their instance. Friends-focused vs random.

**What KIND of PvP (shapes everything) — the standout option:**
- ⭐ **Asymmetric "Lord vs team" PvP** — directly matches the locked pitch tagline *"Become a Lord powerful enough to hold a throne against a full team."* ONE Lord defends, a TEAM of Contenders attacks, fought via the turn-based combat. This fits the Lord/Contender hierarchy and the shared-world vision better than symmetric PvP. Strong candidate for THE pvp identity.
- (Alternatives: co-op-same-trial-with-competitive-scoring; or symmetric direct PvP.)

**⚠️ HARD PREREQUISITE — networked multiplayer:** ALL PvP (and shared trials generally) needs networked MP — replication (sync game state across machines), a server/host model, matchmaking infra. Combat is currently single-player/local. This is likely the BIGGEST system in the project and sits far from current work (walkable hub character). Bank PvP; build networked MP as its own major arc first. Everything above is DESIGN ONLY until then.

### ⭐ Combat camera / skill presentation — THE EXPEDITION 33 MODEL: Sequencer per-skill (researched 2026-06-25)

**KEY FINDING (from Sandfall's own UE dev interviews + GDC 2026 talk):** Expedition 33 does NOT use a coded camera manager. **Every combat skill is a Level Sequence (Sequencer cinematic)** — "treating all skills as small cinematics, in which we dynamically bind battle actors at runtime." Camera moves, VFX, projectiles, and timing are ALL co-authored per-skill in Sequencer, with the real battle actors bound at runtime. Quote: "Every single move in the game, during battle in particular, is a level sequence." This gave art/design direct control over on-screen action per ability WITHOUT programmer time per skill.

**This VALIDATES Crown's instinct exactly:** "make action cameras for the skills rather than have it coded." The mechanism is **Sequencer / Level Sequences**, not a bespoke camera-state manager.

**Implications for WoR:**
- The current `CombatCameraManager` (coded Home/Character/Action states via SetViewTargetWithBlend) is the OLD, over-engineered approach. Crown's "it's over-engineered" read is correct. NOTE: Crown says it's not currently in active use — LOW RISK to rework/replace.
- ⭐ **Direction: per-skill Level Sequences.** Each skill asset references (or carries) a Level Sequence that authors its camera + VFX + timing; battle actors dynamically bound at runtime. Replaces coded camera states with authored-per-skill content. This is potentially how ALL skill PRESENTATION should work, not just camera.
- E33 is **"vanilla-first," ~95% Blueprints, minimal bespoke code** (GDC 2026) — they push native UE tools (Sequencer, CommonUI, ALS, KawaiiPhysics) to their limits rather than building custom systems. The lesson for a solo dev: lean on Sequencer (native) over bespoke camera code.
- Turn-based makes it tractable: controlled environments, predictable on-screen actor count, authored camera + scripted timing. WoR is turn-based → same approach viable.
- E33 also ships a **"Camera Movement" OFF toggle** (static wide camera) for accessibility/motion-sickness — the dynamic camera is divisive (vocal "make it stop moving" contingent). Worth replicating: dynamic per-skill sequences + a static-camera accessibility toggle.

**STATUS: design direction identified, NOT YET DESIGNED/BUILT.** This is a significant architectural shift (coded camera → Sequencer-per-skill) touching skill presentation broadly. Deserves its own design pass + survey of current CombatCameraManager + how skills currently trigger presentation, before building. The now-present character cameras (from the reparent) are great for the HUB; combat presentation should go the Sequencer route, not character-camera.

### Per-skill camera — DIRECTION LOCKED (2026-06-25): Level Sequence per skill, layered (option b)

**Decision (Crown): full Level Sequence per skill, NOT simple transform data.** Reason: the IMMERSIVE cinematic feel is the whole point (the "Expedition 33 meets Destiny" pitch) — a static framed angle doesn't deliver it; the moving authored shot does. Don't undercut the point by going simple.

**Architecture (locked):**
- **New `Camera` field on the skill data asset** = `TSoftObjectPtr<ULevelSequence>` — references an authored cinematic (camera moves + optional VFX flourish) authored per-skill in Sequencer. This is the E33 model ("every skill is a level sequence").
- **Layering = option (b):** the sequence runs ALONGSIDE the existing casting. Crown's EXISTING combat timing stays UNTOUCHED — ActionExecutor → montage → notify → damage/defense windows all keep running as-is. The sequence is for CAMERA (+ presentation flourish), NOT gameplay timing/damage. Do NOT move damage/defense events into Sequencer event tracks (that's option a — a risky rewrite of working combat; rejected).
- **On cast:** play the skill's Level Sequence, DYNAMICALLY BIND its caster/target placeholder tracks to the actual battle combatants (the genuinely tricky part — Sandfall's "dynamically bind battle actors at runtime"; UE Sequencer has a binding API for this), camera track drives the view; on sequence end, return to the combat camera.

**The hard part = runtime actor binding.** The sequence is authored with placeholder caster/target; at runtime it must bind to THIS cast's actual actors. Doable (Sequencer binding API) but it's the real engineering of this system.

**Scope/sequencing:** This is its OWN focused arc (survey + design + build), NOT a tail-on. Needs: (1) survey how casting currently triggers/times a skill (ActionExecutor → montage → notify chain) so the sequence syncs to action-start without fighting existing timing; (2) the `Camera` skill field; (3) the runtime bind + play/cleanup lifecycle; (4) confirm the sequence camera and the (to-be-reworked/removed) CombatCameraManager don't conflict — the sequence likely SUPERSEDES the coded camera states. Replaces the over-engineered CombatCameraManager's coded approach with authored-per-skill sequences (Crown: CombatCameraManager over-engineered + not currently in active use → low risk to supersede).

**STATUS: direction locked, build is a fresh-session arc.**

### Combat camera architecture — DISTRIBUTED model (locked direction 2026-06-25)

**Crown's key insight:** since every character now OWNS a camera (from the Generic-as-base reparent), the camera system doesn't need a monolithic manager juggling shared cameras. **Each character's camera frames THAT character.** The system becomes a simple STATE-DRIVEN SELECTOR, not a coded position-computing manager — which dissolves the "CombatCameraManager is over-engineered" problem.

**Plus:** caster + target are ALREADY known to combat (skills already aim at targets), so the per-skill Level Sequence (see "Per-skill camera" section) just borrows the caster/target combat already has — the "runtime binding" is a non-problem, not the hard part.

**The model (state → camera selection):**
| Combat state                              | Camera                                                                     |
| ----------------------------------------- | -------------------------------------------------------------------------- |
| Character's turn (idle / choosing action) | THEIR camera, framing them                                                 |
| Character casts a skill                   | The skill's Level Sequence, anchored to them (their camera / their action) |
| Not their turn                            | WIDE SHOT (establishing, see the field)                                    |
| Defending (incoming attack)               | Reaction framing / wider (see the incoming threat)                         |

**Camera ownership:**
- **Character cameras** (one per character, already present via reparent) → that character's turn + their actions.
- **Per-skill Level Sequences** → when they cast (anchored to the caster, see locked per-skill camera direction).
- **A SMALL set of SHARED "establishing" cameras** (wide field shot + maybe a couple angles) → the not-their-turn + defense moments. These aren't character-owned (a wide field shot belongs to no single character). This is the ONLY shared-camera piece — everything else is distributed.

**Why this is better:** distributed (character-owned + skill sequences) + a few shared wides, SELECTED by combat state — vs the old monolithic coded manager computing positions. Far less code, cameras as content (on characters / skills), the "manager" shrinks to a state→camera selector. Matches Crown's original instinct (less code, camera as data) AND the E33 Sequencer-per-skill model.

**[SUPERSEDED by the COMPLETE ARCHITECTURE section below — the 'shared establishing cameras' idea was dropped in favour of one-camera-per-character following shared mode rules + agency-based viewer perspective.]**

### Combat camera — COMPLETE ARCHITECTURE (locked 2026-06-25, E33-validated w/ screenshots + research)

**Core principle: cameras are INDEPENDENT but follow the SAME RULES.** Each character owns ONE camera (from the Generic-as-base reparent). Every camera obeys the same mode ruleset; GAME STATE (whose turn, who's acting) determines which mode each camera is in. The correct framing EMERGES from each camera independently evaluating the shared rules against state — NO central position-computing manager. This supersedes the over-engineered CombatCameraManager.

**The shared mode rules (every camera follows these):**
| Mode                          | The camera does                                                                                                                                                                                                                                                                                            |
| ----------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Turn**                      | Close over-the-shoulder on the acting character, character-LEFT, OPPONENT(S) VISIBLE across the field (option b — NOT pure character; the standoff/opponent read is the content, esp. in PvP). E33-confirmed via screenshots: combat reuses the close exploration-style framing, cinematic layered on top. |
| **Action** (action confirmed) | The per-skill Level Sequence takes over (cinematic, anchored to the known caster — see per-skill camera section).                                                                                                                                                                                          |
| **Defence**                   | Camera frames the INCOMING attack so the defender can time dodge/parry. KEEP this — it does real work (E33 screenshot confirms the defense QTE needs to see the threat).                                                                                                                                   |
| **Not your turn**             | Pan out (establishing/wider).                                                                                                                                                                                                                                                                              |

**Reuse insight (E33):** combat Turn-mode framing = the same close over-shoulder as EXPLORATION (E33 doesn't switch to a special rig for the basic turn view). WoR can reuse a similar close framing for Turn mode (the character's own camera in a close mode) — less to build.

**⚠️ E33's ONE big camera complaint = the LOCKED over-shoulder angle (motion sickness; most-requested mod = a CENTERED preset + adjustable distance).** Since WoR's camera is mode-driven/per-character anyway, build it MODE-SWAPPABLE / adjustable from the start (close-over-shoulder AND a centered/wider option) to avoid E33's mistake.

**VIEWER PERSPECTIVE = tied to AGENCY (the key multiplayer insight).** What camera YOU see depends on what YOU control:
| Mode                                  | Whose camera you see                                                                                                                                                                                 |
| ------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Solo PvE** (control whole team)     | Zooms in on EACH of your members as their turn comes (you direct all of them → close on each in sequence).                                                                                           |
| **Co-op PvE** (control one character) | Close-up ONLY on YOUR turns. When a TEAMMATE acts → you see the PAN-OUT / spectator view (clean "not my decision" signal; also keeps co-op pacing light — no sitting through teammates' cinematics). |
| **PvP** (control your Lord/team)      | Same logic — your perspective centers on what you control; opponents' turns = pan-out/spectator.                                                                                                     |

Same ruleset across all modes — only "who is MINE to control" differs. The close camera MEANS "this is yours to decide"; pan-out means you're watching. Scales solo/co-op/PvP from one system.

**Full architecture summary:**
1. Each character owns ONE camera (reparent).
2. All cameras follow the same mode rules (Turn/Action/Defence/Not-your-turn).
3. Game state drives each camera's mode.
4. Viewer sees the camera tied to THEIR agency (close on what they control, pan-out on what they don't).
5. Per-skill Level Sequences plug into Action mode.
6. Build mode-swappable/adjustable (close vs centered) to dodge E33's locked-angle complaint.

**STATUS: architecture fully designed. Build = fresh-session arc. Survey current casting (caster/target refs, skill-fire timing, how CombatCameraManager currently hooks in) first, then build the per-character camera + the shared mode-rule selector + per-skill sequence playback, superseding CombatCameraManager.**

### Camera flow refinement — acting-player vs watching-players + Defence-maybe-redundant (2026-06-25)

**Two viewpoints in PvP (the acting player sees differently from watchers):**

**Acting player (the one taking the turn):**
- Close-up of THEIR character while choosing/confirming the action.
- On CONFIRM → the ACTION camera (per-skill Level Sequence) takes over → cinematic shot of their attack.

**Watching players (everyone else, during the actor's turn) — 3 options, (b) is the likely default:**
- (a) Show the whole team — best tactical readability, least cinematic.
- (b) ⭐ Close on the actor but wide enough to SEE OTHER TEAM MEMBERS — readable + focused. Likely sweet spot for PvP team fights (pure close-up loses battlefield awareness; whole-team loses immersion).
- (c) Just the acting character — most cinematic, but in PvP team fights risks feeling claustrophobic (lose awareness of teammates/threats).
- PvP-specific: team fights need SOME battlefield awareness (whose turn next, threats) → (b) over (c) as default.

**⭐ Crown's insight — the DEFENCE camera may be REDUNDANT:** if the camera is always on whoever's ACTING, then when you're attacked you see the incoming hit THROUGH THE ATTACKER'S ACTION CAMERA (you're the target, you're in that shot). The action camera doubles as the defense view → no separate Defence camera/mode needed.
- ⚠️ PRESSURE-TEST before dropping Defence: the defense system has REAL-TIME parry/dodge timing windows — the defender must CLEARLY see the attack wind-up + strike moment to time it. VIABLE to drop Defence camera IF the action camera keeps the defender's timing readable (it's aimed at the attack landing on the defender, so it should). If the cinematic is too "attacker-glory-shot" and obscures defense timing, KEEP a defence beat. → This is a PLAYTEST question: author an action camera, check if you can still parry off it.

**Refined mode set (pending the defence playtest):** Turn (close on actor) → Action (per-skill sequence, on confirm) → [Defence: possibly folded into Action]. Watching-players default = (b) actor + team context.

## Hub/Trial Merchants — LOCKED DESIGN (2026-06-26)

**Merchant map (type→merchant is AUTOMATIC from item class; vendor tags only for overrides):**
| Merchant                   | Sells                                                                                            |
| -------------------------- | ------------------------------------------------------------------------------------------------ |
| Blacksmith                 | Weapons, augment stones                                                                          |
| Jeweler                    | Rings                                                                                            |
| Combat Master              | Abilities                                                                                        |
| Spell Shop                 | Spells                                                                                           |
| Spiritualist               | Evolutions, crystals, repairs (it's the CRYSTAL that wears, not the weapon — hence repairs here) |
| ALL merchants — TRIAL ONLY | World stats (existing C4 EconomyService::BuyWorldStat, Gold, escalating, run-scoped)             |

**Availability tags (option a — locked):**
- No tags → HUB only, at its type's merchant (default)
- `Trial.X` → trial only (e.g. `Trial.Garnet` = any Garnet-trial floor; `Trial.Garnet.Floor1` = that floor only — hierarchical, both levels kept)
- `Hub` + `Trial.X` → BOTH
- Trials are element/class-based (9 elements + Generic + Resonator) → trial tags map to the existing taxonomy; no item-registry blocker for trial matching

**Currency:** hub merchants = persistent currency (Prisms); trial merchants = Gold (run-scoped). Same merchant type, different currency per context.

**Presentation:**
- HUB: placeholder CUBES + interact (walk up, press E) → shop UI. 3D models / shop-door interaction later.
- TRIAL: NO NPCs — shop UI only (menu at floor stops listing accessible merchants).

**v1 build:** UMerchantData (UPrimaryDataAsset): merchant type, explicit item list (TArray — registry/tag-QUERY is v2 when C0 item registry lands), currency, element/class tag field. Tags authored on items from day one so v2 migration = swap list for query.