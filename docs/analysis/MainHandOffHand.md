# Main hand + off hand (survey, 2026-11-12)

**Status:** a survey and proposal for Crown. Docs only, no code changed. Branch `docs/main-off-hand` (from `dev` at `e098a07c3`).

**Crown's ask (2026-11-12, defaults approved):** the primary weapon splits into **main hand + off hand**, WoW style.
- The off hand holds **a shield**, **a second one-handed weapon of the same family**, or **nothing**.
- **Abilities and the crystal come from the main hand.** The off hand adds its own stats: a shield adds guard, a second weapon adds damage.
- **Two-handed weapons fill both hands.**
- The **main + off hand pair picks the anim set** (meshes stay shared per family).
- **Shields become their own item type.**

## 0. What exists today

- One weapon per slot. `FCombatLoadout::PrimaryWeapon` and, for a Generic, `SecondaryWeapon` (the Switch Weapon pair, CL-03 / CL-18). See `Public/Loadout/FCombatLoadout.h:124-167`.
- **Off hand is only visual.** `UWeaponData::WieldMode` (Single / Dual / OffHandShield) plus `LeftHand*Mesh` spawn a left mesh on the same weapon asset (`WeaponMeshComponent.cpp:94-144`). The off hand has no item, no stats and no instance.
- **No 1H / 2H field.** Two-handedness lives only in the animations (`EWeaponWieldMode.h` comment). `EWeaponType` is the family (Sword, Greatsword, Spear, …).
- **No shield item.** `EGearKind` is Weapon / Adapter only (`FGearInstance.h:27`). Guard is a flat `BlockReduction = 0.5` (`DefenseSystem.h:443`).
- **Anims are per weapon asset**, not per pair: `WeaponStance`, `DrawMontage`, `SheatheMontage`, `ParryMontage` (`WeaponData.h:71-98`). The stance is read by `UCombatAnimInstance::GetDesiredStance` → `LoadoutComponent::GetCurrentStance` (`LoadoutComponent.cpp:3020`).
- **Wield mode does gate abilities.** `UAbilityData::RequiredWieldMode` is checked in `LoadoutRules.cpp:184`, although Rules CL-23 says wield mode "changes no rule". This doc treats that as a Rules row to fix.

## 1. Impact on loadouts

| Area | What changes | Where |
|---|---|---|
| **Slots** | Each weapon slot becomes a **hand pair**: main hand (an `FWeaponLoadoutEntry`, as today) plus an off hand of None / Weapon / Shield. A 2H main hand locks the off hand to None. A Generic's secondary slot becomes a second pair, so Switch Weapon swaps pairs. Caster / Resonator get the off hand on their weapon primary only. An adapter or evolution primary has no off hand. | `FCombatLoadout.h`, `FSavedLoadout.h`, `EWeaponSlotType.h` |
| **Validation** | New rules: an off-hand weapon must be 1H and the **same family** as the main hand. A shield needs a 1H main hand. A 2H main hand means no off hand. Off-hand items count toward the class item limits and the S-tier count (`LoadoutRules.cpp:394`, `:568-631`). `RequiredWieldMode` becomes "requires shield / requires dual / any", read from the pair, not the asset. | `LoadoutRules.cpp/.h` |
| **UnifiedLoadout** (D-110) | `FGearInstance` / `EGearKind` gets a **Shield** kind, and `FLoadoutSlot` an off-hand slot. B-301..B-309 are mid-flight (B-302..B-305 at REVIEW, B-309 deletes the old path). **Land the off hand after B-309**, so it is built once, on the new slots only. | `FGearInstance.h`, `FLoadoutSlot.h`, `docs/analysis/UnifiedLoadout_2026-10-29.md` |
| **Crystal and ability slots (B-098)** | Unchanged in principle: the main hand's element socket, stone socket, tier slots and converted gem slots (`FWeaponLoadoutEntry::GetSkillSlotCount`) give the abilities. A 1H weapon placed in the off hand gets **no ability slots**, and its crystal / stone sit unread (it keeps them, so moving it back to the main hand restores them). B-410's element limit applies to the main hand only. | `FWeaponLoadoutEntry.cpp` |
| **Switching** | Switch Weapon (CL-18, Generic) swaps the whole pair. "Which weapon is active" (CL-17, `GetActiveWeaponLoadout`) still returns the main hand. The off hand adds to the active pair only. | `LoadoutComponent.cpp:2869-3454`, `FCombatCapabilities.cpp:232` |
| **Guard (B-392)** | With a shield, the weapon guard plays the **pair's** guard anim and adds the shield's guard. The unarmed guard ignores the shield. `GuardWeaponId` stays the main hand's id. | `LoadoutComponent.cpp:4159-4222`, `DefenseSystem.h` |
| **Saves** | Off-hand asset, instance and kind per pair in `FSavedLoadout` / `FSavedCustomLoadout`. Shields go in the gear list as a new kind. Save version bump: dev wipes older saves (D-060, `MIN_SUPPORTED = LATEST`), so no migration. | `WoRSaveTypes.h:360-530`, `SaveSubsystem.cpp`, `WoRSaveGame.h:15` |
| **Co-op** | Nothing is replicated per property. Loadouts reach guests as a save copy (`CoopCheckpoint.h`), so the save fields carry the off hand. The guest guard RPC (`ServerSetGuard`) is unchanged. The run-gear record (`PartySessionSubsystem::SetRunMemberGear`) needs the off hand. A 2-player PIE check covers it. | `CoopCheckpoint.h`, `PartySessionSubsystem.cpp:540` |
| **UI placeholders** | There is no C++ loadout screen (the loadout UI is Blueprint, an editor job). The battle HUD shows "Main / Off" in the weapon line (`BattleHUDView.cpp:319`). The party card's run weapon cycler (`PartyControlWidget.h:70-116`) and the durability header list the off hand. Console `wor.Loadout.SetOffHand` until the screen exists. | `BattleHUDView.cpp`, `PartyControlWidget`, `DurabilityHeaderWidget.cpp` |
| **Meshes and anims** | The off-hand mesh comes from the off-hand **item**, not from `LeftHand*Mesh` on the main weapon. Those fields and `WieldMode` become OBSOLETE (kept until PIE). The **anim set** is chosen by (main family, off kind: none / shield / weapon / 2H) from one new data asset, replacing the per-weapon stance / draw / sheathe / parry fields as the source. | `WeaponMeshComponent.cpp`, `CombatAnimInstance.cpp`, `WeaponData.h` |
| **Data** | Every weapon asset needs a **Hands** value (1H / 2H). Each family needs its anim sets authored. Shield assets need creating. These are editor steps, not code. | Content (LFS) |

