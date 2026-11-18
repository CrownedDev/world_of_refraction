# Weapon model orientation: what the project has today

**Date:** 2026-11-17. **Read only:** nothing was edited or saved; measured through the editor connection.
**Asked by Crown:** one standard for how a weapon model is built (which way it points, where its origin is), so no weapon needs its own position fix.

## Short answer

- **StylizedWeapons is one pattern, start to finish.** Every sampled weapon runs along **Y**, with the grip at the origin and the business end toward **+Y**. All are skeletal meshes with a single `Root` bone and no sockets. The four weapons the game uses are all from this pack.
- **CombatMaster and the 100 Fantasy pack mostly run along Z**, blade pointing **-Z**, but neither pack is consistent inside itself (centred axes, shields facing three different ways, a sword whose origin is off the end).
- **The hand sockets, not the weapons, carry the fix-ups today.** There is one socket per weapon type per hand (`hand_r_sword`, `hand_r_axe`, ...), and Manny and Quinn have **different rotations under the same socket names**.
- **Seven weapon types have no socket mapped at all** (Spear, Staff, Hammer, Bow, Gauntlets, Scythe, Gun): the code logs an error and shows no mesh.
- **No Paragon weapon model is on disk.** `/Game/Imports/Paragon/` holds animations only: no static mesh, no skeletal mesh.

## 1. The weapon models

Bounds are the mesh's own box in centimetres, as min to max on each axis. "Runs along" is the longest axis. "Origin" is where 0,0,0 sits along that axis. Length is the box size on the long axis.

### The four weapons the game uses (all StylizedWeapons, WeaponPack1, skeletal)

Folder: `/Game/StylizedWeapons/WeaponPack1/Meshes/Item/Weapons/`

| Weapon data | Mesh | X | Y | Z | Runs along | Origin | Length |
|---|---|---|---|---|---|---|---|
| `DA_Weapon_Sword` | `Sword/SK_Sword_1H_Fantasy_01` | -4.6 to 4.7 | -18.8 to 93.4 | -14.3 to 18.2 | Y, blade to +Y | Grip, 19 from the pommel end | 112 |
| `DA_Weapon_Shield` | `Shield/SK_Shield_Fantasy_01` | -18.3 to 8.3 | -44.3 to 42.5 | -61.6 to 57.1 | Tall along Z, wide along Y, face along X | Near the centre | 119 tall, 87 wide |
| `DA_Weapon_Axe_Hand` | `Axe/SK_Axe_1H_Fantasy_01` | -7.0 to 5.6 | -53.2 to 100.3 | -35.1 to 26.9 | Y, head to +Y | Grip, 53 from the butt end | 154 |
| `DA_Weapon_Axe_Bearded` | `Axe/SK_Axe_1H_Fantasy_03` | -7.6 to 7.6 | -39.9 to 88.0 | -39.6 to 22.5 | Y, head to +Y | Grip, 40 from the butt end | 128 |

All four: skeleton `SKEL_Weapon`, one bone `Root`, no sockets.

### StylizedWeapons, one of each kind (all skeletal, `SKEL_Weapon`, one bone `Root`, no sockets, unless noted)

