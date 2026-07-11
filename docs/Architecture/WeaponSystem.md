# Weapon System

## Overview

The Weapon System defines the design-time data for the equipment a character
wields in combat. It centres on two equippable equipment kinds — **weapons**
(`UWeaponData`) and **rings** (`URingData`) — which share a common base
(`UEquipmentDataBase`). Equipment carries the character's attack, abilities or
spells, stat bonuses ("Mastery" on weapons, "Engravings" on rings), an
elemental crystal slot, requirements, and triggered/passive skill effects.

All weapon-system types are immutable design-time data assets
(`UPrimaryDataAsset` descendants), consistent with the project's architecture
rule that immutable design-time data uses `UPrimaryDataAsset`. Per-instance
mutable runtime state (durability, the rolled stat bonus actually carried by an
owned item) lives elsewhere — the data assets here only provide templates and
defaults that runtime inventory factories copy from.

The skill data assets `UAbilityData` and `USpellData` are the executable
content the weapon/ring points at. They are documented here because they are
the payload of the weapon system, though they derive from
`USkillDataBase` (a separate skill hierarchy not fully covered by this
doc).

## Architecture

### `UEquipmentDataBase` (abstract base)

`UCLASS(Abstract, BlueprintType)`, derives from `UPrimaryDataAsset` and
implements the `IEquipmentGenerator` interface. Common base for both weapon and
ring assets. Key fields and members:

- **Identity** — `Name` (FString), `Tier` (`EItemTier`, default `E_Tier`),
  `Description` (multi-line), `Icon` (`UTexture2D*`).
- **Crystal / attachment** — `AttachedItem` (`FAttachedItem`): design-time
  attachment slot, discriminated by `Kind ∈ {None, Crystal, Evolution,
  AugmentStone}`. `Crystal` and `AugmentStone` both carry an `FCrystalId`
  (`CrystalType` + `CrystalTier`); `Evolution` carries a `UEvolutionItemData*`
  pointer; `None` is the empty default. Determines the equipment's element via
  `GetCrystalElement()` (a `AugmentStone` is non-elemental — see note below).
  The `AugmentStone` family (`DamageStone` / `AbilityStone`) is documented in
  `AugmentStoneSystem.md`.
  - `DefaultSpells` (`TArray<USpellData*>`): default spells copied to the
    inventory entry when obtained; lost if the crystal is removed. Hidden in the
    editor when a augment stone is attached (`EditCondition = "!IsAugmentStoneAttached"`).
  - `DefaultAbilities` (`TArray<UAbilityData*>`, "Extra Abilities"): abilities
    seeded onto a augment-stone-attached weapon's ability slots; shown only when a
    augment stone is attached (`EditCondition = "IsAugmentStoneAttached"`).
  - `bLockSkills` (bool): generic lock for the attachment's provided skills
    (spells for crystals, abilities for augment stones); no runtime consumer yet.
- **Infusion** — `bImmuneToInfusion` (bool): when true the equipment cannot be
  an infusion source and infusion-requesting actions are rejected (ORed with
  action-level immunity at the infusion gate). `InfusionStatusMultiplier`
  (float, clamped 0.0–2.0): status buildup multiplier when this equipment is
  the infusion source.
- **Bonuses** — two layers of `FEquipmentStatBonus`:
  - `BaseStatBonus` (`EditAnywhere`): designer-authored baseline, never touched
    by the generator.
  - `GeneratedStatBonus` (`VisibleAnywhere`, read-only in editor):
    generator-rolled, rerollable, zero-sum per roll.
  - `GetCombinedStatBonus()` returns the field-wise sum of the two layers — the
    final per-instance roll template that inventory factories copy.
- **Effects** — `Effects` (`TArray<FSkillEffect>`): equipment-level skill
  effects (passives + triggered), applied via
  `USkillEffectManager::ApplyEquipmentEffects` at combat start. Distinct from
  evolution-crystal effects on `UEvolutionItemData::Effects`. Helpers
  `GetAlwaysActiveEffects()` and `GetTriggeredEffects()` split the array by
  `FSkillEffect::IsAlwaysActive()`.
- **Requirements** — `Requirements` (`FWorldStatRequirements`), queried by
  `MeetsRequirements()` and `GetRequirementsSummary()`.
