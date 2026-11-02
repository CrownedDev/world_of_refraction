# Simplification Audit (2026-10-28)

**Status:** proposal; findings turned into backlog jobs B-267 onward (Phase 1d). Defaults: S1 follows D-054's rule; S2 legacy "Speed" means turn speed; S6 Ice nests under Water in the tag tree. S3's essence name is Crown's.

---


**Status:** everything here is a proposal. This was a read-only audit of `dev`; nothing in the repo was edited.
It sits alongside `docs/analysis/DryAudit_2026-10-28.md`, which covered copy-paste. This audit covers concepts that are
**represented more than once**. Where a DRY item (A1–A18) touches the same code, it is referenced by number, not repeated.
Paths are relative to `Source/world_of_refraction/` (`P/` = `Private/`, `H/` = `Public/`).

**Crown's question:** "Is there logic that could be simplified, e.g. damage type and elements?"
**Answer:** yes. "An element" is written down in about **15 hand-written switches or tables in 12 files**, and "a hit's damage
type" is written down in **two forms at once** (a pair of enums, and one combined enum). Adding Ice (B-064a), or any later
element or status, means editing every one of them. One element/damage-type table, plus one "hit" struct, would turn
that into one row. Along the way the audit found **three behaviour bugs** (S1–S3 below). They are small and worth doing first.

---

## 1. The map: what encodes "an element" or "a damage type" today

| Concept | Representations | Where |
|---|---|---|
| Element identity | `ESpellElement` (Generic=0 "inherit", Fire..Reality 1-9, None=10 "non-elemental") | `H/Skills/Definitions/ESpellElement.h:15` |
| Hit damage type | `EWoRDamageType` (None, 9 elements with matching numbers, Slash/Pierce/Impact 10-12, Ice 13) | `H/Combat/Resistance/EWoRDamageType.h:20` |
| Physical subtype | `EPhysicalDamageType` (Slash=0, Pierce, Impact, None=3) | `H/Combat/Damage/EPhysicalDamageType.h:9` |
| Gem crystal | `ECrystalType` Garnet..Iolite map 1:1 to elements; Quartz = None | `H/Equipment/Crystals/CrystalType.h:16` |
| Essence | `EEssenceType`: the elements in a **different order** (Lightning is 2), Generic used for Quartz/None | `H/Currency/CurrencyTypes.h` (`EEssenceType`) |
| Immunity | 9 members `GrantFireImmunity..GrantRealityImmunity`; no Ice or physical immunity exists | `H/Skills/Effects/ESkillEffectType.h:177-185` |
| Element-keyed on-hit statuses | `ApplyBurnToTarget` (Fire DoT), `ApplyChillToTarget` (Water DoT) | `ESkillEffectType.h:188-189`, `P/Skills/Effects/SkillEffectManager.cpp:2053-2072` |
| Tags | `Element.*` (11), `DamageType.*` (14, with `DamageType.Ice` **not** under `Element.Water`), `Immunity.Element.*` (9) | `P/Tags/WoRGameplayTags.cpp:7-56` |
| Resistance columns | 4 parallel 12-field shapes, plus 14 attributes (see S7) | `ClassInnateResistanceTable.h:47`, `FResistanceBonus.h:24`, `ResistanceProfileData.h:19`, `WoRAttributeSets.h` |

**The conversions, one hand-written switch or table each:**
- **Damage type from element or physical:**
  - `WoRDamageType::ElementToDamageType` / `PhysicalToDamageType` / `ToDamageType` (`EWoRDamageType.h:56,79,92`)
  - the reverse, `ToAxes` (`P/Combat/Resistance/ResistanceQuery.cpp:30`)
