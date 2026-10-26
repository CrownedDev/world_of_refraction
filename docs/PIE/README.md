# PIE checks — what to play-test, and how

Automated tests (`Scripts/ue/test.sh`) prove the logic. This folder holds what still needs **eyes and hands in PIE** (look, feel, flow), so Crown can test at leisure instead of when the work lands.

## How it works
- **One file per job:** `B-xxx.md`, written by Claude Code at hand-off (Backlog mode step 5) — or by a cloud session when it hands work over.
- Each file lists numbered checks: **what to do**, **what you should see**, and a result box.
- Crown ticks results in the file (or tells Claude, who ticks them):
  - `[ ]` not tested yet · `[x]` passed · `[!]` failed — write what happened underneath.
- A job's PIE file being all `[x]` is the signal it's ready for `dev → main`.
- A `[!]` becomes a new backlog job (or reopens the old one) — Claude reads the notes.

## Pending
| Job | Branch | What it covers | Status |
|---|---|---|---|
| [B-001](B-001.md) | `fix/combat-hang-arc-a` (in `dev`) | Combat can no longer freeze; clean end of combat | Not tested |
| [B-007](B-007.md) | `feature/B-007-automation-tests` (in `dev`) | 26 headless tests + test.sh output; no gameplay change | Not tested |
| [B-006](B-006.md) | `chore/B-006-log-categories` (in `dev`) | Seven `LogWoR*` log categories; no gameplay change | Not tested |
| [B-061](B-061.md) | `fix/B-061-stun-attack-defend` (in `dev`) | Stunned = Attack or Defend only; no more skipped turn | Not tested |
| [B-002](B-002.md) | `refactor/B-002-merge-dev` (in `dev` via B-003) | `dev` merged into the native-UI refactor; HUD + results + vitals banking | Not tested |
| [B-008](B-008.md) | `chore/B-008-shipping-guards` (in `dev`) | Debug console commands + test actors compiled out of Shipping; no editor change | Not tested |
| [B-092](B-092.md) | `fix/B-092-stunned-menu-defend` (in `dev`) | Defend button; stunned menu = Attack + Defend, plain attack only (D-023 A) | Not tested |
| [B-094](B-094.md) | `fix/B-094-guard-rm-word` (in `dev`) | Guard blocks real `rm -r` only, not read-only commands; tooling, no game change | Not tested |
| [B-100](B-100.md) | `feature/B-100-mockup-tool` (in `dev`) | Mockup tool in `Tools/ui_drafts/` + one test draft to view on the phone | Not tested |
| [B-101](B-101.md) | `feature/B-101-combat-hud-mockup` (in `dev`) | Combat HUD drafts v1: command, target, hit, stunned. View on the phone; approval is B-102 | Not tested |
| [B-009](B-009.md) | `fix/B-009-defense-world-time` (in `dev`) | Defense press window on world time: survives a pause, matches the close timer | Not tested |
| [B-046](B-046.md) | `docs/B-046-rules-reference` (in `dev`) | Game rules reference in `docs/Rules/` (254 rules) + 23 new questions D-034..D-056. Read on the phone | Not tested |
| [B-047](B-047.md) | `docs/B-047-best-practice-audit` (in `dev`) | Best-practice audit vs Epic/Lyra; 25 jobs B-118..B-142, questions D-057..D-068. Read on the phone | Not tested |
| [B-010](B-010.md) | `feature/B-010-battle-random-stream` (in `dev`) | One random seed per battle, logged; `wor.Battle.Seed` replays a fight | Not tested |
| [B-020](B-020.md) | `refactor/B-020-stat-modifier-table` (in `dev`) | Stat buffs/debuffs read from one table (same numbers); `wor.CompareStats`, `wor.PrintStatModifiers` | Not tested |
| [B-021](B-021.md) | `refactor/B-021-stone-row-table` (in `dev`) | Stat stones read from one row table (same effects); `wor.PrintStoneRows` | Not tested |
| [B-022](B-022.md) | `refactor/B-022-loadout-getters` (in `dev`) | Loadout read without copying; class from CharacterData; no gameplay change | Not tested |
| [B-023](B-023.md) | `fix/B-023-ai-live-enums` (in `dev`) | AI sees Stun (not Wind's delay) and finds Cleanse Self spells | Not tested |
| [B-024](B-024.md) | `refactor/B-024-fold-weapon-ring-managers` (in `dev`) | Ring/weapon managers folded into the loadout; ring auto-switch on break; no gameplay change | Not tested |
| [B-025](B-025.md) | `refactor/B-025-economy-preamble` (in `dev`) | Economy actions share one owner/authority/component check; no gameplay change | Not tested |
| [B-030](B-030.md) | `fix/B-030-reset-for-combat` (in `dev`) | Nothing from one fight carries into the next (defense, charge, status bars, turn order, menu) | Not tested |
| [B-003](B-003.md) | `chore/B-003-bp-cpp-cutover` (in `dev`) | Combat HUD, panel effects list and turn slots run on C++ (BP overrides deleted); hub unchanged (D-076) | Not tested |
| [B-156](B-156.md) | `feature/B-156-combat-camera` (in `dev`) | Combat camera: turn view behind the acting character, pan-out on enemy turns, `wor.Camera` style / movement-off commands (no cinematics until B-157) | Not tested |
| [B-028](B-028.md) | `refactor/B-028-one-loadout-validator` (in `dev`) | One loadout rule set for the editor and the fight (D-073); `wor.Loadout.CompareRules` shows the difference | Not tested |
| [B-031](B-031.md) | `refactor/B-031-async-only` (in `dev`) | Rituals fire through the async path; old sync router obsolete | Not tested |
| [B-155](B-155.md) | `feature/B-155-utility-crystal-caps` (in `dev`) | Utility crystal tier caps reported in loadout checks | Not tested |
| [B-159](B-159.md) | `feature/B-159-refuse-or-drop` (in `dev`) | Invalid picks refused; invalid loadout parts dropped at load | Not tested |
| [B-160](B-160.md) | `feature/B-160-camera-cinematic-settings` (in `dev`) | Camera cinematic settings: cinematics on/off, enemy cinematics, skip after hit | Not tested |
| [B-161](B-161.md) | `feature/B-161-editor-discount-authored-stats` (in `dev`) | Editor loadout check uses the character's authored spell discount | Not tested |
| [B-163](B-163.md) | `feature/B-163-broken-sources` (in `dev`) | Broken-source spells greyed with the reason; broken ring keeps F-tier | Not tested |
| [B-166](B-166.md) | `fix/B-166-headless-skill-start` (in `dev`) | A montage-less skill no longer crashes the async start; ritual spec covers one | Not tested |
| [B-174](B-174.md) | `fix/B-174-support-no-buildup` (in `dev`) | Heals, buffs and support skills build no status | Not tested |

Move rows to **Done** when every check passes.

## Done
| Job | Tested | Notes |
|---|---|---|
| [B-026](B-026.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
| [B-027](B-027.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
| [B-029](B-029.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
