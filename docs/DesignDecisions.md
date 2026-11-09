# Open Design Decisions

The queue of questions only Crown can answer.

**How it's used**
- **Claude Code (local, working the backlog):**
  - Never decides an `OPEN` item.
  - When a backlog job needs a decision, it adds or links an entry here, marks the job `BLOCKED (Q-xxx)` in `docs/Backlog.md`, and **moves on to the next unblocked job**. No waiting.
- **Crown:** answers whenever convenient. Ways to do it:
  - from a Claude cloud session (phone or web), talking it through and having it write the answer here and push;
  - by editing this file on GitHub;
  - by telling a local session.
- **Next backlog run:**
  1. `git pull`
  2. sees `ANSWERED`
  3. unblocks the jobs
  4. implements
  5. moves the entry to **Decided** with a link to where it was applied.

**Statuses**
- `OPEN`: waiting on Crown.
- `ANSWERED`: Crown replied; not yet applied.
- `DECIDED`: applied; recorded in the relevant design doc.
- `PARKED`: deliberately later.

**Numbering (Crown, 2026-10-26), from D-081 on:**
- **`Q-<job>`**: a question Claude Code raises while doing a backlog job takes **that job's number**: a question from B-155 is **Q-155**; a second one is Q-155b (the first stays Q-155). The number says which job it belongs to, and Claude Code never needs to pick a free number.
- **`D-xxx`**: design decisions from Crown's chat sessions (cloud), next free D number.
- Existing D-001…D-081 keep their numbers. Both kinds live in this file and use the same entry format and statuses.

**Entry format** (keep each short; one decision per entry):
```

### D-NNN — Title   (or Q-<job> — Title)
**Status:** OPEN · **Raised:** YYYY-MM-DD · **Blocks:** B-xxx, B-yyy
**Question:** one line.
**Context:** 2–4 lines, with file:line.
**Options:** A … / B … / C …
**Recommendation:** X, because …
**Answer (Crown):** _write here_
**Notes for Claude:** _optional: anything to do differently when applying it_
```

**Archive (D-132, B-340):** answered and decided entries live in [`DesignDecisions_Archive.md`](DesignDecisions_Archive.md), same numbers and format. This file keeps what still needs Crown or a job: `OPEN`, `DISCUSSED`, `PARKED`, and `ANSWERED` entries whose jobs are still `TODO` / `BLOCKED`. Once applied, an entry moves to the archive. New entries go here.

---

## Open

### D-017 — Header bar style (UI)
**Status:** PARKED until the B-101 mockup · **Raised:** 2026-10-25 · **Blocks:** UI work (later)
**Question:** Which of the four deckbuilder candidates (`UI_DeckbuilderReuse.md` §3)?
**Answer (Crown):** (2026-10-26) Pick from pictures: B-101 draws all four candidates (plus a crystal-themed one, see D-024) in the combat HUD mockup, and Crown chooses there. **Drawn (B-111, 2026-10-27):** `combat/hud/v2/header_candidates_annotated.png` shows A-E; Crown picks in B-102.

