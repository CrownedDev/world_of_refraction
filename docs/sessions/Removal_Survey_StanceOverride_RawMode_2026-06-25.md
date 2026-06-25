# Removal Survey — Weapon Stance Override + Skill `bIsRawMode`

**Date:** 2026-06-25
**Scope:** Map the full removal footprint of two features so they can be deleted cleanly. Survey only — **no edits proposed or made.**

All citations are `file:line` against the current working tree.

---

## TARGET 1 — Weapon STANCE OVERRIDE (remove per-loadout overrides; KEEP `UWeaponData::WeaponStance`)

### 1.1 Declaration sites

| Field | Location | UPROPERTY exposure |
|---|---|---|
| `FWeaponLoadoutEntry::StanceOverride` | `Source/world_of_refraction/Public/Loadout/Entries/FWeaponLoadoutEntry.h:68-69` | `EditAnywhere, BlueprintReadWrite, Category="Overrides"` |
| `FSavedLoadout::PrimaryWeaponStanceOverride` | `Source/world_of_refraction/Public/Loadout/FSavedLoadout.h:244-246` | `EditAnywhere, BlueprintReadOnly` + meta `EditCondition="PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides` |
| `FSavedLoadout::SecondaryWeaponStanceOverride` | `Source/world_of_refraction/Public/Loadout/FSavedLoadout.h:282-284` | `EditAnywhere, BlueprintReadOnly` + meta `EditCondition="RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides` |

Doc-comment-only references (no code, but name the fields — update when editing):
- `FWeaponLoadoutEntry.h:64-67` (block comment describing the override)
- `FSavedLoadout.h:240-243` and `:280-281` (block comments describing propagation)

### 1.2 Every read/write site

