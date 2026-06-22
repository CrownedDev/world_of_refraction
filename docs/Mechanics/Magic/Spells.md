# Spells

**Status:** Live. The cast experience + the shared **targeting** model. Owning code: `USpellData`, `ESpellSchool`, `SkillDataBase` (targeting/hit-count), `UActionExecutor::ExecuteSpellAsync`.

## Casting (what the player does)

1. **Pick a spell** — from the spells your loadout pools (per source; see [`SpellPoolBudget.md`](./SpellPoolBudget.md)).
2. **Pick target(s)** — per the spell's targeting (below).
3. **Optionally charge** — hold to infuse L1/L2 (see [`Infusion.md`](./Infusion.md)).
4. **Resolve** — animation → impact(s) → damage + status + effects.

## Targeting

A spell picks targets via the shared two-axis model — `TargetType` (Enemy/Ally/Self) × `TargetCount` (Single/All) — plus `HitCount` multi-hit (each hit is defended separately and adds buildup). Full model in [`Targeting.md`](../Combat/Targeting.md).

## Element (authored, not always inherited)

A spell's **element is authored on the asset** — `USpellData::Element` (`SpellData.h:48`, **default Fire**). It is **not** automatically the source's element.

- A **Fire** spell is Fire wherever it's cast from.
- Only a spell authored as **`Generic`** inherits its source's element ("Ball" → "Fire Ball" through a Fire catalyst) — see [`GenericSpells.md`](./GenericSpells.md).

**Element (what) and source (where) are separate axes** — what the element *means* in play is in [`Elements.md`](./Elements.md) (note: element drives status, **not** raw damage).

## Schools

`USpellData::School` (`SpellData.h:51`, `ESpellSchool`) — the spell's role:
- **Destruction** — offense (damage).
- **Enhancement** — buffs.
- **Restoration** — healing.
- (+ further schools per the enum.)

Schools also pool separately for innate casters (count + budget) — see [`SpellPoolBudget.md`](./SpellPoolBudget.md).

## Spell vs ability vs item (caster's seat)

- **Spell** (`USpellData`) — has an **element + school**, comes from a **catalyst/source**, costs EP (or is free for crystal sources). The magical action.
- **Ability** (`UAbilityData`) — **weapon-bound**, **non-elemental** unless infused; physical. See [`Abilities.md`](../Combat/Abilities.md).
- **Item** — consumable (stones, crystals); see [`Items/`](./Items/).

## Cross-links (not repeated here)

- Origins & their costs (Innate / Ring / Weapon / Evolution / Item; break/wear/consume) → [`SpellSources.md`](./SpellSources.md)
- Pool count + weight budget → [`SpellPoolBudget.md`](./SpellPoolBudget.md)
- Generic-element inheritance → [`GenericSpells.md`](./GenericSpells.md)
- What an element does → [`Elements.md`](./Elements.md)
- Charge / infusion → [`Infusion.md`](./Infusion.md)
- EP cost & pools → [`ResourcePools.md`](../Combat/ResourcePools.md)
- Tier / requirement scaling → [`TierGap.md`](../Scaling/TierGap.md), [`TierPower.md`](../Scaling/TierPower.md), [`RequirementGap.md`](../Scaling/RequirementGap.md)

## Entry points

- `USpellData` — `Element`, `School`, `RequiredEvolutionCrystal`.
- `SkillDataBase` — `TargetType`, `TargetCount`, `HitCount`.
- `UActionExecutor::ExecuteSpellAsync` — the cast path.
