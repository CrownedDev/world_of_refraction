# Open Design Decisions

The queue of questions only Crown can answer.

**How it's used**
- **Claude Code (local, working the backlog):**
  - Never decides an `OPEN` item.
  - When a backlog job needs a decision, it adds or links an entry here, marks the job `BLOCKED (D-xxx)` in `docs/Backlog.md`, and **moves on to the next unblocked job**. No waiting.
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

**Entry format** (keep each short; one decision per entry):
```
### D-NNN — Title
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
- Deckbuilder reference (`wor-deckbuilder/docs/reference/01-card-model.md` "Cast through", Pivot "Generic element memory"): a Generic Ability starts Physical and takes its source's element; a Generic Refraction needs an elemental source. Its extra costs (1 Action to switch, +1 Energy when a physical ability deals an element) are **not** ruled here; ask before adding them.
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
- **Water:** lowers healing for two turns (lowers, not blocks).
- **Ice:** slows action speed for two turns (D-029).
**Notes for Claude:**
- Turn counting per the deckbuilder (`04-effects.md` "For N turns counts the holder's turns"): it counts the holder's turns; applied during the holder's own turn, that turn counts as turn 1; it ends when the holder's turn after those starts. Put this in one shared helper (it's also what GAS stage 3's turn tracker needs, B-042/B-044).
- Duration 2 is a named constant (`STATUS_CAP_TURNS = 2`); sizes ("how big") are named constants, `// TODO: tune`.
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
**Applied:** docs fixed to x2.5 (`DamageCalculator.md`, `Luck.md`, Rules 02 ED-19/CED-2, `chore/apply-D-034-041`); the `CombatConstants.h:184` comment is B-147.

### D-035 — Should infusing an ability cost extra on top of the charge level?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** An infused ability pays the flat x1.5 `INFUSION_ENERGY_MULTIPLIER` and then the charge multiplier (L1 x1.5, L2 x2.0): about x2.25 / x3.0 EP. A spell pays only x1.5 / x2.0. Keep the double surcharge?
**Context:** `InfusionConstants` comments and `Infusion.md` say abilities and spells share the charge; two comments give wrong numbers ("30%/60%", "1.15x/1.30x"). Rules: 02 CED-1.
**Options:** A keep both (abilities cost more) / B charge multiplier only (same as spells).
**Recommendation:** A, because the deckbuilder also makes a physical ability pay +1 Energy to deal an element (`wor-deckbuilder/docs/reference/01-card-model.md:93`, "Cast through"). Fix the comments either way.
**Answer (Crown):** **A** (2026-10-26): keep the extra x1.5 for infusing an ability on top of the charge level (deckbuilder: a physical ability pays more to deal an element). Fix the two wrong comments.
**Queued:** the two wrong comments are B-147; Rules 02 ED-30/CED-1 updated.

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
**Answer (Crown):** **B, reshaped** (2026-10-26): Silenced affects **every Energy spend** (spells, abilities, infusion), but instead of a full block it **seals part of your Energy**: **50% of max EP is locked for 2 of the holder's turns** (D-030); you can only spend the rest. Basic attack, Defend and items still work.
**Notes for Claude:**
- Sealed share is a named constant (`SILENCE_SEALED_EP_FRACTION = 0.5f`, `// TODO: tune`). Spendable EP = CurrentEP − sealed amount (floored at 0); validation and the menu read spendable EP.
- UI: the sealed part of the EP bar is drawn dark (fits the B-111 bar work).
- Replaces the spells-only block at `ActionExecutor.cpp:176-180`; the display name "No EP Spend" changes to match (naming: ask Crown).
- Broken Darkness: its EP is the same field, so the seal applies to it too unless Crown says otherwise.
**Queued:** B-148 (the seal). Its display name is D-070.

### D-039 — How many spells does an evolution hold?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Set by the evolution's tier (1–6), or a flat number?
**Context:** Combat caps it by tier (`LoadoutComponent.cpp:575-581`); the saved-loadout check uses a flat 6 (`FSavedLoadout.cpp:190-195`); `EvolutionCrystals.md` says about 4. Rules: 01 CL-C1.
**Options:** A by tier, 1–6 / B flat 6 / C flat 4.
**Recommendation:** A, the live rule; then make the saved-loadout check match.
**Answer (Crown):** **A, by tier, as already designed** (2026-10-26): crystals and evolutions hold spells by their tier (1–6, the shared `SlotsForContainerTier` curve). Not a new decision: fix the saved-loadout check's flat 6 (`FSavedLoadout.cpp:190-195`) and `EvolutionCrystals.md`'s "about 4" to match.
**Queued:** B-147 (saved-loadout check and `EvolutionCrystals.md`).

