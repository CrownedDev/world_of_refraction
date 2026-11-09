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

**Confirmed by Crown (2026-11-09):** the fixed rotation and banked actions stay (Turn Speed goes; Reflex sets Max Actions). Next is B-170's plan doc (`docs/analysis/ActionPips.md`). Open for the plan: Speed is split again into Action Speed and Spell Speed (D-151), so say which one Ice slows. **Also (Crown, 2026-11-09):** effects can **give or take actions** (e.g. +1 action now, -1 action next turn), alongside Wind's pip loss; the plan lists them as effect types.
### D-089 — Should Energy refill each turn?
**Status:** DECIDED, BUILD (Crown, 2026-11-09: changed from C, see the change below) · **Raised:** 2026-10-26 (rules review) · **Blocks:** —
**Question:** EP never refills on its own today (ET-29: only effects, items, drains and Broken Darkness absorption). Should it?
**Context:** Max Energy (Spirit) is the pool size; Efficiency (Mind) lowers costs. Deckbuilder: +1 Energy per turn, banked, no cap. Expedition 33: AP +1 per turn plus more from basic attacks and parries. With action pips (D-088), a basic attack that returns Energy becomes a use for spare pips.
**Options:** A no refill (current) / B a share of Max Energy each turn (e.g. 10%; Max Energy then also sets refill) / C Expedition 33 style: a small refill each turn **plus** Energy from basic attacks and Perfect Guards.
**Recommendation:** C, because it rewards good play (timing gives Energy back), gives basic attacks a purpose, keeps casters from running dry, and needs no new stat. Broken Darkness keeps its own absorption system.
**Answer (Crown):** **C** (2026-10-26): Energy **refills a little at the start of each of your turns**, plus extra from **basic attacks** and **Perfect Guards** (Expedition 33 style). Amounts are named constants (`// TODO: tune`); the per-turn share can scale with Max Energy. Broken Darkness keeps its own absorption system (no passive refill, ET-29 for BD).

**Changed by Crown (2026-11-09):** Energy refills **only at the start of each of your turns** (a share of Max Energy, a named constant, `// TODO: tune`). Energy from basic attacks and Perfect Guards is **not** a base rule: it comes from **gear effects** (passives) where an item grants it. Broken Darkness keeps its own absorption (no passive refill). Job: B-173.
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

## Parked

### D-P01 — Move Broken Darkness onto GAS?
**Status:** PARKED until after GAS stage 3 (`GASMigration.md`, decision 1: option C). Only with a zero-difference `wor.CompareBD`.

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
