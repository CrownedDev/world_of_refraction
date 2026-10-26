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
