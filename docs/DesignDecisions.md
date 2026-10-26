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
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-080
**Question:** Is it intended that Generic can cast spells from a crystal on its weapon?
**Context:** The menu shows them to any class (`FCombatCapabilities.cpp:62-68`), but `UCharacterData::CanUseSpells()` is false for Generic.
**Options:**
- A: no, block it.
- B: yes, it's Generic's only spell access.
**Recommendation:** Crown's call; it defines Generic's identity.
**Answer (Crown):** _write here_

### D-008 — Do gear effects update when swapping mid-combat?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-081
**Question:** When a weapon is toggled or a ring switched mid-fight, does the new gear's effect apply?
**Context:** Effects are armed once at combat start (`CombatOrchestrator.cpp:1163-1170`). After a swap, the old effects stay and the new ones never apply.
**Options:**
- A: re-gather on swap (old effects off, new on).
- B: locked at combat start.
**Recommendation:** A. Otherwise the Resonator ring switch is cosmetic.
**Answer (Crown):** _write here_

### D-009 — Do stones and fusions on rings work?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-082
**Question:** Should stones on rings have any effect?
**Context:** Fusions are allowed on rings (`RingData.cpp:40-44`), but every stone read goes through the active weapon only. Ring stones are inert.
**Options:**
- A: rings' stones count.
- B: disallow stones on rings.
**Recommendation:** A, for Resonators.
**Answer (Crown):** _write here_

### D-010 — Resonator ring rules
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-083
**Question:** "Up to 3 of the same element": enforce it? And is the ring cap 5 or 6?
**Context:** Only a budget of 5 is enforced (`LoadoutConstants.h:31`). The comment says 6 (`ECharacterClass.h:24`).
**Answer (Crown):** _write here_

### D-011 — Once-per-match effects: per character or per match?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-062
**Question:** If two enemies wear the same "once per match" gear, does each get its own use?
**Context:** It's tracked per effect ID (`SkillEffectManager.h:423`), so the second wearer is blocked.
**Recommendation:** Per character.
**Answer (Crown):** _write here_

### D-012 — Repeated status caps: refresh or stack?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-062
**Question:** When a bar caps again (Burn twice), refresh the existing effect or add a second?
**Context:** Random IDs make it stack today (`SkillEffectManager.cpp:2405`).
**Recommendation:** Refresh.
**Answer (Crown):** _write here_

### D-013 — Do status bars carry over between fights?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** —
**Context:** Arc A resets them at every combat end.
**Recommendation:** Reset (current after Arc A).
**Answer (Crown):** _write here_

### D-014 — What does Wind's "skip turn" do?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-063
**Question:** Wind's bar cap stores SkipTurn but nothing enforces it. Is it the same as stun?
**Recommendation:** Pass the next turn (like stun), distinct name and icon.
**Answer (Crown):** _write here_

### D-015 — Should cleanse remove your own gear effects?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-065
**Context:** On-hit gear effects count as debuffs, so `CleanseSelf` strips them. Iolite also strips an enemy's permanent gear buffs, and nothing re-applies them (`ESkillEffectType.h:391-393`, `ItemExecutor.cpp:1029-1041`).
**Recommendation:** Cleanse removes only effects applied by others; gear effects are immune.
**Answer (Crown):** _write here_

### D-016 — Can equipped items be dismantled?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-084
**Recommendation:** No. Block it with a message.
**Answer (Crown):** _write here_

### D-017 — Header bar style (UI)
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** UI work (later)
**Question:** Which of the four deckbuilder candidates (`UI_DeckbuilderReuse.md` §3)?
**Answer (Crown):** _write here_

### D-018 — Art pack licences for Unreal
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** asset import (later)
**Question:** Confirm each pack in the deckbuilder's `CREDITS.md` allows Unreal / commercial use.
**Answer (Crown):** _write here_

### D-019 — Where does the mockup generator live?
**Status:** ANSWERED · **Raised:** 2026-10-25 · **Blocks:** —
**Question:** Copy `tools/ui_drafts/` into this repo, or keep it in wor-deckbuilder?
**Answer (Crown):** **Answered by D-025 = A** (2026-10-26): the tool is copied into this repo (`Tools/ui_drafts/`).
**Notes for Claude:** Nothing to apply beyond B-100.

### D-020 — Is the account pool shared across characters?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-091
**Question:** Is the owned pool shared by all characters, or per character?
**Context:** `InitializeFromPool` copies the whole pool into one pawn (`InventoryComponent.cpp:1446-1503`). With 3 party members, each would get duplicates.
**Answer (Crown):** _write here_

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

---

## Parked

### D-P01 — Move Broken Darkness onto GAS?
**Status:** PARKED until after GAS stage 3 (`GASMigration.md`, decision 1: option C). Only with a zero-difference `wor.CompareBD`.

---

## Decided

Moved here once applied. Newest first.

| ID | Decision | Date | Applied in |
|---|---|---|---|
| D-023 | Stunned = plain basic attack or Defend. No infused or charged attack, no Flee, no Switch Weapon | 2026-10-26 | `fix/B-092-stunned-menu-defend` (gate + menu) |
| D-022 | Only `BP_CombatGameMode` was really deleted, so it was restored. The other 15 widgets were moved, not deleted, and stay where they are; they get replaced in the CommonUI rebuild (D-024). Crown confirmed | 2026-10-26 | `chore/D-022-restore-combat-gamemode` |
| — | CommonUI: yes | 2026-10-25 | `ArchitectureReview_2026-10.md` §3 |
| — | GAS: adopt, 4 stages; BD option C; real-time field abilities yes; ASC on the character | 2026-10-25 | `Design/GASMigration.md` |
| — | All gameplay logic in C++; BP for layout/visuals only | 2026-10-25 | `analysis/CodebaseReview_2026-10.md` §8 |
| — | UI: Expedition 33 combat layout; deckbuilder art direction + layouts; gamepad yes; flat panels for now | 2026-10-25 | `Design/UI_ScreenInventory.md`, `Design/UI_DeckbuilderReuse.md` |
| — | Source art: save what we use (LFS) | 2026-10-25 | `.gitattributes`, `UI_DeckbuilderReuse.md` §4 |
