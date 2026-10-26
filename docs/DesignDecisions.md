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
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-061
**Question:** Does a stunned character skip the turn, or can it still Attack / Defend?
**Context:**
- `ActionExecutor::ValidateAction` blocks every action while stunned (`ActionExecutor.cpp:167`). The enum comment says only "Attack/Defend" are allowed (`ESkillEffectType.h:122`).
- Arc A makes stunned actors pass their turn automatically.
**Options:**
- A: skip the turn entirely (current, after Arc A).
- B: Attack and Defend allowed.
- C: Defend only.
**Recommendation:** A. Simplest, and readable for the player.
**Answer (Crown):** _write here_

### D-002 — What happens after winning vs losing a battle?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-072
**Question:** Where does each outcome send the player?
**Context:** Today both return to the trial, which reloads with the enemy respawned (`BattleGameMode.h:11-13`). The results screen now exists (branch B).
**Options:**
- A: Win → back to the trial with the enemy gone; Lose → back to the hub.
- B: Lose → retry the fight.
- C: Lose → back to the trial at the last checkpoint.
**Recommendation:** A for now. Needs the persistence work (B-090) for "enemy gone".
**Answer (Crown):** _write here_

### D-003 — How do recruited party members get into battle?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-071
**Question:** How do they get from the party list into a fight?
**Context:**
- The battle roster is built only from actors standing in the arena sphere (`EncounterComponent.cpp:182-197`). Recruits are never spawned into the trial.
- All pawns spawn as `DefaultPlayerPawnClass` (`BattleGameMode.cpp:133`).
**Options:**
- A: taken from the party list when the encounter starts, each spawned with its own `FPartyMember.PawnClass`.
- B: recruits follow the player around the trial and join via the sphere.
**Recommendation:** A. No follower AI is needed, and it works for co-op later.
**Answer (Crown):** _write here_

### D-004 — Are non-leader party members AI-controlled in battle?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-071
**Question:** Who acts for party members other than the leader?
**Context:** Only PC0's pawn is player-controlled; the others run on AI (`CombatOrchestrator.cpp:1198-1209`).
**Options:**
- A: the player controls every party member on their turn (Expedition 33 style).
- B: AI companions.
**Recommendation:** A, because it's the genre standard. AI control stays for co-op ghosts.
**Answer (Crown):** _write here_

### D-005 — Should elements change damage?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-064
**Question:** Weakness/resistance multipliers, or are elements status-buildup only?
**Context:** `ElementMultiplier = 1.0f` always (`DamageCalculator.cpp:207-208`).
**Options:**
- A: status-only (current).
- B: a weakness wheel.
- C: per-character resistances only.
**Recommendation:** C. A light touch that keeps the buildup system as the star.
**Answer (Crown):** _write here_

### D-006 — Where do Prisms come from?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-073
**Question:** What grants Prisms in C++?
**Context:** Nothing in C++ grants Prisms or Gold; the wallet starts at 0 (`CurrencyComponent.h:129-145`). The deckbuilder shows "+120 Prisms" on victory.
**Options:**
- A: battle rewards scaled by difficulty.
- B: plus trial completion bonuses.
- C: plus a starting grant.
**Recommendation:** A + C.
**Answer (Crown):** _write here_

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
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** —
**Question:** Copy `tools/ui_drafts/` into this repo, or keep it in wor-deckbuilder?
**Answer (Crown):** _write here_

### D-020 — Is the account pool shared across characters?
**Status:** OPEN · **Raised:** 2026-10-25 · **Blocks:** B-091
**Question:** Is the owned pool shared by all characters, or per character?
**Context:** `InitializeFromPool` copies the whole pool into one pawn (`InventoryComponent.cpp:1446-1503`). With 3 party members, each would get duplicates.
**Answer (Crown):** _write here_

---

## Parked

### D-P01 — Move Broken Darkness onto GAS?
**Status:** PARKED until after GAS stage 3 (`GASMigration.md`, decision 1: option C). Only with a zero-difference `wor.CompareBD`.

---

## Decided

Moved here once applied. Newest first.

| ID | Decision | Date | Applied in |
|---|---|---|---|
| — | CommonUI: yes | 2026-10-25 | `ArchitectureReview_2026-10.md` §3 |
| — | GAS: adopt, 4 stages; BD option C; real-time field abilities yes; ASC on the character | 2026-10-25 | `Design/GASMigration.md` |
| — | All gameplay logic in C++; BP for layout/visuals only | 2026-10-25 | `analysis/CodebaseReview_2026-10.md` §8 |
| — | UI: Expedition 33 combat layout; deckbuilder art direction + layouts; gamepad yes; flat panels for now | 2026-10-25 | `Design/UI_ScreenInventory.md`, `Design/UI_DeckbuilderReuse.md` |
| — | Source art: save what we use (LFS) | 2026-10-25 | `.gitattributes`, `UI_DeckbuilderReuse.md` §4 |
