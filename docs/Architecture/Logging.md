# Logging

## Overview

Project log categories, so the Output Log can be filtered per system instead of everything landing in `LogTemp`. Declared in `Public/WoRLog.h`, defined in `Private/WoRLog.cpp`.

## Architecture

| Category | Use for |
|---|---|
| `LogWoR` | General, module-level, test helpers |
| `LogWoRCombat` | Turn flow, orchestrator, actions, damage |
| `LogWoREffects` | Skill effects, status buildup, Broken Darkness |
| `LogWoRAI` | AI decisions and scoring |
| `LogWoRLoadout` | Loadouts, inventory, equipment, crystals |
| `LogWoRUI` | Widgets, HUD, menus |
| `LogWoREconomy` | Currency, shop, merchant, dismantle/upgrade |

All are `Log` verbosity by default, compile-time `All`, and exported with `WORLD_OF_REFRACTION_API`.

**Rule:** new code uses these categories. An existing file moves its `LogTemp` calls over when it is touched for other work; there's no bulk migration. At creation, about 83 files still used `LogTemp`.

## Integration Points

- Include `WoRLog.h`.
- Raise or lower a category at runtime with the console command `Log <Category> <Verbosity>`, e.g. `Log LogWoRCombat Verbose` or `Log LogWoRAI Warning`.

## Known Limitations / TODOs

- The category-to-system mapping (Broken Darkness under Effects, crystals under Loadout) is a reading of the B-006 backlog line, not a recorded decision.
- No per-category default verbosity in `DefaultEngine.ini` yet.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created: 7 categories; first user is the test-world helper (B-006) | `chore/B-006-log-categories` |
| 2026-10-27 | B-141: routine per-hit / per-turn lines move to `Verbose` (TierGap, defense-window close, action validation, infusion VFX, Broken Darkness checks and strain, combat-menu traces) on `LogWoRCombat` / `LogWoRUI`; Broken Darkness events (break, transform, revert, overload, forbidden cast) go from Warning to `Log`. Error paths unchanged. | `chore/B-141-per-hit-logs-verbose` |
| 2026-11-02 | B-266 (D-068): `TRACE_CPUPROFILER_EVENT_SCOPE` markers on stat reads, attribute sync, AI decision, preview damage and the effect mirror, for Unreal Insights. | chore/B-266-profiling-markers |
| 2026-11-03 | B-264 (P7): per-action / per-hit / per-turn `Log` and `Display` lines in the fight path (orchestrator, turn manager, action executor and montage, effects, buildup, AI, command menu, grid, items, projectiles, defense, BD, loadout, crystals, anim instance, infusion VFX, weapon mesh) moved to `Verbose`. Still at `Log`: combat start / end / win, flee, take-over, deaths / resurrections, BD break reason / transform / revert / overload, status breaks, crystal / evolution / ring breaks, combat-end crystal loss / repair, subsystem init. Debug commands and `*TestActor`s unchanged. See them with `log LogTemp Verbose` (or the `LogWoR*` category). | chore/B-264-log-levels |
| 2026-11-07 | B-349 (D-139, Q-349): every console command `wor.<System>.<Verb>`; forty flat names renamed (table in `docs/PIE/B-349.md`), `wor.PrintCombatTuning` removed as a duplicate of `wor.Tuning.Print`; PIE checklists, Architecture, Rules and Design docs updated. Unbuilt (cloud pre-write). | chore/B-349-console-names-cloud |
