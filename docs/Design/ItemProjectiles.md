# Item Projectiles + Speed-Scaled Projectiles

> **Status: DESIGN-LOCKED, NOT YET BUILT.**
> Two linked changes: (1) projectile *travel speed* scales by the action's speed stat (abilities/spells),
> and (2) consumable items become defendable thrown projectiles. When shipped, this moves to
> `docs/Design/Completed/`.

## Part 1 — Projectile travel speed scales by stat

### What's already done

Animation play-rate already scales by the right stat at the `ActionExecutor` play sites:

- **Physical (attack/ability)** anim → `GetEffectiveActionSpeed()` (Body/ActionSpeed — the retired
  locomotion stat, now the physical play-rate driver).
- **Spell** anim → `GetEffectiveSpellSpeed()` (Mind/SpellSpeed).

### The gap

Projectile **travel** speed is flat — `Entry.ProjectileSpeed` (or the loose/default 1500), no stat input.
Neither physical nor spell projectiles scale their flight.

### The change

At projectile spawn (`ActionExecutor::SpawnProjectile`), multiply travel speed by the same stat that drives
the animation, keyed on action type:

| Action | Travel speed × |
| ------ | -------------- |
| Attack / Ability (physical) | `GetEffectiveActionSpeed()` |
| Spell | `GetEffectiveSpellSpeed()` |
| Item (thrown — physical) | `GetEffectiveActionSpeed()` |

Action type is known at spawn (spell vs ability vs item). Anim and travel then move in lockstep — a fast
character visibly throws/casts faster *and* the projectile flies faster.

## Part 2 — Defendable item projectiles

### Current state

Items are **not skills.** The consumable surface is decoupled from the asset and driven by `FCrystalId`
identity through lookup tables (`CrystalEffectTable` / `CrystalIdentity`), resolved by
`UItemExecutor::UseItem`. Today an item use is animation-only — no projectile, no counterplay.

### The change

A thrown item becomes a real projectile through the existing `ASkillProjectile`, **registered with the
defense system** so it can be dodged/blocked/parried like any other projectile — including, amusingly, by
allies in its path.

### Assembled model

- **One shared throw montage** — every item uses the same throwing animation (arm motion identical for a
  potion or a bomb; items differ by VFX, not animation). Physical → ActionSpeed-scaled play-rate (Part 1).
- **`ASkillProjectile`** — travel speed × `GetEffectiveActionSpeed()`, defendable via the same window.
- **VFX from `ItemVFXTable`** (the backlog table the FusedMontage doc anticipates — "same shape of lookup"
  as `ElementColors`). One generic VFX shape per role: **Muzzle** (throw flash), **Trail** (follows the
  projectile), **Impact** (landing burst). NOT the Cosmetic role (that's ambient/auras).
- **Tinting** — items route through the **same element-tint pipeline as spells** (`bElementTinted = true`):
  - **Crystal items** → tinted by their **crystal element** (Fire item → orange, Water → blue, …).
  - **Stones** (augment/damage/etc.) → carry **Generic** element → neutral/no-element look.

  No new colour machinery — one VFX shape, element recolours it, consistent with the game's colour
  language.
- **One-time-ness** — purely the inventory decrement on use; orthogonal to delivery/VFX.

## Build scope (for the survey)

**Part 1:**
- `ActionExecutor::SpawnProjectile` — multiply `Speed` by `GetEffectiveActionSpeed()` /
  `GetEffectiveSpellSpeed()` per action type, before `InitializeProjectile`.
- AI projectile-timing previews (if any read travel speed) see the scaled value.

**Part 2:**
- **Item delivery adapter** — route an item use through `ASkillProjectile` (item isn't a `USkillDataBase`,
  so a thin spawn path that supplies the projectile its speed + VFX + impact effect from the item's
  identity, rather than a skill asset).
- **`ItemVFXTable`** — new lookup keyed on `FCrystalId` (or item type) → Muzzle/Trail/Impact Niagara
  shapes, element-tinted at spawn (crystal element for items, Generic for stones).
- **Shared throw montage** — one montage asset; the item path plays it ActionSpeed-scaled.
- **Defense registration** — the item projectile registers with `UDefenseSystem` like a skill projectile
  (dodge/block/parry windows apply).
- **`UItemExecutor`** — the effect now fires on projectile *impact* (after travel + defense resolution),
  not immediately on use.
- **Debug tooling** — item-projectile spawn/impact log; `ItemVFXTable` readout.

Cross-system (projectile + defense + item executor + VFX) → survey-first.

## Open / carry-over

- **Instant-cast items** — rejected in favour of defendable projectiles (an item with no defense
  interaction would be the odd action in a reactive system). If some items should be instant (e.g.
  self-buffs with no target), that's a per-item delivery flag, not the default.
- **`ItemVFXTable` authoring** — the table's contents (which Niagara shape per item family) is a content
  pass, separate from the C++ plumbing.
- **Self-targeted items** — a thrown projectile model assumes a target; self-buffs may keep the
  instant/animation-only path. Confirm at survey which item categories throw vs self-apply.

## Changelog

| Date | Change | Branch |
| ---- | ------ | ------ |
| (pending) | Design locked: projectile travel speed scales by action speed stat (physical → ActionSpeed, spell → SpellSpeed); items become defendable thrown projectiles via ASkillProjectile (one shared throw montage, ActionSpeed-scaled, VFX from a new ItemVFXTable element-tinted by crystal element / Generic for stones). Not yet built. | (tbd) |
