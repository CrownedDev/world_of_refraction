# TODO / Backlog

Deliberately-deferred and watch-later items. One line each + a status tag:
**WATCH** (verify in PIE) · **BLOCKED** (needs a prerequisite) · **POSSIBLE** (do only
if PIE/usage shows the need) · **CLEANUP** (remove after verification) · **DONE**.
Living backlog — keep entries short; promote to a real doc/issue when worked.

## Combat — lethality & infusion

- **WATCH** — Corpse-walk-back: after a lethal infusion, the dead caster may visibly slide back to position (`SignalActionComplete` plays return-movement on a flag-dead pawn). If janky in PIE → gate return-movement on `bIsAlive`. Not yet observed; confirm next PIE.
- **DONE** — Infusion lethal-at-finalize documented (`CombatOrchestrator.md` → *Infusion HP cost — lethal, paid at finalize*).

## Emerald-AI

- **BLOCKED** — Self-target Emerald-AI: wired but DORMANT (`ESTIMATED_EP_REGEN_PER_TURN = 0`). Activate by setting it >0 **only** once a passive per-turn EP-regen mechanic exists. Cross-ref `AISystem.md`.
- **POSSIBLE** — Enemy all-target Emerald scan: AI evaluates Emerald only on `BestTarget`. If PIE shows missed one-tick-lethal kills on non-selected targets, add an all-enemy scan.
- **POSSIBLE** — Loosen the one-tick-lethal Emerald gate: currently requires next-tick ≥ HP (guaranteed kill). If too conservative in PIE, consider an accumulated-over-exposure-window check.

## Diagnostics & cleanup

- **CLEANUP** — Strip `[AI Emerald]` + `[BONUSDIAG]` diagnostic logs after final PIE verification of Emerald-AI + the bonus-turn visual.
- **CLEANUP** — `SpellData.h` + `CastableSkillDataBase.h` embed `FWorldStatRequirements` with the same Requirements▸Requirements double-header collapsed on `UEquipmentDataBase`; same one-line `ShowOnlyInnerProperties` fix available if wanted.

## Loadout — instance bridge

- **WATCH** — Future equip UI must forbid referencing the same owned instance in TWO slots of one loadout (e.g. primary + secondary weapon): both combat entries would share an effect-ID window (int32 `InstanceID`, `ID*100+i`) → apply/remove collisions. Cross-loadout sharing is safe (one active at a time). Enforce in the equip UI when built.
- **BLOCKED** — Player equip UI: the runtime flow that binds a loadout slot to an owned instance (sets the `FSavedLoadout` instance-ref FGuids). The U1 bridge is built and inert until this exists — it's what makes per-instance rolls carry into combat. See `Architecture/PerInstanceRollSystem.md`.
- **BLOCKED** — Reroll economy: Pool charging (rewards fill `Stat/ResistancePool`), the reroll trigger (unlock at Pool==MaxPool, spend to 0, re-roll from the stored MaxPool), and its UI. Storage shipped (U0); economy not built.
- **BLOCKED** — Cross-session persistence of instance rolls: the owned pool is rebuilt from the asset every spawn (GUIDs re-mint, toggle-ON gear re-rolls). Needs the save system before per-instance rolls (and instance refs) survive a session. SaveGame tags already in place on the persistent fields.

## Refactor — banked

- **POSSIBLE** — 5 Group-B attachment-accessor variants (banked from the accessor migration).
- **POSSIBLE** — `StatusMultiplier` base-extract: only if base composition grows beyond ~3 terms (currently keep-both).

## Resistances — gear arc

- **DONE** — Gear resistance: rings, weapons, and evolutions granting status-buildup resistance (element + physical). Two paths shipped — authored effect (`FSkillEffect` Element/PhysicalType) + rolled `FResistanceBonus` (own zero-sum pool, per-instance for weapon/ring, always-on for evolution). Composes as term #6 in `GetTotalStatusResistance`. See `Architecture/ResistanceSystem.md` → *Gear resistance*. **Open balance knob:** `RESISTANCE_CATEGORY_CAP` (PIE-tune).