- **Generator (CallInEditor)** — `SubstatPoints` (int32) and `PillarPoints`
  (float) act as a pending pool. `RollSubstatPoints()`, `RollPillarPoints()`,
  and `ClearAllBonuses()` are `CallInEditor` buttons. `IEquipmentGenerator`
  hooks: `GetEditableStatBonus()` returns `GeneratedStatBonus`,
  `GetGeneratorTier()` returns `Tier`.
- **Crystal helpers** — `HasCrystal()`, `IsEvolved()`, `GetCrystalElement()`,
  `IsAugmentStoneAttached()` (`Kind == AugmentStone` — drives the contextual
  `EditCondition`s on `DefaultSpells` / `DefaultAbilities`), and
  `GetRestrictedCrystalTypes()` (supplies the `GetRestrictedEnumValues` grey-out
  set for `AttachedItem.CrystalType`; see *Attachment type filter* below).
  - **Note — `GetCrystalElement()` and augment stones.** The switch handles
    `Crystal` / `Evolution` / `None`; a `AugmentStone` attachment falls through to
    the `ESpellElement::None` default (the non-elemental sentinel — post the
    `Generic→None` migration; `Generic` now means "inherit at cast"). This is
    intended (stones grant mechanics, not an element), but is expressed as a
    `default:` fall-through rather than an explicit `AugmentStone` case.
- **`GetMaxSpells()`** — virtual cap on `DefaultSpells`, used by validation;
  the base returns `TNumericLimits<int32>::Max()` (no cap), subclasses
  override.

### `UWeaponData`

`UCLASS(BlueprintType)`, derives from `UEquipmentDataBase`. Adds weapon-specific
data:

- **Weapon** — `WeaponType` (`EWeaponType`, default `Sword`),
  `WieldMode` (`EWeaponWieldMode`, default `Single`; drives both mesh-spawn
  behaviour and ability dual-gating — see *Wield modes and mesh attachment*
  below), `WeaponAttack` (`USkillDataBase*` — post the attack/ability merge it holds a
  `UAbilityData` with `bIsAttack=true`; replaces base attack when equipped),
  `PresetAbilities` (`TArray<UAbilityData*>`), `bAbilitiesLocked`
  (bool — when true abilities cannot be customised; used for conjured weapons),
  `WeaponStance` (`UStanceData*`).
- **Animations** — `DrawMontage`, `SheatheMontage`, `ParryMontage`
  (`UAnimMontage*`).
- **Mesh** — right-hand mesh `WeaponStaticMesh`, `WeaponSkeletalMesh` and its
  `MeshRotation`; left-hand mesh `LeftHandStaticMesh`, `LeftHandSkeletalMesh`
  and its `LeftMeshRotation`. The left-hand fields carry `EditConditionHides`
  on `WieldMode != Single`, so they only appear in the details panel for
  `Dual` / `OffHandShield` weapons. There are **no socket-name fields on the
  weapon DA** — attachment sockets are derived from `WeaponType` in
  `UWeaponMeshComponent` (see *Wield modes and mesh attachment*).
- **Utility** — `GetWeaponTypeName()`, `GetAbilityCount()`,
  `IsConjuredWeapon()` (returns `bAbilitiesLocked`), `IsDualWielded()` (true
  when `WieldMode` is `Dual` or `OffHandShield`), `HasAttack()`,
  `HasStance()`. Overrides `GetMaxSpells()`.

### `URingData`

`UCLASS(BlueprintType)`, derives from `UEquipmentDataBase`. The Resonator
class's primary spell source. Adds:

- **Ring** — `bSpellsLocked` (bool — conjured-ring equivalent of
  `bAbilitiesLocked`), `PresetSpells` (`TArray<USpellData*>`; when
  `bSpellsLocked` is true these are the locked all-or-nothing spells, otherwise
  customisable defaults copied into the loadout on `InitializeFromRing`).
  `IsConjuredRing()` returns `bSpellsLocked`.
- **Mesh** — `RingStaticMesh`, `MeshRotation`.
- Overrides `GetMaxSpells()`.

Note: the `PresetSpells` property has `meta = (EditCondition = "bSpellsLocked")`
— it is only editable in the details panel when `bSpellsLocked` is true, even
though the header comment describes a non-locked customisable-defaults use. This
is a slight mismatch between the comment and the `EditCondition`.

### `UAbilityData`

`UCLASS(BlueprintType)`, derives from `USkillDataBase`. Universal skills usable
by all characters; can be infused with the character's innate element for status
effects.

