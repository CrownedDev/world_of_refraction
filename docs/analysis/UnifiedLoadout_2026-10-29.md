# Unified Loadout (D-110) — analysis

**Status:** proposal (2026-10-29), being decided with Crown one question at a time. Weapons, adapters and the save already share most of their shape; the deckbuilder already uses kind-agnostic slots.

---


Status: proposal, 2026-10-29, read-only analysis of `dev`. Nothing here is decided; §5 lists what Crown rules on.
Paths are relative to `Source/world_of_refraction/` unless they start with `docs/`.

## 0. The short version

Today a character's gear lives in **three hand-written paths** that hold the same thing: a gear instance
(asset + tier + quality + one attachment + rolled stats) plus "what this gear lets me use". The primary
weapon, the secondary weapon and the Resonator's ring loadout differ in **three real ways** (weapons give
abilities, rings give spells; a ring auto-switches when its crystal breaks; a weapon drives stance, mesh and
EP) and in **about a dozen accidental ways** (copy-paste drift). The save file already treats a weapon and a
ring as one `FSavedOwnedItem`; the data assets already share one base class. The proposal is to make the
runtime do the same: **one `FGearInstance`, one `FLoadoutSlot`, one per-class slot table**, with the active
slot per kind. The deckbuilder already made this move ("three free slots, each taking a Weapon, a Ring or an
Apparel in any mix, at most 2 Weapons": `wor-deckbuilder/docs/reference/01-card-model.md:372-377`), so it
is the reference-backed default.

## 1. Today's three paths side by side

| What | Primary weapon slot | Secondary weapon slot | Ring / adapter slot (primary ring or Resonator ring N) |
|---|---|---|---|
| Storage | `FCombatLoadout::PrimaryWeapon` (`FWeaponLoadoutEntry`), `Public/Loadout/FCombatLoadout.h:115` | `SecondaryWeapon`, `:150`, gated by `SecondarySlotType` `:146` | `PrimaryRing` `:119` (Generic/Caster) or `RingLoadout[]` `:156` + `ActiveRingIndex` `:187` (Resonator) |
| Instance struct | `FWeaponInventoryEntry` (`Entries/FWeaponInventoryEntry.h:36-115`): asset, `PersistentID`, `Tier`, `Quality`, `AttachedItem`, `AssignedSpells`, `StatBonus`, `ResistanceBonus`, 4 roll pools | same struct | `FRingInventoryEntry` (`Entries/FRingInventoryEntry.h:40-120`): **field-for-field the same** (asset type differs; category label "Engravings" vs "Mastery") |
| Loadout wrapper | `FWeaponLoadoutEntry` (`FWeaponLoadoutEntry.h:35-50`): `AssignedAbilities`, `AssignedAugmentStoneAbilities`, `AssignedSpells`, `OverrideAttack` | same | `FRingLoadoutEntry` (`FRingLoadoutEntry.h:28`): only wraps the entry; spells live on the entry |
| Data asset | `UWeaponData : UEquipmentDataBase` (`Equipment/Weapons/WeaponData.h:31`): `WeaponType`, `WieldMode`, `InfusionMode`, `WeaponAttack`, `PresetAbilities`/`bAbilitiesLocked`, stance, draw/sheathe/parry montages, meshes | same | `URingData : UEquipmentDataBase` (`Equipment/Rings/RingData.h:23`): `InfusionMode`, `PresetSpells`/`bSpellsLocked`, `RingStaticMesh` (**no reader anywhere**) |
| Shared base | `UEquipmentDataBase` (`Equipment/EquipmentDataBase.h:76`): name, tier, `AttachedItem` `:113`, `DefaultSpells` `:121`, `DefaultAbilities` `:128`, stat/resistance rolls, effects, bound gear, requirements | | same base |
| Attachment | one `FRuntimeAttachedItem` (crystal / stone / fusion / evolution), CL-30 | same | same struct; rings refuse bare stones and two-stone fusions (CL-28, `if constexpr (IsRingGear)` in `Private/Inventory/InventoryComponent.cpp`, B-026) — **real** |
| Spells | `AssignedSpells` through the crystal (`FWeaponLoadoutEntry::GetAllSpells`), to be removed by B-098 (D-027: weapons never give spells) | same (`ResolveSpellSource` walks it: `Private/Loadout/LoadoutComponent.cpp:2096-2108`) | the ring's purpose: `GetAllSpells`, cap by tier/gem (`ResolveSpellSlotCap`, `CrystalEffectTable.h:771`) — **real** |
| Abilities | `GetAllAbilities` + stone abilities (CL-20, CL-31) | same, only when active (`FCombatCapabilities.cpp:50-77`) | none — **real** (D-108 says adapters also key elemental abilities; that's the weapon's abilities taking the adapter's element, not abilities on the adapter) |
| Element | `AttachedItem.GetElement()` with **no broken check** (`FWeaponInventoryEntry.h:150-153`) | same | `GetElement()` returns None when broken (`FRingInventoryEntry.cpp:13-20`, CL-24) — **drift** (S4) |
| Infusion source | `WeaponCrystal`, only the **active** weapon (`Private/Combat/Actions/ActionExecutor.cpp:4016-4025`) | never a source unless shown | `ActiveRing` (Resonator) / `PrimaryRing` (others): two enum values for one idea (`Public/Infusion/EInfusionSourceOption.h:38-41`, `:3993-4014`) — **accident** |
| Infusion mode | `Weapon->InfusionMode` (`ActionExecutorCosts.cpp:255-300`) | n/a | `Ring->InfusionMode`, same switch — **accident** |
| Active / switch | Generic dual: `bShowPrimary` (`GetActiveWeapon` `:2528-2588`, duplicated in `GetActiveWeaponLoadout` `:3179-3231`, A2); `ToggleEquipment` `:2876-2921`; `TrySwitchWeaponThisTurn` `:3360` | the other half of the toggle | `ActiveRingIndex`; `SetActiveRingIndex` `:3734-3783` is **the same body** as `ToggleEquipment` with one line changed; `TrySwitchRingThisTurn` `:3375`; `SwitchToNextUsableRing` `:3805` — **accident**, except the crystal-break auto-switch (`HandleCrystalBroken` `:3828`, rings only) — **real** |
| Stats / effects | counted per class in 5 hand-mirrored switches (`:4082`, `:4241`, `:4343`, `:4434`, `:4516`, A1); Generic dual counts the shown weapon only | Generic ring-primary counts ring **+** secondary (`:4155-4164`) | Resonator counts active ring + primary weapon (`:4172-4182`); Caster ring primary counts the ring |
| Validation | `CheckPrimaryWeapon`: ownership, spell coherence, spell cap, ability counts (`Private/Loadout/LoadoutRules.cpp:147-218`) | ownership + ability counts **only**, no spell checks (`:403-411`) — **accident** | `CheckRingSpellCoherence` + `CheckRingSpellCap` per ring (`:45-125`) |
| Budget cost | bare 1 / attached 2 / evolution 3 (`Inventory/InventoryConstants.h:57-66`), counted only for a Resonator (`LoadoutRules.cpp:220-233`, `:413-437`) | none | 1 / evolved 2 (`:68-70`); Resonator total ≤ 5 (`Loadout/LoadoutConstants.h:31`) |
| Wear / break | crystal wears per cast; break stays slotted, no switch; broken ⇒ no infusing, no elemental abilities (D-083) | same | wear same; break auto-switches (`:3828`) and keeps F-tier spells (D-083, `SourceBreakRules.h`) — **real** |
| Mesh / stance / EP | `UWeaponMeshComponent` follows `OnActiveWeaponChanged` (B-138); stance `GetCurrentStance` `:3027`; EP only with an active weapon (`Private/Character/CharacterDataComponent.cpp:600-611`, CL-06) | same when shown | none shown; `GetRingSwitchMontage` `:4033` — **real** (form drives it after D-107) |
| Saves | `FSavedOwnedItem` (`Public/Save/WoRSaveTypes.h:255-276`) **one struct for both**; `FSavedPool::Weapons` / `Rings` `:323-326`; `FSavedPartyMember::RunWeapon` / `RunRing` `:111-114`; carried gear is holder-generic (`FSavedCarriedGear` `:60-78`) | | same struct; separate list |
| Authoring | `FSavedLoadout::PrimaryWeapon*` (`Public/Loadout/FSavedLoadout.h:182-231`) | `Secondary*` `:244-269` | `PrimaryRing*` `:206`, `EquippedRings` (`FResonatorRingSlot` `:100-119`) |
| Pool / inventory | `UInventoryComponent::AddWeapon / RemoveWeapon / RemoveWeaponByPersistentID / Attach*ToWeapon` (`Public/Inventory/InventoryComponent.h:226-302`); `UPoolSubsystem::OwnedWeapons` | same | `AddRing / ... / Attach*ToRing` (`:308-370`); `OwnedRings`; the A12 economy pairs |

Real differences (keep): abilities vs spells; auto-switch on break and F-tier spells for adapters; stance,
mesh, basic attack and EP for weapons; ring attachment restrictions. Everything else in the table is the same
rule written two or three times, and already drifting (broken element, secondary spell checks, stat
coverage).

Size of the duplication: 2457 `Ring` mentions in 112 files (157 distinct identifiers); `FRingInventoryEntry`
123 uses vs `FWeaponInventoryEntry` 151; `FRingLoadoutEntry` 84 vs `FWeaponLoadoutEntry` 67; `RingLoadout`
223 uses in 31 files; `SecondaryWeapon` 137 in 21; `bShowPrimary` 46; `ActiveRingIndex` 54. Getter callers:
`GetActiveWeapon` 38 (11 files), `GetActiveRing` 13, `GetPrimaryRing` 17, `GetActiveWeaponLoadout` 20,
`GetActiveRingLoadout` 12, `GetPrimaryRingLoadout` 13.

## 2. The unified model

### 2.1 Structs

```
UENUM EGearKind : uint8 { Weapon = 0, Adapter = 1 }          // append-only; Adapter = today's ring

USTRUCT FGearInstance                                         // replaces FWeaponInventoryEntry + FRingInventoryEntry
  TObjectPtr<UEquipmentDataBase> Asset;                       // UWeaponData or URingData (later UAdapterData)
  EGearKind Kind;                                             // derived from Asset's class at mint, stored for saves
  FGuid PersistentID; int32 InstanceID; EItemTier Tier; EItemQuality Quality;
  FRuntimeAttachedItem AttachedItem;                          // crystal / stone / fusion / evolution — THE answer to
                                                              // "where do weapons get attached": on the instance, as today
  TArray<USpellData*> AssignedSpells;                         // adapters; empty on weapons once B-098 lands
  FEquipmentStatBonus StatBonus; FResistanceBonus ResistanceBonus; StatPool/MaxPool, ResistancePool/MaxPool;

USTRUCT FLoadoutSlot                                          // replaces FWeaponLoadoutEntry + FRingLoadoutEntry
  FGuid GearID;                                               // the owned instance (owned is the truth, B-085)
  FGearInstance Gear;                                         // cache of it, as today's entries are
  TArray<UAbilityData*> AssignedAbilities, AssignedAugmentStoneAbilities;   // read only when Kind == Weapon
  TArray<USpellData*> SpellOverrides;                         // today's loadout-level AssignedSpells (adapters)
  TObjectPtr<USkillDataBase> OverrideAttack;                  // weapons
  EGearKind Kind() const { return Gear.Kind; }  bool IsEmpty() const;

FCombatLoadout
  TArray<FLoadoutSlot> Slots;                                 // replaces PrimaryWeapon / SecondaryWeapon / PrimaryRing / RingLoadout
  int32 ActiveWeaponSlot = INDEX_NONE;                        // replaces bShowPrimary (which weapon is in hand)
  int32 ActiveAdapterSlot = INDEX_NONE;                       // replaces ActiveRingIndex (and "the primary ring")
  FEvolutionAttachment PrimaryEvolution; FGuid PrimaryEvolutionInstance; TArray<USpellData*> EvolutionSpells;   // the evolution SEAT stays
  InnateSpells, BDSpellPools, ItemSlots, bAutoEquipItemsOnCombatStart;                                          // unchanged
```

The evolution seat stays separate: an evolution in the primary slot is a crystal with no holder, destroyed
on removal (CL-33), with its own spells and pillar changes. It is not gear; it only **uses up one slot** of
the class table (so a Caster with an evolution seated has no weapon and a Generic keeps one weapon, as today).

### 2.2 Per-class slot table (one constexpr table in `LoadoutRules.h`, read by rules, UI and the editor)

| Class | Slots | Max weapons | Max adapters | Evolution seat uses a slot | Budget |
|---|---|---|---|---|---|
| Generic | 2 | 2 | 1 (today's ring primary; Q-5 in §5) | yes | 5 (Q-4) |
| Caster (Refractor) | 1 | 1 | 1 | yes | 5 (never reached) |
| Resonator | 1 + N | 1 | N (no count cap, CL-11) | yes | 5 (CL-10, the existing rule) |

Crown's words fit this: Generic = 2 weapons (the weapon cap), Caster = 1 weapon or 1 adapter (one slot,
either kind), Resonator = its adapters plus a weapon under the budget. Rows are data, so B-116's run-start
limits ("1 weapon / 2 rings / 1 evolution early, growing with perks") become the same table read with the
account's perks, instead of a second rule set.

### 2.3 One cost function for every class

`int32 FLoadoutRules::SlotCost(const FLoadoutSlot&)` = today's numbers (weapon bare 1 / attached 2 /
evolution 3; adapter 1 / evolved 2; evolution seat 2; `InventoryConstants.h:57-70`, `LoadoutConstants.h:34`)
and `Σ SlotCost ≤ Budget(class)`. The budget row is what "branches the cost rules out": today it is checked
for a Resonator only (`LoadoutRules.cpp:413-437`, CL-12); with a table it is checked for everyone with the
same code. Reference: Warframe's mod capacity and Path of Exile's socket budget apply the same point cost to
every build regardless of class, and players read that as fair; a class-specific exemption is the thing
that needs explaining. With 5 for all, the only shape newly refused is a Generic carrying **two**
evolution-bearing weapons (3 + 3) — see Q-4.

### 2.4 Active slot per kind

- `const FLoadoutSlot* GetActiveSlot(EGearKind) const` — `Slots[ActiveWeaponSlot]` / `Slots[ActiveAdapterSlot]`
  when valid and of that kind, else the **first** slot of that kind (so a Generic or Caster with one adapter
  needs no "primary ring" concept: its one adapter is the active adapter). This one function replaces the ten
  getters of A2; `GetActiveWeapon()` etc. become one-line forwards, so the 38 + 13 + 17 callers don't move.
- `bool TrySwitchActiveSlot(EGearKind, int32 NewSlot, FText& OutReason)` — replaces `ToggleEquipment`,
  `SetActiveRingIndex`, `TrySwitchWeaponThisTurn`, `TrySwitchRingThisTurn`: one free switch per turn
  (ET-19, B-175) for either kind; the turn-manager speed notify, `SwapGearEffects` (D-008) and
  `RecomputeMaxPools` written once. `SwitchToNextUsableSlot(Kind)` serves the crystal-break auto-switch;
  the per-kind rule is one flag in the table (`bAutoSwitchOnBreak`: adapters yes, weapons no, because a
  weapon still attacks with a broken crystal, D-083).
- "Switch Weapon" / "Switch Adapter" menu buttons = `CountSlots(Kind) > 1` (CL-18, CL-26 unchanged).
- Stance, mesh, EP target and basic attack read `GetActiveSlot(Weapon)`; the infusion source `Weapon` reads it
  too. The non-active weapon contributes nothing (Q-2).

### 2.5 Everything that reads "which gear counts" becomes one visitor

`ForEachContributingGear(TFunctionRef<void(const FLoadoutSlot&)>)` = the active weapon slot, the active
adapter slot, and (Q-3/A1's answer, reference-backed default yes) nothing else; the evolution seat is added
by the caller as today. The five per-class switches (A1) and the debug print collapse onto it, and the
drift in §1 (ring-primary Generic counting ring + secondary, dual Generic counting the shown weapon only for
stats but `GetActiveWeapon()` for effects) ends because there is one list.

### 2.6 Infusion and spell sources

`EInfusionSourceOption` keeps its ordinals and gains nothing: `ActiveRing` and `PrimaryRing` both resolve
to `GetActiveSlot(Adapter)` (the two values are kept, `PrimaryRing` marked obsolete, until the UI stops
offering it), `WeaponCrystal` to `GetActiveSlot(Weapon)`. That is S4's `ResolveInfusionSource(Actor,
Option)` returning {slot, attachment, element through `FGearInstance::GetElement()` (broken ⇒ None, one
rule for both kinds), `InfusionMode` from the asset, usable}. `ResolveSpellSource` walks `Slots` once:
Innate → adapter slots → weapon slots (until B-098 removes weapon spells) → evolution. `GetBrokenSourceReason`
/ `IsSpellFromBrokenRing` become `GetBrokenSourceReason(Spell)` over the slot that holds the spell, with the
kind deciding the reason text and the F-tier allowance.

### 2.7 Adapter form is data on the adapter asset

`EAdapterForm { Ring = 0, Pistol, Trainer, … }` on `URingData` with a `TSoftObjectPtr` mesh and a cast
montage (B-298 as planned, D-057). The slot never reads the form; only the mesh/animation layer does, the
way `WieldMode` is visual-only on weapons (CL-23). A pair (dual pistols, a pair of trainers) is **one**
adapter item of a dual form, like a Dual-wield weapon is one `UWeaponData` (Q-6).

**Ares Citrine** (Resonator): `Slots = [ {Weapon: his weapon, crystal or not}, {Adapter: Trainers, Lightning
crystal}, {Adapter: Trainers, Lightning crystal} ]`, `ActiveWeaponSlot 0`, `ActiveAdapterSlot 1`; cost
1–2 + 1 + 1 ≤ 5; "Switch Adapter" offered (2 adapters); a Lightning break auto-switches to slot 2.
**Amethyst** (Resonator or Caster): `Slots = [ {Adapter: Dual Pistols form, crystal X} (+ a weapon slot if
Resonator) ]`; he casts resonance refractions through the pistols' cast montage; no weapon means no basic
attack (the kept rule in `docs/Rules/01` Gaps).

## 3. What it cuts (estimate, from the line counts in §1 and `wc -l`)

| Collapses | Today | After | Removed (approx.) |
|---|---|---|---|
| Instance structs `FWeaponInventoryEntry` (222 h) + `FRingInventoryEntry` (219 h + 62 cpp) | 2 | `FGearInstance` ~240 | ~260 lines |
| Wrappers `FWeaponLoadoutEntry` (162 + 391) + `FRingLoadoutEntry` (96 + 166) | 2 | `FLoadoutSlot` ~380 | ~430 |
| A2 getter pairs (10 bodies, 49 preambles) in `LoadoutComponent.cpp` | 10 | 2 + forwards | ~250 |
| Switch bodies (`ToggleEquipment`, `SetActiveRingIndex`, 2× `TrySwitch…`, `SwitchToNextUsableRing`) | 5 | 2 | ~90 |
| A1 per-class readers (5 × 80–150 lines) + debug copy | 6 | visitor + 6 short callers | ~300 |
| `FLoadoutRules`: `CheckPrimaryWeapon` / ring checks / class branches in `CollectFindings` (`:147-447`) | per kind + per class | per slot + table | ~150, and the secondary weapon gets the missing spell checks |
| `FSavedLoadout` → `FCombatLoadout` resolve lambdas (`FCombatLoadout.cpp:318-377`) | 2 | 1 | ~60 |
| Inventory pairs A13 (`Add/Remove/RemoveBy…` ×2) + B-026's `if constexpr` branches | 2 | 1 list `OwnedGear` | ~200 |
| Economy pairs A12 (`Dismantle/LevelUp/Downgrade` ×2) | 2 | 1 | ~150 |
| Pool lists + save lists (`OwnedWeapons/OwnedRings`, `FSavedPool::Weapons/Rings`) | 2 | 1 | ~60 |
| S4 infusion switches (5), `ActiveRing`/`PrimaryRing` split, `IsUsableRing` lambda | 5 | 1 resolver | ~100 |
| `FCombatCapabilities` primary-ring + ring-loadout blocks (`:129-191`) | 2 | 1 | ~35 |

Roughly **1,700–2,100 lines**, about a third of `LoadoutComponent.cpp` (4,638) and most of the 157 `Ring`
identifiers, before the move-only rename. Stays per kind, on purpose: ability lists and `OverrideAttack`
(weapon), `WeaponType` / `WieldMode` gates in `ValidateAbilities`, stance/mesh/montages, CL-40's
requirement factor, CL-06's EP target; spell lists, coherence, cap-by-gem, F-tier-when-broken and
auto-switch (adapter); the ring attachment restrictions (CL-28); the evolution seat.

## 4. Risks and migration

- **Saves (D-059, D-060).** The owned item is already one struct (`FSavedOwnedItem`). Change: `FSavedPool`
  gains `Gear` (list of `FSavedOwnedItem` + `EGearKind`), `FSavedPartyMember` gains `RunGear` (two guids →
  N); bump `WoRSaveVersion::LATEST` 3 → 4 (`Public/Save/WoRSaveGame.h:15`) and read the old `Weapons` /
  `Rings` / `RunWeapon` / `RunRing` into the new fields in `HandlePostLoad` (version 3 is above
  `MIN_SUPPORTED`, so it must load, not wipe). Keep the old fields until Crown's PIE (CLAUDE.md).
- **Authored assets.** `FSavedLoadout` sits inline on `UInventoryData` `.uasset`s (`InventoryData.h:114`),
  which we can't read or edit outside the editor. So `FSavedLoadout` **stays the authoring shape** and
  `CreateFromSavedLoadout` (the sole factory, `FCombatLoadout.h:82-94`) builds `Slots` from it. A new
  authoring shape (a `TArray<FSavedSlot>`) is a later, editor-side job with Crown re-saving the assets.
- **Call sites.** 2,457 `Ring` mentions and ~100 getter callers: keep every public getter as a forward
  (`GetActiveWeapon()` = `GetActiveSlot(Weapon)->Gear.Asset`), so callers move only in the final rename.
  `ULoadoutComponent` is 828 h + 4,638 cpp lines; each cluster must stay ≤ 3 files, so the work is ~9 clusters.
- **GAS stage 3 (B-042b–g).** The effect readers (`GetActiveEffectsGathered`, `SwapGearEffects`) are the ones
  GAS replaces. The visitor (§2.5) serves both the old gather and the GE path, so it can land before stage 3
  finishes; the *rename* (B-299) is blocked on B-042g already and stays so.
- **Shared armoury (B-091, B-221, B-221b).** `UPoolSubsystem::Wearers` is keyed by `FGuid` and is already
  kind-blind (`Public/Pool/PoolSubsystem.h:208`). B-221b's "every reader skips a piece worn by someone else"
  becomes one check in `GetActiveSlot` / the visitor instead of edits to each reader: merge B-221b into cluster 4.
  The unbuilt `feature/B-221-worn-gear-leaves-inventory` edits the weapon/ring lists; build and merge it (or
  drop it) **before** cluster 8, not during.
- **Adapter rename (B-298, B-299).** B-298 (player strings + `EAdapterForm`) is unchanged and goes first.
  B-299 shrinks: after the unification there are far fewer `Ring` names to move; the kind enum is born
  `Adapter`, so only the asset class and the leftover forwards need the move-only rename.
- **Unbuilt REVIEW branches.** `refactor/B-243-active-gear-rule`, `refactor/B-251-gear-templates`,
  `refactor/B-252-economy-pairs` were written in cloud sessions and never built. They do, smaller, what
  clusters 4, 8 and 9 do. Default: build and merge them first if green (their specs become the safety net
  `WoR.Loadout.ActiveGearRule`, the gear templates spec, `EconomyPreambleSpec`), otherwise close them as
  superseded. Don't run them in parallel with this work.
- **Behaviour changes to call out in PIE** (each a §5 item or a drift fix): the secondary weapon gets spell
  checks; a broken weapon crystal's element becomes None everywhere (S4, matches CL-24); Generic stats
  follow the weapon in hand (A1's Q-a); one budget for every class (Q-4).

### Incremental path (new beside old; adapters first; ≤ 3 files per cluster)

0. B-298 as planned (`EAdapterForm`, LOCTEXT "Adapter"). No model change.
1. `FGearInstance` + `ToGearInstance(FWeaponInventoryEntry)` / `(FRingInventoryEntry)` and back, + spec
   (round-trip, `GetElement` broken ⇒ None for both kinds). 3 files. Nothing reads it yet.
2. `FLoadoutSlot` + `FCombatLoadout::Slots`, `ActiveWeaponSlot`, `ActiveAdapterSlot`, mirrored from the old
   fields inside `CreateFromSavedLoadout` and kept in step by the 13 mutators that already announce through
   `FActiveWeaponChangeScope` (B-138); `wor.Loadout.CompareSlots` prints old vs new (the B-028 CompareRules
   pattern). 3 files.
3. Slot table + `SlotCost` + rules over `Slots`; findings compared with the legacy ones (same as B-028 did);
   secondary-weapon spell checks appear here. 3 files (`LoadoutRules.h/.cpp`, spec).
4. `GetActiveSlot(Kind)`; the ten getters forward; B-221b's worn-elsewhere skip lands in it. 2 files + spec.
   (Supersedes B-243.)
5. `TrySwitchActiveSlot` / `SwitchToNextUsableSlot`; old switch functions forward; command menu reads one
   capability per kind. 3 files. (Needs Q-2/Q-5 answers.)
6. `ForEachContributingGear` over `Slots`; the five readers + the debug print. 3 files. (Supersedes B-244.)
7. `ResolveInfusionSource` over the active slots; `ActiveRing`/`PrimaryRing` collapse; `ResolveSpellSource`
   and `GetBrokenSourceReason` over `Slots`. 3 files (`ActionExecutor.h/.cpp`, `ActionExecutorCosts.cpp`).
   (Supersedes B-270.)
8. `UInventoryComponent::OwnedGear` with `Weapons` / `Rings` as views; pool lists; save v4 + loader. Three
   clusters of ≤ 3 files. (Supersedes B-251, the pair half of B-252/B-252b; the `PersistentID` finder stays.)
9. After Crown's PIE: delete the old fields, entries, wrappers and legacy validators (B-172 folds in), then
   B-299's move-only rename on what's left.

Jobs this replaces or merges: **B-243, B-244, B-251, B-270** (replaced), **B-252/B-252b** (the verb pairs;
the lookup part stays), **B-221b** (merged into cluster 4), **B-098** (becomes "weapon slots carry no spells",
cluster 1/2 or before), **B-299** (shrinks, stays last), **B-172** (cluster 9), **B-116**'s slot limits (read
the table). **B-296** is a Generic-spell price fix in `EconomyService` and is unrelated: leave it.

## 5. Decisions for Crown (each with the reference-backed default; non-lore, so they ship with the default unless Crown changes one)

- **Q-1 Does the Caster's one slot take either kind (weapon or adapter), with the evolution seat using it?**
  Default **yes**: today's CL-04 already allows weapon / ring / evolution in the primary slot, and the
  deckbuilder's vessel slots take any kind (`01-card-model.md:372-377`). Nothing changes for players.
- **Q-2 Does the secondary weapon keep any special rule?** Default **no**: two equal weapon slots, one in hand.
  The non-active weapon gives no stats, effects or infusion source (Expedition 33, FF7 Remake: only the drawn
  weapon's passives apply), is still checked for ownership, abilities and (new) spells, and D-083's broken
  rule reads whichever slot holds the skill (Q-204 unchanged in effect; moot for spells after B-098).
- **Q-3 Do adapters cost by form in the Resonator budget?** Default **no**: cost by state (1 / evolved 2) as
  CL-10; form is look and animation only (D-107), like wield mode on weapons (CL-23). A pair of pistols or
  trainers is one item, one slot, one crystal, one element. Costing by form would be a new tuning axis with
  nothing in the deckbuilder behind it.
- **Q-4 One budget (5) for every class, or Resonator only (today, CL-12)?** Default **one budget for all**
  (Warframe mod capacity, PoE sockets: same cost, every class). The one shape it refuses that is legal
  today: a Generic with two evolution-bearing weapons (3 + 3 = 6). If Crown wants that shape, the table row
  says 6 for Generic — one number, no code.
- **Q-5 Generic: strictly two weapons, or two slots with at most two weapons (so one can be an adapter)?**
  D-110's words say "2 weapons"; today a Generic may wear one ring in the primary (CL-03, CL-26) and D-027
  says a Generic casts spells through rings. Default **two slots, ≤ 2 weapons, ≤ 1 adapter** (the
  deckbuilder's vessel rule), which keeps every Generic build that works today. Taking the adapter away is a
  one-cell change in the table if Crown means it literally.
- **Q-6 Can both of a Generic's weapons be active at once (both ability lists, both passives)?** Default
  **no**: one weapon in hand, one free switch per turn (ET-19, D-023's stun rule). Two active weapons would
  double passives for one class and make "Switch Weapon" meaningless.
- **Q-7 Where attachments live.** Default **on the gear instance** (as today: `AttachedItem` on the entry,
  evolution seat separate), never on the slot. The slot references the owned piece (B-085: owned is the
  truth), so a crystal socketed in the hub shows in every preset, and two presets can't disagree about what a
  weapon carries. Crystals, stones, fusions keep CL-28/30/32; evolutions keep CL-33's destroy-on-unseat.
