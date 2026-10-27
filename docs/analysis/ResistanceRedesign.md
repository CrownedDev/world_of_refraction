# Resistance redesign — plan (B-145)

**Job:** B-145 (review and plan, no code until Crown approves; Q-145). **Builds on:** B-044 (`EffectsOverhaul.md`), B-041 (attribute sets, `GASSystem.md`), D-005 / D-036 (resistance scales elemental damage 1:1, capped ±50%), D-037 (a damage type beside the element), D-029 (Ice). **Stat maths outside resistance:** B-146 (`StatMathsRedesign.md`).

**How to read it:**
- Code is cited as `path:line` under `Source/world_of_refraction/`. Today's model is in `docs/Architecture/ResistanceSystem.md`.
- GAS names are from Unreal 5.x. Ones marked *(verify)* are checked in the first build step.
- Questions are **Q-145, Q-145b, Q-145c** in `DesignDecisions.md`.

---

## 1. Today

Resistance is computed in one getter, `UStatusBuildupManager::GetTotalStatusResistance` (`Private/Skills/Effects/StatusBuildupManager.cpp:215-292`), from **eight sources in two layers**, and read **only by status buildup** (`:440`). Damage reads none of it (`Private/Combat/Damage/DamageCalculator.cpp:208-209`: `ElementMultiplier = 1.0f`).

| # | Source | Layer | Keyed by | Where |
|---|---|---|---|---|
| 1 | Spirit-driven Resistance stat, capped alone at 0.5 | General (×, floor 0) | — | `CharacterDataComponent.cpp:1332` |
| 2 | Gear `BonusResistance` points | General | — | `StatusBuildupManager.cpp:239` |
| 3 | Attached Resistance stone % | General | — | `:243` |
| 4 | `ResistanceBuff` / `ResistanceDebuff` effects | Matched (+, signed) | element **or** physical type | `:253`, `GetTotalElementResistance` `:164-213` |
| 5 | Class / innate table (Broken Darkness row overrides) | Matched | element + physical | `ClassInnateResistanceTable.h` |
| 6 | Rolled / authored gear `FResistanceBonus` | Matched | element + physical | `:261-271` |
| 7 | `ModifyStatusResist` (Resistance Stone consumable, curses) | Matched | — | `:276-280` |
| 8 | Boss rising resistance per status break (B-193) | Matched | — | `:283-286` |

`return clamp(General + Matched, −1, +1)`; buildup is `× (1 − total)`.

A third, separate value, `UCharacterDataComponent::GetEffectiveResistance` (`CharacterDataComponent.cpp:1387-1424`), feeds **crystal wear** (uncapped, element-agnostic). It is what the B-041 `Resistance` attribute mirrors (`Private/GAS/WoRAttributeSyncComponent.cpp:93`). It stays as it is.

