# GAS Migration — design

**Status:** Agreed in principle (Crown, 2026-10-25). Not started.
**Why:** a standard, networking-ready way to do stats, effects and skills, for a Destiny-style shared world that will likely have real-time abilities outside battle. Background: `docs/analysis/ArchitectureReview_2026-10.md` §1.2 and §4.

---

## In one paragraph

We move **stats, effects and skills** onto Epic's Gameplay Ability System (GAS) in **four stages**, and the game keeps working after each one. **Turn order, the AI's decision-making, the grid, the parry/dodge windows and the UI stay ours.** GAS doesn't count in turns, so we add one small piece of our own, a **turn tracker**, that counts effect durations down at the end of each turn. **Authoring stays the same.** Our `UEffectDefinition` and spell/ability data assets remain the authoring format and get translated into GAS at runtime, so hundreds of assets don't have to be rebuilt.

---

## What stays ours vs what becomes GAS

| Area | Today | After | Notes |
|---|---|---|---|
| Turn order, speed debt, bonus and ritual (Execution) turns | `UTurnManager` | **Stays** | Gains one job: signal end-of-turn to the turn tracker |
| Combat flow, win check, teardown | `ACombatOrchestrator` | **Stays** | Calls the ability system instead of `SkillEffectManager` |
| Grid | `UCombatGridSubsystem` | **Stays** | |
| AI decisions | `UAIDecisionManager` | **Stays** | Reads attributes + tags instead of our getters |
| Parry/dodge/block windows | `UDefenseSystem` | **Stays** | Sends a GameplayEvent when a defense resolves; abilities wait on it |
| UI | Widgets (+ CommonUI, decided) | **Stays** | Reads attributes + tags |
| Damage formula | `UDamageCalculator` | **Formula stays**, wrapped in a GAS damage calculation (`UGameplayEffectExecutionCalculation`) | The math and its tests don't change |
| Stat composition (stat cap ×1.5 → gear multiplies → cap ×2.0, "Pattern P") | Getters on `UCharacterDataComponent` | **Stays as a pure C++ function**, fed by GAS attributes | Keeps the locked combat-economy math in one tested place |
| HP, EP, pillars, 12 substats, buildup | Fields on `UCharacterDataComponent` | **GAS AttributeSets** | |
| Buffs, debuffs, DOTs, stones, item effects | `USkillEffectManager`, `FActiveSkillEffect`, 108-value `ESkillEffectType` | **GameplayEffects + GameplayTags** | Also fixes the effect-ID collisions and the enum bloat (Architecture review §2.1, §2.3) |
| Skills (montage chain, notifies, warp, costs, delivery) | `UActionExecutor` | **GameplayAbilities + AbilityTasks** | The largest stage |
| Status bars → cap effect | `UStatusBuildupManager` | Buildup attribute + cap effect | |
| Broken Darkness | `UBrokenDarknessManager` | **Stays custom for now** | "BD-states-as-effects" is locked; reopening it is Crown's call (see Open decisions) |

---

## Key design choices

### 1. Where the ability system lives

- **Combatants:** an `UAbilitySystemComponent` (ASC) on `ACombatCharacter`.
  - Battle pawns are spawned per fight, so the pawn owns its own ASC.