### D-040 — Does a Refractor's innate always work?
**Status:** DECIDED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Here a Refractor's innate spells always work. In the deckbuilder they work only while a gear slot is empty. Should wearing gear switch them off?
**Context:** Rules: 01 CL-C2; `wor-deckbuilder/docs/reference/01-card-model.md` (Refractor passive). Raised only because the two games differ.
**Options:** A always on (current) / B only while the gear slot is empty (deckbuilder).
**Recommendation:** A, because a Refractor here has only one gear slot, so B would make gear and innate an either-or.
**Answer (Crown):** **A, always on** (2026-10-26): a Refractor's innate spells always work, whatever is in the primary slot. Already the code (`FCombatCapabilities.cpp:80-85` offers `InnateSpells` for every Caster, checked 2026-10-26); nothing to build. The deckbuilder's empty-slot rule doesn't apply here.
**Applied:** nothing to build; Rules 01 CL-C2 updated (`chore/apply-D-034-041`).

### D-041 — Are skill requirements a gate or only a scale?
**Status:** ANSWERED · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Can a character use a skill it is under-levelled for?
**Context:** The code never blocks; the requirement gap only scales the skill down. A commented-out refusal sits at `ActionExecutor.cpp:233-236`, and `Stats.md` says pillar levels "gate skill tiers". Rules: 01 CL-C3.
**Options:** A usable, just weaker (current) / B refused below the requirement.
**Recommendation:** A, the current behaviour; then delete the commented-out gate and fix `Stats.md`.
**Answer (Crown):** **A, usable but weaker** (2026-10-26): the shipped requirement-gap scaling stays (`Mechanics/Scaling/RequirementGap.md`); overreaching also feeds Broken Darkness strain. Delete the commented-out refusal (`ActionExecutor.cpp:233-236`) and fix `Stats.md`'s "gates skill tiers". Crown: like everything else, it could be **done better**, so it's in the standard-way redesign (B-146).
**Queued:** B-147 (delete the commented-out refusal, fix `Stats.md`); the redesign is B-146.

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
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `MAX_TURN_RATIO = 2` and `TURN_SPEED_THRESHOLD_DOUBLE` exist (`CombatConstants.h:331`), but the scheduler never reads them, so a fast character can get any number of turns per round.
**Context:** `Combat/CombatConstants.h:330-331` (unread constants); `Combat/TurnManager.cpp:265-270` (uncapped ratio). Rules: 04 C4-1.
**Options:** A cap at 2:1 / B no cap (current).
**Recommendation:** A, because the constants show that was the design, and an uncapped ratio lets stacked speed lock enemies out.
**Answer (Crown):** _write here_

