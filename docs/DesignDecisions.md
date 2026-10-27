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

---

## Open

### D-001 — What does stun do?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-061
**Question:** Does a stunned character skip the turn, or can it still Attack / Defend?
**Context:**
- `ActionExecutor::ValidateAction` blocks every action while stunned (`ActionExecutor.cpp:167`). The enum comment says only "Attack/Defend" are allowed (`ESkillEffectType.h:122`).
- Arc A makes stunned actors pass their turn automatically.
**Options:**
- A: skip the turn entirely (current, after Arc A).
- B: Attack and Defend allowed.
- C: Defend only.
**Recommendation:** A. Simplest, and readable for the player.
**Answer (Crown):** **B** (2026-10-26). A stunned character can still **Attack or Defend**; spells, abilities and items are blocked.
**Notes for Claude:**
- `ActionExecutor::ValidateAction`: stun blocks Spell/Ability/Item but allows the basic attack and Defend. "Basic attack" = the loadout's current attack (`GetCurrentAttack`), not any ability.
- Remove the turn-start auto-pass for stunned actors (Arc A, `ProceedWithTurnStart`); keep the invalid-action fallback as the safety net.
- Command menu shows only Attack + Defend while stunned; AI `BuildAction` picks attack or defend while stunned (fallback already does).
- Update `WoR.Combat.HangFixes` stun test (it asserts the old skip rule) and the `ESkillEffectType.h:122` comment stays true.

### D-002 — What happens after winning vs losing a battle?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-072
**Question:** Where does each outcome send the player?
**Context:** Today both return to the trial, which reloads with the enemy respawned (`BattleGameMode.h:11-13`). The results screen now exists (branch B).
**Options:**
- A: Win → back to the trial with the enemy gone; Lose → back to the hub.
- B: Lose → retry the fight.
- C: Lose → back to the trial at the last checkpoint.
**Recommendation:** A for now. Needs the persistence work (B-090) for "enemy gone".
**Answer (Crown):** **Trials are runs, Elden Ring Nightreign style** (2026-10-26), as already set out in `Design/Resources_Design.md` (one hub, trial = run).
- **Win a fight** → continue the trial; that enemy stays beaten.
- **Lose a fight** → the run ends; back to the hub.
- **Lost at run end:** run world-stat levels, Gold, gear found in the run, slotted crystals (consumed).
- **Kept:** the pool (everything unlocked) **with its upgrade levels**, Prisms/Diamond, head-start allocation.
- **Upgrades happen both in the hub and during a run**, and an item's level is what you get the next time you bring or find it ("you find it again at the same level you had it last time").
**Notes for Claude:**
- **Costs stay as the code has them** (Crown, 2026-10-26): upgrading (tier-up) costs leveling essence + ½ Reality **anywhere** (hub or run, `EconomyService::TryLevelUpEntry`), never Gold. Only *buying* differs by place: Prisms in the hub, Gold in a run.
- Interpretation to confirm with Crown when applying: an upgrade made *during* a run is written back to the pool (kept). If unsure, ask before building it.
- "Enemy stays beaten" and run state across level loads need the persistence work (B-090); routing (lose → hub) can land first.
- Co-op revive on defeat: not decided yet (later, with co-op).

### D-003 — How do recruited party members get into battle?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-071
**Question:** How do they get from the party list into a fight?
**Context:**
- The battle roster is built only from actors standing in the arena sphere (`EncounterComponent.cpp:182-197`). Recruits are never spawned into the trial.
- All pawns spawn as `DefaultPlayerPawnClass` (`BattleGameMode.cpp:133`).
**Options:**
- A: taken from the party list when the encounter starts, each spawned with its own `FPartyMember.PawnClass`.
- B: recruits follow the player around the trial and join via the sphere.
**Recommendation:** A. No follower AI is needed, and it works for co-op later.
**Answer (Crown):** (2026-10-26) **Both, by who the member is:**
- **Party members (characters):** you build your team in the hub before a run, and you can also **find characters during a trial** and add them. They join a fight from the party list when it starts (option A): spawned next to the leader, each with its own `FPartyMember.PawnClass`.
- **Human players (co-op):** the radius system (option B). Only players whose radius overlaps the encounter are pulled into the fight.
**Notes for Claude:** - Single-player now: build A (spawn from the party list at encounter start, `EncounterComponent.cpp:182-197`, `BattleGameMode.cpp:133`). Keep the sphere-overlap gather for player pawns so co-op can use it later; don't delete it.
- Finding characters in a trial: the recruit flow already exists in the hub; reusing it inside a trial is a separate job (B-076). Whether a character found in a run stays after the run is open (D-026).
- Party size, slots and leader rules: as in the party system today (`docs/Architecture/PartySystem.md`).