- **Crystal to element:** `CrystalTypeHelpers::GetElement` (`H/Equipment/Crystals/CrystalTypeHelpers.h:20`), wrapped by `ItemIdentity::GetElement` (`ItemIdentity.h:28`). It is mirrored on the flat authored fields by `UEquipmentDataBase::GetCrystalElement` (`P/Equipment/EquipmentDataBase.cpp:24`) and by `FRuntimeAttachedItem::GetElement` (`P/Equipment/FRuntimeAttachedItem.cpp:61`).
- **Element to essence:** `EconomyYield::ElementToEssenceType` (`H/Currency/EconomyYield.h:356`).
- **Element to immunity:** ×2 plus the tag table (DRY A9).
- **Element to tag and damage type to tag:** `WoRGameplayTags.cpp:89-111`.
- **Element to bar-cap trigger:** `BarCapTriggerResolver.h:30`. Its flavour names live in `SkillEffectDisplayNames.h` (DRY A8).
- **Element to colour:** `ElementColors::GetColorForElement` (`H/Infusion/ElementColors.h:24`). It has no Physical colour, though B-105 asks for Physical white.
- **Element to name:** `WoRGameNames::GetElementName` (`P/WoRGameNames.cpp:14`; Reality is shown as "Unknown", D-095).
- **Element to resistance column:** `GetElementColumn` / `GetPhysicalColumn` (`ClassInnateResistanceTable.h:132,163`).
- **Broken Darkness element rules:**
  - "Forbidden" (Light/Void) is written **twice**: `P/Combat/Mechanics/BrokenDarknessManager.cpp:493` and `P/Infusion/HybridSpellColors.cpp:97`.
  - "Absorbable" is written once: `BrokenDarknessManager.cpp:498`.
- **Hand-written element lists:**
  - the gamble roll's 9 + 3 arrays (`P/Inventory/ItemExecutor.cpp:996-1013`)
  - `ResistanceProfileData.cpp:15`
  - the loadout's BD loop `i < None` (`P/Loadout/LoadoutComponent.cpp:2465`)

**Where the representations disagree:**
- **Ice can't be dealt.** `EWoRDamageType::Ice` exists, but no hit can produce it: `ToDamageType` takes (element, physical) and neither enum has Ice. `ToAxes` maps Ice to (None, None), so class and gear Ice resistance is always 0. `EffectMatches` (`ResistanceQuery.cpp:57`) can never match Ice. The "Water also adds Ice" lines therefore always add 0 (`ResistanceQuery.cpp:143-146`, `:154-157`; `P/GAS/WoRBuildupExecution.cpp:41-48`). The rules page already says no source grants Ice yet (ED-27), so this is not a bug today. It is the wall B-064a will hit.
- **Generic means two things.**
  - On a spell it means "inherit" (`ESpellElement.h:28`).
  - On a character (`UCharacterData::GetElement` returns Generic for non-Casters, `H/Character/CharacterData.h:281`), a trial (`H/Trial/TrialData.h:43`, "Generic = untyped trial"), a loadout pool default (`FCombatLoadout.h:51`) and BD alignment (`BrokenDarknessManager.h:508`), it means "none / unset".
  - The colour table paints Generic and None the same (`ElementColors.h:44`). Resistance treats a leftover Generic as None (Q-216).
  - The checks for "Generic or None" are written inline (`P/Skills/Definitions/SpellData.cpp:168`, `P/UI/Combat/CharacterPanelWidget.cpp:624`).
  - This is the deckbuilder's Affinity vs Element split (`Affinity` = a card's key including Generic; `Element` = None + nine only, for what something *is*), done with one enum here.
- **Reality shown as "Reality".** It reaches players through essence names (S3).
- **Broken Darkness** is no longer an element (correct, `ESpellElement.h:30-31`). BD's status is chosen by a `bSourceIsBrokenDarkness` special case inside the resolver (`BarCapTriggerResolver.h:52`), which is fine, but its element rules are split across two files (above).

---

## 2. Findings, ranked (wrong behaviour first, then how much future work they save)

Size: S = under half a day, M = about a day, L = several days. "Files" counts the files touched; clusters are 3 files or fewer.

### S1. "Energy Cost Up" is a cost **cut** that is classed as a **debuff** (wrong behaviour) — S
- **Current:**
  - `ModifyEnergyCost` is named "Energy Cost Up" (`ESkillEffectType.h:151`, `P/WoRGameNames.cpp:172-173`).
  - The cost maths makes a positive value **cheaper**: `1 - (CostBuff - CostDebuff + ModifyEnergy)/100` (`P/Combat/Actions/ActionExecutor.cpp:4295-4300`).
  - The card text agrees that positive is cheaper (`P/Skills/Effects/EffectText.cpp:20-25`, "cost N less").
  - The payload clamps both Magnitude and Value to ≥ 0 (`H/Skills/Effects/FSkillEffectPayload.h`), so **every authored one is a cost cut**.
  - Yet `IsDebuff` lists it unconditionally (`ESkillEffectType.h:396`). A cleanse strips it from an ally, and Iolite can't strip it from an enemy. `EffectText.cpp:544-547` already works around this for the text lighting only.