| Kind | Mesh | X | Y | Z | Runs along | Origin | Length |
|---|---|---|---|---|---|---|---|
| 1H sword | `WeaponPack2/.../Sword/SK_Sword_1H_Generic_01` | -3.5 to 3.5 | -21.8 to 103.8 | -13.0 to 13.0 | Y, blade to +Y | Grip, 22 from the pommel | 126 |
| 2H sword | `WeaponPack1/.../Sword/SK_Sword_2H_Fantasy_01` | -8.5 to 8.5 | -33.3 to 146.8 | -25.1 to 25.2 | Y, blade to +Y | Grip, 33 from the pommel | 180 |
| 2H sword | `WeaponPack2/.../Sword/SK_Sword_2H_Generic_01` | -4.2 to 4.2 | -34.8 to 162.1 | -20.7 to 20.7 | Y, blade to +Y | Grip, 35 from the pommel | 197 |
| 1H axe | `WeaponPack2/.../Axe/SK_Axe_1H_Generic_01` | -6.9 to 6.9 | -46.7 to 111.1 | -33.5 to 34.7 | Y, head to +Y | Grip, 47 from the butt | 158 |
| 2H axe | `WeaponPack1/.../Axe/SK_Axe_2HL_Fantasy_01` | -9.8 to 9.8 | -79.0 to 116.1 | -45.1 to 44.4 | Y, head to +Y | On the shaft, 79 from the butt | 195 |
| 2H axe | `WeaponPack2/.../Axe/SK_Axe_2HL_Generic_01` | -7.4 to 7.4 | -88.8 to 101.6 | -41.9 to 12.2 | Y | On the shaft, near the middle | 190 |
| Dagger | `WeaponPack1/.../Dagger/SK_Dagger_1H_Fantasy_01` | -4.6 to 4.6 | -18.9 to 69.9 | -11.4 to 14.7 | Y, blade to +Y | Grip, 19 from the pommel | 89 |
| Hammer 1H | `WeaponPack1/.../Hammer/SK_Hammer_1H_Fantasy_01` | -19.9 to 17.8 | -44.5 to 71.9 | -24.4 to 21.5 | Y, head to +Y | Grip, 45 from the butt | 116 |
| Hammer 2H | `WeaponPack1/.../Hammer/SK_Hammer_2H_Fantasy_01` | -13.1 to 13.0 | -73.4 to 89.0 | -28.4 to 28.8 | Y | On the shaft, near the middle | 162 |
| Polearm | `WeaponPack1/.../Polearm/SK_Polearm_2HL_Fantasy_01` | -2.8 to 2.8 | -108.1 to 138.0 | -13.2 to 12.3 | Y, tip to +Y | On the shaft, 108 from the butt | 246 |
| Staff | `WeaponPack1/.../Staff/SK_Staff_Fantasy_01` | -7.8 to 7.8 | -125.8 to 117.5 | -21.3 to 27.8 | Y | On the shaft, near the middle | 243 |
| Shield | `WeaponPack1/.../Shield/SK_Shield_Fantasy_02` | -23.4 to 0.5 | -42.1 to 42.1 | -61.4 to 61.4 | Tall along Z, wide along Y, face along X | Centre, on the back face | 123 tall, 84 wide |
| Bow | `WeaponPack2/.../Bow/SK_Bow_Generic_01` | -7.4 to 7.8 | -93.7 to 93.7 | -8.0 to 34.1 | Y (limb to limb), curve toward +Z | Centre (the grip) | 187 |
| Fist | `WeaponPack2/.../Fist/SK_Fist_1H_MetalPunch` | -12.5 to 4.7 | -12.9 to 13.6 | -13.5 to 24.5 | Z, toward +Z | Inside the fist | 38 |
| Fist (claw) | `WeaponPack2/.../Fist/SK_Fist_1H_MetalClaw` | -16.6 to 9.8 | -17.1 to 13.5 | -12.8 to 42.7 | Z, claws to +Z | Inside the fist | 56 |

The bow is the one exception on bones: skeleton `SKEL_Weapon_Bow`, bones `Root`, `String`, `Arm_Low`, `Arm_Up`.

### CombatMaster bundle (all static meshes)

Folder: `/Game/CombatMasterAnimBundle/Weapon/Meshes/` (the same 12 meshes are duplicated under `/Game/Imports/CombatMasterAnimBundle/Weapon/Meshes/`).