**`FWeaponLoadoutEntry::StanceOverride`**
- **READ** — `Source/world_of_refraction/Private/Loadout/LoadoutComponent.cpp:2553` (`if (ShownWeapon->StanceOverride)`) and `:2555` (`return ShownWeapon->StanceOverride;`) inside `ULoadoutComponent::GetCurrentStance`. *(This is the only read; confirms the prompt's lead.)*
- **WRITE (propagation in)** — `Source/world_of_refraction/Private/Loadout/FCombatLoadout.cpp:406` (`Result.PrimaryWeapon.StanceOverride = SavedLoadout.PrimaryWeaponStanceOverride;`)
- **WRITE (propagation in)** — `Source/world_of_refraction/Private/Loadout/FCombatLoadout.cpp:518` (`Result.SecondaryWeapon.StanceOverride = SavedLoadout.SecondaryWeaponStanceOverride;`)
- **WRITE (reset to nullptr)** — `Source/world_of_refraction/Public/Loadout/Entries/FWeaponLoadoutEntry.h:160` inside `FWeaponLoadoutEntry::Clear()`

**`FSavedLoadout::PrimaryWeaponStanceOverride`**
- **READ** — `FCombatLoadout.cpp:406` (RHS of the propagation above)
- **WRITE (editor clear → nullptr)** — `Source/world_of_refraction/Private/Inventory/InventoryData.cpp:245` (cleared when `PrimarySlotType != Weapon`)

**`FSavedLoadout::SecondaryWeaponStanceOverride`**
- **READ** — `FCombatLoadout.cpp:518` (RHS of the propagation above)
- **WRITE (editor clear → nullptr)** — `InventoryData.cpp:218` (cleared when `RequiredClass != Generic`)
- **WRITE (editor clear → nullptr)** — `InventoryData.cpp:263` (cleared when `SecondarySlotType != Weapon`)

### 1.3 Do `Primary/SecondaryWeaponStanceOverride` propagate INTO `FWeaponLoadoutEntry::StanceOverride`?

**Yes — they are NOT read directly by the consumer.** The full chain is:

```
FSavedLoadout::Primary/SecondaryWeaponStanceOverride   (serialized, authored)
        │  copied during inflation
        ▼   FCombatLoadout::CreateFromSavedLoadout  (FCombatLoadout.cpp:406 / :518)
FWeaponLoadoutEntry::StanceOverride                    (runtime, on FCombatLoadout)
        │  read at stance query time
        ▼   ULoadoutComponent::GetCurrentStance  (LoadoutComponent.cpp:2553/2555)
   returned UStanceData*
```

So `GetCurrentStance` reads only the **runtime** `Entry::StanceOverride`; the saved fields reach it solely via the two copy sites in `FCombatLoadout.cpp`. There is no other resolve/copy path.

### 1.4 EditCondition / meta on OTHER properties referencing these names

**None.** No property anywhere uses these three field names in an `EditCondition`/meta. (The meta strings on the override fields themselves reference `PrimarySlotType` / `RequiredClass` / `SecondarySlotType` only — see 1.1.) Removal does not strand any other property's edit-condition.

### 1.5 SURVIVOR: `UWeaponData::WeaponStance` is a SEPARATE field that stays

- **Declaration** — `Source/world_of_refraction/Public/Equipment/Weapons/WeaponData.h:75` (`EditAnywhere, BlueprintReadOnly`)
- **Helper** — `WeaponData.h:142` (`bool HasStance() const { return WeaponStance != nullptr; }`)
- **Read sites (all survive):**
  - `Source/world_of_refraction/Private/Equipment/Weapons/WeaponData.cpp:66` — `IsDataValid` warning when unset
  - `Source/world_of_refraction/Private/Loadout/LoadoutComponent.cpp:2557` and `:2559` — **the fallback inside `GetCurrentStance`** (`ShownWeapon->WeaponEntry.Weapon->WeaponStance`). **This is the path that survives once the override is removed** — it already runs whenever `StanceOverride` is null.
  - `Source/world_of_refraction/Private/Debug/Equipment/WeaponDataDebug.cpp:78`, `:80`, `:111`, `:112` — debug dumps
- **Default path after removal:** `GetCurrentStance` collapses from "override → weapon default → unarmed" to "weapon default → unarmed". `LoadoutComponent.cpp:2557-2559` already implement the weapon-default branch verbatim, so the default path is fully intact independent of the override.

---

## TARGET 2 — Skill `bIsRawMode` (full removal)

### 2.1 `USkillDataBase::bIsRawMode` declaration + EditConditions

- **Declaration** — `Source/world_of_refraction/Public/Skills/Definitions/SkillDataBase.h:176-177` (`EditAnywhere, BlueprintReadOnly`). Comment at `:175`.
- **EditCondition referencing it** — `SkillDataBase.h:180-181`: `StatusBuildup` carries `meta=(EditCondition="!bIsRawMode", EditConditionHides, ClampMin="0")`. **This is the ONLY `EditCondition` referencing `bIsRawMode`** (verified across all `.h`). Removing `bIsRawMode` requires dropping `!bIsRawMode` from this meta (keep `ClampMin="0"`); `StatusBuildup` itself stays.

### 2.2 `ActionUtils::ApplyRawModeRedirect` + call sites

- **Definition** — `Source/world_of_refraction/Public/Combat/Actions/ActionUtils.h:30-40` (inline, whole function is raw-mode-only). The `ActionUtils` namespace currently contains **only** this function (file is `:1-42`), so removing it orphans the entire header.
- **Call sites (both fold `StatusBuildup` into damage):**
  - `Source/world_of_refraction/Private/Combat/Actions/ActionExecutor.cpp:1251` — `ApplyRawModeRedirect(Spell->bIsRawMode, FinalDamage, SpellBaseBuildup)` (spell path; comment `:1249-1250`)
  - `ActionExecutor.cpp:1491` — `ApplyRawModeRedirect(Ability->bIsRawMode, FinalDamage, AbilityBaseBuildup)` (ability path; also serves merged basic-attacks, which are now `UAbilityData`)
- After removal, `SpellBaseBuildup`/`AbilityBaseBuildup` flow straight to `FinalizeDamageInputs` (`ActionExecutor.cpp:1253` / `:1494`) with no fold.

### 2.3 `USpellData` raw branches

- `Source/world_of_refraction/Private/Skills/Definitions/SpellData.cpp:101-104` — `CalculateDamage`: `if (bIsRawMode) FinalDamage *= CombatConstants::RAW_MODE_DAMAGE_MULTIPLIER;` (comment `:89`)
- `SpellData.cpp:114-116` — `CalculateStatusBuildup`: `if (bIsRawMode) return 0;`
- `SpellData.cpp:269` — `IsDataValid`: guard `if (!bIsRawMode && StatusBuildup < 0)` (the `!bIsRawMode` term must be dropped; keep the negative-buildup error)

### 2.4 `UAbilityData` / `UWeaponAttackData` paths reading `bIsRawMode`

- **`UAbilityData`** — `AbilityData.cpp` has **no** `bIsRawMode` reference (verified). Abilities receive raw-mode treatment **only** through the `ActionExecutor.cpp:1491` redirect. **Asymmetry to note:** abilities never had the `+10% RAW_MODE_DAMAGE_MULTIPLIER` (that lives only in `SpellData::CalculateDamage`); for abilities, raw-mode is purely the StatusBuildup→damage fold. Removing it for abilities therefore changes only that fold, not a multiplier.
- **`UWeaponAttackData`** — **no live class exists.** All remaining mentions are comments referring to the merged-away type (`SkillDataBase.h:151,318,394,403`, `ActionExecutor.h:829`, `ActionExecutor.cpp:2187`, `ActiveSkillEffect.h:450`). The merge folded attacks into `UAbilityData`, so there is no separate attack-data read of `bIsRawMode`.

### 2.5 `CombatConstants::RAW_MODE_DAMAGE_MULTIPLIER`

- **Declaration** — `Source/world_of_refraction/Public/Combat/CombatConstants.h:353` (`constexpr float RAW_MODE_DAMAGE_MULTIPLIER = 1.10f;`, comment `:352`)
- **Only usage** — `SpellData.cpp:103`
- **Becomes orphaned** once the 2.3 raw branch is removed → safe to delete the constant.

### 2.6 `FDamageCalculationInput::bIsRawMode` (prompt's "FDamageInput") — SCOPE DECISION

- **Declaration** — `Source/world_of_refraction/Public/Combat/Damage/DamageCalculator.h:82-83` (`EditAnywhere`→`BlueprintReadWrite`, default `false`).
- **Sites that POPULATE it from the skill flag** — **NONE.** No assignment to `bIsRawMode` exists anywhere except the two `= false` declaration defaults (verified by grep for `bIsRawMode =` across all `.cpp`). All six `FDamageCalculationInput` construction sites leave it at default:
  - `AIDecisionManager.cpp:666`, `:745`
  - `DamageCalculator.cpp:311`
  - `ActionExecutor.cpp:2844`, `:3031`
  - `ScalingDebug.cpp:27`
- **Sites that CONSUME it** — **NONE.** `DamageCalculator.cpp` contains no `bIsRawMode` reference (verified). Architecture doc corroborates: "`bIsRawMode` … present on the struct but **not consumed** inside `CalculateDamage`" (`docs/Architecture/DamageCalculator.md:28,112`).
- **Verdict:** the field is **fully inert today** (never written, never read). 
  - **Option A (recommended): remove it.** Zero behavioral impact; it is already dead. Touches `DamageCalculator.h` only.
  - **Option B: leave it inert (always-false).** Harmless, but leaves a dead `BlueprintReadWrite` field BP could still wire to. Given it is already dead, Option A is the clean choice.

### 2.7 `SpellDataDebug` references

- `Source/world_of_refraction/Private/Debug/Skills/SpellDataDebug.cpp:92-99` — `if (Spell->bIsRawMode)` MODE block ("Raw (+10% damage, no status)" vs "Elemental")
- `SpellDataDebug.cpp:158` — `Spell->bIsRawMode ? TEXT("Raw") : TEXT("Elemental")` in the comparison header

### 2.8 EXPLICITLY EXCLUDED — `ESubStat::RawDamage` (different concept, untouched)

`ESubStat::RawDamage` is the **physical, Body-driven damage substat** — a completely different concept from the `bIsRawMode` authoring flag. It is **not** part of this removal. Proof it is a distinct lineage (none reference `bIsRawMode`):
- `FActionStatModifiers::RawDamage` consumed by `DamageCalculator` (`DamageCalculator.h:77` comment).
- `UWeaponData::BonusRawDamage` (renamed from `BonusAttack` — `Config/DefaultEngine.ini:56`).
- `CharacterDataComponent::GetEvolutionModifiedRawDamage` (`Config/DefaultEngine.ini:193`).
- `EItemEffectType::BuffRawDamage` (`Config/DefaultEngine.ini:179`).
- The design doc explicitly flags the collision: *"this `bIsRawMode` is UNRELATED to `ESubStat::RawDamage`"* (`docs/Design/RawModeExtension.md:16`).

**Leave every `RawDamage` symbol alone.**

---

## CROSS-CUTTING (both targets)

### 3.1 Serialized types → assets that drop data on resave

| Removed field | Serialized in | Asset type on disk | Resave consequence |
|---|---|---|---|
| `FSavedLoadout::Primary/SecondaryWeaponStanceOverride` | `FSavedLoadout` (USTRUCT, `FSavedLoadout.h:135`) held in `UInventoryData::SavedLoadouts` (`InventoryData.h:114`) | **`UInventoryData`** (`UPrimaryDataAsset`, `InventoryData.h:46`) | Every `UInventoryData` asset drops both override values on next resave. |
| `USkillDataBase::bIsRawMode` | `USkillDataBase` base class | **`USpellData`** and **`UAbilityData`** (both `: public USkillDataBase`, both `UPrimaryDataAsset`-derived skill assets; `SpellData.h:36`, `AbilityData.h:31`) | Every spell and ability asset drops its `bIsRawMode` value on next resave. (Authored "raw" spells/abilities silently revert to elemental/normal.) |
| `FDamageCalculationInput::bIsRawMode` | runtime-only struct | none | No on-disk asset stores it (always default-constructed at runtime). |

**Not serialized to a content asset:** `FWeaponLoadoutEntry::StanceOverride` lives on `FCombatLoadout`, which is built at runtime by `CreateFromSavedLoadout` (`InventoryComponent.h:107` holds `TArray<FCombatLoadout>` as runtime state, not authored). No asset stores it — only the `FSavedLoadout` source above does.

**Config redirects that become orphaned** (point to a `NewName` that will no longer exist — harmless to leave, cleaner to remove):
- `Config/DefaultEngine.ini:112` — `AbilityData.bIsRawMode → SkillDataBase.bIsRawMode`
- `Config/DefaultEngine.ini:127` — `SpellData.bIsRawMode → SkillDataBase.bIsRawMode`
- `Config/DefaultEngine.ini:142` — `WeaponAttackData.bIsRawMode → SkillDataBase.bIsRawMode`

### 3.2 Blueprint (.uasset / LFS) exposure — NOT greppable

`.uasset` graphs are LFS pointers and cannot be searched. Flag the following BP-visible surfaces — graphs could read/write these and would break or silently no-op after removal:

| Field | BP exposure | Risk |
|---|---|---|
| `FWeaponLoadoutEntry::StanceOverride` | **BlueprintReadWrite** | BP can both read **and write** it — highest BP risk. |
| `FSavedLoadout::PrimaryWeaponStanceOverride` | BlueprintReadOnly | BP can read. |
| `FSavedLoadout::SecondaryWeaponStanceOverride` | BlueprintReadOnly | BP can read. |
| `USkillDataBase::bIsRawMode` | BlueprintReadOnly | BP can read (e.g. UI/tooltip graphs). |
| `FDamageCalculationInput::bIsRawMode` | **BlueprintReadWrite** | BP could read/write; struct is `BlueprintType`. |

Recommend a manual BP reference check (right-click → Find References in editor, or a node search) on each before deleting.

---

## REMOVAL CLUSTER PLAN

Ordered so that **at every compile boundary, nothing references an already-deleted symbol** (consumers cleared before declarations). Each cluster ≤3 files; compile (in Rider/UE) after each.

### Cluster A — Stance: clear runtime consumers + remove `FWeaponLoadoutEntry::StanceOverride` (3 files)
1. `LoadoutComponent.cpp:2549-2561` — drop the `StanceOverride` read; keep the `WeaponStance` fallback (`:2557-2559`) and `GetUnarmedStance()`.
2. `FCombatLoadout.cpp:406` and `:518` — delete the two propagation writes.
3. `FWeaponLoadoutEntry.h:160` (Clear) + `:64-69` (declaration + comment) — delete.

*After A:* `Entry::StanceOverride` has no references. `Saved*` fields now referenced only in `InventoryData.cpp` + their declarations. **Compile.**

### Cluster B — Stance: remove `FSavedLoadout::Primary/SecondaryWeaponStanceOverride` (2 files)
1. `InventoryData.cpp:218`, `:245`, `:263` — delete the three editor `nullptr` clears.
2. `FSavedLoadout.h:240-246` and `:280-284` — delete both declarations + comments.

*After B:* Target 1 fully removed; `UWeaponData::WeaponStance` untouched. **Compile.**

### Cluster C — RawMode: clear all skill-flag consumers (3 files)
1. `ActionExecutor.cpp:1251` and `:1491` — delete both `ApplyRawModeRedirect` calls (+ comment `:1249-1250`).
2. `SpellData.cpp:101-104` (damage mult), `:114-116` (status return-0), `:269` (drop `!bIsRawMode` term, keep negative-buildup check).
3. `SpellDataDebug.cpp:92-99` (MODE block) and `:158` (comparison header) — remove/simplify.

*After C:* `USkillDataBase::bIsRawMode` has no readers; `ApplyRawModeRedirect` has no callers; `RAW_MODE_DAMAGE_MULTIPLIER` has no usages. **Compile.**

### Cluster D — RawMode: remove declaration + orphaned helpers (3 files)
1. `SkillDataBase.h:175-177` — delete `bIsRawMode`; `SkillDataBase.h:180-181` — drop `EditCondition="!bIsRawMode"` from `StatusBuildup` meta (keep `ClampMin="0"`).
2. `ActionUtils.h:30-40` — delete `ApplyRawModeRedirect` (and the now-empty `ActionUtils` namespace / header if nothing else uses it — verified it holds only this function).
3. `CombatConstants.h:352-353` — delete `RAW_MODE_DAMAGE_MULTIPLIER`.

*After D:* the skill-side flag and its machinery are gone. **Compile.**

### Cluster E — RawMode: inert `FDamageCalculationInput` field + config cleanup (2 files, independent)
1. `DamageCalculator.h:81-83` — delete inert `bIsRawMode` field (Option A from §2.6).
2. `Config/DefaultEngine.ini:112,127,142` — remove the three orphaned `PropertyRedirects`.

*Cluster E has no dependency on C/D* (the field is already inert) — it may run any time. **Compile.**

**Excluded from all clusters:** every `ESubStat::RawDamage` / `RawDamage` symbol (§2.8), and `UWeaponData::WeaponStance` (§1.5).

---

## Notes for the implementer
- Resaving any `UInventoryData`, `USpellData`, or `UAbilityData` asset after removal permanently drops the corresponding authored values (§3.1) — decide whether any authored "raw" skills need their `+10%`/no-status behavior re-expressed another way before deleting.
- Run an in-editor BP reference search on the five surfaces in §3.2 before deleting; `.uasset` graphs are invisible to grep.
- Docs to refresh post-removal (out of scope here, flagged): `docs/Design/RawModeExtension.md`, `docs/Architecture/DamageCalculator.md`, `docs/Architecture/WeaponSystem.md`, `docs/Architecture/LoadoutSystem.md`.