- **Related:** `ModifyTurnSpeed` is the same bug. Crown answered it in **D-054** (sign-aware, like `ModifyStatusResist`), but it is unbuilt: it is still at `ESkillEffectType.h:397`, and no backlog row mentions D-054.
- **Simpler shape:** one `IsSignAware(Type)` list in `SkillEffectClassification` (ModifyStatusResist, ModifyTurnSpeed, ModifyEnergyCost). `IsBuff`/`IsDebuff` read the sign for those, with "cheaper" counting as positive. Fix the "Energy Cost Up" name to match ("Energy Cost Down", or "Energy Cost" with the sign).
- **Behaviour change:** yes, a fix. **Files:** 2 (`ESkillEffectType.h`, `WoRGameNames.cpp`), plus a spec.
- **Decision:** ModifyTurnSpeed is already ruled (D-054). For ModifyEnergyCost, the reference-backed default is the same rule as D-054: a cost cut on an ally is a buff, a cost rise on an enemy is a debuff. That matches FF/Persona, where "Mana cost down" counts as a buff.

### S2. Authorable effect types that silently do nothing (wrong behaviour risk) — S
- **Current:**
  - `SpeedBuff`/`SpeedDebuff` ("Speed Buff (Generic)", `ESkillEffectType.h:98,29`) have **no stat reader**. Turn speed reads `TurnSpeed*`/`ModifyTurnSpeed` through `StatModifierTable` (`H/Skills/Effects/StatModifierTable.h:44`, `P/Combat/TurnManager.cpp:273-275`), and action speed reads `ActionSpeed*`.
  - `CritDebuff` authored directly is stored as itself. `DamageCalculator` reads only `CritChanceBuff/Debuff` + `ModifyCritChance` (`P/Combat/Damage/DamageCalculator.cpp:305-307`).
  - The bar-cap builder has cases for `SpeedDebuff`, `CritDebuff`, `EnergyDebuff` and `RandomDebuff` (`SkillEffectManager.cpp:2940-2970`). They are unreachable, because the resolver never returns those types and `ApplyTriggeredSkillEffect` has one caller (`P/Skills/Effects/StatusBuildupManager.cpp:668`). `RandomDebuff` would also pick the no-op `SpeedDebuff` one time in four.
  - None of these are `UMETA(Hidden)` (only `ModifyHealing` is, `:148`).
