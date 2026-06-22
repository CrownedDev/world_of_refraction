# Targeting

**Status:** [Live]. The shared targeting model for spells, abilities, and items. Owning code: `SkillDataBase` (`TargetType`, `TargetCount`, `HitCount`).

## Two axes

Every action's targeting is **two independent axes** (`SkillDataBase.h`):

- **`TargetType`** — `Enemy` / `Ally` / `Self` (default `Enemy`).
- **`TargetCount`** — `Single` / `All` (default `Single`).

So an action is e.g. "Single Enemy", "All Allies", "Self". (A legacy single-axis `ETargetType` was migrated to this two-axis model in PostLoad — don't author the legacy field.)

## Multi-hit (`HitCount`)

`HitCount` (default 1) — an action can land several hits on its target(s). This matters **twice**:

- **Defense is per-impact** — each hit opens its own defense window; you can block one and miss another. See [Defense](./DefenseResolution.md).
- **Status builds per hit** — each hit adds buildup, so multi-hit fills the bar faster. See [Status buildup](../Status/StatusBuildup.md).

## Who uses this

- **[Spells](../Magic/Spells.md)**, **[abilities/attacks](./Abilities.md)**, and items all resolve targets through this same model — they cross-link here rather than redefining it.

## Entry points

- `SkillDataBase` — `TargetType`, `TargetCount`, `HitCount` (`Combat/TargetType.h`).

## Related

- [Spells](../Magic/Spells.md) · [Abilities](./Abilities.md) · [Defense](./DefenseResolution.md) · [Status buildup](../Status/StatusBuildup.md)