## 2. Stat values

**What a weapon's stats are today:**
- `Power`, authored at the asset tier (reference **F 40, E 52, D 68, C 88, B 114, A 148, S 192**, BaselineNumbers.md and `WoRWeaponPower::GetReferencePower`).
- `StatScaling` grades.
- Rolled `StatBonus` / `ResistanceBonus` on a **fixed 20-point substat budget** (`CombatConstants.h` `FIXED_SUBSTAT_BUDGET`).
- Durability.
- There is **one ladder for every weapon**: a dagger and a greatsword of the same tier get the same reference Power.

**Recommendation: WoW's rule.** A 2H weapon's budget ≈ main hand + off hand, so a full pair is never stronger than a 2H. **Main hand 65%, off hand 35%**:
- A 2H weapon keeps today's numbers.
- 1H weapons are re-authored to the main-hand column.
- An off-hand weapon uses its **off-hand value** when it sits in the off hand: the same item carries one Power, and in the off hand it counts at 35/65 of it. One item works in either hand, as in WoW.

| Tier | 2H Power (today's ladder, unchanged) | Main hand 1H (65%) | Off-hand weapon (35%) | Main + off |
|---|---|---|---|---|
| F | 40 | 26 | 14 | 40 |
| E | 52 | 34 | 18 | 52 |
| D | 68 | 44 | 24 | 68 |
| C | 88 | 57 | 31 | 88 |
| B | 114 | 74 | 40 | 114 |
| A | 148 | 96 | 52 | 148 |
| S | 192 | 125 | 67 | 192 |

- **A 1H with nothing in the off hand** hits for 65% of the weapon part. That is the price of a free hand: no shield, no second weapon. Crown can also give an empty off hand a small bonus (not recommended for now).
- **Shield off hand:** its 35% goes to **guard, not Power**. Proposal: the shield adds to `BlockReduction` (today a flat 0.5) by tier, **F +0.05, E +0.07, D +0.09, C +0.11, B +0.13, A +0.16, S +0.20** (cap 0.75 total), `TODO: tune`. Sword + S shield therefore blocks 70% of a guarded hit instead of 50%, for 35% less weapon damage.
- **Rolls:** the substat budget splits the same way. **2H 20, main hand 13, off hand 7** (rounded 65 / 35 of `FIXED_SUBSTAT_BUDGET`). Each item rolls on its own budget.
- **Scaling grades:** the main hand's grades scale the main hand's part, and the off hand's grades scale its own part.
- **Requirement factor:** each hand's requirement cuts only its own part, as today (D-119's x0.6).

**What it does to current numbers:**
- A **2H** weapon: nothing changes.
- A **1H** weapon authored on today's ladder is 54% too strong as a main hand (40 vs 26 at F) and becomes **~35% stronger than a 2H** once an off hand is added (40 + 14 = 54 at F). So every 1H asset's `Power` is re-authored to the main-hand column.
- **Today's real Power values cannot be read here:** they live in LFS `.uasset` files. Run `wor.Weapon.Print` in the editor to list them. Per BaselineNumbers.md, Power defaulted to 0 (no weapon part) until authored, so assets still at 0 change nothing.
- **Code side:** `GetReferencePower(Tier)` gains the hands split, so the authoring guide and debug line match the table above.

## 3. Proposed jobs (each ≤3 files; each updates its Architecture doc and Rules row)

Proposed Backlog rows (all `TODO`, all need **D-174**). Order: B-412 → B-413 → B-414 → B-415 / B-416 / B-417 → B-418 → B-419 / B-420 → B-421.

