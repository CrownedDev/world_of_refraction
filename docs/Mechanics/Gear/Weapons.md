# Weapons

**Status:** mixed — the **physical-type → status** routing is [Live]; **equipping/selecting** weapons is [Built · No UI]. Deep spec: [`../Architecture/WeaponSystem.md`](../../Architecture/WeaponSystem.md). Slot capacity (how many skills a weapon holds) is a separate page: [`EquipmentSlots.md`](./EquipmentSlots.md).

## Weapon types (11)

`EWeaponType` — Sword, Greatsword, Spear, Staff, Dagger, Axe, Hammer, Bow, Gauntlets, Scythe, Gun (exact set per the enum). Each type carries its own attack animations and a **physical damage type**.

## Physical damage type → status (the live hook)

`WeaponData::PhysicalDamageType` (`EPhysicalDamageType`) decides **which physical status your basic pressure builds** on the enemy's bar:

| Physical type | Builds toward |
|---|---|
| **Slash** | Bleed (DOT) |
| **Pierce** | Armor-break |
| **Impact** | Stun |

So weapon choice is a **status decision**, not just a damage one — pick the pressure you want. The buildup→proc mechanic itself is in [`StatusEffects.md`](../Status/StatusEffects.md). [Live]

## Wield modes

`EWeaponWieldMode` — how the weapon occupies your hands:
- **Single** — one weapon (one mesh; two-handedness is animation-driven).
- **Dual** — two weapons, one per hand (Generic-class dual-wield).
- **Off-hand shield** — primary weapon + shield in the off hand.

Mode drives mesh layout (`WeaponMeshComponent`); it is **not** a player-selectable toggle in UI today. [Built · No UI]

## Locked / preset skills

`WeaponData::bAbilitiesLocked` + `PresetAbilities` — a conjured weapon ships with fixed abilities you **cannot** remove; they occupy its tier slots, so your customisable room is *tier slots − locked count* (slot counts in [`EquipmentSlots.md`](./EquipmentSlots.md)). [Live] (enforced)

## Switching weapons

Swapping your active weapon mid-combat is a **free in-menu loadout toggle** (`ULoadoutComponent::ToggleEquipment`, via the command menu) — it rebuilds the action menu for the new weapon and **costs no turn and no EP**. It is *not* a costed action through the executor. [Live]

## Equipping — not reachable yet

Choosing weapons, wield mode, and per-instance rolls is **[Built · No UI]** — there's no equip screen; loadouts are data/Blueprint-configured. Per-instance stat rolls on weapons: [`../Architecture/PerInstanceRollSystem.md`](../../Architecture/PerInstanceRollSystem.md).

## Entry points

- `EWeaponType`, `EWeaponWieldMode` — type + wield enums.
- `WeaponData::PhysicalDamageType`, `bAbilitiesLocked`, `PresetAbilities` — the live fields.
- `WeaponMeshComponent` — per-mode mesh spawn.

## Related

- [`EquipmentSlots.md`](./EquipmentSlots.md) — tier → skill-slot capacity.
- [`StatusEffects.md`](../Status/StatusEffects.md) — what Slash/Pierce/Impact build toward.
- [`Archetypes/Generic.md`](../Archetypes/Generic.md) — dual-wield class.
- [`../Architecture/WeaponSystem.md`](../../Architecture/WeaponSystem.md) — deep spec.
