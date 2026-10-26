# Memory System

## Overview
Memories are the story the player collects in trials (RunStructure §5). A memory fragment on a trial map unlocks a memory: a scene, lore, later dialogue or a quest. The same memory can be deep for one lord and minor for another, and it can change a boss fight. There is no hint: the player learns what a fragment does when it plays. B-190 builds the data, the pickup and the record; the journal screen, V's lines and conversations come in B-194, boss changes in B-191 / B-193.

## Architecture
- **`UMemoryData`** (`Public/Memory/MemoryData.h`, `UPrimaryDataAsset`): `MemoryId` (empty: the asset's name, `GetMemoryId`), `Title`, `Text` (placeholders), `CharacterDepth` (`TMap<UCharacterData, EMemoryDepth>`: None / Minor / Deep; `GetDepthFor`), `BossModifiers` (`FMemoryBossModifier`: boss + Weaken / Strengthen / SkipPhase / AddPhase). Debug: `GetMemoryString`, the `PrintMemory` editor button.
- **`AMemoryFragment`** (`Public/Memory/MemoryFragment.h`): a trial map feature (`ATrialMapFeature`, B-188) holding one memory. Interact adds it to the run and the journal, records it in the run's found list, shows "You found a memory fragment." and hides itself with collision off. Taken earlier in the run, it hides itself on `BeginPlay` (the trial level reloads after each fight).
- **`UTrialRunSubsystem::CollectMemory`** / `GetRunMemories`: this run's memories (run state, cleared on trial entry and exit; moves for co-op with the rest of the run state, D-065).
- **`UMemoryJournalSubsystem`** (`UGameInstanceSubsystem`): every memory collected, across runs, in order. `RecordMemory`, `HasMemory`, `GetMemories`, `GetJournalString`.

## Integration Points
- The exploration controller's interact key reaches fragments through `ATrialMapFeature::FindNearestInRange`.
- B-191 reads `BossModifiers` and the run's memories at encounter start; B-194 keys its data tables by `GetMemoryId`.
- Debug: `wor.Memory.Print` (this run's memories, then the journal with depths); `wor.Trial.Print` counts the run's memories.

## Known Limitations / TODOs
- The journal isn't saved: it lasts while the game runs (TODO B-090).
- Nothing reads `BossModifiers` or the depth yet (B-191, B-193, B-194).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | B-190 (Q-190): `UMemoryData`, `AMemoryFragment`, `UMemoryJournalSubsystem`, run memories; spec `WoR.Memory.Fragment`. | feature/B-190-memory-fragments |