### D-004 — Are non-leader party members AI-controlled in battle?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-071
**Question:** Who acts for party members other than the leader?
**Context:** Only PC0's pawn is player-controlled; the others run on AI (`CombatOrchestrator.cpp:1198-1209`).
**Options:**
- A: the player controls every party member on their turn (Expedition 33 style).
- B: AI companions.
**Recommendation:** A, because it's the genre standard. AI control stays for co-op ghosts.
**Answer (Crown):** (2026-10-26) **Your choice, per party member:** you can control a member yourself on their turn, or let the AI play them.
**Notes for Claude:** - Add a per-member control setting (Player / AI) on `FPartyMember`, set on the hub party screen; default Player. The leader is always Player.
- Player-controlled members use the normal command menu on their turn; AI members use the existing AI path (`CombatOrchestrator.cpp:1198-1209`, which today keys on PC0's pawn only).
- Whether the setting can be changed mid-fight is open (D-026).

### D-005 — Should elements change damage?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-064
**Question:** Weakness/resistance multipliers, or are elements status-buildup only?
**Context:** `ElementMultiplier = 1.0f` always (`DamageCalculator.cpp:207-208`).
**Options:**
- A: status-only (current).
- B: a weakness wheel.
- C: per-character resistances only.
**Recommendation:** C. A light touch that keeps the buildup system as the star.
**Answer (Crown):** **C** (2026-10-26). The resistance table, which already sets each character's starting weaknesses, changes **damage as well as status buildup**. Crown notes the table's structure could be improved, as with other systems.
**Notes for Claude:**
- Use the same composed resistance the buildup path reads (class table `ClassInnateResistanceTable.h` + gear sources, `docs/Architecture/ResistanceSystem.md`) to set `ElementMultiplier` (`DamageCalculator.cpp:207-208`): `1 - resistance × ElementDamageResistanceScale`, clamped. The scale is a named constant, `// TODO: tune` (a lighter touch than buildup).
- Element column only for now. If the physical columns (Slash/Pierce/Impact) would double-count with Defense, leave them out and say so in the hand-off.
- Tests: weak takes more, resistant takes less, neutral unchanged, `None` element unchanged.
- The table restructure is its own job (B-095), not part of B-064.

### D-006 — Where do Prisms come from?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-073
**Question:** What grants Prisms in C++?
**Context:** Nothing in C++ grants Prisms or Gold; the wallet starts at 0 (`CurrencyComponent.h:129-145`). The deckbuilder shows "+120 Prisms" on victory.
**Options:**
- A: battle rewards scaled by difficulty.
- B: plus trial completion bonuses.
- C: plus a starting grant.
**Recommendation:** A + C.
**Answer (Crown):** **A, Nightreign style** (2026-10-26; changed from Hades style the same day, to slow down upgrading). **Prisms are paid when a run ends, win or lose**, more the further you got and the harder the difficulty. The same goes for upgrade materials earned in a run. Later, **passive effects can raise the payout** (e.g. "+10% Prisms at run end").
**Notes for Claude:**
- **Prisms:** tally per run (fights won, how far you got, difficulty) and bank it once at run end, on a win or a failure. Not banked during the run. Named constants, `// TODO: tune`.
- **Upgrade materials** (leveling essence etc.) earned *in a run* follow the same rule: paid at run end. Essence from dismantling stays as it is. If the code makes this split unclear, raise a decision rather than guess.
- **Gold:** unchanged. Every battle won in a run grants Gold, scaled by difficulty; spent at trial vendors; lost at run end.
- Results screen shows "+N Gold" per fight; the run-end screen shows the Prism/material payout.
- Passive payout boosts are a later job (B-096): a hook where the payout total is computed (one multiplier, default 1.0) so passives can add to it.
- Not decided here: a starting Prism grant for a new save.
### D-007 — Can Generic cast weapon-crystal spells?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-080
**Question:** Is it intended that Generic can cast spells from a crystal on its weapon?
**Context:** The menu shows them to any class (`FCombatCapabilities.cpp:62-68`), but `UCharacterData::CanUseSpells()` is false for Generic.
**Options:**
- A: no, block it.
- B: yes, it's Generic's only spell access.
**Recommendation:** Crown's call; it defines Generic's identity.
**Answer (Crown):** (2026-10-26) **A for spells, plus a damage type on skills.** Generic can't cast spells from a weapon crystal. Generic gets elements on its abilities **only through a crystal**. _(Clarified by D-027, same day: weapons never give spells to any class; Generic **can** cast spells through rings.)_
- Every skill has a **damage type**. A physical skill starts as **Physical**; an infusion (a crystal / chosen source) changes its damage type to that **element**.
- The same applies to **Generic spells** (non-Generic classes): they take the element of their source.
- Crown: the deckbuilder explains this well. It is the simplest level of this same game, so its rules are a good reference.
**Notes for Claude:**
- Weapon-crystal spells go for every class (D-027, B-098); ring spells stay available to Generic. Don't apply the old "Generic sees no spells" gate.
- **Fix found while checking:** an infused ability takes the user's **innate** element (`ActionExecutor.cpp:1450-1455`), but Generic, the only class with abilities, has no innate element (`ECharacterClass.h` `HasInnateElement`), so a Generic's infused ability deals **no element** today. The status path already uses the chosen source (`ActionExecutor.cpp:2253`). Damage must use the selected source's element too (crystal on the weapon), else Physical.
- Deckbuilder reference (`wor-deckbuilder/docs/reference/01-card-model.md` "Cast through", Pivot "Generic element memory"): a Generic Ability starts Physical and takes its source's element; a Generic Refraction needs an elemental source. Its extra costs: the switch cost is **not adopted** (Crown, 2026-10-26, rules review: no switching cost; pick the source fresh each use); the extra Energy for a physical ability dealing an element is D-035.
- Tests: Generic + fire crystal → infused ability deals Fire; uninfused → Physical; Generic with a ring can cast its spells.

### D-008 — Do gear effects update when swapping mid-combat?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-081
**Question:** When a weapon is toggled or a ring switched mid-fight, does the new gear's effect apply?
**Context:** Effects are armed once at combat start (`CombatOrchestrator.cpp:1163-1170`). After a swap, the old effects stay and the new ones never apply.
**Options:**
- A: re-gather on swap (old effects off, new on).
- B: locked at combat start.
**Recommendation:** A. Otherwise the Resonator ring switch is cosmetic.
**Answer (Crown):** **A** (2026-10-26): effects follow the swap. (Crown believed this was already built; re-checked the same day, it isn't, see notes.)
**Notes for Claude:**
- Checked 2026-10-26: gear **stat bonuses** already follow a swap (`ToggleEquipment()` / `SetActiveRingIndex()` refresh cached stats, `LoadoutSystem.md`), but gear **effects** don't. They are applied and armed once in `PrepareAllLoadoutsForBattle` (`CombatOrchestrator.cpp:1181-1197`), and `SkillEffectSystem.md` says "No mid-combat re-application". Nothing listens to a swap.
- On a weapon toggle or ring switch: remove the old gear's starting effects and disarm its conditionals, then apply/arm the new gear's (same `GetActiveEffectsGathered` / `GetActiveConditionalEffectsGathered` calls). Don't touch effects from skills or enemies.
- Tests: swap away removes the old ring's effect; swap back restores it once (no stacking).

### D-009 — Do stones and fusions on rings work?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-082
**Question:** Should stones on rings have any effect?
**Context:** Fusions are allowed on rings (`RingData.cpp:40-44`), but every stone read goes through the active weapon only. Ring stones are inert.
**Options:**
- A: rings' stones count.
- B: disallow stones on rings.
**Recommendation:** A, for Resonators.
**Answer (Crown):** **A** (2026-10-26): on a ring, the stat-stone half of an elemental fusion counts.
**Notes for Claude:**
- Already settled, checked 2026-10-26: rings reject bare augment stones and augmented (two-stone) fusions, and accept only elemental fusions (`RingData.cpp:30-44`); the crystal half gives the ring its spells (`AugmentStoneSystem.md` "Rings + AugmentStones").
- To build: the stone half's **attached form** (the permanent `+X%` self-passive to the holder) applies from a ring too. Today stone reads go through the active weapon only.
- Which rings count: the same rule gear effects use (the Resonator's active ring, a primary ring), unless D-008's swap rule says otherwise; stays consistent with B-081.
- Tests: ring with a DefenseStone + Fire crystal fusion raises Defense and still offers the Fire spells.

### D-010 — Resonator ring rules
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-083
**Question:** "Up to 3 of the same element": enforce it? And is the ring cap 5 or 6?
**Context:** Only a budget of 5 is enforced (`LoadoutConstants.h:31`). The comment says 6 (`ECharacterClass.h:24`).
**Answer (Crown):** (2026-10-26) **Keep the budget of 5; drop "up to 3 of the same element" (B).** Any mix of ring elements; the budget is the only limit.
**Notes for Claude:**
- No rule change in code: the budget of 5 (`LoadoutConstants.h:31`, primary + rings) already stands, and no same-element cap exists.
- Fix the stale class comment (`ECharacterClass.h:24-29`: "6 slots, up to 3 same element") to describe the budget of 5. Also check the rest of that comment block against D-027 (e.g. "2 physical weapons", "1 weapon") and correct it.

### D-011 — Once-per-match effects: per character or per match?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-062
**Question:** If two enemies wear the same "once per match" gear, does each get its own use?
**Context:** It's tracked per effect ID (`SkillEffectManager.h:423`), so the second wearer is blocked.
**Recommendation:** Per character.
**Answer (Crown):** **A, once per character** (2026-10-26). Each wearer gets their own use.
**Notes for Claude:** key `FiredOnceThisMatch` (`SkillEffectManager.h:423`, checked at `SkillEffectManager.cpp:100`, set at `:261`) on (actor, EffectID) instead of EffectID alone; still cleared by `ResetForNewCombat()`. Matches the deckbuilder (a card's fires-once belongs to that card). Test: two wearers of the same once-per-match gear each fire once.

### D-012 — Repeated status caps: refresh or stack?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-062
**Question:** When a bar caps again (Burn twice), refresh the existing effect or add a second?
**Context:** Random IDs make it stack today (`SkillEffectManager.cpp:2405`).
**Recommendation:** Refresh.
**Answer (Crown):** **C, the deckbuilder rule** (2026-10-26): the same source again **refreshes** the effect it already put on you; a different source **adds** its own.
**Notes for Claude:**
- Today every capped-bar effect gets `EffectID = FMath::Rand()` (`SkillEffectManager.cpp`, `ApplyTriggeredSkillEffect`), so they always stack.
- Identity = (status type, element, source actor). Same identity → reset the duration (and re-snapshot the source's boost, as the deckbuilder's DoT rule does: `04-effects.md` DoT row); different source → a new effect.
- Tests: same attacker Burns twice → one Burn, timer reset; two attackers → two Burns.

### D-013 — Do status bars carry over between fights?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** —
**Context:** Arc A resets them at every combat end.
**Recommendation:** Reset (current after Arc A).
**Answer (Crown):** **A, reset every fight** (2026-10-26). Status bars start empty in each fight. HP, EP, absorption and durability carry over within a trial run (souls-like attrition); status bars don't.
**Notes for Claude:** already the behaviour since Arc A (reset at combat end); nothing to build. Keep it when the effects overhaul / GAS stage 3 lands.

### D-014 — What does Wind's "skip turn" do?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-063
**Question:** Wind's bar cap stores SkipTurn but nothing enforces it. Is it the same as stun?
**Recommendation:** Pass the next turn (like stun), distinct name and icon.
**Answer (Crown):** **B, pushed back in the turn order** (2026-10-26). A full Wind bar knocks the target later in the queue; it isn't a stun (Lightning owns Stun).
**Notes for Claude:**
- Checked: the Wind cap resolves to `SkipTurn` (`BarCapTriggerResolver.h`), which nothing enforces; Lightning resolves to `Stun`.
- Build it as a turn-order delay through `UTurnManager` (it already re-sorts on speed changes: `SkillEffectManager::NotifySpeedChanged` → `OnActorSpeedChanged`). How far back is a named constant, `// TODO: tune`. Visible on the turn strip.
- Keep the `SkipTurn` enum member (append-only); give it this meaning, with its own name and icon (e.g. "Gusted"; naming is Crown's, ask if unsure).
- Tests: a full Wind bar moves the target later in the next turn order; it still acts that round.

### D-015 — Should cleanse remove your own gear effects?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-065
**Context:** On-hit gear effects count as debuffs, so `CleanseSelf` strips them. Iolite also strips an enemy's permanent gear buffs, and nothing re-applies them (`ESkillEffectType.h:391-393`, `ItemExecutor.cpp:1029-1041`).
**Recommendation:** Cleanse removes only effects applied by others; gear effects are immune.
**Answer (Crown):** **A** (2026-10-26). Cleanse (and Iolite) only removes effects **someone else applied**; gear effects are immune, yours and the enemy's.
**Notes for Claude:**
- Gear effects (applied via `ApplyEquipmentEffects` / armed conditionals) need a "from gear" mark on the active effect; `CleanseSelf`/`CleanseAllies` and Iolite (`ItemExecutor.cpp:1025-1045`) skip marked effects.
- Leave `IsDebuff()`'s classification (`ESkillEffectType.h:386-396`) alone; the exemption is by origin, not type.
- Tests: cleanse removes an enemy's Burn but keeps your "attacks apply Burn" ring; Iolite on an enemy keeps their gear buffs.

### D-016 — Can equipped items be dismantled?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-084
**Recommendation:** No. Block it with a message.
**Answer (Crown):** **C, with a warning** (2026-10-26): "This is equipped in loadout N. Dismantle anyway?", then it's unequipped and dismantled. Crown: **a Generic or Resonator can't enter combat with no gear.**
**Notes for Claude:**
- Checked: dismantling destroys the item for Gear essence by tier (`EconomyService::DismantleWeapon/DismantleRing`), with no equipped check, and loadouts hold copies (B-085), so a dismantled item stays usable in its loadout.
- Dismantle: if the item is in any saved loadout, require a confirm (placeholder UI per the PIE rule), then remove it from those loadouts, then dismantle.
- **Dismantling loses the item's upgrades for good** (Crown, 2026-10-26): it leaves the pool with its upgrade levels; getting the item again starts it from scratch. The warning says so ("You'll lose its upgrades").
- If that leaves a Generic or Resonator with no gear, say so in the warning ("X will have no gear and can't fight until you equip something").
- Combat entry: a Generic or Resonator with no gear can't enter a fight. A Refractor can (innate). Today an empty primary is valid (`FSavedLoadout.cpp` `EPrimarySlotType::None`). A gearless party member is **left out** of the fight (Crown, 2026-10-26); the fight still starts with the others (B-071).
- Tests: dismantling an equipped weapon removes it from the loadout; a gearless Generic is refused at fight start.

### D-017 — Header bar style (UI)
**Status:** PARKED until the B-101 mockup · **Raised:** 2026-10-25 · **Blocks:** UI work (later)
**Question:** Which of the four deckbuilder candidates (`UI_DeckbuilderReuse.md` §3)?
**Answer (Crown):** (2026-10-26) Pick from pictures: B-101 draws all four candidates (plus a crystal-themed one, see D-024) in the combat HUD mockup, and Crown chooses there. **Drawn (B-111, 2026-10-27):** `combat/hud/v2/header_candidates_annotated.png` shows A-E; Crown picks in B-102.

### D-018 — Art pack licences for Unreal
**Status:** DECIDED · **Raised:** 2026-10-25 · **Blocks:** asset import (later)
**Question:** Confirm each pack in the deckbuilder's `CREDITS.md` allows Unreal / commercial use.
**Checked (2026-10-26, from the deckbuilder's `CREDITS.md`; the vendor licence files aren't in git):**
| Pack | Unreal + commercial game? | Catch |
|---|---|---|
| Spectral SC, Lato (fonts) | Yes (SIL OFL) | Files may be committed and shipped |
| PONETI Classic Fantasy UI Kit, Fantasy Icons Megapack | Yes: Unity says Asset Store assets may be used in other engines under the standard EULA ([Unity support](https://support.unity.com/hc/en-us/articles/34387186019988-Can-I-use-assets-from-the-Asset-Store-with-other-engines)) | No redistribution; players mustn't be able to extract raw files. **Check** each store page says "Standard Unity Asset Store EULA" (no extra publisher terms) |
| nulllgames Gems & Wood kit | Yes (author's comment: any commercial use, any media) | Terms are only a comment; screenshot it |
| godboyhappy HD Spell VFX (8 packs) | Yes (custom licence: commercial games, modify freely) | No resale/redistribution of the images. Store link missing in CREDITS |
| Dumivid Mechanized Magic, dobo Casual UI, CafeDraw Fantasy Card Assets | Yes (CC0) | none |
| Kitsinu Galaxy Backgrounds | Yes (royalty-free, commercial games) | No redistributing as a pack |
| Squibbls Card Backs | Yes | No redistribution even modified |
| AD Sounds Cards Vol. II | Yes (commercial games) | Don't remix or claim the files |
| Stormwave Glass SFX Vol. I | Yes (royalty-free, bought) | The agreement PDF wasn't in the download: **get it** |
All of them allow use inside the game; none allows sharing the raw files. "Save what we use" in this **private** repo is fine; never make it public.
**Answer (Crown):** (2026-10-26) No further checks: Crown is confident the packs are fine. Keep the repo private (no raw files shared).

### D-019 — Where does the mockup generator live?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** —
**Question:** Copy `tools/ui_drafts/` into this repo, or keep it in wor-deckbuilder?
**Answer (Crown):** **Answered by D-025 = A** (2026-10-26): the tool is copied into this repo (`Tools/ui_drafts/`).
**Notes for Claude:** Nothing to apply beyond B-100.

### D-020 — Is the account pool shared across characters?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** B-091
**Question:** Is the owned pool shared by all characters, or per character?
**Context:** `InitializeFromPool` copies the whole pool into one pawn (`InventoryComponent.cpp:1446-1503`). With 3 party members, each would get duplicates.
**Answer (Crown):** **A, one shared pool per player** (2026-10-26). All your characters draw from the same collection; an item you own once can be equipped by one character at a time.
**Notes for Claude:**
- Fits D-026 (one shared run inventory for the party). The pool is account scope (`APlayerState` or the save, `CurrencySystem.md` routing note); characters equip *from* it, they don't each copy it.
- Fix `InitializeFromPool` copying the whole pool into each pawn (`InventoryComponent.cpp:1446-1503`).
- **Gear presets (Crown, 2026-10-26):** you can preset each character's gear (their saved loadouts, `UInventoryComponent::SavedLoadouts`), drawn from the shared pool. Presets reference pool items (B-085), never copies.
- An item can sit in several characters' presets; only one can wear it at a time. Suggested clash rule (not ruled; confirm when building): the party screen flags the clash and you pick who wears it, before the run or fight starts.
- Tests: a party of 3 with one Fire Sword has exactly one; equipping it on a second member doesn't duplicate it.

### D-021 — How do you get your loadout at the start of a run?
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** B-091
**Question:** Random draft of 3 loadouts (as `Resources_Design.md` §draft says) or something else?
**Answer (Crown):** **Purchase model, option A** (2026-10-26). No RNG draft: at run start you **buy** your loadout from **your pool** with a **starting Gold budget**.
- Prices scale with tier and level, so the budget forces choices (one great weapon vs a balanced kit).
- **Unspent Gold carries into the run** (trial vendors: upgrades, world stats).
- Account perks can raise the starting budget; slot limits still apply (1 weapon / 2 rings / 1 evolution early, growing with perks).
- Crystals are still consumed per run.
**Notes for Claude:** supersedes the "3 loadouts spawn (the draft)" mechanic in `Resources_Design.md` (update that section when applied); the Luck-weighted drop curve stays for **in-run finds**, not the loadout. Budget size and price curve are tuning values → named constants with `// TODO: tune`.

### D-022 — Were the 16 asset deletions in commit 41c3e86d intended?
**Status:** DECIDED: applied and confirmed by Crown, 2026-10-26 (row in Decided) · **Raised:** 2026-10-26 · **Blocks:** nothing
**Question:** Keep or delete the old combat/shop widgets and `BP_CombatGameMode` that commit `41c3e86d` removed?
**Context:**
- `41c3e86d` is titled "docs: deckbuilder UI reference (reference only)". It also deleted 16 `.uasset`s: `WBP_CharacterPanel` (×2), `WBP_EffectRow`, `WBP_Enemy/PlayerPanelContainer`, `WBP_CombatMenu`, `WBP_CombatMenuButton`, `WBP_DurabilityHeader`, `WBP_TurnOrderSlot/Strip/StripContainer`, `WBP_ShopWindow`, `WBP_CartRow`, `WBP_StockRow`, `WBP_HUDTestActor` and `BP_CombatGameMode`. It also modified ~15 character/weapon data assets, `TestLevel_Nav`, `BP_CombatOrchestrator` and `BP_HubPlayerController`.
- These deletions are already on `main` and `dev`. `refactor/bp-logic-to-cpp` still has the assets, and its native HUD drives them (CharacterPanelWidget, turn-order slot binding).
- Merging `dev` into it (B-002) would apply the deletions. It also conflicts in `CombatOrchestrator.h/.cpp`, `BattleGameMode.cpp` and `CharacterPanelWidget.h/.cpp`. The merge was aborted untouched.
**Options:**
- A: deletions were intended (CommonUI rebuild) → B-002 accepts them, and the refactor branch's native widgets are rebuilt on CommonUI later.
- B: accidental → restore the 16 assets on `dev` from `41c3e86d^` (one commit, LFS), then B-002 merges cleanly asset-wise.
- C: accidental, and the data-asset/map edits in the same commit were too → review that whole commit in the editor before deciding.
**Recommendation:** B or C. The commit message says "reference only", and `BP_CombatGameMode` going missing is likely to break the battle map. Check in the editor whether `dev` still opens a fight.
**Answer (Crown):** **B: accidental** (2026-10-26). Restore the 16 deleted assets.
**Notes for Claude (D-022 answer):**
- Restore exactly the 16 deleted `.uasset`s on `dev` from `41c3e86d^` (`git checkout 41c3e86d^ -- <paths>`), one commit, LFS; then B-002 can proceed.
- Only the **deletions** were ruled accidental. The other changes in that commit (data assets, `TestLevel_Nav`, `BP_CombatOrchestrator`, `BP_HubPlayerController`, CommonUI/party C++) came from Crown's CommonUI WIP — **leave them**; flag anything that conflicts.
- After restoring, confirm `dev` builds + tests, and that `BP_CombatGameMode` is referenced again where the battle map expects it.
**Applied (2026-10-26, `chore/D-022-restore-combat-gamemode`), please confirm:** a closer look showed only **1 of the 16 was really deleted**.
- **Restored:** `Content/Core/GameModes/BP_CombatGameMode.uasset` (from `41c3e86d^`).
- **Not restored, because they were *moved*, not deleted:** the other 15 widgets were re-added in the same commit under `Content/UI/UMG/...` (an editor move: same names, `WBP_CharacterPanel` ×2 merged into one, package paths updated). Restoring them would duplicate every widget at the old paths. No C++/config hard-codes either path.
- `refactor/bp-logic-to-cpp` changed **no** `Content/` files since its merge base, so B-002's merge takes the moves with no asset conflicts.
- If you *did* want the old-path copies back too, say so: it's one more `git checkout 41c3e86d^ -- <paths>` commit.
**Notes for Claude:** whichever answer, resolve the 4 C++ conflicts by keeping Arc A's cleanup (`ReleaseCombatResources`, InProgress-before-InitializeCombat) together with the refactor's native HUD calls, then rebuild + test.

### D-023 — Can a stunned character infuse its basic attack, and can it flee or switch weapon?
**Status:** DECIDED: applied in B-092, 2026-10-26 (row in Decided) · **Raised:** 2026-10-26 · **Blocks:** nothing
**Question:** Under D-001 (stunned = Attack or Defend), does "Attack" include an infused or crystal-charged attack, and are Flee and Switch Weapon blocked?
**Context:**
- B-061 lets a stunned actor use the loadout's current basic attack. The gate checks only the action type and the skill, so an attack with an infusion charge level still passes and spends EP/charge (`ActionExecutor.cpp` cost path).
- B-061 blocks Flee and Switch Weapon, reading D-001's "can still Attack or Defend" and the Stun enum's "can only Attack or Defend" literally.
**Options:**
- A: plain attack only. Infusion is refused while stunned (charge forced to 0), and Flee/Switch stay blocked.
- B: infused attack allowed (it's still "the attack"), and Flee/Switch stay blocked. This is the current behaviour.
- C: B, plus Flee and/or Switch Weapon allowed.
**Recommendation:** A. Stun should mean "your basic swing or brace", and infusion is the EP-spend layer that Silenced already gates.
**Answer (Crown):** **A: plain attack only** (2026-10-26). No infused/charged attack, no Flee, no Switch Weapon while stunned.
**Notes for Claude:** A → in `IsActionAllowedWhileStunned`, also require no infusion charge; add a StunRule test. C → add the allowed types to the same function.
- Defend (Crown, 2026-10-26): **add a Defend button to the combat menu now (B-092)**; later Defend also gets its own **key binding** (keyboard + gamepad), see B-093.

### D-024 — Keep the current UI or rebuild it on CommonUI?
**Status:** DECIDED · **Raised:** 2026-10-26 · **Applied:** `docs/Backlog.md` → "UI rebuild"
**Answer (Crown, 2026-10-26):** rebuild the UI on CommonUI rather than restyle the current widgets. Same as the deckbuilder: **mockups first, from the kit assets, and Crown reviews them before anything is built.**
**Crystal theme (Crown, 2026-10-26):** the UI should **focus on crystal themes**: crystal light, faceted shapes, element gems (the art direction's "dark stone and steel, lit by crystal", leaning harder on the crystal). Mockups show at least one more-crystal option where a piece has candidates.
**Element colours (Crown, 2026-10-26):** Reality is **indigo**, Void is **violet** (keeps the two apart; the kit has no Reality badge, so draw one in indigo).
**Broken Darkness display (Crown, 2026-10-26):** absorption stacks as a multiplier on the EP value, "89 (x3)"; overload drawn on the EP bar (the bar continues past full into the overload band), not a separate gauge. **Bar colour (Crown, 2026-10-26):** up to full, the bar is the **absorbed element's colour** (`CurrentAlignmentElement`; the normal EP colour when nothing is absorbed yet); past full, it switches to **dark variants of that colour that intensify** through the overload steps (the existing 100% / 110% / 120% thresholds, `CombatConstants::OVERLOAD_*_THRESHOLD`). A glow/pulse at the top step for colour-blind readability is a suggestion, not ruled. Checked: overload is `CurrentEP` above `MaxEP`, the same field (`BrokenDarkness.md` "Energy"); today's panel only colours the EP text (`CharacterPanelWidget::RefreshEnergyBar`).
**Notes for Claude:**
- Replace one screen group at a time. The old widgets keep working until the new screen passes Crown's PIE check, then they're retired (B-106).
- Logic stays in C++ (`refactor/bp-logic-to-cpp`, B-002/B-003); new widgets are layout only on those bases.
- Mockups need no editor. Building and importing do (CC+UE jobs).
- The moved widgets in `Content/UI/UMG/` (D-022) aren't worth more investigation: they are replaced anyway.

### D-025 — Where does the mockup tool live, and where do the mockups go?
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** B-100 (then the whole UI rebuild)
**Question:** Copy the deckbuilder's mockup tool into this repo, and are the drafts committed so Crown can review them from the phone?
**Context:** `tools/ui_drafts/` in `wor-deckbuilder` (Python + Pillow) draws plain + annotated 1280×720 drafts from the licensed kit in `wor.game/assets/ui/library/` (git-ignored there). Its drafts are git-ignored too, so they can only be seen on the PC (`UI_DeckbuilderReuse.md` §5, open item 4).
**Options:**
- A: copy the tool into `Tools/ui_drafts/` here; it reads the kit from a local path (env var `WOR_UI_KIT`); drafts committed under `docs/Design/screen_mockups/<area>/<screen>/vN/` (LFS, private repo) so they open on the phone.
- B: as A, but drafts stay git-ignored (PC only, like the deckbuilder).
- C: keep the tool in the deckbuilder and point its output at this repo.
**Recommendation:** A. One repo to work in, and you can review drafts from your phone. The repo is private and LFS-tracks `png`; drafts are small (1280×720).
**Answer (Crown):** **A** (2026-10-26). Copy the tool into this repo; commit the drafts so they can be reviewed from the phone.
**Notes for Claude:** tool under `Tools/ui_drafts/`, kit path from env var `WOR_UI_KIT` (fail with a clear message if unset); drafts under `docs/Design/screen_mockups/<area>/<screen>/vN/` (`_plain.png` + `_annotated.png`, LFS). Copy only the tool code, never the kit itself.

### D-026 — Party follow-ups: do found characters stay, and can you switch control mid-fight?
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** B-076 (part), B-071 (control switch only)
**Question:** (1) Does a character you find during a trial stay in your roster after the run? (2) Can you switch a member between you and the AI during a fight?
**Context:** D-003/D-004 answers (2026-10-26). Runs reset found gear to a pool (trials are runs); characters aren't covered yet.
**Options:**
- (1) A: they stay (recruited for good) · B: run-only, gone after the run · C: run-only, but you unlock them to recruit in the hub.
- (2) A: hub only · B: also mid-fight, on that member's turn.
**Recommendation:** (1) C: finding them in a run is the reward, and the hub keeps the team-building · (2) B: cheap to add and handy when a fight turns.
**Answer (Crown):** (2026-10-26)
- **(1) B, run-only.** The characters you find in a trial are the same characters you'd find in the hub. Finding one in a run is temporary: they leave when the run ends. They come **ungeared**; you gear them yourself, and the gear in your run inventory is the gear they use as well (one shared run inventory for the party).
- **(2) B.** You can switch a member between you and the AI mid-fight, on that member's turn.
**Notes for Claude:**
- B-076: a character found in a trial is added to the party for that run only and removed at run end (no roster or save change). Draw them from the same character templates as the hub recruits.
- Found characters start with empty slots; equipping draws from the run inventory, the same one the leader uses. Gear stays in the run inventory when it's unequipped or the character leaves.
- How a hub-built team's own gear works at run start isn't covered by this answer (D-021 covers the leader's run-start loadout). If B-076 needs it, raise a new decision rather than assume.
- B-071: the Player/AI switch is also offered on the member's turn in combat (a command-menu entry).

### D-027 — Should classes gate skills, or only change passives (the deckbuilder way)?
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (shapes B-080 and later class work)
**Question:** Keep the hard class gate (Generic = abilities only, Refractor/Resonator = spells only), or let any class use any skill it has a source for, with classes differing by passives?
**Context:**
- This game: helpers `CharacterClassHelpers::CanUseAbilities` (Generic-only) and `CanUseSpells` (not Generic) exist (`ECharacterClass.h:45-55`), but combat doesn't use them (corrected 2026-10-26, see Answer).
- Deckbuilder: no class gate. Abilities need their weapons; spells need an elemental source (Ring, Evolution, a Refractor's innate), never a weapon. Classes differ by passives: Generic +1 Physical Resistance, Resonator +1 Resistance per keyed Ring's element, Refractor +3 Amp and +1 Resistance to its innate (`01-card-model.md` "Class passives").
**Options:**
- A: keep the hard gate (current).
- B: the deckbuilder model: gear and sources decide, classes give passives.
- C: half-way: Generic is still spell-less (D-007), but Refractors and Resonators can also use weapon abilities.
**Recommendation:** none yet: it defines the classes. B makes builds freer and matches the deckbuilder; A keeps classes very distinct.
**Answer (Crown):** (2026-10-26) **The deckbuilder way, with classes unique by design.** Each class's slots and rules make it what it is; what a character can use comes from what its class slots hold. **Weapons never give spells:** a weapon with a crystal gives **elemental (infused) abilities**. Spells come from rings, evolutions and a Refractor's innate.
**The classes as the code has them (checked in code and confirmed by Crown, 2026-10-26):**
- **Generic:** primary slot (weapon, ring or evolution) + a secondary weapon slot. **With an evolution, the primary is locked to the evolution and the secondary weapon stays** (Crown, 2026-10-26; the code disagrees in places, B-099). With two weapons, the shown weapon decides which abilities are available (`LoadoutComponent.cpp:1803`).
- **Refractor (Caster):** primary slot (weapon, ring or evolution) + **innate spells with a cost system**: each spell costs its tier (F 1 … S 7), budget 24, at most 6 per school; each world pillar (Mind/Body/Spirit) at 4/7/5+ cuts costs by 1, minimum 1; Broken Darkness doubles the budget to 48 (`SpellPoolConstants.h`, `LoadoutSystem.md` "Caster innate spell budget").
- **Resonator:** primary slot (weapon or evolution, not a ring) + a **ring loadout**, all sharing one **budget of 5**: weapon 1 (2 with a crystal, 3 with an evolution on it), evolution primary 2, ring 1 (evolved ring 2) (`LoadoutConstants.h:31-34`, `LoadoutComponent.cpp:607-655`). It casts the active ring's spells.
**Notes for Claude:**
- Abilities are already slot-driven: the combat menu shows the active weapon's abilities for any class (`FCombatCapabilities.cpp:49-59`). `CharacterClassHelpers::CanUseAbilities/CanUseSpells` (`ECharacterClass.h:45-55`) and `UCharacterData::CanUseSpells/CanUseAbilities` (`CharacterData.h:261-264`) are not used by combat. B-097 checks for any other callers and removes them, or flags them; no behaviour change.
- The real change is B-098: weapon-crystal spells (`AssignedSpells`, `FCombatCapabilities.cpp:61-67`) become infused abilities.
- Class passives like the deckbuilder's are **not** ruled here; suggest them in a new decision if wanted.

### D-029 — Ice as a sub-element of Water: how does it work?
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (new job once answered)
**Question:** Crown (2026-10-26): Water should have a sub-element, Ice. What can be Ice, and what does a full Ice bar do?
**Context:**
- This game has no Ice (`ESpellElement` has none).
- Deckbuilder (`02-enums.md` "Ice — a damage type, not an element"): Ice is a **damage type**, not a full element. No Ice crystal, ring or Refractor; an Ice skill is keyed Water and Water sources unlock it. Ice resistance also resists Water; Water Amp doesn't boost Ice; "on Water hit" hears Ice too. Guideline: Ice is a heavier, slower Water (more Energy, more damage).
**Options:**
- A: the deckbuilder model (Ice = a Water damage type for some skills).
- B: a full tenth element with its own crystals, rings and Refractors.
**Recommendation:** A, per the deckbuilder; much smaller, and it keeps nine elements. Follow-up if A: what a full Ice bar does (Water's is "no healing"), e.g. Freeze = slower turns.
**Answer (Crown):** (2026-10-26) **A, the deckbuilder model**, and **a full Ice bar slows action speed** (Freeze).
**Notes for Claude:**
- Add `Ice` to the element/damage-type enum **at the end** (append-only, explicit value). Ice is a damage type only: no Ice crystal, ring or Refractor innate; Ice skills are Water-keyed and Water sources unlock them.
- Resistance and triggers per the deckbuilder table (`02-enums.md` "Ice — a damage type, not an element"): Ice resistance covers Ice and Water; Water Amp doesn't boost Ice; "on Water hit" hears Ice. Resistance table (D-005) gets an Ice column; values `// TODO: tune`.
- Ice bar cap → a timed **turn-speed debuff** (the existing `TurnSpeedDebuff` effect), not Wind's one-off push-back (D-014). Duration and size named constants, `// TODO: tune`.
- Needs a survey first: every `switch` on the element enum, element colours/icons, the buildup bars and the UI list of elements.

### D-030 — What a full status bar does, and how long
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (shapes B-062, B-063, B-064a)
**Question:** Crown (2026-10-26) wants the status-bar caps changed: what does each one do, and for how long?
**Answer (Crown):** (2026-10-26) **A full bar does something big for 2 turns**, counted the deckbuilder way: **2 of the affected character's own turns**, not every turn that passes. Examples:
- **Wind:** pushes your next two turns down the turn order (refines D-014).
- **Water:** ~~lowers healing for two turns~~ → slows Action and Spell Speed, 1 turn (D-090, Crown 2026-10-26).
- **Ice:** slows action speed for two turns (D-029).
**Notes for Claude:**
- Turn counting per the deckbuilder (`04-effects.md` "For N turns counts the holder's turns"): it counts the holder's turns; applied during the holder's own turn, that turn counts as turn 1; it ends when the holder's turn after those starts. Put this in one shared helper (it's also what GAS stage 3's turn tracker needs, B-042/B-044).
- Duration: **1 turn since D-090** (was 2), a named constant (`STATUS_CAP_TURNS`); sizes ("how big") are named constants, `// TODO: tune`.
- The other caps (Fire burn, Earth defence down, Lightning stun, Light crit down, Darkness silence) follow the same 2-holder-turns rule unless Crown says otherwise; flag any that already have a different authored duration rather than overwrite it.
- Water today is `HealBlock` (blocks all healing); change it to a healing reduction (named constant).

### D-031 — Item and skill descriptions: the deckbuilder's nested text
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (shapes B-044, B-045, the tooltip screens)
**Answer (Crown):** (2026-10-26) Use **the same nested descriptions as the deckbuilder**.
**What that is** (`wor-deckbuilder/docs/design/WoR_TCG_Pivot_Design.md` "Card text as a tree"; generated text per `docs/reference/07-text-grammar.md`):
- The text is a **tree**: level 1 is the trigger (On Hit, On Damage, Activate, Passive…); each indented level adds a condition ("While below 30 HP"), all of which must hold; the leaves are the effects with their numbers, duration and charges. One item can have several triggers.
- **Passive** holds only always-on effects and bonus stats; conditional ones nest under their condition.
- **Short form** for small spaces: one line per trigger, "Trigger: effect", with "*" when it has conditions ("On Hit*: Gain 2 Energy").
- **Live lighting** in fights: conditional lines are greyed; when the condition holds now, the branch lights **green (buff) or red (debuff)**. The pop-up of the base item shows no lighting.
- Text is **generated from the data**, never hand-written, so it can't drift from what the item does.
**Notes for Claude:**
- Needs the effect data to carry trigger → nested conditions → effects; B-044 covers that in its comparison.
- A text generator (B-045) with tests per the deckbuilder's fixtures (`09-fixtures.md`, text-grammar rows), then the skill tooltip / item detail screens use it (UI rebuild).
- Words follow this game's names (Refractor, Refraction where player-facing per the deckbuilder's naming doc); ask Crown on any new player-facing term.

### D-032 — Shape of a run (trial)
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Written up:** `docs/Design/RunStructure.md` · **Blocks:** — (shapes trial content, enemies, rewards, UI)
**Context:** `Resources_Design.md` "Trial = the run" leaves the shape deferred; §7.1 describes an older "continuous escalating climb". Code: each trial is a walkable map behind a hub door (`TrialData.h`: Name, Element, Level, EncounterLevel; LootPool, POIs, Boss, Modifiers banked), fights start on contact (`EncounterComponent`).
**Answer (Crown):**
- **(1) Open maps, Nightreign style** (2026-10-26): you roam the trial's map and choose your fights; abilities and movement out in the world (field abilities, `GASMigration.md`); vendors, chests and characters to find along the way; the boss at the end. "Just enjoy yourself." Supersedes §7.1's escalating climb.
- **(2) Goal: A and C** (2026-10-26): **no timer**; the boss can be faced whenever you're ready, **and** you collect **memory fragments** while exploring. How the two combine is the next question (D-032 part 3).
- **(3) The story behind trials** (Crown, 2026-10-26): the **Elemental Lords** are characters from Crown's story who have **lost their memories**. In the trials they **face versions of themselves**, and find **items, gear and dialogue relating to themselves**:
  - Lore dialogue is **logged** (e.g. the Garnet Lord and the Void Lord share history, so they have lines about each other).
  - Lords **react to what you bring**: face one carrying a specific weapon and they may notice it and give you a **quest** (e.g. a weapon tied to the Fire Lord).
  - This needs **bound gear and skills** (D-033).
- **(4) Memory fragments** (Crown, 2026-10-26): A, **a fragment unlocks a memory** (scene, lore, dialogue, maybe a quest). A memory **can also weaken or strengthen the boss**, and that's **authored per memory from the lore** (a designer picks the effect), not a fixed happy/sad rule.
  - **Character-specific:** the same fragment or item means different things to different lords. The Garnet Lord carrying a weapon may get a deep memory where the Amethyst Lord gets little. Who you are and what you carry both decide what plays.
  - **No hint (A):** the player doesn't know what a fragment does until it plays. Exploring is a gamble.
- **(5) Leaving early: C + B** (Crown, 2026-10-26): you can only leave at **exit points** on the map (C), and leaving **pays less** (B). **The full payout only comes from death or victory.** The leave-confirm screen says so plainly (e.g. "Leaving now pays N of M Prisms. Full rewards only for beating the boss or falling in battle."). The reduced share is a named constant, `// TODO: tune`. Replaces `Resources_Design.md` §7.2's "leave early = forfeit everything".
- **(6) What's on a trial map** (Crown, 2026-10-26), all ten: **enemies** (optional fights), **mini-bosses** (tougher, better rewards, may guard fragments), **memory fragments**, **characters to recruit** (run-only, D-026), **vendors** (Gold), **chests**, **exit points**, **rest spots** (heal HP/EP), **lore objects** (logged dialogue and notes), **the boss arena**. **Rest spots double as exit points.**
  - These are `TrialData.h`'s banked POIs / LootPool / Boss fields.
- **(7) Resting** (Crown, 2026-10-26, after comparing Nightreign and Elden Ring):
  - Rest spots **fully heal** (HP, EP) and are **where you sort your inventory and loadout** during a run. They're also exit points.
  - **Beating a mini-boss creates a new rest spot** there (Nightreign).
  - **Resting does not reset enemies.** Beaten enemies stay beaten (D-002).
  - **Optional boss revival:** a trigger can **revive a beaten boss, stronger**, for a **higher chance at better gear**. That's the opt-in way to farm. Where the trigger is, how much stronger, and whether it covers mini-bosses too isn't ruled; ask when building.
- **(8) Trials and lords: B** (Crown, 2026-10-26): **any lord can enter any trial**, and the trial reacts to who's there. **Everything is cross-linked:** all characters have interactions with each other in some way, so e.g. the Water Lord entering the Fire Lord's run gets lore about their reactions to each other.
- **(9) Reaction triggers** (Crown, 2026-10-26): one data structure, **"if these are equipped while facing this character → trigger these"**:
  - **Equipped is a list**: e.g. if these three items are equipped together.
  - **Facing** a given character (a lord, its mirror, an NPC).
  - **Triggers** any mix of: effects (e.g. the hidden bound bonus, a boss weaken/strengthen), a quest, dialogue.
  - Bound gear (D-033), memory reactions (part 4) and lord-vs-lord lore all use this one structure.
- **(10) Difficulty: A, picked at the door** (Crown, 2026-10-26). Gates also come in **variations**, e.g. **Past, Present, Future, Anger** and more, so the door needs to **show what the gate represents** (its variation, plus the difficulty pick) before you enter. The variation names are **examples only**.
- **(11) What a variation changes** (Crown, 2026-10-26): **the boss.** It may be a **variant of the lord** (e.g. a younger version) or **someone related to them**, with the story that goes with it. **No rule twists** (Crown, 2026-10-26): a variation changes the boss and story, not the rules.
- **(12) Maps** (Crown, 2026-10-26): **one unified map per representation** (variation), shared by every lord's trial. It's **tailored to the trial's element** with the weather system and other element assets (sky, lighting, effects, props), not a separate map per lord.
  - Today the weather system only swaps the in-combat sky by team dominance (`WeatherSystem.md`). Tinting the explore map by trial element is new work; `TrialData.Element` already exists.
**Notes for Claude:** the lords exist as data already (`DA_Character_FireLord` plus 6 more, commit `bdf2557`; Fire, Water, Darkness, Broken Darkness, Earth, Wind, Light, Lightning, Void, Reality). Build reaction triggers as one data asset type (e.g. `UReactionTriggerData`: required equipped items (all of), who's wearing them (optional: a specific lord), facing character, then effects / quest / dialogue). Reuse the condition model from B-044/D-031 (trigger → nested conditions → effects) rather than a second system. Data shape to propose when building (not ruled): a memory-fragment data asset with its trial, per-lord entries (which lord, optional item it reacts to, lore text/dialogue, depth), and an authored boss effect per entry (weaken / strengthen / none, size). Record further answers here; when complete, write `docs/Design/RunStructure.md` and point `Resources_Design.md` §7.1 at it.

### D-033 — Bound gear and skills
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (lord quests, trial loot, D-032)
**Question:** Crown wants bound gear/skills so items can belong to a lord (e.g. the Fire Lord's weapon). How do they work?
**Context:**
- This game has nothing like it yet.
- Deckbuilder (`WoR_TCG_Pivot_Design.md`, 2026-10-14): **Bonded** cards are a Character's own cards; **Bonded effects** give that Character a bonus when using them (e.g. Garrick with Dual Blades). **Signature** cards are what a Character's encounter always brings.
**Options (who can use a bound item):**
- A: anyone can equip it; only its own lord gets the **bound bonus** (and the reactions and quests).
- B: only its own lord can equip it.
- C: anyone can, but it's weaker in other hands.
**Recommendation:** A, the deckbuilder's Bonded model: nothing is locked away, and the bound bonus makes the right pairing feel special. A lord's trial version uses its bound kit as its **signature** gear (what it always brings).
**Answer (Crown):** **A** (2026-10-26): anyone can equip a bound item; only its own lord gets the **bound bonus**, the reactions and the quests. **The bound bonus is hidden unless the item is equipped by its owner** (Crown, 2026-10-26): on anyone else it doesn't show; on its own lord the description shows it.
**Notes for Claude:**
- Data: a bound item names its lord (one field on the item asset) and carries a bound-bonus effect list, applied only when the wearer is that lord (same gear-effect path as D-008).
- Hidden: the description generator (D-031/B-045) shows bound-bonus rows only when the item is equipped by its own lord.
- A lord's mirror version in its trial carries its bound kit as its signature gear (deckbuilder "Signature").

---

### D-034 — Crit damage ceiling: x2.5 or x2.0?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** What is the biggest multiplier a crit can reach with gear?
**Context:** The stat caps crit damage at x1.5; gear multiplies it toward `CRIT_DAMAGE_GEAR_CEILING = 2.5` (`CombatConstants.h:192`). `DamageCalculator.md` and `Luck.md` say x2.0. Rules: `docs/Rules/02-elements-and-damage.md` CED-2.
**Options:** A x2.5 (code) / B x2.0 (docs).
**Recommendation:** B, because the docs were written to that design and the other gear ceilings are x2.0; the 2.5 looks like drift.
**Answer (Crown):** **A, x2.5** (2026-10-26, after comparing PoE2, Diablo 4 and Expedition 33: freedom games don't hard-cap crit damage; crit chance being multiplicative already limits it). Fix the docs (`DamageCalculator.md`, `Luck.md`) to x2.5; the code stays. Uncapped with diminishing returns is a possible later step.
**Applied:** docs fixed to x2.5 (`DamageCalculator.md`, `Luck.md`, Rules 02 ED-19/CED-2, `chore/apply-D-034-041`); the `CombatConstants.h:184`, `DamageCalculator.h` and `AIDecisionManager.cpp` comments and `StatComposition.md` fixed in B-147.

### D-035 — Should infusing an ability cost extra on top of the charge level?
**Status:** DECIDED (applied in B-147) · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** An infused ability pays the flat x1.5 `INFUSION_ENERGY_MULTIPLIER` and then the charge multiplier (L1 x1.5, L2 x2.0): about x2.25 / x3.0 EP. A spell pays only x1.5 / x2.0. Keep the double surcharge?
**Context:** `InfusionConstants` comments and `Infusion.md` say abilities and spells share the charge; two comments give wrong numbers ("30%/60%", "1.15x/1.30x"). Rules: 02 CED-1.
**Options:** A keep both (abilities cost more) / B charge multiplier only (same as spells).
**Recommendation:** A, because the deckbuilder also makes a physical ability pay +1 Energy to deal an element (`wor-deckbuilder/docs/reference/01-card-model.md:93`, "Cast through"). Fix the comments either way.
**Answer (Crown):** **A** (2026-10-26): keep the extra x1.5 for infusing an ability on top of the charge level (deckbuilder: a physical ability pays more to deal an element). Fix the two wrong comments.
**Applied:** the two wrong comments fixed in B-147; Rules 02 ED-30/CED-1 updated.

### D-036 — Under D-005, which resistance scales damage?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-064 (now after B-145)
**Question:** When resistance starts scaling elemental damage (D-005), do the general Resistance stat (up to 100% with gear) and resistance buffs count, or only the per-element columns?
**Context:** D-005's notes say "class table + gear"; the buildup path's value also includes the general stat and buffs. Stacked with Defense's 85% cap it could approach immunity. Rules: 02 CED-3.
**Options:** A per-element columns only / B everything the buildup path uses.
**Recommendation:** A, because it can't stack toward immunity and it matches the deckbuilder, where resistance is per element.
**Answer (Crown):** **A** (2026-10-26): only **per-element resistance** (class table + gear + element-specific effects) scales elemental damage, **1:1** (+20% Fire = 20% less Fire damage; −15% = 15% more), **capped at ±50%** (compared with PoE 75%, Diablo 4 70%, Elden Ring diminishing returns; lower because turn-based fights are a few hits). General Resistance and blanket buffs don't count. Crown also wants the resistance system **implemented better** (B-145).
**Queued:** B-064 waits for the B-145 resistance plan (same files); Rules 02 ED-27/CED-3 updated.

### D-037 — What is "Physical" in code?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-080 (now after B-145)
**Question:** D-007 says uninfused skills deal Physical, but there is no Physical element: they deal `None` plus Slash/Pierce/Impact. Is Physical "None + its physical type", or a new `ESpellElement` member?
**Context:** `ESpellElement.h:28-34`; `EPhysicalDamageType.h:12-15`. Rules: 02 CED-4.
**Options:** A None + physical type, shown as "Physical" / B a new enum member (appended).
**Recommendation:** A, because it needs no enum change and resistance and buildup already read the physical type.
**Answer (Crown):** **C** (2026-10-26, after comparing Expedition 33, Elden Ring, PoE/Diablo 4, BG3: Physical is a damage type, a peer of the elements): add a proper **damage type**: Physical (with its subtypes Slash / Pierce / Impact), the nine elements, and Ice (D-029). It's separate from the **element** a character, crystal or ring *is* (`ESpellElement`), as the deckbuilder does (`DamageType` vs `Affinity`/`Element`).
**Notes for Claude:**
- Do it inside the GAS / resistance redesign (B-145, B-041/B-042), not as a separate pass: same files. Until then the game **shows** "Physical" for None + physical type (A as a stopgap).
- New enum appended-only with explicit values; `ESpellElement` stays for identity. Ice (B-064a) goes on the damage type, not on `ESpellElement`.
- Resistance per damage type (one attribute each), so Physical resistance sits beside the elements.
**Queued:** B-080 and B-064a wait for B-145; Rules 02 CED-4 updated.

### D-038 — What does Silenced block?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Spells only, or every EP spend (abilities, infusion)?
**Context:** The code blocks spells only (`ActionExecutor.cpp:176-180`). The display name ("No EP Spend"), `docs/Mechanics/Status/StatusEffects.md` and D-023's reasoning say every EP spend. Rules: 03 C3-2, 04 C4-6.
**Options:** A spells only (code) / B every EP spend.
**Recommendation:** B, because that's what the name and the docs promise; under A a silenced Generic is barely affected.
**Answer (Crown):** **B, reshaped** (2026-10-26): Silenced affects **every Energy spend** (spells, abilities, infusion), but instead of a full block it **seals part of your Energy**: **50% of max EP is locked for 1 of the holder's turns** (D-030 as changed by D-090); you can only spend the rest. Basic attack, Defend and items still work.
**Notes for Claude:**
- Sealed share is a named constant (`SILENCE_SEALED_EP_FRACTION = 0.5f`, `// TODO: tune`). Spendable EP = CurrentEP − sealed amount (floored at 0); validation and the menu read spendable EP.
- UI: the sealed part of the EP bar is drawn dark (fits the B-111 bar work).
- Replaces the spells-only block at `ActionExecutor.cpp:176-180`; the display name "No EP Spend" changes to match (naming: ask Crown).
- Broken Darkness: its EP is the same field, so the seal applies to it too unless Crown says otherwise.
**Queued:** B-148 (the seal). Its display name is D-070.

### D-039 — How many spells does an evolution hold?
**Status:** DECIDED (applied in B-147) · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Set by the evolution's tier (1–6), or a flat number?
**Context:** Combat caps it by tier (`LoadoutComponent.cpp:575-581`); the saved-loadout check uses a flat 6 (`FSavedLoadout.cpp:190-195`); `EvolutionCrystals.md` says about 4. Rules: 01 CL-C1.
**Options:** A by tier, 1–6 / B flat 6 / C flat 4.
**Recommendation:** A, the live rule; then make the saved-loadout check match.
**Answer (Crown):** **A, by tier, as already designed** (2026-10-26): crystals and evolutions hold spells by their tier (1–6, the shared `SlotsForContainerTier` curve). Not a new decision: fix the saved-loadout check's flat 6 (`FSavedLoadout.cpp:190-195`) and `EvolutionCrystals.md`'s "about 4" to match.
**Applied (B-147):** the live check (`LoadoutRules.cpp`) already capped by tier; a test now pins it, and `EvolutionCrystals.md` is fixed. The flat 6 remains only in the legacy `FSavedLoadout::GetValidationErrorsLegacy`, which B-172 retires. An evolution *attached* to a weapon or ring now holds spells by its tier too (**Q-147 = B**, applied).

### D-040 — Does a Refractor's innate always work?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Here a Refractor's innate spells always work. In the deckbuilder they work only while a gear slot is empty. Should wearing gear switch them off?
**Context:** Rules: 01 CL-C2; `wor-deckbuilder/docs/reference/01-card-model.md` (Refractor passive). Raised only because the two games differ.
**Options:** A always on (current) / B only while the gear slot is empty (deckbuilder).
**Recommendation:** A, because a Refractor here has only one gear slot, so B would make gear and innate an either-or.
**Answer (Crown):** **A, always on** (2026-10-26): a Refractor's innate spells always work, whatever is in the primary slot. Already the code (`FCombatCapabilities.cpp:80-85` offers `InnateSpells` for every Caster, checked 2026-10-26); nothing to build. The deckbuilder's empty-slot rule doesn't apply here.
**Applied:** nothing to build; Rules 01 CL-C2 updated (`chore/apply-D-034-041`).

### D-041 — Are skill requirements a gate or only a scale?
**Status:** DECIDED (applied in B-147) · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Can a character use a skill it is under-levelled for?
**Context:** The code never blocks; the requirement gap only scales the skill down. A commented-out refusal sits at `ActionExecutor.cpp:233-236`, and `Stats.md` says pillar levels "gate skill tiers". Rules: 01 CL-C3.
**Options:** A usable, just weaker (current) / B refused below the requirement.
**Recommendation:** A, the current behaviour; then delete the commented-out gate and fix `Stats.md`.
**Answer (Crown):** **A, usable but weaker** (2026-10-26): the shipped requirement-gap scaling stays (`Mechanics/Scaling/RequirementGap.md`); overreaching also feeds Broken Darkness strain. Delete the commented-out refusal (`ActionExecutor.cpp:233-236`) and fix `Stats.md`'s "gates skill tiers". Crown: like everything else, it could be **done better**, so it's in the standard-way redesign (B-146).
**Applied (B-147):** the commented-out refusal deleted, `Stats.md` fixed; the redesign is B-146.

### D-042 — Does a member's chosen cell decide where they start a fight?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `FPartyMember.PreferredRow/PreferredColumn` are saved, but `StartCombat` puts both sides in the Middle row (`CombatOrchestrator.cpp:214-215`), so the ±5% row bonus never applies in play. Use the chosen cell?
**Context:** Rules: 06 Cp-2. The deckbuilder has no grid.
**Options:** A start in the chosen cell / B everyone in the Middle row (current).
**Recommendation:** A, because the row bonus and the stored formation exist for it.
**Answer (Crown):** **A** (2026-10-26): members start in their **chosen formation cell**, so row bonuses apply. Replace the Middle-row auto-assign (`CombatOrchestrator.cpp:222-223`) with each member's `PreferredRow/PreferredColumn`, falling back to auto-assign when unset or clashing. Enemies use their own preferred cells (tanks front, casters back, per `AIArchitecture.md`'s placeholder heuristic) until authored. **Formation can be changed at rest spots** during a run (Crown, 2026-10-26), alongside inventory and loadout (D-032 part 7).
**Queued:** B-149 (start in the chosen cell); formation at rest spots comes with the rest-spot work.

### D-043 — How does a fallen party member come back during a run?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Sapphire only revives inside a fight, and members dead at a fight's end are never spawned again, so a death lasts the rest of the run. Is that the rule?
**Context:** `EncounterComponent.cpp:210-260` leaves dead members out. Rules: 06 Cp-3.
**Options:** A no return this run (current) / B a trial vendor or rest-point service / C revive with low HP after the fight.
**Recommendation:** A for now (souls-like attrition, as in D-013), with B as a later vendor service.
**Answer (Crown):** **B** (2026-10-26): a fallen member can be **revived during a fight by spells or items** (Sapphire today; revive spells too), and **revived at a rest spot** (full heal, as rest spots already give). Otherwise they sit out the fights in between.
**Notes for Claude:** today members dead at a fight's end are left out for good (`EncounterComponent.cpp:210-260`); rest spots must bring them back. Check that a revive **spell** effect exists, not only the Sapphire item; if not, raise it. Rest spots themselves are part of the trial work (`RunStructure.md`).
**Queued:** B-150 (fallen members stay in the party; revive at rest spots). No revive **spell** effect exists (`ESkillEffectType` `Revive` was renamed `LastStand`; only Sapphire's `DefyDeath` revives), so that part is D-071.

### D-044 — What difficulty do AI-controlled party members play at?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-071
**Question:** One difficulty per encounter drives every AI character, so on an Easy encounter your own AI allies act near-randomly and try to defend only 40% of hits.
**Context:** `AI/AIDecisionManager.cpp:193-199` (one difficulty per encounter). Rules: 06 Cp-4.
**Options:** A a fixed ally tier / B the encounter's difficulty (current) / C a player setting.
**Recommendation:** A, because an easier fight shouldn't make your own team worse.
**Answer (Crown):** **A now, C later** (2026-10-26): AI-controlled party members play at a **fixed, good level** whatever the encounter's difficulty (named constant, `// TODO: tune`; the encounter difficulty drives enemies only). Later: **tactics** the player sets per ally (Dragon Quest / Persona style), a new decision when it comes up.
**Queued:** B-071 (now TODO).

### D-045 — Who do you control when the leader is down?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-071
**Question:** D-004 says the leader is always player-controlled. Today the player takes the first living member spawned, and if the leader dies the rest play on AI. What should happen?
**Context:** `Trial/BattleGameMode.cpp:129-170` possesses the first living member. Rules: 06 Cp-6.
**Options:** A only members set to Player (B-071); if none are left, the AI finishes the fight / B you take over the next living member.
**Recommendation:** A, because it keeps D-026's per-member choice meaningful.
**Answer (Crown):** **B by default, as a setting** (2026-10-26): when every member set to Player has fallen, **you take over the remaining AI members** so you never just watch. A **setting** lets the player choose instead to let the AI finish the fight (A).
**Notes for Claude:** setting on the Settings screen (Gameplay), default "Take over" (B), alternative "Let AI finish" (A). The leader stays Player while alive (D-004). Placeholder UI per the PIE rule until the CommonUI Settings screen exists.
**Queued:** B-071 (now TODO), with the setting as placeholder UI.

### D-046 — One wallet per player, or per character?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-073
**Question:** The wallet (Gold, Prisms, Diamond) sits on each character, while the gear pool and run inventory are shared (D-020, D-026).
**Context:** `Currency/CurrencyComponent.h:15-24`. Rules: 05 Cx-5. The deckbuilder has one account-wide currency and shared Essences (`wor-deckbuilder/docs/design/WoR_TCG_Pivot_Design.md` §5).
**Options:** A one wallet per player, everything / B Prisms and Diamond per player, essence per character / C per character (current).
**Recommendation:** A, because the pool and run inventory are already shared (D-020, D-026), and per-character essence would strand upgrades.
**Answer (Crown):** **A** (2026-10-26): **one wallet per player** for everything (Gold, Prisms, Diamond, all essences), like the shared pool (D-020) and run inventory (D-026). Move the currency off each character (`CurrencyComponent.h:15-24`) to player scope (PlayerState / the save, per D-064's rule).
**Queued:** B-151 (one wallet per player); B-073 still waits for D-048.

### D-047 — Where can you upgrade, repair, merge and downgrade?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** D-002 says upgrades work in the hub and in a run. `docs/Mechanics/Economy/Hubs.md` says upgrade, repair and merge are run-only, and still describes the draft run start that D-021 replaced. Which split holds for repair, merge and downgrade?
**Context:** `docs/Mechanics/Economy/Hubs.md` (composition table); `D-002`. Rules: 05 Cx-4.
**Options:** A upgrade in both (D-002); repair, merge, downgrade as Hubs.md says / B every service in both places.
**Recommendation:** A, because it keeps the one written design and only overrides it where D-002 ruled.
**Answer (Crown):** **B** (2026-10-26): **every service works in both places**: buy (Prisms at home, Gold in a run), upgrade, repair, merge, downgrade, reroll. In a run they sit at **rest spots** and **vendors**; at home, at the hub's service NPCs. Supersedes `Hubs.md`'s run-only split (written for the old two-hub model).
**Notes for Claude:** compared (2026-10-26): Expedition 33 upgrades at camp/rest points; Dark Souls 3 and Elden Ring at a blacksmith in the hub; Nightreign levels at Sites of Grace. Costs stay as each service carries them (`Upgrading.md`, `Merging.md`). Update `Hubs.md` when applied.
**Applied:** `Hubs.md` updated (every service in both places); the services themselves are unbuilt (`chore/apply-D-042-047`).

### D-048 — What does Gold buy in a run?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-073
**Question:** A code comment and `Currency.md` say consumables and temporary buffs. D-021 says the run-start loadout, in-run purchases and world stats. D-002 says upgrades never cost Gold, yet in-run shops charge Prisms today (`Purchase`).
**Context:** Rules: 05 Cx-1, Cx-6.
**Options:** A D-021's list only (run-start loadout, in-run purchases, world stats) / B also consumables and temporary buffs.
**Recommendation:** A, as it matches D-002 and D-021; then update the comment (`CurrencyComponent.h:128`) and `Currency.md`. Price trial merchants in Gold when B-073 lands.
**Answer (Crown):** **B** (2026-10-26): Gold buys the **run loadout**, gear at **in-run vendors**, **world stats**, **consumables** (healing, revive items such as Sapphire) and **temporary buffs**. Never upgrades (D-002). In-run shops charge Gold, not Prisms.
- **Naming (Crown):** the "starting loadout" becomes the **purchase loadout**, bought from a **loadout vendor** at the start of a run (D-021's purchase model, given an NPC).
**Notes for Claude:** fix the comment at `CurrencyComponent.h:128` and `Currency.md`; trial merchants priced in Gold when B-073 lands. The loadout vendor is a placeholder UI (C++) until the CommonUI screen exists.

### D-049 — Cap turns at 2 per round of the slowest?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `MAX_TURN_RATIO = 2` and `TURN_SPEED_THRESHOLD_DOUBLE` exist (`CombatConstants.h:331`), but the scheduler never reads them, so a fast character can get any number of turns per round.
**Context:** `Combat/CombatConstants.h:330-331` (unread constants); `Combat/TurnManager.cpp:265-270` (uncapped ratio). Rules: 04 C4-1.
**Options:** A cap at 2:1 / B no cap (current).
**Recommendation:** A, because the constants show that was the design, and an uncapped ratio lets stacked speed lock enemies out.
**Answer (Crown):** **A** (2026-10-26): cap turns at **2 per turn of the slowest** (`MAX_TURN_RATIO = 2`, which the scheduler must now read: `TurnManager.cpp` speed ratio). Wind push-back (D-014/D-030), Ice slow (D-029) and bonus turns all act within the cap. Compared: FFX (uncapped), Expedition 33, Octopath (one per round).

### D-050 — How long does Defend last?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Defend's +50% Defense for 1 turn ticks away at the end of the defender's own turn, before anyone attacks, so it does nothing.
**Context:** `Combat/Actions/ActionExecutor.cpp:2787-2792` gives it; the end-of-turn tick (`Combat/CombatOrchestrator.cpp:589`, `Skills/Effects/SkillEffectManager.cpp:1125-1145`) removes it. Rules: 04 C4-2. In the deckbuilder, Defend lasts until your next turn starts (`04-effects.md`, "Lasts").
**Options:** A until your next turn starts / B as now.
**Recommendation:** A (the deckbuilder's rule), with the 50% and the duration as named constants.
**Answer (Crown):** **A** (2026-10-26): Defend lasts **until the defender's next turn starts**, covering every enemy turn in between (deckbuilder rule; the shared holder-turn counting of D-030). Today it ticks off at the end of the defender's own turn and does nothing. The +50% and the duration become named constants (`// TODO: tune`).

### D-051 — How are effect durations counted?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-062, B-063
**Question:** The code ends an effect at the end of the holder's Nth turn, and a new effect acts the moment it lands, so a 2-turn DoT ticks 3 times. D-030 counts the landing turn as turn 1 and ends the effect as the next turn starts. Should D-030's counting apply to every timed effect, with exactly N ticks?
**Context:** `Skills/Effects/SkillEffectManager.cpp:1099-1153` (expiry), `:232-237, 974-991` (DoT ticks). Rules: 04 C4-3, C4-4. The deckbuilder gives exactly N ticks (`04-effects.md`, "Lasts").
**Options:** A D-030 counting everywhere, exactly N ticks / B D-030 for bar caps only.
**Recommendation:** A, one counting rule for everything, matching the deckbuilder.
**Answer (Crown):** **A** (2026-10-26): **one counting rule for every timed effect**: the holder's own turns, the landing turn counts as turn 1, ends as the holder's next turn after those starts, **exactly N ticks** (D-030, D-050; deckbuilder `04-effects.md` "Lasts"). One shared helper.

### D-052 — Is Emerald's bonus turn extra, or the same turn moved earlier?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Emerald's turn counts as a turn taken with no credit, so it costs a later normal turn. Extra Action adds a real turn.
**Context:** `Combat/TurnManager.cpp:566-575` (pinned turn, no credit), `:305` (Extra Action). The deckbuilder's `AddTurn` adds a turn (`04-effects.md`, verb 7). Rules: 04 C4-5.
**Options:** A a real extra turn / B a turn moved earlier (current), named to match.
**Recommendation:** A, because players read "bonus turn" as an extra one, and it matches the deckbuilder.
**Answer (Crown):** **A** (2026-10-26): Emerald's bonus turn is a **real extra turn** (no later normal turn lost), like Extra Action and the deckbuilder's Add Turn. It still respects the 2:1 cap (D-049). Change `TurnManager.cpp:563-571` (pinned turn currently costs a future slot).

### D-053 — Does the same effect from two gear pieces add up?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-062
**Question:** Here two gear pieces giving the same effect merge into one (deliberate, per `SkillEffects.md`). The deckbuilder adds one per source, which is also D-012's idea (a different source adds).
**Context:** `Skills/Effects/SkillEffectManager.cpp:319-330`; the deckbuilder's `Stat` duration row (`04-effects.md`). Rules: 04 C4-7.
**Options:** A add up, one per source item / B merge (current).
**Recommendation:** A, so D-012's rule covers gear too.
**Answer (Crown):** **A, the item is the source** (2026-10-26, the deckbuilder's model): an effect's identity is **the item (or skill) it comes from**, not the shared effect definition.
- The **same item** applying its effect again **refreshes** it (D-012).
- A **different item** with the same effect **adds** its own (Sword A + Ring B, each +10% Fire = +20%). Two copies of one ring are two sources.
- An effect can be ticked **"doesn't stack"** on its definition for things that would break if doubled (e.g. "revive once per fight"); a second source then adds nothing (Elden Ring's same-talisman rule).
**Notes for Claude:** effect definitions stay as **templates** only. Change the identity used for merging (`SkillEffectManager.cpp:320-331`, `PackEffectID(DefID, BundleIndex, payload)`) to include the source item's `PersistentID` (or the skill / actor for non-gear sources), consistent with D-011 (per character) and D-012 (per source). Add a `bDoesNotStack` flag to the definition. Fold into B-062 and the effects review (B-044).

### D-054 — Is "Turn Speed Up" a buff?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `ModifyTurnSpeed` is displayed "Turn Speed Up" and adds speed (`Combat/TurnManager.cpp:228-231`), but `IsDebuff` lists it (`Skills/Effects/ESkillEffectType.h:390`), so cleansing an ally strips it and Iolite can't strip it from an enemy. Is it a buff?
**Context:** Rules: 04 C4-8.
**Options:** A a buff / B sign-aware, like Status Resist Up / C a debuff (current).
**Recommendation:** A, matching its name and what it does.
**Answer (Crown):** (2026-10-26) **On an ally it's a buff; on an enemy it's a debuff.** Used on an ally it speeds them up (a buff, cleanse leaves it, Iolite can strip it); used on an enemy it slows them (a debuff, cleanse removes it).
**Notes for Claude:** this is the stat-stone consumable rule (`AugmentStoneSystem.md` "dual-form": ally → +X%, enemy → −X%, the sign flips on target allegiance). Implement as sign-aware (option B, like `ModifyStatusResist`): the sign is set when applied, from the target's side relative to the applier; `IsBuff`/`IsDebuff` read the sign. Remove `ModifyTurnSpeed` from the unconditional debuff list (`ESkillEffectType.h:390`).

### D-055 — Do bonus and Execution turns count toward effect durations?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-062, B-063
**Question:** D-030 counts "the holder's own turns". Does an Emerald bonus turn or an Execution turn count as one of them?
**Context:** Pinned turns are taken from `Combat/TurnManager.cpp:566-575`; the effect tick runs on every turn end (`Skills/Effects/SkillEffectManager.cpp:1099-1153`). Rules: 04 Gaps.
**Options:** A every turn the holder takes counts / B only normal turns count.
**Recommendation:** A, the simplest to read ("2 of your turns" means any 2 turns you take), and it needs no special case.
**Answer (Crown):** **A** (2026-10-26): every turn the holder takes counts toward durations, bonus and Execution turns included. Bonus turns are real turns (D-052).

### D-056 — Do the generic Speed Buff/Debuff move turn order?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `docs/Mechanics/Combat/TurnOrder.md` says Slow (`SpeedDebuff`) pushes turns back, but the scheduler reads only Turn Speed Up/Buff/Debuff (`Combat/TurnManager.cpp:228-231`); `SpeedBuff`/`SpeedDebuff` trigger a recalculation that changes nothing (`Skills/Effects/SkillEffectManager.cpp:2374-2375`).
**Context:** Rules: 04 C4-9.
**Options:** A yes, like Turn Speed / B no, fix the doc.
**Recommendation:** B, keeping one speed stat for turn order; D-029 already gives Ice its own turn-speed slow.
**Answer (Crown):** **C, retire them** (2026-10-26): the legacy generic `SpeedBuff`/`SpeedDebuff` (unused outside test actors, `ESkillEffectType.h` "Legacy Generic") are retired: kept in the enum (append-only), marked `[Obsolete]`-style with a comment, rejected by data validation so nothing new uses them. Turn order keeps one family (Turn Speed: `TurnSpeedBuff`/`TurnSpeedDebuff`/`ModifyTurnSpeed`); **Action Speed** (attack animation rate) and **Spell Speed** (spell animation rate) stay as they are. Fix `docs/Mechanics/Combat/TurnOrder.md` (Slow doesn't move turns).

### D-057 — Heavy visuals on data assets: hard or soft references?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** —
**Question:** Heavy visuals on data assets: hard or soft references?
**Context:** Montages, meshes and Niagara VFX on data assets are hard references (`WeaponData.h:57-124`, `SpellData.h:55-81`, `CosmeticsData.h:32-75`), so they load up front; Epic recommends soft references for heavy content. The newer VFX entries are already soft (`SkillCastEntry.h:59`). See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A keep hard; new visual/audio fields use soft refs / B migrate the existing fields now.
**Recommendation:** A, because a turn-based game with small arenas has no memory or load pressure today, and the migration touches many `.uasset`s for nothing visible.
**Answer (Crown):** **A, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-058 — Content naming: rename existing drift, or new assets only?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** B-123
**Question:** Content naming: rename existing drift, or new assets only?
**Context:** Prefixes are mostly Epic-style (456 `DA_`); drift is small (`Anim_` ×15, unprefixed maps, `DA_Spells_Fire_FireBall` vs `DA_Spell_*`). Renames create redirectors and touch LFS `.uasset`s. See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A new assets only, plus the junk/duplicate cleanup (B-122) / B also rename the existing drift.
**Recommendation:** A, because renaming risks breaking assets for a small gain.
**Answer (Crown):** **A, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-059 — Save: SaveGame tags or explicit save structs?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** B-124
**Question:** Save: SaveGame tags or explicit save structs?
**Context:** The dormant `SaveGame` tags on owned items miss their identity (`FWeaponInventoryEntry.h:59-72`); `UPoolSubsystem` holds the data to save (`PoolSubsystem.h:170-229`). See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A `SaveGame` tags read through `ArIsSaveGame` / B an explicit save struct, the pool's data copied whole.
**Recommendation:** B, because it is one obvious place to look and needs no per-field tagging discipline; tags pay off for generically saved actors, and we save a subsystem.
**Answer (Crown):** **B, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-060 — Old dev saves: wipe or upgrade?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** B-090
**Question:** Old dev saves: wipe or upgrade?
**Context:** Rules still change often (enum and struct renames, D-027 removing weapon spells). B-090 adds a version stamp and an upgrade hook. See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A write an upgrade step for every change / B until the first public build, saves below a minimum version are wiped with a warning; upgrade steps after that.
**Recommendation:** B, because writing upgrades for play-test saves is wasted work.
**Answer (Crown):** **B, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-061 — Localisation scope
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** B-127
**Question:** Localisation scope?
**Context:** Asset text is `FString` (`EquipmentDataBase.h:64,70`, `SkillDataBase.h:125,128`); about 70 player-facing `FText::FromString` calls; no localisation target. The CommonUI rebuild and B-045 are about to add much more text. See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A English only, ever / B localisation-ready now (all player text `FText`), translations and a localisation target at the first public build.
**Recommendation:** B, because it costs almost nothing while the UI is rebuilt and is expensive to retrofit.
**Answer (Crown):** **B, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068). Steam's AI disclosure covers AI-made localisation that ships: use human translators when translating, or disclose.

### D-062 — Where shared game words live (element, status, trigger names, button verbs)
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** B-126
**Question:** Where shared game words live (element, status, trigger names, button verbs)?
**Context:** Today they are English `TEXT()` in `SkillEffectDisplayNames.h` and enum metadata (`UMETA(DisplayName)`), which is stripped in cooked builds. See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A one String Table asset designers can edit / B `LOCTEXT` name tables in C++.
**Recommendation:** B for now, because the words are tied to enums the code owns and can be tested; move to a String Table if a writer or translator needs them.
**Answer (Crown):** **B, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-063 — Key rebinding in the first playable build?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** —
**Question:** Key rebinding in the first playable build?
**Context:** Input is Enhanced Input only, about 4 combat keys (`CombatPlayerController.h:51-71`); B-093 adds Defend/Infusion keys. Epic still marks runtime remapping (`UEnhancedInputUserSettings`) as experimental. See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A build rebinding now, on the settings screen / B fixed keys for now, action names kept unique so rebinding can come later.
**Recommendation:** B, because the feature is still experimental and the key count is small.
**Answer (Crown):** **B, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-064 — Stop adding per-fight state to GameInstance subsystems?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** —
**Question:** Stop adding per-fight state to GameInstance subsystems?
**Context:** Turn, menu, defense and effect state live in GameInstance subsystems, which never replicate; this is the first thing co-op would have to undo (`docs/analysis/ArchitectureReview_2026-10.md` §1.6/§1.9). See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A add a CLAUDE.md rule now: new per-fight state goes on the battle's actors / GameState / PlayerState / B keep the pattern, migrate when co-op starts.
**Recommendation:** A, because it costs nothing from here on and shrinks the co-op bill.
**Answer (Crown):** **A, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-065 — When does co-op move from Later to scheduled?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** B-131
**Question:** When does co-op move from Later to scheduled?
**Context:** D-003 keeps the co-op sphere gather; the Backlog has co-op only under Later. B-090 (save) and PlayerState ownership are tied to it. See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A after the single-player loop (B-090, B-105) / B after GAS stage 3.
**Recommendation:** A, so the save and PlayerState travel are designed together.
**Answer (Crown):** **A, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-066 — When to add functional (level) tests?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** —
**Question:** When to add functional (level) tests?
**Context:** All 35 tests are headless specs; Epic also offers `AFunctionalTest` and Gauntlet. A fight map built now would be rebuilt with the new HUD (B-105). See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A now / B after B-105.
**Recommendation:** B, because specs cover the rules meanwhile and the map would be rebuilt anyway.
**Answer (Crown):** **B, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-067 — Retire the legacy `*TestActor` classes?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** —
**Question:** Retire the legacy `*TestActor` classes?
**Context:** Five test actors (about 3.15k lines) duplicate logic the specs now cover; they were kept for manual PIE checks (B-008 stubbed them out of Shipping). See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A retire each one after a matching spec lands and Crown's PIE pass / B keep them as manual tools.
**Recommendation:** A, one at a time, because duplicate logic drifts from the real code.
**Answer (Crown):** **A, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-068 — A demo frame budget
**Status:** DECIDED · **Raised:** 2026-10-26 (B-047) · **Blocks:** —
**Question:** A demo frame budget?
**Context:** No budget exists and nothing has been profiled; the code is event-driven but has a few per-frame costs (B-138, B-139, B-140). See `docs/analysis/BestPractice_2026-10.md`.
**Options:** A 60 fps at 1080p on Crown's PC, checked once per milestone / B no budget until content is heavier.
**Recommendation:** A, because a number to check against makes regressions visible early.
**Answer (Crown):** **A, the recommendation** (2026-10-26, accepted with the other technical decisions D-057–D-068).

### D-069 — Enemies and bosses
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Written up:** `docs/Design/Enemies.md` · **Blocks:** — (enemy content, encounter composition Arc 2)
**Context:** already designed: enemies are characters with inventories/loadouts like players (`Resources_Design.md` §7.9: generic enemies roll from a source-tag item pool, bosses bespoke); AI (`AIArchitecture.md`); multi-enemy encounters (`EncounterCompositionSystem.md` Arc 2). Not designed: what you fight, and how they appear on the open maps (`RunStructure.md`).
**Answer (Crown):**
- **(1) Both echoes and creatures (C)** (2026-10-26). **Every mob enemy is "V", as an enemy.** Concept sheet: `docs/Design/Characters/V_turnaround.png` (shared by Crown 2026-10-26; to be added from the PC, as the cloud session cannot upload LFS files):
  - V: 7 ft, 160 kg; black suit; a faceless head and hands with a **galaxy pattern** on a violet body base; a gold tie with a star; dark smoke wisps rising off the body.
  - Built on **Manny and Quinn** (UE5's mannequins) as the main guide: their skeleton, rig and animations, with V's look on top.
  - Creatures add variety alongside the V mobs.
- **(2) Visible enemies + first strike (C)** (2026-10-26): enemies are **visible on the map**, standing guard or patrolling; you can approach or avoid them, and **touching one starts the fight** (Expedition 33 / Persona). **First strike:** hit an enemy with a field ability before it notices you and you **start with an advantage** (e.g. act first, or it starts damaged); get caught from behind and **the enemy gets the advantage**. The advantage's exact form and size are tuning (*tune*).
- **(3) Fight size: B, teams of three** (2026-10-26): enemies roam in **teams of up to three**; touching one pulls in its team. Fights are at most **3 v 3**. **If several enemy teams are caught in one fight** (e.g. overlapping radii, as with co-op players in D-003), the opponents are **the strongest member of each team**, keeping it to three.
- **(4) Bosses: B, phases** (2026-10-26): at set HP points the boss changes (new skills, element or stance shift, a line of dialogue); collected memories can alter phases (skip one, add a harder one). **How phases work in detail is to be discussed with Crown later** (open: how phases are authored, what can change, how memories plug in). Don't build beyond a placeholder until then.
- **(5) Mini-bosses: D, a mix chosen per trial** (2026-10-26): **elite V** (stronger, special gear, a distinct look), **story characters** connected to the lords (bringing lore, dialogue and reactions), and **big elemental creatures**.
- **(6) Enemy strength: A** (2026-10-26): set by the **difficulty picked at the door** and **where on the map** they are (zones nearer the boss are tougher; you can wander into a hard zone early). **No scaling to the party.** Harder zones roll gear from richer item pools, so they drop better loot.
**Notes for Claude:** the item-pool tags in `Resources_Design.md` §7.9 were written for floors (`Source.Floor.N`); on open maps they become zones (e.g. `Source.Zone.<n>`) crossed with difficulty. Propose the tag names when building. "strongest" needs one measure; propose the existing caliber (`GetTotalPool()`, `Resources_Design.md` §7.9) rather than a new score, and confirm with Crown when building. How the fight resolves more than three teams isn't ruled. first strike is a real-time field ability (`GASMigration.md`, field abilities). Today fights start on contact via `EncounterComponent`'s sphere; the advantage needs a hook into battle start (turn order / opening damage). mobs share one body (V on the Manny/Quinn skeleton), so variety comes from their **gear and class** (the same inventory/loadout system as players) and element tinting, not new models. Record further answers here; write `docs/Design/Enemies.md` when complete.
- **First strike (Crown, 2026-10-26):** the advantage is that **the side with first strike acts first** in the fight (both ways: you strike first, or the enemy catches you from behind).

### D-070 — Display name for the reshaped Silenced
**Status:** ANSWERED · **Raised:** 2026-10-26 (applying D-038) · **Blocks:** — (B-148 keeps the old name until answered)
**Question:** Silenced now seals half your Energy instead of blocking spells. What should the player see it called?
**Context:** The enum's display name is "Silenced (No EP Spend)" (`ESkillEffectType.h:129`), which no longer fits. D-038's notes say to ask Crown.
**Options:** A "Silenced" (keep the word, new tooltip: half your Energy is sealed) / B "Sealed" / C another name.
**Recommendation:** A, because the Darkness cap and the docs already say Silenced; only the tooltip changes.
**Answer (Crown):** **A** (2026-10-26): keep **"Silenced"**; the tooltip says half your Energy is sealed.


### D-071 — Revive spells: what do they do?
**Status:** ANSWERED · **Raised:** 2026-10-26 (applying D-043) · **Blocks:** — (B-150 builds the rest-spot revive without it)
**Question:** D-043 says spells can revive a fallen ally mid-fight, but there is no revive spell effect. How should one work?
**Context:** The old `Revive` skill effect was renamed `LastStand` (survive a killing blow, `ESkillEffectType.h:200-201`). Only Sapphire's `DefyDeath` item effect revives a dead ally (`ItemEffectType.h:86-90`).
**Options:** A a new `Resurrect` skill effect: brings a dead ally back at the effect's value as % of max HP, reusing Sapphire's revive path / B revive stays item-only (Sapphire) / C revive spells only out of combat.
**Recommendation:** A, because D-043 asks for revive spells and Sapphire's path already does the hard part (turn order, death state); the HP % is a per-spell value.
**Answer (Crown):** **A** (2026-10-26): a new **Resurrect** skill effect brings a fallen ally back at the effect's value as a % of max HP, reusing Sapphire's revive path.
### D-072 — Crystal tier limits per character
**Status:** ANSWERED · **Raised:** 2026-10-26 (Crown) · **Blocks:** —
**Answer (Crown):** (2026-10-26) **Per character**, a cap on how many crystals of each tier band can be equipped at once:
| Tier band | Max equipped per character |
|---|---|
| S | 1 |
| A / B | 2 |
| C / D | 3 |
| E / F | 4 |
**Notes for Claude:**
- Counts **utility crystals** in the character's **item slots**, per crystal (stacks count each one); attachments don't count (D-077). Enforced in loadout validation (`LoadoutComponent::GetValidationErrors` and the saved-loadout check), with a clear message.
- **Bands are combined** (Crown, 2026-10-26: "A or B = 2"): at most 2 crystals of tier A or B together; likewise C or D = 3, E or F = 4.
- The deckbuilder caps S-tier **equipment** at 3 per kind per deck and leaves crystals uncapped (`WoR_TCG_Pivot_Design.md` §8b rule 4); this is this game's own rule.
- Caps as named constants.

### D-073 — One loadout validator: which rule wins where the three disagree?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-028) · **Blocks:** B-028
**Question:** B-028 merges the three loadout validators into one rule set. Where they give different answers today, which answer should the one rule set give?
**Context:** the three are the saved-loadout (editor) check `FSavedLoadout::GetValidationErrors` (`FSavedLoadout.cpp:25`), the pre-fight check `ULoadoutComponent::GetValidationErrors` (`LoadoutComponent.cpp:478`, reports errors), and the pre-fight slot cleaner `CollectInvalidSlotFindings` (`:773`, empties bad slots). None has a test. The main disagreements:
- **Evolution spell cap:** the editor check allows 6 spells on any evolution; the pre-fight checks allow 1–6 by the evolution's tier (F 1 … A/S 6). An F-tier evolution with 3 spells passes in the editor and gets cut in the fight.
- **Broken Darkness:** the editor check uses BD rules when BD spell pools are authored; the pre-fight checks use them when the character *is* BD. A BD character with ordinary spells (or a non-BD character with BD pools) gets opposite verdicts.
- **Spell weight budgets** (24 normal, 48 BD) are checked before a fight only.
- **Resonator budget:** the editor check costs gear from the asset; the pre-fight checks cost the owned instance (its real crystal), and charge 1 for an empty weapon slot where the editor charges 0.
- **Editor-only checks:** a slot typed Weapon/Ring/Evolution with nothing in it; more than 6 abilities on a weapon or stone; item quantity 0–3 and more than 6 items; an empty ring slot. Before a fight these pass silently.
- **Slot-cleaner-only checks:** ability matches weapon type and wield mode; weapon/ring spells need a crystal of a matching element. The pre-fight error check doesn't report these, so slots get cleared with no "validation failed" message.
**Options:** A **one rule set, pre-fight answer wins** where they conflict (tier spell cap, BD-by-character, budgets on, instance costs), plus every check that only one of them has today, so the editor warns about what the fight would cut / B **merge only the two pre-fight checks** (error list + slot cleaner; adds the cleaner's coherence checks to the error list) and leave the editor check as it is / C go through the list one by one.
**Recommendation:** A, because the fight is what the player sees: an editor check that passes a loadout the fight then cuts is the bug the review found. It makes some existing authored loadouts show new editor warnings (e.g. an over-cap evolution); nothing changes in play. The deckbuilder has no loadout validator to borrow from (its decks are card lists).
**Answer (Crown):** **A** (2026-10-26): **one rule set; the fight's answer wins** where the checks disagree, plus every check only one of them has today, so saving a loadout warns about anything the fight would cut. Include the crystal tier limits (D-072).
- **You can't make an invalid loadout** (Crown, 2026-10-26): the same rule set runs **when you equip**, so anything that would break a rule is **refused or dropped** right there (with a short reason), rather than saved and warned about. A loadout that's already invalid (old save, changed rules) has its invalid parts dropped when loaded, with a message saying what was removed.
**Notes for Claude:** _optional_

### D-074 — Combat camera: the open parts
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (camera arc)
**Context:** the camera is already designed (`Resources_Design.md` "Combat camera — COMPLETE ARCHITECTURE", 2026-06-25, Expedition 33 model): one camera per character following shared mode rules (Turn: close over-the-shoulder; Action: the skill's Level Sequence; Not your turn: pan out; Defence possibly folded into Action, pending a play-test); viewer perspective follows what you control; centred/wider option and a camera-movement-off toggle. Nothing is built: the old `CombatCameraManager` was deleted (`Gaps/Roadmap.md`).
**Answer (Crown):**
- **(1) Skill cinematics: C** (2026-10-26): **reusable template sequences by skill type** (melee swing, ranged shot, area spell, buff, …) for most skills, and **hand-made sequences for signature moves** (lord skills, bosses, big spells). A skill references a template or its own sequence.
- **(2) Default turn view: B** (2026-10-26): **centred behind the acting character, a bit wider**, so the whole enemy team is visible. Settings offer **A** (close over-the-shoulder, Expedition 33) and **C** (wide tactical side-on). Skill cinematics still give the dramatic shots.
- **(3) When: A, a basic version soon** (2026-10-26): Turn view, pan-out and 2–3 template sequences (melee, ranged, spell) right after the current jobs (B-156, B-157).
**Notes for Claude:** written up in `docs/Design/CombatCamera.md`.

### D-076 — Hub controller: one class for interact and hub menus
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-003) · **Blocks:** B-003 hub part only (the combat HUD cut-over is done)
**Question:** the refactor's `AExplorationPlayerController` (E interacts with the nearest door / merchant / recruit; `OnPossess` makes sure the party exists) was meant to become `BP_HubPlayerController`'s parent. Since then `dev` gave that Blueprint a different C++ parent, `AWoRHubController` (hub modes and the CommonUI menu stack: `SetHubMode`, `PushMenu`/`PopMenu`). Both derive from `APlayerController`. How should they join?
**Context:** reparenting as the review notes say would drop `AWoRHubController`'s hub modes and menu stack. Today the Blueprint still does interact and `EnsureLocalParty` itself (its BeginPlay also adds the input mapping and the party display), so the hub works as before. The trial level's controller is not reparented either.
**Options:** A **fold the interact + party code into `AWoRHubController`** and retire `AExplorationPlayerController` (one hub controller) / B **make `AExplorationPlayerController` derive from `AWoRHubController`** and reparent the Blueprint to it (two layers) / C leave the hub on Blueprint interact for now.
**Recommendation:** A, because one controller owns the hub and nothing else uses `AExplorationPlayerController` yet; it's the simplest working shape. The deckbuilder has no hub controller to borrow from.
**Answer (Crown):** **A, one hub controller** (2026-10-26): fold the interact (E: door / merchant / recruit) and `EnsureLocalParty` code into `AWoRHubController`, retire `AExplorationPlayerController`; nothing changes for the player.
**Notes for Claude:** _optional_

### D-077 — Crystal tier caps: what counts as "a crystal"?
**Status:** DECIDED (applied in B-155) · **Raised:** 2026-10-26 (B-155) · **Blocks:** —
**Question:** D-072 caps how many crystals of each tier band a character can have equipped (S 1, A+B 2, C+D 3, E+F 4). A weapon or ring slot can hold more than a plain crystal (CL-30), so which of these count toward the caps, and at what tier?
**Context:** a slot holds one of: a **crystal**; an **augment stone** (stat stone or Ability Stone, weapons only, it has a tier); a **fusion** (two halves with at least one stat stone: stone + crystal, stone + stone, or stone + Ability Stone, CL-32); or an **evolution** (its own tier; the primary-slot evolution gives spells, CL-33). Plain crystals clearly count. The others aren't settled.
**Options:**
- **A** count **crystals and evolutions** (one each, at their tier); a **fusion counts once at its crystal half's tier** if it has one (stone-only fusions don't count); **augment stones don't count**.
- **B** count **plain crystals only**.
- **C** count **every attachment** (crystal, stone, fusion, evolution) at its own tier (a fusion at its higher half).
**Recommendation:** A, because it caps what channels elements and spells (crystals, the crystal half of a fusion, evolutions) and leaves stat stones, which are a different kind of gear, alone. The deckbuilder leaves crystals uncapped, so there's nothing to borrow.
**Answer (Crown):** (2026-10-26) **None of A–C: the caps are for crystals carried as items**, i.e. crystals in the character's **item slots** (thrown/used in a fight, `FCombatLoadout::ItemSlots`), not attachments. Crystals, stones, fusions and evolutions **attached to gear don't count**.
- **Counted per crystal (A):** a stack counts each crystal, so S = 1 means one S-tier utility crystal in total (a slot of 2 S-tier breaks it).
- **Naming:** crystals and stones used as items are called **utility crystals** / **utility stones** (player-facing; code names unchanged).
**Notes for Claude:** update D-072's note (it said "every crystal the character has equipped"); enforce in the one loadout rule set (B-028) on item slots, summing quantities per tier band. Utility stones: they're item-slot entries too, but D-072 named crystals; ask Crown before capping stones.
**Notes for Claude:** _optional_

### D-078 — Combat camera: three choices the basic version made
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-156) · **Blocks:** — (B-156 shipped with the recommendation; B-157 tunes it)
**Question:** B-156 had to pick three behaviours D-074 doesn't cover. Keep them?
**Context:** `CombatCameraDirector.cpp` HandleActionStarted / HandleActorTurnStarted. (1) With **camera movement off**, skill cinematics are skipped too (one static view). (2) **Enemy skills** play their cinematics like yours. (3) A cinematic runs **until it ends or the next turn starts** (not cut when the action's damage resolves).
**Options:** A keep all three / B movement-off still plays cinematics (only turn moves stop) / C enemy skills keep the pan-out view (no cinematics) / any mix.
**Recommendation:** A, because movement off is a motion-sickness setting (cinematics move the camera most), enemy cinematics are how you read an incoming hit (design §2, Defence mode), and cutting at damage would chop the end of most shots.
**Answer (Crown):** **A, keep all three** (2026-10-26): movement-off also skips cinematics; enemy skills play their cinematics; a cinematic runs until it ends or the next turn starts. **Keep for now, needs testing** (Crown): mark all three as untested defaults to check in PIE (`docs/PIE/B-156.md`). **Each also becomes a setting** (Crown, 2026-10-26): (1) cinematics on/off separate from camera movement, (2) enemy skill cinematics on/off, (3) skip a cinematic once its hit has landed. Defaults as above.
**Notes for Claude:** _optional_

### D-079 — The hub and the story
**Status:** ANSWERED · **Raised:** 2026-10-26 · **Blocks:** — (hub content, story, NPCs)
**Context:** the hub's function is designed (one hub, Nightreign Roundtable model; service NPCs Blacksmith, Jeweler, Stone Smith, Spiritualist, ability/spell shops: `Resources_Design.md` "SUPERSEDED — collapsed to ONE hub", "Hub NPCs", "Hub/Trial Merchants"). Not designed: the hub's place in the story and how the story unfolds.
**Answer (Crown):**
- **(1) The hub is fixed (A)** (2026-10-26). Progress-as-you-remember (B) shows up in other ways, not by rebuilding the hub.
- **(2) The premise:** the hub is **the Void Realm**. The characters went to a **bar**, passed out after drinking **void-spiked drinks**, and woke up in the Void Realm **with no memories**. **V is their guide.**
- **(3) Who V is** (Crown, 2026-10-26): **V is the Void**: the Void Realm itself, or the **Elemental Lord of Void**. **Void represents any and all concepts**, which is why V can appear as every mob enemy (D-069): the enemies are V taking on concepts.
  - **Hidden from players** (Crown, 2026-10-26): players are **not told** any of this. They have to **figure it out** from lore, dialogue and clues. Treat V's nature as a spoiler: nothing player-facing states it outright.
- **(4) Names** (Crown, 2026-10-26): the playable characters are **not actually lords**: "Elemental Lord" was a working label. Each will have a **real character name and lore** (to come). So there's no second "Void Lord" in the party; V is the Void. Read "lord" in D-032, D-033, D-069, `RunStructure.md` and `Enemies.md` as "the character tied to that element".
- **(5) How recovered memories show (D, all three)** (Crown, 2026-10-26):
  - **Memory journal**: every recovered memory, lore object and logged dialogue, rereadable in the hub.
  - **V reacts**: between runs V has new lines based on what's been remembered; the natural place for clues about who V is.
  - **Character conversations** (the heart of it): talks between characters unlock in the hub as memories return (Expedition 33 camp style); home of the cross-linked lore.
  All three key off the same unlock data as memories and reaction triggers (D-032 parts 4, 9).
- **(6) Hub NPCs: A, V in different forms** (Crown, 2026-10-26): every service NPC (Blacksmith, Jeweler, Stone Smith, Spiritualist, shops) is **V**, with a different outfit or prop per role. A quiet clue to V's nature (part 3). Lost-soul NPCs may come later.
  - So **V's look** (void material + clothing on Manny/Quinn, B-143/B-144) carries the guide, every mob and every hub NPC: Crown calls it **imperative** when the time comes.
- **(7) The opening: B** (Crown, 2026-10-26): the game **opens with waking up in the Void Realm**, no memories, V there. The bar night isn't shown; it's **recovered later as one of the first memories**.
- **(8) The goal is phased by the gate's theme** (Crown, 2026-10-26): the story advances **theme by theme**, following the trial variations (D-032 part 10). First you recover **lost Present memories**, next **memories of the Past**, and so on; what each phase is about depends on the trial theme. How a phase unlocks the next (e.g. enough memories recovered) isn't ruled.
**Notes for Claude:** the data assets keep their names for now (`DA_Character_FireLord` etc., D-058: existing names stay); rename when the real names exist. Player-facing text never says "Lord" for them. Written up in `docs/Design/HubAndStory.md`.

### D-080 — Loadout check in the editor: which spell discount?
**Status:** DECIDED (applied in B-161) · **Raised:** 2026-10-26 (B-028) · **Blocks:** —
**Question:** the fight lowers each spell's weight by a mastery discount from the character's world stats (Mind/Body/Spirit). In the editor there is no character progress. Which discount should the editor check use?
**Context:** `FSavedLoadout::GetValidationErrors` (`FSavedLoadout.cpp`) uses 0, the strictest budget. A Caster loadout that only fits with a high-mastery discount now shows an asset error, though in play the fight would accept it for a trained character (and cut it for a fresh one).
**Options:** A discount 0 (warns about what a fresh character would have cut) / B the biggest discount possible (warns only about what no character could use) / C skip the weight budget in the editor.
**Recommendation:** A, because authored loadouts are what new characters start with, so they should fit with no mastery.
**Answer (Crown):** **D** (2026-10-26, new option): the editor check uses **the authored stats of the character the loadout belongs to** (its authored world Mind/Body/Spirit) to compute the discount; **0 only when no character is attached**. So a high-stat boss can carry what its stats allow, and an under-stat starting character is warned. Players' in-game loadouts already use the live character.
**Notes for Claude:** the editor-side check (`FSavedLoadout::GetValidationErrors`, now in the B-028 rule set) needs the owning `UCharacterData` passed in (or looked up from the `UInventoryData` it sits on); fall back to 0 when absent. **Applied in B-161** (`UCharacterData::GetAuthoredSpellDiscount`).

### D-081 — Equipping: when is a pick refused, and when are other parts dropped?
**Status:** DECIDED (applied in B-159; gear-change drops are B-171) · **Raised:** 2026-10-26 (B-159) · **Blocks:** B-171
**Question:** D-073 says a loadout change that would break a rule is "refused or dropped right there". Which one, when?
**Context:**
- Some changes only add one thing: assign an ability or a spell, equip a weapon or ring. Refusing it is clean.
- Others make parts you already have invalid. Examples: swapping a weapon's crystal leaves its spells the wrong element; a weapon of another type leaves its abilities mismatched; a lower-tier evolution leaves too many spells.
- Today nothing is refused or dropped at equip time. The pre-fight cleaner (`ClearInvalidSlots`) empties bad slots when the fight starts.
- Where the cleaner runs in the code: `LoadoutComponent.cpp` `PrepareForBattle`; the equip paths are in `UInventoryComponent` (`AttachCrystalTo*` / `AttachEvolutionTo*`) and `ULoadoutComponent`.
**Options:**
- **A** refuse a pick that is itself invalid; let a change that breaks other parts go through and drop those parts, with a message naming them ("Fire Bolt removed: needs a Fire crystal").
- **B** refuse anything that would leave the loadout invalid; the player removes the clashing parts first.
- **C** never refuse; always drop what breaks, with a message.
**Recommendation:** A, because swapping gear is the common case and B makes it a chore of manual removals, while C lets a single bad pick (a Spear ability on a Sword) look like it worked. The deckbuilder has no loadout to borrow from (its decks are card lists).
**Answer (Crown):** **A** (2026-10-26): a pick that is itself invalid is **refused** with a reason ("Spear skills need a Spear"); a change that breaks parts you already have **goes through and drops them**, with a message naming each ("Fire Bolt removed: needs a Fire crystal").
**Notes for Claude:** _optional_

### D-082 — Weapon requirements reduce the weapon's output
**Status:** ANSWERED · **Raised:** 2026-10-26 (rules review, CL-40) · **Blocks:** — (B-162)
**Answer (Crown):** (2026-10-26) A weapon's Mind/Body/Spirit requirement is an **efficiency** rule: if you don't meet it, you **don't get the weapon's full output** ("if I don't have the requirements to use Excalibur, I won't get the maximum output from it"). Changes CL-40, which today only uses it for Broken Darkness strain.
**Notes for Claude:**
- Reuse the shipped **requirement-gap curve** (CL-39, per pillar, ±5) on the weapon's **own contribution** (its base damage and stats feeding attacks and abilities used through it), the same way skills use it (D-041). The skill's own requirement still scales the skill; they're two separate parts, so not a double penalty on the same number.
- **To confirm with Crown when building:** whether being *over* the weapon's requirement also gives the curve's bonus (skills do), or the weapon only caps at 100%.
- Fold into the stat-maths plan (B-146). Broken Darkness strain keeps using the weapon requirement as today.

### D-083 — A skill's source breaks mid-fight
**Status:** DECIDED (applied in B-163) · **Raised:** 2026-10-26 (rules review, page 01 gap) · **Blocks:** —
**Answer (Crown):** (2026-10-26)
- **A:** a skill whose source breaks **stays in the menu, greyed out**, with the reason ("Source broken"), as in the deckbuilder; it can't be used.
- **Broken crystal on a ring:** the ring is limited to **F-tier spells**; its higher-tier spells grey out.
- **Broken crystal on a weapon:** **no infusing** and **no elemental abilities** through that weapon; physical abilities and plain attacks still work.
**Notes for Claude:** today a broken attachment stays slotted and gives no spells (`AttachedItem.CanProvideSpells()`, `FCombatCapabilities.cpp`; `CrystalWear.md`). Change the menu to grey rather than hide, apply the F-tier allowance for rings, and block infusion / elemental abilities on weapons. Tests for each.
- **Crown, 2026-10-26 (clarified):** the F-tier allowance is only so the player can keep fighting. Once the fight ends, the broken crystal is **lost** (it leaves the ring), as built in `CrystalBreak` (`docs/Rules/05-economy-and-runs.md` ER-35). It still **pays out essence** like a dismantle (Crown, 2026-10-26: keep).

### Q-031 — What is "the ApplyDamage fallback" B-031 should delete?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-031) · **Blocks:** B-167
**Question:** the architecture review says "delete the `SubmitAction` sync router and the `ApplyDamage` fallback" with no detail. Which code is meant?
**Context:**
- `ApplyDamage` (`ActionExecutor.cpp` ~3085) is still called from four places. The likely "fallbacks" are the direct-damage paths that skip the defence window:
  - spells not yet converted to per-hit windows (AOE ~3635, Instant ~3740)
  - the "no DefenseSystem" branches (~1701 melee, ~3640 AOE, ~4011 projectile), which can't happen in a real game (it is always there)
- The "no DamageCalculator: deal at least 1" branches in `ApplyDamage`/`ApplyHit` are a third candidate.
- B-032 later merges `ApplyHit` and `ApplyDamage` anyway.
**Options:**
- **A** delete only the "subsystem missing" branches (no DefenseSystem / no DamageCalculator): dead in a real game, no content check needed.
- **B** also delete the unconverted-spell paths: needs an editor check that every spell asset is converted first (CC+UE).
- **C** leave all of it to B-032's merge.
**Recommendation:** A now, because it is dead code that hides setup errors (log an error instead), and leave the unconverted-spell paths to B-032, which has to touch them anyway.
**Answer (Crown):** **A** (2026-10-26): delete only the 'subsystem missing' fallbacks now (log an error instead); the unconverted-spell paths wait for B-032.
**Notes for Claude:** Applied in B-167 (refactor/B-167-remove-subsystem-fallbacks).

### Q-155 — Auto-equip and the utility crystal caps
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-155) · **Blocks:** — (B-155 shipped the check; this is the gap)
**Question:** auto-equip tops each item slot up to 3 crystals at the start of a fight, after the loadout check. Should it stop at the tier cap?
**Context:** `PrepareForBattle` (`LoadoutComponent.cpp` ~1460) checks and cleans the loadout, then `FCombatLoadout::ApplyAutoEquip` (`FCombatLoadout.cpp` ~609) refills every slot to `MAX_QUANTITY_PER_ITEM_SLOT` (3) from the inventory. So a loadout with 1 S-tier crystal passes the check and enters the fight with 3, unreported.
**Options:**
- **A** auto-equip stops at the band cap: it refills only as far as S 1 / A+B 2 / C+D 3 / E+F 4 allow (lowest slot first).
- **B** auto-equip refills as today, then the excess is dropped back to the inventory with a message.
- **C** auto-equip is exempt (it is a convenience; the cap is only for what you pick).
**Recommendation:** A, because the cap is a per-character rule (D-072) and refilling past it would undo it every fight; A never takes crystals out of the inventory that would then be handed straight back.
**Answer (Crown):** (2026-10-26) **Auto-refill happens only at rest spots**, not at the start of every fight; and it stops at the tier caps (A). Move `ApplyAutoEquip` out of `PrepareForBattle` to the rest-spot service; in fights you use what you carry.
**Notes for Claude:** _optional_

### D-084 — What a Perfect defense does (Block and Parry merged into Guard)
**Status:** DECIDED (applied in B-168) · **Raised:** 2026-10-26 (rules review, page 02 gap) · **Blocks:** —
**Answer (Crown):** (2026-10-26, revised the same day)
- **Block and Parry merge into one button, Guard** (Expedition 33 style). **Two defense buttons: Guard and Dodge.**
  - **Guard, normal timing:** a block, taking 50% (`BlockReduction`).
  - **Guard, Perfect timing** (ED-37 window): **a parry**: **takes less** (placeholder 25%, tune). **No built-in reflect** (revised by D-087): reflecting, healing etc. come from **effects attached to "on Perfect Guard"**.
  - **Dodge:** unchanged: takes nothing, no status buildup.
- "On perfect" triggers still fire, and Broken Darkness absorption still doubles on a Perfect (its block/parry absorption rates map to Guard / Perfect Guard).
- Counter-attacks (Expedition 33) can come later through "on perfect" gear effects.
**Notes for Claude:**
- The deckbuilder already calls its block "Guard" (`WoR_TCG_Pivot_Design.md`, "Block renamed Guard, 2026-10-09"). Player-facing "Guard"; code names can stay until touched.
- Input: one Guard action replaces the separate Block and Parry inputs (unique action names, D-063). Per-skill defense difficulty (ED-36) and allowed defenses (ED-38) collapse Block/Parry into Guard.
- Status through a defended hit (ED-34): Guard 50%, Perfect Guard 30%, Dodge 0% (today's Block / Parry / Dodge values).
- The menu's Defend action (D-050) is a separate turn action, unchanged.

### D-085 — Luck dodge
**Status:** DECIDED (applied in B-169) · **Raised:** 2026-10-26 (rules review, page 02 gap) · **Blocks:** —
**Answer (Crown):** **A, capped at 15%** (2026-10-26): Luck gives a chance to **automatically dodge** a hit, up to **15%** (lowered from the unused `LUCK_DODGE_MAX` 25%).
**Notes for Claude:** roll only when the hit would otherwise land (no successful Guard or Dodge press), so timing still matters most; use the per-battle random stream (B-010) so fights replay. Scale with Luck up to the 15% cap (curve `// TODO: tune`). Show it as a "Lucky dodge!" pop-up. Test with a fixed seed.

### D-086 — Ignore Defense: an amount, not all-or-nothing
**Status:** ANSWERED · **Raised:** 2026-10-26 (rules review, page 02 gap) · **Blocks:** —
**Answer (Crown):** **B** (2026-10-26): Ignore Defense becomes **an amount set per skill** (e.g. "ignores 30% of Defense"; 100% = today's full ignore), like the deckbuilder's Ignore Defense N.
**Notes for Claude:** **no separate job:** build it inside the GAS damage calculation (stat maths plan B-146 → B-041/B-042), not in today's `DamageCalculator`. Existing all-or-nothing skills map to 100%.

### D-087 — Reflection is an effect, not part of Guard
**Status:** DECIDED (applied in B-168) · **Raised:** 2026-10-26 (rules review, page 02 gap) · **Blocks:** —
**Answer (Crown):** (2026-10-26)
- **Guard and Perfect Guard only reduce damage.** Nothing is reflected by default.
- **Reflection is an effect you attach** to things (gear, skills, buffs), triggered e.g. **"on Perfect Guard: reflect damage"**; other on-Guard effects work the same way ("on Perfect Guard: heal", …).
- **Reflected damage is pure (B):** it hits for exactly the reflected amount, with no Defense, resistance or crit.
**Notes for Claude:**
- Already in code: `DamageReflect` / `ReflectPhysicalDamage` / `ReflectSpellDamage` effects (`ESkillEffectType.h:162-164`) and on-defense triggers `OnBlock`/`OnParry`/`OnPerfectBlock`/`OnPerfectParry`/`OnPerfectDodge` (`ESkillTrigger.h`). With Guard (D-084): add `OnGuard` / `OnPerfectGuard` **at the end** of the enum (append-only) and map the old Block/Parry triggers onto them (Block → Guard; Perfect Block, Parry, Perfect Parry → Perfect Guard); retire the old members with a comment.
- Remove the built-in `ParryReflect` from the Guard path; reflected damage skips Defense, resistance and crit.
- Fold into B-168 (Guard).

### D-088 — Actions per turn (discussion: one utility then one battle action; Turn Speed stays)
**Status:** DISCUSSED, NOT TO BUILD YET (Crown, 2026-10-26: a discussion for now; don't implement until Crown says) · **Raised:** 2026-10-26 (rules review, actions per turn) · **Blocks:** — (turn-system plan, B-170)
**Answer (Crown):** (2026-10-26)
- **Action pips:** each turn you get **action pips** set by your **Reflex** stat; each action (attack, skill, item, …) costs a pip, so a high-Reflex character can act **more than once per turn**.
- **No Turn Speed:** turn order becomes **fixed (round-robin)**, as the deckbuilder's turn belt. A faster-paced game; Reflex is what makes a character "fast".
- **Ice slows Action Speed** (the attack/spell animation speed stat): a full Ice bar is an Action Speed debuff for 2 holder turns.
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

### D-089 — Should Energy refill each turn?
**Status:** DISCUSSED, NOT TO BUILD YET (Crown, 2026-10-26: a discussion for now; don't implement until Crown says) · **Raised:** 2026-10-26 (rules review) · **Blocks:** —
**Question:** EP never refills on its own today (ET-29: only effects, items, drains and Broken Darkness absorption). Should it?
**Context:** Max Energy (Spirit) is the pool size; Efficiency (Mind) lowers costs. Deckbuilder: +1 Energy per turn, banked, no cap. Expedition 33: AP +1 per turn plus more from basic attacks and parries. With action pips (D-088), a basic attack that returns Energy becomes a use for spare pips.
**Options:** A no refill (current) / B a share of Max Energy each turn (e.g. 10%; Max Energy then also sets refill) / C Expedition 33 style: a small refill each turn **plus** Energy from basic attacks and Perfect Guards.
**Recommendation:** C, because it rewards good play (timing gives Energy back), gives basic attacks a purpose, keeps casters from running dry, and needs no new stat. Broken Darkness keeps its own absorption system.
**Answer (Crown):** **C** (2026-10-26): Energy **refills a little at the start of each of your turns**, plus extra from **basic attacks** and **Perfect Guards** (Expedition 33 style). Amounts are named constants (`// TODO: tune`); the per-turn share can scale with Max Energy. Broken Darkness keeps its own absorption system (no passive refill, ET-29 for BD).

### Q-159 — Do dropped utility crystals go back to the inventory?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-159) · **Blocks:** — (B-159 shipped without returning them)
**Question:** when a loadout's item slots are trimmed (over a tier cap, a quantity above 3, a duplicate crystal type), do the removed crystals return to the character's crystal inventory?
**Context:** today they just vanish from the slot (`ULoadoutComponent::DropInvalidParts`, the existing duplicate-type rule did the same). Crystals a fight's auto-equip put in a slot were taken out of the inventory (`FCombatLoadout::ApplyAutoEquip`), so dropping them loses them. Crystals in an authored loadout were never taken out of the inventory.
**Options:**
- **A** always return them to the inventory.
- **B** never (today's behaviour).
- **C** return only crystals that came out of the inventory (needs the slot to remember where its crystals came from).
**Recommendation:** A, because losing crystals to a rule the player didn't break on purpose feels like a bug, and authored loadouts are a dev-time case where an extra crystal in the inventory is harmless.
**Answer (Crown):** **A** (2026-10-26): utility crystals **removed by a rule** (over a cap, invalid loadout, duplicate type) are not destroyed, so they **return to the inventory, or storage if it's full** (D-097). Separately: crystals **destroyed** through use or wear are **lost**, as today, **except evolutions**, which count as equipment and are never lost that way.
**Notes for Claude:** Applied in fix/Q-159-return-dropped-crystals: `DropInvalidParts` returns dropped utility crystals to the inventory (storage when full waits for D-097: logged as lost for now). Evolutions: a broken one is no longer dismantled at the end of a fight (`CrystalBreak`) or when removal wear breaks it (`InventoryComponent`); it stays, broken, until repaired. Follow-up: Q-159c.

### Q-159c — Where a broken evolution goes
**Status:** DECIDED · **Raised:** 2026-10-26 (Q-159 follow-up) · **Blocks:** — (shipped with A)
**Question:** Q-159 says evolutions are never lost to wear. Before, a broken evolution was dismantled for essence at the end of a fight (including a primary-slot one such as Broken Darkness's) and when removal wear broke it. What happens to it now?
**Context:** shipped: it stays where it is (on the weapon, ring or primary slot), broken, with its spells greyed out (D-083; a broken primary evolution greys its Breakthrough spells). One removed by wear goes to the inventory at 0 durability. **Nothing repairs an evolution today**: the between-fight +10 repair covers gems only (`ApplyBetweenCombatRepair`), and paid repair is B-178 (blocked on B-090). So until then a broken evolution stays broken for good; for Broken Darkness that means no Breakthrough spells. Before Q-159 it was dismantled for essence instead.
**Options:**
- **A** stays where it is, broken, until repaired (shipped).
- **B** goes back to the inventory, broken, freeing the slot.
- **C** as A, but a `BDBreakable` evolution worn out by Broken Darkness is still destroyed (a BD mechanic).
**Recommendation:** A, and let the between-fight repair (+10) cover evolutions too, so a broken one comes back (otherwise it waits for B-178). Gear doesn't vanish and the player gets it working again.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** applied (fix/Q-159c-repair-evolutions): A, plus the between-fight +10 repair covers evolutions, attached and primary slot, broken included (`CrystalBreak::RepairOn`, `ULoadoutComponent::RepairActivePrimaryEvolution`). Paid repair stays B-178.
### Q-159b — Which parts go when a loadout is over a cap?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-159) · **Blocks:** — (B-159 shipped with A)
**Question:** when a loadout has too many abilities, or too many utility crystals of a tier band, which ones are dropped?
**Context:** today the last ones go: abilities after the cap (`ULoadoutComponent::DropInvalidParts`), and the later item slots in the band lose crystals first (`FLoadoutRules::TrimUtilityCrystalsToTierCaps`). The message names what went. Related, also shipped without a ruling: a pick that repeats a part already assigned (the same ability, weapon spell, ring spell or innate spell twice) is refused.
**Options:**
- **A** drop the last ones (today).
- **B** drop the lowest-tier / cheapest first.
- **C** don't drop: refuse the load and ask the player to pick (needs the loadout screen).
**Recommendation:** A, because it is predictable and the player only meets it with an old save or a changed cap; the message tells them what went.
**Answer (Crown):** **A** (2026-10-26): drop the last ones in the list, as built, with the message naming what went.
**Notes for Claude:** A as built (B-159); nothing to change.

### D-090 — A full status bar is a one-turn "status break"
**Status:** ANSWERED · **Raised:** 2026-10-26 (rules review, page 03) · **Blocks:** —
**Answer (Crown):** (2026-10-26) A full status bar's effect lasts **one** of the holder's turns, not two: a short, sharp **status break**. Applies to **every** cap (all elements, Ice, physical Slash/Pierce/Impact, Void, Broken Darkness's EP drain).
**Changes:** D-030's "2 turns" → **1** (`STATUS_CAP_TURNS = 1`); D-038 Silenced seals EP for 1 turn; **Wind delays the target's turn by one** (pushed one place back in the turn order; D-014, the D-088 pip idea dropped). **Water's break slows Action Speed and Spell Speed** for 1 turn (replacing "lowers healing", D-030); **Ice's break does the same and also deals damage** ("a heavier Water"; damage size a named %, tune). Slower abilities and spells also widen the defender's timing window against them (ED-35) (Crown, 2026-10-26). The holder-turn counting (D-030, D-051) is unchanged. How *big* each break is stays a tuning question (rules page 03 gaps).
**Notes for Claude:** player-facing word for a full bar's effect: "break" (e.g. "Burn break") is Crown's direction; confirm exact wording when the UI text is written.

### Q-161 — An inventory asset validated on its own: which character's discount?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-161) · **Blocks:** — (B-161 shipped with A)
**Question:** validating a `UInventoryData` asset by itself (not through its character) still weighs spells with no discount, so a high-stat boss's inventory shows as **Invalid** while the boss itself validates clean. What should the inventory on its own use?
**Context:** one inventory asset can be used by several characters. D-080's notes suggested looking the owner up from the inventory; with several owners there is no single answer.
**Options:**
- **A** discount 0 on its own, as today (the character's Validate is the real check).
- **B** look up every character that uses it and pass if any of them can carry it.
- **C** the same lookup, but the strictest of them (fail if any can't).
- **D** on its own, report the budget as a warning instead of an error.
**Recommendation:** D, because it keeps the strict number visible without marking a valid boss inventory as broken, and needs no asset-registry lookup.
**Answer (Crown):** **D** (2026-10-26): an inventory asset validated on its own reports the spell budget as a **warning**, not an error; the character's own validation (with its authored stats, D-080) stays the real check.
**Notes for Claude:** Applied in fix/Q-161-inventory-budget-warning: `UInventoryData::GetStandaloneValidation` / `IsDataValid`.

### D-091 — Fleeing a fight
**Status:** DECIDED (applied in B-176) · **Raised:** 2026-10-26 (rules review, page 04 gap) · **Blocks:** —
**Answer (Crown):** **B** (2026-10-26): you **can flee normal fights**, **not mini-bosses or bosses**. Fleeing **costs something**.
**Notes for Claude:**
- The cost isn't set: propose **losing a share of the run's Gold** (named constant, e.g. 10%, `// TODO: tune`) and confirm with Crown when building.
- After fleeing, the enemy team stays on the map (not beaten) and you're placed back in the trial; your party keeps its HP/EP.
- Stunned characters can't flee (D-023). A fight ending by flee mid-action follows D-075 (no infusion HP cost).
- Today `Flee` is an action type that isn't offered and does nothing (ET-21).

### Q-163 — Can you infuse through a ring whose crystal broke?
**Status:** DECIDED (applied in fix/Q-163-broken-ring-infusion) · **Raised:** 2026-10-26 (B-163) · **Blocks:** — (B-163 shipped with A)
**Question:** D-083 keeps a broken ring's F-tier spells usable and says nothing about infusing. Is the ring still offered as an infusion source (for attacks, abilities and spells) once its crystal is broken?
**Context:** today it is (`UActionExecutor::GetAvailableInfusionSources` offers the active / primary ring whenever one is equipped). A broken **weapon** crystal is not offered (D-083: no infusing). B-163 also read D-083's "no elemental abilities" as "no ability infused through that weapon's crystal", since abilities have no element of their own.
**Options:**
- **A** still offered (today).
- **B** not offered, like a broken weapon crystal.
- **C** offered only for the F-tier spells the broken ring still casts.
**Recommendation:** B, because it matches the weapon rule (a broken crystal gives no element to push), and the F-tier allowance is about casting the ring's own spells.
**Answer (Crown):** **B** (2026-10-26): a ring whose crystal broke is **not** an infusion source (no element to push), matching the weapon rule; its own F-tier spells still cast.
**Notes for Claude:** Applied: `GetAvailableInfusionSources` drops a broken ring; the AI and the menu follow it (the menu pins the spell's charge at 0). Two follow-ups raised as Q-163b.

### Q-163b — A broken ring's F-tier spells: infused through Innate, and their Generic element
**Status:** DECIDED · **Raised:** 2026-10-26 (Q-163 follow-up) · **Blocks:** — (Q-163 shipped with B)
**Question:** Once a ring's crystal breaks, may its F-tier spells still be infused another way, and what element does a Generic (inherit) spell from it take?
**Context:** a broken ring offers no spells (`FRingInventoryEntry::GetSpells`), so `ResolveSpellSource` no longer finds its F-tier spells on the ring and treats them as **Innate**. For a Resonator or a Generic that means no infusion at all (they have no Innate source). For a **Caster** with a ring in the primary slot it means the spell can be infused through the Caster's **Innate** element. Separately, `GetElementForSourceOption` still returns the broken ring's element for a Generic spell that inherits its element.
**Options:**
- **A** keep it: a Caster may infuse a broken ring's F-tier spell with their own element; a Generic spell keeps the ring's element.
- **B** a broken ring's spells can't be infused at all (any source), and a Generic spell from it is non-elemental (None).
**Recommendation:** B. A broken crystal gives no element (Q-163), so its spells cast plain; it also keeps the spell's origin honest (it still comes from the ring).
**Answer (Crown):** **B, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it. **Confirmed by Crown in chat (2026-10-26).**
**Notes for Claude:** Applied in fix/Q-163b-broken-ring-spells-plain (`ULoadoutComponent::IsSpellFromBrokenRing`).

### D-092 — Shields, Wards and Barriers: the deckbuilder's absorb system
**Status:** ANSWERED · **Raised:** 2026-10-26 (rules review, page 04 gap) · **Blocks:** —
**Answer (Crown):** (2026-10-26) Use **the deckbuilder's Shield, Ward and Barrier**, **including its pooling system**.
**Notes for Claude:**
- Source: `wor-deckbuilder/docs/reference/04-effects.md` ("the absorbing family", the Shield / Ward / Barrier verbs and their fields) and its charge **Pool** mode (`ChargeMode.Pool`, Pivot §8 item 184). Port the rules as written there, translating names to this codebase; where the 3D game needs something the card game doesn't have (real-time defense order, element damage types D-037), raise a `Q-<job>`.
- Order with Guard/Dodge (D-084): proposed, the hit is reduced by Guard first, then absorbed; confirm in the job.
- Build inside the GAS effects work (B-044 review → B-042), not on today's `SkillEffectManager`.

### D-093 — Wear and repair are run-only
**Status:** ANSWERED · **Raised:** 2026-10-26 (rules review, page 05) · **Blocks:** —
**Answer (Crown):** (2026-10-26)
- **Wear, breaking and repair happen within a run.** Durability carries between fights in a run (D-013 note).
- **Outside a run it resets:** when a run ends, every item's wear is restored. No repair is needed in the hub.
- **Repair cost: A**, in a run it costs **Gold, scaled by the item's tier** (at rest spots and vendors, D-047).
**Notes for Claude:** today wear lives on the item instances (`CrystalWear.md`); reset it at run end alongside the other run state (B-090). Hub repair (the Spiritualist) is then not needed; keep the Spiritualist's other services. Price curve named constants, `// TODO: tune`.

### Q-174 — Which skills count as "support" for no status buildup?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-174) · **Blocks:** — (B-174 shipped with A)
**Question:** the rules review ruled that heals, buffs and support skills never build status. Which skills does that cover, and does it also stop an effect that adds buildup on purpose?
**Context:** B-174 counts a skill as support when it is a Restoration or Enhancement spell (even one that also deals damage), or any skill with no damage and at least one buff or heal effect (even if it also debuffs enemies). Only the skill's authored `StatusBuildup` is zeroed; an authored `StatusIncrease` effect on a support skill still adds buildup (`SkillEffectManager.cpp:1621-1631`).
**Options:**
- **A** as built: school (Restoration / Enhancement) or no damage + a buff/heal effect; `StatusIncrease` effects still work.
- **B** like A, but a damaging Restoration / Enhancement spell keeps its buildup.
- **C** like A, and `StatusIncrease` effects on a support skill are ignored too.
**Recommendation:** A, because a `StatusIncrease` effect is a deliberate design choice, and a damage-dealing heal is rare enough to tune by moving it to another school.
**Answer (Crown):** **A** (2026-10-26), as built: support = Restoration/Enhancement school, or no damage + a buff/heal effect; deliberate StatusIncrease effects still apply.
**Notes for Claude:** _optional_

### D-094 — Upgrade materials in a run; "Reality essence" becomes "Unknown essence"
**Status:** ANSWERED · **Raised:** 2026-10-26 (rules review, page 05 gap) · **Blocks:** — (B-073)
**Answer (Crown):** (2026-10-26)
- **B: all three upgrade materials drop in runs**: Gear Essence, Skill Essence and the upgrade co-cost essence, **small amounts from every fight**, **bigger amounts from mini-bosses, bosses and chests**. Tallied and **paid at run end** (D-006). Typed essences (element / pillar / ability) still come only from dismantling. **Dismantling in a run stays instant** (Crown, 2026-10-26), so a drop can be dismantled to fund an upgrade on the spot; it isn't tallied.
- **Rename:** "Reality essence" (the half-cost on every upgrade, `Resources_Design.md` §5.3) is now **"Unknown essence"**.
**Notes for Claude:**
- Player-facing name now; code identifiers keep their names until a rename job (enums append-only: add, don't renumber).
- **Correction (checked 2026-10-26):** it's **one wallet**, `EEssenceType::Reality`: the upgrade half-cost (`EconomyService.cpp:1040-1058`), the spell-purchase half-cost (`:644`), and the Reality element's essence / wildcard. The rename applies to that wallet (player-facing).
- Amounts by zone/difficulty: named constants, `// TODO: tune`. Fold into B-073 (rewards).

### Q-175 — Does a bonus turn get its own free switch?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-175) · **Blocks:** — (B-175 shipped with A)
**Question:** ET-19 allows one free weapon or ring switch per turn. Does a bonus turn (ET-15, a real extra turn) give a fresh switch, or does it share the switch of the turn it came from?
**Context:** B-175 resets the switch in `ACombatOrchestrator::ProceedWithTurnStart`, which runs for normal and bonus turns (not Execution turns).
**Options:**
- **A** a bonus turn gets its own switch (as built).
- **B** a bonus turn shares the switch of the turn that earned it.
**Recommendation:** A, because ET-15 makes a bonus turn a real extra turn, and the rule exists to stop endless re-equipping inside one turn, which A still prevents.
**Answer (Crown):** **A** (2026-10-26): a bonus turn gets its own free switch, as built.
**Notes for Claude:** _optional_

### D-095 — The Reality element is "Unknown" to players
**Status:** ANSWERED · **Raised:** 2026-10-26 (Crown, rules review) · **Blocks:** —
**Answer (Crown):** (2026-10-26) **Anything named Reality is shown to the player as "Unknown"**: the element is the **Unknown element**, its essence is **Unknown (element) essence** (one wallet: upgrades, spell purchases, Reality items; D-094 A), and a character or enemy of that element shows its element as **"Unknown"**.
**Notes for Claude:**
- Player-facing text only: code keeps `Reality` (`ESpellElement::Reality`, `EEssenceType::Reality`; enums append-only, D-058). Put the word in the shared game-words `LOCTEXT` table (D-062) so every screen says "Unknown".
- Colour stays indigo (D-024 note). Docs and code comments may keep saying Reality.
- Fits the story's "figure it out" theme (D-079); don't explain what Unknown is in player-facing text.

### D-096 — Changing rows in combat
**Status:** DECIDED (applied in B-180) · **Raised:** 2026-10-26 (rules review, page 06 gap) · **Blocks:** —
**Answer (Crown):** **A** (2026-10-26):
- **Move row:** a character can step one row forward or back on their turn, **free, once per turn** (like weapon switching), into an empty cell or swapping with an ally there.
- **Push / pull skills:** skills (and effects) can push or pull an enemy between rows (e.g. a shield bash knocks them back).
**Notes for Claude:** the grid already supports one-row moves and swaps (PA-32, `CombatGridSubsystem`); add the menu option (placeholder UI) and a push/pull skill effect type (appended). Row bonuses (PA-30) apply from the new row immediately. Tests for move, swap, refused-when-blocked and push/pull.

### Q-176 — What does fleeing cost?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-176) · **Blocks:** — (B-176 shipped with A)
**Question:** D-091 says fleeing costs something and asked to confirm the cost when building. What should it be?
**Context:** B-176 takes a share of each fleeing party member's Gold (`FleeRules::GOLD_LOSS_SHARE`, rounded down, so a small purse can lose nothing). Gold sits on each battle pawn's wallet today and doesn't yet carry across the battle swap (B-090), so in play the cost only shows once run Gold persists. Also as built: the cost comes from each member's own wallet (there is no shared party purse yet); a purse under 10 Gold loses nothing; any party member who isn't stunned can flee for the whole party. Technical note: the encounter's rank reaches the battle stage through a new `PendingRank` on `UTrialRunSubsystem`, next to `PendingDifficulty` (transport across the level swap, an accepted exception to D-064; it moves with `PendingDifficulty` for co-op, D-065).
**Options:**
- **A** 10% of each member's Gold (as built).
- **B** a different share (say which).
- **C** a flat amount, or something other than Gold (e.g. HP, or taking a hit while escaping).
**Recommendation:** A, because it is simple, scales with the run, and can be tuned later through the one constant.
**Answer (Crown):** **A** (2026-10-26): fleeing costs 10% of each member's Gold, as built (one tunable constant).
**Notes for Claude:** _optional_

### Q-177 — A merge whose change does not fit
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-177) · **Blocks:** — (B-177 shipped with A)
**Question:** A merge now gives back its overshoot (ER-28). What happens when the change can't fit in your crystals?
**Context:** Each tier holds at most 20 crystals, shared by every gem type (stones have their own 20). The change can land on a tier the merge did not spend from: 3 F + 2 D for a C gives back one E and one F. If your E tier is already full (say, of Sapphires), the E has nowhere to go. As built, the whole merge is refused and everything is refunded, with a warning in the Output Log.
**Options:**
- **A** refuse the whole merge and refund it (as built).
- **B** do the merge and drop the change that doesn't fit.
- **C** do the merge and give the change in smaller crystals that do fit, dropping only what still cannot.
**Recommendation:** A, because nothing is ever lost silently and the player can make room and try again.
**Answer (Crown):** (2026-10-26) **Moot once D-097 lands:** a merge's change goes to the inventory if there's room, otherwise to **unlimited storage**, so a merge is never refused for lack of space. Until D-097 is built, keep A (refuse and refund).
**Notes for Claude:** _optional_

### Q-179 — What a trial vendor rolls from
**Status:** DECIDED (A as built in B-179; C later) · **Raised:** 2026-10-26 (B-179) · **Blocks:** — (B-179 shipped with A)
**Question:** Trial vendors now roll a random stock each run. What do they roll from, how much do they show, and does their stock run out?
**Context:** The rule says the stock comes from the trial's or zone's item pool, but no trial or zone has an item pool yet. As built: a vendor in a trial shows 8 lines picked at random from its own authored list (the same list the home shop sells), rolled once per run and the same all run. Each vendor rolls its own (keyed by its asset path), shown in the order they are authored, the same on every visit during the run. Bought items still never run out (ER-20). In a trial the shop still charges Prisms, because Gold isn't wired to shops yet (B-073).
**Options:**
- **A** roll 8 lines from the merchant's own list; stock never runs out (as built).
- **B** as A, but each line can be bought only once (or a set number of times) per run.
- **C** give each trial (`UTrialData`) its own item pool to roll from, and say how many lines to show.
**Recommendation:** A for now, because it needs no new content; C once trials have their own loot, which is when B is worth deciding.
**Answer (Crown):** **A for now** (2026-10-26): 8 lines rolled from the merchant's own list, unlimited stock, as built. Later (C): each trial gets its own item pool once trials have their own loot; buy limits (B) decided then.
**Notes for Claude:** _optional_

### Q-180 — Row moves: the open details
**Status:** DECIDED · **Raised:** 2026-10-26 (B-180) · **Blocks:** — (B-180 shipped with A)
**Question:** D-096's row moves are in. A few details had no ruling. Are these right?
**Context:** As built (option A):
- A **push or pull into a taken cell** does nothing (it doesn't swap the two).
- Push and pull are **neither a buff nor a debuff**: status immunity, Cleanse and the AI's debuff scoring ignore them, and there is no resist roll or boss/size immunity.
- A **fallen ally** stays on the grid and **blocks** their cell: you can't step into it or swap with them.
- A **bonus turn** (Extra Action) also brings back the free step.
- The step is instant: the character jumps to the new cell, with no animation or camera move.
- The deckbuilder has no rule to borrow here (it is one-on-one, with no rows).
**Options:**
- **A** keep all of the above (as built).
- **B** change some (say which): e.g. a push into an ally swaps them, push is a debuff that Cleanse and immunity can stop, bosses resist it, a fallen ally's cell is free.
**Recommendation:** A for now, because it is the simplest; bosses resisting a push is the likeliest change once bosses exist.
**Answer (Crown):** **A** (2026-10-26), with one rule made explicit: **movement is only forwards and backwards (between rows), never sideways** (no column change), for stepping and push/pull. **Swapping places with an ally only happens outside battle** (formation, at rest spots or the hub, D-042); in battle you can only step forward or back into an empty cell (Crown, 2026-10-26). Everything else as built: a push into a taken cell does nothing; push/pull aren't buffs/debuffs (bosses resisting can come later); a fallen ally blocks their cell; a bonus turn gives another free step; the step is instant for now.
**Notes for Claude:** Applied in fix/Q-180-no-swap-in-battle: a step into a taken cell is refused (`GetRowMoveRefusedReason`), no swap; moves were already row-only.

### Q-168 — Guard: the open details
**Status:** DECIDED · **Raised:** 2026-10-26 (B-168) · **Blocks:** — (B-168 shipped with A)
**Question:** Guard is in (D-084 / D-087). These details had no ruling. Are they right?
**Context:** As built (option A):
- **Perfect Guard takes 25%** of the hit (`PerfectGuardReduction` 0.75, a placeholder to tune).
- **Interruptable hits** (a spell's choke point) are stopped by a **Perfect Guard or a Dodge**. Before, a parry of any timing or a dodge stopped them; now a normal-timing Guard doesn't, and a perfect block does.
- **Timing difficulty:** a Guard is judged on the skill's **Block** difficulty; a Parry difficulty already set on a skill is ignored.
- **Broken Darkness:** a Guard absorbs at the block rate (5%), a Perfect Guard at the parry rate (10%) and that still doubles on Perfect, so 20%.
- **Reflect amount:** a reflect effect sends back its % of the damage the target actually took (after the Guard and the target's Defense), landing as pure damage.
- **Reflect lifetime:** an "on Perfect Guard: reflect" effect grants a timed Reflect buff, so it also reflects later hits until it runs out, not just that one hit.
- **Animation:** every Guard plays the block animation; the parry animation is no longer used.
- **Input:** the Parry key still works and is also a Guard press, until its key is removed from the combat input mapping in the editor (that asset needs the editor).
- The deckbuilder has no real-time defense, so there is no rule to borrow.
**Options:**
- **A** keep all of the above (as built).
- **B** change some (say which): e.g. a different Perfect Guard share; any Guard stops an interruptable hit; a Perfect Guard plays the parry animation; reflect is one hit only; Broken Darkness does not double twice.
**Recommendation:** A, except the animation: a Perfect Guard should probably play the parry animation (needs the press to know it is perfect, a small follow-up).
**Answer (Crown):** (2026-10-26, detail by detail)
- **Perfect Guard takes 0%** (B, like Expedition 33's parry); normal Guard stays 50%. Change `PerfectGuardReduction` to 1.0.
- **Interruptible (choke) hits:** stopped by a Perfect Guard or a Dodge, not a normal Guard (A, as built).
- **Approved by the reference-backed default (Crown, 2026-10-26: 'just go with them'):** Broken Darkness absorbs 5% on Guard / 20% on Perfect Guard (Expedition 33: +1 AP per parry); reflect effects send back their % of damage actually taken, as pure damage, as a timed buff (FF/Persona style); **Perfect Guard plays the parry animation**, normal Guard the block animation (Expedition 33 / Sekiro; small follow-up); the old Parry key works as Guard until removed in the editor.

**Notes for Claude:** Applied in fix/Q-168-perfect-guard-zero: `PerfectGuardReduction` 1.0. The parry animation on a Perfect Guard is B-182 (the animation plays at the press, before Perfect is known). The rest was as built. An "On Perfect Guard: reflect" is a timed buff, so it pays out on later hits that land.

### Q-168b — Status buildup through a Perfect Guard
**Status:** DECIDED (approved by the reference-backed default, 2026-10-26; Crown may change it) · **Raised:** 2026-10-26 (Q-168 review) · **Blocks:** —
**Question:** a Perfect Guard now takes no damage (Q-168). Does 30% of the hit's status buildup still land, as before?
**Options:**
- **A** 0%: a Perfect Guard negates the hit outright.
- **B** keep 30%.
**Recommendation:** A. Expedition 33's perfect parry cancels the hit and its effects entirely, one of its most praised mechanics; a hit that does no damage but still poisons feels wrong.
**Answer (Crown):** _A, approved by the reference-backed default; reply to change._
**Notes for Claude:** Applied in fix/Q-168-perfect-guard-zero (`PARRY_BUILDUP_MULTIPLIER` 0).

### Q-169 — Luck dodge: the open details
**Status:** DECIDED · **Raised:** 2026-10-26 (B-169) · **Blocks:** — (B-169 shipped with A)
**Question:** D-085's Luck dodge is in. A few details had no ruling. Are these right?
**Context:** As built (option A):
- **Curve:** the chance is Luck x 15%, reaching 15% at full Luck investment. Gear Luck past that adds nothing to dodging (it still lifts crits and crystal-wear skips).
- A lucky dodge **counts as a Dodge** for everything: no damage, no status, and it fires On Dodge triggers (and Broken Darkness treats it as a dodge, so absorbs nothing).
- It is rolled **per impact**, so a multi-hit attack can be partly lucky-dodged.
- It rolls for **everyone with Luck**, enemies included.
- As a Dodge it also **stops an interruptable (choke) hit**, and it applies to **AoE hits** too (AoE has no timed dodge).
- **Not rolled** for hits that skip the per-impact defense step: multi-entry spells, asset multi-hit spells, and abilities with no Impact notifies (their lumped hit). Adding it there is a small follow-up if wanted.
- The "Lucky dodge!" pop-up is a **placeholder** on-screen message (plus an Output Log line) until the combat HUD has floating text.
**Options:**
- **A** keep all of the above (as built).
- **B** change some (say which): e.g. gear can push the chance up to the 15% cap (the cap reached at the gear ceiling instead of full stat), a lucky dodge does not fire On Dodge or stop a choke hit, roll it for the lumped hits too, no lucky dodge against AoE, one roll per attack instead of per impact, enemies do not luck-dodge.
**Recommendation:** A. It is the simplest reading of D-085. The deckbuilder has no Luck dodge to borrow from.
**Answer (Crown):** **A, approved by the reference-backed default** (Crown, 2026-10-26: 'just go with them'): as built: counts as a full Dodge, rolled per hit, for everyone (enemies too), Luck x 15% capped at 15% (Fire Emblem / Darkest Dungeon style per-attack dodge chances, shown on screen).
**Notes for Claude:** As built; nothing to change.

### Q-147 — Does an evolution attached to a weapon or ring hold spells by its tier too?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-147) · **Blocks:** — (B-147 shipped without it)
**Question:** D-039 says crystals and evolutions hold spells by their tier. An evolution in the **primary slot** does (F 1 … S 6). An evolution **attached to a weapon or ring** still gets the holder's flat ceiling (6 for a weapon, the ring's cap for a ring). Should the attached one use its tier too?
**Context:** `CrystalEffectTable::ResolveSpellSlotCap` gives gem crystals their tier count and every other attachment (evolutions included) the caller's flat ceiling (`CrystalEffectTable.h`). D-039's answer did not separate attached from primary evolutions.
**Options:**
- **A** keep it: an attached evolution uses the holder's flat ceiling (as now).
- **B** by tier: an attached evolution holds its tier's count, like a primary one and a gem.
**Recommendation:** B, because it is the plain reading of D-039 and makes every container follow one curve. The deckbuilder has no separate rule.
**Answer (Crown):** **B, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it. **Confirmed by Crown in chat (2026-10-26).**
**Notes for Claude:** applied (fix/Q-147-attached-evolution-spells-by-tier): `CrystalEffectTable::ResolveSpellSlotCap` gives an attached evolution `SlotsForContainerTier(Evolution.Tier)`, the instance tier; every weapon and ring spell cap reads through it.

### D-097 — Storage (items), inventory (items you carry) and the pool (gear)
**Status:** ANSWERED (corrected 2026-10-26; details answered 2026-10-26) · **Raised:** 2026-10-26 (from Q-177) · **Blocks:** —
**Answer (Crown), Monster Hunter model (2026-10-26):**
| | Holds | Limit |
|---|---|---|
| **Storage** (MH Item Box) | **items**: crystals, stones, consumables | unlimited |
| **Inventory** (MH Item Pouch, carried on a run) | the items you take along | **10 slots** to start, raisable; one item type per slot, stacked to the item's max |
| **Pool** (MH Equipment Box, D-020) | **gear**: weapons, rings, evolutions, with upgrade levels | unlimited |
- **Equipped gear never takes inventory slots.** Essences/materials found in a run go to the run tally (D-006/D-094), not slots.
- **Found items** go **into the inventory if there's room, otherwise to storage**; the player can move items between them (at rest spots during a run). Nothing is thrown away.
- **Found gear** goes to the **run's gear list** (equip at rest spots), never to inventory slots.
- A merge's change goes to the inventory or, if full, to storage, so a merge is never refused for space (Q-177).
- **Restocking (Crown, 2026-10-26):** at rest spots, **auto-refill tops up the inventory from storage** (MH camp style), up to each item's stack and the crystal tier caps (Q-155). Rest spots in a run reach **home storage** (everything you own) (A). In a fight you use only what's in the inventory.
**Details (answered 2026-10-26):**
- **Found items are kept: B** (Crown, 2026-10-26): crystals, stones and consumables found in a run **stay after the run**; what doesn't fit the inventory goes to **home storage** for good.
- **Found gear is lost at run end, but its upgrades are remembered** (Crown, 2026-10-26): the upgrade progress a found weapon, ring or evolution reached is saved against that piece of gear, so when the player **finds it again in a later run, it comes back at the level they left it**. (Saved per gear definition, the item asset; part of the save keystone, B-090.)
- **Inventory growth: A** (Crown, 2026-10-26; Resident Evil 4's attaché case upgrades): extra inventory slots are **bought from a hub vendor with Prisms**, one upgrade at a time, rising price, up to a cap (`// TODO: tune`).
- Today: 20 crystals per tier shared by gem types, 20 for stones (Q-177 context); these caps go away for storage.

### D-098 — Boss phases
**Status:** ANSWERED (2026-10-26) · **Raised:** 2026-10-26 (Enemies §5 open) · **Blocks:** — (B-193 builds the shell)
**Answer (Crown):** (2026-10-26)
- **Overflow: B.** Damage carries past a phase point; a strong enough hit can push a boss through a phase. No damage cap at the line.
- **The changeover is smooth, not a cinematic.** The fight keeps going; the **weather** shows it:
  - In a normal fight the weather leans to whoever is winning (today's `UWeatherStateManager` HP blend), and a win turns it to your element.
  - **For a boss, each phase reached pushes the weather toward the boss's element and raises its intensity**: stronger weather = a more dangerous boss.
  - **The boss plays harder** from that phase on: more aggressive and more tactical (its AI steps up).
- **A phase, once reached, stays.** Healing the boss back above the line doesn't undo it (a one-way step).
- **Phase triggers aren't only HP.** Any condition can push a boss into its next phase, for example its absorption reaching max, one of its crystals breaking, being hit by infusions, and so on. These work as **hidden conditions**: the player isn't told and discovers them by playing.
- **What a phase changes is authored per boss** (2026-10-26): each boss has **phase entries**; each entry says its conditions and what changes when they're met: skills, element, AI aggression, weather intensity, and **optionally resistances / weaknesses**. Nothing changes unless the entry sets it, so one boss can shift its weaknesses and another keep them.
- **Boss styles (pillar 8), both ways** (2026-10-26): **mini-bosses** carry a **style tag** (*Planning*, *Timing* or *Both*) that picks a preset of phase entries (planning: statuses, element shifts, row pressure, easier-to-guard hits; timing: fast readable chains, tight Perfect Guard / Dodge windows). **Main bosses** are **hand-authored** phase by phase; the tag is only a guide for them. Hard timing windows stay on timing-styled content and higher difficulties.
- **Statuses on bosses: A** (2026-10-26; Elden Ring's rising status resistance): a boss's status bar can break it like any enemy, but **each break raises that bar's resistance for the rest of the fight**, so status builds stay useful without locking a boss down. The step per break is a named constant (`// TODO: tune`).
- **Long fights: A** (2026-10-26): no built-in enrage timer (hard timers punish careful play, pillar 8). **Turn count** is one of the conditions a phase entry may use, per boss, and can be hidden (pillar 10).
- **Hidden conditions as a wider principle (Shangri-La Frontier's secret quests and conditions):** Crown wants secret conditions for certain things across the game, not just bosses (see `Design/DesignPillars.md` pillar 10).
**Notes for Claude:** build on the reaction-trigger condition model (B-191, B-044, D-031): a phase is a condition list plus what changes (skills, element, AI aggression, weather intensity). HP threshold is one condition among many. Weather: add a boss override to `UWeatherStateManager` (boss element + intensity per phase) instead of the HP blend. (planning / timing), statuses on bosses, long fights.

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
- **What a field hit does** (Crown, 2026-10-27): a field hit **starts the encounter with turn advantage** (your side acts first, as first strike). No outright kills on the map and no damage carried in. (The earlier idea of an elemental field strike for elemental classes stays a note for later.)
- **Field skills as a whole: pinned for research** (Crown, 2026-10-27). Crown's ideas to explore: **movement through field skills** (flight, speed, etc.), and **evolutions changing how you move** (e.g. a character whose evolution lets them run at lightning speed). Research: `Design/FieldAbilities.md`. **Base movement: everyone gets a fast, free sprint** (Crown, 2026-10-27). **Evolutions give both** a small always-on movement bonus and one active EP-cost movement skill (Crown, 2026-10-27). **Movement order: speed and dash, then glide, flight last; dash evolves into sprint; enough speed lets you wall-run** (Crown, 2026-10-27). Where field-only skills are slotted waits on it.
- **Simplified (Crown, 2026-10-27), replacing the field-use / field-only-slot parts above:**
  - **Everyone:** a fast, free sprint.
  - **One active field skill per character**, set by **level / source** (where it comes from: innate, weapon, ring or crystal, evolution, and its level). It costs EP.
  - **Enhanced Sprint** is a field skill: it lets you **wall-run**.
  - **Evolutions** add a small always-on movement bonus and **augment the field skill to a degree** (e.g. the lightning character's skill becomes a lightning dash).
  - **Sources:** a field skill can draw on a source like battle skills do; **a crystal source still loses durability** when used (`CrystalWear.md`).
  - **Dropped:** separate field uses on battle skills, and field-only skill slots.
  - **Enhanced Sprint** is also **faster than the normal sprint** (Elden Ring Nightreign's surge sprint), on top of wall-running.
  - **Every element can do every movement type, each in its own way** (Crown, 2026-10-27, refines the line below): e.g. Lightning and Void can both teleport, but a Lightning teleport and a Void teleport look and behave differently. The **element decides the style**, not which movement type; an elemental source is still needed for the elemental movements (a Generic without one has only the normal dash and sprint).
  - **Source picks the skill: A** (Crown, 2026-10-27): the **source decides which field skill** you have (e.g. a Wind crystal glides, a Lightning one dashes) and **its level decides how strong** it is. The source also decides **what is available at all**: e.g. a Generic can't fly without an elemental source that gives flight.
  - **A range of movement** (Crown, 2026-10-27): dash, Enhanced Sprint, glide, flight, teleport and more. **All are locked until unlocked, except the normal dash**, which everyone has.
  - **Activation:** **double-click L3** (the sprint stick) uses your field movement skill (keyboard: a double-tap of the sprint key, `// TODO` confirm; unique input action name, D-063).
  - **Some field skills are locked** until unlocked: you must have **equipped or learned a specific skill** first. **Once unlocked, anyone can use it** with the matching source: e.g. after the lightning character's lightning speed is unlocked, **any character with a Lightning source** gets lightning speed as their field skill.
**Notes for Claude:** built on GAS (stage 4 abilities, B-043): one ability per skill with a battle and a field mode. Open details: dash-into-sprint meaning; which field skill each source gives and which are locked (`Design/FieldAbilities.md`).

### D-100 — Memories and boss triggers: two separate systems
**Status:** ANSWERED (2026-10-27, restructured the same day) · **Raised:** 2026-10-27 (Q-190 review) · **Blocks:** —
**Answer (Crown):** (2026-10-27; Shangri-La Frontier's hidden Unique Scenarios as the model, pillar 10) two systems that don't depend on each other:

**1. Boss triggers (the boss side): what changes a boss.**
- Each boss / mini-boss holds its own list of **"if these conditions, then this"** rules, e.g. *if the Garnet Lord fights this boss wearing their old sword → the boss is weaker*.
- Results: weaken, strengthen, skip a phase, add a phase, a dialogue line.
- **Not memory-based**: only what's true in the fight (who is there, what they wear and its level, hidden conditions). Same condition model as reaction triggers (B-191) and boss phases (D-098).
- Replaces "a memory changes the boss" (D-032 part 4, D-069 / Enemies §5).

**2. Memories (the story side): what you unlock.**
- Each memory has **unlock conditions; all must be met**. Any mix of: which character, gear equipped (and its upgrade level), where you are, what you beat, how you played (Shangri-La style, e.g. *beat a stronger enemy without being hit*), hidden conditions.
- **Not limited to bosses**: a memory can be earned anywhere (a map pickup, beating a boss, a moment, upgrading bound gear, NieR-style).
- **Per character**: the same situation can give each character a different memory, or none.
- **Memories can chain**: earning one can be a condition of the next (also a way for one story phase to unlock the next, HubAndStory §6).
- **One time only**: once in the journal, a memory isn't granted again (Q-190 changed).
- **Never missed for good** (Crown, 2026-10-27): conditions are checked again every run; failing them only means "not this run".
- **Co-op** (Crown, 2026-10-27): each player who meets the conditions gets the memory on their own save; character / gear conditions per player's character, shared conditions for everyone present; nothing first-come.
- **Rewards** (Crown, 2026-10-27): a hidden unlock can also grant a unique item, a skill, a title or a curse (penalty and key at once), set per unlock; different, not stronger.
- **Equip conditions** (Crown, 2026-10-27, clarified): a condition can be **a certain character equipping a certain skill, spell, ring, weapon or evolution**, optionally **relative to its requirements**: equipped while meeting them, above them by some amount, or **under** them (e.g. *this character equips this spell while below its requirements*). Upgrade level counts too.
- **Alternative routes** (Crown, 2026-10-27): an unlock can list several condition sets; meeting any one set opens it (all conditions within a set).
- **Never told, no hints** (Crown, 2026-10-27): the game doesn't show the conditions and gives no rumours, cues or "?" slots; they activate when they activate (pillar 10).

**Notes for Claude:** a memory = `UMemoryData` + a condition list (character, items + levels, location, beaten enemy, behaviour, prior memories, hidden). Sources (fragment pickups, boss defeat, trigger volumes, gear upgrades) are just *events that check* memory conditions; all grant through `CollectMemory` / the journal (B-190). B-191's `BossModifiers` on memories move to the boss's own trigger list (B-211). Design and research: `Design/HiddenQuests.md`.

### Q-044 — Re-applying the same effect: strength rule or replace?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Re-applying the same effect: strength rule or replace? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today a stronger re-apply replaces the value and refreshes, an equal one refreshes, a weaker one is ignored (`SkillEffectManager.cpp:146-218`, ET-39).
**Options:**
- **A** replace, as the deckbuilder (`04-effects.md`, `Stat` duration row).
- **B** keep the strength rule.
**Recommendation:** A. It matches D-012 / D-053 and needs no strength comparison (GAS has none). Defend and on-hit Burn still refresh, because they re-apply the same value.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044b — What a stack is
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** What a stack is? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today a stack is value x stacks, keeping the strongest per-stack value, and a weaker stack does not refresh. In the deckbuilder a stack is the held magnitude once more, every arrival refreshes, and the default cap is 1.
**Options:**
- **A** deckbuilder stacks.
- **B** keep ours.
**Recommendation:** A. One simple rule: what a stack adds is exactly what its expiry takes off (`04-effects.md` `MaxStacks`).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044c — When damage and heals over time tick
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** When damage and heals over time tick? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today damage over time ticks at turn end and heals at turn start.
**Options:**
- **A** all payouts at turn start.
- **B** keep today's split.
**Recommendation:** A. One moment fits D-051's single counting rule and gives exactly N ticks (`04-effects.md` "Lasts"). A DoT that kills at turn start ends that turn (ET-17 already handles it).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044d — Execution turns and turn-start effects
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Execution turns and turn-start effects? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today Execution turns skip turn-start effects (`CombatOrchestrator.cpp:882`, ET-16); D-055 says they count. The deckbuilder has no Execution turns.
**Options:**
- **A** an Execution turn runs the turn-start step (expiry and ticks) but still gives no choice.
- **B** it counts down durations but skips ticks.
**Recommendation:** A. B would give fewer than N ticks and break D-051.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044e — Where Barrier, Shield and Ward sit in a hit (D-092)
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Where Barrier, Shield and Ward sit in a hit (D-092)? See `docs/analysis/EffectsOverhaul.md`.
**Context:** The deckbuilder applies absorbers before Defense and Guard. Our Defense and Guard are percentages, so the order changes the numbers. D-092's note proposed Guard first.
**Options:**
- **A** deckbuilder order (absorbers first).
- **B** absorbers last, on the damage left after Guard and Defense (where Absorb and Reflect sit today).
**Recommendation:** B, a deliberate difference from the deckbuilder: "negate the next 5 damage" means exactly 5 HP you would have lost, a Perfect Guard keeps a shield longer, and it is the smallest change. Inside the absorbers keep the deckbuilder order (Ward, Shield, Barrier; oldest first).
**Answer (Crown):** **B, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044f — Multi-hit skills and absorb charges
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Multi-hit skills and absorb charges? See `docs/analysis/EffectsOverhaul.md`.
**Context:** A skill can land several impacts.
**Options:**
- **A** each impact is one damage instance and spends one charge.
- **B** one charge per skill.
**Recommendation:** A, the deckbuilder's rule (charges spend on every damage instance). Multi-hit skills become good shield breakers.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044g — "Doesn't stack" (D-053) on existing gear
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** "Doesn't stack" (D-053) on existing gear? See `docs/analysis/EffectsOverhaul.md`.
**Context:** D-053 adds a "doesn't stack" flag to effect definitions.
**Options:**
- **A** default off; Claude lists every Last Stand / Extra Action / fires-once definition for Crown to tick.
- **B** default on for those types in code.
**Recommendation:** A. Data decides (the deckbuilder's "authoritative data" rule) and Crown sees the list once.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044h — Conditions: "or", and % versus absolute HP
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Conditions: "or", and % versus absolute HP? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Our conditions allow Or and use % HP; the deckbuilder's nested conditions must all hold and use absolute numbers.
**Options:**
- Or: **A** retire Or for new data (a validator reports it; author two effects) / **B** keep Or and print "or".
- Numbers: **C** keep % / **D** absolute, as the deckbuilder.
**Recommendation:** A and C. A keeps D-031's tree honest; C differs on purpose because our HP grows with level and gear.
**Answer (Crown):** **A and C, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044i — Which effects a limited cleanse takes
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Which effects a limited cleanse takes? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today Iolite's "up to N" has no stated order (`ItemExecutor.cpp:1014-1045`).
**Options:**
- **A** newest first, a stack whole, gear exempt (D-015).
- **B** oldest first.
**Recommendation:** A, the deckbuilder's rule (`04-effects.md` "The uses verbs and Remove effect"), with no picking UI for now.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044j — Triggers: our dispatcher or GAS abilities?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Triggers: our dispatcher or GAS abilities? See `docs/analysis/EffectsOverhaul.md`.
**Context:** GAS abilities are stage 4 (B-043).
**Options:**
- **A** keep our dispatcher in B-042 and send GameplayEvents so B-043 can move triggers later.
- **B** move triggers to passive abilities in B-042.
**Recommendation:** A. Doing it in stage 3 doubles its size and risk.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-044k — Land B-062 (identity rules) before GAS, or inside it?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Land B-062 (identity rules) before GAS, or inside it? See `docs/analysis/EffectsOverhaul.md`.
**Context:** B-062 builds D-011 / D-012 / D-051 / D-053 / D-055 on today's code.
**Options:**
- **A** build B-062 now, with the shared turn-count helper; B-042 then ports the same tests.
- **B** fold B-062 into B-042.
**Recommendation:** A. Players get the rules sooner and B-042 becomes a pure port. Absorb stays in B-042, as D-092 says.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, deckbuilder rule or pillar fit as given above). Crown may change it.
**Notes for Claude:** _optional_

### Q-182 — When does a Perfect Guard show the parry animation?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-182) · **Blocks:** — (B-182 shipped with A)
**Question:** one Guard button gives a block or, on Perfect timing, a Perfect Guard (Q-168). The press happens before the game knows the timing. When does the parry animation play, and what does B-182's "one animation per press" mean?
**Context:** the press already plays the block montage (`UDefenseSystem::PlayDefenseAnimation`). Perfect is known only when the impact resolves (`MatchAndConsumeInput`).
**Options:**
- **A** block on the press; switch to the parry montage when the impact resolves Perfect (one montage visible at a time; never one added per impact).
- **B** play nothing on the press; play block or parry at the impact (the press feels laggy).
**Recommendation:** A. It is how Sekiro's single block/deflect button works: the guard pose on the press, the deflect animation and spark on a perfectly timed hit; it is widely praised for feeling responsive. Not lore.
**Answer (Crown):** **Changed by Crown** (2026-10-26): **one Guard animation**, the **block**, for every Guard, normal or Perfect; no parry montage, no switch mid-animation. A **Perfect Guard shows with a VFX flash and its own sound at the timing moment** (Expedition 33's parry flash; Sekiro's deflect sparks). Replaces the default A.
**Notes for Claude:** applied (fix/B-182-perfect-guard-parry-anim): `GuardRules::ImpactAnimationOverride`, called in `UActionExecutor`'s per-impact defense resolve. Crown's change applied in B-202 (fix/B-202-one-guard-anim-perfect-flash): every Guard (and a legacy Parry press) plays the block montage; a successful Perfect Guard calls `UDefenseSystem::PlayPerfectGuardFeedback` (flash + sound from the defender's `UCosmeticsData`, engine stand-ins until the art exists). The parry montage getters and `ImpactAnimationOverride` are flagged OBSOLETE. The flash and sound sit per character on `UCosmeticsData` (beside `BlockMontage`), every asset defaulting to the same stand-in, so the game has one shared Perfect Guard look, as Sekiro's deflect spark is the same for every fighter; a character can override it later.

### Q-040 — Switch the effect read sites to tags in stage 1?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-040) · **Blocks:** — (B-040 shipped with A)
**Question:** GASMigration stage 1 step 2 says the read sites (immunity checks, the AI's stunned/silenced checks, UI icons) switch to tag queries. With no Ability System Component yet, the tags are built from the same enum effects. Switch now or with the ASC?
**Options:**
- **A** ship the tags, the maps and `wor.CompareTags` now; switch the read sites in stage 2, when the ASC owns the tags.
- **B** switch the read sites now to a tag container rebuilt from the enum effects.
**Recommendation:** A. In GAS projects such as Epic's Lyra the owned-tag container lives on the ASC and every query reads it there; a stand-in container now would be a second code path to remove later. `wor.CompareTags` and `WoR.Tags` already prove the two answer the same. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-040-gas-tags). Stage 2 (B-041) switches the read sites when the ASC lands.

### Q-040b — Where do immunity and DOT tags sit in the tag tree?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-040) · **Blocks:** — (B-040 shipped with A)
**Question:** GASMigration §4 proposes `Effect.Family.{DOT, Immunity, …}`. B-040 needs tags a character *carries* while warded or burning. Use the family branch, or separate owned-state branches?
**Options:**
- **A** owned-state tags: `Status.DOT`, `Status.HealBlock`, and a top-level `Immunity.{All, Stun, Silence, DOT, Element.<element>}`. `Effect.Family.*` stays for what a GameplayEffect *is* (its asset tags), added with the GameplayEffects in B-042.
- **B** put everything under `Effect.Family.*` (one branch for both what an effect is and what a character has).
**Recommendation:** A. GAS keeps the two apart: a GameplayEffect has asset tags (what it is) and granted tags (what its target then owns), and immunity is checked against owned tags (Application Immunity). Lyra and the community GASDocumentation both name owned state separately from effect families. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-040-gas-tags); GASMigration §4 tree updated.

### Q-060 — What does a failed item use cost?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-060) · **Blocks:** — (B-060 shipped with A)
**Question:** an item use whose effect cannot land (no valid target) used to lose a charge. B-060 keeps the charge. Does the turn still end, and when does an area item count as used?
**Options:**
- **A** the turn ends (like a missed attack) and the charge is kept; an area item is used when it lands on at least one target. A use that reaches its target but is blocked there (heal-blocked, resisted) still counts as used.
- **B** a failed use is refused before it happens and the turn is not spent.
**Recommendation:** A. In turn-based RPGs a committed action that whiffs still spends the turn (a missed attack in Pokémon or Final Fantasy costs the turn), while the item itself is only spent when it is actually applied; an all-target item (Final Fantasy's Megalixir) is spent once for the whole group. B needs a pre-check in the menu, which is the UI's job (the command menu already filters invalid targets). Not lore.
**Answer (Crown):** **Changed by Crown** (2026-10-26): a failed use **loses the charge**, like a thrown item that misses; the turn ends. (An area item that lands on no one is a miss: charge lost.) The use still reports failure. Replaces the default A.
**Notes for Claude:** applied (fix/B-060-item-use-success). `OnItemUsed` no longer fires for a failed use (no C++ binds it; a Blueprint binding cannot be checked from code). Crown's change applied in B-201 (fix/B-201-failed-use-spends-charge): `UActionExecutor::ExecuteItem` spends the charge whenever the item is used from the battle loadout, landed or not.

### Q-041 — Should the stat getters read GAS attributes in stage 2?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-041) · **Blocks:** — (B-041 shipped with A)
**Question:** GASMigration stage 2 step 2 says the `UCharacterDataComponent` getters read from attributes and HP/EP writes go through the ability system. Until stage 3, nothing but the getters' own sources (gear, `SkillEffectManager` effects) can set those attributes. Switch now or with the GameplayEffects?
**Options:**
- **A** stage 2 mirrors: attributes are filled from the getters (`UWoRAttributeSyncComponent`) and proven equal (`wor.CompareAttributes`); the getters and HP/EP writes switch in stage 3, when GameplayEffects write the attributes.
- **B** switch the getters now to read the mirrored attributes.
**Recommendation:** A. In GAS the value an attribute holds comes from GameplayEffects (Epic's Lyra drives health and damage only through GEs and attribute sets); a getter reading a copy of itself adds a second path that can only go stale, with no gain until GEs exist. This keeps the project's incremental rule: build alongside, prove, then switch. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-041-gas-attributes). B-042 (stage 3) switches the getters, the HP/EP writes and Q-040's read sites. Two smaller choices in the same build: the Vitals set does not clamp (EP overloads above max for Broken Darkness; clamping comes with the GEs that write it), and every combat pawn uses Mixed replication for now; GASMigration §1's Minimal mode for AI pawns is set when co-op is built (replication is not exercised before then).

### Q-062 — Build the shared turn counter (D-051, D-055) in B-062 or B-063?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-062) · **Blocks:** — (B-062 shipped with A)
**Question:** Q-044k said B-062 could ship the shared turn counter (`FTurnSpan`, `docs/analysis/EffectsOverhaul.md` §4). B-062 is about who an effect belongs to; B-063 is the first job that needs the counter (every full bar lasts 2 of the holder's turns). Which job builds it?
**Options:**
- **A** B-063, with its first user; B-062 keeps today's counting.
- **B** B-062, before anything uses it.
**Recommendation:** A. Building a shared helper together with its first real user is the usual way to keep it honest (the "rule of three" / YAGNI practice); D-030 asks for one helper, and B-063's 2-turn tests are the fixtures that prove it. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-062-effect-identity). B-063 builds `FTurnSpan` with D-051 / D-055 and Q-044c / Q-044d.

### Q-062b — Who is the source of a skill's effect?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-062) · **Blocks:** — (B-062 shipped with A)
**Question:** D-053 makes the item or skill an effect's source. For a skill, is the source the skill alone, or the caster plus the skill? (Before B-062, the same effect from any caster or skill merged into one.)
**Options:**
- **A** caster + skill: two enemies casting the same Slow on you give two Slows; one enemy re-casting refreshes its own.
- **B** the skill alone: anyone's cast of that skill refreshes the one copy.
**Recommendation:** A. It is the deckbuilder's model (each card played is its own source, `04-effects.md` `Stat` duration row) and the familiar MMO one: in World of Warcraft each player's damage over time on a target is its own debuff, and per-caster debuffs are widely understood. It also matches D-012 (each attacker's full bar adds its own). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-062-effect-identity): `ApplyEffect` keys an unstamped effect on the source actor + ability name. Gear is keyed on the item object (`GetUniqueID` + name); on-defense gear adds the wearer, so two wearers of copies of one item stay two sources. `PersistentID` is not needed while one character can't hold two copies of the same item at once. Stacks: a stackable re-apply now takes the new per-stack value and always refreshes (part of Q-044b); the default cap of 3 stays until B-042. No effect definition sets `bDoesNotStack` yet (Q-044g: defaults off; Crown ticks it per definition). Once per match (D-011) is spent per character **and source**: one character with the same once-per-match effect on two items gets one use from each (two sources add, D-053); a doesn't-stack effect gives one use per character whichever item brings it.

### Q-063 — A Burn that lands on the holder's own turn: when does it pay?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-063) · **Blocks:** — (B-063 shipped with A)
**Question:** D-051 says the landing turn counts as turn 1 when it is the holder's own, and payouts happen at turn starts. A 1-turn break landing on the holder's own turn (a counter-hit, a reflected hit) then has no turn start left to pay at, so it would pay 0 times, while D-051 says exactly N. Which gives way?
**Options:**
- **A** it pays once on landing; it still ends as the holder's next turn starts.
- **B** it pays 0 times (the counting rule wins).
- **C** the landing turn doesn't count for payout effects; it lasts and pays through the next turn.
**Recommendation:** A. It keeps both halves of D-051: the landing turn counts and there are exactly N payouts. A damage-over-time that does nothing at all would read as a bug. Pokémon's poison and burn always deal their damage the turn they land, which players accept without thought. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-063-status-breaks): `ApplyTriggeredSkillEffect` clears `bDelayFirstExecution` when the target is the current turn's actor.

### Q-063b — Void's break: what is the "minimal" version?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-063) · **Blocks:** — (B-063 shipped with A)
**Question:** B-063 says Void's `RandomSkill` break is "minimal". A real loss of control (the holder's action picked at random for it) needs the command menu, the AI and the executor. Build that inside B-063, or keep the marker?
**Options:**
- **A** keep the 1-turn marker for now (counted by the shared rule, logged, no forced action); build the random action as its own job (B-199).
- **B** build the forced random action in B-063.
**Recommendation:** A. It is the incremental rule (CLAUDE.md: build alongside, prove, then extend); a forced action touches the player's menu, the AI and the executor, which is its own job with its own PIE check. When built, Final Fantasy's Confuse is the familiar model (the game picks for you for a turn). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-063-status-breaks). Two more choices made in B-063:
- Only full-bar effects use the shared counter (`FTurnSpan`) for now; every other timed effect keeps today's end-of-turn countdown until B-042 moves effects to the GAS tracker (D-051 "everywhere" lands there).
- Execution turns still skip the turn-start step (Q-044d A is built with B-042), so a break held across an Execution turn counts that turn but has no payout or expiry on it. So a Burn landed off-turn whose holder's next turn is an Execution turn pays 0 times (`CombatOrchestrator.cpp:899-903` skips the start step; `SkillEffectManager.cpp:999` still counts the turn); B-042 fixes it.
- Wind's delay hands the turn to the next in line even when that one has used its turns this round (otherwise Wind does nothing in a 1v1); this can exceed a speed ratio, which matters only if D-049 caps turns.

### Q-113 — Infused hits: is the Status Multiplier meant to count twice?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-113) · **Blocks:** — (B-113 shipped with A)
**Question:** B-113 (from rules review C3-3) read the double use of the Status Multiplier on infused hits as a bug. An infused hit's charge band is `band × (1 + StatFraction(Status Multiplier))` (`Combat/Actions/ActionExecutor.cpp:4781-4805`), and `AddStatusBuildup` then multiplies by the Status Multiplier again (`Skills/Effects/StatusBuildupManager.cpp:393`). But `docs/Architecture/InfusionSystem.md` (§ effect model, 6-5) sets this up on purpose: every charge band is "stat-scaled" by the same surcharge. Damage does the same: its charge band is scaled by the damage stat, and the damage formula applies that stat again. The EP cost carries the same surcharge. Keep it, or apply the stat once?
**Options:**
- **A** keep it as designed: the charge band's stat surcharge is a separate bonus for a character who invests in the stat; pin it in a test.
- **B** apply it once for status only: drop the surcharge from the status band (status then differs from damage and cost).
- **C** apply it once everywhere: drop the surcharge from damage, status and cost (a rework of the infusion rules).
**Recommendation:** A. It is what the infusion rework designed and documented, and it keeps status like damage (both apply their stat in the charge band and again in the base formula); the EP cost carries the surcharge once, so stacking the stat raises an infused hit's effect and its cost together. Stacked multiplicative bonuses that each multiply the whole are a well-understood build rule (Path of Exile's separate "more" multipliers). B would make status the one channel that behaves differently. C is a balance rework, not a bug fix. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (fix/B-113-status-mult-once): no game change. `WoR.Status.InfusedBuildup` pins each layer on its own (the charge band's surcharge; the Status Multiplier on landing), not the full hit path; the element-keyed Status Multiplier buff applies once, on landing only. And the `GetChargeStatusMultiplier` header says so. If Crown picks B or C, change `GetChargeStatusMultiplier` (and for C `GetChargeDamageMultiplier` and `ComputeInfusionCostMultiplier`) and flip the spec's expectations.

### Q-148 — Silenced's seal: do EP drains take sealed EP, and how does the seal round?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-148) · **Blocks:** — (B-148 shipped with A)
**Question:** D-038 seals half of max EP against *spending*. Two things it doesn't say: (1) EP *losses* (a Broken Darkness drain, an Onyx drain, an EP-drain effect) aren't spends; do they still take from the sealed half? (2) With an odd max EP (101), is the seal 50 or 51?
**Options:**
- **A** drains take from the whole pool, sealed part included (the seal only limits what you can spend); the seal rounds to the nearest point (101 seals 51).
- **B** drains take only the unsealed part (the seal also protects EP); round as A.
- **C** as A, but the seal rounds down (101 seals 50).
**Recommendation:** A. A seal that also shields EP from drains would make Silenced partly a buff. MP drains in Final Fantasy (Osmose, Rasp) take from current MP whatever the target's status, and players read "can't use it" and "can lose it" as separate things. Rounding to the nearest point is how the game rounds its other shares (`FMath::RoundToInt` throughout). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-148-silence-seal): drains still call `ServerSpendEnergy` on the whole pool (`SkillEffectManager.cpp` DrainEnergy/EnergyDrain, `BrokenDarknessManager.cpp:571`, `:777`); `GetSealedEP` uses `RoundToInt`. The command menu doesn't grey skills by EP yet, so "the menu reads spendable EP" (D-038) lands with the new HUD (B-111 / B-105).

### Q-071 — What level do AI-controlled party members play at, and how do you take over an AI member?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-071) · **Blocks:** — (B-071 shipped with A)
**Question:** D-044 says AI allies play at "a fixed, good level". Which of the four AI levels (Easy / Medium / Hard / Expert, `AI/EAIDifficulty.h`)? And D-026 lets you switch a member on its turn: an AI member's turn has no menu, so how do you take it over?
**Options:**
- **A** Hard (scores every choice, heals below 40% HP, tries to defend 85% of hits with good timing). Take-over of an AI member: a placeholder console command until the new HUD gives it a button.
- **B** Expert (95% defence, near-perfect timing): allies outplay the player.
- **C** Medium: allies make visible mistakes.
**Recommendation:** A. Ally AI in party RPGs is tuned to be competent but not better than the player: Persona 5's and Dragon Quest XI's AI tactics are praised for allies who heal and exploit weaknesses sensibly, while allies who play perfectly make the player's own turns feel optional. Hard is "optimal play" without Expert's perfect defence. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-071-party-into-battle): `AIConstants::ALLY_AI_DIFFICULTY = Hard` (TODO: tune), read through `UAIDecisionManager::GetDifficultyFor`. Handing a member to the AI is a command-menu button (Let AI Play); taking an AI member over on its turn is `wor.Combat.SwitchControl` until the new HUD (B-105) adds a button. The D-045 setting lives on the party and isn't saved yet (Settings screen, D-024).

### Q-071b — Does a gearless leader sit a fight out too?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-071 review) · **Blocks:** — (B-071 shipped with A)
**Question:** D-016 says a Generic or Resonator with no gear is left out of a fight and the fight starts with the others. The leader is always player-controlled (D-004). If the leader has no gear, do they sit out, and who do you play?
**Options:**
- **A** the leader sits out like any gearless member; you play the first member set to You (or the first who fights).
- **B** the fight can't start until the leader has gear.
**Recommendation:** A. It is D-016 read as written, and it never blocks a run. Dragon Quest XI dropped the old rule that a fallen hero ends the battle: the fight goes on while anyone stands, and players welcomed it. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-071-party-into-battle): `ABattleGameMode::BootstrapCombat` possesses the leader's pawn when it fights, otherwise the first Player-set member, otherwise the first spawned.

### Q-195 — What does each hub service NPC do before the service screens exist?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-195) · **Blocks:** — (B-195 shipped with A)
**Question:** HubAndStory §3 / D-047 / D-048: one V-as-service NPC each for the Blacksmith, Jeweler, Stone Smith, Spiritualist, ability and spell shops and the loadout vendor, each opening the existing placeholder screen. Only the shop window exists (buying); upgrade, repair, merge and the purchase loadout aren't built. What does each NPC open, and how is V shown?
**Options:**
- **A** Every service is the existing hub merchant NPC with a merchant type (Stone Smith and Loadout Vendor added); pressing E opens the shop window with that merchant's stock, and each later service screen hooks into the same NPC when it's built. V's look per service is one mesh slot on the merchant data (empty: the placeholder cube). A coverage check lists any service with no NPC in the hub.
- **B** A new NPC class per service.
- **C** One V NPC with a menu of every service.
**Recommendation:** A. Elden Ring's Roundtable Hold and Nightreign's hub put each service at its own fixed NPC (Hewg, the Twin Maiden Husks), and players find it easy to learn; Hades' House shows one familiar face per role. Reusing the merchant NPC keeps one interaction path, and one mesh slot is all V's outfits need until the meshes exist (B-143 / B-144). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-195-hub-services): `EMerchantType::StoneSmith` / `LoadoutVendor`, `UMerchantData::VOutfitMesh`, `AMerchantInteractable::GetMissingServices`, `wor.Hub.Services`. Known gaps: the NPCs only sell for now (upgrade, repair, merge, reroll and the purchase loadout screens come with their jobs); the merchant data assets and hub placement are authored in the editor.

### Q-194 — How do the journal, V's lines and conversations unlock and play?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-194) · **Blocks:** — (B-194 shipped with A)
**Question:** HubAndStory §4: recovered memories show up in a memory journal (memories, lore, logged dialogue), V's lines between runs and character conversations, all unlocked from the same memory data. How are they authored, when do they unlock, and how are they played?
**Options:**
- **A** Story assets (any number, gathered by the asset registry) list V's lines (in order) and conversations, each naming the memory ids it needs (none: from the start). A line or conversation unlocks once all its memories are in the journal. Talking to V plays the next unlocked line not heard yet, one at a time; a conversation plays line by line and is logged in the journal once played to its end, and can be replayed. Lore objects read on a trial map are kept in the journal at once (whatever happens to the run). The journal lists every memory, lore entry and played conversation to reread. One placeholder hub station opens all three pages. Speakers are written into the line text for now and not checked against the party.
- **B** Data tables (one row struct per kind) instead of data assets.
- **C** V says every unlocked line at once (a list) rather than one at a time.
**Recommendation:** A. Expedition 33's camp conversations and Hades' between-run lines unlock from story progress and play one at a time (players praise how each return home has something new); Hades' Codex and Hollow Knight's Hunter's Journal keep everything to reread. One asset type gathered like reaction triggers (Q-191) keeps one authoring path (data tables need a row struct and an asset per table, for no gain yet). Not lore (the lines themselves are Crown's to write).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it. **Pinned by Crown (2026-10-27): how often V says something new** (one per hub return, Hades, vs one per talk, as built) to revisit; as built until then.
**Notes for Claude:** applied (feature/B-194-memory-journal): `UStoryLinesData`, `StoryUnlocks`, `UMemoryJournalSubsystem` (lore, heard lines, played conversations; saved with B-090), `UMemoryJournalWidget` (placeholder, D-024), `AHubJournalStation`. Known gaps: not saved (B-090); a story phase unlocking the next (HubAndStory §6) is not modelled; speakers aren't checked against the party; no "new" marker on the station.

### Q-193 — How are boss phases reached, and what do they change?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-193) · **Blocks:** — (B-193 shipped with A)
**Question:** D-098: a boss has phases, each a condition list (an HP point, or hidden ones) and optional changes; a reached phase stays; the weather shifts toward the boss with intensity rising per phase; mini-bosses use a style preset; each status break raises resistance. Exactly which conditions, how the changes apply, how much resistance per break, and what the presets are?
**Options:**
- **A** A phase is reached in order once all its conditions hold, checked whenever the boss's HP changes, a turn starts or its status bar breaks; one hit can pass several phases. Conditions: HP at or below a percent, the boss's own turns taken, Broken Darkness absorption maxed, its weapon's crystal broken, status breaks (infusion hits later). Changes: effects put on the boss for the fight (counted as gear, no cleanse), AI one step harder per aggression step (Expert at most), the weather pulled toward the boss's side at least to the phase's intensity (the boss's own sky wins); dialogue, element and added skills logged until those exist. Each status break: +10% status resistance on every bar for the fight (`// TODO: tune`). Presets: Planning = HP 60% then 2 status breaks; Timing = HP 50% then 6 of its own turns; Both = HP 50% then 25%. Memory skip = the next phase not reached is removed; memory add = a last stand at 15% HP with full weather.
- **B** Phases by HP only (simpler, no hidden conditions).
- **C** Resistance per break on that bar only (the per-status kind).
**Recommendation:** A. Elden Ring's and Monster Hunter's bosses change at HP thresholds and stay changed, and players like phase two as the real test; Monster Hunter and Elden Ring raise status resistance after each proc so statuses stay useful without trivialising a boss (widely understood as fair). Final Fantasy XIV / Expedition 33 show the change through the arena and music instead of a cutscene, which matches the weather shift. Hidden conditions (Sekiro's posture breaks, Monster Hunter's part breaks changing behaviour) reward planning. One blanket step is simpler than per-bar tracking for now. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-193-boss-phases): `UBossData`, `UBossPhaseComponent`, hooks in `ABattleGameMode`, `ReactionTriggers::ApplyMemoryBossModifiers`, `UAIDecisionManager::GetDifficultyFor`, `UWeatherStateManager::RecalculateWeather`, `UStatusBuildupManager`. Known gaps: dialogue (B-194), element change and added skills logged only; infusion hits never met; phase effects only on the boss; boss revival still open (D-098). `FMemoryBossModifier::Boss` stays a `UCharacterData` (boss data is looked up from it).

### Q-192 — What form does a bound bonus take?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-192) · **Blocks:** — (B-192 shipped with A)
**Question:** D-033 (A): anyone can equip a bound item; only its lord gets the bound bonus, hidden unless the lord wears it. What is the bonus, and what does another wearer see?
**Options:**
- **A** The item names its owner (a character) and a list of effect bundles, authored like the item's own effects; they join the item's effects only when the owner wears it, so they work exactly like gear effects (start of combat, or armed with their trigger). Anyone else sees no line at all for it, not even that it is bound. Gear only for now (bound skills later).
- **B** A stat bonus (numbers) instead of effects.
- **C** Show "Bound to <lord>" to everyone, with the bonus hidden.
**Recommendation:** A. The deckbuilder's Bonded effects (`WoR_TCG_Pivot_Design.md`) are effects that only their Character gets; Fire Emblem's personal weapons and Xenoblade's character-specific gear give their owner an extra effect, and players like discovering them. Reusing effect bundles keeps one system. Hiding the whole line follows RunStructure §5's hidden rule. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it. **Confirmed by Crown (2026-10-27): fully hidden**, no bound mark for anyone else.
**Notes for Claude:** applied (feature/B-192-bound-gear): `UEquipmentDataBase::BoundOwner` / `BoundEffects` / `IsBoundBonusActive` / `GetBoundBonusText`; the loadout's gathered combat effects pass the wearer. Known gaps: no screen shows the bonus line yet (no description generator, D-031 / B-045; the text is the debug line, a D-061 gap until then); the flat `GetActiveEffects` used by the gear-effect debug dump's PRE list still leaves bound effects out (they appear in POST); evolutions can't be bound; ownership is the character asset, so a trial mirror enemy with its own asset won't match (decide with trial loot). B-081 must pass the wearer when a swap re-gathers effects.

### Q-191 — How do reaction triggers fire, and what do memories do to a boss now?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-191) · **Blocks:** — (B-191 shipped with A)
**Question:** D-032 / RunStructure §5: if these items (all equipped) are worn while facing this character, trigger effects, a quest or dialogue. Worn by whom, when is it checked, where do the effects go, and how do memory boss changes apply before boss phases exist (B-193)?
**Options:**
- **A** All the items worn by **one** party member (weapons and rings in use: primary and secondary weapon, primary and active ring); the character is anyone in the opposing team. Checked once, at encounter start; each reaction fires at most once per fight. Effects are ordinary skill effects: a payload aimed at Enemy lands on the faced character, any other on the wearer; they count as gear (no cleanse strips them, D-015). Quest and dialogue ids are logged until those systems exist. Effects with a target count (an ally, all enemies) still go to the wearer or the one faced character for now. A party member's max HP changes but current HP doesn't (the existing over-cap rule). Run memories naming a boss add up, weaken -1 and strengthen +1 step (15% max HP and damage per step, at most 3 steps, `// TODO: tune`), into one net change, and the boss starts at its new full HP; skip / add a phase waits for B-193. Gear the battle's loadout check later removes as invalid can still have fired a reaction (rare; checked before `StartCombat`).
- **B** The items may be spread across the party.
- **C** Checked every turn, so a mid-fight swap can start or stop a reaction.
**Recommendation:** A. Elden Ring and Dark Souls tie special boss dialogue and outcomes to what one character carries or wears (e.g. Ranni's ring, the Covenant items), and players like finding those hidden links; Persona and Fire Emblem's special battle conversations fire once at the start of a fight. One wearer keeps bound-gear stories (D-033) about one lord; a start-of-fight check keeps it predictable and cheap. Not lore (which items and characters to link is Crown's).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it. **Changed by Crown (2026-10-27): reactions can also fire mid-fight**, at any trigger the effect system has (a turn start, a hit, an HP threshold for either side, a boss phase change), still **at most once per fight** each. Boss triggers (D-100) use the same timing.
**Notes for Claude:** applied (feature/B-191-reaction-triggers): `FReactionTrigger`, `UReactionTriggerData`, `ReactionTriggers::FindFired` / `ApplyFired` / `ApplyMemoryBossModifiers`, called by `ABattleGameMode` before `StartCombat`. B-193 hooks phase changes into the same place.

### Q-190 — What does a memory fragment hold, and when is it kept?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-190) · **Blocks:** — (B-190 shipped with A)
**Question:** RunStructure §5: a fragment unlocks a memory, can weaken or strengthen a boss, means more to one lord than another, and gives no hint. What does the data hold, and is a memory kept when the run fails?
**Options:**
- **A** A memory has an id, a placeholder title and text, a depth per character (none, minor, deep) and a list of boss changes (weaken, strengthen, skip a phase, add a phase; used by B-191 / B-193). Picking up a fragment adds the memory to the run and, at once, to the journal, which keeps it whatever happens to the run. The pickup only says a fragment was found; the fragment is gone for the rest of the run, and is back in the next run (picking it up again adds it to that run; the journal already has it).
- **B** The journal only gets the memories of a run that is won.
- **C** Numeric depth (0-10) instead of three levels.
**Recommendation:** A. Hades keeps every story unlock when a run fails, and players praise that a failed run still moves the story on; Hollow Knight's and Elden Ring's lore items are kept the moment you pick them up and explain themselves only when read, matching the no-hint rule. Three depth levels are easy to author and read. Not lore (the memories themselves are Crown's to write).
**Answer (Crown):** A, approved by the reference-backed default (2026-10-26), **except, changed by Crown (2026-10-27): a collected fragment is gone for good**: once a memory is in the journal, its fragment no longer appears in later runs (D-100).
**Notes for Claude:** applied (feature/B-190-memory-fragments): `UMemoryData`, `AMemoryFragment` (a trial map feature, B-188), `UTrialRunSubsystem::CollectMemory` (run state, D-065), `UMemoryJournalSubsystem` (lasts while the game runs; saved with B-090). `FMemoryBossModifier::Boss` is a `UCharacterData` for now; B-193 may switch it to `UBossData`. Crown's change applied by B-210 (feature/B-210-collected-memories): `MemoryUnlock::Unlock` refuses a memory the journal has, `MemoryUnlock::IsCollected`; `AMemoryFragment::IsTaken` hides the fragment on load; `AMemorySpot` is spent. A collected memory is no longer in a later run's memories, so B-191's memory boss change applies only in the run it was found; D-100 (restructured) moves boss changes to the boss's own triggers (B-211).

### Q-188 — How do the trial map's chest, lore object and boss arena behave?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-188) · **Blocks:** — (B-188 shipped with A)
**Question:** RunStructure §4 lists a chest (rolls the zone pool), a lore object (a journal entry) and a boss arena (the boss fight, when the player chooses). What does each do exactly, with no journal screen and no loot weights yet?
**Options:**
- **A** *Chest:* opens once per run and gives one item from its pool: its own pool when set, else its zone's (`Resources_Design.md` §7.9: the tag picks the candidates, a roll seeded by the run seed and the chest picks one; unweighted for now, `// TODO`). An empty pool opens empty; with no room the chest stays closed so you can come back. *Lore:* shows its text; the first read goes into the run's found list (the placeholder journal; `wor.Trial.Print`). *Boss arena:* pressing interact at it is the choice; it fights the chosen variation's boss (else an authored fallback) as a Boss-rank fight, AI Hard by default, the arena's zone setting the strength; beaten for the rest of the run once won.
- **B** Chests come back after a rest, or show a confirm screen before the boss.
- **C** Weighted rolls and a journal screen now.
**Recommendation:** A. Hades and Dead Cells chests and rooms give their reward once per run and players like that a cleared room stays cleared; Elden Ring and Dark Souls put the boss behind a fog wall you choose to walk through, praised for making the fight the player's choice, and their item descriptions and notes are the lore you collect as you go. One press keeps the placeholder simple; the real screens come with D-024. Not lore (the lore text itself is placeholder; Crown writes it).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-188-map-features): `ATrialMapFeature` with `ATrialChest`, `ATrialLoreObject`, `ABossArenaTrigger`; `TrialLoot` (pool gather, roll, grant; `wor.Trial.Pool`); `UTrialRunSubsystem::RecordFound` (run state like `BeatenEncounters`, moves for co-op with it, D-065). Vendor reuses `AMerchantInteractable` (B-179 trial stock); recruiting waits on B-076.

### Q-186 — How do zones set enemy strength?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-186) · **Blocks:** — (B-186 shipped with A)
**Question:** Enemies §4 says the zone on the map sets enemy strength and the loot pool, nearer the boss is tougher, and nothing scales to the party. How is strength expressed, and how does it combine with the door's difficulty?
**Options:**
- **A** A zone has a tier, 0 to 4. Each tier has one named multiplier on enemy max HP and damage (1.0, 1.15, 1.3, 1.5, 1.75; `// TODO: tune`), multiplied with the door's difficulty. The fight takes the zone where it starts (the arena centre); where zones overlap, the highest tier. Outside every zone is tier 0. The loot pool is one source tag (`Resources_Design.md` §7.9; a pool is a tag query, so one tag per zone is enough to start) carried with the fight for chests and drops.
- **B** Each zone authors its own multiplier.
- **C** A zone swaps in a stronger version of each enemy.
**Recommendation:** A. Dark Souls and Elden Ring set enemy strength by area and never scale to the player, and players praise how getting stronger shows and how you can wander into a hard area early (Enemies §4 asks for exactly that). Diablo II's fixed area levels work the same way. A few named tiers keep zones comparable and easy to tune. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-186-enemy-zones): `AEnemyZone` (`FEncounterZoneInfo`), `AEnemyZone::FindZoneAt` from `UEncounterComponent`, `UTrialRunSubsystem::PendingZone` (the same transport exception to D-064 as `PendingRank` and `PendingFirstStrike`, moves with them for co-op, D-065), `ABattleGameMode` multiplies it in. The loot pool is not read yet (B-188 chests, B-073 drops).

### Q-185 — Placeholder first strike: what counts as a hit, and how long is the head start?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-185) · **Blocks:** — (B-185 shipped with A)
**Question:** First strike needs a field ability (GAS, not built). Until then, what decides it, and does "that side acts first" mean a free extra round or only going first?
**Options:**
- **A** Contact decides it. Touch a team from outside its notice cone (60° each side of where it faces or walks) and the party strikes first. A team that has seen you and reaches you while your back is turned (it is in your rear half) strikes first. Otherwise nobody does. Contact stands in for the field hit, so any touch from outside the cone counts. The side with first strike takes its whole first round (each member's normal turns; a fast member may take two or three) before the other side moves. Nobody gets extra turns. During that opening the waiting side's pinned turns wait too, and a Wind delay doesn't hand turns between openers. A walking team faces the way it walks; standing still (or at a patrol turnaround) it faces its placed direction.
- **B** A free extra turn for every member of that side, on top of the normal order.
- **C** Wait for the field ability.
**Recommendation:** A. Persona 5 and Dragon Quest XI judge the ambush by approaching an enemy from behind and being caught from behind, and players like it because it rewards sneaking up. Going first without extra turns keeps the speed schedule fair; Octopath Traveler's preemptive strike also only reorders the first round. It is a placeholder until the field ability (`GASMigration.md`). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-185-first-strike): `UEncounterComponent::JudgeFirstStrike` (`NoticeHalfAngleDegrees`), `UTurnManager::InitializeCombat(..., OpeningTeam)` with `bWaitsForOpening` (shown in `DebugPrintTurnOrder` as `[WAITS: first strike]`), `ACombatOrchestrator::SetFirstStrike`. `UTrialRunSubsystem::PendingFirstStrike` is the same transport exception to D-064 as `PendingRank`.

### Q-184 — Which teams count as "caught at once", and what happens to a team you don't beat?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-184) · **Blocks:** — (B-184 shipped with A)
**Question:** Enemies §3 says several teams caught at once send their strongest member each, max 3. Which teams are caught, what rank and AI level does the mixed fight have, and does a team you lost to or fled from stay on the map?
**Options:**
- **A** Caught = every armed team within the arena's full radius (`ArenaSphereMaxRadius`) of its centre when the join window closes; the touched team comes first and past three the rest wait. While one window is open no other team opens its own. Nearby single enemy pawns are caught the same way. The mixed fight takes the hardest rank (a boss among them can't be fled) and the hardest AI level. Only a win removes them, all of them; after a loss or a flee they are still there.
- **B** Only teams touching the player when the window closes.
- **C** A flee also removes the team.
**Recommendation:** A. Persona 5 and Dragon Quest XI show enemies on the map; escaping or losing leaves the enemy in the level, and only a win clears it. That is widely liked because the map shows your progress. Pulling in the teams near the fight matches Chained Echoes and Octopath, where nearby enemies join the battle. The arena radius is the area already shown to the player. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-184-field-enemy-teams): `UEncounterComponent::GatherCaughtEncounters`, `UEncounterData::BuildCaughtRoster`, `UTrialRunSubsystem::MarkPendingEncounterBeaten` (Victory only). Every placed encounter stays beaten, not only `AFieldEnemyTeam`; the key is the owner's level path without the PIE prefix. `PendingEncounterSources` carries the fight's sources across the OpenLevel like `PendingRank` (the same transport exception to D-064, moves with it for co-op, D-065).

### Q-183 — How many trial difficulty levels, and what does each change?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-183) · **Blocks:** — (B-183 shipped with A)
**Question:** RunStructure §3 says the door asks for a difficulty that scales enemy strength and the payout. How many levels, and what does "enemy strength" scale?
**Options:**
- **A** Three levels, Easy / Medium (default) / Hard, as in the deckbuilder. Strength scales each enemy's max HP and damage (Easy 0.8, Medium 1, Hard 1.25); payout scales by 0.75 / 1 / 1.5. Defense, speed and status are not scaled; each enemy starts the fight full at its scaled max, and the scaling can't be cleansed (it is part of the enemy, like gear, D-015). All values `// TODO: tune`. This run difficulty is separate from each encounter's AI difficulty (ER-44), which still sets how well the enemy plays.
- **B** Four levels, matching the AI's Easy / Medium / Hard / Expert, and the run difficulty also raises the AI level.
- **C** Strength scales HP only.
**Recommendation:** A. The deckbuilder has the same three levels (`Difficulty`: Easy = 0, Medium = 1, Hard = 2, `docs/deferrals.md`; its effect is deferred there, so the numbers are ours). Diablo III and IV scale monster HP and damage per difficulty tier and pay more for the harder tiers, and that is widely liked because the player chooses risk against reward. Keeping the AI level separate keeps each encounter's authored behaviour. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-183-gate-choice): `Trial/TrialGate.h` (`ETrialDifficulty`, the multipliers, `ApplyEnemyStrength`), `ABattleGameMode` scales each enemy before `StartCombat`. The payout multiplier is not read yet: no run-end payout exists (B-073 / B-096 read `UTrialRunSubsystem::GetPayoutMultiplier`).

### Q-115 — Which Broken Darkness state is "absorption" that carries between fights?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-115) · **Blocks:** — (B-115 shipped with A)
**Question:** D-013 says absorption carries over within a run. Broken Darkness keeps several kinds of state: the absorbed EP (above max = overload), the absorption stacks and absorbed elements, the strain toward a transformation, and the transformation itself for a Darkness caster who isn't born Broken Darkness. Which carry?
**Options:**
- **A** The absorption pool carries (absorbed elements, alignment, stacks and the consecutive-absorption streak, the EP above max, which re-enters overload at once); strain and a non-born caster's transformation start fresh each fight.
- **B** Everything carries, the transformation included.
- **C** Only the EP (already carried), nothing else.
**Recommendation:** A. It keeps what the player built up by guarding (the attrition D-013 asks for), while fight-tempo states reset: Darkest Dungeon carries HP and stress between battles but each battle's buffs end with it, and Slay the Spire ends every in-fight form at the fight's end. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-115-carry-gear-state): `FCarriedAbsorptionState`, `UBrokenDarknessManager::SnapshotCarriedAbsorption` / `ApplyCarriedAbsorption` (born Broken Darkness only).

### Q-115b — After a flee or a draw, does crystal wear carry?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-115) · **Blocks:** — (B-115 shipped with A)
**Question:** a won or lost fight runs the between-fight destruction and the +10 repair before its state is banked. A fled or drawn fight runs neither. Should its wear still carry?
**Options:**
- **A** Yes, as the fight left it: no +10 repair, and a gem broken at 0 stays broken (it is destroyed at the end of the next fight it reaches).
- **B** No: a flee or draw drops the fight's crystal state (the next fight starts from the last banked state).
**Recommendation:** A. HP and EP already carry after a flee, so wear does too; in Dark Souls running from a fight never restores durability, and Darkest Dungeon's retreat keeps the damage taken. The repair is the reward for finishing a fight. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied: `ABattleGameMode::HandleCombatEnd` banks on every result.

### Q-074 — Does an encounter's enemy team replace its map pawn or join it?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-074) · **Blocks:** — (B-074 shipped with A)
**Question:** an encounter on the map sits on one enemy pawn. When it names an enemy team (`UEncounterData`, up to three), does the team fight instead of that pawn, or alongside it?
**Options:**
- **A** The team replaces the pawn: the pawn is the team's marker on the map. No team (or an empty one) = the pawn fights alone.
- **B** The pawn always fights, and the team adds up to two more.
**Recommendation:** A. Symbol encounters work this way in Dragon Quest XI and Persona 5 (one shadow on the map opens a fight with a whole group), and players read the map symbol as "a fight", not as one of the enemies. It keeps the team's full make-up in one asset. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-074-encounter-data): `UEncounterData::BuildOpposingRoster`. B-184's `AFieldEnemyTeam` should hold a `UEncounterData` and call `BuildOpposingRoster` rather than carry a second list of enemies.

### Q-119 — Data validation in test.sh: what to check, and what to do with today's errors
**Status:** DECIDED · **Raised:** 2026-10-27 (B-119) · **Blocks:** — (B-119 built with A)
**Question:** The job says "fail on errors", but current content already has 13 validation errors (missing damage types, test inventories over the loadout caps, test characters without Cosmetics), and validating all of `/Game` takes over 200 s and 8 GB because of the vendor animation packs. `test.sh` runs from the Stop hook after every C++ change.
**Options:**
- **A** validate data assets only (`PrimaryDataAsset` subclasses, ~12 s); today's errors go in a committed baseline file that is listed but doesn't fail; any new error fails; fixing the 13 is its own editor job (B-206).
- **B** validate everything and fail on any error: every run fails until the content is fixed, and each run costs minutes.
- **C** validate but never fail: errors are only printed.
**Recommendation:** A. A committed "known issues" baseline that only lets new problems fail is how widely used checkers bring in a strict rule on an existing codebase (detekt and ktlint baselines, PHPStan's baseline, RuboCop's `.rubocop_todo.yml`); it keeps the check strict for new work without blocking everything on old debt. Data assets are what this game's `IsDataValid` overrides cover; the vendor packs have none. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** `Scripts/ue/test.sh` (end), `Scripts/ue/validation_baseline.txt`. When an asset is fixed, delete its baseline line in the same change.

### Q-111 — Combat HUD v2: damage colours, crack timing, overload look
**Status:** DECIDED · **Raised:** 2026-10-27 (B-111) · **Blocks:** — (B-111 drew A)
**Question:** The v2 drafts needed three looks Crown's notes didn't fix: the colours of the resistance gradient on damage numbers, when a status bar starts to crack, and how Broken Darkness overload darkens the EP bar.
**Options:**
- **A** damage numbers run warm gold (weak) → near-white (neutral) → cool steel-blue (resisted) → grey (immune); status bars stay clean below half, then crack more and more until they shatter on the break and lie in dimmed shards while it lasts; overload darkens the full EP bar with denser dark facets up to the 1.30 cap, the absorbed element still showing through.
- **B** a red-to-blue gradient; cracks from the first point of buildup; overload as a separate band past the bar's end.
**Recommendation:** A. Warm = good for you, cool/grey = wasted is how Honkai: Star Rail and Persona 5 read their weakness hits at a glance (both praised for readable feedback). A bar that visibly breaks is Octopath Traveler's shield break and Star Rail's toughness bar shattering, both widely liked. Keeping the overload on the bar follows Crown's note (one bar, EP above max). Not lore.
**Answer (Crown):** **Damage numbers changed by Crown** (2026-10-27): a number's **colour is the hit's element** (the element colour table; Physical = plain white), and **how well it landed is shown by size and brightness**: weak = bigger, brighter, a glow; neutral = normal; resisted = smaller, dimmer; immune = grey and small (Genshin Impact's element-coloured numbers). Still no Weak/Resist labels (rules 02 note). Crack timing and the overload look stay A (approved by the reference-backed default); Crown may change them in the v2 review (B-102).
**Notes for Claude:** applied in `Tools/ui_drafts/combat_hud_v2.py` (`DAMAGE_GRADIENT`, `crystal_bar`, `shards`).

### Q-103 — CommonUI base: small defaults
**Status:** DECIDED · **Raised:** 2026-10-27 (B-103) · **Blocks:** — (B-103 shipped with A)
**Question:** Three small defaults in the CommonUI base: where a modal goes while the root layout WBP has no modal layer, a new button's job, and the test screen's layer.
**Options:**
- **A** a modal goes on the menu layer (with a warning) until the WBP gains a ModalLayer; a new button is a Confirm button; the test screen opens on the menu layer.
- **B** a modal push fails until the WBP has a ModalLayer; a new button has no job until one is picked.
**Recommendation:** A. Lyra's CommonUI sample (Epic's reference project) pushes menus and dialogs to named layers and falls back gracefully; Confirm is the commonest button, so it is the least-surprising default. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-103-commonui-base): `UWoRRootLayout::GetLayerStack`, `UWoRButtonBase::Job`.

### Q-084 — Dismantling an equipped ring: what happens to the slot, and which loadouts does the warning check?
**Status:** DECIDED · **Raised:** 2026-10-27 (B-084) · **Blocks:** — (B-084 shipped with A)
**Question:** D-016 says an equipped item is taken out of its loadouts and dismantled. For a Resonator's ring slots, is the slot removed or left empty, and which ring is active after? Does the "can't fight" line look at every loadout or only the one you fight with?
**Options:**
- **A** the ring slot is removed (the rest close up); the ring that was active stays active, or the next one if it was the active one. The "can't fight" line checks the active loadout, the one a fight starts from. Not allowed during a fight.
- **B** the slot stays, empty; the active index is unchanged (it may point at an empty slot). The warning checks every loadout.
**Recommendation:** A. Removing the gone item and keeping the player's current pick is what inventory games do: Diablo and Path of Exile salvage an item straight out of its slot with no empty placeholder, and Final Fantasy XIV's gear sets simply drop a desynthesized item. Warning about the loadout you fight with keeps the message short. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-084-dismantle-equipped): `UInventoryComponent::StripFromLoadout`, `UEconomyService::GetDismantleWarning`.

### Q-082 — Does a ring fusion's bonus stat count too?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-082) · **Blocks:** — (B-082 shipped with A)
**Question:** D-009 says the stat-stone half of an elemental fusion counts on a ring. A fusion also has a bonus stat of its own (the average of its halves' tiers). Does that count on a ring, and which rings count?
**Options:**
- **A** the fusion counts on a ring exactly as on a weapon (stone half plus bonus stat); the counting ring is the Resonator's active ring, else a primary ring (the rings gear effects read); it adds to the weapon's stone.
- **B** only the stone half's percent; the bonus stat is weapon-only.
**Recommendation:** A. One item behaving the same wherever it is slotted is what players expect: Final Fantasy IX's and Diablo's socketed gems give the same bonus in any socketed item. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-082-ring-fusion-stone): `ULoadoutComponent::GetAttachedStonePercent`.

### Q-081 — On a mid-fight swap, which of the new gear's effects come on?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-081) · **Blocks:** — (B-081 shipped with A)
**Question:** D-008 says gear effects follow a swap (old off, new on). A starting effect can be a lasting passive, or a one-shot, timed or charged effect given at the start of the fight. With a free switch every turn, re-giving those on each swap back would farm them. Which come on?
**Options:**
- **A** only passives (every payload permanent) come on with a swap; one-shot, timed and charged starting effects are given once a fight. The old gear's effects always go off.
- **B** everything comes on fresh on every swap.
- **C** nothing new comes on mid-fight (effects locked at the start; against D-008).
**Recommendation:** A. Final Fantasy X lets you swap weapons and armor mid-battle: their passive auto-abilities follow at once, but start-of-battle ones (First Strike, Initiative) only count at the start; players liked that swapping is tactical, not a loop to farm. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-081-gear-effects-follow-swap): `USkillEffectManager::SwapEquipmentEffects`.

### Q-198 — Which way does a Luck dodge step?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-198) · **Blocks:** — (B-198 shipped with A)
**Question:** a Luck dodge (D-085) now plays the dodge montage, but no press chose a side. Left or right?
**Options:**
- **A** alternate by impact (right, then left), no random draw, so seeded fights replay the same; a mistimed Dodge press keeps its own side (no replay).
- **B** a random side from the battle random stream.
- **C** always the same side.
**Recommendation:** A. Expedition 33 and Sekiro play fixed, readable evade animations rather than random ones, and alternating sides makes a multi-hit flurry look like weaving. A cosmetic choice should not take a draw from the battle stream, which would change every later roll of a seeded fight (B-010). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (fix/B-198-luck-dodge-animation): `LuckDodge::Direction`, `UActionExecutor::ResolveImpactDefense`.

### Q-204 — A spell held by both a broken and an intact source: greyed or not?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-204) · **Blocks:** — (B-204 shipped with A)
**Question:** D-083 greys out a broken crystal's spells. When the same spell is also on another source (the other weapon, or a ring), is it still castable?
**Options:**
- **A** the source the cast would use decides, in the cast's own order (innate, then an intact ring, then the primary weapon, then the secondary): castable when that source is intact, greyed when it is broken.
- **B** castable whenever any intact source holds it.
- **C** greyed whenever any broken source holds it.
**Recommendation:** A. It keeps the menu, the AI and the cast agreeing on one source, so a spell is never shown castable and then channelled through a broken crystal. Elden Ring does the same with two weapons: the skill always comes from one fixed hand (the right), not whichever is fit; players read that rule easily. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (fix/B-204-broken-secondary-weapon): `ULoadoutComponent::GetBrokenSourceReason`, the order of `ResolveSpellSource` / `FindSpellCatalystHolder`.

### Q-199 — What can a Void-broken turn do?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-199) · **Blocks:** — (B-199 shipped with A)
**Question:** Q-063b says the Void break picks the holder's action at random from its usable actions. Which actions are in the draw, and who can it hit?
**Options:**
- **A** Defend, the basic attack, any usable spell or ability (each one the rules accept), aimed at a random target on the side it is made for (an attack at an enemy, a heal or buff at the holder or an ally). No items (a random throw would waste charges), no Flee, no weapon switch.
- **B** everything on the menu, items and Flee included, at any target, allies too.
- **C** the basic attack only, at a random target (Final Fantasy's classic Confuse).
**Recommendation:** A. Final Fantasy's Confuse and Persona's Confusion take the turn away but keep it a combat action, never Flee or an item spent on nothing, and players accept a lost turn but resent losing resources to it. A random pick over the holder's own kit makes the break about this character, as Q-063b asks. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-199-void-random-action): `UAIDecisionManager::BuildRandomAction`; aimed by the skill's `TargetType` (Self: the holder; Ally: the holder or a living ally, at random; Enemy and Anyone: a random living enemy). Switching who plays the member is refused while the break lasts. A break that lands during the holder's own turn is used up on that turn (the shared holder-turn rule, Q-063); one that lands off-turn takes the next one.

### Q-151 — Where does the player's one wallet live, and who spends from it?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-151) · **Blocks:** — (B-151 shipped with A)
**Question:** D-046 moves the wallet to player scope. An AI-played ally has no PlayerState, and a level change destroys the PlayerState. Which wallet does each actor use, and is the save/load part of B-151's test buildable now?
**Options:**
- **A** the wallet is a component on the player's PlayerState (`AWoRPlayerState`). The player's pawn uses it; any member of the player's party uses the party leader's; an actor with no player behind it (an enemy) keeps its own. Carrying it across level changes and to disk waits for the Pool persistence arc and the save structs (D-059), as the per-character wallet already wiped on every level change.
- **B** the wallet lives on a GameInstance subsystem now (survives level changes, but D-064 keeps new state off subsystems and co-op needs it per player).
**Recommendation:** A. Epic's own samples keep a player's persistent, replicated data on the PlayerState (Lyra's player-scoped state; ShooterGame's score), and an RPG party spending from one purse is the norm (Final Fantasy's and Dragon Quest's shared Gil and Gold). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-151-player-wallet): `CurrencyWallet::FindFor`, in this order: the pawn's PlayerState wallet; for a local party member, the party leader's (falling back to the world's local player when the leader pointer is stale after OpenLevel); else the actor's own component. An AI ally's spend and earnings are the player's. Flee now takes its Gold share once, from the one wallet (a party of three used to lose three shares of three purses; the total share is unchanged, 10% of what the player holds). The save/load check of B-151 is left for the save job (D-059); spec covers one shared balance. A Blueprint game mode that does not derive from `ATrialGameMode`/`ABattleGameMode` must set Player State Class to `WoRPlayerState`, or its party falls back to each character's own wallet.

### Q-149 — Which enemies start in the front row and which in the back, until enemies have authored cells?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-149) · **Blocks:** — (B-149 shipped with A)
**Question:** D-042 says enemies use "tanks front, casters back" until their cells are authored. The game has no tank tag, only the three classes (Generic, Refractor, Resonator).
**Options:**
- **A** by class: a Refractor (Caster) starts in the Back row; a Generic or Resonator (weapon users) in the Front row. An enemy with no character data starts Front.
- **B** every enemy in the Middle row until authored (no row bonus).
- **C** by stats: the highest-HP enemy Front, the rest Back.
**Recommendation:** A. The classic row games put spellcasters in the back row and weapon fighters in the front (Final Fantasy IV to VI; Octopath Traveler's tank/mage roles), and players read it at a glance. A Resonator fights with a weapon, so it stands with the fighters. Not lore. Note: `UCharacterData` defaults to Caster, so an enemy asset whose class was never set starts in the Back row.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-26; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-149-formation-cells): `ACombatOrchestrator::GetPlaceholderEnemyStartRow`. Authored enemy cells replace it later.

### Q-091 — The shared pool: what is shared, who wears what, and what happens on a clash
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-091, D-020 = A) · **Blocks:** — (ships with A)
**Question:** D-020 gives the party one pool, and an item can sit in several characters' presets but is worn by one at a time. Building it raises four details: which items are shared, what "wearing" means, who keeps a piece when two members want it, and when the clash is settled.
**Options:**
- **A** **Gear is shared** (weapons, rings, evolutions): each character's starting gear is banked into the pool once, and every party member's inventory shows the whole armoury. **Wearing = in the active loadout**: other presets may name the piece. On a clash, **the member already wearing it keeps it**; the other's active loadout loses it with a "Removed: Fire Sword (worn by Ayla)" message, and the party panel lists the clash. The check runs when a member spawns, switches loadout, and before each fight. Skills (spells, abilities) stay with the character who learned them, and crystals stay per character until B-203 builds the D-097 shared item inventory.
- **B** As A, but the party panel asks you to pick the wearer on every clash (a choice screen before each fight; more UI, the same outcome when you pick the current wearer).
- **C** Share skills and crystals too (every character could use every learned spell, bypassing class gating; crystals would need the D-097 inventory first).
**Recommendation:** A. Final Fantasy IX and X share one bag for the party: a weapon on one character is shown as theirs and can't be put on another until it's taken off. Fire Emblem's convoy works the same way. Players read it as fair because nothing is ever duplicated or silently swapped. Skills that stay with their learner match Final Fantasy X and Dragon Quest, where each character keeps their own spells.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it, e.g. to B's pick-the-wearer screen. Also decided with it: the inventory caps (20 weapons, 20 rings, 5 evolutions) count the party's whole armoury until D-097's storage (B-203) replaces them; a preset piece a character's authored inventory doesn't list joins its starting gear instead of being dropped.
**Notes for Claude:** applied (feature/B-091-shared-gear-pool): `UPoolSubsystem` wearers, banked characters and `OnPoolGearChanged`; `UInventoryComponent::DrawSharedGear` / `ClaimActiveSharedGear` / `PushGearToPool`; `ULoadoutComponent::ClaimSharedGear`; save `FSavedPool::Wearers` / `BankedCharacters`; party panel clash line. Limits: the per-inventory caps (20 weapons, 20 rings, 5 evolutions) now count the party's whole armoury; a saved weapon or ring still restores with its default crystal (B-124); the bought-loadout run start is B-116.

### Q-076 — A trial recruit: what gear it can use, and when it applies
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-076, D-026 = B) · **Blocks:** — (ships with A)
**Question:** D-026 makes a character found in a trial run-only and ungeared, using the party's run inventory. Building it raises four details: what of its own it keeps, which gear it can be given, when a choice applies, and whether a quit mid-run keeps it.
**Options:**
- **A** It brings **no gear** into the shared armoury (its authored weapons, rings and evolutions stay out). Its **skills stay its own** (its authored preset keeps its innate spells and item slots), as for every member (Q-091); its authored crystals and evolutions don't come. It joins at **full HP/EP**, like the rest of the party at trial entry. The party panel gives it a **weapon and a ring** from the armoury (each click steps to the next piece, then none); the choice applies **from the next fight**. A Resonator wears the ring in its ring loadout; a Generic or Caster wears it as its primary only when given no weapon (the weapon takes the primary slot). A piece another member wears is offered too, and B-091's one-wearer rule keeps it with the wearer (whoever spawns first wears it). A recruit dismissed mid-run can be recruited again. A run saved mid-trial **keeps the recruit** and its chosen gear; it still leaves when the run ends.
- **B** As A, plus evolutions and a full loadout editor for the recruit (needs the real loadout screen, D-024).
- **C** The recruit keeps its authored gear for the run (contradicts D-026's "ungeared").
**Recommendation:** A. XCOM 2's armoury: a new soldier arrives with basics and you kit them from the shared stock, each item on one soldier at a time; Darkest Dungeon's trinkets work the same from a shared stash. Players liked that a new member costs the team gear, which makes the choice matter. Keeping a mid-run quit's recruit matches Slay the Spire's save-and-quit keeping the whole run (Q-208). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it, e.g. to B once the loadout screen exists.
**Notes for Claude:** applied (feature/B-076-trial-recruits). Trial maps have no recruit spots yet (placing a recruitable pawn in a trial level is editor work): B-215. Until then, `wor.Party.RecruitForRun <CharacterData path>` recruits in a trial. Evolutions for a recruit wait for B with the real screen.

### Q-064 — Element resistance and damage that isn't a hit: DoT ticks, Reality Burst, reflect
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-064) · **Blocks:** — (B-064 ships hits only; B-064b builds A)
**Question:** Since B-064, element resistance scales every hit's elemental damage (D-036). Three kinds of damage don't go through the damage maths: status DoT ticks (Burn, Chill… `SkillEffectManager.cpp`, `ESkillEffectType::DOT`), Reality Burst (25% of max HP) and reflected damage. Should element resistance cut them?
**Options:**
- **A** Elemental DoT ticks are resisted like hits (the effect's element, the same 1:1, +-50% cap); Reality Burst (a share of max HP) and reflect stay unresisted.
- **B** Only hits; nothing else is resisted.
- **C** Everything with an element, Reality Burst and reflect included.
**Recommendation:** A. In Path of Exile, ignite (burning) damage is reduced by fire resistance, and in Diablo 4 burning damage over time is reduced by fire resistance; players expect a fire-resistant enemy to burn less. Max-HP-percentage effects are usually left unresisted so they stay a reliable answer to tough enemies, and reflected damage returns what the attacker dealt. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B or C.
**Notes for Claude:** B-064b: the DoT effect needs its element (check `FActiveSkillEffect`); apply `WoRDamageMath::GetElementMultiplier(GetTypeResistance(target, element), false)` to the tick (a Water DoT also reads Ice, Q-145b, as `GatherDamageMathInputs` does); tests: a Fire DoT on a Fire Caster ticks less. Built (B-064b): `USkillEffectManager::GetDotTickDamage` (`WoRResistance::GetElementDamageResistance`). The status-bar cap DOT (8% of max HP, `FActiveSkillEffect::bShareOfMaxHP`) is left unresisted like Reality Burst: it is a share of max HP, and resistance already cut the buildup that capped the bar. DoT ticks don't fire `OnElementDamageDealt` (hits only).

### Q-217 — Requirements in combat: the character's authored world levels, or the live run levels?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-217) · **Blocks:** — (B-217 ships with A)
**Question:** In a run, each world pillar has a live level: the authored level + the head start + levels earned or bought (CL-37). The stat getters read the live level, but the requirement gap (CL-39), the weapon requirement factor (CL-40), Broken Darkness strain and its exact-attunement waiver read the authored level, so a pillar raised during the run doesn't count. Which should they read?
**Options:**
- **A** The live level: a pillar raised in the run meets requirements at once.
- **B** The authored level (today): raising a pillar changes the stats but not the requirements.
**Recommendation:** A. In Dark Souls and Elden Ring, a level-up counts toward weapon and spell requirements at once; that is what makes levelling a stat for a weapon feel rewarding. The live value was built as the combat value (`CharacterDataComponent.h`: "C2 repoints readers here"). Numbers change only for a character with a head start or levels bought or earned this run. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B.
**Notes for Claude:** B-217: `FWorldPillarLevels::FromComponent`; the combat readers in `ActionExecutor` pass it. Not changed: the memory requirement fit (`MemoryConditions.cpp`, reads the party snapshot's asset, TODO there), `URequirementGapDebug::GetRequirementGapString` (takes an asset; `GetLiveRequirementGapString` reads the live levels), and the Blueprint-only `UBrokenDarknessManager::DoesSpellExceedRequirements` / `DoesAbilityExceedRequirements` (asset levels, no C++ caller now). TODO: an executor-level test of the strain and attunement reads (`CheckBrokenDarknessBreak` is private; they use the same `GetLiveWorldLevels` helper the weapon factor test covers). `FWorldStatRequirements::CalculatePenalty` stays as recorded: Blueprint-only, not applied to damage (B-117).

### Q-162 — A weapon asks for more than one pillar: which gap sets its damage factor?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-162) · **Blocks:** — (B-162 ships with A)
**Question:** D-082 / Q-146c: under a weapon's requirement it gives less (the requirement-gap ladder, capped at 100%). A weapon can require Mind, Body and Spirit at once, so up to three gaps. How do they make one factor?
**Options:**
- **A** The worst pillar decides: the factor is the lowest of the three pillars' ladder values (each clamped to [floor, 1]).
- **B** Multiply them: each pillar you're short on cuts again (two pillars at gap +2: 0.78 x 0.78 = 0.61).
**Recommendation:** A. In Elden Ring, missing any of a weapon's stat requirements gives one flat attack penalty, not one per missed stat; being one point short on several stats isn't punished more than being short on one, which players found fair. B compounds quickly toward the floor. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B.
**Notes for Claude:** `FWorldStatRequirements::GetWeaponRequirementFactor` (`Min3`); the floor `WeaponRequirement::WEAPON_REQ_FACTOR_MIN` is 0.25 (`// TODO: tune`; the ladder's lowest rung is 0.32, so it doesn't bite today). Two readings built with the plan (`StatMathsRedesign.md` 2.9, "a separate damage factor on attacks and abilities"), for Crown to change if wanted: the factor scales the whole pre-defense damage of the attack or ability (the skill's base too, not only a weapon-only share, since this game has no separate weapon damage term), and it scales **damage only**: status buildup from the hit is unchanged.

### Q-218 — Which single tuning values move into the tuning data now?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-218) · **Blocks:** — (B-218 ships with A)
**Question:** B-218 puts the ladders into curve tables and "single tuning values (caps, per-point bases, Broken Darkness rates)" into `UCombatTuningData`. The stat caps and per-point bases (`UNIVERSAL_STAT_CAP`, `RESISTANCE_PER_POINT`, `PCT_STAT_PER_POINT`, …) feed compile-time formulas (`STAT_DERIVE_DENOM` and the derived per-point values in `CombatConstants.h`) read in many places. Which single values move now?
**Options:**
- **A** Now: the infusion charge bands and the Broken Darkness strain values (one reader each). The stat caps and per-point bases stay C++ until B-042, where they become MMC inputs.
- **B** Now: everything, including the caps and per-point bases (turns the derived constants into runtime values across every stat reader).
**Recommendation:** A. The plan already lists structural values derived from the caps as staying in C++ (`StatMathsRedesign.md` §3). In GAS the caps belong in the MMCs that compute the sub-stats (Epic's GASDocumentation: MMCs read their coefficients from scalable floats), so B-042 moves them once, in their final shape. Doing B now would touch every stat reader twice. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B.
**Notes for Claude:** B-042: the MMCs read the caps and per-point bases from `UCombatTuningData` (add the fields then, defaults = the constants).

### Q-216 — A hit or a resistance buff left with the Generic element: which resistance is it?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-216) · **Blocks:** — (B-216 ships with A)
**Question:** Resistance is now one value per damage type (`EWoRDamageType`). `ESpellElement::Generic` means "inherit the caster's element" and is resolved at cast time, so a hit should never reach resistance as Generic. If one does (or a resistance buff is authored with Generic), which type is it? Before B-216 a Generic hit only matched Generic-keyed resistance buffs, and the class table and gear gave it 0.
**Options:**
- **A** Non-elemental, like `None`: it reads the non-elemental bucket (`Resist_NonElemental`), and a Generic-keyed buff counts there too.
- **B** Its own fourteenth resistance, kept only for Generic.
**Recommendation:** A. No outside game has this case; it follows this game's own rule that Generic is "inherit" and never a real element (`ESpellElement.h:28-31`), and it keeps one resistance per damage type. Nothing in the game sends a Generic hit to buildup today (every caller passes a resolved element), so no number changes. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B.
**Notes for Claude:** `WoRDamageType::ElementToDamageType`; `wor.CompareResistance` leaves Generic out because it differs from the old composition by design.

### Q-146 — The stat maths plan: approve it, and where do the tuning numbers live?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-146) · **Blocks:** — (B-218, B-219 ship with A)
**Question:** `docs/analysis/StatMathsRedesign.md` maps every stat-maths system to the GAS shape (attributes, modifiers, MMCs for stat-derived bases, one damage and one buildup execution) with the same numbers, in steps: tuning data (B-218), one damage core shared with the AI (B-219), then GAS stage 3 (B-042). Approve it, and where should the ladders and caps live (all C++ `constexpr` today)?
**Options:**
- **A** Approve; ladders as curve tables (`FScalableFloat`), single tuning values in one `UCombatTuningData` data asset, read through one accessor that falls back to the C++ value.
- **B** Approve; keep every number in C++ (`CombatConstants.h` etc.).
- **C** Change the plan (say what).
**Recommendation:** A. Curve tables and `FScalableFloat` are Epic's standard for values by level in GAS (GASDocumentation, attribute init and scalable floats), and Lyra keeps its tuning in data assets; designers tune without a rebuild. The C++ fallback means a missing asset can't change the game. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** curve interpolation must be constant (step), not linear; a spec checks every key. Broken Darkness stays custom (D-P01) but reads its rates from the same data.

### Q-146b — Ignore Defense as an amount (D-086): what does it ignore?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-146, D-086) · **Blocks:** — (B-042 ships with A)
**Question:** D-086 makes Ignore Defense an amount per skill ("ignores 30% of Defense"), like the deckbuilder's Ignore Defense N. There, N is **one budget**: it removes the hit's element Resistance first, then plain Defense with what's left; a hit with no element only reaches Defense (`wor-deckbuilder/docs/reference/04-effects.md`, `IgnoreDefense`). Here Defense and elemental resistance are both percentage reductions, so a shared budget doesn't translate directly. Which does the amount reduce?
**Options:**
- **A** Both (an adaptation of the deckbuilder's budget to percentages): the amount scales down the target's **positive** elemental resistance for that hit and its Defense (a 30% ignore keeps 70% of each); weaknesses (negative resistance) are untouched; a hit with no element only reaches Defense.
- **B** Defense only (D-086's example wording).
**Recommendation:** A, the closest percentage form of the deckbuilder's rule (it comes first where it has one): there, an ignore reaches the element's resistance as well as Defense. Path of Exile's penetration works the same way on resistances. Weaknesses stay, so ignoring never makes a hit weaker. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B.
**Notes for Claude:** today's all-or-nothing skills map to 100% (Defense only, as now; resistance didn't touch damage before B-064). The Defense half is built in the damage execution (B-042); the resistance half with B-064 (its row says so).

### Q-146c — Being over a weapon's requirement (D-082): a bonus, or capped at 100%?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-146, D-082 "to confirm") · **Blocks:** — (B-162 ships with A)
**Question:** D-082: under a weapon's requirement you don't get its full output (the ±5 requirement-gap ladder). Skills also get the ladder's bonus when you're over their requirement. Does the weapon?
**Options:**
- **A** The weapon caps at 100%: under the requirement the ladder's penalty applies, over it nothing extra.
- **B** The full ladder both ways, as skills do (up to ×1.40 at +5).
**Recommendation:** A. In Dark Souls and Elden Ring a weapon's requirement only removes a penalty; extra stats help through the weapon's scaling, not the requirement. Here the skill's own requirement and its per-skill stat scaling already reward being over (D-041), so B would pay the same stats twice. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may pick B.
**Notes for Claude:** B-162 builds it as a separate damage factor on attacks and abilities through the weapon (not ActionMods, which spreads a pillar percent to every sub-stat and has no floor), `clamp(ladder, WEAPON_REQ_FACTOR_MIN, 1)` (the floor a named constant, `// TODO: tune`). Broken Darkness strain keeps reading the weapon requirement as today.

### Q-145 — The resistance plan: approve it, and does physical resistance cut physical damage?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-145) · **Blocks:** — (B-216 ships with B)
**Question:** `docs/analysis/ResistanceRedesign.md` puts resistance in the standard GAS shape: a damage type beside the element (D-037), one resistance attribute per damage type fed by modifiers (class table, gear, buffs), and the damage and buildup calculations reading them, with every buildup number unchanged. One rule isn't settled: D-036 says per-element resistance scales elemental damage, but the class table and gear also carry Slash / Pierce / Impact. Does a physical subtype's resistance scale physical damage?
**Options:**
- **A** Approve the plan; physical subtype resistance scales physical damage the same way (1:1, capped ±50%), with Defense still applied after it. A Generic takes 15% less physical damage, a Resonator 15% more.
- **B** Approve the plan; physical damage is reduced by Defense only (subtype resistance stays buildup-only, as today).
- **C** Change the plan (say what).
**Recommendation:** B. D-005's notes already say to leave the physical columns out of damage if they double-count with Defense, and they do: resistance (up to 50%) then Defense (up to 85%) could reach about 92.5% reduction, the stacking toward immunity D-036 avoided. Path of Exile and Diablo 4 do the same split: elemental resistances per element, physical damage reduced by armour. B changes no balance. A (Elden Ring's per-subtype negation on top of armour) stays open for Crown. Not lore.
**Answer (Crown):** **B, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may pick A.
**Notes for Claude:** the plan keeps a `Resist_<subtype>` attribute either way (buildup reads it), so A later is one step in the damage execution.

### Q-145b — Does Ice resistance also resist Water?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-145, D-029) · **Blocks:** — (ships with A)
**Question:** Ice is a Water damage type (D-029, which took the deckbuilder's model). Confirming the reach for the plan: which resistance does each read, for damage and buildup?
**Options:**
- **A** The deckbuilder's rule (`wor-deckbuilder/docs/reference/02-enums.md`, "Ice — a damage type"): Ice resistance resists **Ice and Water**; Water resistance resists **Water only**.
- **B** Each reads only its own resistance.
**Recommendation:** A, the deckbuilder's rule (it comes first where it has one). It makes Ice resistance the stronger, rarer pick, matching Ice as "a heavier Water". Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may change it.
**Notes for Claude:** a Water hit reads `Resist_Water + Resist_Ice` (damage and buildup); an Ice hit reads `Resist_Ice`. Built in B-064a.

### Q-145c — Build the resistance seam now, or wait for GAS stage 3?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-145) · **Blocks:** — (ships with A)
**Question:** B-064 (damage resistance), B-080 (damage type) and B-064a (Ice) wait on this plan. GAS stage 3 (B-042) also waits on B-146. Build them on the new shape now, or after stage 3?
**Options:**
- **A** Now: B-216 adds the damage type, one per-type resistance query, the class table as a data asset and a mirrored resistance attribute set (no behaviour change); B-064 / B-080 / B-064a build on that query; stage 3 later swaps the query's inside to read the attributes.
- **B** Wait: build them inside B-042.
**Recommendation:** A. CLAUDE.md's "Incremental" rule (build new beside old, prove it, then switch), and it's how B-041 mirrored stats before stage 3 (`GASSystem.md`). Lyra and GASDocumentation set up attributes before effects use them. Waiting would hold three jobs behind the largest one. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27). Crown may change it.
**Notes for Claude:** B-216's proof is "every buildup number identical" (`wor.CompareResistance`, `wor.CompareStats`, a spec).

### Q-144 — V's clothing: painted on, bought, or custom-made
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-144) · **Blocks:** — (ships with A, then B)
**Question:** V's black suit, gold tie, elite look and the hub NPCs' role outfits need clothing on the UE5 Manny/Quinn skeleton (`docs/analysis/MobClothing.md`). Paint it in V's material, buy a rigged pack, or have one made?
**Options:**
- **A then B** Paint the suit, tie and element trims as masks in B-143's one V master material, with the tie as a small attached mesh (free, ships with B-143). Then add one bought suit pack rigged to the UE5 skeleton (Fab Standard License, about $20–40) as a Leader Pose child for a real suit silhouette. Elite V and the hub roles add one piece each (a coat, an apron) and socketed props.
- **B only** Buy a pack first; V waits on the purchase. (C, single pieces from a collection, is B in smaller parts; E, a retargeted character pack, is a parts source only.)
- **D** A custom suit (Marvelous Designer or a commission), weight-transferred to Manny in-engine; matches the turnaround sheet exactly, highest cost.
**Recommendation:** A then B. Destiny's majors (the stronger version of a common enemy, read at a glance; [Destinypedia](https://destinypedia.com/Major)) and Expedition 33's Chromatic Nevrons (recoloured, stronger versions of normal enemies; [guide](https://www.next-stage.fr/2025/06/guide-clair-obscur-expedition-33-comment-battre-boss-bourgeon-chromatique.html)) give tough variants from a shared model: that is D-069's one-body rule with an elite add-on. Monster Hunter's subspecies (a new colour and element on one model; [GameSpot](https://gamespot.com/articles/monster-hunter-rise-sunbreak-new-monsters-guide/1100-6505092/)) show element recolours are accepted when the colour means something. Not lore.
**Answer (Crown):** **A then B, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Which pack to buy (and any later commission, D) is Crown's: Claude doesn't buy.
**Notes for Claude:** A is built in B-143 (CC+UE): suit/tie/trim masks in the master material, element instances, tie mesh. B needs Crown's purchase first: only UE5-skeleton listings (some project content is UE4 mannequin), and confirm the pack's bone names match `SK_Manny_HTS` before buying (Leader Pose matches by name). B-143's material also needs a body opacity mask so the body doesn't poke through bought clothing. Hub props attach to hand sockets (B-195's outfit/prop slot).

### Q-203 — The item inventory: what one item type is, and how big a stack is
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-203, D-097) · **Blocks:** — (ships with A)
**Question:** D-097 gives the party an inventory of item slots (one item type per slot, stacked to the item's max) and unlimited storage. Building it needs: what one "item type" is, each item's stack size, whether the old per-tier crystal cap still applies, and who shares the slots.
**Options:**
- **A** One item type = **crystal type and tier** (Garnet F and Garnet E take two slots). Every stack holds up to **10** for now (`ITEM_STACK_MAX`, one value until items carry their own). The slots **replace** the old cap of 20 per tier for the party (the loadout's own tier caps, Q-155, still apply in a fight). The **whole party shares** one set of slots and one storage (D-020); a character's starting items go in once; a trial recruit brings none (Q-076). Found items, a merge's change and dropped crystals fill the slots and overflow to storage, so nothing is refused for space. A fight's auto-equip takes from the slots, and what the fight doesn't use goes back after it. Moving items between slots and storage is the placeholder `wor.UI.ItemStorage` until rest spots (B-187) and the hub screen.
- **B** One item type = crystal type only (all tiers in one slot; fewer slots used, but a slot then mixes tiers).
- **C** Keep the per-tier cap as well as the slots (two limits on one inventory).
**Recommendation:** A. Monster Hunter's item pouch, which D-097 follows, gives each distinct item its own slot and stack limit; a potion and a mega potion are two slots. Resident Evil 4's attaché case and Diablo II's belt make the same trade: a small carried space and a big stash at home. One limit (slots) is easier to read than two. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it, e.g. the stack size per item.
**Notes for Claude:** applied (feature/B-203-item-inventory-storage). Still to come: auto-refill at rest spots (D-097, with B-187); buying more slots with Prisms (D-097 growth, a hub vendor job); found gear in a run's gear list. An authored loadout's item slots still start with their authored crystals each spawn (free crystals, as before; only crystals taken from the carried items go back after a fight). Known limit: crystals auto-equipped for a fight that never ends (the level closes mid-fight) are not given back.

### Q-208 — Resuming a saved run: where the player lands, and what comes back
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-208, Q-090 = A) · **Blocks:** — (ships with A)
**Question:** Q-090 resumes a quit run at the last fight's exit. Three details: where in the trial the player stands, whether the run's Gold comes back (B-178 keeps Gold in memory only, never banked), and what happens to a save loaded somewhere other than the game's start.
**Options:**
- **A** The save written at each fight's exit holds the run: the trial, seed, gate choice, beaten encounters, found features, run memories, the party's HP/EP, crystal wear and absorption, and the run's Gold (with the run, not the wallet; still lost when the run ends). Loading it at the game's start (the hub) reopens the trial level without a fresh start: the map comes back as it was (beaten fights gone, found chests open) and the player stands at the trial's start point, as after any fight. A trial's exit writes "no run". Loaded in any other level (a PIE test map), the run is not resumed and is dropped at the next save, with a warning. The memory journal is not part of it.
- **B** Drop the run's Gold on a resume (Gold strictly never written).
- **C** Save the player's position too (needs a position snapshot every save point; fights already return to the start point).
**Recommendation:** A. Slay the Spire's save-and-quit keeps the floor, the deck and the gold, and Hades resumes the chamber with its obols; losing the run's money on a quit would feel like a penalty for closing the game. Landing at the start point matches how the game already returns from every fight. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** the shutdown save also captures the run, so a quit keeps chests opened after the last fight, and a quit mid-fight keeps the vitals from before that fight. Applied (feature/B-208-save-trial-run): `FSavedRun`, `FSavedCarriedGear`, `FSavedPartyMember::CarriedGear` / `CarriedAbsorption`, save version 2 (version-1 saves load with no run). `UTrialRunSubsystem::RestoreRun` / `ResumeRun`; `USaveSubsystem::CaptureRun` / `RestoreRun`.

### Q-171 — A gear change's drops: how much is dropped, and during a fight?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-171, D-081 = A) · **Blocks:** — (ships with A)
**Question:** D-081 drops the parts a gear change breaks. A loadout can also hold problems the change didn't cause (an unlearned innate spell, a duplicate item slot), and a socket can in principle change mid-fight. Does the change drop those too, and does it drop during a fight?
**Options:**
- **A** Drop only what a socket can break: the weapon's or ring's spells, the weapon's abilities, and the evolution / ring / ability overflows, in every loadout holding that gear, at once, with a message. Other problems wait for the pre-fight clean (`ClearInvalidSlots`), as today. During a fight nothing is dropped; the pre-fight clean handles it before the next one.
- **B** Clean the whole loadout on any gear change (drops things the player didn't touch, with no obvious cause).
- **C** Refuse socket changes during a fight (no path makes one today; a later choice if one appears).
**Recommendation:** A. Path of Exile and Diablo II drop a gem's skill the moment it leaves the socket, and only that skill, which players read as cause and effect; Final Fantasy VII's materia does the same. Leaving the fight's loadout alone mid-battle follows Persona and Fire Emblem, where equipment is settled outside the fight. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applied (feature/B-171-gear-change-drops): `UInventoryComponent::OnGearSocketChanged`, `ULoadoutComponent::HandleGearSocketChanged`, `DropInvalidParts(Index, OnlySlotTypes)`. The message is today's `Removed: <rule reason>` (reason text still a built string, D-061 TODO).

### Q-085 — Owned gear and loadouts: which copy is the truth, and when they sync
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-085) · **Blocks:** — (ships with A)
**Question:** A loadout holds its own copy of each weapon, ring and primary evolution. Combat wear lands on the active loadout's copy; a level-up, an attached crystal or a dismantle lands on the owned gear. Neither reaches the other, so the same sword can be worn in one place and fresh in another. Which is the truth, and when do they meet?
**Options:**
- **A** The **owned gear is the truth**. Each loadout slot points at an owned piece by its ID (weapons and rings by `PersistentID`, the primary evolution by its `InstanceID`) and keeps a copy only as a cache. An owned change (level-up, downgrade, a crystal / evolution / fusion attached or removed) refreshes every loadout holding that piece at once; a dismantled piece leaves every loadout. **During a fight the active loadout leads** (wear, breaks and repairs land there as today) and is written back to the owned gear when the fight ends, when the active loadout is switched, and when a trial run's carried state is applied. A loadout slot whose authored ID matches nothing owned is linked to an owned piece of the same gear. What a loadout picks (a Resonator ring slot's spells, a weapon's assigned abilities) stays the loadout's own. Carried gear between a run's fights (B-115) stays as it is.
- **B** Remove the copies: every slot reads the owned entry each time (about a hundred call sites; a larger, riskier change for the same result).
- **C** Keep copies and sync only at fight start (a level-up in the hub still shows the old tier in the gear screen until a fight).
**Recommendation:** A. Diablo, Path of Exile and Final Fantasy XIV's gear sets are references to items in the inventory, never copies: upgrading or breaking an item shows in every set that uses it, which players expect (FFXIV's gear-set "item not found" handling is the familiar case of a set losing a piece). Writing combat state back at the fight's end follows Darkest Dungeon's and Fire Emblem's durability, which is settled per battle. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** owned changes are made out of a fight (the gear screen, the merchant, the hub); a mid-fight owned change to a piece would win and drop that piece's wear from the fight so far (no path does this today). One evolution, one slot: a second loadout naming an evolution already linked keeps its own copy, with a warning. Dismantling an evolution also takes it off any weapon or ring holding it. Applied (refactor/B-085-owned-gear-truth): `UInventoryComponent::LinkLoadoutsToOwned`, `RefreshLoadoutsFromOwned`, `RefreshEvolutionInLoadouts`, `WriteLoadoutToOwned`, `GetOwnedGearSyncString`.

### Q-212 — Mid-fight reactions and boss triggers: when they fire and what a mid-fight change does
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-212, Q-191 changed by Crown) · **Blocks:** — (ships with A)
**Question:** Reactions and boss triggers can now fire mid-fight, at a turn start, a hit, an HP threshold for either side or a boss phase change, still once per fight each. How is the moment authored, whose turn / hit / HP counts, and what does a mid-fight weaken or strengthen do to the boss's HP?
**Options:**
- **A** Each reaction and boss trigger gets **When**: a moment (**Fight start** (the default, as today), **Turn start**, **Hit** (deals damage), **Damage taken**, **HP below %**, **HP above %**, **Boss phase**) and a side (**Ours** / **Theirs**). For a reaction, Ours is the wearer and Theirs the faced character; for a boss trigger, Ours is any party member and Theirs the boss. Who matches (the worn items, the party, the gear) is still decided at fight start; only the firing waits. An HP threshold fires when it is **crossed** (true now, false before), never at the start. Boss phase fires when the boss (or, for a reaction, the faced character) reaches any phase. A mid-fight weaken or strengthen keeps the boss's **share of HP** (a boss at 60% stays at 60% of its new max); every step this fight adds to one net change, still capped at 3. Skip / add phase stay at most once per boss per fight.
- **B** Reuse every effect-system trigger (crit, dodge, status applied...) as a moment (many never reach the battle).
- **C** A mid-fight change refills the boss's HP (a free heal for the boss, or a free kill when weakened).
**Recommendation:** A. Final Fantasy XIV and Monster Hunter bosses change at HP thresholds and phase changes ("at 50% it enrages"), which players read as the fight's rhythm; Persona and Fire Emblem's mid-battle lines fire once when a set moment happens. Keeping the HP share follows how Elden Ring's and FFXIV's boss transitions preserve progress, so a change never undoes the player's damage. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** a per-fight `UFightTriggers` owned by `ABattleGameMode` (D-064) holds the waiting triggers and listens to the turn manager, the action executor's damage and the combatants' HP changes, and the bosses' phase delegates. Applied (feature/B-212-mid-fight-triggers): `Reaction/FightMoment.h`, `Reaction/FightTriggers.h`, `BossTriggers::ApplyResults` / `FBossFightState`, `ReactionTriggers::ApplyBossSteps(..., bBeforeFight)`. Details settled with A: nothing fires on a killing blow or for a downed wearer, faced character or boss (it keeps waiting); a mid-fight reaction unlocks its memory only when it fires; a boss phase is always the boss's (or the faced character's), whatever the side.

### Q-209e — Equip conditions relative to requirements: how "above" and "under" are measured
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-209, D-100 "Equip conditions", clarified by Crown) · **Blocks:** — (ships with A)
**Question:** Crown clarified that an equip condition can ask for a character equipping a skill, spell, ring, weapon or evolution **while meeting its requirements, above them by some amount, or under them**. How is "above by N" / "under by N" measured across the three pillars (Mind, Body, Spirit), and what of things with no requirements?
**Options:**
- **A** Each gear or skill condition gets **Requirement Fit** (Any / Met / Above / Under) and **Requirement Margin** N, measured on the wearer's world levels against the item's or skill's `Requirements` (the same numbers as the D-082 / CL-39 requirement gap). **Met**: every pillar meets it. **Above by N**: met, and every pillar the item asks for is at least N over (N 0 = met). **Under by N**: some pillar is at least N short (N 0 counts as 1). Something with no requirements (evolutions have none) is always **Met**, never above or under. A level-up with an unknown wearer can't satisfy a fit other than Any.
- **B** Sum the three pillars' gaps (one number; hides which pillar is short).
- **C** Use the gap's damage multiplier as the threshold (hard to author).
**Recommendation:** A. Dark Souls and Elden Ring judge requirements per stat (one short stat is "under", shown in red), which players read at a glance; measuring the same way keeps the hidden condition consistent with the requirement gap players already feel in damage (D-082). "Under" quests follow Elden Ring's and Dark Souls' low-level challenge runs, which players celebrate. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** applies to GearEquipped, EvolutionEquipped and SkillSlotted. Crown's clarification is about **equipping**; the **used** kinds from Q-209d (skill used, Breakthrough, finishing blow) stay as extra kinds (Crown, B-213). Applied (feature/B-209-requirement-fit): `EMemoryRequirementFit`, `FMemoryCondition::RequirementFit` / `RequirementMargin`, `MemoryConditions::FitsRequirements`. It reads the character asset's world levels, as the D-082 gap does today; a pillar raised during the run doesn't count yet (TODO: both move to the live stats, `UCharacterDataComponent::GetWorldStat`).

### Q-209d — Memory conditions on gear and skills: equipped, levelled, used
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-209, D-100 "gear and skills can trigger them") · **Blocks:** — (ships with A)
**Question:** Crown added gear and skills as memory triggers: evolutions, weapons and skills equipped or slotted, their upgrade level, and **used** (a skill cast, an evolution's Breakthrough, a finishing blow with a certain weapon). Which condition kinds, and when does "used" count?
**Options:**
- **A** Five new kinds, appended: **EvolutionEquipped** (the evolution in the primary slot, at least UpgradeLevel tiers above its authored tier; an evolution level-up counts it as equipped), **SkillSlotted** (the skill is in the wearer's loadout: attacks, abilities, spells), **SkillUsed** (the skill was used in the fight just won), **BreakthroughUsed** (a Breakthrough spell was cast from that evolution in the fight just won), **FinishingBlow** (a kill in the fight just won by whoever held that weapon). Each takes an optional Character (empty: anyone). Weapons and rings keep GearEquipped. "Used" is recorded per fight on the battle (D-064) from the action executor's start and kill events, and checked at the fight-won event, so a fight lost or fled counts nothing.
- **B** One generic "Used" kind with a target object (harder to author, harder to read in `wor.Memory.Print`).
- **C** Count use across the whole run (needs a saved tally; no event to check it at).
**Recommendation:** A. Destiny 2's exotic catalysts and weapon quests (kills with a given weapon) and Monster Hunter's weapon-specific requests unlock by using the thing itself, and players like them because they reward playing with the gear, not grinding a counter; checking at the fight's win follows Bloodborne's and Elden Ring's questline flags, which are set at an event, never by a menu. One kind per thing keeps each condition readable. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** usage lives on `ABattleGameMode` for one fight; `MemoryConditions::RecordAction` / `RecordKill` fill it so specs can drive it without a fight. Applied (feature/B-209-gear-skill-conditions): the five kinds in `Memory/MemoryData.h`; `FMemoryFightUsage` on `FMemoryEventMember`; `ABattleGameMode::FightUsage` / `WatchFightUsage`; `EconomyService::LevelUpEvolution` is an upgrade event. A deferred spell counts as used once armed (committing it is the use), even if the fight ends before it fires; a finishing blow counts only on an enemy; kills from damage over time don't reach `OnTargetKilled`, so they aren't finishing blows (TODO); an evolution level-up counts as equipped even when not slotted (as A says).

### Q-209c — Memory alternative routes and rewards: data and placeholder grants
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-209, D-100 rewards and alternative routes) · **Blocks:** — (ships with A)
**Question:** Crown added two things to memories: several condition sets (any one opens it) and rewards (a unique item, a skill, a title or a curse). How are they authored, and what does a reward do before titles, curses and the item grant path exist?
**Options:**
- **A** `UMemoryData::UnlockConditions` stays the first set; `AlternativeConditionSets` adds more (each a list, all within it needed). A memory opens when any non-empty set fully holds; with no non-empty set, only a source naming it opens it (as before). `Rewards` is a list of `FMemoryReward` (kind Item / Skill / Title / Curse, with the item, skill or id). For now every reward is **logged** when its memory unlocks (`[MemoryUnlock] reward: ...`), with a `// TODO` per kind: the item goes to the party's inventory once out-of-fight gear is reachable (B-085), skills once learning from rewards exists, titles and curses once they are systems. `wor.Memory.Print` lists both.
- **B** Replace `UnlockConditions` with a list of sets (breaks memories already authored with conditions).
- **C** Grant the item now into the leader's inventory.
**Recommendation:** A. Shangri-La Frontier's Unique Scenarios and Destiny 2's exotic quests open by more than one route and hand out unique, sidegrade rewards, which players praise for rewarding curiosity without power creep. Keeping the first set where it is leaves anything already authored working. Logging until the grant systems exist follows the dialogue / quest placeholders already in the game. Not lore (which rewards is Crown's).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** grants go in `MemoryUnlock::Unlock`, the single unlock path, so every source gives the rewards once. Applied (feature/B-209-condition-sets-rewards): `FMemoryConditionSet`, `UMemoryData::AlternativeConditionSets` / `GetConditionSets` / `HasConditions`, `FMemoryReward` / `Rewards`; `MemoryConditions::AreMet` any route; rewards logged in `MemoryUnlock::Unlock`.

### Q-211 — Boss triggers: conditions, results and how they add up
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-211, D-100 part 1) · **Blocks:** — (B-211 ships with A)
**Question:** D-100 part 1 gives each boss / mini-boss its own "if these conditions, then this" rules, using only what's true in the fight. Which conditions and results exist now, when are they checked, and how do several add up?
**Options:**
- **A** `UBossData::Triggers`: each trigger has conditions, all needed: **Party member** (a character in the party), **Gear worn** (an item worn, by a given character or anyone, at least N tiers above its authored tier), **Hidden** (never met for now, TODO). Its results are any of **weaken**, **strengthen**, **skip a phase**, **add a phase** and a **dialogue line** (an id, logged until the dialogue system, B-194). Checked once at fight start, after the boss's phases are set up. Weaken / strengthen from every fired trigger add up to one net change (−1 / +1 step each, the same 15% max HP and damage step and ±3 cap memories used). Skip a phase and add a phase each happen at most once per boss per fight, however many triggers ask (no doubled last stand, no boss stripped of every phase; Hades' Pact of Punishment likewise offers each modifier once). Memories no longer change bosses: the fight-start hook stops reading run memories; `UMemoryData::BossModifiers` stays, marked obsolete, until Crown's PIE.
- **B** Each trigger authors its own percentage instead of steps.
- **C** Re-check triggers during the fight (a swap of gear mid-fight).
**Recommendation:** A. Elden Ring's Rykard (the Serpent-Hunter makes the fight winnable) and Sekiro's Divine Confetti show the right gear changing a boss fight, and players remember those moments; Hades' bosses greet you with lines about the weapon aspect you bring, widely praised. Steps keep triggers comparable and capped so stacking can't trivialise a boss; checking once at the start matches reaction triggers (B-191) and fixed-loadout fights. Not lore (which bosses react to what is Crown's).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** conditions reuse the memory condition check (`MemoryConditions::IsMet` on a fight-start event), with only the fight-true kinds exposed (D-100: not memory-based). Applied (feature/B-211-boss-triggers): `UBossData::Triggers` (`FBossTrigger`), `Combat/Boss/BossTriggers.h`, `ReactionTriggers::ApplyBossSteps` (shared with the obsolete memory path), `ABattleGameMode` fight start.

### Q-209b — Memories by conditions: how the conditions are checked
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-209, D-100 restructured) · **Blocks:** — (B-209 ships with A)
**Question:** D-100 part 2 gives each memory a list of unlock conditions, all needed, checked on events. Which condition types exist now, what does each event know, and what does a source do when its memory's conditions aren't met?
**Options:**
- **A** A memory's `UnlockConditions` is a list of typed conditions: **Character** (in the party), **Gear equipped** (an item worn, by a given character or anyone, at least N tiers above its authored tier), **Location** (an id: the trial, or the fragment / spot where it happens), **Enemy beaten** (in that fight), **No damage taken** (the fight was won with no party member below their HP at its start), **Prior memory** (already in the journal; this is how memories chain), **Hidden** (never met for now, TODO). The events are a fragment pickup, reaching a map spot, a gear upgrade (the levelled item counts as equipped) and a fight won (a beaten boss is that event's source). Each event checks every memory that has conditions, plus the memory the source itself names (which needs its own conditions, and none means always). A memory earned in an event can satisfy the next one's Prior memory in the same event. A fragment or spot whose memory isn't unlocked stays where it is (come back with the right party); pressing at a fragment shows "Nothing happens." Out of a fight, an event knows the party's characters but not their worn gear (loadouts live on combat pawns until B-085), so gear conditions are met there only by the upgraded item.
- **B** One condition struct with every field (all set ones must hold), no kinds.
- **C** Conditions only on the source (the fragment, the boss), not on the memory.
**Recommendation:** A. Shangri-La Frontier's Unique Scenarios (Crown's model) start from hidden mixes of who you are, where you go and how you fought; Hollow Knight and Elden Ring NPC questlines chain one discovery to the next, and players praise working them out. A typed list is the boss-phase condition model already in use (D-098, `FBossPhaseCondition`), so it reads the same in the editor. Not lore (which memories and conditions to author is Crown's).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** building on B-209's first pass (in dev): `MemoryUnlock::Unlock` stays the single grant (journal once, B-210). The old per-source fields (`MemoryOnDefeat`, `UnlockMemory`, fragment / spot `Memory`, `BoundMemories`) stay as the memory the source names, until Crown's PIE; B-211 moves boss changes off memories. Applied (feature/B-209-memory-conditions): `Memory/MemoryConditions.h` (`FMemoryEvent`, `CheckEvent`, `wor.Memory.Conditions`), `FMemoryCondition` / `UMemoryData::UnlockConditions`, `EMemorySource::FightWon`; events in `AMemoryFragment::Interact`, `AMemorySpot::Reach`, `UEconomyService::LevelUpWeapon` / `LevelUpRing`, `ABattleGameMode::CheckFightWonMemories`. The Backlog's "entering a trigger volume" is `AMemorySpot`, and "per character" is the Character condition (one memory per character). Still outside the conditions until Crown's PIE: reactions' `UnlockMemory` and `BoundMemories` (B-209's first pass) unlock without checking the memory's own conditions. Each event loads every memory asset (TODO: cache).

### Q-209 — How the new memory sources behave
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-209) · **Blocks:** — (B-209 shipped with A)
**Question:** D-100 adds four memory sources: bosses, reactions, map spots and bound gear. When does each one unlock, and how often?
**Options:**
- **A** (built):
  - **Bosses and mini-bosses:** the boss data (`UBossData`) names an optional memory; **winning** the fight unlocks it. A mini-boss drops one only if it has boss data, which it already needs for its phases.
  - **Reactions:** a reaction can name a memory; it unlocks when the reaction fires at the start of the fight, and **counts in that same fight** (a memory that weakens this boss weakens it now).
  - **Map spots:** an unseen spot; **walking into it** unlocks the memory at once, with the line "A memory surfaces." (no key, no hint; the interact key in it works too). Once per run.
  - **Bound gear** (D-100 as Crown answered it): each memory on a bound item names an **upgrade level** (tiers above the item's authored tier) and/or **trigger conditions**: its owner wearing it, and/or facing a certain character (hidden conditions: TODO). It needs every one set. A memory with only an upgrade level unlocks when the item is levelled that far (in the shop); one with a trigger is checked when the item is worn into a fight, using the owned item's level. One with nothing set unlocks the first time it's levelled or worn into a fight.
  - Every source puts the memory in the run and the journal. The journal keeps it for good, with the **first** source it came from. Since B-210 (Q-190 changed by Crown), no source grants a memory the journal already has.
- **B** map spots need the interact key and show a hint.
**Recommendation:** A. Hollow Knight's and Dark Souls' lore spots trigger when you walk into them, so finding them by exploring is the reward; a boss memory on the win matches how Hades gives story beats after a boss. Bound gear follows Crown's D-100 answer (NieR: Automata's weapon stories). Not lore (which memory and what it says stays Crown's).
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above); bound gear is Crown's D-100 answer.
**Notes for Claude:** applied (feature/B-209-memory-sources): `Memory/MemoryUnlock.h`, `Memory/MemorySpot.h`, `FBoundMemory` (`Equipment/EquipmentDataBase.h`), `ABattleGameMode` (fight start, Victory), `UEconomyService::LevelUpWeapon` / `LevelUpRing`. A fight reads the owned gear's level by its id, since loadouts hold copies until B-085.

### Q-093 — Which keys do Defend and infusion use, and how do they behave?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-093) · **Blocks:** — (B-093 shipped with A)
**Question:** B-093 adds keys for Defend, the infusion source and the infusion level next to the command menu. Which keys, when do they work, and does infusion cycle or charge?
**Options:**
- **A** (built):
  - **Keys** (fixed for now, D-063): Defend **G** / gamepad **Y**; infusion source **R** / **LB**; infusion level **T** / **RB**. Each has its own action name (`IA_Combat_MenuDefend`, `IA_Combat_MenuInfusionSource`, `IA_Combat_MenuInfusionLevel`) so rebinding can come later.
  - **Same as the buttons:** a key presses the menu's own button, so it follows the button's rules. Defend works on your turn from the main menu **and from a submenu** (a shortcut out of it). The infusion keys work in the target picker when it offers them (not for items, a stunned actor or an immune action). While a target is being picked, Defend does nothing; on the Allies / Enemies step, no key does anything.
  - **Infusion cycles**, like the menu's button (level 0, 1, 2, 0). Hold-to-charge is not used (its dead `InfusionChargeManager` is set for removal, B-034).
  - **Defense keys win too:** the menu keys don't consume the press, so if a real-time Guard or Dodge key shares one of these keys, it still works on an enemy's turn.
- **B** no Defend from a submenu (the button isn't shown there).
- **C** hold-to-charge infusion on the key.
**Recommendation:** A. Divinity: Original Sin 2 and Baldur's Gate 3 give every action-bar command a key that does exactly what clicking it does, which is how players expect a menu shortcut to behave; letting Defend skip out of a submenu saves a Back press. Cycling keeps one behaviour for the button and the key. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Change any key you like.
**Notes for Claude:** applied (feature/B-093-menu-keys): `UCombatCommandMenuSubsystem::PressMenuKey`, `ACombatPlayerController::BuildMenuKeys`. The editor-authored defense context's keys can't be read here: PIE check 6 looks for a clash.

### Q-178 — How a run's paid repair works
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-178) · **Blocks:** — (B-178 shipped with A)
**Question:** D-093 says repair in a run costs Gold scaled by tier, at rest spots and vendors. How is it priced and bought?
**Options:**
- **A** (built):
  - **Price:** each worn item costs its tier's price (F 5, E 10, D 15, C 25, B 40, A 60, S 90 Gold, `GearRepair::GOLD_BY_TIER`, TODO tune) times the share of durability it is missing, rounded up. A scratch costs at least 1. A fusion is priced at the higher tier of its two halves.
  - **What:** one repair brings the whole party's worn gems, fusions (with a gem half) and evolutions to full, broken evolutions included. Augment stones and unbreakable evolutions never wear and cost nothing.
  - **Paying:** all or nothing. With too little Gold nothing is paid or repaired.
  - **Gold:** the run's Gold now carries across level changes (it used to reset on each one), and is cleared when a run starts and ends. It is never saved.
  - **Where:** rest spots and vendors don't exist yet, so the placeholder party screen (`wor.UI.PartyControl`) offers the repair anywhere. The real rest-spot and vendor screens call the same `GearRepair::RepairParty`.
- **B** a flat price per item, whatever its wear.
- **C** repair item by item.
**Recommendation:** A. Diablo's blacksmith charges by item level and missing durability, and players read it as fair: small wear is cheap. One button for the whole party matches Darkest Dungeon's and Diablo's "repair all", which players use far more than single repairs. Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above).
**Notes for Claude:** applied (feature/B-178-run-repair): `Equipment/Durability/GearRepair.h`, `USaveSubsystem::ClearRunGold`. Vendor and rest-spot screens: with the run map (D-047).

### Q-090 — Quitting mid-trial: resume the run, or abandon it?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-090) · **Blocks:** — (B-090 shipped with A now, B-208 for the rest)
**Question:** The save keystone saves the party, the banked wallet and the pool. The run itself (trial, beaten encounters, seed, carried HP, crystal wear) is not saved. What happens when the player quits mid-trial?
**Options:**
- **A** resume the run where it was left (at the last fight's exit), like Slay the Spire and Hades. Until that save exists, quitting abandons the run: the save writes no carried HP/EP, and a reload starts in the hub at full health.
- **B** quitting always abandons the run.
**Recommendation:** A. Slay the Spire (save and quit, resume the floor) and Hades (resume the chamber) both keep a run across a quit, and players expect it of a run-based game; losing a run to closing the game reads as a bug. Building it is its own job (B-208); B-090 does the abandon half safely now, so no damaged HP is carried into the hub.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). B-208 builds the run save.
**Notes for Claude:** applied (feature/B-090-save-keystone): `USaveSubsystem::CaptureParty(Party, false)`. `docs/Architecture/SaveSystem.md`.

### Q-045 — How this game's triggers, conditions and effects read as text
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-045) · **Blocks:** — (B-045 shipped with A)
**Question:** D-031 takes the deckbuilder's nested text. This game's effect data differs: its triggers and HP/Energy checks sit in one list joined by AND/OR, it has its own effect kinds (immunities, row moves, buildup, reflect...), and its skills have no card opener. How do those read?
**Options:**
- **A** (built), read exactly as the game evaluates them (all AND entries, and one of the OR entries):
  - Headings: your own event named ("On Hit"), another side's as a sentence ("When a Team Member takes damage"). Every AND event, then the either-or set: "On Critical Hit and either On Hit or On Dodge". A lone OR entry must hold, so it reads "and". An either-or set mixing an event and an HP/Energy check is one heading: "On Hit or while your HP is below 30%".
  - Each AND HP/Energy check is its own "While ..." level; all OR checks share one level ("While your HP is below 30% or your Energy is below 20%"). Thresholds print as percents.
  - An untriggered effect sits under **Passive** on gear and **Activate** on a skill (the deckbuilder's active-block word); Activate comes first and Passive last, as in the deckbuilder.
  - The deckbuilder's verbs where one exists (gain / lose / give, deal / take damage, heal, negate, reflect, drain); plain verbs for the rest ("become immune to Fire for 2 turns", "push an Enemy back one row", "add 10 Fire buildup to an Enemy").
  - What the effect really does: Skip Turn reads "delay an Enemy's next turn" (Wind's break pushes it back, D-014); Extra Action reads "take an extra turn" (one per use); a positive Energy Cost change reads "your Refractions and Abilities cost 20% less" (the cost change applies to both) and lights as a buff (the type's debuff classification is left as is). Gear that applies Skip Turn directly doesn't delay a turn yet; only Wind's break does. Flagged as a TODO in `docs/Architecture/EffectText.md`.
  - Stats keep this game's stat names ("Spell Damage", not the deckbuilder's "Refraction Damage"); spells are "Refractions" in sentences ("When you cast a Refraction"), as the deckbuilder names them; a Generic element reads "Elemental" (it takes the caster's element); a Double target reads "2 Enemies".
- **B** write a custom sentence per item.
**Recommendation:** A. Generated text that cannot drift is D-031's point; Slay the Spire and Hearthstone template every card from its data the same way, and players read the shared verbs at a glance. Mirroring the AND/OR evaluation keeps the text truthful. Not lore. New player-facing words in it ("Team Member", "the attacked Enemy", "Passive", "Activate") are the deckbuilder's.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change any word: they are `LOCTEXT` entries in `Private/Skills/Effects/EffectText.cpp`.
**Notes for Claude:** applied (feature/B-045-effect-text). `docs/Architecture/EffectText.md`.

### Q-042 — Build GAS stage 3 now, or after the B-145 / B-146 plans?
**Status:** ANSWERED · **Raised:** 2026-10-27 (B-042) · **Blocks:** B-042 (until B-145 and B-146 are approved)
**Question:** B-042 (GAS stage 3: effects on GameplayEffects, the getters reading attributes) became pickable when B-134 landed. B-145 (resistance) and B-146 (all stat maths) are plan jobs whose results are to be folded into B-041 to B-043: one Execution Calculation for damage and buildup, every bonus a modifier, constants in data tables. Build stage 3 first, or wait for those plans?
**Context:** `docs/Design/GASMigration.md` stage 3 has six steps and touches the whole combat core (its Risks section asks for a feature freeze). B-145 and B-146 say "no code until Crown approves", and they decide how the GameplayEffects and the damage calculation that B-042 builds are shaped.
**Options:**
- **A** write the B-145 / B-146 plans first; B-042 waits until Crown approves them, then builds the agreed shape once.
- **B** build B-042 now to the current GASMigration plan; B-145 / B-146 later rework it.
**Recommendation:** A. Plan the data model before the large port: Epic's Lyra and the GASDocumentation community guide both settle the attribute sets, the modifiers and the one damage Execution Calculation before moving effects onto GameplayEffects, because reshaping effects after they are ported is the costly part. B would build stage 3 twice. It also keeps the feature freeze short (one pass through the combat core). Not lore.
**Answer (Crown):** **A, approved by the reference-backed default** (2026-10-27; Claude's recommendation, reference above). Crown may change it.
**Notes for Claude:** B-042 is `BLOCKED (B-145, B-146; Q-042)`. Unblock it when Crown approves both plans; the plan docs list what changes in B-042.

## Parked

### D-P01 — Move Broken Darkness onto GAS?
**Status:** PARKED until after GAS stage 3 (`GASMigration.md`, decision 1: option C). Only with a zero-difference `wor.CompareBD`.

---

## Decided

Moved here once applied. Newest first.

| ID | Decision | Date | Applied in |
|---|---|---|---|
| D-075 | A fight that ends mid-action: the unfinished action pays no infusion HP cost; its defense windows are dropped | 2026-10-26 | B-030 (`UActionExecutor::ResetForCombat`; TODO removed in `chore/apply-D-073-D-075-D-076`) |
| D-040 | A Refractor's innate spells always work, whatever it wears (already the code) | 2026-10-26 | `chore/apply-D-034-041` (Rules 01 CL-C2) |
| D-034 | Crit damage ceiling with gear is x2.5 (the code); docs fixed | 2026-10-26 | `chore/apply-D-034-041` (docs); comments in B-147 |
| D-064 | No new per-fight state on GameInstance subsystems: battle actors / GameState / PlayerState | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md standing rules) |
| D-057 | Heavy visuals/audio: new data-asset fields use soft refs; existing hard refs stay | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md standing rules) |
| D-058 | Content naming: new assets only, plus the junk/duplicate cleanup | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md; B-123, B-122 TODO) |
| D-059 | Saves: explicit save structs, not SaveGame tags | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md; B-090 note; B-124 re-worded, after B-090) |
| D-060 | Old dev saves wiped with a warning below the minimum version until the first public build | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md; B-090 note) |
| D-061 | Localisation-ready now (all player text FText); translations at the first public build, human translators or disclose AI | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md; B-127 TODO; B-105/B-045 notes) |
| D-062 | Shared game words in C++ LOCTEXT name tables | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md; B-126 TODO) |
| D-063 | Fixed keys for now; unique input action names | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md standing rules) |
| D-065 | Co-op scheduled after the single-player loop (B-090, B-105) | 2026-10-26 | `chore/apply-D-057-068` (B-131 blocked on it) |
| D-066 | Functional level tests after B-105 | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md standing rules) |
| D-067 | Retire each legacy *TestActor after a matching spec and Crown's PIE pass | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md standing rules) |
| D-068 | Frame budget: 60 fps at 1080p on Crown's PC, checked once per milestone | 2026-10-26 | `chore/apply-D-057-068` (CLAUDE.md standing rules) |
| D-023 | Stunned = plain basic attack or Defend. No infused or charged attack, no Flee, no Switch Weapon | 2026-10-26 | `fix/B-092-stunned-menu-defend` (gate + menu) |
| D-022 | Only `BP_CombatGameMode` was really deleted, so it was restored. The other 15 widgets were moved, not deleted, and stay where they are; they get replaced in the CommonUI rebuild (D-024). Crown confirmed | 2026-10-26 | `chore/D-022-restore-combat-gamemode` |
| — | CommonUI: yes | 2026-10-25 | `ArchitectureReview_2026-10.md` §3 |
| — | GAS: adopt, 4 stages; BD option C; real-time field abilities yes; ASC on the character | 2026-10-25 | `Design/GASMigration.md` |
| — | All gameplay logic in C++; BP for layout/visuals only | 2026-10-25 | `analysis/CodebaseReview_2026-10.md` §8 |
| — | UI: Expedition 33 combat layout; deckbuilder art direction + layouts; gamepad yes; flat panels for now | 2026-10-25 | `Design/UI_ScreenInventory.md`, `Design/UI_DeckbuilderReuse.md` |
| — | Source art: save what we use (LFS) | 2026-10-25 | `.gitattributes`, `UI_DeckbuilderReuse.md` §4 |
