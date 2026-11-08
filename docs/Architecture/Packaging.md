# Packaging

## Overview
A packaged build cooks only what the maps and their assets reference. This game finds many data assets at runtime by an asset-registry scan or a fixed path, so nothing references them; without rules they would be missing from a packaged build. B-278 adds the rules and the PSO (shader pipeline) cache steps so first fights don't stutter.

## Architecture
- **Asset Manager rules** (`Config/DefaultGame.ini`, `[/Script/Engine.AssetManagerSettings]`): one `PrimaryAssetTypesToScan` entry per type, `CookRule=AlwaysCook`, scanning `/Game`:

  | Type | Why nothing references it |
  |---|---|
  | `StoryLinesData` | `StoryUnlocks::GatherStoryData` scans the registry |
  | `BossData` | `BossData.cpp` scans the registry |
  | `ReactionTriggerData` | `ReactionTrigger.cpp` scans the registry |
  | `MemoryData` | `MemoryConditions.cpp` scans the registry |
  | `ResistanceProfileData`, `CombatTuningData` | loaded by a fixed path |
  | `TrialData` | picked by the hub, saved by id |
  | `CharacterForm` | `UCharacterFormSubsystem` lists the forms by type (`GetPrimaryAssetIdList`) |
  | `WeaponData`, `RingData`, `EvolutionItemData`, `SpellData`, `AbilityData` | `TrialLoot::GatherPool` scans every equipment and skill asset for its pool tags |

  The other types (`CharacterData`, `InventoryData`) stay `Unknown` (cooked when referenced).
- **Packaging settings** (`[/Script/UnrealEd.ProjectPackagingSettings]`): `MapsToCook` lists the five levels (travel opens them by name: `HubLevel`, the trials' `Level` / `EncounterLevel`); `DirectoriesToAlwaysCook` the player-form meshes `AWoRPlayerState` loads by path.
- **Check:** `WoR.Packaging.CookRules` (`Tests/CookRulesSpec.cpp`) fails when a listed type has no AlwaysCook rule. A new scanned or path-loaded type goes in that list and in `DefaultGame.ini`.

## PSO cache (desktop step)
UE 5.8 precaches PSOs at load by default (`r.PSOPrecaching`), which removes most first-use hitches. If a packaged Development build still hitches on the first fight's effects, record a bundled cache:
1. Package Development with **Share Material Shader Code** and **Shared Material Native Libraries** on (Project Settings > Packaging).
2. Run the packaged game with `-logPSO`, play hub → trial → fight (cast every element once), quit.
3. Copy the `.upipelinecache` files from `Saved/CollectedPSOs` and the `.shk` files from the cook output into `Build/Windows/PipelineCaches/`, then package again.
4. In-game, `r.ShaderPipelineCache.Enabled 1` loads it (set in `DefaultEngine.ini [SystemSettings]` once a cache exists).

## Integration points
- `UAssetManager` (engine): reads the rules at cook and at runtime.
- Every registry scan listed above.

## Known limitations / TODOs
- `/Game/Testing` assets are cooked too (the rules scan `/Game`); a Shipping build should exclude them (`DirectoriesToNeverCook`) once the test actors are retired (D-067).
- The registry scans load every asset of a type synchronously (`TrialLoot` TODO): fine at the current counts.
- New maps must be added to `MapsToCook`.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | B-278 (CC part): AlwaysCook rules for the scanned / path-loaded types, `MapsToCook`, the player-form mesh directory, `CookRulesSpec`, the PSO steps. The packaged run is the desktop step. | `feature/B-278-asset-manager-rules-cloud` (UNBUILT) |