- **Player characters, including co-op and persistence: also on the character** (decided; see Open decisions #3).
  - Not on `APlayerState`: one player leads up to 3 characters, and AI companions have no PlayerState.
  - Continuity across levels comes from the save/persistence layer.
  - Look the ASC up through `IAbilitySystemInterface` anyway; it costs nothing.
- **Replication mode:** `Mixed` for player-controlled characters, `Minimal` for AI.

### 2. Turn-based durations (the one custom piece)

GAS durations are in seconds. We need "lasts N turns".

- **Effect setup:** turn-limited effects are **Infinite** duration GEs. The turn count travels with the spec as a SetByCaller value under the tag `Data.Duration.Turns`.
- **`UTurnEffectTrackerComponent` (new, on each combatant):**
  - Listens to the ASC's "effect added" / "effect removed" events.
  - Keeps `ActiveHandle → TurnsRemaining`.
  - On re-stack or refresh, resets the count per the effect's refresh rule.
- **Turn ticking:**
  - `UTurnManager` end of turn → tracker decrements → removes the effect at 0.
  - Start-of-turn and end-of-turn ticking effects (DOTs) are applied by the tracker at the right turn phase, matching today's `ESkillEffectTiming`.
- **Charges:** separate from turns, as today. Tracked the same way under `Data.Charges`; spending a charge decrements and removes at 0.
- **Fires once per match:** a loose tag on the target (`State.FiredOnce.<DefID>`) or a tracker set keyed `(Actor, Source)`. This also fixes today's per-ID bug.

### 3. Effects without re-authoring every asset

- Hand-authoring a GameplayEffect asset per effect would mean hundreds of new `.uasset`s.
- **Instead, a small set of C++ GameplayEffect classes, one per effect family:**
  - `UGE_StatModifier_<Stat>`: one per substat/pillar, with a SetByCaller magnitude (sign = buff/debuff)
  - `UGE_DamageOverTime`
  - `UGE_HealOverTime`
  - `UGE_Stun`, `UGE_Silence`, `UGE_HealBlock` (tag-granting)
  - `UGE_ElementImmunity` (grants `Immunity.Element.<X>`)
  - `UGE_Reflect`
  - `UGE_LastStand`
  - …
  - About 35 classes, mirroring Architecture review §2.1 step 2.
- **Translator:** `FEffectSpecBuilder` turns an `FSkillEffectPayload` (from a `UEffectDefinition`) into the right GE class plus SetByCaller values (magnitude, turns, charges, element) plus tags.
- **Authoring doesn't change.** Designers keep editing `UEffectDefinition`s. A designer-only effect type later can still be a GE asset.

### 4. Tags (stage 1, useful on their own)

- Native tags declared in C++ (`UE_DEFINE_GAMEPLAY_TAG`). Proposed tree:
  - `Element.{Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, Reality, Generic, None}`
  - `Status.{Stun, Silence, HealBlock, Burn, Chill, …}`
  - `Effect.Family.{StatModifier, DOT, HOT, Control, Immunity, Reflect, LastStand, …}`
  - `Data.{Duration.Turns, Charges, Magnitude, Element}` (SetByCaller keys)
  - `Event.Combat.{Impact, DefenseResolved, TurnStart, TurnEnd}` (GameplayEvents)
  - `Class.{Generic, Caster, Resonator}`
- **Keep the enums for now; each gets a mapping table.** The enums are serialized in assets. Tags become the runtime language first, and the enums retire later.

### 5. Skills (stage 4)

- **One GameplayAbility class per delivery shape** (melee, projectile, AOE, instant, ritual), not one per skill. The spell or ability data asset is passed in as the ability's source object.
- **The montage chain becomes AbilityTasks:**
  - `PlayMontageAndWait`
  - `WaitGameplayEvent(Event.Combat.Impact)` for notify-driven impacts
  - Motion warping stays as it is.
- **Costs:** EP, durability and HP (infusion) become a custom cost check plus cost GEs. The existing `CalculateActionEnergyCost` math is reused.
- **Turn integration:** the orchestrator activates the ability on the actor's turn, and ability end = action complete. The turn system still owns *whose turn it is*.

---

## Stages (≤3 files per cluster, game playable after each stage)

**Before starting:**
1. `fix/combat-hang-arc-a` and `refactor/bp-logic-to-cpp` merged. They touch the same files.
2. CommonUI enabled.
3. GameplayAbilities plugin enabled in `.uproject`.
4. `GameplayAbilities` and `GameplayTasks` added to `Build.cs`.

### Stage 1 — Tags (small)

1. Native tag declarations + enum↔tag mapping tables (elements, statuses, effect families, classes) + a debug dump command.
2. Read sites switch to tags where they *query* (immunity checks, the AI's "is stunned/silenced" checks, UI icons). Writes stay on enums.

- **Done when:** tag queries give the same answers as the enum queries. Verified by a `wor.CompareTags` debug command.

### Stage 2 — Stats (medium)

1. `UWoRAttributeSet_Vitals` (HP, MaxHP, EP, MaxEP, Buildup, MaxBuildup) + `UWoRAttributeSet_Stats` (pillars, 12 substats, plus the composition inputs: base, equip %, stone %, transient %) + ASC on `ACombatCharacter`.
2. `UCharacterDataComponent` getters read from attributes; the composition function stays C++. `CurrentHP`/`CurrentEP` writes go through the ASC.
3. UI and AI read paths: no change needed, because they still call the same getters.

- **Done when:** a `wor.CompareStats` snapshot is identical before and after on the same loadout.

### Stage 3 — Effects (large)

1. `UTurnEffectTrackerComponent` + its TurnManager hook.
2. The ~35 effect-family GE classes + `FEffectSpecBuilder`.
3. `SkillEffectManager` apply/remove/query re-routed to the ASC (a facade during transition, so callers don't change).
4. Status buildup → attribute + cap effect.
5. Stones and item effects through the same builder.
6. Retire `FActiveSkillEffect` storage once no reader remains.

- **Done when:** the starting-effects, defender-trigger and DOT tests from the 2026-06-20 session doc still pass, plus a `wor.CompareEffects` snapshot.

### Stage 4 — Skills (large)

1. Ability classes per delivery shape + montage/event tasks.
2. Costs.
3. Orchestrator activates abilities; `UActionExecutor` shrinks to target resolution + presentation helpers.
4. Rituals (Execution turns) as an ability that arms, then fires on the scheduled turn.
5. Delete the dead paths.

- **Done when:** every demo skill plays identically, with the same damage numbers, in PIE.

---

## What this supersedes

These Architecture review items are absorbed by the migration and should **not** be done separately:
- §2.1 step 2 (effect enum collapse)
- §2.3 (effect-ID schemes)
- the effect half of §1.1 (subsystem state)

§2.1 step 1 (stat-modifier lookup table) is still worth doing first. It becomes the stage-2 mapping.

---

## Debug tools (per CLAUDE.md rule)

- **Per stage:** a `Compare*` snapshot command (`wor.CompareTags`, `wor.CompareStats`, `wor.CompareEffects`) so each stage can be proven equal to the old path in PIE.
- **Built-in:** GAS's `showdebug abilitysystem` comes for free.

---

## Risks

- **Learning curve:** GAS concepts (spec, handle, prediction keys, AttributeSet clamping). Mitigation: stages 1–2 are small and teach the basics before the big stages.
- **Turn-duration tracker correctness:** stacking/refresh semantics must match today's rules (refresh if non-stacking, stack if stackable, merge by definition identity). Write the Automation Spec tests for this first.
- **Pattern-P math:** must stay exact (locked combat economy). Keeping it as one pure function avoids re-implementing it inside GAS.
- **Asset references:** spell/ability assets gain an ability-class field (defaulted by delivery type, so no hand edits needed).
- **Feature freeze:** stages 3–4 touch the whole combat core. Avoid other combat features while they're in flight.

---

## Open decisions (Crown)

1. **Broken Darkness — DECIDED (2026-10-25): option C.**
   - Keep it custom for now; it reads/writes GAS attributes and exposes `State.BrokenDarkness` / `State.Overloaded` tags.
   - **Revisit after stage 3.** If it's ever moved, it is built beside the old code and only switched over when a `wor.CompareBD` side-by-side (energy, stacks, strain, damage, state over scripted sequences) and Automation tests show **zero differences**. Watch rounding (float attributes vs int HP/EP), event order, turn phase and RNG order.
2. **Real-time field abilities — DECIDED (2026-10-25): YES, all of them.**
   - First strikes in the field, traversal, crystal powers on the world, real-time effects on other players.
   - **Movement types** (flight, speed bursts, blink/teleport) come from activated spells/abilities → GameplayAbilities with client prediction.
   - **Evolution crystals grant passives** → infinite GameplayEffects while equipped: stat changes (e.g. move speed), granting abilities (e.g. glide), modifying abilities.
   - This revives the evolution-feature system dropped in `docs/sessions/2026-06-20.md`.
3. **Where the ability system lives — DECIDED (2026-10-25): on the character, permanently.**
   - Every character (hero, recruits, AI companions, enemies) carries its own ASC.
   - A player leads a **party of up to 3**, and AI companions have no PlayerState, so the Lyra-style "ASC on PlayerState" (one player = one hero) doesn't fit.
   - Non-seamless travel recreates the PlayerState anyway.
   - Persistence across hub → trial → battle comes from the save/persistence layer: snapshot attributes, equipped crystals and granted abilities on level exit, rebuild on spawn.
   - PlayerState holds player-level data only (account, Prisms/Diamond, which party you lead).

---

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-25 | Initial design: 4-stage migration, turn tracker, family GE classes + spec builder, what stays custom | docs/gas-migration |
| 2026-10-25 | Decisions: BD option C (custom, revisit after stage 3 with zero-diff bar); real-time field abilities yes (movement types + evolution passives); ASC on the character permanently | docs/gas-decisions, docs/gas-asc-placement |
