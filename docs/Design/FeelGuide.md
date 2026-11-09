# Feel Guide — how each moment should play

**Crown's answers, 2026-11-04** (brain session interview). Every polish job checks against this page, as gameplay rules check against `DesignPillars.md`. The look itself is `VisualDirection.md` (Rematch-style painterly); this page is feel and flow. Numbers marked *starting value* are first guesses for tuning, not rulings.

## The moments

| # | Moment | Crown's answer | Reference | Starting values / never |
|---|---|---|---|---|
| 1 | **Fight start** | Touch the enemy in the world and you're in the fight. Hitting it first gives you the first turn. The transition is a **crystal break**: the screen shatters like crystal into the battle. | Expedition 33's seamless entry | In the fight within 1–2 s (*starting value*). Never a long load or a separate "battle start" screen. |
| 2 | **Turn pace and camera** | The camera **follows the acting character**. No deep-dive cinematic on every action, no cut-ins. A skill's only cinematic part is its **cast**: two marks in its animation (cast start, cast end, an anim-notify window) switch to a cinematic camera for that stretch, then back to the follow camera. **No battle-speed setting**: animation and projectile speed are gameplay (Action Speed / Spell Speed narrow the defense window, D-151), so tempo carries information. | Final Fantasy X / Persona 5 follow camera | Only moments with no defense timing (victory, your own buffs, menus) may be shortened later. |
| 3 | **Perfect defense** | Subtle: a crisp clang and sparks, **no pause**; the flow keeps going. | Sekiro's deflect | Never a hit-stop freeze on a defense. |
| 4 | **Your hits** | Every hit has weight: a tiny freeze, a light shake, a flinch and the damage number. **Weakness hits, criticals and status breaks scale it up**: longer freeze, bigger shake and number. | Expedition 33 / Persona 5 weakness hits | Normal hit freeze ~40 ms, big hit ~100 ms (*starting values*). |
| 5 | **Battle screen** | Minimal but readable. HP, EP and the turn order are always on screen. **Weakness is shown by the damage-number colours, not icons**, so you find it by hitting. Statuses are small icons on the character, plus one button to view every status and detail. | Expedition 33's clean HUD | Never weakness icons on enemies. Hidden conditions stay hidden (pillar 10). |
| 6 | **Winning** | Normal fights end fast. Level-ups, rare drops and bosses get a short pause and a victory pose. **No results screen**: back in the world, reward popups appear and fade on their own; details are in the inventory. | Expedition 33's quick return | Never a click-through results screen. |
| 7 | **Losing** | Rules in `DesignDecisions.md` (Crown, 2026-11-04): a wipe costs run pillar points (up to 1 to a normal enemy, up to 2 to a mini-boss, 0 to the main boss). You come back at the **last rest spot used**, with the enemy still there. Three losses to the main boss, or **five wipes in total**, end the run and send you to the hub. | Elden Ring / Expedition 33 respawn; Hades' Death Defiance | — |
| 8 | **Exploring** | Guided routes with side branches, hidden chests and optional fights; light climbing and jumping. Side branches are where hidden conditions live. | Expedition 33 | — |
| 9 | **Bosses** | A boss is a tougher engagement with its phases. **Cinematics are per-boss opt-in, on trial** (Crown, 2026-11-09): a boss's data can turn on a short cinematic opening and phase-change cinematics; try them and judge them in play. Otherwise no flashy staging. Lore comes through item and skill descriptions and hidden quests. **Player and team spells** each have their own camera, but **deliberately less cinematic than a boss's**, to keep the pace: short, no long sequences; signature shots stay brief. | Nightreign; Expedition 33 boss openings and phase changes (`analysis/MaelleCombatPresentationStudy.md` R17/R18) | Off unless the boss's data opts in. Never a long sequence on a player or team spell. |
| 10 | **Zone travel** | Seamless where possible: a short gate or a fade under 1 s with the loading hidden. A **lore loading screen** (item or character descriptions) only for big jumps, like hub to trial. | Elden Ring's loading-screen lore | — |
| 11 | **Hub** | A walkable home base: talk, shop, prepare; it changes as the story moves on. It has **its own secrets** to explore and find. | Hades' House of Hades | — |
| 12 | **Music** | **Parked** (Crown, return later). Idea: music as a cosmetic, tied to the combat weather, shifting when you're winning or losing. | — | — |
| 13 | **Battle menu** | Face-button shortcuts: each button is a category (attack, skills, items); two presses to act. | Expedition 33's button menu | Gamepad first (pillar 9). |
| 14 | **Telegraphs** | Learnable, and they belong to the **skill, ability or spell**, not to the enemy: no one is unique in what they can use, so every user of a skill, ability or spell winds it up with the same readable animation, sound and cast marks. Players learn the move, and that knowledge carries to every enemy and ally who uses it. A death should teach you something. | Souls games and Expedition 33's readable wind-ups | Never an unreadable or silent wind-up on a defendable hit. |

**Camera reference (Crown, 2026-11-08):** Expedition 33, **Maelle vs Painted Maelle**. A mirror match, so one fight shows every camera moment: the attack, the enemy attack on you (the Defence view: an angle that lets you see the attack properly), the parry, the counter. Study what the camera does at each moment and build our own rules (B-370, B-371, B-157): camera behaviour only, never copying its shots, scene or assets. Hand-made cinematic sequences are optional, kept for big moments (ultimates, boss finishers); normal skills use the code-built shot patterns and the Defence view.

## How it's used

- **Vertical slice first:** one fight built to this page end to end (entry, turns, hits, defense, victory, return). Crown plays it and gives notes; each note becomes a row or a value here. That fight is the template for the rest.
- **Play-test notes** from Crown go into the table above in the same change, so a note is never repeated.
- A polish job that can't meet a row raises a `Q-` entry instead of guessing.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-04 | Created from Crown's feel interview | docs/feel-guide |
| 2026-11-08 | Camera reference (Maelle vs Painted Maelle) and optional sequences (Crown, via the brain) | feature/B-370-cinematic-window-cloud |
| 2026-11-09 | Rule 9: boss cinematics (opening, phase change) become per-boss opt-in, on trial; player and team spells get their own short camera, less cinematic than a boss's (Crown) | docs/feelguide-boss-cinematics |