### D-078 — Combat camera: three choices the basic version made
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-156) · **Blocks:** — (B-156 shipped with the recommendation; B-157 tunes it)
**Question:** B-156 had to pick three behaviours D-074 doesn't cover. Keep them?
**Context:** `CombatCameraDirector.cpp` HandleActionStarted / HandleActorTurnStarted. (1) With **camera movement off**, skill cinematics are skipped too (one static view). (2) **Enemy skills** play their cinematics like yours. (3) A cinematic runs **until it ends or the next turn starts** (not cut when the action's damage resolves).
**Options:** A keep all three / B movement-off still plays cinematics (only turn moves stop) / C enemy skills keep the pan-out view (no cinematics) / any mix.
**Recommendation:** A, because movement off is a motion-sickness setting (cinematics move the camera most), enemy cinematics are how you read an incoming hit (design §2, Defence mode), and cutting at damage would chop the end of most shots.
**Answer (Crown):** **A, keep all three** (2026-10-26): movement-off also skips cinematics; enemy skills play their cinematics; a cinematic runs until it ends or the next turn starts. **Keep for now, needs testing** (Crown): mark all three as untested defaults to check in PIE (`docs/PIE/B-156.md`). **Each also becomes a setting** (Crown, 2026-10-26): (1) cinematics on/off separate from camera movement, (2) enemy skill cinematics on/off, (3) skip a cinematic once its hit has landed. Defaults as above.
**Notes for Claude:** _optional_

### D-088 — Actions per turn (discussion: one utility then one battle action; Turn Speed stays)
**Status:** APPROVED, PLAN FIRST (Crown, 2026-11-09: build B-170's plan; code after Crown approves it) · **Raised:** 2026-10-26 (rules review, actions per turn) · **Blocks:** — (turn-system plan, B-170)
**Answer (Crown):** (2026-10-26)
- **Action pips:** each turn you get **action pips** set by your **Reflex** stat; each action (attack, skill, item, …) costs a pip, so a high-Reflex character can act **more than once per turn**.
- **No Turn Speed:** turn order becomes **fixed (round-robin)**, as the deckbuilder's turn belt. A faster-paced game; Reflex is what makes a character "fast".
- **Ice slows Action Speed** (the attack/spell animation speed stat): a full Ice bar is an Action Speed debuff for 2 holder turns. *(D-123: Action Speed is now the one **Speed**; read this as "Ice slows Speed".)*
- **Wind affects action pips:** a full Wind bar costs the target action pips (placeholder: −1 pip on their next turn, D-090).
**Supersedes / changes:**
- D-014/D-030 Wind push-back → pip loss. D-029 Ice → Action Speed slow (Crown's original wording, "slows action speed"; it had been mapped to turn speed).
- D-049 (2:1 turn cap) no longer applies: a **pip cap** replaces it.
- D-054/D-056: the Turn Speed family (`TurnSpeedBuff/Debuff`, `ModifyTurnSpeed`, TurnSpeedStone, the Turn Speed substat) is retired (enums append-only: kept, marked retired).
- ET-18 / one action per turn (kept earlier today) becomes "one action per **pip**".
**Model agreed in discussion (Crown, 2026-10-26, later the same day):**
- **Actions work like Energy:** you **gain 1 action per turn**, banked **up to your Max Actions** (set by Reflex); unused actions carry over. Start each fight with **1 action** and **3 Energy**.
- **Skills cost actions by tier** (by default): **F, E, D = 1 action**; **C, B, A = 2**; **S = 3**; plus their Energy cost. So big skills mean saving up (skip spending this turn, cast it next).
- **Energy:** start 3, gains per turn up to Max Energy (ties to D-089 C).
- **Stats and effects control both:** Max Actions, Max Energy and per-turn gains can be raised or lowered (Reflex, gear, buffs, Wind's pip loss).
**Refined (Crown, 2026-10-26, still discussion only):**
- **Base Max Actions is 2.** You **raise your Max Actions** (stats, gear, effects) to unlock **better skills**.
- **Higher tiers cost many more actions**, e.g. an **S-rank skill needs 7 actions**: you wait turns, use items and effects to build up to it. Replaces the 1/2/3 costs above. A one-per-tier ladder (F 1, E 2, D 3, C 4, B 5, A 6, S 7) is the natural reading of "S = 7"; confirm the exact ladder with Crown.
- So Max Actions becomes the key build stat for big skills, and action-gain effects and items become valuable.

**Simplified (Crown, 2026-10-26, latest; still discussion only), replacing the pool/pip ideas above:** the action-point pool was over-engineering.
- **Each turn: one battle action + one utility action**, so a turn is more tactical than one move. **The split (Crown, 2026-10-26):** **battle action** = basic attack, skill / spell, **move row**; **utility action** = **Defend**, use an item (incl. utility crystals), switch weapon or ring. So you can e.g. attack and Defend in the same turn, or move row and drink a potion. If built, this replaces the separate "one free switch per turn" (ET-19) and "free row move once per turn" (D-096) rules.
- **Energy stays, with low values** (deckbuilder-style small numbers) as the cost of skills.
- No action-point costs by tier.
- **Order (Crown, 2026-10-26):** **utility first, then battle.** Using your battle action **ends your turn**, so any utility action (Defend, item, switch) has to come before it.
- **Turn Speed stays (Crown, 2026-10-26, A):** the Reflex-pip and fixed-turn-order ideas above are dropped. Speed-based turn order, the 2:1 cap (D-049), bonus turns (D-052) and Wind's push-back (D-014) stand; Ice slows Action Speed (as Crown first said, D-029). The Turn Speed family is **not** retired (D-054/D-056 as decided).

**Notes for Claude, open details to bring to Crown (as questions in the plan, B-170):**
- Pip numbers: base pips, Reflex thresholds, the cap (suggested: 1 base, max 3).
- What Emerald's bonus turn (D-052) becomes: +1 pip, or an extra turn in the belt.
- First strike (D-069): +1 pip, or act first in the order.
- Turn order within a round (party order, alternating sides, …).
- Reflex today also widens the defense timing window; keep both jobs or split them.
- Effect durations already count the holder's own turns (D-051), unchanged.

**Confirmed by Crown (2026-11-09):** the fixed rotation and banked actions stay (Turn Speed goes; Reflex sets Max Actions). Next is B-170's plan doc (`docs/analysis/ActionPips.md`). Open for the plan: Speed is split again into Action Speed and Spell Speed (D-151), so say which one Ice slows. **Also (Crown, 2026-11-09):** effects can **give or take actions** (e.g. +1 action now, -1 action next turn), alongside Wind's pip loss; the plan lists them as effect types. **Wind (Crown, 2026-11-09):** the Wind **status bar** (build-up) takes **action pips**; the **Wind crystal effect** makes the target **lose their next turn** in the rotation. **No actions, no turn (Crown, 2026-11-09):** a unit that starts its turn with 0 action pips after the turn's gain (e.g. Wind or a lose-actions effect took them) has its turn skipped.
### D-089 — Should Energy refill each turn?
**Status:** DECIDED, BUILD (Crown, 2026-11-09: changed from C, see the change below) · **Raised:** 2026-10-26 (rules review) · **Blocks:** —
**Question:** EP never refills on its own today (ET-29: only effects, items, drains and Broken Darkness absorption). Should it?
**Context:** Max Energy (Spirit) is the pool size; Efficiency (Mind) lowers costs. Deckbuilder: +1 Energy per turn, banked, no cap. Expedition 33: AP +1 per turn plus more from basic attacks and parries. With action pips (D-088), a basic attack that returns Energy becomes a use for spare pips.
**Options:** A no refill (current) / B a share of Max Energy each turn (e.g. 10%; Max Energy then also sets refill) / C Expedition 33 style: a small refill each turn **plus** Energy from basic attacks and Perfect Guards.
**Recommendation:** C, because it rewards good play (timing gives Energy back), gives basic attacks a purpose, keeps casters from running dry, and needs no new stat. Broken Darkness keeps its own absorption system.
**Answer (Crown):** **C** (2026-10-26): Energy **refills a little at the start of each of your turns**, plus extra from **basic attacks** and **Perfect Guards** (Expedition 33 style). Amounts are named constants (`// TODO: tune`); the per-turn share can scale with Max Energy. Broken Darkness keeps its own absorption system (no passive refill, ET-29 for BD).

**Changed by Crown (2026-11-09):** Energy refills **only at the start of each of your turns** (a share of Max Energy, a named constant, `// TODO: tune`). Energy from basic attacks and Perfect Guards is **not** a base rule: it comes from **gear effects** (passives) where an item grants it. Broken Darkness keeps its own absorption (no passive refill). Job: B-173.
### Q-173 — Which turns refill Energy: every turn kind, or Normal turns only?
**Status:** ANSWERED (approved by the reference-backed default) · **Raised:** 2026-11-09 (B-173, cloud) · **Blocks:** —
**Question:** D-089 (changed 2026-11-09) says Energy refills "at the start of each of your turns". A character also gets Bonus turns (Emerald) and a ritual's Execution turn. Do those refill too?
**Options / as built:** **A** every turn kind refills, because each runs the same turn-start step (ET-17; Q-044d: an Execution turn is still one of the caster's turns) / **B** only Normal turns refill (a check on the turn type).
**Recommendation:** A. It is what "each of your turns" says, and it keeps one turn-start step for every turn kind (Q-044d). D-089's own reference, Expedition 33, gives AP at the start of a character's turn (Crown's 2026-10-26 context); the deckbuilder's +1 Energy per turn (D-089 context) has no bonus-turn case to compare.
**Answer:** **A**, by the reference-backed default (2026-11-09). Crown may change it (B is a one-line turn-type check in `TurnStartEnergy::Apply`'s caller).
**Notes for Claude (as built):** B-173: `ACombatOrchestrator::ProcessStartOfTurnEffects` calls `TurnStartEnergy::Apply` on both of `HandleTurnStarted`'s branches.

### D-099 — Field commands: the battle menu outside battle
**Status:** ANSWERED in part (2026-10-27; details being asked one at a time) · **Raised:** 2026-10-27 (Crown, from Q-185 first strike) · **Blocks:** — (builds on GASMigration decision 2, real-time field abilities)
**Answer (Crown):** (2026-10-27; Metaphor: ReFantazio, Kingdom Hearts)
- **The battle command menu also works on the map**, for different uses: **attacks, spells, abilities, items**.
- **Spells and skills get field utility**: a skill can have an effect outside battle, e.g. a **movement speed spell actually makes you move faster** on the map. Authored per skill.
- **Attacks and spells are free-aimed outside battle:** pick from the menu, the animation plays as in a fight, but there's **no lock-on**: you have to actually hit.
- **Abilities may have two animations:** the battle one (warped to the target) and a **field one (not warped, in place)** so it works without a target.
- **Field durations are in minutes:** a skill's duration in turns becomes minutes on the map, **1 turn = 1 minute** (a 1-turn speed buff lasts 1 minute).
- **Field skills cost Energy (EP)**, as in battle.
- **Time while the menu is open: B, it keeps running** (Kingdom Hearts), because of multiplayer (pillar 7): a shared world can't pause or slow for one player. **Noted for later:** A, slowing time while choosing (Final Fantasy VII Remake), perhaps as a solo-only option.
- **EP on the map: B** (Persona's SP model): EP does **not** refill over time; only **rest spots and items** restore it. EP spent on the map is EP you don't have in the next fight.
- **Battle skills can double as field skills** (a field use is one more side of the same skill), **and there are also field-only skills** (Crown, 2026-10-27, corrected): skills that only do something on the map (e.g. traversal). Where field-only skills are slotted: open.
- **First strike (Q-185):** landing a field attack or spell on an enemy that hasn't noticed you gives your side first strike (it acts first). Until field commands exist, a touch from outside its notice cone stands in (as built).
- **What a field hit does** (Crown, 2026-10-27): a field hit **starts the encounter with turn advantage** (your side acts first, as first strike). No outright kills on the map and no damage carried in. **It also gives status buildup** of its element, carried into the fight (Crown, 2026-10-27). **Holding the field attack with a Water source makes it an Ice attack.** (The earlier idea of an elemental field strike for elemental classes stays a note for later.)
- **Field skills as a whole: pinned for research** (Crown, 2026-10-27). Crown's ideas to explore: **movement through field skills** (flight, speed, etc.), and **evolutions changing how you move** (e.g. a character whose evolution lets them run at lightning speed). Research: `Design/FieldAbilities.md`. **Base movement: everyone gets a fast, free sprint** (Crown, 2026-10-27). **Evolutions give both** a small always-on movement bonus and one active EP-cost movement skill (Crown, 2026-10-27). **Movement order: speed and dash, then glide, flight last; dash evolves into sprint; enough speed lets you wall-run** (Crown, 2026-10-27). Where field-only skills are slotted waits on it.
- **Simplified (Crown, 2026-10-27), replacing the field-use / field-only-slot parts above:**
  - **Everyone:** a fast, free sprint.
  - **One active field skill per character**, set by **level / source** (where it comes from: innate, weapon, ring or crystal, evolution, and its level). It costs EP.
  - **Enhanced Sprint** is a field skill: it lets you **wall-run**.
  - **Evolutions** add a small always-on movement bonus and **augment the field skill to a degree** (e.g. the lightning character's skill becomes a lightning dash).
  - **Sources:** a field skill can draw on a source like battle skills do; **a crystal source still loses durability** when used (`CrystalWear.md`).
  - **Dropped:** separate field uses on battle skills, and field-only skill slots.
  - **Enhanced Sprint also runs on water** (Crown, 2026-10-27).
  - **Flight limits: A** (Crown, 2026-10-27): EP drain while flying, a height ceiling, no-fly zones; **effects can raise them** (an evolution might give unlimited flight; a character with unlimited energy: for another day). **Hidden areas in the sky** are welcome (pillar 10).
  - **Swimming:** everyone can swim (base movement).
  - **Elements change the world: A** (Crown, 2026-10-27): a few general rules defined once (element → material, element → element; materials don't change each other), Breath of the Wild style; 1-2 element shortcuts or secrets per map region.
  - **Enhanced Sprint** is also **faster than the normal sprint** (Elden Ring Nightreign's surge sprint), on top of wall-running.
  - **Every element can do every movement type, each in its own way** (Crown, 2026-10-27, refines the line below), **except Teleport, which is Void only** (Crown, 2026-10-27; Lightning's chain of blinks is its Air dash / Dash style). Movement types unlock through the three aura skills (`Design/FieldAbilities.md`). The **element decides the style**, not which movement type; an elemental source is still needed for the elemental movements (a Generic without one has only the normal dash and sprint).
  - **How a movement type unlocks: A** (Crown, 2026-10-27): each type has its **own unlock skill** (equip or learn it once: unlocked for good, for everyone); **some unlock by hidden conditions** instead (the memory condition system, D-100, pillar 10).
  - **How you get your one movement skill: A** (Crown, 2026-10-27): in the loadout you **pick a source you have available, then pick a movement type you've unlocked**; the source gives it its element style (e.g. a Void source + Teleport = a Void teleport). Strength follows the source's level.
  - **Source picks the skill: A** (Crown, 2026-10-27): the **source decides which field skill** you have (e.g. a Wind crystal glides, a Lightning one dashes) and **its level decides how strong** it is. The source also decides **what is available at all**: e.g. a Generic can't fly without an elemental source that gives flight.
  - **A range of movement** (Crown, 2026-10-27): dash, Enhanced Sprint, glide, flight, teleport, high / double jump, air dash (hover, aim, then dash; hover merged in), super launch, phase / shadow step, mark and recall, **Regroup** (teleport to a party member: co-op catch-up is this spell / movement type, not a free system; Crown, 2026-10-27) (flight hovers when stationary; no grapple, wall climb, mounts or surface surf; element styles share one mechanic per type with a few signature differences; `Design/FieldAbilities.md`). **All are locked until unlocked, except the normal dash**, which everyone has.
  - **Activation:** **double-click L3** (the sprint stick) uses your field movement skill (keyboard: a double-tap of the sprint key, `// TODO` confirm; unique input action name, D-063).
  - **Some field skills are locked** until unlocked: you must have **equipped or learned a specific skill** first. **Once unlocked, anyone can use it** with the matching source: e.g. after the lightning character's lightning speed is unlocked, **any character with a Lightning source** gets lightning speed as their field skill.
**Notes for Claude:** built on GAS (stage 4 abilities, B-043): one ability per skill with a battle and a field mode. Open details: each element's movement style (Crown) (`Design/FieldAbilities.md`).

### D-104 — PvP
**Status:** ANSWERED in part (2026-10-27; rest pinned) · **Raised:** 2026-10-27 (Crown) · **Blocks:** —
**Answer (Crown):** (2026-10-27)
- **PvP is part of this game: B, planned for now**, so the co-op plumbing (D-103) allows for it. Earlier concept: a **PvP trial door** in the hub (`Resources_Design.md` "Hub→Trial door model + PvP options", June; its "Lord" wording is a placeholder).
- **Modes** (Crown, 2026-10-27): **1v1, 2v2, 3v3** and **Fracture**: **any vs any** (uneven sides such as 1v2 or 2v3, or every player for themselves).
- **Pinned for later** (Crown, 2026-10-27): what gear players bring (own builds with a tier cap, uncapped, or preset loadouts), how matches are found, rewards / stakes.
**Notes for Claude:** the co-op plumbing (B-227 onward) should not assume two sides are fixed at 3 or that only PvE fights exist; nothing else to build for PvP yet.

### D-110 — Unify the primary weapon, secondary weapon and adapter (Resonator) loadouts
**Status:** ANSWERED in part (2026-10-29) · **Raised:** 2026-10-29 (Crown) · **Blocks:** —
**Question:** Crown: the primary weapon, secondary weapon and the Resonator's adapter loadout "all do the same thing in different ways"; merge them into **one loadout model**. Weapons are limited to **abilities and elemental abilities** (also valid for adapters, D-108), so the unified approach works; the open issue is **how / where weapons get attached** (crystals, evolutions). Class rules under it: **Generic: 2 weapons; Caster: 1 weapon or 1 adapter; Resonator: its adapter loadout plus a weapon, with the loadout cost rules**, which should **branch out to the other classes**. Ares's trainer adapters must still be portrayable.
**Context:** Crown wants this kind of analysis (unification that cuts code because the rules are known). Analysis: `docs/analysis/UnifiedLoadout_2026-10-29.md` (being written).
**Options:** to come from the analysis.
**Answer (Crown):** (2026-10-29, in progress, one question at a time)
- **Go: A**, the unified model, built incrementally (adapters first) per `analysis/UnifiedLoadout_2026-10-29.md`.
- **Points for everyone (A):** today's costs for every class (plain weapon / adapter 1; with a crystal, stone or fusion 2; weapon with an evolution attached 3; evolution on its own 2; adapter with an evolution 2), and **each class = a budget + a limit**: **Refractor** budget 3, **1 item** (weapon or adapter or evolution) plus its innate spell pool; **Generic** budget 6, up to **2 weapons**, and **keeps one adapter** (B: a Generic may still wear one adapter, as today's ring primary); **Resonator** budget 5, up to **1 weapon**, any number of adapters. Checks: a Refractor with an evolution has no other gear; a Generic may carry two evolved weapons (3 + 3); a Resonator with an evolution has 3 left (e.g. one weapon with an evolution).
- **Class identity:** Generic = weapons, Resonator = adapters, Refractor = innate magic (the only class with an innate element and spell pool; its lever is the innate budget, not more gear).
- **Attachments live on the item (A):** a crystal, stone, fusion or evolution belongs to the weapon / adapter instance and travels with it (saved loadouts, the shared armoury, other characters); the slot only references the item.
- **Adapters cost by state, not form (A):** 1, or 2 with an evolution; the form (ring, pistols, trainers) is look and animation only; a pair (dual pistols) is one item.
- **Note (Crown, 2026-11-08, Q-357 final):** Generic budget 6 → 5 (at most one evolved weapon); the costs are stated as frame + attachment (weapon frame 1, adapter frame 0, crystal +1, evolution on gear +2) and an evolution alone costs 3 (was 2).
- **No primary / secondary difference (A):** a Generic's two weapon slots are equal (same checks, either may hold an evolved weapon, either may be active); fixes the secondary slot's missing spell checks.
- **One active weapon at a time (A):** a Generic's two weapons switch freely; only the active one gives its abilities, stats and passives (no stacking of two evolved weapons).

### D-111 — One owned-items store per player
**Status:** ANSWERED in part (2026-10-29; details one at a time); solo gear sharing and the one party pouch **changed by D-117** (2026-11-03) · **Raised:** 2026-10-29 (`analysis/InventoryArchitecture_2026-10-29.md`) · **Blocks:** —
**Answer (Crown):** (2026-10-29)
- **A: one store per player** holding every owned instance by GUID (gear, evolutions, crystal and stone stacks, outfit pieces), each entry knowing where it is (stored, carried, worn by whom) and whether it is run-only. The **armoury, storage, pouch and run bag are views** of it, not containers. Loadouts and outfits **reference** items (GUIDs), never copy them. The run is a **layer** on top (run-only flag + upgrade memory per definition, D-097). Lyra (inventory + equipment managers), Monster Hunter (box / pouch / equipment box).
- D-110 stands as step one, with three changes: the slot's GUID is the truth (B-302), the kind-agnostic owned list lands in the store (B-308), and run wear is keyed by item GUID, not asset (new job before B-304).
- **Where it lives (A):** in `UPoolSubsystem` now; moves to `AWoRPlayerState` as an owner-only replicated list when co-op lands (Lyra's placement).
- **Presets (A, Crown 2026-10-29):** two kinds: **lore loadouts** (each authored character's own loadout, read-only, from the character asset: what the loadout-match meter compares against and what a skin reveals) and **custom loadouts** (the player's, saved per character as lists of item GUIDs, up to 5 each; an item that is gone or worn elsewhere shows as an empty slot).
- **Run purchase (A, corrected by Crown 2026-10-29):** you take the **real item** (marked on the run, one instance) **as it is at home**, including crystals you attached in the hub; an item that comes with none can be given one during the run (from the pouch, at rest spots). **At run end it resets** to its state before the run (run wear gone, D-093; anything attached during the run comes off); upgrades follow D-097.
- **Per-loadout item lists retire (A):** the 10-slot party pouch (D-097) is the only place carried items live; a loadout carries no items of its own (Monster Hunter: one pouch).
- **Learned spells and abilities stay per character (A):** stored in the player's store with an owner (e.g. "Mend, owner: Orthos"); another character must learn it too. Gear is shared, skills belong to whoever learned them (Q-091; FFX / Dragon Quest).
- **Outfit pieces are store entries shared by the party (A):** an outfit set is six references; a piece unlocked once can be worn by anyone (cosmetic; Monster Hunter layered armour).
- **Give / request (A, Crown 2026-10-29):** skills move between characters by **give** (one copy: the giver loses it) and **request** (ask a teammate for theirs). **Between co-op players** gear, items and skills all move only by give / request (each player's store stays their own, personal loot D-103). **Within your own party (solo)** gear stays shared (D-020).
- **Co-op visibility (default, reference-backed):** what a player has **equipped** is visible to everyone (others see your gear and form); the rest of their store is owner-only until given (Diablo / Destiny: inspect equipped, private stash).
- **Buy for an AI character (Crown, 2026-10-29):** shops and vendors have a **"buy for"** choice that includes AI party members; in co-op **any player can buy for the AI member** (e.g. the third, recruited character), paying from their own wallet; the purchase goes to that AI character to use. **The owner is whoever bought it** (Crown, 2026-10-29): it counts as part of **the buyer's run**: the instance goes at run end, and its progress is remembered for the buyer (the roguelike layer, below).
- **The roguelike layer (Crown, 2026-10-29, corrected):** two families.
  - **Items** (crystals, stones, potions) are **consumables**, like a health potion: a crystal **used** in a run (attached, losing durability) is **used up at run end**; unused items stay in the pouch / storage (D-097).
  - **Gear** (weapons, adapters, evolutions): gear **found or bought during a run** is a run instance, gone at run end, but its **upgrade progress is remembered** per definition, so it **appears at that level the next run** it turns up in. Gear **brought from home** comes back restored (D-093).
  - **Evolutions can break from wear** in a run; a broken one can be **repaired in the run**, and it **resets outside the run** (never lost: Q-159, Q-159c, D-093).
**Notes for Claude:** bugs found by the analysis to fix regardless: run wear keyed by asset (two identical adapters share one wear record); found gear never dropped at run end; player-edited presets not saved (PIE check). Remaining details: where it lives (pool now, PlayerState with co-op), presets as GUID lists per character, the run purchase marks pieces rather than copying, the per-preset 6 x 3 item slots retire, spells / abilities stay per character, outfits as entries, co-op visibility.

### Q-064a2 — Ice break's name, and whether Water's "Frozen" moves to Ice
**Status:** OPEN (lore: Crown's) · **Raised:** 2026-11-09 (B-064a, cloud) · **Blocks:** — (the mechanic ships with the placeholder)
**Question:** What is Ice's break called? Water's break is "Frozen" and Water's damage over time "Frost Bite" today (`WoRGameNames::GetElementBreakName`, `SkillEffectDisplayNames`), names that fit Ice better now that Ice exists. The deckbuilder gives Water the adjective "Tide" and Ice "Frost" (`02-enums.md`, element trait table; "Ice took Frost", Crown, 2026-09-29).
**Context:** B-064a ships the placeholder "Ice break" (`WoRGameNames::GetSkillEffectTypeName(IceBreak)`, `// TODO: Crown`); Water's names are unchanged.
**Options:**
- **A** Water keeps "Frozen" and "Frost Bite"; Ice gets a new name of Crown's choosing.
- **B** Ice takes "Frozen" (and "Frost Bite" if Ice gets a damage over time); Water's break gets a new, tide-like name, matching the deckbuilder's Tide / Frost split.
- **C** Something else.
**Recommendation:** none: names and lore are Crown's (CLAUDE.md, "Recommendations").
**Answer (Crown):** _write here_
**Notes for Claude:** a name is one `LOCTEXT` change (and `ElementBreakNameSpec`'s expected words if Water's change).

### Q-170 — Action pips: the numbers
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088) · **Blocks:** — (B-170a)
**Question:** How many actions at fight start, per turn, at most, and how much does Reflex add?
**Options:** **A** start 1, +1 per turn, Max 2 base, Reflex adds +1 Max at each third of its range (2 → 5), hard cap 5 / **B** start 1, +1 per turn, Max 3 base, Reflex up to 6 (Divinity: Original Sin 2's AP ceiling).
**Recommendation (superseded by Crown's answer below):** A. Crown already set start 1, +1 per turn and base Max 2 (D-088). Octopath Traveler's Boost Points (start 1, +1 per turn, bank up to 5) are the best-liked banked-turn resource in recent turn-based RPGs; its cap of 5 keeps bursts readable. Reflex's share is read like the defense window's, so its gear and stones count with no new maths.
**Answer (superseded by Crown's answer below):** **A**, by the reference-backed default (2026-11-09). Constants `ACTIONS_AT_FIGHT_START`, `ACTIONS_GAINED_PER_TURN`, `MAX_ACTIONS_BASE`, `MAX_ACTIONS_FROM_REFLEX`, `MAX_ACTIONS_CAP`, all `// TODO: tune`.
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** start a fight with your **regain amount**. **Base Max Actions 2.** Max Actions come from **gear, evolutions and the like, not Reflex**: they give **action points**, 100 points = 1 action; items give e.g. +25 or +50 and they add up; only whole actions count (175 = +1), the leftover shows as progress ("Max 3 (+75/100)"). **Reflex sets the regain per turn:** 1, or 2 at a Reflex threshold, with room for 3 at a second threshold; the regain never fills past Max. Constants (`// TODO: tune`): `ACTIONS_REGAIN_BASE` 1, `ACTIONS_REGAIN_REFLEX_THRESHOLD_2` 0.5, `ACTIONS_REGAIN_REFLEX_THRESHOLD_3` 0.8 (share of the Reflex window range), `MAX_ACTIONS_BASE` 2, `ACTION_POINTS_PER_ACTION` 100, `MAX_ACTIONS_CAP` 5 (a safety ceiling only). Plan: `ActionPips.md` §2, §3.1.

### Q-170a — Action pips part 1: how action points and the bank are built
**Status:** ANSWERED (approved by the reference-backed default, 2026-11-09; Crown vetoes) · **Raised:** 2026-11-09 (B-170a) · **Blocks:** —
**Question:** The plan (`ActionPips.md` §2, §3.1) leaves the build details of part 1 open: how action points join the stat composition, whether gear carries them yet, how Reflex's share is read, what replicates and saves, and when the bank is first set.
**Recommendation (reference-backed default):**
- **Action points are their own additive layer set** (`FWoRActionPointLayers`: gear + evolution + transient + debug, never below 0), not a new `ESubStat` entry or GAS attribute. **This differs from the approved plan** (`ActionPips.md` §3.1: "a new stat in the stat composition, one new stat entry"); Crown can veto it, and the stat entry is then a follow-up. Stones fold into the gear layer (a socketed stone is gear), as the plan's "stones" source. Sub-stats are percentages bought with stat points and multiplied (Pattern P); action points are flat and add up (+25 +50 = 75), as Divinity: Original Sin 2's "+1 Max AP" gear and Pillars of Eternity's flat bonuses stack (sums players read at a glance). The plan already puts a GAS attribute later (§3.1).
- **No gear or evolution field yet.** No asset authors action points (§2), so the gear and evolution layers read 0 (`// TODO:`); a later job adds a `BonusActionPoints` line to `FEquipmentStatBonus` and the evolution data with its first authored item. `wor.Actions.AddPoints <N>` adds debug points for PIE.
- **Reflex's share counts Reflex gear:** (`CalculateReflexWindowBonus` × `ReflexWindowGearFactor`) ÷ `WINDOW_CAP_SECONDS`, clamped to 0..1, exactly as the defense window reads Reflex, so Reflex gear, stones and buffs lift the regain with no new maths (Q-170's own recommendation).
- **`CurrentActions` replicates** (as `CurrentEP`); Max Actions is computed where it is read until a source exists that a client may not hold (then it replicates like `MaxEP`, B-131). **Not saved** (plan §6): runtime, per fight (D-064).
- **The bank is first set when a fight starts, by the orchestrator (B-170c)**, through `ResetActionsForFight`; part 1 calls nothing in play (`wor.Actions.Reset` sets it for PIE checks).
- `SpendActions` refuses a cost above the bank (nothing spent); `LoseActions` floors at 0; `GainActions(N, bMayPassMax)` stops at Max unless it may pass, and never trims actions already over Max.
**Answer:** the recommendation, by the reference-backed default (2026-11-09). Built in B-170a (`ActionPipsRules.h`, `UCharacterDataComponent`).

### Q-170b2 — Action pips part 2: how the rotation is built
**Status:** ANSWERED (approved by the reference-backed default, 2026-11-09; Crown vetoes) · **Raised:** 2026-11-09 (B-170b) · **Blocks:** —
**Question:** The plan (`ActionPips.md` §3.2) and Q-170d / Q-170f set the rotation's order. Some build details are left open: what an uneven side does when someone falls mid-round, whether pinned turns take a rotation seat, how first strike's opening round counts, and what the retired speed hooks do with the switch on.
**Recommendation (reference-backed default):**
- **Seats per round** = the larger side's living count at the round's start; the smaller side's rotation carries on across rounds (the deckbuilder, `WoR_TCG_Pivot_Design.md` §8 item 53: 2v3 plays A1 B1 A2 B2 A1 B3, then A2 B1 A1 B2 A2 B3). **A fallen combatant's own turn is purged; a smaller side's extra-seat turn passes to the next living teammate in the same place** (items 55, 56). The next round recounts the living.
- **Pinned turns don't take a rotation seat:** a bonus turn and a ritual's Execution turn are inserted (the deckbuilder's "a bonus turn inserts, a skip removes", §7c), and the holder keeps its own rotation turn. Under the speed timeline an Execution turn cost one slot of the caster's normal priority; in a fixed rotation there is no priority to cost, and the cast already cost its actions (B-170c). Every turn still burns pinned delays (ET-16 as built).
- **First strike's opening round is a round of its own** (`OnRoundAdded` fires at its start and again when the alternation starts), so the balance simulator counts it.
- **The switch is read once per fight**, at `InitializeCombat`: flipping it mid-fight changes nothing until the next fight (no half-switched fight).
- **With the switch on, `DelayNextTurn` (Wind's one-place delay) and `OnActorSpeedChanged` do nothing.** Wind's break takes an action instead, which lands with B-170d; until then the Wind break has no turn effect in rotation mode (switch on only; default play unchanged).
- **"No actions, no turn" (D-088) is not the rotation's job:** the rotation always gives the seat; the orchestrator skips the turn at 0 actions after the turn-start changes (B-170c, §3.3). Emerald on an enemy already removes their next turn through `SkipNextTurn`, which the rotation honours; Emerald's ally actions are B-170d.
- **The first-striking side also leads every later round** (Q-170f says the alternation starts with that side). Visible: a boss that strikes first acts twice in a row (its opening round, then B P1 …); a party that strikes first in 4v4 plays five turns in a row. Crown may prefer the other side to lead after the opening round.
**Answer:** the recommendation, by the reference-backed default (2026-11-09). Built in B-170b (`Combat/TurnRotation.h`, `UTurnManager`).

### Q-170b — Action pips: what a skill costs
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088) · **Blocks:** — (B-170c)
**Question:** D-088's discussion tried costs by tier (1/1/1/2/2/2/3, then F 1 … S 7), then dropped them. With banked actions confirmed, what does an act cost?
**Options:** **A** every Attack, Ability, Refraction and Item costs 1 action; Energy alone prices big skills / **B** by tier, F–D 1, C–A 2, S 3 (so Max Actions unlocks the big skills) / **C** F 1 … S 7.
**Recommendation (superseded by Crown's answer below):** A. The deckbuilder prices skills in Energy only and never in Actions (`docs/reference/06-bonuses.md`, "The turn economy"), and that rule comes first. Two prices on one skill (Energy and actions) is the kind of double gate Crown dropped as over-engineering (D-088, "Simplified"). Banking still matters: Divinity: Original Sin 2's carried-over AP is what made its turns feel tactical. B stays an easy later change (one function from tier to cost).
**Answer (superseded by Crown's answer below):** **A**, by the reference-backed default (2026-11-09).
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** every skill, spell and item has **its own action cost**, default 1, set per asset (big skills can cost 2 or more). **Energy still applies too.** A skill whose cost is above the unit's Max Actions is unusable (greyed). Plan: `ActionPips.md` §1, §3.1, §3.3.

### Q-170c — Action pips: what costs an action, and what ends the turn
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088) · **Blocks:** — (B-170c)
**Question:** Which commands cost an action, and when does a turn end?
**Recommendation (superseded by Crown's answer below) (default):**
- Attack, Ability, Refraction, Item: 1 action each.
- **Defend:** free, and ends the turn (the rest stays banked). Octopath Traveler's Defend also ends the turn and still lets BP build.
- **End Turn** (new): ends the turn, banking the rest (Divinity: Original Sin 2's "end turn" keeps unused AP).
- **Arming a ritual:** 1 action, and ends the turn (ET-16 already ends it).
- **Weapon / ring switch:** the first each turn is free; a second costs 1 action (Crown's own note on ET-19).
- **Row move:** one free per turn (D-096), unchanged.
- **Flee:** free; it ends the fight (D-091).
- The turn also ends at 0 actions; a turn that **starts** with 0 (after the gain) is skipped (Crown, 2026-11-09).
**Answer (superseded by Crown's answer below):** the default, by the reference-backed default (2026-11-09).
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** the turn ends **at 0 actions, on End Turn, or on Defend**; unused actions bank. **Arming a ritual does not end the turn**; it just costs its actions (ET-16 changes). **Weapon switching is always free** (no free-first rule, never costs; ET-19 changes). The row move and Flee stay as in the default. Plan: `ActionPips.md` §1, §3.3.

### Q-170d — Fixed rotation: the order within a round
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088) · **Blocks:** — (B-170b)
**Question:** With Turn Speed gone, who goes when in a round?
**Options:** **A** the deckbuilder's turn belt: sides alternate (P1 E1 P2 E2 …); a smaller side fills the empty seats in turn, carried across rounds; within a side, party-screen order and enemy spawn slot / **B** sides alternate while both have members left, then the rest of the larger side in order (each combatant exactly once a round).
**Recommendation:** A. It is the deckbuilder's rule (`WoR_TCG_Pivot_Design.md` §7c and §8 items 53–56), which comes first. It also answers the classic "a lone boss loses to the party's action count" problem the way D&D 5e's legendary actions do (a boss acts between the heroes' turns; widely used and praised). **Cost:** a lone boss acts 4 times a round against 4 heroes, so boss damage per hit needs a tuning pass. Party first in round 1 unless the enemy struck first.
**Answer:** **A**, by the reference-backed default (2026-11-09). Crown: if a lone boss acting once per hero feels wrong, B is a small change in one function.
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** **approved as planned** (A): sides alternate; a lone boss acts after every hero; boss damage gets a tuning pass.

### Q-170e — Ice: which speed does it slow?
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088, D-151) · **Blocks:** —
**Question:** D-088 first said "Ice slows Action Speed". D-151 split Action Speed and Spell Speed again, and D-090 / B-064a made Ice's break "a heavier Water", which slows both. Which stands?
**Options:** **A** both, as built (Action Speed and Spell Speed, plus Ice's damage) / **B** Action Speed only, so Ice differs from Water by more than its damage / **C** Spell Speed only.
**Recommendation (superseded by Crown's answer below):** A. It is the latest ruling and already in the game (`BreakTable`, `ICE_BREAK_SPEED_SLOW_PERCENT`), so no code changes. Final Fantasy's Slow slows every kind of action and players read it at a glance. With Turn Speed gone, a slow makes the holder's attacks and spells easier to defend against (wider defense windows) and slower to play out.
**Answer (superseded by Crown's answer below):** **A**, by the reference-backed default (2026-11-09).
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** **Ice** slows Action Speed and Spell Speed, **removes 1 action** and deals its damage. **Water** slows **Spell Speed only**. **Wind** slows **Action Speed** and still takes 1 action at the target's next turn. Reading (reference-backed, Crown can change it): Ice's action comes off **at once** (Wind's is the one Crown timed at the next turn). Constants (`// TODO: tune`): `ICE_BREAK_ACTIONS_LOST` 1, `WIND_BREAK_ACTIONS_LOST` 1, `WIND_BREAK_ACTION_SPEED_SLOW_PERCENT` (starts at Water's 30%). Behind `wor.Turns.ActionPips` with the rest. Plan: `ActionPips.md` §3.4; Rules SB-22, SB-23, SB-29 ("Coming with B-170").

### Q-170f — Bonus turns, Extra Action and first strike with action pips
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088) · **Blocks:** — (B-170b, B-170d)
**Question:** Does Emerald's bonus turn (D-052) become +1 action? Does first strike (D-069 / Q-185) become +1 action or stay "act first"?
**Recommendation (superseded by Crown's answer below) (default):**
- **Emerald (Wind's crystal), changed by Crown's Wind rule (D-088, 2026-11-09):** on an **enemy** it makes them **lose their next turn** in the rotation (the built skip, ET-13; S: the next two, Q-341b).
- **Emerald on yourself or an ally: actions, not a bonus turn (Crown, 2026-11-09).** Lower tiers give +1 action after a set number of the holder's turns; S gives 2 to 3 actions at once. Ladder (reference-backed default, all `// TODO: tune`): F +1 after 6 turns, E 5, D 4, C 3, B 2, A 1 (its next turn); S +2 to +3 now, rolled per crystal. Why: it reuses Emerald's own former delay table (F 6 to S 0, Q-341b), so the tier spacing is one this game already had; an immediate +2 echoes Divinity: Original Sin 2's Adrenaline (+2 AP now, a well-liked tempo tool), topped at 3 like Octopath Traveler's maximum boost. A delayed gain lands in step 2 of the turn-start order (ActionPips.md §1), before the 0-action check. The S gain may pass Max Actions for that turn (Q-170g). Replaces CC's earlier "keeps its bonus turn" reading.
- **The Extra Action effect becomes +1 action now** (what its name says; the deckbuilder: "an Action effect arrives in the pool at once").
- **First strike stays as built:** the striking side takes its whole first round, then the alternation starts with that side.
**Answer (superseded by Crown's answer below):** the default, by the reference-backed default (2026-11-09), except Emerald on yourself or an ally: **Crown, 2026-11-09: actions scaled by tier** (above; the ladder numbers are the reference-backed default).
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** **keep both effects:** **Gain Action** (+1 action now, within the turn; the new `GainActions`) and **Extra Turn** (a whole bonus turn, as built). The existing **Extra Action** effects **become Extra Turns** (they keep their bonus turn; the name changes; ET-15 stays). **Emerald on yourself or an ally** (replaces the 6..1 ladder): F +1 action in 2 turns; E +1 next turn; D +1 now; C +2 in 2 turns; B +2 next turn; A +2 now; S +3 now. **Emerald's actions may go over Max.** On an enemy, Emerald still removes their next turn. Constants (`// TODO: tune`): `EMERALD_ALLY_ACTIONS_GAINED` {1,1,1,2,2,2,3}, `EMERALD_ALLY_GAIN_DELAY_TURNS` {2,1,0,2,1,0,0} (F..S). First strike: as in the default. Plan: `ActionPips.md` §3.4.

### Q-170g — Effects that give or take actions
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088, Crown's note of 2026-11-09) · **Blocks:** — (B-170d)
**Question:** Which effect types, and can a gain go past Max Actions?
**Recommendation (default):** four new effect types:
- `GainActions` (+N, now or at the holder's next turn start);
- `LoseActions` (−N, same two timings, never below 0);
- `MaxActionsBuff` / `MaxActionsDebuff` (±N Max for N holder turns, D-051).
- Crown's "+1 now, −1 next turn" is one skill with two rows, exactly Divinity: Original Sin 2's Adrenaline (+2 AP now, −2 next turn), a well-liked tempo skill.
- Wind's break becomes `LoseActions` 1 at the next turn start (D-090's placeholder).
- **A gain "now" may go past Max for this turn;** (superseded: the extra now stays banked, see Crown's answer below) the bank is trimmed to Max when the turn ends, so "+1 action" is never wasted at a full bank (CC's call).
**Answer:** the default, by the reference-backed default (2026-11-09).
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** **approved:** gain actions, lose actions (now or next turn), raise or lower Max Actions for N turns. A gain may pass Max and the extra **stays banked until spent** (follows Crown's "Emerald's actions may go over max"; the default's trim at turn end is dropped; a reading to confirm); the regain adds nothing at or above Max.

### Q-170h — Reflex's two jobs, and Spirit's Turn Speed slot
**Status:** ANSWERED (Crown, 2026-11-09) · **Raised:** 2026-11-09 (B-170, D-088) · **Blocks:** — (B-170a, B-170g)
**Question:** Reflex widens the defense window today. Does it keep that job as well as setting Max Actions? And Turn Speed is one of Spirit's five sub-stats: what fills its place?
**Options:** **A** Reflex keeps both jobs; Spirit keeps four sub-stats (Turn Speed retired, its points inert, data validation flags them) / **B** Reflex sets Max Actions only, and the window loses its stat / **C** move Max Actions to a Spirit sub-stat.
**Recommendation (superseded by Crown's answer below):** A. One "quick" stat that acts and reacts more reads naturally, and none of Reflex's gear, stones or buffs need reworking. Spirit loses no value in practice: Max Energy now also sets the per-turn Energy refill (D-089 as changed, B-173). Crown already chose Reflex for Max Actions (D-088), so C is out.
**Answer (superseded by Crown's answer below):** **A**, by the reference-backed default (2026-11-09). Watch in play: if Reflex becomes a must-have, split it (B).
**Answer (Crown, 2026-11-09, supersedes the default above where they differ):** **approved:** Reflex = the regain amount + the defense window (Reflex no longer sets Max Actions, Q-170); Turn Speed removed from Spirit.

## Parked

### D-P01 — Move Broken Darkness onto GAS?
**Status:** PARKED until after GAS stage 3 (`GASMigration.md`, decision 1: option C). Only with a zero-difference `wor.CompareBD`.

---

### D-171 — Hidden quests: found in secret, then tracked in plain words
**Status:** DECIDED (Crown, 2026-11-09, desktop operator session) · **Raised:** 2026-11-09 · **Blocks:** — (B-397)
**Answer (Crown)** (model: Shangri-La Frontier's Unique Scenarios; changes D-100's and HiddenQuestSystem's "no text ever says what the next step is"):
- **Starting a hidden quest stays fully hidden:** no hints, no listing, no text about how to trigger it (D-100's "no hints" stands for the start).
- **Once a player triggers it,** that player alone gets a private notice, **"Hidden quest discovered: <title>"** (D-100's private unlocks), and the quest appears in their journal's **Quests** page.
- **From then on, the current step's conditions are shown in plain words** (the step's `Goal` text, the journal list `GetVisibleQuests` from B-388). **Only the current step** is shown; later steps stay hidden until reached. **The end memory and its rewards stay hidden until earned.**
- **Co-op:** each player sees only their own discovered hidden quests.
**Notes for Claude:** hidden quest assets need a `Title` and a `Goal` per step (designer-written `FText`; both fields exist since B-388; placeholder text is fine until Crown writes them). Job: B-397.

---

### D-170 — Visual effects and parry rules: weapon VFX fixed, two guards (weapon / unarmed), crystal parry effect
**Status:** DECIDED (Crown, 2026-11-09, desktop operator session) · **Raised:** 2026-11-09 · **Blocks:** —
**Answer (Crown):**
- **Weapon attack VFX are fixed** per weapon.
- **Adapter and spell-cast VFX:** each adapter type has a default (rings are the default). There is **no per-adapter override**; adapters share the character's own setting with Refractors and Generic.
- **Generic with both a weapon and an adapter:** the class preference decides which VFX shows.
- **Two guards:**
  - The **unarmed guard** (the body) is the default for adapter users, Refractors and Generic, and can be customised per character.
  - The **weapon guard** is that weapon type's own and can't be customised.
  - A per-character setting picks which is used: with a weapon equipped, either parry with it or put it away and parry unarmed. Equipping a weapon makes the weapon guard the default.
- **Landing a parry** (corrected by Crown, 2026-11-09): there is a **parry-effect cosmetic** (e.g. a magic circle), one for the weapon guard and one for the unarmed guard. Weapons always have the default sparks; unarmed has nothing by default. The equipped cosmetic plays on a landed parry, coloured by the **element behind it**: a Refractor's source element, a Resonator's active adapter, or a weapon's own element or its crystal. With no element behind it, unarmed shows nothing. When more than one source applies, a **loadout setting** says which takes priority.
- **Parry timing** is identical in both guards: looks only (reference-backed default: Expedition 33's parry plays the same for every character).
- **There is no separate parry-style cosmetic** beyond the unarmed guard's customisation (Crown changed this the same day).
**Notes for Claude:** plan in `docs/Design/VisualGameplayPlan.md`; jobs B-391 to B-395.