| Mesh | X | Y | Z | Runs along | Origin | Length |
|---|---|---|---|---|---|---|
| `SM_Sword_HS1` | -12.8 to 12.8 | -2.2 to 2.2 | -118.3 to 20.5 | Z, blade to **-Z** | Grip, 21 from the pommel | 139 |
| `SM_HSAS_Sword` | same as `SM_Sword_HS1` | | | Z, blade to -Z | Grip | 139 |
| `SM_Katana` | -12.5 to 3.6 | -3.5 to 3.5 | -92.2 to 13.9 | Z, blade to -Z | Grip, 14 from the pommel | 106 |
| `SM_GreatSword` | -2.8 to 3.2 | -13.9 to 13.0 | -150.3 to 30.0 | Z, blade to -Z; **edges face Y, not X** | Grip, 30 from the pommel | 180 |
| `SM_Axe` | -3.2 to 3.2 | -44.1 to 44.5 | -23.7 to 6.0 | **Y, centred** | Middle of the shaft | 89 |
| `SM_Shield_DSAS` | -34.0 to 40.8 | -2.9 to 15.6 | -44.0 to 35.9 | Face along Y | Near the centre | 80 tall, 75 wide |
| `SM_HSAS_Shield` | -46.0 to 36.7 | -25.5 to 25.5 | -6.8 to 8.9 | **Face along Z (lying flat)** | Near the centre | 83 by 51 |

### 100 Fantasy Weapons pack (all static meshes, 109 in the folder)

Folder: `/Game/Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/`

| Kind | Mesh | X | Y | Z | Runs along | Origin | Length |
|---|---|---|---|---|---|---|---|
| Scythe | `Death_Scythe` | -53.3 to 7.3 | -5.1 to 5.1 | -28.5 to 74.7 | Z, blade out to -X | On the shaft, 29 from one end | 103 |
| Scythe | `Ash_Crusade_Scythe` | -36.1 to 18.2 | -2.3 to 2.3 | 0 to 106.9 | Z | **At the very bottom end** | 107 |
| Spear | `Adventurer_s_Spear` | -13.9 to 13.9 | -3.9 to 3.9 | -80.7 to 128.7 | Z | On the shaft, 81 from one end | 209 |
| Spear | `Ash_Spear` | -9.3 to 9.3 | -1.6 to 1.6 | -80.7 to 129.6 | Z | On the shaft, 81 from one end | 210 |
| Gun | `Hunter_Gun` | -6.3 to 39.0 | -3.4 to 3.4 | -6.2 to 13.1 | **X**, barrel to +X | Grip | 45 |
| Gun | `Hunter_Gun_2` | -6.5 to 48.8 | -4.2 to 4.1 | -6.5 to 14.2 | **X**, barrel to +X | Grip | 55 |
| Sword | `Adventurer_Sword` | -12.9 to 13.0 | -2.8 to 2.8 | -160.6 to -1.3 | Z, all of it below the origin | **Just off one end** | 159 |
| Dagger | `Dagger` | -3.7 to 6.1 | -2.7 to 2.7 | -52.9 to 9.9 | Z, blade to -Z | Grip, 10 from the pommel | 63 |
| Axe | `Axe` | -13.4 to 7.4 | -2.7 to 2.7 | -40.4 to 40.3 | Z, **centred** | Middle of the shaft | 81 |
| Shield | `Shield_1` | -32.3 to 32.3 | -5.4 to 5.4 | -41.8 to 41.8 | Face along Y | Dead centre | 84 tall, 65 wide |
| Bow | `Great_Crystal_Bow` | -17.9 to 38.5 | -1.8 to 1.8 | -77.5 to 77.5 | Z (limb to limb), curve toward +X | Centre | 155 |

## 2. Manny and Quinn: hand bones and sockets

| | Manny | Quinn |
|---|---|---|
| Mesh | `/Game/Data/Characters/Models/CharacterBase/SKM_Manny_Base` | `/Game/Data/Characters/Models/CharacterBase/SKM_Quinn` |
| Skeleton | `/Game/CombatMasterAnimBundle/Demo/Characters/Manny_UE5/Meshes/SK_Manny_HTS` | `/Game/Imports/Big_Pack_Magic_VFX/Demo/Characters/Mannequins/Meshes/SK_Mannequin` |
| Bones | 161 | 161 |

**They are on two different skeleton assets.**

