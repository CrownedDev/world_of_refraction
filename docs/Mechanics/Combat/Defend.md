# Defend (Brace)

**Status:** [Live]. The proactive brace action. Owning code: `UActionExecutor::ExecuteDefend`, `ESkillEffectType::DefenseBuff`.

## What it does

Choosing **Defend** as your action:

- Applies a **+50% DefenseBuff** to yourself, lasting **1 turn** (until your next turn). `ExecuteDefend` → `FActiveSkillEffect::CreateBuff(DefenseBuff, 50%, 1)`.
- Costs **0 EP**, and **ends your turn**.

Use it to soak a turn you expect to take heavy fire — you trade your action for harder-to-hurt.

## Not the same as the reactive window

This is **distinct** from the real-time **Block / Parry / Dodge** window:

| | Defend (this action) | [Defense window](./DefenseResolution.md) |
|---|---|---|
| When | a **turn action** you pick | **reactive**, per incoming hit |
| Effect | +50% Defense buff for the round | reduce/avoid *that* hit by timing |
| Input | choose it from the menu | time a button in the window |

Picking Defend does **not** open the reactive window for free — they're separate systems (a Defending character still defends incoming hits through the normal window, now with +50% Defense stacked on).

## Entry points

- `UActionExecutor::ExecuteDefend` — applies the buff, 0 EP.
- `ESkillEffectType::DefenseBuff` — the +50%/1-turn effect (see [Status effects](../Status/StatusEffects.md)).

## Related

- [Defense resolution](./DefenseResolution.md) (the reactive window) · [Status effects](../Status/StatusEffects.md) (DefenseBuff) · [Stats](../Character/Stats.md) (Defense substat)
