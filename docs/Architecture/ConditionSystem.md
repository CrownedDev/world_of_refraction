# Condition System

## Overview
One condition vocabulary for the game (D-130), on the deckbuilder's trigger → condition → effect model (`wor-deckbuilder/docs/reference/05-triggers-conditions.md`): a condition says **when** it is checked (Trigger) and **what** must be true then (Subject). Memories use it first (B-337); boss phases, boss triggers, reactions and equip conditions follow in B-337b. D-100 still holds: memories and boss triggers stay separate systems that share the vocabulary.

## Architecture
- `Public/Conditions/WoRCondition.h`
  - `FWoRCondition`: `Trigger` (`EWoRConditionTrigger`, Any = every event), `Subject` (`EWoRConditionSubject`), operands (`Character` = whose, `Item`, `Evolution`, `Skill`, `Enemy`, `PriorMemory`, `LocationId`), `Operator` / `Value` for numbers, `RequirementFit` / `RequirementMargin`.
  - `FWoRConditionSet`: all its conditions must hold (the deckbuilder's Nested list).
  - Enums are append-only (saved in assets).
- `Public/Conditions/WoRConditions.h`: the evaluator.
  - `IsMet` (a condition, a set), `IsAnyMet` (routes), `Compare`, `FitsRequirements`.
  - `FWoRConditionContext` (trigger, the memory event, the journal), `MakeContext`, `TriggerFor`.
  - `FromMemory`: converts the old memory types on read.

## Integration Points
- `MemoryConditions::AreMet` builds the routes with `FromMemory` and asks `IsAnyMet`. `GetConditionsString` (`wor.Memory.Conditions`) prints through `GetConditionSetString`.
- Authored memories still store `FMemoryCondition` (OBSOLETE (B-337)). `MemoryConditions::IsMet` stays as the parity reference until PIE.

## Known Limitations / TODOs
- The context only carries memory events. B-337b adds fight state (HP, phase, turn) for boss phases and reactions, and their subjects.
- No condition editor or validation yet; the deckbuilder's rules (a row must say when it fires; nested conditions are state-checks only) come with B-337b's authoring.

## Changelog
| Date | Change | Branch |
|------|--------|--------|
| 2026-11-04 | B-337 (D-130): `FWoRCondition` / `FWoRConditionSet`, the `WoRConditions` evaluator; memories evaluate through it; the old memory condition types OBSOLETE. Spec `WoR.Conditions`. Unbuilt (cloud). | feature/B-337-conditions |
| 2026-11-04 | B-337 built and tested on the desktop (`WoR.Conditions` (4) and `WoR.Memory` (28)); no must-fix; should-fixes applied (helpers renamed `IsMemberInParty` / `ConditionEnumName` so a unity build can't collide with `MemoryConditions.cpp` / `SkillEffectDebug.cpp`; Q-337 records the evaluator's four defaults). Merged into dev. | feature/B-337-conditions |