**What's wrong with it:**
- **Two questions in one number.** "How much less Fire *damage*" (D-036: per-type sources #4–6 only) and "how much less Fire *buildup*" (everything) are mixed in one getter. B-064 would have to pull the per-type part back out.
- **No damage type.** A hit carries an element (`ESpellElement`, also the identity of characters, crystals and rings) and a physical type (`EPhysicalDamageType`, `None` for spells). "Physical" is `None` + a subtype (D-037 asks for a proper type); Ice has nowhere to go (D-029).
- **The class table is C++** (`ClassInnateResistanceTable.h`), and each source is gathered by hand at the moment of the hit. The UI and debug tools have to repeat the gathering (`ResistanceProfileDebug.cpp:171-211` does, with a MATCH check).

## 2. The standard shape

What Unreal's GAS and the games it's compared with do (Q-145 sources):

- **One value per damage type, held on the character.** Path of Exile and Diablo 4 keep one resistance per element with a cap (75% / 70%); Elden Ring keeps a damage negation per type, physical subtypes included (Standard, Strike, Slash, Pierce, then Magic, Fire, Lightning, Holy); Expedition 33 gives each enemy a reaction (weak +50%, resistant −50%, immune, absorbs) per damage type, Physical among them. In GAS that is **one attribute per type**.
- **Every source is a modifier on that attribute.** Class table, gear and buffs each apply a GameplayEffect with modifiers; removing the source removes its modifier. Nothing is gathered at hit time.
- **One calculation reads it.** Lyra's damage is one `UGameplayEffectExecutionCalculation` (`ULyraDamageExecution`) that captures attributes and outputs the change; the GASDocumentation sample's damage execution captures the target's Armor the same way. Here: one damage execution and one buildup execution, both reading the same attributes.
- **Caps live in the calculation, not the attribute.** The attribute shows the raw sum (+60% Fire is visible in the UI); the damage execution applies the ±50% cap (D-036), the buildup execution the ±100% clamp. Both caps are named constants, as today.

## 3. The plan

### 3.1 A damage type (D-037)

New `EWoRDamageType : uint8`, append-only, explicit values, element members matching `ESpellElement`'s numbers so a cast element converts with no table:

| Value | Member | Notes |
|---|---|---|
| 0 | `None` | no damage (pure effects); reflected damage is pure (D-087) |
| 1–9 | `Fire` … `Reality` | same numbers as `ESpellElement` 1–9 |
| 10 | `Slash` | Physical |
| 11 | `Pierce` | Physical |
| 12 | `Impact` | Physical |
| 13 | `Ice` | D-029; keyed Water (Water sources unlock it), no crystal, ring or Caster |

- **Physical** is the group 10–12. Shown as "Physical (Slash)" etc., `FText` from the shared name table (D-061/D-062).
- Native tags beside B-040's: `DamageType.Physical.Slash`, `DamageType.Physical.Pierce`, `DamageType.Physical.Impact`, `DamageType.Element.Fire` … `DamageType.Ice`; a modifier or immunity can key the parent (`DamageType.Physical` = all three).
- `ESpellElement` stays the **identity** (what a character, crystal, ring is; Generic = inherit). `EPhysicalDamageType` stays the **status axis** of a hit (Slash → Bleed, Pierce → Armor Break, Impact → Stun).
- A hit carries **both**: `DamageType` (what damage resists) and the status axes it already has (element + physical type, for buildup). D-007 / B-080: an uninfused swing is `Slash` damage with physical type Slash; a Fire-infused swing is `Fire` damage with element Fire and physical type Slash (buildup reads both, as today).

### 3.2 One resistance value per damage type

`UWoRAttributeSet_Resistance` (a third set beside Vitals and Stats), fractions (0.15 = 15%):

| Attribute | Fed by (as modifiers) | Read by |
|---|---|---|
| `Resist_Fire` … `Resist_Reality` (9), `Resist_Slash`, `Resist_Pierce`, `Resist_Impact`, `Resist_Ice` (13) | #4 typed buffs/debuffs, #5 class table, #6 gear rolls | damage (capped ±50%) and buildup |
| `StatusResistance` | #1 base (the capped Spirit stat), #2 gear points and #3 stone as multiply modifiers | buildup only (floor 0, ceiling 1) |
| `StatusResistBonus` | #7 `ModifyStatusResist`, #8 boss rising resistance | buildup only (signed) |

Per source:

| # | Today | Standard | Notes |
|---|---|---|---|
| 1 | `GetCrystalResistanceStatCapped` at hit time | base value of `StatusResistance`, from the Spirit composition (B-146) | the "capped alone, then gear multiplies" rule needs the base computed first |
| 2, 3 | multiplied in by hand | **MultiplyCompound** modifiers (`EGameplayModOp::MultiplyCompound` multiplies, 1.5 × 1.5 = 2.25; plain Multiply adds the bonuses, 1 + Σ, which would change numbers) | one infinite GE per equipped piece / stone |
| 4 | `GetTotalElementResistance` scans active effects | a duration GE per buff/debuff with an additive modifier on the matching `Resist_*` (or on all three physical ones for a `DamageType.Physical` buff) | B-042's turn tracker ends it (Q-044 durations) |
| 5 | C++ table, resolved per hit | one infinite **class GE** at spawn, magnitudes from a **data table** (row per class / innate element / Broken Darkness) | B-095 becomes "the data table"; a BD transform removes the class GE and applies the BD row's (BD stays custom, D-P01: it only swaps the GE) |
| 6 | `GetActiveResistanceBonus` summed per hit | one infinite GE per equipped piece, SetByCaller magnitudes from the instance's `FResistanceBonus` | the loadout keeps the handle; unequip removes it |
| 7 | `GetTotalStatModifier(ModifyStatusResist)` | a duration GE with an additive modifier on `StatusResistBonus` | sign kept (curse reaches the amplify range, the A2 rule) |
| 8 | `UBossPhaseComponent::GetStatusResistanceBonus` | an infinite GE whose stack count is the breaks so far | B-193's rule unchanged |

### 3.3 Which attributes a hit reads

| Hit's damage type | Damage resist (capped ±50%, D-036) | Buildup: Matched part |
|---|---|---|
| an element (Fire…Reality) | `Resist_<element>` | `Resist_<element>` + `Resist_<physical type>` (if any) + `StatusResistBonus` |
| Water | `Resist_Water` + `Resist_Ice` (Q-145b) | as above |
| Ice | `Resist_Ice` | `Resist_Ice` + … |
| Slash / Pierce / Impact | `Resist_<subtype>` (Q-145) | `Resist_<subtype>` + `StatusResistBonus` (as today: element None) |

Buildup total stays `clamp(clamp(StatusResistance, 0, 1) + Matched, −1, +1)`, so **every buildup number is unchanged**. Immunities stay tag checks (`Immunity.*`, B-040) before the maths.

### 3.4 The calculations

- **Damage:** the damage execution (B-146 owns its full shape) gains one step where `ElementMultiplier` sits today: `× (1 − clamp(resist, −0.5, +0.5))`. Captured from the target **at execution** (not snapshot), so a buff applied this turn counts.
- **Buildup:** the buildup execution does what `AddStatusBuildup` does today (`StatusBuildupManager.cpp:303-460`), reading the three attributes above instead of the eight sources.

## 4. Steps (no big bang)

The game keeps working after each step (CLAUDE.md "Incremental"; Q-145c).

| Step | Job | What | Proof |
|---|---|---|---|
| 1 | **B-216** (new, before GAS stage 3) | `EWoRDamageType` + tags; **one query** `GetTypeResistance(Target, DamageType)` = sources #4–6 for that type, used by `GetTotalStatusResistance`'s Matched layer; the class table moves to a data table (B-095 folded in); `UWoRAttributeSet_Resistance` added as a **mirror** (like B-041), filled by the sync component; `wor.CompareResistance` | every buildup number identical (`wor.CompareStats`, a spec over class × type × physical); `wor.CompareAttributes` 0 differ |
| 2 | **B-080**, **B-064**, **B-064a** (now on B-216) | the hit's damage type (D-007); damage reads `GetTypeResistance` with the ±50% cap (D-005/D-036); Ice | their own tests (weak / resistant / neutral; Ice reaches Water resist) |
| 3 | **B-042** (GAS stage 3) | sources become GEs (§3.2); the attributes become the truth; `GetTypeResistance` reads the attribute; buildup and damage move into the executions (with B-146) | `wor.CompareResistance` and the before snapshot (B-134) identical |

Step 1 is code Crown approves with this plan (Q-145). Steps 2–3 are already queued jobs; this plan only changes what they build on.

## 5. Risks

- **Multiply order (#2, #3).** MultiplyCompound runs after the additive and plain multiply terms; if that order doesn't reproduce "capped stat × gear × stone", the General layer stays a custom calculation (an MMC). Caught by `wor.CompareResistance` in step 3.
- **Broken Darkness row swap.** The BD row replaces the class row mid-fight; a missed swap shows as a wrong row. `wor.PrintResistance` (today's `PrintResistanceProfile`) shows the active class GE.
- **Physical damage now resisted (Q-145).** A Generic takes 15% less physical damage than today, on top of Defense; a Resonator 15% more. Balance change, flagged in B-064's hand-off.
- **Save data.** None: resistance is runtime only; gear rolls are already saved (B-124).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Plan written | chore/B-145-resistance-plan |

## Sources

- [Path of Exile wiki: Resistance](https://www.poewiki.net/wiki/Resistance) (one resist per element, 75% cap); [Elden Ring wiki: Damage Negation](https://eldenring.wiki.fextralife.com/Damage+Negation) (per-type negation incl. Slash/Strike/Pierce); [Diablo 4 resistances (Maxroll)](https://maxroll.gg/d4/resources/resistances); [Expedition 33 elements and weaknesses (Playnews, Spanish)](https://www.playnews.gg/es/guias/clair-obscur-expedition-33-atributos-escalado-de-armas-y-debilidades-elementales-guia-de-dano).
- [Lyra: `LyraDamageExecution`](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) (Lyra sample); [GASDocumentation: Execution Calculations](https://github.com/tranek/GASDocumentation#concepts-ge-ec); [Epic API: `EGameplayModOp`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/EGameplayModOp__Type).