- **Simpler shape:** one small "legacy alias" table (`CritDebuff→CritChanceDebuff`, `EnergyDebuff→EnergyDrain`, `SpeedBuff/Debuff→TurnSpeedBuff/Debuff`), applied once when an effect is built. Hide those types in the editor, and have `IsDataValid` warn on assets that still use them. Mark the dead bar-cap cases OBSOLETE (remove them after PIE).
- **Behaviour change:** authored legacy rows start working instead of doing nothing. **Files:** 3. Crown should check in the editor which assets use them (this audit can't read `.uasset`s).
- **Decision:** what "Speed (Generic)" means. Default: turn speed, since that is the speed shown in turn order. Octopath and FFX "Haste" both act on turn order.

### S3. Reality leaks to players through essence names; the labels are built `FString`s — S
- **Current:** essence names come straight from `StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue` in player UI: the shop (`P/UI/Shop/ShopWindowWidget.cpp:41-43,471`, `P/UI/Shop/ShopRowWidget.cpp:163`) and crystal-break text (`P/Equipment/Durability/CrystalBreak.cpp:49-51`). So players see "Reality" (against D-095) and "Generic", and the strings are built `FString`s (against D-061).
- **Simpler shape:** `WoRGameNames::GetEssenceName(EEssenceType)` as `FText`, using `GetElementName` for the ten element keys (so Reality shows "Unknown"), plus the pillar and Ability names.
- **Behaviour change:** text only. **Files:** 4, so two clusters (names + shop; crystal break).
- **Decision:** none beyond D-095. Whether "Generic" essence should read "Elemental", "Quartz" or something else is a game word, so Crown names it. Default: keep "Generic" until Crown renames it.

### S4. Infusion source resolution: 5 switches over `EInfusionSourceOption`, already drifting — M
- **Current:** each of these maps a source to the gear behind it on its own:
  - `GetElementForSourceOption` (`ActionExecutor.cpp:4111`)
  - `ResolveInfusionAttachment` (`:4306`)
  - `ResolveInfusionMode` (`P/Combat/Actions/ActionExecutorCosts.cpp:255`)
  - the availability check `IsUsableRing` (`ActionExecutor.cpp:3995-4002`)
  - the wear blocks (DRY A3)
- **Drift:**
  - **Broken rings and crystals:** `FRingInventoryEntry::GetElement` returns None for a broken crystal (CL-24, `P/Loadout/Entries/FRingInventoryEntry.cpp:17`). `ULoadoutComponent::GetActiveRingElement` (`LoadoutComponent.cpp:3799`) and the PrimaryRing/WeaponCrystal cases (`ActionExecutor.cpp:4137-4157`) read `AttachedItem.GetElement()` with no broken check. The broken rule is re-stated in three other places instead (`IsUsableRing`, `IsSpellFromBrokenRing` at `:4229`, the entry getter).
  - **Evolution:** the element comes via `GetActiveLoadout().PrimaryEvolution.Item` behind `IsEvolved()` (`:4161-4170`), the mode via `GetPrimaryEvolution()` (`ActionExecutorCosts.cpp:278`), and the attachment via a whole-struct copy (`:4350`).
- **Simpler shape:** one `FInfusionSourceView ResolveInfusionSource(Actor, Option)` returning {attachment, element (through the entry getters, so the broken rule applies), mode, wear tier, usable, pays-HP}. The five readers become field reads. This pairs with DRY A3 and A1 (`ForEachContributingGear`).
- **Behaviour change:** none intended. A broken source's element becomes None everywhere, which matches CL-24. **Files:** 3 (`ActionExecutor.h/.cpp`, `ActionExecutorCosts.cpp`), one cluster after A2.
- **Also dead:** `EInfusionSourceHelpers::WeaponStatsApply` (`H/Infusion/EInfusionSourceOption.h:65`) and `UActionExecutor::DoWeaponStatsApply` (`ActionExecutor.cpp:4259`) have no callers, and the comment ("Raw bypasses weapon stats") contradicts the code (Raw returns true). Flag them OBSOLETE.

### S5. One element / damage-type table (the main answer to Crown's question) — L, in clusters
- **Current:** the ~15 conversions in §1, each a switch someone must remember to extend.
- **Simpler shape:** `H/Combat/Elements/ElementTable.h`, a plain `constexpr` C++ table (no `.uasset`; it works in headless specs). It has **one row per `EWoRDamageType`** (that enum is the superset, 14 rows), with these columns:
  - `Element` (the `ESpellElement` it counts as: Ice → Water, Slash → None)
  - `Physical` (`EPhysicalDamageType`)
  - `AlsoResistedBy` (Water → Ice, Q-145b)
  - `Gem` (`ECrystalType`), `Essence` (`EEssenceType`)
  - `BreakTrigger` (`ESkillEffectType`)
  - `Immunity` (`ESkillEffectType`)
  - `ElementTag` / `DamageTypeTag` / `ImmunityTag`
  - `Name` (`LOCTEXT`, Reality "Unknown")
  - `Colour` (Physical white for B-105)
  - `bBDAbsorbable`, `bBDForbidden`

  Plus `ElementTable::For(EWoRDamageType)` / `ForElement(ESpellElement)` lookups.
- **What it replaces:**
  - `ElementToDamageType`, `PhysicalToDamageType`, `ToAxes`
  - `CrystalTypeHelpers::GetElement` (it stays as a one-line forward)
  - `ElementToEssenceType`
  - both `GetElementImmunityType`s (DRY A9)
  - the three tag tables' element and immunity rows
  - `GetColorForElement`, `GetElementName`
  - the element part of `BarCapTriggerResolver`
  - `IsForbiddenElement` ×2, `CanAbsorbElement`
  - `GetElementColumn` (through S7)
  - the gamble arrays
  - the 9 immunity cases in `EffectText.cpp:705-713`
- **Enums untouched:** append-only, ordinals unchanged, saves unaffected. Each old function becomes a one-line forward first (build new beside old), with a `wor.CompareElements` spec that walks every row against the old functions before any forward is switched over. This follows the `wor.Compare*` pattern from B-020/B-021.
- **Order:**
  - (1) table + spec + debug print (`ElementTableDebug`), 3 files;
  - (2) damage-type, essence and crystal forwards;
  - (3) tags and immunity forwards;
  - (4) colours, names and BD flags.
- **Behaviour change:** none. **Decision:** none needed, since this is an engineering shape. The deckbuilder's naming (Affinity / Element / DamageType) was used as reference only, and the enum is not split (that would mean re-saving every asset).

### S6. One "hit" struct so Ice is representable — M (do it as B-064a's first step)
- **Current:**
  - **13 signatures** pass `(ESpellElement, EPhysicalDamageType)` as a pair. Examples: `StatusBuildupManager.h:127,183,195,232`, `BarCapTriggerResolver.h:30`, `ResistanceQuery.h:58,69`, `ClassInnateResistanceTable.h:188`, `GAS/WoRBuildupExecution.h:54`.
  - The same pair sits in **7 structs**: `FAction`, `FActionHitInput`, `FActiveSkillEffect` (`ActiveSkillEffect.h:167,173`), `FStatusBarState`, the buildup request, and others.
  - The damage type is then re-derived from the pair (`ToDamageType`), and the derivation can't yield Ice.
- **The pair is still right for buildup** (a Fire sword swing builds both Fire and Slash, SB-11). Damage uses the single derived type (Fire wins). So keep both and carry them together.
- **Simpler shape:** `FWoRHitKind { ESpellElement Element; EPhysicalDamageType Physical; EWoRDamageType DamageType; }` with `FromPair(...)` (today's rule) and an explicit override. An Ice skill is then `{Water, None, Ice}`: Water-keyed and Water-sourced as ED-04 says, with Ice resistance and an Ice break.
  - The gamble's 12-way pick becomes one random `DamageType`.
  - `EffectMatches` compares one field.
  - The 4 "Water adds Ice" special cases become `Row.AlsoResistedBy` (S5).
- **Tag idea (free now, costly later):** make `DamageType.Ice` into `DamageType.Element.Water.Ice` while nothing in content references it. Then "on Water hit" triggers hear Ice through ordinary tag matching (ED-04). Default: yes, following the deckbuilder's "Ice is a damage type of Water" (`02-enums.md`).
- **Behaviour change:** none until Ice content exists. **Files:** more than 3, so clusters: (1) struct + the resistance query; (2) buildup; (3) GAS executions.

### S7. Resistance columns: 4 parallel 12-field shapes + 14 attributes, copied field by field — M
- **The shapes:**
  - `ClassInnateResistanceTable::FResistanceRow` (`H/Combat/Resistance/ClassInnateResistanceTable.h:47`)
  - `FResistanceBonus` (gear, saved; `H/Equipment/FResistanceBonus.h:24`)
  - `FResistanceProfileRow` (data asset; `ResistanceProfileData.h:19`)
  - the Details display copy (`CharacterData.cpp:29`)
  - `UWoRAttributeSet_Resistance` (`Resist_*`, 14 including Ice)
- **Field-by-field copies:**
  - `ResistanceQuery.cpp:102-104`
  - `ResistanceProfileData.cpp:21,27,48`
  - `FResistanceBonus.cpp:19,37,96` (`case 0..11` index arithmetic)
  - `EquipmentDataBase.cpp:407`
  - `CharacterData.cpp:29`
  - three debug printers
- **No Ice column:** none of the four struct shapes has one, so B-064a means adding Ice in every shape and every copy.
- **Simpler shape:** keep every serialized field (save and asset safe). Add one static column list `{EWoRDamageType, float Struct::*}` per struct, with `Get(EWoRDamageType)` / `ForEachColumn`. The copies, sums, rerolls and debug prints become loops, and adding Ice is one field + one row per struct (an append to saved data, so no save break, D-059).
- **Behaviour change:** none (check with `wor.CompareResistance`). **Files:** 6, so 2 clusters.

### S8. Status-break table: trigger, size and timing in one row (extends DRY A8) — M
- **Current:**
  - The trigger comes from `BarCapTriggerResolver` (`:30-75`).
  - The size and timing come from a switch in `ApplyTriggeredSkillEffect` (`SkillEffectManager.cpp:2883-3060`). It still has literals Crown ruled must become named tunable constants (rules 03, "every size becomes a named, tunable constant"): Burn `MaxHP * 0.08f` (`:2913`) with a `30.0f` fallback, Pierce/Earth `30.0f` (`:2930`), Reality Burst `0.25f` (`:3037`).
  - The flavour names come from `SkillEffectDisplayNames` (A8).
  - Water's second half (Spell Speed) is added after the switch (`:3053-3060`).
- **Simpler shape:** `FBreakRow { Trigger; SizeConstant; Timing; bShareOfMaxHP; ExtraEffect; FlavourName }` per damage type, inside S5's table (the BD drain stays a source-side override). Ice's slow (B-064a) and any new break become one row.
- **Behaviour change:** none (the same values become named). **Files:** 3. Do it with or after the GAS buildup step, so the cap effect isn't built twice.

### S9. Element baked into effect types — M, with the GAS spec builder
- **Current:**
  - 9 `Grant<Element>Immunity` members (`ESkillEffectType.h:177-185`), plus `ApplyBurn/ChillToTarget` (Fire/Water hard-coded at `SkillEffectManager.cpp:2061,2072`).
  - Meanwhile every payload already has an `Element` field (`FSkillEffectPayload.h:33`).
  - GAS already plans one `UGE_ElementImmunity` keyed by element (`docs/Design/GASMigration.md` §3).
  - There is no Ice or physical immunity, and no element DoT other than Fire and Water, without adding new enum members.
- **Simpler shape:** append `GrantElementImmunity` and `ApplyDOTOnHit` (keyed by `Element`, later by `DamageType`). Keep the 9 + 2 old members working (append-only), hide them in the editor, and have the spec builder map both to the same GE.
- **Files:** 3. **Decision:** none for the engineering; new content uses the keyed form. Re-authoring old assets is Crown's editor pass, and it is optional.

### S10. "Generic", innate-element reads and the Reality-Refractor check (low risk, simplifies reading) — S–M
- **Raw `InnateElement` reads:** about 15 production sites read it directly instead of `UCharacterData::GetElement()`, which gates on Caster. Examples: weather `P/Combat/Mechanics/WeatherStateManager.cpp:131`, the innate source `ActionExecutor.cpp:4129`, `BrokenDarknessManager.cpp:1015`, `CharacterPanelWidget.cpp:863`, `DamageCalculator.cpp:175` (already B-080). The editor hides the field for non-Casters but keeps its value, so a class change can leave a stale element behind.
- **"Reality Refractor" checks:** written ≥5 times (`ActionExecutor.cpp:4093,5522`, `CrystalManager.cpp:299,390`, `BrokenDarknessManager.cpp:1016`).
- **Simpler shape:** `ElementHelpers::IsConcrete(E)` (Fire..Reality) and `IsInherit(E)` (Generic, on skills only). Use them for the inline "Generic or None" pairs. Add `UCharacterDataComponent::GetInnateElement()` (Caster-gated) and `IsRealityRefractor()`. Document the legal subset per field (the deckbuilder's Element = None + nine) in `IsDataValid`.
- **Behaviour change:** none for valid data. **Files:** 3 + 3 (two clusters).
- **Related:** the Broken Darkness state is three booleans (`UCharacterData::bBrokenDarknessInnate`, `UCharacterDataComponent::bIsBrokenDarkness`, `UBrokenDarknessManager::bIsFlipped`). Fold this into DRY A15's `IsInBDForm` rather than adding an enum now.

### S11. Small items (low value, do as filler)
- **Cost clamp:** the `0.1f`/`2.0f` clamp and the `/ 100.0f` literal (`ActionExecutor.cpp:4298-4300`, beside `CombatConstants::STAT_PERCENT_DIVISOR`) should be named constants. 8 `/ 100.0f` sites remain (ActionExecutor ×4, `FEquipmentStatBonus.cpp` ×3, CharacterDataComponent ×1).
- **Unwritten defense flags:** `FCombatHitResult::bWasBlocked/bWasParried/bWasDodged` (`H/Combat/Actions/ActionStructs.h:535-541`) are never written in C++, while `EDefenseType` already says which defense landed. Check whether a Blueprint reads them, then replace them with one `EDefenseType` field (B-105 era).
- **Tier letters:** `EItemTier` (F=0..S=6), `EItemQuality` (F..S) and `EScalingTier` (**S=0**..F=6) share the same letters with reversed numbers. No cast between them exists today; add a comment ("never cast between these") and route their letters through one helper (DRY A14). `EItemQuality` is rolled, saved and filterable, but no gameplay number reads it yet (its readers are `PoolAccessors.cpp:97-137` and the save). Fine, but don't show it beside Tier until it does something.
- **Difficulty enums:** the four (`EAIDifficulty`, `ETrialDifficulty`, `EDefenseDifficulty`, `EEncounterRank`) are independent. No change proposed.
- **"Resistance" names three things:** the general Spirit sub-stat, per-type resistance, and `ResistanceBuff` keyed to an element (Element None = the non-elemental bucket, not "all"). Clarify the editor tooltip on `ResistanceBuff` so designers don't author a "blanket" one by mistake (ED-27).

---

## 3. Suggested order (3 files or fewer per cluster; build + tests + `wor-reviewer` each)
1. **S1** (D-054 + ModifyEnergyCost sign) and **S3** (essence names). These are bug fixes, 1–2 clusters each.
2. **S2** (legacy aliases + validator), after Crown checks asset use in the editor.
3. **S4** (infusion source view), right after DRY A2/A3.
4. **S5** table + spec, then its forwards. **S7** resistance columns.
5. **S6** hit struct, and **S8** break rows, as the opening of **B-064a** (Ice). Its "survey every element switch first" becomes "add rows".
6. **S9** with the GAS spec-builder work; **S10**, **S11** as filler.

## 4. What GAS stage 3 already simplifies (don't plan these twice)
Once B-042f passes PIE and **B-042g** retires the enum path, these go away on their own:
- `FActiveSkillEffect` storage and its state booleans: `bCanStack` / `bDoesNotStack` / `bRefreshDurationOnReapply` (stacking, owned by the facade since B-042h), `bPermanent` / `bCountsHolderTurns`, `bProcessedThisTurn` / `bPendingRemoval` (`ActiveSkillEffect.h:119-260`)
- `ESkillEffectTiming`
- the Space-B / hashed effect IDs (`GetTypeHash(Attacker) ^ type`, `SkillEffectManager.cpp:2059-2081`)
- the enum-path immunity query (`IsImmuneToEffectType`, `SkillEffectManager.cpp:2528`; replaced by `Immunity.*` tags)
- the `GetTotalStatModifier` mirrors

Stat composition is already one shape (`GetStatLayers` / `FWoRStatLayers`, B-042p–t), so it is **not** listed as a finding.

**GAS does not simplify:**
- the authoring enums (`GASMigration.md` §3: "Authoring doesn't change"; `ESkillEffectType` and `ESpellElement` stay)
- the element and damage-type maps. Tags add a *further* mapping (`WoRGameplayTags.cpp:84-146`), which is why S5 should feed the tag tables.
- the resistance rows (S7). The `Resist_*` attributes are filled *from* them.
- the bar-cap/break table (S8)
- `IsBuff`/`IsDebuff` (S1). The spec builder still uses them.
- the crystal, essence, name and colour tables

So S1–S10 are not duplicate work with GAS. S9 and S8 should be built *inside* the GAS spec-builder and buildup steps, not before them.

## 5. Questions raised (reference-backed defaults, approved by default unless Crown changes them)
- **S1:** `ModifyEnergyCost` follows D-054's sign rule (a cost cut on an ally is a buff). Also: D-054 itself needs a backlog job.
- **S2:** a legacy "Speed (Generic)" effect means turn speed.
- **S3:** what to call the Generic/Quartz essence (a game word, Crown names it; keep "Generic" until then).
- **S6:** whether Ice's tag nests under Water (`DamageType.Element.Water.Ice`). Default: yes, per ED-04 and the deckbuilder.
