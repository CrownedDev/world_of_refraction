# Character Forms

## Overview

An authored character the player can unlock as a skin (`docs/Design/CharacterForms.md`). Each form has a **loadout-match meter**: every fight won adds credit for the pieces of the form's reference loadout the party wore. At 100% the form unlocks. Skins, outfits and the mini-boss use come with B-279b to B-279d.

## Architecture

| Class | Role |
|---|---|
| `UCharacterFormData` (`Character/Forms/CharacterFormData.h`) | `UPrimaryDataAsset`, asset type `CharacterForm` (scanned via `DefaultGame.ini`). `FormName`, `Character` (its `Inventory` is the reference loadout), `FormMesh` / `FormMaterials` (soft refs, D-057), `LoreIds`. `GetReferencePieces()`; Details-panel `PrintForm`. |
| `CharacterFormMeter` (same header) | `GetFightFill(Reference, Equipped)` = `FULL_SET_FILL_PER_FIGHT` x matched share. Each reference piece counts once: exact tier (or no tier) = 1, other tier = `OTHER_TIER_CREDIT`. Both tunables are `// TODO: tune`. |
| `UCharacterFormSubsystem` (`Character/Forms/CharacterFormSubsystem.h`) | `UGameInstanceSubsystem`. Loads every `CharacterForm` asset at start. `RecordFightWon(Fighters)` gathers each fighter's active-loadout pieces and adds the **best** fighter's fill to each locked form, capped at 1. `GetProgress`, `IsUnlocked`, `OnFormUnlocked`. |
| `UCharacterFormDebug` | `wor.Forms.Print`: every form and its meter. |

Progress is account state like the pool (saved), not per-fight state, so it sits on a game-instance subsystem for now; it moves to the player state with the co-op plumbing.

## Integration Points

- `ACombatOrchestrator::FinishCombat`: on `Victory`, `RecordFightWon(LocalPartyCombatants)` before the party arrays empty.
- `USaveSubsystem`: `FWoRSaveData::Forms` (`FSavedForms`, progress by form asset name); save version 4. Older saves load with empty meters.
- Pieces gathered per slot: gear asset at instance tier, the gear's spells, the slot's spell overrides and abilities.

## Known Limitations / TODOs

- Evolutions aren't on loadout slots yet, so they are left out of the reference too (both sides count the same pieces).
- Progress is saved by form asset name: renaming a form asset drops its saved meter.
- Every form loads at game start (`TryLoad`); move to an async load if the form count grows.
- Skills match by asset only (no instance tier yet). Who counts and how tiers credit: Q-279.
- No UI shows the meter yet; `wor.Forms.Print` and the `OnFormUnlocked` log are the way to see it.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-279: form asset, meter math, meter subsystem with victory hook, `wor.Forms.Print`, save (version 4); spec `WoR.Character.Forms` | feature/B-279-character-forms |