**Hand and weapon bones (same names on both):** `hand_r` (parent `lowerarm_r`), `hand_l` (parent `lowerarm_l`), `weapon_r`, `weapon_l`, `ik_hand_root` (parent `root`), `ik_hand_gun` (parent `ik_hand_root`), `ik_hand_r` and `ik_hand_l` (parent `ik_hand_gun`).
On Quinn, `weapon_r` and `weapon_l` sit under `hand_r` and `hand_l`. On Manny the tool returned an empty parent for both; treat that as unconfirmed, not as a fact about the skeleton.

**Sockets.** Location is in centimetres from the parent bone; rotation is pitch / yaw / roll in degrees.

| Socket | Parent | Manny location | Manny rotation | Quinn location | Quinn rotation |
|---|---|---|---|---|---|
| `hand_r_sword` | `hand_r` | -3.7, 2.65, -6.47 | 10.9 / 89.24 / -67.42 | -3.7, 2.65, -6.47 | -29.74 / 163.24 / -174.02 |
| `hand_l_sword` | `hand_l` | 5.04, -3.16, 5.34 | 10.9 / 90.76 / 112.58 | 5.04, -3.16, 5.34 | 18.72 / 0.64 / -2.84 |
| `hand_r_axe` | `hand_r` | -12.55, 2.92, 9.02 | 10.9 / 89.24 / -67.42 | -12.55, 2.92, 9.02 | 18.01 / -29.17 / -16.44 |
| `hand_l_axe` | `hand_l` | 12.44, -3.11, -10.36 | 10.9 / 90.76 / 112.58 | 12.44, -3.11, -10.36 | -17.47 / 158.81 / -170.0 |
| `hand_r_greatsword` | `hand_r` | -1.24, 5.35, -13.59 | 10.9 / 89.24 / -67.42 | -1.24, 5.35, -13.59 | -1.92 / -92.21 / 151.97 |
| `hand_l_greatsword` | `hand_l` | 10.32, -3.73, -0.64 | -1.92 / -92.21 / -18.03 | 10.32, -3.73, -0.64 | -1.92 / -92.21 / -18.03 |
| `hand_l_sas` (shield) | `hand_l` | 0.03, -1.75, 2.58 | -42.56 / -94.23 / -105.5 | 1.14, -9.42, -3.73 | -7.32 / -172.72 / 179.83 |
| `hand_r_weapon` | `hand_r` | not on Manny | | -3.7, 2.65, -6.47 | -29.74 / 163.24 / -174.02 |
| `hand_l_weapon` | `hand_l` | not on Manny | | 5.04, -3.16, 5.34 | 18.72 / 0.64 / -2.84 |
| `weapon_r_muzzle` | `weapon_r` | 45, 0, 25 | 0 / 0 / 0 | 45, 0, 25 | 0 / 0 / 0 |

- **Same locations, different rotations.** For almost every socket the two bodies agree on location and disagree on rotation. Only `hand_l_greatsword` matches fully.
- **On Manny the three right-hand sockets share one rotation** (10.9 / 89.24 / -67.42); only the location differs per weapon type.
- Quinn also carries leftover copies: `hand_l_sword_0`, `hand_l_axe_0`, `hand_l_sas_0`, `hand_l_greatsword_0`, each identical to the socket without `_0`.
- Both also have `foot_r_Socket` and `foot_l_Socket` (not weapon related).

**What the game attaches to today** (`Source/world_of_refraction/Private/Equipment/Weapons/WeaponMeshComponent.cpp:70-125`):

| Weapon type | Right hand | Left hand |
|---|---|---|
| Sword, Dagger | `hand_r_sword` | `hand_l_sword` |
| Greatsword | `hand_r_greatsword` | `hand_l_greatsword` |
| Axe | `hand_r_axe` | `hand_l_axe` |
| Shield (or a weapon with a built-in off-hand shield) | n/a | `hand_l_sas` |
| Spear, Staff, Hammer, Bow, Gauntlets, Scythe, Gun | **none: logs an error, no mesh shown** | none |

