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
- The context carries memory events and, since B-355, the boss's fight state (`FWoRBossState`: HP, own turns, status breaks, phases reached, absorption, crystal) for the `Boss*` subjects. Since B-337b: `Opposing` (the opposing team's characters, `FacingEnemy`) and `Self` / `Target` (`FWoRFighterState`, current / max HP, the `Hp` subject by `Who`). Reactions fill all three; boss rules do not fill `Self` yet (B-337c).
- A reaction's context trigger is `Any`: a condition authored with a named `Trigger` never holds in a reaction (reactions say when through their `When` moment).
- Not moved yet: a bound item's memory conditions (`FBoundMemory`) and the skill-effect triggers (`ESkillTrigger`): B-337c.
- No condition editor or validation yet; the deckbuilder's rules (a row must say when it fires; nested conditions are state-checks only) come with B-337b's authoring.

## Changelog
| Date | Change | Branch |
|------|--------|--------|
| 2026-11-04 | B-337 (D-130): `FWoRCondition` / `FWoRConditionSet`, the `WoRConditions` evaluator; memories evaluate through it; the old memory condition types OBSOLETE. Spec `WoR.Conditions`. Unbuilt (cloud). | feature/B-337-conditions |
| 2026-11-04 | B-337 built and tested on the desktop (`WoR.Conditions` (4) and `WoR.Memory` (28)); no must-fix; should-fixes applied (helpers renamed `IsMemberInParty` / `ConditionEnumName` so a unity build can't collide with `MemoryConditions.cpp` / `SkillEffectDebug.cpp`; Q-337 records the evaluator's four defaults). Merged into dev. | feature/B-337-conditions |
| 2026-11-04 | B-337b (D-130, Q-337b): subjects `FacingEnemy` (20) and `Hp` (21, absolute, `EWoRConditionWho` Self / Target); context `Opposing`, `Self`, `Target`; reactions evaluate here (`ReactionTriggers::Matches`). Equip conditions already came with memories (B-337). Spec `WoR.Reaction.Trigger` (parity grid). | feature/B-337b-conditions-part2 |
| 2026-11-04 | B-355 (D-144): `Boss*` subjects (`BossHPAtMostPercent`, `BossTurnsTaken`, `BossAbsorptionMaxed`, `BossCrystalBroken`, `BossStatusBreaks`, `BossInfusionHits` (TODO), `BossPhasesReached`) read `FWoRConditionContext::Boss` (`FWoRBossState`); false without one. | feature/B-355-boss-rules |