- **Identity** — `RequiredWeaponType` (`EWeaponType`), `RequiredWieldMode`
  (`EWeaponWieldMode`, default `Single`). `Single` = no off-hand requirement,
  usable on any wield mode; `Dual` = usable only on a two-weapon loadout;
  `OffHandShield` = usable only on a sword-and-shield loadout. Resolved by
  `UAbilityData::AllowsWieldMode(WeaponWieldMode)` (Single allows any, else
  exact match) and enforced by `FWeaponLoadoutEntry::ValidateAbilities` — see
  the Loadout System doc.
- **Combat** — `PhysicalDamageType` (`EPhysicalDamageType`, default `None`).
  **The ability/attack is the SOLE physical-type source** — the field moved
  here from `UWeaponData` (Clusters A–C): the same swing delivers the same
  physical type regardless of which weapon performs it. Drives the physical
  bar-cap trigger and the authored-DoT physical path when no elemental infusion
  is active; `ActionExecutor` reads it off the executing `UAbilityData`
  directly (no weapon fallback — a resolved `None` logs a warning and
  suppresses the physical trigger). Populated on all ~204 pool attack/ability
  assets + the legacy weapon skills (Cluster B). ⚠️ The field's header comment
  in `AbilityData.h` still describes the Cluster-A interim ("None = inherit the
  weapon's") — stale since Cluster C.
- **Execution** — `ExecutionType` (`EAbilityExecutionType`, default `Melee`).
  Melee-only fields: `ApproachData` (`UMovementData*`), `ExecutionRange`
  (float, default 150.0). Both are gated by `EditCondition`/`EditConditionHides`
  on `ExecutionType == Melee`.
- **Visuals** — `ExecutionMontage`, `NormalVFX`, `InfusedVFX`,
  `ProjectileVFX` (Ranged-only).
- **Damage** — `CalculateDamage(Character, bIsInfused)` returns `BaseDamage`
  (attacker-side base); the `RawDamage` multiplier is applied once downstream by
  the DamageCalculator. The old `× (1 - requirement penalty)` was removed —
  requirement scaling is per-pillar via `ComputeActionStatModifiers` (see
  `Mechanics/RequirementGap.md`). `bIsInfused` no longer affects the damage value.
- **Energy** — `CalculateEnergyCost(Character, bIsInfused)`,
  `CalculateNormalEnergyCost()`, `CalculateInfusedEnergyCost()`.
- **Status** — `CalculateStatusBuildup(Character)`.
- **Execution helpers** — `IsRanged()`, `IsMelee()`, `IsSupportAbility()`
  (`BaseDamage == 0 && HasBuffEffects()`), `RequiresApproach()`.

### `USpellData`

`UCLASS(BlueprintType)`, derives from `USkillDataBase`. Element-locked
magical abilities; supports a mode toggle (Elemental vs Raw/Construct).

- **Identity** — `Element` (`ESpellElement`, default `Fire`), `School`
  (`ESpellSchool`, default `Destruction`).
- **Requirements** — `RequiredEvolutionCrystal` (`UEvolutionItemData*`; non-null
  only for evolution spells).
- **Stats** — `TurnCost` (int32, default 1; flagged for future multi-turn
  casting).
- **Visuals** — `CastAnimation`, `SpellVFX`, `ImpactVFX`, `MuzzleVFX`.
- **Delivery extensions** — The **Homing** and **Beam** delivery types were both
  REMOVED (`feature/realtime-defense`). Homing tracking is meaningless without a
  spatial dodge — a homing shot was just a projectile with a curvy path; a beam is
  now authored as a burst of projectiles (`Projectile`, `Count>1`), which the
  per-impact burst path covers (even-split per arrival). `HomingStrength`,
  `BeamDuration`/`BeamTickInterval` and the `ASkillProjectile` homing/beam tick
  machinery / `OnBeamTick` delegate are gone. `ESpellDeliveryType` is now
  `Projectile` / `AOE` / `Instant`.
- **Size** — `BaseSize`, `HitboxRatio` (0.5–1.2).
- **Defense helpers** — `CanBeBlocked()`, `CanBeParried()`,
  `CanBeDodgedByMoving()`, `CanBeDodgedByTiming()`, `GetAvailableDefenses()`.
- **Damage** — `CalculateDamage(Character, ActionMods)` where `ActionMods`
  (`FActionStatModifiers`) carries per-action stat modifiers (Reality,
  Evolution, future buffs) populated by `ActionExecutor` at spell execution;
  AI-preview callers omit it to use the no-boost default.
- **Energy / Status** — `CalculateEnergyCost(Character)`,
  `CalculateStatusBuildup(Character, ActionMods)`.
- **Helpers** — `CanCharacterCast()`, `GetDisplayName(Caster)`,
  `GetElementName()`.
- **Construct** — `bIsConstruct` (bool), `ConstructedWeapon` (`UWeaponData*`),
  `bSealsSpells` (bool, default true).

### Wield modes and mesh attachment

`EWeaponWieldMode` (the type of `UWeaponData::WieldMode`) governs both how many
weapon meshes spawn and how abilities gate. It has three values:

- **`Single`** — one mesh, right hand only; the off-hand is free. Used by both
  one-handed and two-handed weapons — two-handedness is an animation concern,
  not a wield-mode one.
- **`Dual`** — two meshes, one per hand. Each hand resolves its mesh
  independently; there is no cross-hand fallback.
- **`OffHandShield`** — two meshes, asymmetric: a weapon in the right hand and
  a shield (or other off-hand item) in the left.

`UWeaponData::IsDualWielded()` returns `true` for `Dual` and `OffHandShield`,
`false` for `Single`.

`UWeaponMeshComponent` spawns and attaches the meshes at runtime. It resolves
attachment sockets **from `WeaponType`**, not from any per-DA field — the
weapon DA carries no socket names:

- `GetRightSocketForWeapon()` switches on `WeaponType` and returns the
  right-hand socket (`hand_r_sword`, `hand_r_axe`, `hand_r_greatsword`;
  `Dagger` shares the `Sword` socket). A `WeaponType` with no `case` hits the
  `default:`, which logs an Error and returns `NAME_None`.
- `GetLeftSocketForWeapon()` short-circuits to `hand_l_sas` when
  `WieldMode == OffHandShield` — a shield always uses the same socket
  regardless of the right-hand `WeaponType`. Otherwise it switches on
  `WeaponType` exactly like the right-hand lookup, with the same
  Error-and-`NAME_None` default.

**To support a new weapon type's attachment, add a `case` to both switches**
(`GetRightSocketForWeapon` and `GetLeftSocketForWeapon`). If a socket resolves
to `NAME_None`, the spawn function logs a Warning and skips that hand — there
is no fallback socket.

Spawn behaviour:

- `Single` → `SpawnWeaponMesh` spawns the right-hand mesh only.
- `Dual` / `OffHandShield` → `SpawnDualWeaponMesh` spawns both hands
  independently. The right hand prefers `WeaponSkeletalMesh`, then
  `WeaponStaticMesh`; the left hand prefers `LeftHandSkeletalMesh`, then
  `LeftHandStaticMesh`. There is **no reuse of the right-hand mesh** on the
  left — if both `LeftHand*` fields are null, no left mesh spawns and a Warning
  is logged.
- `MeshRotation` applies to the right-hand mesh, `LeftMeshRotation` to the
  left. No mirror scale is applied — the left socket itself carries the
  handedness.

**Authoring rule for the left-hand mesh.** `LeftHandStaticMesh` /
`LeftHandSkeletalMesh` are the explicit off-hand mesh. For `OffHandShield` they
hold the shield and must be set. For `Dual` they are optional — set them when
the off-hand should use different geometry, leave them null otherwise (no left
mesh spawns). They are hidden in the details panel while `WieldMode` is
`Single`.

## How It Works

1. **Authoring.** A designer creates a `UWeaponData` or `URingData` asset, sets
   identity/tier, fills `AttachedItem` (`FAttachedItem`) with Kind = `Crystal`
   (FCrystalId), `AugmentStone` (FCrystalId — weapons only), or `Evolution`
   (UEvolutionItemData*), and authors a `BaseStatBonus` baseline plus
   `PresetAbilities`/`PresetSpells` and `DefaultSpells`. When the slot is a
   augment stone, the `CrystalType` dropdown is filtered to the stone sub-types
   and `DefaultAbilities` ("Extra Abilities") is used instead of `DefaultSpells`.
2. **Rolling stat bonuses.** In the editor the designer fills the
   `SubstatPoints` / `PillarPoints` pending pools to the tier budget, then
   clicks **Roll Substat Points** / **Roll Pillar Points**:
   - `RollSubstatPoints()` checks `SubstatPoints >= EquipmentBonusGen::GetSubstatBudget(Tier)`,
     calls `Modify()` (transaction stack), runs
     `GeneratedStatBonus.RerollSubstats(Tier, EqualWeights)` with an equal
     `FPillarWeights`, and resets `SubstatPoints` to 0.
   - `RollPillarPoints()` checks against `GetPillarBudget(Tier)`, calls
     `Modify()`, runs `GeneratedStatBonus.RerollPillars(Tier)`, resets
     `PillarPoints`.
   - Insufficient points logs a warning to `LogEquipmentBonusEditor` and
     aborts. `ClearAllBonuses()` resets `GeneratedStatBonus` and both pools but
     intentionally preserves `BaseStatBonus`.
3. **Combining bonuses.** `GetCombinedStatBonus()` produces the field-wise sum
   of `BaseStatBonus + GeneratedStatBonus` across all 16 `FEquipmentStatBonus`
   fields (raw/spell damage, efficiency, status multiplier, crit, speeds,
   defense, max HP/EP, resistance, luck, three pillar percents). This combined
   struct is the per-instance roll template inventory factories copy at
   `CreateFromWeapon` / `CreateFromRing` time.
4. **Editor validation (`IsDataValid`).** The base checks `Name` is non-empty,
   that `DefaultSpells.Num()` does not exceed `GetMaxSpells()`, and the
   **attachment Kind/type backstop**: `Kind==Crystal` requires a gem
   `CrystalType` (Garnet..Quartz) and `Kind==AugmentStone` requires a stone
   `CrystalType` (DamageStone / AbilityStone) — either mismatch is a hard error
   (`CrystalTypeHelpers::IsGemType` / `IsAugmentStoneType`). Subclasses add their
   own validation (e.g. `URingData` **hard-rejects any `AugmentStone` on a ring** —
   weapon stones are weapon-only; `UAbilityData`, `USpellData`, and `URingData` each
   override `IsDataValid`). The old `UWeaponData` `PhysicalDamageType::None`
   rejection is gone with the field — the type now lives on the ability
   (see `UAbilityData` above).

   **Attachment type filter (editor).** `AttachedItem.CrystalType` carries
   `meta=(GetRestrictedEnumValues="GetRestrictedCrystalTypes")`, which greys out
   (non-selectable, not hidden) the wrong-Kind values in the dropdown — stones
   when `Kind==Crystal`, gems when `Kind==AugmentStone`. The `IsDataValid`
   backstop above is the hard guarantee; the grey-out is the editor affordance.
   See `AugmentStoneSystem.md` for the engine mechanism and the single-field
   forward-risk.
5. **Live editor reactivity (`PostEditChangeChainProperty`).** On any property
   chain edit, the base re-derives the threshold-visibility flags
   (`bConditionUsesThreshold`, `bSecondaryConditionUsesThreshold`,
   `bTargetConditionUsesThreshold`) on every `FSkillEffect` in `Effects` from
   `SkillTriggerUtils::IsThresholdTrigger`, so threshold `EditCondition` gating
   reacts live.
6. **Combat use.** At combat start `USkillEffectManager::ApplyEquipmentEffects`
   applies the equipment's `Effects`. When equipped, `WeaponAttack` replaces
   the base attack; `PresetAbilities` / `PresetSpells` populate the action set;
   `WeaponStance` sets the idle stance. Skill assets compute damage / energy /
   status via their `CalculateXxx` methods, with the `DamageCalculator`
   applying the final multipliers.

## Integration Points

### Delegates broadcast

- None. Weapon-system data assets are passive design-time data and broadcast no
  delegates.

### Subsystems / systems depended on

- `EquipmentBonusGen` (free functions) — roll budgets (`GetSubstatBudget`,
  `GetPillarBudget`) for the roll buttons. ⚠️ Tier-power arc (2026-06-21):
  `GetSubstatBudget` now returns a **flat** `FIXED_SUBSTAT_BUDGET` (~20) at every
  tier (was F6→S45) — signature unchanged, but the roll-budget gate at §266 no
  longer scales with tier. Higher-tier strength comes from per-point VALUE scaling
  at `GetActiveStatBonus` (see `TierPowerScaling.md`). `GetPillarBudget` unchanged.
- `FEquipmentStatBonus` / `FPillarWeights` — the stat-bonus struct and reroll
  logic (`RerollSubstats`, `RerollPillars`).
- `SkillTriggerUtils` — `IsThresholdTrigger` for editor effect-flag refresh.
- `UEvolutionItemData` — read for the evolution-crystal asset's element
  (`GetAssociatedElement`) and `RequiredEvolutionCrystal` spell gate. Equipment
  reads its own `AttachedItem` for element via `GetCrystalElement()` (dispatches
  by `Kind`: `Crystal` → `FCrystalId` → element table; `Evolution` → asset's
  `GetAssociatedElement`; `AugmentStone` / `None` → `Generic`).
- `IEquipmentGenerator` — interface implemented by `UEquipmentDataBase`,
  letting a generic generator target the editable bonus layer.
- `UCharacterData` — passed into the skill `CalculateXxx` methods.
- The `DamageCalculator` subsystem applies the downstream `RawDamage`
  multiplier referenced by `UAbilityData::CalculateDamage`.

### Systems that depend on the Weapon System

- `USkillEffectManager` — consumes `Effects` via `ApplyEquipmentEffects` at
  combat start.
- Inventory factories — `CreateFromWeapon` / `CreateFromRing` copy
  `GetCombinedStatBonus()` and `DefaultSpells` into runtime inventory entries.
- `ActionExecutor` — populates `FActionStatModifiers` consumed by
  `USpellData::CalculateDamage` / `CalculateStatusBuildup`.
- Resonator loadout (`InitializeFromRing`) — copies `URingData::PresetSpells`.
- Combat action / animation systems — consume `WeaponAttack`,
  `PresetAbilities`, `WeaponStance`, draw/sheathe/parry montages, and
  spell/ability VFX and montages.

## Known Limitations / TODOs

- No literal `// TODO`, `// FIXME`, or `// HACK` markers exist in the six files
  documented.
- **Deprecated functions** — `UAbilityData::CalculateNormalDamage` and
  `CalculateInfusedDamage` are explicitly `@deprecated` (and carry
  `meta=(DeprecatedFunction, ...)`). They are kept only as Blueprint forwarders
  so existing BP graphs resolve; the header says to remove them once those
  graphs are repointed to `CalculateDamage(Character, bool)`.
- **Removed mechanic** — the element-infusion damage penalty was removed per
  the locked cost matrix, so `bIsInfused` no longer affects
  `UAbilityData::CalculateDamage`'s value. The parameter is retained.
- **Editor cosmetic limitation** — per the `UEquipmentDataBase` class comment,
  UE5.7 details-panel category ordering across base/derived class boundaries is
  not controllable via `meta = (DisplayAfter = ...)`; base categories always
  render before derived ones. Accepted as cosmetic; a real fix needs an
  editor-only module with `IDetailCustomization`.
- **Comment vs. EditCondition mismatch** — `URingData::PresetSpells` has the
  `EditCondition = "bSpellsLocked"` meta, which gates editing to the locked
  case only, while the property comment also describes a non-locked
  customisable-defaults role. Whether non-locked rings can author preset spells
  in-editor is unclear from the code.
- `USpellData::TurnCost` is annotated "Future: multi-turn casting" — multi-turn
  casting is not yet implemented.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-19 | Added `EWeaponWieldMode` (Single / Dual / OffHandShield) and the *Wield modes and mesh attachment* section; documented the left-hand mesh fields on `UWeaponData`, enum-driven socket resolution in `UWeaponMeshComponent`, and `UAbilityData::bRequiresDualWeapon`. Noted removal of `EWeaponType::DualBlades` and the `Fists` → `Gauntlets` rename. | feature/weapon-sockets-dual-flag |
| 2026-05-28 | Sweep-1 — `USpellData::BeamTickInterval` field + `ASpellProjectile` discrete-tick model documented (`BeamTickIntervalSec`/`BeamTickCount`/`BeamTickIndex`/`BeamBaseDmgPerTick`/`BeamRemainder`/`BeamTimeUntilNextTick`); `OnBeamTick` signature is `(Target, int32 TickDamage, bool bTargetInBeam)`. `UWeaponAttackData::BaseSize` field added — ActionExecutor reads the raw asset value with no fallback constant. Infused attack EP cost now read from `BaseEnergyCost` instead of hardcoded `5`. | feature/integration-gaps-sweep-1 |
| 2026-06-07 | Weapon-stone alignment — `AttachedItem.Kind` corrected to `{None, Crystal, Evolution, WeaponStone}` (3 spots); documented `DefaultAbilities`/`bLockSkills` fields, `IsWeaponStoneAttached()`/`GetRestrictedCrystalTypes()` helpers, the `IsDataValid` Kind/type backstop + `URingData` weapon-stone rejection, and the `GetRestrictedEnumValues` attachment-type filter. Flagged `GetCrystalElement()` `WeaponStone → Generic` fall-through. Weapon-stone family in new `AugmentStoneSystem.md`. | feature/weapon-stones |
| 2026-06-12 | Phase-2 skill-data unification on `UCastableSkillDataBase` (shared base for attack/ability/spell): `SkillMontage` (D2), `BaseAnimSpeed` (D7), `VFXArray` of `FSkillVFXEntry` (D5), and now `CastArray` of `FSkillCastEntry` (D6) — each entry one self-contained delivery (type, speed, `Size` = mechanical hitbox, `Trail` VFX + `TrailScale`, homing/beam params, `Count`/`BurstInterval`), index-ordered for `UCombatNotify` Family=Cast/Index=N. Spell `PostLoad` migrates the loose delivery fields into one entry (`Size = BaseSize × HitboxRatio` — the actual hitbox; `TrailScale = BaseSize`; `SpellVFX` → entry `Trail`). Loose per-type fields deprecated-but-runtime-authoritative pending the Stage-12 runner, which switches readers and removes them. Fully-default spells migrate no entry (delta-serialization limit) — empty `CastArray` = "use loose defaults". Debug: `USkillCastDebug::GetCastArrayString`/`PrintCastArray`. | feature/d6-cast-array |
| 2026-06-12 | **Stage 12 COMPLETE — the fused-montage runner is live.** The three execution paths unify through the notify spine: `BeginSkillExecution` (movement-independent start; facing = nearest living enemy at start + settle) binds `UCombatNotify` `OnCombatNotify(Family, Index)` for all three skill types — VFX→`VFXArray[Index]`, Cast→`CastArray[Index]` via `DispatchSpellCast` (one spawn site shared with the legacy `SpellRelease` bridge; burst chain for `Count>1`), Hit damage consumes `ResolvedDamageSplit` (legacy-exact floor rounding). All reader-switches landed: `SkillMontage` (D2), `BaseAnimSpeed` (D7), `VFXArray` by role for muzzle/impact (D5), `CastArray` for delivery + async decision (D6), `ResolvedDamageSplit` (D1). Cast-entry `TrailScale`→`VisualScale` (PropertyRedirect); defense window sizes from `CastArray[0].VisualScale` (parity — Size-keyed sizing banked as a balance decision). Montage chain: `RitualCastMontage` → `SkillMontage` → `ReturnMontage`, presence-driven, finalize spans all legs. Loose fields (montages/VFX/delivery/`BaseSize`/`HitboxRatio`) are `DeprecatedProperty` load-only; `CalculateAnimSpeed`/`GetCurrentAttackMontage` deleted. The spike is retired (~560 lines; production runner is the sole notify consumer; legacy name-notify path survives content-gated). **Banked**: ritual arm gesture (turn-timing decision), SFX array, `BaseSize`/`HitboxRatio` hard-delete post-resave-bake, Depth-2 `Execute*Async` unification (re-assess now the preludes are thin), Depth-3 movement/warp + per-hit-defense arc. | feature/d-fused-runner |
| 2026-06-16 | `UWeaponAttackData::BaseSize` deleted — its sole reader was the dodge attack-size gate, which was removed (dodge is now timing-only). Melee `AttackSize` is a neutral `1.0` (still feeds the `OnDefenseWindowOpened` delegate + AI scheduling). NOTE: spell `USpellData::BaseSize`/`HitboxRatio` (the *Size* field above) are UNAFFECTED — still live via the empty-`CastArray` fallback + PostLoad migration, deferred to the post-SC8 resave bake. | feature/realtime-defense |
| 2026-06-16 | **Beam delivery type REMOVED.** `ESpellDeliveryType::Beam` (the last enum value — append-safe, no CoreRedirect) and its entire footprint deleted: the `ASkillProjectile` beam tick machinery (`TickBeam`, `OnBeamTick` delegate, `BeamTickCount`/`BeamBaseDmgPerTick`/`BeamRemainder`/… state), the `FSkillCastEntry`/`USpellData` `BeamDuration`/`BeamTickInterval` fields + migration, the `OnBeamTick` handler/bind, and the Beam fall-through labels in the spawn switches. A beam is now authored as a **burst of projectiles** (`Projectile`, `Count>1`) — the per-impact burst path (Stage 6 cluster 6) covers it with even-split-per-arrival defense. Also removes the prior beam double-apply (lumped window + ticks) and unscaled-damage (`Generic`/no-crit) placeholder bugs. Confirmed zero Beam-authored assets before removal. | feature/realtime-defense |
| 2026-06-17 | **Attack/ability merge COMPLETE.** `UWeaponAttackData` deleted — basic attacks are now `UAbilityData` with `bIsAttack=true`. `UWeaponData::WeaponAttack` (and `FWeaponLoadoutEntry::OverrideAttack`) are `USkillDataBase*` (hold the merged type). `EActionType::Attack` collapsed into `Ability` (`IsAttack()` is the runtime discriminator); one `FAction.SkillData` pointer, one dispatch (`ExecuteSkillAsync`), one animation (`PlaySkillAnimation`). The 6 attack data-assets were class-redirected to `UAbilityData` (resaved with `bIsAttack=true`). A `!IsAttack()` slotting gate keeps basic attacks out of the ability bar. ⚠️ The `WeaponAttackData→AbilityData` CoreRedirect (+ the field PropertyRedirects) in `DefaultEngine.ini` is **PERMANENT** — covers un-resaved weapon refs + old savegames; do not remove. | feature/realtime-defense |
| 2026-06-16 | **Homing delivery type REMOVED.** `ESpellDeliveryType::Homing` (mid-enum value 1 — needs a CoreRedirect, unlike trailing Beam) and its footprint deleted: `ASkillProjectile::TickHoming` + the Tick `Homing` case + the `OnHitBoxOverlap` Homing branch (function/binding kept as a Projectile no-op), the `HomingStrength` field on `FSkillCastEntry`/`USpellData`/`ASkillProjectile` + migration, and every `\|\| Homing` clause across the defense helpers (`CanBeParried`/`CanBeDodgedByTiming`), the `EditCondition` metas, the cluster-4/6 conversion gate, the async-decision, and both dispatch switches. Tracking is meaningless without a spatial dodge (Crown-confirmed: a homing shot = a projectile with a curvy path — dead weight). `+EnumRedirects=(OldName="ESpellDeliveryType",ValueChanges=(("Homing","Projectile")))` maps any stray authored value; enum is now `Projectile=0 / AOE=1 / Instant=2` (name-based serialization keeps AOE/Instant safe). Shared projectile plumbing (`SpawnProjectileActor`/`TickProjectile`/`ResolveImpact`/`OnSkillImpact`/`OnSkillDodged`/the count-based per-impact defense path) untouched. Confirmed zero Homing-authored assets before removal. | feature/realtime-defense |
| 2026-06-21 | Tier-power arc — `GetSubstatBudget` now returns a **flat** `FIXED_SUBSTAT_BUDGET` (~20) at every tier (was F6→S45); the editor roll-budget gate (§263–266) no longer scales with tier. Higher-tier strength comes from per-point VALUE scaling at `GetActiveStatBonus`. `GetPillarBudget` unchanged. See `TierPowerScaling.md`. | feature/tier-power-scaling |
| 2026-07-03 | `UAbilityData::bRequiresDualWeapon` (bool) replaced by `RequiredWieldMode` (`EWeaponWieldMode`, default `Single` = no requirement / any mode) + inline `AllowsWieldMode(WeaponWieldMode)` helper. The off-hand gate now distinguishes `Dual` from `OffHandShield` (previously lumped together as `IsDualWielded`). Straight field swap — no existing asset had the old flag set, so no migration needed. | feature/hub-merchants |
| 2026-07-11 | **PhysicalDamageType migrated weapon → ability (Clusters A–C).** A: `UAbilityData::PhysicalDamageType` added (default `None`) with weapon fallback. B: populated on ~204 pool attack/ability assets + legacy weapon skills. C: `UWeaponData::PhysicalDamageType` **removed** (with its `IsDataValid` None-rejection); `ActionExecutor` reads the executing ability directly — no weapon fallback, `None` logs a warning and suppresses the physical bar-cap / authored-DoT trigger. Rationale: the same swing delivers the same physical type on any weapon. | feature/hub-merchants |