The mesh snaps to the socket, then the weapon's own Mesh Rotation is applied on top (`WeaponMeshComponent.cpp:223-228`). The socket names are in C++, not on the Blueprint: the character Blueprint only holds the component, with no settable socket or offset fields.

## 3. Fix-ups already made

**On the four weapons the game uses: none.** Mesh Rotation and Left Mesh Rotation are 0 / 0 / 0 on all four, and the weapon data has no location offset field at all.

**Across all 25 weapon data assets, two carry a rotation fix:**

| Weapon data | Mesh | Fix |
|---|---|---|
| `DA_Weapon_Tomahawk` | Fantasy pack `Axe` | Mesh Rotation pitch 180 |
| `DA_Resonator_Equpment_Sword` | CombatMaster `SM_Sword_DSAS` | Mesh Rotation pitch 180 |

The other fix-ups live in the sockets: one socket per weapon type, each with its own location, and each body with its own rotations (section 2).

## 4. Paragon

`/Game/Imports/Paragon/` contains only `ParagonAnimations/` (Aurora, Belica, Countess, Crunch, Dekker, Drongo, Feng Mao and others, retargeted to Manny). A search of the folder found **no static mesh and no skeletal mesh**, and nothing with "weapon" in its name. The brain's belief is confirmed.

## In plain words

**Which packs agree.** StylizedWeapons agrees with itself across both of its packs and every kind sampled. CombatMaster and the Fantasy pack loosely agree with each other (long axis Z, blade to -Z) for swords and daggers, and disagree with StylizedWeapons on everything.

**The most common pattern.** "Runs along Y, business end to +Y, origin at the grip, one `Root` bone, no sockets." That is StylizedWeapons: about 110 meshes, including all four the game uses.

**What breaks it.**
- Inside StylizedWeapons: fist weapons run along Z (they wrap the hand, so this is reasonable), and long two-handers (staff, 2H hammer, one 2H axe) have the origin near the middle of the shaft, not near the butt.
- CombatMaster: blades point -Z; `SM_Axe` runs along Y and is centred; `SM_GreatSword` has its edges turned 90 degrees from the other swords; the two shields face different ways.
- Fantasy pack: blades along Z; guns along X; `Axe` is centred; `Ash_Crusade_Scythe` has its origin at the bottom end and `Adventurer_Sword` just off one end.
- Shields: three different facings across the three packs (X in Stylized, Y or Z in CombatMaster, Y in Fantasy).

**One rule that would work.**
1. Adopt the StylizedWeapons pattern as the standard: the weapon runs along **+Y** from the grip, the **origin is where the main hand holds it**, the cutting edge faces Z, and a shield's face points along X with the origin at the centre of its back.
2. Use **one socket per hand** on every body (Quinn already has `hand_r_weapon` and `hand_l_weapon`), plus one shield socket, with the same transform on Manny and Quinn. Per-type sockets then go away, and the seven unmapped weapon types get a mesh for free.
3. A mesh that does not follow the standard is fixed **once at import** (re-export it rotated and re-origined), not with a per-weapon rotation. Every CombatMaster and Fantasy mesh would need this; no StylizedWeapons one-hander would.
4. Two-handed and fist weapons need one extra decision each: where the main hand sits on a long shaft, and whether a fist weapon keeps its Z axis.

## What this does not tell you

- **Whether weapons look right in the hand today.** That needs eyes in PIE. The sockets were read, not seen. In particular, Manny and Quinn disagree on rotation, so at least one of them is likely holding weapons at a different angle.
- **Which end is the tip where the origin is centred** (the two centred axes, the staff, the bows). Bounds alone cannot say.
- **Which way the cutting edge faces** on axes and curved blades; only the box was measured.
- **Sockets on static meshes.** The tool could not read them, so the CombatMaster and Fantasy rows say nothing about sockets either way. Skeletal sockets were read (none on any weapon).
- **Skeletal bounds can be padded by a few centimetres**; treat the Stylized numbers as close, not exact.
- **Only a sample was measured:** 19 of about 110 StylizedWeapons meshes, 7 of 12 CombatMaster, 11 of 109 Fantasy.
