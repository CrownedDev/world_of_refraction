# Effect text (generated descriptions)

## Overview

Item and skill descriptions are **generated from the effect data**, never hand-written, so they can't drift from what the item does (D-031). The shape is the deckbuilder's card-text tree (`wor-deckbuilder/docs/reference/07-text-grammar.md`, "The card-text tree"):

- **Level 0, the trigger heading:** "On Hit", "At the start of your turn", "When a Team Member takes damage". An effect with no trigger goes under **Passive** (gear) or **Activate** (a skill). Headings with the same words merge into one, wherever their rows come from.
- **Each deeper level is a condition**, all of which must hold: "While your HP is below 30%".
- **The leaves are the effects**, as imperative sentences with their numbers and duration: "Deal 3 Fire damage to an Enemy each turn for 2 turns".
- **Short form** for small spaces: one line per heading, "On Hit*: Heal 3, gain 1 Energy", with "*" when anything under it has a condition.
- **Live lighting** in fights: each condition line is Held or Not Held right now. A screen lights a held branch green (a buff) or red (a debuff); the base item's pop-up shows no lighting.

## Architecture

- `Skills/Effects/EffectText.h` (`namespace EffectText`), pure functions, no game state:
  - `BuildTree(Effects, Source)` returns `TArray<FEffectTextLine>`: Kind (Heading / Condition / Effect), Depth, Text, the effect and condition indices it came from, `bAnyOf`, `bDebuff`, `Light`.
  - `BuildShortForm`, `ToMultilineText` (two spaces per level, effects as bullets).
  - `DescribePayload` (one body, lowercase-initial), `DescribeHeading`.
  - `LightTree(Lines, Effects, IsHeld)`: sets Light on every condition line through a predicate.
- **The tree mirrors `EvaluateConditionGroup`** (all AND entries, and at least one OR entry when there are any):
  - The heading is every AND event, then the any-of set: "On Crit and either On Hit or On Dodge". A lone OR entry must hold, so it reads as AND.
  - One condition level per AND state check; all OR state checks share one level that holds when any does.
  - An any-of set that mixes an event and a state check is one either-or heading: "On Hit or while your HP is below 30%".
  - Always and None print nothing. Activate comes first and Passive last (07, "Block structure").
  - Leaves light by what they do: a cheaper `ModifyEnergyCost` is a buff here, even though `SkillEffectClassification` classes the type as a debuff.
- **Words:** every player-facing word is a `LOCTEXT` in namespace `EffectText` (D-061, D-062); element names come from `WoRGameNames`. The wording choices this game needed beyond the deckbuilder are Q-045.
- `USkillEffectManager::IsConditionHeld(Condition, Owner, Target)`: the live predicate (HP / Energy thresholds, any member of a team). A target-side check with no Target reads as held, because the runtime skips it.

## Integration points

- Inputs: any `TArray<FSkillEffect>`: `ULoadoutComponent::GetActiveEffects`, `UEffectDefinition::Effects`, `USkillDataBase::GetAllEffects`, an item's starting / conditional effects.
- Consumers: the skill tooltip and item detail screens (UI rebuild, B-105 onward). Nothing in the game shows the text yet.

## Debug

- `wor.DescribeEffects`: every combatant's gear effects as text, each condition tagged `[held]` / `[not held]`.
- `UEffectDefinition` Details panel: **Log Effect Text**.
- `UEffectTextDebug::GetEffectTextString` / `GetActorEffectTextString`.
- Spec: `WoR.Text.Effects`. Its "every effect kind has a sentence" case fails until a new `ESkillEffectType` value is worded.

## Known limitations / TODOs

- Sentence case is changed on the finished string: `Capitalise` upper-cases a leaf's first letter, and the short form lower-cases each later leaf's. Both go through `FText::FromString`, so the result loses its localisation history and won't re-render on a culture change. Rebuild the text after a language switch.
- A mixed either-or heading ("On Hit or while your HP is below 30%") is a heading, so it gets no live lighting.
- No element fusion in headings ("On Fire Hit"): this game's conditions carry no element.
- The deckbuilder's prose form (rows folded into sentences) is not built; this game shows the tree and the short form (D-031).
- TODO: "delay an Enemy's next turn" (`SkipTurn`) is only true on the Wind break path (`ApplyTriggeredSkillEffect`). No handler delays a turn when gear or a skill applies a `SkipTurn` payload directly.
- An effect kind with no sentence prints its type name (the loud fallback the every-kind spec guards).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created (B-045): `EffectText` tree, short form, lighting; `IsConditionHeld`; `wor.DescribeEffects`; `WoR.Text.Effects`. | feature/B-045-effect-text |
