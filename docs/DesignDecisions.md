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
**Answer (Crown):** (2026-10-26) Pick from pictures: B-101 draws all four candidates (plus a crystal-themed one, see D-024) in the combat HUD mockup, and Crown chooses there.

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
**Applied (B-147):** the live check (`LoadoutRules.cpp`) already capped by tier; a test now pins it, and `EvolutionCrystals.md` is fixed. The flat 6 remains only in the legacy `FSavedLoadout::GetValidationErrorsLegacy`, which B-172 retires. An evolution *attached* to a weapon or ring still uses the flat ceiling: **Q-147**.

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

### Q-031 — What is "the ApplyDamage fallback" B-031 should delete?
**Status:** OPEN · **Raised:** 2026-10-26 (B-031) · **Blocks:** B-167
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
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

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
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-159) · **Blocks:** — (B-159 shipped without returning them)
**Question:** when a loadout's item slots are trimmed (over a tier cap, a quantity above 3, a duplicate crystal type), do the removed crystals return to the character's crystal inventory?
**Context:** today they just vanish from the slot (`ULoadoutComponent::DropInvalidParts`, the existing duplicate-type rule did the same). Crystals a fight's auto-equip put in a slot were taken out of the inventory (`FCombatLoadout::ApplyAutoEquip`), so dropping them loses them. Crystals in an authored loadout were never taken out of the inventory.
**Options:**
- **A** always return them to the inventory.
- **B** never (today's behaviour).
- **C** return only crystals that came out of the inventory (needs the slot to remember where its crystals came from).
**Recommendation:** A, because losing crystals to a rule the player didn't break on purpose feels like a bug, and authored loadouts are a dev-time case where an extra crystal in the inventory is harmless.
**Answer (Crown):** **A** (2026-10-26): utility crystals **removed by a rule** (over a cap, invalid loadout, duplicate type) are not destroyed, so they **return to the inventory, or storage if it's full** (D-097). Separately: crystals **destroyed** through use or wear are **lost**, as today, **except evolutions**, which count as equipment and are never lost that way.
**Notes for Claude:** _optional_

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
**Status:** OPEN · **Raised:** 2026-10-26 (Q-163 follow-up) · **Blocks:** — (Q-163 shipped with B)
**Question:** Once a ring's crystal breaks, may its F-tier spells still be infused another way, and what element does a Generic (inherit) spell from it take?
**Context:** a broken ring offers no spells (`FRingInventoryEntry::GetSpells`), so `ResolveSpellSource` no longer finds its F-tier spells on the ring and treats them as **Innate**. For a Resonator or a Generic that means no infusion at all (they have no Innate source). For a **Caster** with a ring in the primary slot it means the spell can be infused through the Caster's **Innate** element. Separately, `GetElementForSourceOption` still returns the broken ring's element for a Generic spell that inherits its element.
**Options:**
- **A** keep it: a Caster may infuse a broken ring's F-tier spell with their own element; a Generic spell keeps the ring's element.
- **B** a broken ring's spells can't be infused at all (any source), and a Generic spell from it is non-elemental (None).
**Recommendation:** B. A broken crystal gives no element (Q-163), so its spells cast plain; it also keeps the spell's origin honest (it still comes from the ring).
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

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
**Status:** OPEN · **Raised:** 2026-10-26 (B-180) · **Blocks:** — (B-180 shipped with A)
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
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-168 — Guard: the open details
**Status:** OPEN · **Raised:** 2026-10-26 (B-168) · **Blocks:** — (B-168 shipped with A)
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
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-169 — Luck dodge: the open details
**Status:** OPEN · **Raised:** 2026-10-26 (B-169) · **Blocks:** — (B-169 shipped with A)
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
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-147 — Does an evolution attached to a weapon or ring hold spells by its tier too?
**Status:** OPEN · **Raised:** 2026-10-26 (B-147) · **Blocks:** — (B-147 shipped without it)
**Question:** D-039 says crystals and evolutions hold spells by their tier. An evolution in the **primary slot** does (F 1 … S 6). An evolution **attached to a weapon or ring** still gets the holder's flat ceiling (6 for a weapon, the ring's cap for a ring). Should the attached one use its tier too?
**Context:** `CrystalEffectTable::ResolveSpellSlotCap` gives gem crystals their tier count and every other attachment (evolutions included) the caller's flat ceiling (`CrystalEffectTable.h`). D-039's answer did not separate attached from primary evolutions.
**Options:**
- **A** keep it: an attached evolution uses the holder's flat ceiling (as now).
- **B** by tier: an attached evolution holds its tier's count, like a primary one and a gem.
**Recommendation:** B, because it is the plain reading of D-039 and makes every container follow one curve. The deckbuilder has no separate rule.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### D-097 — Storage (items), inventory (items you carry) and the pool (gear)
**Status:** ANSWERED (corrected 2026-10-26; details open) · **Raised:** 2026-10-26 (from Q-177) · **Blocks:** —
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
**Notes for Claude, open details to confirm with Crown:**
- Found gear and items are **run-only** (D-002, lost at run end) unless Crown says otherwise; during a run, "storage" for found items is then a run stash that empties at run end. Confirm when building.
- How the inventory limit grows (account perks, like D-021's slot growth?).
- Today: 20 crystals per tier shared by gem types, 20 for stones (Q-177 context); these caps go away for storage.

### Q-044 — Re-applying the same effect: strength rule or replace?
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Re-applying the same effect: strength rule or replace? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today a stronger re-apply replaces the value and refreshes, an equal one refreshes, a weaker one is ignored (`SkillEffectManager.cpp:146-218`, ET-39).
**Options:**
- **A** replace, as the deckbuilder (`04-effects.md`, `Stat` duration row).
- **B** keep the strength rule.
**Recommendation:** A. It matches D-012 / D-053 and needs no strength comparison (GAS has none). Defend and on-hit Burn still refresh, because they re-apply the same value.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044b — What a stack is
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** What a stack is? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today a stack is value x stacks, keeping the strongest per-stack value, and a weaker stack does not refresh. In the deckbuilder a stack is the held magnitude once more, every arrival refreshes, and the default cap is 1.
**Options:**
- **A** deckbuilder stacks.
- **B** keep ours.
**Recommendation:** A. One simple rule: what a stack adds is exactly what its expiry takes off (`04-effects.md` `MaxStacks`).
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044c — When damage and heals over time tick
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** When damage and heals over time tick? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today damage over time ticks at turn end and heals at turn start.
**Options:**
- **A** all payouts at turn start.
- **B** keep today's split.
**Recommendation:** A. One moment fits D-051's single counting rule and gives exactly N ticks (`04-effects.md` "Lasts"). A DoT that kills at turn start ends that turn (ET-17 already handles it).
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044d — Execution turns and turn-start effects
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Execution turns and turn-start effects? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today Execution turns skip turn-start effects (`CombatOrchestrator.cpp:882`, ET-16); D-055 says they count. The deckbuilder has no Execution turns.
**Options:**
- **A** an Execution turn runs the turn-start step (expiry and ticks) but still gives no choice.
- **B** it counts down durations but skips ticks.
**Recommendation:** A. B would give fewer than N ticks and break D-051.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044e — Where Barrier, Shield and Ward sit in a hit (D-092)
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Where Barrier, Shield and Ward sit in a hit (D-092)? See `docs/analysis/EffectsOverhaul.md`.
**Context:** The deckbuilder applies absorbers before Defense and Guard. Our Defense and Guard are percentages, so the order changes the numbers. D-092's note proposed Guard first.
**Options:**
- **A** deckbuilder order (absorbers first).
- **B** absorbers last, on the damage left after Guard and Defense (where Absorb and Reflect sit today).
**Recommendation:** B, a deliberate difference from the deckbuilder: "negate the next 5 damage" means exactly 5 HP you would have lost, a Perfect Guard keeps a shield longer, and it is the smallest change. Inside the absorbers keep the deckbuilder order (Ward, Shield, Barrier; oldest first).
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044f — Multi-hit skills and absorb charges
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Multi-hit skills and absorb charges? See `docs/analysis/EffectsOverhaul.md`.
**Context:** A skill can land several impacts.
**Options:**
- **A** each impact is one damage instance and spends one charge.
- **B** one charge per skill.
**Recommendation:** A, the deckbuilder's rule (charges spend on every damage instance). Multi-hit skills become good shield breakers.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044g — "Doesn't stack" (D-053) on existing gear
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** "Doesn't stack" (D-053) on existing gear? See `docs/analysis/EffectsOverhaul.md`.
**Context:** D-053 adds a "doesn't stack" flag to effect definitions.
**Options:**
- **A** default off; Claude lists every Last Stand / Extra Action / fires-once definition for Crown to tick.
- **B** default on for those types in code.
**Recommendation:** A. Data decides (the deckbuilder's "authoritative data" rule) and Crown sees the list once.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044h — Conditions: "or", and % versus absolute HP
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Conditions: "or", and % versus absolute HP? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Our conditions allow Or and use % HP; the deckbuilder's nested conditions must all hold and use absolute numbers.
**Options:**
- Or: **A** retire Or for new data (a validator reports it; author two effects) / **B** keep Or and print "or".
- Numbers: **C** keep % / **D** absolute, as the deckbuilder.
**Recommendation:** A and C. A keeps D-031's tree honest; C differs on purpose because our HP grows with level and gear.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044i — Which effects a limited cleanse takes
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Which effects a limited cleanse takes? See `docs/analysis/EffectsOverhaul.md`.
**Context:** Today Iolite's "up to N" has no stated order (`ItemExecutor.cpp:1014-1045`).
**Options:**
- **A** newest first, a stack whole, gear exempt (D-015).
- **B** oldest first.
**Recommendation:** A, the deckbuilder's rule (`04-effects.md` "The uses verbs and Remove effect"), with no picking UI for now.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044j — Triggers: our dispatcher or GAS abilities?
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Triggers: our dispatcher or GAS abilities? See `docs/analysis/EffectsOverhaul.md`.
**Context:** GAS abilities are stage 4 (B-043).
**Options:**
- **A** keep our dispatcher in B-042 and send GameplayEvents so B-043 can move triggers later.
- **B** move triggers to passive abilities in B-042.
**Recommendation:** A. Doing it in stage 3 doubles its size and risk.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

### Q-044k — Land B-062 (identity rules) before GAS, or inside it?
**Status:** OPEN · **Raised:** 2026-10-26 (B-044, effects overhaul review) · **Blocks:** — (shapes B-062, B-042)
**Question:** Land B-062 (identity rules) before GAS, or inside it? See `docs/analysis/EffectsOverhaul.md`.
**Context:** B-062 builds D-011 / D-012 / D-051 / D-053 / D-055 on today's code.
**Options:**
- **A** build B-062 now, with the shared turn-count helper; B-042 then ports the same tests.
- **B** fold B-062 into B-042.
**Recommendation:** A. Players get the rules sooner and B-042 becomes a pure port. Absorb stays in B-042, as D-092 says.
**Answer (Crown):** _pending_
**Notes for Claude:** _optional_

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
