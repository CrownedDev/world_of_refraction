# Design Pillars — what kind of game this is

**Crown's words, 2026-10-26.** Every default (`CLAUDE.md`, "Approve by default") is judged against these. If a choice works against a pillar, stop and ask Crown.

## The pillars

1. **Immersive, like Expedition 33, built on Crown's own story.** Presentation, characters and lore carry the experience; the world should feel authored, not generic.
2. **Your character is yours.** Players build a **unique** character through the **skills, equipment and stats** they pick. Build choices must matter and feel distinct; avoid one "correct" build.
3. **Challenging but fair** (Expedition 33, Hades): careless play is punished, but the game gives tools and second chances. **Difficulty settings** let players choose.
4. **One chapter of a bigger story.** Crown has great ambitions for the whole story; this game is **a small section of it**. Systems and lore should leave room to grow (new characters, eras, trials) without rework.

5. **Short runs, about 30–45 minutes** (Crown leaning, "maybe A"; Nightreign's ~40-minute expeditions). One run fits an evening; trial maps, fight counts and in-run progression are sized to it.

6. **Progression that never locks you out** (a blend of light and moderate). Levelling Mind, Body and Spirit makes you stronger, but **a low-level character can still have a real chance against a max-level one** through gear and build: equipment that grants Mind/Body/Spirit points (which lift every stat under that pillar), stones, crystals, evolutions and the scaling rules. And the reverse: a high level with poor gear can be caught out. This is why the game has so many stat sources (`Resources_Design.md` §7.4: gear vs world-stat split; `Mechanics/Scaling/`).

7. **Solo and co-op equally** (Crown, 2026-10-26): designed to work just as well alone (your party of three, ally AI) and together (players joining runs, invites at rest spots), not solo with co-op bolted on, and not co-op first like Nightreign (whose solo mode was criticised at launch).

8. **Combat depends on playstyle** (Crown, 2026-10-26, revised): **players can lean on tactics** (elements, resistances, rows, status breaks, builds, party roles) **or on timing** (Guard, Perfect Guard, Dodge), and both should be viable ways to win. **Bosses have their own styles**: some test planning, some test timing, some both.
9. **PC and consoles** (Crown, 2026-10-26): planned from the start, so **full gamepad support** and console-friendly UI (CommonUI) throughout; keyboard and mouse on PC.

## What this means for decisions

- Prefer options that **add build variety** (more ways to be different) over options that narrow it, as long as they stay fair (pillars 2, 3).
- Prefer **readable, satisfying feedback** (Expedition 33's parry, cinematic skills) over hidden numbers (pillars 1, 3).
- Prefer **second chances** (revives, rest spots, fleeing) over run-ending punishment, at the default difficulty (pillar 3).
- Keep content **data-driven and extendable** (trials, characters, memories, reaction triggers) so later chapters plug in (pillar 4).
- Size trials for a **30–45 minute run** (pillar 5): map size, number of fights, rest spots and in-run levelling all fit that.
- **No level wall** (pillar 6): keep gear and builds able to close a level gap, so stat sources stay meaningful next to levels; each run still moves you forward.
- **Every system must work solo and co-op** (pillar 7): design both cases when building (who controls what, rewards, rest spots, revives), even though co-op is built later; keep to the co-op-ready rule (D-064).
- **Two ways to win** (pillar 8): a tactics-leaning build and a timing-leaning player should both be able to clear normal content; vary bosses by style rather than making every fight demand the same skill. Hard timing windows belong to timing-focused bosses and higher difficulty settings.
- **Gamepad first-class** (pillar 9): every screen and action works on a controller; respect console performance (D-068 frame budget).
- Lore and story are Crown's alone (`CLAUDE.md`).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created from Crown's answers | docs/design-pillars |
