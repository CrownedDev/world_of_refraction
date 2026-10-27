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
| [B-004](B-004.md) | `chore/B-004-enable-plugins` (in `dev`) | GameplayAbilities plugin enabled (CommonUI already on) | Not tested |
| [B-181](B-181.md) | `fix/B-181-ai-skips-greyed-spells` (in `dev`) | The AI skips spells greyed by a broken source | Not tested |
| [B-182](B-182.md) | `fix/B-182-perfect-guard-parry-anim` (in `dev`) | A Perfect Guard plays the parry animation | Not tested |
| [B-040](B-040.md) | `feature/B-040-gas-tags` (in `dev`) | Gameplay tags for elements, classes and statuses (`wor.CompareTags`) | Not tested |
| [B-060](B-060.md) | `fix/B-060-item-use-success` (in `dev`) | A failed item use keeps its charge | Not tested |
| [B-110](B-110.md) | `fix/B-110-menu-close-after-refusal` (in `dev`) | A refused pick keeps the command menu open | Not tested |
| [B-041](B-041.md) | `feature/B-041-gas-attributes` (in `dev`) | GAS attributes mirror the stats (`wor.CompareAttributes`) | Not tested |
| [B-062](B-062.md) | `feature/B-062-effect-identity` (in `dev`) | An effect belongs to its source: same source refreshes, another adds | Not tested |
| [B-063](B-063.md) | `feature/B-063-status-breaks` (in `dev`) | Every full bar lasts one holder turn; Water slows, Wind delays, BD drains | Not tested |
| [B-112](B-112.md) | `fix/B-112-light-break` (in `dev`) | A full Light bar stops crits for one turn | Not tested |
| [B-113](B-113.md) | `fix/B-113-status-mult-once` (in `dev`) | No game change: infused status buildup confirmed as designed (Q-113) | Not tested |
| [B-114](B-114.md) | `fix/B-114-ai-buildup-estimate` (in `dev`) | The AI judges status buildup by the real number | Not tested |
| [B-148](B-148.md) | `feature/B-148-silence-seal` (in `dev`) | Silenced seals half your EP instead of blocking spells | Not tested |
| [B-065](B-065.md) | `feature/B-065-cleanse-skips-gear` (in `dev`) | Cleanse and Iolite leave gear effects alone | Not tested |
| [B-067](B-067.md) | `fix/B-067-small-fixes` (in `dev`) | Small fixes: no corpse walk-back, no animation hang, DoubleHit totals | Not tested |
| [B-201](B-201.md) | `fix/B-201-failed-use-spends-charge` (in `dev`) | A failed item use spends its charge (Q-060 changed) | Not tested |
| [B-202](B-202.md) | `fix/B-202-one-guard-anim-perfect-flash` (in `dev`) | One Guard animation; a Perfect Guard flashes and sounds (Q-182 changed) | Not tested |
| [B-150](B-150.md) | `feature/B-150-fallen-members` (in `dev`) | Fallen members stay in the party; a rest revives them | Not tested |
| [B-151](B-151.md) | `feature/B-151-player-wallet` (in `dev`) | One wallet per player, shared by the party (`wor.Wallet.Print`) | Not tested |
| [B-199](B-199.md) | `feature/B-199-void-random-action` (in `dev`) | A Void break plays the holder's next turn at random | Not tested |
| [B-200](B-200.md) | `fix/B-200-ai-live-status-multiplier` (in `dev`) | The AI's buildup estimate counts gear and buffs | Not tested |
| [B-074](B-074.md) | `feature/B-074-encounter-data` (in `dev`) | An encounter fields a team of up to three enemies | Not tested |
| [B-075](B-075.md) | `fix/B-075-small-fixes` (in `dev`) | Recruit as the placed character; currency limit; shop always closes; hints cleared | Not tested |
| [B-115](B-115.md) | `feature/B-115-carry-gear-state` (in `dev`) | Crystal wear and Broken Darkness absorption carry between fights | Not tested |
| [B-183](B-183.md) | `feature/B-183-gate-choice` (in `dev`) | The trial door asks for a variation and a difficulty; Hard makes enemies stronger | Not tested |
| [B-184](B-184.md) | `feature/B-184-field-enemy-teams` (in `dev`) | Enemy teams on the map; beaten teams stay gone for the run | Not tested |
| [B-185](B-185.md) | `feature/B-185-first-strike` (in `dev`) | First strike: sneak up and act first; caught from behind and the enemy does | Not tested |
| [B-186](B-186.md) | `feature/B-186-enemy-zones` (in `dev`) | Zones: a fight inside a higher-tier zone has stronger enemies | Not tested |
| [B-188](B-188.md) | `feature/B-188-map-features` (in `dev`) | Map features: a chest from the zone's loot pool, a lore object, the boss arena | Not tested |
| [B-190](B-190.md) | `feature/B-190-memory-fragments` (in `dev`) | Memory fragments: pick one up, it goes into the run and the journal | Not tested |
| [B-191](B-191.md) | `feature/B-191-reaction-triggers` (in `dev`) | Reactions: worn items facing a character change the fight; memories weaken a boss | Not tested |
| [B-192](B-192.md) | `feature/B-192-bound-gear` (in `dev`) | Bound gear: only its owner gets the bound bonus | Not tested |
| [B-193](B-193.md) | `feature/B-193-boss-phases` (in `dev`) | Boss phases: HP and hidden conditions, phase kept after a heal, weather and AI rise | Not tested |
| [B-196 (spell cap)](B-196-spell-cap.md) | `fix/B-196-loadout-spell-cap` (in `dev`) | The loadout check flags spells over the holder's spell cap | Not tested |
| [B-197 (AI broken ring)](B-197-ai-broken-ring.md) | `fix/B-197-ai-broken-ring` (in `dev`) | The AI casts a broken ring's F-tier spells; loadout weapon spells greyed on a broken crystal | Not tested |
| [B-204](B-204.md) | `fix/B-204-broken-secondary-weapon` (in `dev`) | A broken secondary weapon's spells are greyed; the AI skips them | Not tested |
| [B-198](B-198.md) | `fix/B-198-luck-dodge-animation` (in `dev`) | A Luck dodge plays the dodge montage | Not tested |
| [B-099](B-099.md) | `fix/B-099-evolution-keeps-secondary` (in `dev`) | Generic + evolution keeps its secondary weapon (attack, abilities, stance) | Not tested |
| [B-081](B-081.md) | `feature/B-081-gear-effects-follow-swap` (in `dev`) | Gear effects follow a mid-fight weapon toggle or ring switch | Not tested |
| [B-082](B-082.md) | `feature/B-082-ring-fusion-stone` (in `dev`) | A ring's fusion stone gives its stat (e.g. Defense) | Not tested |
| [B-084](B-084.md) | `feature/B-084-dismantle-equipped` (in `dev`) | Dismantling equipped gear warns, then leaves no copy in any loadout | Not tested |
| [B-103](B-103.md) | `feature/B-103-commonui-base` (in `dev`) | CommonUI layers: an empty test screen opens and closes with Back (keyboard and gamepad) | Not tested |
| [B-118](B-118.md) | `chore/B-118-debug-asset-fallbacks` (in `dev`) | Combat debug commands with no skill log an error instead of loading a test asset | Not tested |
| [B-111](B-111.md) | `feature/B-111-hud-mockup-v2` (in `dev`) | Combat HUD drafts v2: header candidates A-E, crystal look, BD EP, damage colours, cracking bars. Review is B-102 | Not tested |
| [B-205](B-205.md) | `chore/B-205-config-asset-paths` (in `dev`) | Hub interact key and combat HUD widgets still work with their asset paths moved to `DefaultGame.ini` | Not tested |
| [B-119](B-119.md) | `chore/B-119-data-validation` (in `dev`) | `test.sh` also validates data assets; a newly broken DA fails it (no PIE, a terminal check) | Not tested |
| [B-120](B-120.md) | `chore/B-120-data-asset-validation` (in `dev`) | Validate and Debug > Print on trial, cosmetics, stance, infusion, character, skill and inventory data assets (editor, no PIE) | Not tested |
| [B-123](B-123.md) | `chore/B-123-content-naming` (in `dev`) | Read the one-page naming convention (doc only) | Not tested |
| [B-126](B-126.md) | `fix/B-126-cooked-name-table` (in `dev`) | Element, status and class names on the combat panels; Reality reads Unknown | Not tested |
| [B-207](B-207.md) | `fix/B-207-cooked-labels` (in `dev`) | Shop, defense prompt, party row and merchant hint names | Not tested |
| [B-195](B-195.md) | `feature/B-195-hub-services` (in `dev`) | Hub services: one V NPC per service (Stone Smith, loadout vendor added), each opens its shop | Not tested |
| [B-194](B-194.md) | `feature/B-194-memory-journal` (in `dev`) | Hub journal: reread memories and lore, V's next line, play a conversation (placeholder screen) | Not tested |
| [B-070](B-070.md) | `fix/B-070-trial-strong-ref` (in `dev`) | The trial run keeps hold of its trial (packaged builds) | Not tested |
| [B-071](B-071.md) | `feature/B-071-party-into-battle` (in `dev`) | Recruited party in battle: own pawns, Player/AI per member, switch on its turn, take over when you fall, gearless sit out | Not tested |
| [B-149](B-149.md) | `feature/B-149-formation-cells` (in `dev`) | Members start a fight in their chosen cell; enemies Front/Back by class | Not tested |
| [B-166](B-166.md) | `fix/B-166-headless-skill-start` (in `dev`) | A montage-less skill no longer crashes the async start; ritual spec covers one | Not tested |
| [B-174](B-174.md) | `fix/B-174-support-no-buildup` (in `dev`) | Heals, buffs and support skills build no status | Not tested |
| [B-175](B-175.md) | `feature/B-175-one-switch-per-turn` (in `dev`) | One free weapon or ring switch per turn | Not tested |
| [B-176](B-176.md) | `feature/B-176-flee` (in `dev`) | Flee a normal fight (not mini-boss/boss), at a Gold cost | Not tested |
| [B-177](B-177.md) | `feature/B-177-merge-overshoot` (in `dev`) | Crystal merge gives back its overshoot (ER-28) | Not tested |
| [B-179](B-179.md) | `feature/B-179-trial-vendor-stock` (in `dev`) | Trial vendors roll their stock per run; home shops unchanged | Not tested |
| [B-180](B-180.md) | `feature/B-180-row-changes` (in `dev`) | Free one-row step per turn; push / pull effects | Not tested |
| [B-167](B-167.md) | `refactor/B-167-remove-subsystem-fallbacks` (in `dev`) | "Subsystem missing" damage fallbacks removed (no behaviour change in a normal game) | Not tested |
| [B-168](B-168.md) | `feature/B-168-guard` (in `dev`) | Guard (Block + Parry merged), Perfect Guard, no built-in reflect | Not tested |
| [B-169](B-169.md) | `feature/B-169-luck-dodge` (in `dev`) | Luck dodge: a hit that would land can be dodged by luck, max 15% | Not tested |
| [B-035](B-035.md) | `refactor/B-035-flatten-crystal-destruction` (in `dev`) | Combat-end crystal destruction split out (no behaviour change) | Not tested |
| [B-117](B-117.md) | `chore/B-117-stale-text-sweep` (in `dev`) | Stale comments and docs fixed; unread constants removed (no behaviour change) | Not tested |
| [B-147](B-147.md) | `fix/B-147-rulings-in-code` (in `dev`) | D-034 / D-035 / D-039 / D-041 in comments, docs and a test (no behaviour change) | Not tested |

Move rows to **Done** when every check passes.

## Done
| Job | Tested | Notes |
|---|---|---|
| [B-026](B-026.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
| [B-027](B-027.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
| [B-029](B-029.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