### D-050 — How long does Defend last?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Defend's +50% Defense for 1 turn ticks away at the end of the defender's own turn, before anyone attacks, so it does nothing.
**Context:** `Combat/Actions/ActionExecutor.cpp:2787-2792` gives it; the end-of-turn tick (`Combat/CombatOrchestrator.cpp:589`, `Skills/Effects/SkillEffectManager.cpp:1125-1145`) removes it. Rules: 04 C4-2. In the deckbuilder, Defend lasts until your next turn starts (`04-effects.md`, "Lasts").
**Options:** A until your next turn starts / B as now.
**Recommendation:** A (the deckbuilder's rule), with the 50% and the duration as named constants.
**Answer (Crown):** _write here_

### D-051 — How are effect durations counted?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-062, B-063
**Question:** The code ends an effect at the end of the holder's Nth turn, and a new effect acts the moment it lands, so a 2-turn DoT ticks 3 times. D-030 counts the landing turn as turn 1 and ends the effect as the next turn starts. Should D-030's counting apply to every timed effect, with exactly N ticks?
**Context:** `Skills/Effects/SkillEffectManager.cpp:1099-1153` (expiry), `:232-237, 974-991` (DoT ticks). Rules: 04 C4-3, C4-4. The deckbuilder gives exactly N ticks (`04-effects.md`, "Lasts").
**Options:** A D-030 counting everywhere, exactly N ticks / B D-030 for bar caps only.
**Recommendation:** A, one counting rule for everything, matching the deckbuilder.
**Answer (Crown):** _write here_

### D-052 — Is Emerald's bonus turn extra, or the same turn moved earlier?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** Emerald's turn counts as a turn taken with no credit, so it costs a later normal turn. Extra Action adds a real turn.
**Context:** `Combat/TurnManager.cpp:566-575` (pinned turn, no credit), `:305` (Extra Action). The deckbuilder's `AddTurn` adds a turn (`04-effects.md`, verb 7). Rules: 04 C4-5.
**Options:** A a real extra turn / B a turn moved earlier (current), named to match.
**Recommendation:** A, because players read "bonus turn" as an extra one, and it matches the deckbuilder.
**Answer (Crown):** _write here_

### D-053 — Does the same effect from two gear pieces add up?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-062
**Question:** Here two gear pieces giving the same effect merge into one (deliberate, per `SkillEffects.md`). The deckbuilder adds one per source, which is also D-012's idea (a different source adds).
**Context:** `Skills/Effects/SkillEffectManager.cpp:319-330`; the deckbuilder's `Stat` duration row (`04-effects.md`). Rules: 04 C4-7.
**Options:** A add up, one per source item / B merge (current).
**Recommendation:** A, so D-012's rule covers gear too.
**Answer (Crown):** _write here_

### D-054 — Is "Turn Speed Up" a buff?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `ModifyTurnSpeed` is displayed "Turn Speed Up" and adds speed (`Combat/TurnManager.cpp:228-231`), but `IsDebuff` lists it (`Skills/Effects/ESkillEffectType.h:390`), so cleansing an ally strips it and Iolite can't strip it from an enemy. Is it a buff?
**Context:** Rules: 04 C4-8.
**Options:** A a buff / B sign-aware, like Status Resist Up / C a debuff (current).
**Recommendation:** A, matching its name and what it does.
**Answer (Crown):** _write here_

### D-055 — Do bonus and Execution turns count toward effect durations?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** B-062, B-063
**Question:** D-030 counts "the holder's own turns". Does an Emerald bonus turn or an Execution turn count as one of them?
**Context:** Pinned turns are taken from `Combat/TurnManager.cpp:566-575`; the effect tick runs on every turn end (`Skills/Effects/SkillEffectManager.cpp:1099-1153`). Rules: 04 Gaps.
**Options:** A every turn the holder takes counts / B only normal turns count.
**Recommendation:** A, the simplest to read ("2 of your turns" means any 2 turns you take), and it needs no special case.
**Answer (Crown):** _write here_

### D-056 — Do the generic Speed Buff/Debuff move turn order?
**Status:** OPEN · **Raised:** 2026-10-26 (B-046) · **Blocks:** —
**Question:** `docs/Mechanics/Combat/TurnOrder.md` says Slow (`SpeedDebuff`) pushes turns back, but the scheduler reads only Turn Speed Up/Buff/Debuff (`Combat/TurnManager.cpp:228-231`); `SpeedBuff`/`SpeedDebuff` trigger a recalculation that changes nothing (`Skills/Effects/SkillEffectManager.cpp:2374-2375`).
**Context:** Rules: 04 C4-9.
**Options:** A yes, like Turn Speed / B no, fix the doc.
**Recommendation:** B, keeping one speed stat for turn order; D-029 already gives Ice its own turn-speed slow.
**Answer (Crown):** _write here_

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
**Status:** OPEN · **Raised:** 2026-10-26 (applying D-038) · **Blocks:** — (B-148 keeps the old name until answered)
**Question:** Silenced now seals half your Energy instead of blocking spells. What should the player see it called?
**Context:** The enum's display name is "Silenced (No EP Spend)" (`ESkillEffectType.h:129`), which no longer fits. D-038's notes say to ask Crown.
**Options:** A "Silenced" (keep the word, new tooltip: half your Energy is sealed) / B "Sealed" / C another name.
**Recommendation:** A, because the Darkness cap and the docs already say Silenced; only the tooltip changes.
**Answer (Crown):** _write here_


### D-071 — Revive spells: what do they do?
**Status:** OPEN · **Raised:** 2026-10-26 (applying D-043) · **Blocks:** — (B-150 builds the rest-spot revive without it)
**Question:** D-043 says spells can revive a fallen ally mid-fight, but there is no revive spell effect. How should one work?
**Context:** The old `Revive` skill effect was renamed `LastStand` (survive a killing blow, `ESkillEffectType.h:200-201`). Only Sapphire's `DefyDeath` item effect revives a dead ally (`ItemEffectType.h:86-90`).
**Options:** A a new `Resurrect` skill effect: brings a dead ally back at the effect's value as % of max HP, reusing Sapphire's revive path / B revive stays item-only (Sapphire) / C revive spells only out of combat.
**Recommendation:** A, because D-043 asks for revive spells and Sapphire's path already does the hard part (turn order, death state); the HP % is a per-spell value.
**Answer (Crown):** _write here_
## Parked

### D-P01 — Move Broken Darkness onto GAS?
**Status:** PARKED until after GAS stage 3 (`GASMigration.md`, decision 1: option C). Only with a zero-difference `wor.CompareBD`.

---

## Decided

Moved here once applied. Newest first.

| ID | Decision | Date | Applied in |
|---|---|---|---|
| D-040 | A Refractor's innate spells always work, whatever it wears (already the code) | 2026-10-26 | `chore/apply-D-034-041` (Rules 01 CL-C2) |
| D-034 | Crit damage ceiling with gear is x2.5 (the code); docs fixed | 2026-10-26 | `chore/apply-D-034-041` (docs; comment in B-147) |
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
