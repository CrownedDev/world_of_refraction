# Memory System

## Overview
Memories are the story the player collects in trials (RunStructure §5). A memory fragment on a trial map unlocks a memory: a scene, lore, later dialogue or a quest. The same memory can be deep for one lord and minor for another, and it can change a boss fight. There is no hint: the player learns what a fragment does when it plays. B-190 builds the data, the pickup and the record; B-194 adds the hub journal, V's lines and conversations, unlocked by the same memory ids; B-191 / B-193 apply the boss changes.

## Architecture
- **`UMemoryData`** (`Public/Memory/MemoryData.h`, `UPrimaryDataAsset`): `MemoryId` (empty: the asset's name, `GetMemoryId`), `Title`, `Text` (placeholders), `CharacterDepth` (`TMap<UCharacterData, EMemoryDepth>`: None / Minor / Deep; `GetDepthFor`), `BossModifiers` (`FMemoryBossModifier`: boss + Weaken / Strengthen / SkipPhase / AddPhase). Debug: `GetMemoryString`, the `PrintMemory` editor button.
- **`AMemoryFragment`** (`Public/Memory/MemoryFragment.h`): a trial map feature (`ATrialMapFeature`, B-188) holding one memory. Interact adds it to the run and the journal, records it in the run's found list, shows "You found a memory fragment." and hides itself with collision off. Taken earlier in the run, it hides itself on `BeginPlay` (the trial level reloads after each fight).
- **`UTrialRunSubsystem::CollectMemory`** / `GetRunMemories`: this run's memories (run state, cleared on trial entry and exit; moves for co-op with the rest of the run state, D-065).
- **`UMemoryJournalSubsystem`** (`UGameInstanceSubsystem`), across runs:
  - every memory collected, in order: `RecordMemory`, `HasMemory`, `HasMemoryId`, `GetMemories`;
  - lore objects read: `RecordLore` / `GetLore` (`FJournalLoreEntry`, once per object, from `ATrialLoreObject::Interact`);
  - V lines heard: `MarkVLineHeard` / `HasHeardVLine`;
  - conversations played to the end: `MarkConversationPlayed` / `HasPlayedConversation`.
  - Debug: `GetJournalString`.
- **`UStoryLinesData`** (`Public/Story/StoryLinesData.h`, `UPrimaryDataAsset`, any number, gathered by the asset registry and sorted by path):
  - `VLines` (`FVLine`: `LineId`, `RequiredMemories` ids, `Text`) and `Conversations` (`FStoryConversation`: `ConversationId`, `Title`, `Speakers`, `RequiredMemories`, `Lines`).
  - `StoryUnlocks::AreUnlocked`, `FindNextVLine` (the first unlocked line not heard yet), `GetUnlockedConversations`.
  - Debug: `PrintStory`, `wor.Story.Print`.
- **`UMemoryJournalWidget`** (`Public/UI/Hub/MemoryJournalWidget.h`, PLACEHOLDER UI, D-024):
  - Journal page: memories, lore and played conversations to reread.
  - Talk to V: the next line. Conversations: cycle the unlocked ones; Next plays them line by line.
  - Opened by **`AHubJournalStation`** (a map feature for the hub) or `wor.Journal.Open`.

## Integration Points
- The exploration controller's interact key reaches fragments through `ATrialMapFeature::FindNearestInRange`.
- B-191 reads `BossModifiers` and the run's memories at encounter start; B-194's story assets name memories by `GetMemoryId`.
- Debug: `wor.Memory.Print` (this run's memories, then the journal with depths); `wor.Trial.Print` counts the run's memories.

## Known Limitations / TODOs
- The journal isn't saved: it lasts while the game runs (TODO B-090).
- Nothing reads the character depth yet.
- A story phase unlocking the next (HubAndStory §6) isn't modelled; conversation speakers aren't checked against the party; the station shows no "new" marker.
- The lore object's journal hook (`ATrialLoreObject::Interact`) has no spec; the journal's `RecordLore` has. The placeholder journal closes only with its Close button.
- `UStoryLinesData` assets must be in the Asset Manager's rules for packaged builds (same TODO as `TrialLoot`).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | B-209 Crown's additions (Q-209c): `FMemoryConditionSet` / `AlternativeConditionSets` (any route opens it; `GetConditionSets`, `HasConditions`), `FMemoryReward` (`EMemoryRewardKind` Item / Skill / Title / Curse) / `Rewards`, logged once by `MemoryUnlock::Unlock` (grants TODO). | feature/B-209-condition-sets-rewards |
| 2026-10-27 | B-211: memories no longer change bosses (D-100 part 1); `UMemoryData::BossModifiers` is obsolete, unread, tagged so in `GetMemoryString`, kept until Crown's PIE. | feature/B-211-boss-triggers |
| 2026-10-27 | B-209 rewritten (D-100 part 2, Q-209b): memories by conditions. `FMemoryCondition` (`EMemoryConditionKind`: Character, GearEquipped, Location, EnemyBeaten, NoDamageTaken, PriorMemory, Hidden) in `UMemoryData::UnlockConditions`; `MemoryConditions` (`FMemoryEvent`, `MakeEvent`, `MakeMember`, `IsMet`, `AreMet`, `GatherConditional`, `CheckEvent` with chains, `wor.Memory.Conditions`, and `wor.Memory.Print` lists them). Events: fragment pickup, map spot, gear upgrade, fight won (`ABattleGameMode::CheckFightWonMemories`, `PartyStartHP`). `EMemorySource::FightWon`. Spec `WoR.Memory.Conditions`. | feature/B-209-memory-conditions |
| 2026-10-27 | B-210 (Q-190 changed by Crown): a collected memory is gone for good. `MemoryUnlock::Unlock` refuses a memory the journal has (`MemoryUnlock::IsCollected`); `AMemoryFragment::IsTaken` hides its fragment on load, `AMemorySpot` is spent. A later run's memories (and so its boss changes) leave it out. Specs in `WoR.Memory.Fragment` and `WoR.Memory.Sources`. | feature/B-210-collected-memories |
| 2026-10-27 | B-209 (D-100, Q-209): more sources, one path. `MemoryUnlock` (`Unlock`, `UnlockBossDrop(s)`, `UnlockAtFightStart`) into the run and the journal, which records `EMemorySource` per memory (`GetSource`, shown by `wor.Memory.Print`). `UBossData::MemoryOnDefeat` (on a win), `FReactionTrigger::UnlockMemory` (fight start), `UEquipmentDataBase::BoundMemories` (`FBoundMemory`: upgrade level and/or triggers, owner wearing / facing a character; on `LevelUpWeapon` / `LevelUpRing` and at fight start), `AMemorySpot` (walk-in map spot). Spec `WoR.Memory.Sources`. | feature/B-209-memory-sources |
| 2026-10-26 | B-194 (Q-194): `UStoryLinesData` (V lines, conversations), the journal keeps lore / heard lines / played conversations, placeholder `UMemoryJournalWidget` + `AHubJournalStation`; spec `WoR.Story.Journal`. | feature/B-194-memory-journal |
| 2026-10-26 | B-191: `BossModifiers` read at fight start: weaken / strengthen one step (see `ReactionTriggers.md`). | feature/B-191-reaction-triggers |
| 2026-10-26 | B-190 (Q-190): `UMemoryData`, `AMemoryFragment`, `UMemoryJournalSubsystem`, run memories; spec `WoR.Memory.Fragment`. | feature/B-190-memory-fragments |