| ID | Job | Who | Size | Needs | Done when | Status |
|---|---|---|---|---|---|---|
| B-412 | **Weapon hands (D-174)**: `EWeaponHands` (OneHand / TwoHand) on `UWeaponData` + `IsTwoHanded()`; `wor.Weapon.Print` shows it; a data validation warning when unset on a 2H family | CC | S | D-174 | Spec; every weapon asset prints its hands; UE step: author Hands on each weapon asset | TODO |
| B-413 | **Shield item type**: `UShieldData` (tier, Guard, meshes, durability) + `ShieldDataDebug` (CallInEditor print) | CC | S | D-174 | Spec: a shield's guard by tier; debug print | TODO |
| B-414 | **Off-hand slot**: `FCombatLoadout` / `FSavedLoadout` get an off hand (None / Weapon / Shield + entry) per weapon pair; a 2H main hand clears it; Switch Weapon swaps pairs | CC | M | B-412, B-413, B-309 | Spec: set / clear / swap; `wor.Loadout.SetOffHand` | TODO |
| B-415 | **Off-hand rules**: `LoadoutRules` (same family 1H, shield needs a 1H main hand, 2H = no off hand, item limits, S-tier count); `RequiredWieldMode` read from the pair | CC | M | B-414 | Spec: each refusal with its reason; Rules CL-23 corrected | TODO |
| B-416 | **Hand split damage**: `WoRWeaponPower` weapon part = main hand + off-hand weapon × 35/65; `GetReferencePower(Tier, Hands)` with the §2 table; `wor.Weapon.Print` shows both parts | CC | M | B-414 | Spec: 2H = MH + OH at every tier; 1H alone = 65% | TODO |
| B-417 | **Shield guard**: Guard adds the shield's guard to `BlockReduction` (cap 0.75) with a weapon guard only | CC | S | B-414 | Spec: guarded hit with / without a shield, unarmed guard ignores it | TODO |
| B-418 | **Saves + pool**: shields in the gear list (new kind), off hand per saved / custom loadout, run-gear record; save version bump | CC | M | B-414 | Save round-trip spec; 2-player PIE check (guest sees the off hand) | TODO |
| B-419 | **Pair anim sets**: `UWeaponAnimSetData` keyed by (family, off kind); `GetCurrentStance` / `GetGuardMontage` / draw / sheathe read it, the per-weapon fields as fallback (OBSOLETE) | CC + UE | M | B-414 | Spec on the lookup; UE step: author one set per family | TODO |
| B-420 | **Off-hand mesh**: `WeaponMeshComponent` spawns the off-hand item's mesh; `LeftHand*Mesh` / `WieldMode` OBSOLETE (kept until PIE) | CC | S | B-414 | PIE: sword + shield, dual daggers, greatsword look right | TODO |
| B-421 | **UI placeholders**: battle HUD "Main / Off", party card cycler, durability header list the off hand | CC | S | B-414 | PIE check; the real loadout screen stays a UE / CommonUI job | TODO |
| B-422 | **Sweep**: delete `WieldMode`, `LeftHand*Mesh` and the per-weapon anim fields after PIE | CC | S | B-419, B-420 PIE | No reads left; Blueprint check | TODO |

**Unity build:** B-414 touches the loadout struct used everywhere. Run the non-unity check build (`WOR_BUILD_ARGS=-DisableUnity`) once on it.

## 4. Draft decision (for `docs/DesignDecisions.md`)

```
### D-174 — Main hand + off hand (WoW style)
**Status:** DECIDED (Crown, 2026-11-12; defaults approved) · **Raised:** 2026-11-12 · **Blocks:** B-412..B-422
**Decision:**
- Each weapon slot is a hand pair: a main hand and an off hand. The off hand holds a shield, a second one-handed
  weapon of the same family, or nothing. A two-handed weapon fills both hands.
- Abilities, the element crystal and the stone socket come from the main hand only (B-098's converted gem
  slots included). An off-hand weapon keeps its crystal but it is not read there.
- The off hand adds its own stats: a second weapon adds damage, a shield adds guard.
- The main + off hand pair picks the anim set; meshes stay shared per family.
- Shields are their own item type.
**Open (proposed defaults, Crown to confirm):**
1. Stat split: WoW's rule, a 2H's budget = main + off; main hand 65%, off hand 35% (Power, substat budget 20 → 13 / 7).
   1H assets re-authored to the main-hand column (MainHandOffHand.md §2).
2. Shield guard: + block reduction by tier F 0.05 .. S 0.20, cap 0.75 total.
3. A Generic's secondary slot becomes a second pair; Switch Weapon swaps pairs.
4. Built after UnifiedLoadout's B-309, on the new slots only. B-309 is BLOCKED on Crown's PIE of B-301..B-308,
   so B-414 onward waits on that PIE (B-412 and B-413 can start now). Alternative: build on today's
   PrimaryWeapon / SecondaryWeapon and port in B-309 (twice the slot work).
**Notes for Claude:** survey and job split in docs/analysis/MainHandOffHand.md.
```
