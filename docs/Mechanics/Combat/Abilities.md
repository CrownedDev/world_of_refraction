# Abilities (and Attacks)

**Status:** Live. The attack / ability / spell taxonomy and what an ability is from the player's seat. Owning code: `UAbilityData`, `USkillDataBase::IsAttack`, slotting gates in `LoadoutComponent`.

## The taxonomy — all three are `USkillDataBase`

| Kind | Class / flag | Slottable? | Element |
|---|---|---|---|
| **Attack** | `UAbilityData` with `bIsAttack = true` (`AbilityData.h:51`) | **No** — the weapon's basic attack | none (physical) unless infused |
| **Ability** | `UAbilityData` with `bIsAttack = false` | **Yes** — slotted into a weapon | none (physical) unless infused |
| **Spell** | `USpellData` | Yes — pooled per source | authored element + school (see [`Spells.md`](../Magic/Spells.md)) |

`IsAttack()` is the only attack-vs-ability distinction post-merge (`UAbilityData::IsAttack()` returns `bIsAttack`; base `USkillDataBase::IsAttack()` = `false`, `SkillDataBase.h:386`). A `bIsAttack` asset **cannot be slotted as an ability** — it's the weapon's built-in basic.

## What an ability is

- **Weapon-bound** — `RequiredWeaponType` (`AbilityData.h:39`); some require dual-wield / off-hand (`bRequiresDualWeapon`, `:45`). You only have an ability if you wield a matching weapon.
- **Non-elemental by default** — `UAbilityData` has **no element field**; abilities are physical and scale RawDamage. They gain an element/status **only if you infuse** them (player choice) — see [`Infusion.md`](../Magic/Infusion.md).
- **Cost** — can be **free** (a basic attack with `BaseEnergyCost = 0`) or cost EP. → [`ResourcePools.md`](./ResourcePools.md)
- **Targeting** — same two-axis model (`TargetType` × `TargetCount`, `HitCount`) — see [`Targeting.md`](./Targeting.md).

## Where abilities come from

- **Slotted into a weapon**, up to the weapon's tier capacity → [`EquipmentSlots.md`](../Gear/EquipmentSlots.md).
- **Locked presets** belong to the **gear**, not the ability: `UWeaponData::bAbilitiesLocked` + `PresetAbilities` (conjured weapons ship fixed abilities). The ring mirror is `URingData::bSpellsLocked` + `PresetSpells`. See [`Weapons.md`](../Gear/Weapons.md).

## Ability vs spell — the player tell

An **ability** needs a **weapon** and is **physical/colourless** until you infuse it; a **spell** carries its own **element + school** and comes from a **catalyst/source**. Infusing an ability is how you bolt an element/status onto a physical hit.

## Entry points

- `UAbilityData` — `bIsAttack`, `IsAttack()`, `RequiredWeaponType`, `bRequiresDualWeapon`.
- `USkillDataBase::IsAttack` — base returns `false`.
- `LoadoutComponent` — ability-slotting gates.

## Related

- [`Spells.md`](../Magic/Spells.md) — spells + the shared targeting model.
- [`Weapons.md`](../Gear/Weapons.md) — weapon types, locked presets, physical-type → status.
- [`EquipmentSlots.md`](../Gear/EquipmentSlots.md) — tier → slot capacity.
- [`Infusion.md`](../Magic/Infusion.md) — adding element/charge to an ability.
