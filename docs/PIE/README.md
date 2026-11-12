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
**The sweep:** [`Sweep.md`](Sweep.md) lists every retire-after-PIE delete as one checklist (deletes batched into one sweep per milestone, D-124) and the running list of every job waiting on Crown's PIE; nothing is DONE without it (D-128).

| Job | Branch | What it covers | Status |
|---|---|---|---|
| [B-406b](B-406b.md) | `feature/B-406b-boss-cinematic-skip` (in dev, built 2026-11-12) | Boss cinematics, the rest: Tab / Menu skips a boss opening or phase cinematic (HUD back, the fight goes on), does nothing otherwise; a ritual's Execution turn waits for a phase cinematic, then fires | Pending |
| [B-391b](B-391b.md) | `feature/B-391b-hud-pieces` (in dev, built 2026-11-12) | Damage numbers on the new HUD: element colour (Physical white), size / brightness by resistance, immune grey, heals green, the same on host and client; the Broken Darkness card's absorb and overload | Pending |
| [B-400b](B-400b.md) | `feature/B-400b-new-hud-target-highlight` (in dev, built 2026-11-12) | New HUD's target rows: hovering an enemy's row in the new battle HUD's picker highlights it, and the B-400 target view follows | Pending |
| [B-170e](B-170e.md) | `feature/B-170e-action-pips-ui` (in dev, built 2026-11-12) | Action pips 5: with `wor.Turns.ActionPips 1`, party cards show "Actions ●●○ 2 · Max 3 (+75/100)" live; the menu has End Turn (Backspace / View) and every skill / spell / item shows its cost, greyed with the reason when it can't be paid; switch off = unchanged | Pending |
| [B-406](B-406.md) | `feature/B-406-boss-cinematics` (in dev, built 2026-11-12) | Boss cinematics on trial: a boss's opening plays before turn 1 and a phase's cinematic on that phase, the HUD hidden and the fight waiting; empty fields change nothing; an enemy's named attack opens on a front shot; `wor.Camera.BossCinematic` | Pending |
| [B-401](B-401.md) | `feature/B-401-no-reset-between-actions` (in dev, built 2026-11-12) | No reset between actions: with `wor.Turns.ActionPips 1`, after each action of a turn the camera blends (~0.35 s) back to the same hero's turn view, no wide beat; only a turn change does the full move (`wor.Camera.Print` counts same-turn returns) | Pending |
| [B-170c](B-170c.md) | `feature/B-170c-multi-action-turns` (in dev) | Action pips 3: with `wor.Turns.ActionPips 1`, a turn spends banked actions: act again while actions are left, Defend ends the turn and keeps the rest, a skill's Action Cost above Max or the bank is refused, arming a ritual keeps the turn, switching is free, the AI still acts once; `wor.Actions.Print` shows the turn's spends; switch off = unchanged | Pending |
| [B-170f](B-170f.md) | `feature/B-170f-ai-actions` (in dev, built 2026-11-12) | Action pips 6: with `wor.Turns.ActionPips 1`, the AI acts again while it has actions, ends its turn and banks when nothing is worth doing or a cost-2 pick is worth the wait, pays action costs, values action gains and losses and Emerald's new rule; stunned or Void-broken one action; Easy sometimes stops early; switch off = one action a turn as before | Pending |
| [B-170i](B-170i.md) | `feature/B-170i-reflex-regain` (in dev) | Reflex regain rework (Crown, Q-170i): `wor.Actions.Print` shows "Regain N (Reflex value / next)"; light Reflex regains 2, focused 3; Body levels cancel out, Body boosts count; defence window unchanged | Pending |
| [B-170d](B-170d.md) | dev (from `feature/B-170d-action-effects-cloud`, built 2026-11-12) | Action pips 4: with `wor.Turns.ActionPips 1`, Ice takes an action at once, Water slows Spell Speed only, Wind slows actions and takes one next turn; Emerald gives actions on your side (F..S ladder, may pass Max) and skips an enemy's next turn; new Gain / Lose Action and Max Actions effects; Extra Action shown as Extra Turn; switch off = unchanged | Pending |
| [B-170b](B-170b.md) | `feature/B-170b-turn-rotation` (in dev) | Action pips 2: with `wor.Turns.ActionPips 1` set before a fight, turns go round in a fixed rotation (sides alternate, party first; a lone boss after every hero; a smaller side's extra seats rotate on; first strike opens); the strip matches; bonus, skip, Execution turns as built; `wor.Turns.PrintBelt` shows the mode and round; switch off = unchanged | Pending |
| [B-407](B-407.md) | `feature/B-407-short-signature-cameras` (in dev, built 2026-11-12) | A player or team skill's own camera sequence is cut back to the turn view at 2 s; an enemy's runs to its end; `wor.Presentation.Validate` warns on a long one; `wor.Camera.SignatureCap` | Pending |
| [B-170a](B-170a.md) | `feature/B-170a-action-pips` (in dev) | Action pips 1: `wor.Actions.Print` shows each combatant's actions, Max Actions with action-point progress ("Max 3 (+75/100)") and Reflex regain; `AddPoints` / `Reset` move them; play unchanged | Pending |
| [B-173](B-173.md) | `feature/B-173-turn-start-energy` (in dev, built 2026-11-12) | Energy refills a share of Max EP at each turn start (D-089); no base EP from basic attacks / Perfect Guards | Pending |
| [B-394](B-394.md) | `feature/B-394-cast-vfx-source` (in dev) | A cast's visual comes from the weapon (fixed), the adapter type's default (Ring) or, for a Generic wearing both, the class preference; unchanged until art is set | Pending |
| [B-392](B-392.md) | `feature/B-392-two-guards` (in dev, save v32) | Two guards: the weapon's guard or the character's unarmed one by a per-character setting (`wor.Guard.Set`); same window | Pending |
| [B-393](B-393.md) | `feature/B-393-parry-vfx` (in dev, save v33) | Landing a parry: the weapon guard sparks; the guard's parry effect plays in the element behind it (weapon crystal or own magic, `wor.Guard.SetParryPriority`); unarmed with no element shows nothing | Pending |
| [B-391](B-391.md) | `feature/B-391-battle-hud` (in dev, built 2026-11-12) | The new plain C++ battle HUD (`wor.UI.NewHUD` 1): cards, turn order, statuses view-all, the command menu with key hints; main-menu category keys | Pending |
| [B-402](B-402.md) | `feature/B-402-side-on-defence-view` (in dev, built 2026-11-12) | An enemy attack on your character shows a low side-on shot of both fighters (the whole party: a wide side-on shot); no shake or zoom punch while you defend | Pending |
| [B-398](B-398.md) | `feature/B-398-turn-view-setting` (in dev, built 2026-11-12, save v34) | Turn-view setting: Centred Wide (default) or Over the Shoulder (hero lower-left third, enemies mid-frame, the same for every member); saved, kept for the next fight (`wor.Camera.Style`) | Pending |
| [B-399](B-399.md) | `feature/B-399-calm-menus` (in dev, built 2026-11-12) | Calm menus: opening a sub-menu steps the turn camera one short step closer and lower, Back steps out; nothing moves while browsing or picking a target (`wor.Camera.MenuDepth`) | Pending |
| [B-400](B-400.md) | `feature/B-400-target-cut` (in dev, built 2026-11-12) | Target cut: with 2+ targets the picker cuts (Over the Shoulder) or re-aims (Centred Wide) to the highlighted target on each hover / focus / `wor.Camera.TargetCycle`; one target never moves; cancel returns | Pending |
| [B-404](B-404.md) | `feature/B-404-wide-beats` (in dev, built 2026-11-12) | Wide beats and the party handover: after an action the camera goes wide, then blends (~0.5 s) behind the next hero; `OnTurnHandover` once per turn start (`wor.Camera.Handover` preview) | Pending |
| [B-396](B-396.md) | `fix/B-396-combat-hud-visibility` (in dev) | Combat HUD invisible: `wor.UI.Print` in a fight shows each HUD widget's viewport, visibility, opacity, position and size; the trial gate's focus error is gone | Pending |
| [B-064c](B-064c.md) | `feature/B-064c-ice-rest` (in dev) | Ice, the rest: Ice resistance from the profile asset, gear and Ice-keyed buffs (Ice and Water); a Water Ward or Water-scoped boost doesn't reach an Ice hit, an Ice-keyed Ward does | Pending |
| [B-064a](B-064a.md) | `feature/B-064a-ice-cloud` (in dev) | Ice: a Water spell can deal Ice (Deals Ice); Ice resistance alone resists it; a full Ice bar slows like Water's and deals 10% max HP | Pending |
| [B-273b](B-273b.md) | `feature/B-273b-hit-kind-cloud` (in dev, with B-064a) | The hit kind replaces the element / physical pair in buildup, damage resistance and the GAS executions; no number changes (an Ice hit reads Ice resistance alone, for B-064a) | Pending |
| [B-042f](B-042f.md) | `feature/B-042f-stones-item-effects-gas-cloud` (in dev) | GAS stage 3 part 6: `wor.Effects.Compare` 0 differ at UseGAS 0 and 1 (stones and item effects included); B-042g's gate | Pending |
| [B-042g](B-042g.md) | `feature/B-042g-gas-readers-default-cloud` (in dev) | GAS: resistance, Ignore Defense and reflect read the GEs at UseGAS 1 | Pending |
| [B-043a](B-043a.md) | `feature/B-043a-costs-gas-cloud` (in dev) | GAS stage 4 part 1: skill EP costs and the infusion HP backlash paid as Instant GEs (`wor.Costs.UseGAS`), same numbers | Pending |
| [B-043b](B-043b.md) | `feature/B-043b-ability-base-cloud` (in dev) | GAS stage 4 part 2: loadout skills granted as abilities (`wor.Abilities.Print`); at `wor.Abilities.UseGAS` 1 every spell and ability starts through its ability, same play, EP paid once | Pending |
| [B-043c](B-043c.md) | `feature/B-043c-melee-ability-tasks-cloud` (in dev) | GAS stage 4 part 3: at `wor.Abilities.UseGAS` 1 melee impacts arrive at the skill's ability as `Event.Combat.Impact`; hits, parries, dodges and interrupts play as before | Pending |
| [B-043d](B-043d.md) | `feature/B-043d-projectile-ability-tasks` (in dev) | GAS stage 4 part 4: at `wor.Abilities.UseGAS` 1 projectile arrivals reach the spell's ability as events and a burst's spacing runs on it; spells fly, land and are defended as before | Pending |
| [B-043e](B-043e.md) | `feature/B-043e-spell-shapes-abilities` (in dev) | GAS stage 4 part 5: at `wor.Abilities.UseGAS` 1 each AOE / Instant hit and each support cast arrives at the spell's ability as `Event.Combat.SpellResolve.*`; area, instant, heal and buff spells play as before | Pending |
| [B-043f](B-043f.md) | `feature/B-043f-ability-commits-cost` (in dev) | GAS stage 4 part 6: at `wor.Abilities.UseGAS` 1 the skill ability commits and pays its action's EP at the executor's pay point; EP, refusals, rituals and whole fights play as before | Pending |
| [B-043g](B-043g.md) | `feature/B-043g-ritual-arm-ability` (in dev) | GAS stage 4 part 7: at `wor.Abilities.UseGAS` 1 a ritual's fire runs through its skill ability; paid once at the arm, fires as before | Pending |
| [B-390](B-390.md) | `feature/B-390-curse-tiers-clear-price` (in dev) | A curse's tier (F to S) sets its clear price in its essence (Pillar or Elemental); short of it the hub refuses with a message | Pending |
| [B-357e](B-357e.md) | `refactor/B-357e-evolution-readers` (in dev) | No play change: the remaining evolution readers go through the one resolve path; one lasting-passive check | Pending |
| [B-380d](B-380d.md) | `feature/B-380d-apparel-lore-socket-cloud` (in dev) | Apparel socket swap: the old crystal goes back to storage; an evolution must be owned and free | Pending |
| [B-386](B-386.md) | `feature/B-386-apparel-amp-cloud` (in dev) | Apparel carries Amp lines (the deckbuilder's HP · Amp · Def) | Pending |
| [B-387](B-387.md) | `feature/B-387-crystal-amp-cloud` (in dev) | Worn crystals (and the seated evolution) grant Amp to their element by tier: F0 E0 D1 C1 B2 A2 S3 | Pending |
| [B-389](B-389.md) | `feature/B-389-resistance-by-type-cloud` (in dev) | Resistance by damage type: Defence is the All line; a Physical line cuts physical hits only | Pending |
| [B-382](B-382.md) | `feature/B-382-amp-lines-cloud` (unbuilt) | Amp lines on gear: All / Physical / one element, added to the action's base; damage buffs keyed by element; `wor.Damage.Amp` | Pending |
| [B-383](B-383.md) | `feature/B-383-damage-scopes-cloud` (unbuilt) | Every damage / defence boost has a scope (All, Physical, Spell, one element; default All); `wor.Damage.Scopes` | Pending |
| [B-384](B-384.md) | `feature/B-384-one-damage-channel-cloud` (in dev) | The four all-damage % channels add into one; **stacked builds hit a little lower** (before / after numbers in the sheet); `wor.Damage.FoldAllDamage` | Pending |
| [B-385](B-385.md) | `feature/B-385-scope-gas-followups-cloud` (in dev) | Damage scopes hold at `wor.Effects.UseGAS 1` (Defense, the mirror, `wor.Damage.Scopes`) | Pending |
| [B-360c](B-360c.md) | `feature/B-360c-copies-by-member` (in `dev`) | A later copy of a character places its own victory levels and is bought for under its own key | Pending |
| [B-336](B-336.md) | `refactor/B-336-world-subsystems` (in `dev`) | The ten fight systems live on the level's world (one per PIE window and per level); `wor.Fight.Subsystems` lists them | Pending |
| [B-361c](B-361c.md) | `test/B-361c-recruit-inventory-specs` (in `dev`) | Tests only: a recruit through a mid-run save, the one-pouch give refusal, buying for a recruit, a full or missing host ledger at hand-over | Pending |
| [B-337b](B-337b.md) | `feature/B-337b-conditions-part2` (in `dev`) | Reactions match through the shared conditions (no gameplay change); `wor.Reaction.Print` shows the condition list | Pending |
| [B-337c](B-337c.md) | `feature/B-337c-bound-memory-conditions` (in `dev`) | Bound-item memories check through the shared conditions; skill trigger On Flawless Parry | Pending |
| [B-337d](B-337d.md) | `refactor/B-337d-skill-conditions` (in `dev`) | Skill-effect conditions check through the shared conditions (HP / EP percent) | Pending |
| [B-337e](B-337e.md) | `feature/B-337e-condition-migration-cloud` (in `dev`) | Skill-effect conditions convert on load; `wor.Conditions.ListOld` lists assets to resave (editor step); effects fire as before | Pending |
| [B-362b](B-362b.md) | `feature/B-362b-wipe-drops-prisms` (in `dev`) | A wipe drops the run's Prisms at the fight instead of taking pillar points; beating that team gets them back; a second wipe loses the first drop | Pending |
| [B-362c](B-362c.md) | `feature/B-362c-prism-pickup` (in `dev`) | The Prism drop lies as a pickup where the party stood when the fight started; walking into it gives it back; a win no longer does | Pending |
| [B-365](B-365.md) | `feature/B-365-flawless-parry` (in `dev`) | Flawless Parry: every hit of one attack perfectly guarded or dodged logs one line at the last hit; a condition only | Pending |
| [B-349](B-349.md) | `chore/B-349-console-names-cloud` (in `dev`) | Every console command is `wor.<System>.<Verb>`; old names gone (table in the sheet) | Pending |
| [B-355b](B-355b.md) | `feature/B-355b-boss-moment-rules-cloud` (in `dev`) | A boss's "at moment" rules fire from its one rule list, conditions read at the moment | Pending |
| [B-230](B-230.md) | `feature/B-230-replicated-status-bars-cloud` (in `dev`) | 2-player PIE: the client's status bars follow the host's | Pending |
| [B-231](B-231.md) | `feature/B-231-actions-server-multicast-cloud` (UNBUILT, on B-230) | 2-player PIE: the client sees the host's swings, guards and hits (montages and hits by multicast) | Pending |
| [B-230b](B-230b.md) | `feature/B-230b-grid-slot-replicated-cloud` (UNBUILT, on B-231) | 2-player: the client sees every combatant's row; its menu greys the row step it can't take | Pending |
| [B-231b](B-231b.md) | `feature/B-231b-cosmetic-projectiles-cloud` (UNBUILT, on B-231) | 2-player: the client sees the host's projectiles fly and burst | Pending |
| [B-231c](B-231c.md) | `feature/B-231c-client-telegraphs-cloud` (UNBUILT, on B-231b) | 2-player: the client sees the host's skill effects and telegraph tells | Pending |
| [B-228b](B-228b.md) | `feature/B-228b-client-combat-hud-cloud` (in `dev`) | 2-player PIE: the client builds its own combat HUD at fight start and removes it at the end | Pending |
| [B-232b](B-232b.md) | `feature/B-232b-menu-on-commander-cloud` (UNBUILT, on B-228b) | 2-player PIE: each player's menu opens only for the members it commands | Pending |
| [B-232c](B-232c.md) | `feature/B-232c-run-readers-cloud` (UNBUILT, on B-232b) | 2-player: a client's weapon switch, row step and "Let AI play" run on the host | Pending |
| [B-232d](B-232d.md) | `feature/B-232d-run-readers-cloud` (UNBUILT, on B-232c) | 2-player: the client sees opened chests, spent memory spots and its waiting pillar levels | Pending |
| [B-232e](B-232e.md) | `feature/B-232e-recruit-ai-seat-cloud` (UNBUILT, on B-232d) | 2-player: a client's recruit joins through the host as an AI seat | Pending |
| [B-233](B-233.md) | `feature/B-233-sessions-null-cloud` (in `dev`) | Two instances: host, find and join a LAN session with `wor.Session.*` | Pending |
| [B-234](B-234.md) | `feature/B-234-checkpoint-copies-cloud` (UNBUILT, on B-233) | 2-player: each save the host makes is copied to the client's checkpoint slot (`wor.Coop.PrintCheckpoint`) | Pending |
| [B-234b](B-234b.md) | `feature/B-234b-host-hand-over-cloud` (UNBUILT, on B-234) | The next player hosts from its copy when the host drops; the others rejoin | Pending |
| [B-348b](B-348b.md) | `feature/B-348b-save-head-starts-cloud` (in `dev`) | A member's head-start pillars survive save / load (save v21) | Pending |
| [B-351b](B-351b.md) | `feature/B-351b-stone-socket-carry-cloud` (in `dev`) | Removing crystals takes the stone off too; no second stone over an authored one; the stone socket carries through a run and its save (v22) | Pending |
| [B-351c](B-351c.md) | `feature/B-351c-one-list-readers-cloud` (UNBUILT, on B-351b) | `wor.Loadout.Print` shows both sockets and the one skill list; a stone slot's pick reaches the fight | |
| [B-357](B-357.md) | `feature/B-357-evolution-one-cost-cloud` (in `dev`) | An evolution costs 2 wherever it goes: an evolved adapter now costs 3 | Pending |
| [B-235](B-235.md) | `feature/B-235-personal-prism-drops-cloud` (in `dev`) | 2-player PIE: a wipe drops each player's own Prisms; each picks up only theirs | Pending |
| [B-235b](B-235b.md) | `feature/B-235b-personal-rewards-cloud` (UNBUILT, on B-235) | 2-player: the pickup message on the picker's screen; a player who left doesn't keep the pickup up | Pending |
| [B-235c](B-235c.md) | `feature/B-235c-instanced-chests-cloud` (UNBUILT, on B-235b) | 2-player: each player opens a chest for their own item; a client's presses run on the host | Pending |
| [B-235d](B-235d.md) | `feature/B-235d-private-memories-cloud` (UNBUILT, on B-235c) | 2-player: a friend's qualifying memories land in their own journal, privately | Pending |
| [B-366](B-366.md) | `feature/B-366-currency-names` (in `dev`) | Currency names: the run currency reads Prisms, the hub currency Gold (names only) | Pending |
| [B-366d](B-366d.md) | `feature/B-366d-merge-place-currency` (in `dev`) | Merging crystals costs hub Gold at home and run Prisms in a run (console merge) | Pending |
| [B-366e](B-366e.md) | `fix/B-366e-repair-place-currency` (in `dev`) | Gear repair costs the currency of the place | Pending |
| [B-367](B-367.md) | `feature/B-367-interrupt-points` (in `dev`) | Only a Perfect Guard / Perfect Dodge stops an interrupt point; Flawless Parry at an interrupt | Pending |
| [B-189](B-189.md) | `feature/B-189-element-map-look-cloud` (in `dev`) | A trial's element sets its map's sky, fog and sun (placeholder colours); `wor.Trial.Look <Element>` previews | Pending |
| [B-278](B-278.md) | `feature/B-278-asset-manager-rules-cloud` (in `dev`) | A packaged Development build has every data asset; first fights don't stutter (PSO steps) | Pending |
| [B-367b](B-367b.md) | `feature/B-367b-perfect-guard-only-cloud` (in `dev`) | Only a Perfect Guard stops an interrupt point or counts toward a Flawless Parry; a Perfect Dodge does neither | Pending |
| [B-374](B-374.md) | `feature/B-374-six-schools-cloud` (in `dev`) | Six spell schools: the menu lists held schools in order; Destruction is its own school; a validator lists assets to re-tag (editor step) | Pending |
| [B-368](B-368.md) | `feature/B-368-notify-validator-cloud` (in `dev`) | Checks only: `wor.Presentation.Validate` lists skills whose montage notifies don't fit their arrays; fix the listed data | Pending |
| [B-369](B-369.md) | `feature/B-369-impact-vfx-outcome-cloud` (in `dev`) | Each impact shows its outcome's visual (hit / crit / guard spark / dodge whiff); set the defaults on a CosmeticsData first | Pending |
| [B-370](B-370.md) | `feature/B-370-cinematic-window-cloud` (in `dev`) | An enemy attack on your character moves the camera behind it (the Defence view) 0.75 s before the first hit; movement off keeps the static view | Pending |
| [B-371](B-371.md) | `feature/B-371-shot-patterns-cloud` (UNBUILT, on B-370) | Skills without a sequence get code-built cameras: shoulder push-in, projectile follow, impact punch, pull-out for several targets (Crown approves the look, B-157) | |
| [B-073](B-073.md) | `feature/B-073-run-rewards-cloud` (in `dev`) | A win pays run Prisms now; hub Gold and essences are tallied and paid at the run's end, win or lose; save v23 | Pending |
| [B-187](B-187.md) | `feature/B-187-rest-spot-cloud` (UNBUILT, on B-073) | A rest spot heals, revives and becomes the last rest spot; leaving there pays half; placeholder rest menu | Pending |
| [B-363](B-363.md) | `feature/B-363-levelling-at-rest-spots-cloud` (UNBUILT, on B-187) | Pillar points are placed and bought (run Prisms) only at rest spots | Pending |
| [B-364](B-364.md) | `feature/B-364-over-the-pillar-cap-cloud` (UNBUILT, on B-363) | Past 7 a pillar point becomes an over-cap point (3 per pillar, +5 % each) | Pending |
| [B-347b](B-347b.md) | `feature/B-347b-run-armoury` (in `dev`) | During a run a member sees and equips only what the run brought or found; left-home gear and loadouts come back intact; the chest waits until the run ends | Pending |
| [B-366c](B-366c.md) | `feature/B-366c-shop-place-currency` (in `dev`) | Shops charge the hub currency (Gold) at home and the run currency (Prisms) in a run; cost lines, totals and Confirm follow it | Pending |
| [B-366b](B-366b.md) | `fix/B-366b-convert-place-currency` (in `dev`) | Converting a duplicate pays the hub currency (Gold) at home and the run currency (Prisms) in a run; the texts name it | Pending |
| [B-362](B-362.md) | `feature/B-362-losing-a-fight` (in `dev`) | A party wipe costs run pillar points, revives the party at the trial; the run ends after 3 main-boss losses or 5 wipes; save v18 | Pending |
| [B-361b](B-361b.md) | `feature/B-361b-mentor-tokens` (in `dev`) | Mentor Tokens: earned playing together (a boss, a finished co-op run), spent teaching a skill to another player's character; save v17 | Pending |
| [B-361](B-361.md) | `feature/B-361-recruits-host-inventory` (in `dev`) | A trial recruit carries the host's pouch and trades skills; its skills stay with the host at run end; no skill trading between players | Pending |
| [B-333b](B-333b.md) | `chore/B-333b-seven-balances-specs` (in `dev`) | Tests only: four specs moved off the retired typed essences | Pending |
| [B-316b](B-316b.md) | `feature/B-316b-skills-per-character` (in `dev`) | Learned skills are each character's own and kept; give / ask for a skill on the give placeholder; buy a spell or ability for another member; save v16 | Pending |
| [B-314b](B-314b.md) | `feature/B-314b-run-layer-save` (in `dev`) | A run saved mid-way keeps its run finds run-only and its home gear's home state; the upgrade memory is saved; save v15 | Pending |
| [B-313b](B-313b.md) | `feature/B-313b-pouch-rest` (in `dev`) | A member's crystal counts come from its pouch; the item-slot rules skip a member's inert preset picks | Pending |
| [B-042j3](B-042j3.md) | `feature/B-042j3-absorb-part3` (in `dev`) | Reflect by element; a fully absorbed hit fires no On Take Damage conditional; DoT previews count the absorbers | Pending |
| [B-360b](B-360b.md) | `feature/B-360b-member-identity` (in `dev`) | Two players may bring the same character: each copy has its own id, gear, pouch, cell and commander; save v14 | Pending |
| [B-328b](B-328b.md) | `feature/B-328b-zone-guide-enforced` (in `dev`) | `wor.Trial.PrintZones` lists every encounter placed below its zone's guide tier | Pending |
| [B-042j2](B-042j2.md) | `feature/B-042j2-absorb-part2` (in `dev`) | Absorbers on DoT ticks; a stopped hit counts as On Guard and as a hit, not damage; Ward scopes; skills and items can give an absorber (Negate … effect types) | Pending |
| [B-360](B-360.md) | `feature/B-360-coop-party` (in `dev`) | Co-op party (D-150): no rotation; a joiner takes the AI seat the host picks (`wor.UI.JoinSeat`), its gear goes to the host; full = refused; 2-player PIE | Pending |
| B-331 (reverted) | `feature/B-331-one-speed` → `fix/B-331r-revert-one-speed` (in `dev`) | One Speed, part 1 — **reverted by B-331r (D-151)**: Action Speed and Spell Speed stay separate; the PIE sheet is gone, nothing to check | Reverted |
| [B-328](B-328.md) | `feature/B-328-enemy-hp-no-zone` (in `dev`) | Each zone tier names the enemy Combat Tier that belongs there (`wor.Trial.PrintZones`); an authoring guide, the zone itself scales nothing (B-327) | Pending |
| [B-353b](B-353b.md) | `fix/B-353b-bd-stone-energy` (in `dev`) | No stone (Luck, Reflex, Healing included) gives a Broken Darkness character energy; gems still do | Pending |
| [B-232](B-232.md) | `feature/B-232-run-state-party` (in `dev`) | Run state and roster on the GameState; each player commands its own character; an unclaimed member rotates (2-player PIE) | Pending |
| [B-353](B-353.md) | `feature/B-353-stat-stone` (in `dev`) | One StatStone kind with a sub-stat; old single-stat stones map to it, same bonus and name | Pending |
| [B-352](B-352.md) | `feature/B-352-fusions-retire` (in `dev`) | Fusions retire: nothing makes, buys or attaches one; an authored fusion reads as an empty socket; evolution wear stays | Pending |
| [B-351](B-351.md) | `feature/B-351-two-sockets` (in `dev`) | Two sockets per weapon / adapter; one skill-slot list per weapon (`wor.Loadout.ReplaceSkill` / `RestoreSkill` until the gear screen); save v13 wipes older saves | Pending |
| [B-158](B-158.md) | `feature/B-158-one-hub-controller` (in `dev`) | One hub controller: E interact and EnsureLocalParty on `AWoRHubController`; editor step deletes the Blueprint's IA_Interact / EnsureLocalParty nodes first | Pending |
| [B-356](B-356.md) | `feature/B-356-three-schools` + `feature/B-356c-conjuration-damage-school` (in `dev`) | Three spell schools: Conjuration (damage, Destruction retired onto it; D-146 corrected), Enhancement, Restoration; names on spells; AI reads effects; loadout caps and filters on the shown school; re-tag Destruction assets in the editor | Pending |
| [B-345](B-345.md) | `feature/B-345-one-ai-path` (in `dev`) | One AI scoring path for spells and abilities; AI weights in the combat tuning data; no change at default weights | Pending |
| [B-359](B-359.md) | `feature/B-359-weather-baseline` (in `dev`) | Weather = the trial's element; a boss phase steps it to the boss's element; leader HP no longer moves it | Pending |
| [B-358](B-358.md) | `feature/B-358-one-physical-resistance` (in `dev`) | One Physical resistance column (buildup only, ED-27); each physical subtype still picks its break | Pending |
| [B-350](B-350.md) | `chore/B-350-test-actors-out` (in `dev`) | Gap specs for the five legacy test actors (listed for an editor delete); test.sh slowest 10; balance sim nightly only | Pending |
| [B-348](B-348.md) | `chore/B-348-one-save-version` (in `dev`) | One save version (D-138): older saves wiped; SaveGame tags gone | Pending |
| [B-346](B-346.md) | `feature/B-346-party-seats` (in `dev`) | Seats: you command everyone; AI seats set before the fight stay AI (own difficulty, Medium); no takeover or Let AI Play; co-op rotation with B-232 | Pending |
| [B-342](B-342.md) | `feature/B-342-three-rows` (in `dev`) | Three rows, no columns: front ±20% dealt and taken, back −20%; an ally never blocks a row step | Pending |
| [B-341](B-341.md) | `feature/B-341-turn-rules-trim` (in `dev`) | 3-step tie-break; one bonus-turn rule (lands next; skip removes one; death purges, revive next round); Emerald and Extra Action follow; `wor.Turns.GrantBonus`, `wor.Turns.SkipNext` | Pending |
| [B-355](B-355.md) | `feature/B-355-boss-rules` (in `dev`) | A boss's phases and triggers are one rule list (when → condition → result); old data reads across; `wor.Boss.Print` lists the rules | Pending |
| [B-337](B-337.md) | `feature/B-337-conditions` (in `dev`) | One condition vocabulary (`FWoRCondition`, `WoRConditions`); memories evaluate through it; no gameplay change | Pending |
| [B-343](B-343.md) | `chore/B-343-guard-dodge-only` (in `dev`) | Guard and Dodge only; Parry and Dodge Right unbound; the one Dodge plays the left montage (Q-343) | Pending |
| [B-229](B-229.md) | `feature/B-229-input-rpcs` (in `dev`) | Co-op: a client's Guard / Dodge and menu pick (with its infusion) reach the server; `wor.Net.SendDefendPick`, `wor.Net.PrintInputs` | Pending |
| [B-228](B-228.md) | `feature/B-228-turn-state-gamestate` (in `dev`) | Turn order on the GameState; client HUD and command menu read it | Pending |
| [B-344](B-344.md) | `feature/B-344-difficulty-sets-ai` (in `dev`) | Trial difficulty sets the enemy AI tier; per-encounter override | Not built |
| [B-338](B-338.md) | `chore/B-338-delete-item-storage` (in `dev`) | `wor.UI.ItemStorage` deleted (D-131); `wor.UI.Storage` covers pouch and storage moves | Pending |
| [B-333](B-333.md) | `feature/B-333-seven-balances` (in `dev`) | Seven balances: Elemental / Pillar Essence, typed essences retire, prices and yields rerouted, save v11 wipes older saves | Not built |
| [B-335](B-335.md) | `chore/B-335-per-instance-rolls-obsolete` (in `dev`) | Per-instance random rolls flagged OBSOLETE (D-127); gear keeps tier + quality; no gameplay change | Not built |
| [B-334](B-334.md) | `feature/B-334-infusion-costs` (in `dev`) | Infusion EP = base × charge level (L1 ×1, L2 ×2); HP cost only at L2; source choice stays | Not built |
| [B-330](B-330.md) | `feature/B-330-pillars-only` (in `dev`) | Pillars only: a level-up grants no sub-stat points; the authored budget is 93 at every level | Not built |
| [B-326](B-326.md) | `chore/B-326-flat-requirement-penalty` (in `dev`) | Unmet requirement = x0.6 and no scaling for weapons, abilities and spells; still castable | Not built |
| [B-327](B-327.md) | `fix/B-327-zone-health-once` (in `dev`) | The zone scales no stats; enemy HP = own x rank x Combat Tier (D-120) | Pending |
| [B-224](B-224.md) | `feature/B-224-combat-numbers` (in `dev`) | Combat numbers baseline: rank HP x1.5/3/6, Defense cap 75%, crit base x1.25, requirement floor 0.5, tuning ladders | Not built |
| [B-288](B-288.md) | `feature/B-288-ladders` (in `dev`) | Every ladder rung pinned by a spec; Damage Taken Down cap named; no gameplay change | Not built |
| [B-322](B-322.md) | `feature/B-322-per-character-pouch` (in `dev`) | A 10-slot pouch per character; save v9; older save's pouch to the leader | Pending |
| [B-323](B-323.md) | `feature/B-323-give-between-characters` (in `dev`) | Give a weapon, adapter or pouch stack between your characters; `wor.UI.Give` | Pending |
| [B-324](B-324.md) | `feature/B-324-shared-chest-gear` (in `dev`) | Unworn gear in the shared chest, any member takes it; `wor.UI.Storage` | Pending |
| [B-313](B-313.md) | `refactor/B-313-pouch-per-member` (in `dev`) | Each member fights with its own pouch; item use spends the pouch only; D-072 caps per fight | Pending |
| [B-325](B-325.md) | `feature/B-325-combat-tier` (in `dev`) | Combat Tier on panels, enemies' HP and level drops from it; `wor.Combat.PrintTiers` | Not built |
| [B-042i](B-042i.md) | `refactor/B-042i-trigger-gas-specs` (in `dev`) | Parity specs: a trigger's effect lands on GAS as one GE on both paths; no gameplay change | Pending |
| [B-312b](B-312b.md) | `refactor/B-312b-loadout-owns` (in `dev`) | Store 1b step 1: the loadout's owns-checks read the store view; no gameplay change | Pending |
| [B-314](B-314.md) | `refactor/B-314-run-layer` (in `dev`) | The run layer: found gear dropped at run end with its tier remembered; home gear's wear and run-added attachments reset | Pending |
| [B-347](B-347.md) | `feature/B-347-free-run-loadout` (in `dev`) | The run loadout is free: the worn loadout goes on the run, no purchase; Gold for in-run vendors | Pending |
| [B-316](B-316.md) | `feature/B-316-give-request-buyfor` (in `dev`) | Buy for any party member (AI included) from a shop; `wor.Shop.BuyFor` | Not built |
| [B-339](B-339.md) | `feature/B-339-inventory-screen` (in `dev`) | One inventory screen `wor.UI.Inventory`: Give, Store, Take, Dismantle, Convert over a member and the chest | Not built |
| [B-354](B-354.md) | `feature/B-354-tier-caps-retire` (in `dev`) | One S crystal per battle loadout; D-072 bands and per-fight pouch caps gone | Pending |
| [B-312](B-312.md) | `refactor/B-312-store-views` (in `dev`) | Gear store step 1: GUID index, member views, `wor.Inventory.CompareStore`; no gameplay change | Pending |
| [B-293](B-293.md) | `feature/B-293-balance-targets` (in `dev`) | Balance targets, reports, GAS parity, `wor.Balance.Run`; no gameplay change | Pending |
| [B-261](B-261.md) | `refactor/B-261-mirror-dirty-check` (in `dev`) | A GAS stat read skips the mirror re-sync when nothing changed (P5) | Pending |
| [B-223](B-223.md) | `feature/B-223-level-drops` (in `dev`) | Level drops: chance from normal enemies, guaranteed from mini-bosses; place them with `wor.UI.LevelDrop` | Pending |
| [B-224b](B-224b.md) | `feature/B-224b-rank-hp-per-team` (in `dev`) | Rank HP per team; enemy HP by tier (F default) | Pending |
| [B-293d](B-293d.md) | `chore/B-293d-decided-balance` (in `dev`) | Crown's balance answers: rank HP x4 / x8, requirement floor 0.25, 10% damage floor | Not built |
| [B-223b](B-223b.md) | `feature/B-223b-personal-level-drops` (in `dev`) | Level drops per character: each member rolls its own, places its own | Not built |
| [B-318](B-318.md) | `feature/B-318-client-gas-reads` (in `dev`) | Co-op guests read effects from the server's GAS read (UseGAS 1) | Not built |
| [B-279c](B-279c.md) | `feature/B-279c-outfits` (in `dev`) | Outfits: six slots, piece assets, authored / current outfit (save v10), Leader Pose assembly | Not built |
| [B-293b](B-293b.md) | `feature/B-293b-parity-effects` (in `dev`) | GAS parity with status breaks; test code only | Not built |
| [B-293c](B-293c.md) | `chore/B-293c-balance-tuning` (in `dev`) | Balance first pass: AI defends less in even steps; difficulty scales enemy damage, not HP; normal fights in party rounds | Not built |
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
| [B-020](B-020.md) | `refactor/B-020-stat-modifier-table` (in `dev`) | Stat buffs/debuffs read from one table (same numbers); `wor.Stats.Compare`, `wor.Stats.Print` | Not tested |
| [B-021](B-021.md) | `refactor/B-021-stone-row-table` (in `dev`) | Stat stones read from one row table (same effects); `wor.Stones.Print` | Not tested |
| [B-022](B-022.md) | `refactor/B-022-loadout-getters` (in `dev`) | Loadout read without copying; class from CharacterData; no gameplay change | Not tested |
| [B-023](B-023.md) | `fix/B-023-ai-live-enums` (in `dev`) | AI sees Stun (not Wind's delay) and finds Cleanse Self spells | Not tested |
| [B-024](B-024.md) | `refactor/B-024-fold-weapon-ring-managers` (in `dev`) | Ring/weapon managers folded into the loadout; ring auto-switch on break; no gameplay change | Not tested |
| [B-025](B-025.md) | `refactor/B-025-economy-preamble` (in `dev`) | Economy actions share one owner/authority/component check; no gameplay change | Not tested |
| [B-030](B-030.md) | `fix/B-030-reset-for-combat` (in `dev`) | Nothing from one fight carries into the next (defense, charge, status bars, turn order, menu) | Not tested |
| [B-003](B-003.md) | `chore/B-003-bp-cpp-cutover` (in `dev`) | Combat HUD, panel effects list and turn slots run on C++ (BP overrides deleted); hub unchanged (D-076) | Not tested |
| [B-156](B-156.md) | `feature/B-156-combat-camera` (in `dev`) | Combat camera: turn view behind the acting character, pan-out on enemy turns, `wor.Camera.Print` style / movement-off commands (no cinematics until B-157) | Not tested |
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
| [B-040](B-040.md) | `feature/B-040-gas-tags` (in `dev`) | Gameplay tags for elements, classes and statuses (`wor.Tags.Compare`) | Not tested |
| [B-060](B-060.md) | `fix/B-060-item-use-success` (in `dev`) | A failed item use keeps its charge | Not tested |
| [B-110](B-110.md) | `fix/B-110-menu-close-after-refusal` (in `dev`) | A refused pick keeps the command menu open | Not tested |
| [B-041](B-041.md) | `feature/B-041-gas-attributes` (in `dev`) | GAS attributes mirror the stats (`wor.Attributes.Compare`) | Not tested |
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
| [B-130](B-130.md) | `chore/B-130-coop-state-doc` (in `dev`) | Docs only: read the co-op state map | Not tested |
| [B-133](B-133.md) | `chore/B-133-test-harness` (in `dev`) | Tests only: nothing to play | Not tested |
| [B-132](B-132.md) | `fix/B-132-no-player0-lookups` (in `dev`) | Encounters, battle start and the party wallet use the party leader | Not tested |
| [B-134](B-134.md) | `chore/B-134-effects-baseline-spec` (in `dev`) | Tests only: nothing to play | Not tested |
| [B-135](B-135.md) | `chore/B-135-buildup-defense-grid-specs` (in `dev`) | Tests only: nothing to play | Not tested |
| [B-136](B-136.md) | `chore/B-136-infusion-cost-spec` (in `dev`) | Tests only: nothing to play | Not tested |
| [B-137](B-137.md) | `chore/B-137-economy-spec` (in `dev`) | Tests only: nothing to play | Not tested |
| [B-138](B-138.md) | `refactor/B-138-weapon-mesh-delegate` (in `dev`) | Weapon mesh swaps without a tick | Not tested |
| [B-139](B-139.md) | `refactor/B-139-anim-instance-cleanup` (in `dev`) | Combat stances and infusion glow unchanged | Not tested |
| [B-141](B-141.md) | `chore/B-141-per-hit-logs-verbose` (in `dev`) | A normal fight has a quiet Output Log | Not tested |
| [B-045](B-045.md) | `feature/B-045-effect-text` (in `dev`) | Item text generated from its effects | Not tested |
| [B-090](B-090.md) | `feature/B-090-save-keystone` (in `dev`) | Party and currencies saved between sessions | Not tested |
| [B-178](B-178.md) | `feature/B-178-run-repair` (in `dev`) | Paid gear repair in a run; the run's Gold carries between levels | Not tested |
| [B-093](B-093.md) | `feature/B-093-menu-keys` (in `dev`) | Keys for Defend and infusion (keyboard + gamepad) | Not tested |
| [B-209](B-209.md) | `feature/B-209-memory-sources` (in `dev`) | Memories from bosses, reactions, map spots and bound gear | Not tested |
| [B-226](B-226.md) | `fix/B-226-water-buildup-ice` (in `dev`) | Water status buildup also reads Ice resistance (Q-042o changed) | Not tested |
| [B-042yb](B-042yb.md) | `feature/B-042yb-gas-stat-getters` (in `dev`) | GAS stage 3, part 3d2: with UseGAS 1 damage, speed and pillar stats come from GAS | Not tested |
| [B-042y](B-042y.md) | `feature/B-042y-gas-getters-hp` (in `dev`) | GAS stage 3, part 3d: with UseGAS 1 burns and heal-over-time are paid by GAS | Not tested |
| [B-042x](B-042x.md) | `feature/B-042x-gas-tag-reads` (in `dev`) | GAS stage 3, part 3c: with UseGAS 1 the status and immunity checks read tags | Not tested |
| [B-042o](B-042o.md) | `feature/B-042o-gas-status-bar` (in `dev`) | GAS stage 3, part 5e: with UseGAS 1 the status bar lives on GAS | Not tested |
| [B-042h](B-042h.md) | `feature/B-042h-gas-effect-lifetime` (in `dev`) | GAS stage 3, part 3b: with UseGAS 1 the turn tracker owns how long effects last | Not tested |
| [B-225](B-225.md) | `feature/B-225-effect-timing` (in `dev`) | Effect timing per D-051 / Q-044c / Q-044d: N holder turns, payouts at turn start, Execution turns run the turn-start step | Not tested |
| [B-042n](B-042n.md) | `feature/B-042n-gas-dot-hot-ticks` (in `dev`) | GAS stage 3, part 5d: DOT and HOT ticks on GAS pay what the enum path pays | Not tested |
| [B-042w](B-042w.md) | `feature/B-042w-gas-resistance` (in `dev`) | GAS stage 3, part 5m: the resistances built on GAS from their sources | Not tested |
| [B-042v](B-042v.md) | `feature/B-042v-gas-turn-speed` (in `dev`) | GAS stage 3, part 5l: turn speed built on GAS from the number turn order uses | Not tested |
| [B-042u](B-042u.md) | `feature/B-042u-gas-pools` (in `dev`) | GAS stage 3, part 5k: max HP and max EP built on GAS from the pool layers | Not tested |
| [B-042t](B-042t.md) | `feature/B-042t-gas-last-stats` (in `dev`) | GAS stage 3, part 5j: Mind, Body and Spirit built on GAS from their world base and layers | Not tested |
| [B-042s](B-042s.md) | `feature/B-042s-gas-derived-stats` (in `dev`) | GAS stage 3, part 5i: crit chance and Efficiency derived on GAS from Luck and the cost reduction | Not tested |
| [B-042r](B-042r.md) | `feature/B-042r-gas-luck-reflex` (in `dev`) | GAS stage 3, part 5h: Luck and the Reflex window factor composed on GAS | Not tested |
| [B-042q](B-042q.md) | `feature/B-042q-gas-stat-sources-rest` (in `dev`) | GAS stage 3, part 5g: status multiplier, Defense and crit damage composed on GAS; `wor.Attributes.Compare` checks the composed bases | Not tested |
| [B-042p](B-042p.md) | `feature/B-042p-gas-stat-sources` (in `dev`) | GAS stage 3, part 5f: spell / raw damage and action / spell speed composed on GAS from their gear, stone and buff layers | Not tested |
| [B-042m](B-042m.md) | `feature/B-042m-ignore-defense-amount` (in `dev`) | Ignore Defense is an amount per skill (D-086); IgnoreDefense effects ignore their value | Not tested |
| [B-042l](B-042l.md) | `feature/B-042l-gas-buildup-execution` (in `dev`) | GAS stage 3, part 5b: status buildup maths as one core; buildup execution on GAS (not used in play yet) | Not tested |
| [B-042e](B-042e.md) | `feature/B-042e-gas-damage-execution` (in `dev`) | GAS stage 3, part 5: damage execution on GAS (not used in play yet) | Not tested |
| [B-042k](B-042k.md) | `feature/B-042k-gas-remaining-families` (in `dev`) | GAS stage 3, part 4b: every effect type has a GAS family; the mirror copies them all | Not tested |
| [B-042d](B-042d.md) | `feature/B-042d-gas-combat-events` (in `dev`) | GAS stage 3, part 4: `Event.Combat.*` GameplayEvents sent (nothing listens yet) | Not tested |
| [B-042c](B-042c.md) | `feature/B-042c-gas-effect-mirror` (in `dev`) | GAS stage 3, part 3: effects mirrored onto GAS; `wor.Effects.UseGAS 1` reads them | Not tested |
| [B-042b](B-042b.md) | `feature/B-042b-gas-effect-builder` (in `dev`) | GAS stage 3, part 2: effect context, family GEs, spec builder (nothing uses them in play yet) | Not tested |
| [B-042](B-042.md) | `feature/B-042-gas-turn-tracker` (in `dev`) | GAS stage 3, part 1: Stun GE + turn tracker (nothing uses it in play yet) | Not tested |
| [B-034](B-034.md) | `refactor/B-034-infusion-level-on-pending-action` (in `dev`) | The menu holds the infusion level; the VFX only shows it | Not tested |
| [B-033](B-033.md) | `refactor/B-033-god-file-splits` (in `dev`) | God-file splits (move-only): debug buttons, montage runner, costs | Not tested |
| [B-032c](B-032c.md) | `fix/B-032c-reflect-hp-lost` (in `dev`) | Reflect returns a share of the HP lost; Double Hit skips a killed target | Not tested |
| [B-032](B-032.md) | `refactor/B-032-one-hit-path` (in `dev`) | One hit path: same damage, defense and reflect as before | Not tested |
| [B-080](B-080.md) | `fix/B-080-skill-damage-type` (in `dev`) | An infused ability deals its source's element (Generic + fire crystal = Fire) | Not tested |
| [B-064b](B-064b.md) | `feature/B-064b-dot-element-resistance` (in `dev`) | Element resistance also cuts elemental DoT ticks | Not tested |
| [B-064](B-064.md) | `feature/B-064-element-damage-resistance` (in `dev`) | Element resistance changes elemental damage (+-50% cap) | Not tested |
| [B-217](B-217.md) | `fix/B-217-live-world-levels` (in `dev`) | Requirements count a pillar raised during the run | Not tested |
| [B-219](B-219.md) | `refactor/B-219-damage-math-core` (in `dev`) | Damage maths moved into one core function; no number should change | Not tested |
| [B-162](B-162.md) | `feature/B-162-weapon-requirement-factor` (in `dev`) | Under a weapon's requirement, its attacks and abilities deal less | Not tested |
| [B-218](B-218.md) | `feature/B-218-combat-tuning-data` (in `dev`) | Combat tuning data asset and ladders in curve tables; no number changes without it | Not tested |
| [B-216](B-216.md) | `refactor/B-216-resistance-seam` (in `dev`) | Resistance per damage type; same buildup numbers; optional resistance profile asset | Not tested |
| [B-212](B-212.md) | `feature/B-212-mid-fight-triggers` (in `dev`) | Reactions and boss triggers firing mid-fight (turn, hit, HP line, phase), once each | Not tested |
| [B-085](B-085.md) | `refactor/B-085-owned-gear-truth` (in `dev`) | Loadouts follow the owned gear: wear, level-ups, attaches and dismantles stay in sync | Not tested |
| [B-171](B-171.md) | `feature/B-171-gear-change-drops` (in `dev`) | Taking a crystal off or swapping it drops the spells it powered, in every loadout, with a message | Not tested |
| [B-208](B-208.md) | `feature/B-208-save-trial-run` (in `dev`) | Quitting mid-trial and restarting in the hub picks the run back up: fights, chests, HP, wear and Gold | Not tested |
| [B-091](B-091.md) | `feature/B-091-shared-gear-pool` (in `dev`) | The party shares one armoury: each piece once, one wearer, changes reach everyone | Not tested |
| [B-076](B-076.md) | `feature/B-076-trial-recruits` (in `dev`) | A character found in a trial joins for that run, ungeared, uses armoury gear, and leaves at the end | Not tested |
| [B-203](B-203.md) | `feature/B-203-item-inventory-storage` (in `dev`) | The party carries 10 item slots; the rest goes to storage; move items between them | Not tested |
| [B-124](B-124.md) | `fix/B-124-save-gear-detail` (in `dev`) | A saved weapon or ring keeps its crystal, wear, spells and rolls | Not tested |
| [B-211](B-211.md) | `feature/B-211-boss-triggers` (in `dev`) | Boss triggers: the right character + gear weakens / strengthens / changes a boss | Not tested |
| [B-209b](B-209b.md) | `feature/B-209-memory-conditions` (in `dev`) | Memories by conditions (who, gear, where, what you beat, how, chains) | Not tested |
| [B-210](B-210.md) | `feature/B-210-collected-memories` (in `dev`) | A collected memory is gone for good: its fragment isn't in later runs | Not tested |
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
| [B-042yc](B-042yc.md) | `feature/B-042yc-gas-getters` (in `dev`) | GAS stage 3 part 3d3: resistances, turn speed, Defense, crit, luck, reflex, efficiency and status multiplier read GAS at UseGAS 1; no change at UseGAS 0 | Not tested |
| [B-042z](B-042z.md) | `feature/B-042z-fight-records-on-tracker` (in `dev`) | GAS stage 3 part 3e: fires-once records and armed conditionals live on each fighter's turn tracker (per-fight state on the battle's actors, D-064) | Not tested |
| [B-042j](B-042j.md) | `feature/B-042j-absorb-family` (in `dev`) | Barrier / Shield / Ward absorbers (D-092): spent after Guard and Defense, one charge per impact; `wor.Absorb.Add` to try them | Not tested |
| [B-220](B-220.md) | `feature/B-220-weapon-power` (in `dev`) | Weapons get Power and Mind / Body / Spirit scaling grades (D-101); Power 0 by default, so no damage changes until set; `wor.Weapon.Print` | Not tested |
| [B-221](B-221.md) | `feature/B-221-worn-gear-leaves-inventory` (in `dev`) | Shared pool: lists of gear selectable by a member (worn-by-others filtered out) and the inventory debug tag; the pickers switch with B-221b | Not tested |
| [B-222](B-222.md) | `feature/B-222-pillar-cap-7` (in `dev`) | Mind / Body / Spirit can't pass 7 in a run (was 20): head-start, earned and bought levels clamp at 7; the vendor refuses at 7 | Not tested |
| [B-227](B-227.md) | `feature/B-227-seamless-travel` (in `dev`) | Co-op travel: hub / trial / battle changes use seamless ServerTravel so clients come along; `AWoRGameState`; PlayerState carries currencies over | Not tested |
| [B-131](B-131.md) | `feature/B-131-chardata-replicates` (in `dev`) | Co-op: the character data component replicates (HP / EP and their max, alive, Broken Darkness, the template); only the server derives the max pools | Not tested |
| [B-241](B-241.md) | `refactor/B-241-ai-infusion-level` (in `dev`) | AI picks its infusion level with one helper for spells and abilities; energy-percent reads safe at Max EP 0 | Not tested |
| [B-242](B-242.md) | `refactor/B-242-crystal-wear-formula` (in `dev`) | One crystal / evolution cast-wear formula (`ComputeCastWear`); same wear numbers | Not tested |
| [B-245](B-245.md) | `refactor/B-245-one-action-tier` (in `dev`) | Wear and the tier gap use the same action tier (`ResolveActionTier`); ability and basic-attack wear numbers change (Q-245) | Not tested |
| [B-246](B-246.md) | `refactor/B-246-combat-finish` (in `dev`) | Every way a fight ends runs one finish tail (crystal destruction, repair, resets); a TurnManager-ended fight now runs destruction and repair (Q-246) | Not tested |
| [B-247](B-247.md) | `refactor/B-247-one-heal-path` (in `dev`) | One heal amplify + HealBlock gate for every heal kind; every heal broadcasts `OnHealingDone` | Not tested |
| [B-248](B-248.md) | `refactor/B-248-status-per-element` (in `dev`) | Each element's break name comes from one table (D-061 / D-062); Water's break shows Frozen (D-090); Heal Block shows Heal Blocked | Not tested |
| [B-250](B-250.md) | `refactor/B-250-bundle-flatten` (in `dev`) | One effect-bundle flatten for equipment and evolutions (8 copies before); same effects listed | Not tested |
| [B-251](B-251.md) | `refactor/B-251-gear-templates` (in `dev`) | Inventory add / remove and the owned-gear resolvers for weapons and rings share one template each; same behaviour | Not tested |
| [B-252](B-252.md) | `refactor/B-252-economy-pairs` (in `dev`) | Economy actions (dismantle, level-up, revert, repair) are refused during a fight; one owned-gear lookup by PersistentID | Not tested |
| [B-253](B-253.md) | `refactor/B-253-name-tables` (in `dev`) | Crystal names and tier letters come from the cooked-safe name table (B-126); stones now named properly | Not tested |
| [B-254](B-254.md) | `refactor/B-254-infusion-controls` (in `dev`) | The command menu's infusion level / source controls are one helper for the single and group pickers; same behaviour | Not tested |
| [B-255](B-255.md) | `refactor/B-255-bd-form-check` (in `dev`) | One "is in Broken Darkness form" check (two definitions before); same behaviour | Not tested |
| [B-256](B-256.md) | `refactor/B-256-interact-debug-repeats` (in `dev`) | Shared nearest-interactable search (`WoRInteract`) and debug-opponent helper; same hints and prints | Not tested |
| [B-258](B-258.md) | `refactor/B-258-loadout-by-ref` (in `dev`) | Stat reads stop copying the loadout (const reference); no behaviour change | Not tested |
| [B-262](B-262.md) | `refactor/B-262-per-frame-fixes` (in `dev`) | Per-frame fixes: projectiles move in one call and tick only in flight; the loadout caches its sibling components | Not tested |
| [B-266](B-266.md) | `chore/B-266-profiling-markers` (in `dev`) | CPU profiler markers on the five suspect functions; Crown's profiling run (D-068) | Not tested |
| [B-263](B-263.md) | `refactor/B-263-boss-lookup` (in `dev`) | Boss data found by an asset-registry tag instead of loading every BossData; resave the BossData assets once | Not tested |
| [B-267](B-267.md) | `fix/B-267-sign-aware-cost-speed` (in `dev`) | Energy Cost and Turn Speed modifiers follow D-054: a buff on an ally, a debuff on an enemy; an ally's cost cut makes skills cheaper and survives cleanse | Not tested |
| [B-268](B-268.md) | `fix/B-268-dead-effect-types` (in `dev`) | Dead effect types: Speed Buff / Debuff refused by validation (D-056), Crit Debuff stored as Crit Chance Debuff, Energy / Random Debuff warn and are hidden in the editor | Not tested |
| [B-276](B-276.md) | `refactor/B-276-element-helpers` (in `dev`) | Element helpers (`IsConcrete` / `IsInherit`, one Reality Refractor check); innate element read only for Casters | Not tested |
| [B-277](B-277.md) | `chore/B-277-small-tidy-ups` (in `dev`) | Small tidy-ups: cost-clamp constants named, unused hit-result flags and `EItemQuality` flagged, tier order notes | Not tested |
| [B-282](B-282.md) | `chore/B-282-last-stand-spec` (in `dev`) | Last Stand / Sapphire numbers pinned by a test (50%, the 30% fallback, the 2..5 turn window, the revive) | Not tested |
| [B-283](B-283.md) | `chore/B-283-bonus-turn-spec` (in `dev`) | Emerald bonus-turn delays 6..0 and S giving two turns, and Extra Action, pinned by a test | Not tested |
| [B-284](B-284.md) | `chore/B-284-turn-start-order` (in `dev`) | The order of the turn-start steps and the ritual fire order pinned step by step (swapping two steps fails a test); slow, ~1100 frames per ritual step | Not tested |
| [B-289](B-289.md) | `chore/B-289-turn-speed-tiebreak` (in `dev`) | Turn speed 10 / 15 / 20 and every tie-break step pinned by a test | Not tested |
| [B-285](B-285.md) | `chore/B-285-loadout-budget-spec` (in `dev`) | Loadout budgets (Resonator 5, innate 24 / 48) pinned by a test | Not tested |
| [B-286](B-286.md) | `chore/B-286-break-size-constants` (in `dev`) | Break sizes (Burn 8%, Earth 30%, Water 30%, Reality 25%) as named constants, pinned by a test | Not tested |
| [B-287](B-287.md) | `chore/B-287-broken-darkness-spec` (in `dev`) | Broken Darkness absorption, overload and forbidden-cast numbers pinned by a test | Not tested |
| [B-290](B-290.md) | `chore/B-290-economy-numbers-spec` (in `dev`) | World-stat vendor price and cap, merge, spell essence and drop-quality numbers pinned by a test | Not tested |
| [B-295](B-295.md) | `fix/B-295-emerald-real-extra-turn` (in `dev`) | An Emerald bonus turn is a real extra turn (no later turn lost, D-052) | Not tested |
| [B-296](B-296.md) | `fix/B-296-generic-spell-quartz-price` (in `dev`) | Generic spells cost Quartz essence like elemental ones (D-105) | Not tested |
| [B-300](B-300.md) | `fix/B-300-turn-triggers-every-turn` (in `dev`) | On Turn Start / End triggers fire every turn (not just once) | Not tested |
| [B-298](B-298.md) | `feature/B-298-adapters` (in `dev`) | Player-facing Ring is now Adapter; adapter Form field (D-107) | Not tested |
| [B-301](B-301.md) | `refactor/B-301-gear-instance` (in `dev`) | Unified loadout 1: one gear-instance type (not used yet) | Not tested |
| [B-302](B-302.md) | `refactor/B-302-loadout-slots` (in `dev`) | Unified loadout 2: slot list mirrored beside the old fields; `wor.Loadout.CompareSlots` | Not tested |
| [B-303](B-303.md) | `refactor/B-303-class-table-cost` (in `dev`) | Unified loadout 3: one class table and one slot cost (budget unchanged except Q-303b) | Not tested |
| [B-310](B-310.md) | `fix/B-310-run-wear-by-guid` (in `dev`) | Run wear follows the item's GUID: two identical adapters keep their own wear | Not tested |
| [B-304](B-304.md) | `refactor/B-304-active-slot` (in `dev`) | Unified loadout 4: one active-slot rule; empty slot falls back (Q-304); secondary weapon spell checks | Not tested |
| [B-042yd](B-042yd.md) | `refactor/B-042yd-gas-payouts` (in `dev`) | GAS: energy, drains, self-damage and percent restores pay through GAS at UseGAS 1 | Not tested |
| [B-042ye](B-042ye.md) | `refactor/B-042ye-gas-effect-reads` (in `dev`) | GAS: the AI's burn checks, buff / debuff counts and the panel's effect list read GAS at UseGAS 1 | Not tested |
| [B-249](B-249.md) | `refactor/B-249-element-immunity-map` (in `dev`) | One element -> immunity map (no change in play) | Not tested |
| [B-252b](B-252b.md) | `refactor/B-252b-owned-gear-verbs` (in `dev`) | One body for weapon / ring dismantle, level up, revert (no change in play) | Not tested |
| [B-274](B-274.md) | `refactor/B-274-break-rows` (in `dev`) | Status breaks read one row each; no behaviour change | Not tested |
| [B-275](B-275.md) | `refactor/B-275-keyed-effects` (in `dev`) | Element-keyed immunity and DoT on hit; old members unchanged | Not tested |
| [B-273](B-273.md) | `refactor/B-273-hit-kind` (in `dev`) | Hit-kind struct and Ice tag under Water; no behaviour change | Not tested |
| [B-272](B-272.md) | `refactor/B-272-resistance-columns` (in `dev`) | Resistance columns read by damage type; no behaviour change | Not tested |
| [B-297b](B-297b.md) | `fix/B-297b-bonus-cap` (in `dev`) | Emerald bonus turns go past the 2:1 cap (Q-297 A); no behaviour change | Not tested |
| [B-305](B-305.md) | `refactor/B-305-one-switch-v2` (in `dev`) | One weapon / adapter switch behind both menu buttons; no behaviour change | Not tested |
| [B-306](B-306.md) | `refactor/B-306-gear-visitor-v2` (in `dev`) | One gear rule for stats, resistances and effects (Q-244); two Generic fixes | Not tested |
| [B-307](B-307.md) | `refactor/B-307-infusion-resolver-v2` (in `dev`) | One infusion-source resolver; broken gives no element everywhere (CL-24) | Not tested |
| [B-308](B-308.md) | `refactor/B-308-gear-save-v5` (in `dev`) | Save v5: one gear list; older saves still load | Not tested |
| [B-311](B-311.md) | `feature/B-311-custom-loadouts-saved` (in `dev`) | Your own loadouts (up to 5 per character) are kept and saved; the character's own loadouts stay as authored | Not tested |
| [B-321](B-321.md) | `feature/B-321-per-character-gear` (in `dev`) | Each character has its own gear; an old save gives each piece to its wearer, else the leader | Not tested |
| [B-279b](B-279b.md) | `feature/B-279b-form-skins` (in `dev`) | Wear an unlocked character's skin (hub / rest spot); its bound gear works; Manny or Quinn | Not tested |
| [B-279d](B-279d.md) | `feature/B-279d-form-minibosses` (in `dev`) | An encounter can field a character form as its mini-boss (its look, loadout, phases) | Not tested |
| [B-297](B-297.md) | `fix/B-297-turn-cap` (in `dev`) | 2:1 turn cap: a fast combatant gets at most 2 turns per opponent turn | Not tested |
| [B-271](B-271.md) | `refactor/B-271-element-table` (in `dev`) | One element table behind the element switches; no behaviour change | Not tested |
| [B-279](B-279.md) | `feature/B-279-character-forms` (in `dev`) | Character-form meter fills on a won fight, unlocks at 100%, survives save | Not tested |
| [B-269](B-269.md) | `fix/B-269-essence-names` (in `dev`) | Essence names in the shop: Quartz Essence, Unknown Essence | Not tested |
| [B-265](B-265.md) | `chore/B-265-build-time` (in `dev`) | Header includes trimmed; `EWorldPillar` in its own header (Blueprint check) | Not tested |
| [B-264](B-264.md) | `chore/B-264-log-levels` (in `dev`) | Output Log quiet during a normal fight (per-turn lines at Verbose) | Not tested |
| [B-260](B-260.md) | `refactor/B-260-light-attribute-sync` (in `dev`) | Lighter GAS attribute sync on hits; attributes still match | Not tested |
| [B-259](B-259.md) | `refactor/B-259-ai-estimate-memo` (in `dev`) | AI damage estimates cached within one decision; same AI choices | Not tested |
| [B-256b](B-256b.md) | `refactor/B-256b-interact-hint` (in `dev`) | One interact-hint helper; shared crystal / BD debug preambles; same hints and prints | Not tested |
| [B-319](B-319.md) | `feature/B-319-duplicate-choice` (in `dev`) | A duplicate weapon / adapter: weaker copy to Gold or dismantled (`wor.UI.Duplicates`) | Not tested |
| [B-320](B-320.md) | `fix/B-320-always-a-primary` (in `dev`) | A loadout's primary can't be dismantled; an empty active primary is filled (D-116) | Not tested |
| [B-291](B-291.md) | `feature/B-291-headless-seams` (in `dev`) | Balance-sim seams: InstantImpacts / AI think-delay console variables (off by default), round and bar-break events | Not tested |
| [B-294](B-294.md) | `feature/B-294-element-palette` (in `dev`) | Colour-blind element palette: new element colours in bars, tints and glows | Not tested |
| [B-292](B-292.md) | `feature/B-292-sim-runner` (in `dev`) | Balance sim runner, recorder, archetypes (tests only; nothing in play changes) | Not tested |
| [B-372](B-372.md) | `feature/B-372-camera-framing-checks-cloud` (UNBUILT, on B-371) | Camera framing checks; screenshots per shot pattern for Crown | Pending |
| [B-348c](B-348c.md) | `feature/B-348c-broken-darkness-persists-cloud` (UNBUILT, on B-348b) | Broken Darkness stays across fights, runs and the save; strain builds across the run | Pending |
| [B-351d](B-351d.md) | `feature/B-351d-crystal-spell-lists-cloud` (UNBUILT, on B-351c) | A crystal's one spell list: replace / restore, saved on the item | Pending |
| [B-357b](B-357b.md) | `feature/B-357b-evolution-resolve-path-cloud` (UNBUILT, on B-357) | One list of attached evolutions (character, weapon, adapter); no change in play | Pending |
| [B-357c](B-357c.md) | `feature/B-357c-gear-evolutions-passives-only-cloud` (UNBUILT, on B-357b) | An evolution on gear gives no stats or resistance (D-147 play change) | Pending |
| [B-187b](B-187b.md) | `feature/B-187b-mini-boss-rest-spots-cloud` (UNBUILT, on B-187) | A rest spot where a mini-boss fell; revive it one tier stronger (3 times) | Pending |
| [B-187c](B-187c.md) | `feature/B-187c-revive-return-and-save-cloud` (UNBUILT, on B-187b) | Back at the rest spot after a revive; revivals saved (save v24) | Pending |
| [B-234c](B-234c.md) | `feature/B-234c-planned-host-hand-over-cloud` (UNBUILT, on B-234b + B-187) | Hand the host over at a rest spot; the old host rejoins | Pending |
| [B-187d](B-187d.md) | `feature/B-187d-co-op-rest-cloud` (in dev) | 2-player: a client rests the party; everyone returns beside the spot after a revive | Pending |
| [B-187e](B-187e.md) | `feature/B-187e-client-rest-menu-cloud` (in dev) | 2-player: a client's Revive asks the host; Leave leaves the session | Pending |
| [B-235e](B-235e.md) | `feature/B-235e-own-memory-finds-cloud` (UNBUILT, on B-235d) | 2-player: the client's own spot, fragment, gear and reaction memories are theirs; a friend's PriorMemory reads their own memories | Pending |
| [B-375](B-375.md) | `feature/B-375-memory-debug-cloud` (UNBUILT, on B-235e) | `wor.Memory.Why <Id>`: each condition met / not yet per player, who would get it | Pending |
| [B-376](B-376.md) | `feature/B-376-party-has-cloud` (UNBUILT, on B-375) | Pair memories: both characters present, solo or across players | Pending |
| [B-377](B-377.md) | `feature/B-377-hidden-quests-cloud` (UNBUILT, on B-376, save v25) | Hidden quests: start, steps across runs, end memory | Pending |
| [B-388](B-388.md) | `feature/B-388-main-side-quests-cloud` (in dev) | Main and side quests: listed in the journal with their goal | Pending |
| [B-378](B-378.md) | `feature/B-378-curses-cloud` (in dev) | Curses: given by a memory, a key, lifted, kept across runs | Pending |
| [B-378b](B-378b.md) | `feature/B-378b-curse-penalty-cloud` (in dev) | A held curse weakens your own characters by its penalty | Pending |
| [B-379](B-379.md) | `feature/B-379-hidden-counters-cloud` (UNBUILT, on B-378, save v27) | Hidden counters: parries and kills across runs, a milestone memory | Pending |
| [B-381](B-381.md) | `feature/B-381-gear-quests-cloud` (in dev) | A worn item's hidden quest starts while its condition holds | Pending |
| [B-377b](B-377b.md) | `feature/B-377b-coop-quests-journal-save-cloud` (in dev) | The journal saved; a friend's quests move on the host's events | Pending |
| [B-380](B-380.md) | `feature/B-380-apparel-cloud` (UNBUILT, from dev) | Apparel adds HP and Defence outside the budget; `wor.Loadout.Print` | Pending |
| [B-380b](B-380b.md) | `feature/B-380b-apparel-store-save-cloud` (UNBUILT, on B-380) | Owned apparel in the store, saved with its socket | Pending |
| [B-380c](B-380c.md) | `feature/B-380c-apparel-slots-saved-cloud` (UNBUILT, on B-380b, save v29) | Worn apparel saved; a crystal set in play | Pending |
| [B-357d](B-357d.md) | `feature/B-357d-gear-evolution-l2-passives` (in dev) | A gear evolution's passives act on an L2 action from its item (apparel: any L2 action), for that action only | Pending |
| [B-397](B-397.md) | `feature/B-397-hidden-quests-shown` (in dev) | A found hidden quest is announced to its player and listed with its current goal only | Pending |
| [B-405](B-405.md) | `feature/B-405-boss-framing-kill-shots` (in dev, built 2026-11-12) | A boss or mini-boss turn view sits low and looks up; a won fight holds on a kill shot before the rewards (normal: wide and darkened ~1.5 s; boss: close, breaks apart, ~5 s); `wor.Camera.KillShot 0/1` | Pending |
| [B-403](B-403.md) | `feature/B-403-shot-pattern-tuning` (in dev, built 2026-11-12) | Shot-pattern feel: a melee confirm rushes in with blur, a normal hit is a low side-on two-shot with a small punch, an area skill's pull-out holds still; compare with the study | Pending |

Move rows to **Done** when every check passes.

## Done
| Job | Tested | Notes |
|---|---|---|
| [B-026](B-026.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
| [B-027](B-027.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
| [B-029](B-029.md) | 2026-10-26 | General play-through, nothing looked wrong (Crown) |
