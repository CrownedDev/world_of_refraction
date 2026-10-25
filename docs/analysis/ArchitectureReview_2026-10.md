# Architecture & Simplification Review — 2026-10

Two inputs:
1. A **design-level simplification audit** of `main`. This sits above the line-level duplication in `CodebaseReview_2026-10.md` §5.4.
2. **External research** on current UE5 practice for each major architecture choice.

`S/` = `Source/world_of_refraction/`. ✔ = re-read in source during synthesis.

**Research caveat:** page fetches were blocked, so the external research rests on search results plus general UE knowledge. Claims from general knowledge are marked **[GK]**. Sources are listed per item.

**Pending branches:** **A** = `fix/combat-hang-arc-a`, **B** = `refactor/bp-logic-to-cpp`. Items marked *blocked* overlap one of them.

---

## 1. Are we going in the right direction? (per architecture choice)

| # | Choice | Verdict | Cost to change: now → later |
|---|---|---|---|
| 1 | All combat services as **GameInstance subsystems** (20 of them ✔) | **Diverges — split by lifetime** | Low → grows with every system |
| 2 | **Custom effect system** instead of GAS | **Aligned**, but the 108-value enum is the weak point | — / GAS migration is expensive after co-op starts |
| 3 | UMG + C++ bases, **no CommonUI**, gamepad = yes | **Diverges — decide on CommonUI now** | Cheapest during branch B → expensive once UI matures |
| 4 | Defense windows timed with **`FPlatformTime`** ✔ (`DefenseSystem.cpp:68,212,422`) | **Diverges** | Trivial now |
| 5 | No save system yet; `SaveGame` tags; non-seamless `OpenLevel` | **Aligned** for now, with caveats | Medium either way |
| 6 | Almost no replication; global `FRand` | **Diverges — prepare now** | Low now → high once `Server*` functions hold real logic |
| 7 | All logs on `LogTemp`; no automated tests | **Diverges — cheapest fix on the list** | Trivial |
| 8 | Montages + notifies + motion warping | **Aligned** | — |
| 9 | Separate **battle map** via `OpenLevel` | **Aligned for single-player; diverges for a shared world** | Moderate either way |

### 1.1 GameInstance subsystems for combat state

- **Why it matters:**
  - GI subsystems live for the whole game session and survive level changes. A **WorldSubsystem** lives and dies with its level, so per-fight state cleans itself up.
  - Subsystems never replicate. Shared match state belongs on **GameState** or a replicated combat actor.
  - Initialization order differs between PIE and standalone (UE-186247).
- **In our code:** per-fight state lives in GI subsystems. This is the root cause of the hand-written combat teardowns (review §5.1 #2) and of state leaking between fights:
  - `SkillEffectManager::ActiveEffects` and `ArmedConditionals`
  - `StatusBuildupManager`, `DefenseSystem`
  - `CombatGridSubsystem` — `TMap<AActor*,…>` with **strong raw keys** ✔ (`CombatGridSubsystem.h:261`). That is a world-leak risk, and `bCheckForLevelGCBeforePIE=false` hides it.
  - `TurnManager`, `ActionExecutor` (pending chain/burst state), `AIDecisionManager`, `CombatCommandMenuSubsystem`, `WeatherStateManager`
- **Keep as GI:**
  - Stateless services: DamageCalculator, ItemExecutor, CrystalManager, EconomyService.
  - Cross-level state: PartySession, TrialRun, Pool.
- **Pragmatic path:**
  - **Now (after A merges):** the shared `ReleaseCombatResources` (A) calls a `ResetForCombat()` on each per-fight subsystem.
  - **Later:** convert the four per-actor-map ones (effects, buildup, defense, grid) to a WorldSubsystem or a combat actor.
  - **Before co-op:** move turn/match state onto a replicated `ACombatManager` or GameState component.
- **Cost:** about 108 C++ lookups change, and BP "Get GameInstance Subsystem" nodes break. That is another reason to finish the BP→C++ move first.
- Sources:
  - https://uhiyama-lab.com/en/notes/ue/subsystem-gameinstance-world-localplayer/
  - https://docs.unrealengine.com/4.27/InteractiveExperiences/Networking/Blueprints
  - https://issues.unrealengine.com/issue/UE-186247

### 1.2 Custom effects vs Gameplay Ability System (GAS)

- **Custom is defensible for turn-based.**
  - GAS has no native turn durations. People bolt on "infinite effect + turn counter" workarounds.
  - Its biggest payoff is real-time prediction, which a turn-based game barely needs.
- **But GAS-based turn-based games exist:** the commercial ACF J JRPG plugin and a 2024 Aalto thesis.
- **The real weakness is ours, not the choice:** see §2.1. 108 enum values sit in a switch, and roughly 55 of them are just signed stat modifiers.
- **Decision point:** commit to custom (and make it data-driven) or to GAS **before** co-op work. Migrating afterwards is expensive.
- Sources:
  - https://forums.unrealengine.com/t/how-to-think-about-time-based-aspects-of-gameplay-ability-system-in-terms-of-turn-based-instead/451100
  - https://forums.unrealengine.com/t/gameplay-ability-system-turn-based/126620
  - https://aaltodoc.aalto.fi/items/20f87aef-461d-4358-bef4-6bc436f4131f/full

### 1.3 CommonUI

- Epic presents CommonUI as the multiplatform/gamepad path. It provides:
  - an activatable widget stack with input routed to the top widget
  - a standard Back action
  - platform glyphs driven by controller data assets
- Epic's own guidance: adoption cost rises as the UI matures.
- **We are rewriting widget logic into C++ bases right now (branch B).** That is the cheapest moment to reparent:
  - screens → `UCommonActivatableWidget`
  - buttons → `UCommonButtonBase`
- The activatable stack also enforces our "add to viewport, don't recreate inside designer widgets" rule.
- **Caveats:**
  - Not meant for in-world widget components (the floating enemy plates).
  - There are forum reports of gamepad navigation bugs.
  - Lyra is the reference implementation.
- Sources:
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-advanced-multiplatform-user-interfaces-with-common-ui-for-unreal-engine
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/design-guidelines-for-using-commonui-in-unreal-engine

### 1.4 Defense timing

- **Problem:** `FPlatformTime` ignores pause and time dilation. Pausing mid-window lets the window run out, and Expedition 33-style slow-motion on a parry would desync.
- **Fix:** use world time.
- **Better [GK]:** define windows as **AnimNotifyStates** on the attack montage, so they stay in sync with the animation by construction.
- **For co-op [GK]:** the client judges the input and sends it with a timestamp. The server validates it within a latency tolerance.
- Sources:
  - https://community.gamedev.tv/t/why-not-to-use-fplatformtime/1277
  - https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FGameTime

### 1.5 Saves and level travel

- `USaveGame` + `ArIsSaveGame` proxy archive is the standard pattern, with per-actor records of class, name and data blob. Three requirements:
  - **Stable IDs instead of pointers.** Our `PersistentID` FGuid is re-minted every spawn, which breaks this (review §4).
  - Two-phase load: create everything, then restore data.
  - Avoid Blueprint structs in saved data.
- Non-seamless `OpenLevel` is fine for single-player. Multiplayer needs `ServerTravel`, seamless travel and `PlayerState::CopyProperties` [GK].
- Sources:
  - https://forums.unrealengine.com/questions/42349/how-do-i-use-the-savegame-flag.html
  - https://unrealcommunity.wiki/savegame-pointers-and-structs-8wlg0qms

### 1.6 Replication model and random numbers

- **Standard turn-based co-op pattern:**
  - Server GameMode owns the rules.
  - Replicated GameState holds the turn order and the active combatant.
  - Clients send only **intent RPCs** ("skill X on target Y"), and the server validates them.
- Lockstep (replicating only decisions) needs full determinism. For 2–4-player co-op, server-authoritative is simpler.
- **Replace global `FRand` with a per-battle `FRandomStream` seeded by the server** (one per system). This also enables replayable fights and deterministic tests.
- Sources:
  - https://forums.unrealengine.com/t/replication-for-turn-based-game-best-practices/132038
  - https://answers.unrealengine.com/questions/447496/view.html

### 1.7 Logging and tests

- **Logging:** per-system categories (`LogWoRCombat`, `LogWoREffects`, …) via `DECLARE_LOG_CATEGORY_EXTERN`/`DEFINE_LOG_CATEGORY` give per-system verbosity and filtering. `UE_LOGFMT` has been available since 5.2.
- **Tests:** DamageCalculator, scaling, buildup and turn order are pure logic, which makes them ideal for **Automation Spec** or **CQTest**, run headless. Functional tests are for level flows.
- Our CallInEditor and console tooling complements tests; it doesn't replace them.
- Sources:
  - https://unrealcommunity.wiki/Logging
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine

### 1.8 Animation

- Standard practice.
- **Multiplayer watch item [GK]:** notifies only fire where the animation actually ticks; dedicated servers and off-screen meshes can skip them. The server should resolve damage from the action, with notifies used for presentation timing.

### 1.9 Separate battle level

- Matches the Expedition 33 player experience. How Sandfall implemented it (separate map vs streamed arena) is **unverified**.
- **For "Destiny-like" shared play:** `OpenLevel`/`ServerTravel` moves the whole server, every player included.
- **Alternative [GK]:** stream an **arena level instance** into the same world (`ULevelStreamingDynamic::LoadLevelInstance`) and teleport participants.
- Moving combat state to world scope (§1.1) makes this switch easier.

### 1.10 What we know about how Expedition 33 was built

- **Mostly Blueprints:** Sandfall reportedly built **~95% of gameplay in Blueprints**, with **four programmers**, near-vanilla UE5 and Sequencer for combat presentation (GDC 2026 talk coverage).
  - https://www.gamedeveloper.com/programming/clair-obscur-expedition-33-was-built-95-percent-with-unreal-blueprints
  - https://wccftech.com/sandfall-interactive-used-unreal-engine-blueprints-for-all-clair-obscur-expedition-33-gameplay-systems/
  - https://www.unrealengine.com/developer-interviews/clair-obscur-expedition-33-autonomy-creativity-and-community-key-to-sandfall-interactives-success
- **No public detail found** on their arena loading, UI tech or combat code structure.
- **Implication for us:** C++-first is **a valid preference, not a requirement** for this genre.
  - Our reason is different from theirs. A solo developer working with an AI assistant gains most from logic that can be read, diffed and reviewed (CodebaseReview §8).
  - The decision stands. Just don't treat it as the only correct path.

### 1.11 References worth borrowing from

- **Lyra (Epic):** GameState components, CommonUI, Gameplay Tags, message router. The best replication-ready structure reference even without GAS.
- **Narxim-GAS-Example:** turn-based GameplayEffects. https://github.com/Narxim/Narxim-GAS-Example
- **ACF J (Fab):** turn-based JRPG on GAS + CommonUI. A reference point.
- **Gameplay Tags vs enums:** tags such as `Element.Fire` and `Status.Burn` are hierarchical, editable without recompiling and replicate efficiently. Use them for open-ended sets (effects, statuses); keep enums for small fixed sets. https://tomlooman.com/unreal-engine-gameplaytags-data-driven-design/

---

## 2. Where the code is more complex than it needs to be

Ranked by payoff and risk. Line-level duplication is already in `CodebaseReview_2026-10.md` §5.4; this is the design layer above it.

### 2.1 Effect types — 108 enum values, about 55 of them stat modifiers (High payoff)

- **The bloat:**
  - `ESkillEffectType` (`S/Public/Skills/Effects/ESkillEffectType.h:21-262`). About 55 values are signed stat modifiers that `ApplyEffectLogic` treats as passive, doing nothing on apply (`SkillEffectManager.cpp:1318-1362`).
  - Every reader rebuilds "Buff − Debuff" by hand, about 20 times: `CharacterDataComponent.cpp:668,751…1295`, `DamageCalculator.cpp:455,492,637,657,705`, `TurnManager.cpp:228-230`, `ActionExecutor.cpp:4381-4383`.
  - **Synonym pairs:**
    - DamageBuff / ModifyDamageDealt
    - CritChanceBuff / ModifyCritChance
    - TurnSpeedBuff / ModifyTurnSpeed
    - SpellCostBuff / ModifyEnergyCost
    - HealingReceivedBuff / ModifyHealing
    - CritDebuff / CritChanceDebuff
    - EnergyDrain / DrainEnergy
    - Heal / HealthRestore
    - Cleanse / CleanseSelf
    - SkipTurn / Stun
  - Plus 9 `Grant<Element>Immunity` values, 3 `Remove*Debuff` and 3 `Reflect*`.
- **Live bug caused by the synonyms ✔:** the AI looks up the dead synonyms.
  - Stun detection checks `SkipTurn` (`AIDecisionManager.cpp:1746`).
  - Cleanse search looks for `Cleanse` (`:1912`), but only `CleanseSelf`/`CleanseAllies` have handlers (`SkillEffectManager.cpp:1539`).
  - So the AI never sees a stun and never picks a cleanse spell.
- **Simplify:**
  - **Step 1 — low risk, no asset changes:** one table `ESubStat → {Buff, Debuff, Modify}` plus a single `GetNetModifier(Actor, Stat)` replaces the ~20 hand-written pairs. Fix the AI to use `Stun`/`CleanseSelf` (blocked by A: `AIDecisionManager`).
  - **Step 2 — later, high risk:** collapse to `StatModifier{Stat}` (sign taken from the magnitude), `GrantElementImmunity{Element}` and `Reflect{Kind}`, which brings 108 values down to about 35.
    - Needs a PostLoad migration of `FSkillEffectPayload`, because enum values serialize by name, so redirects can't merge them.
    - **If Gameplay Tags (§1.11) are adopted, do it as part of this step.**

### 2.2 Three different answers to "what can this actor do right now?" (High)

- **The three sources:**
  - **UI:** `FCombatCapabilities::BuildFrom` (204 lines, `S/Private/UI/Combat/FCombatCapabilities.cpp:22`). It also lets Generic cast spells (`:62-68`).
  - **Executor:** `ActionExecutor::ValidateAction` (`:150`).
  - **AI:** `BuildOffensiveAction` (394 lines, `AIDecisionManager.cpp:1331`), which never checks stun.
- **Class gating is spread over four places:**
  - dead `CharacterClassHelpers`
  - dead `UCharacterData::CanUse*`
  - 44 switches in `LoadoutComponent`
  - 7+7 switches in `FCombatLoadout`/`FSavedLoadout`
- **Simplify:** one executor query, `GetLegalActions(Actor)` / `CanPerform(Actor, Action)`, consumed by both the UI and the AI. This is the root fix behind Arc A's safety net and Q9 (Generic spells).
- *Blocked by A and B.*

### 2.3 Five effect-ID schemes, none unique by construction (High)

- **The schemes:**
  - `PackEffectID` (×100)
  - `ItemIdentity::GetEffectSourceID` (bare hash, a different number space)
  - `GetTypeHash(X) ^ Type` (`SkillEffectManager.cpp:1687-1709`, `ItemExecutor.cpp:274,1087`)
  - `FMath::Rand()` (`:2405`)
  - unused `GetUniqueEffectID`/`NextInstanceID`
- **The problem:** one ID field does two jobs, telling instances apart and deduping/refreshing by source. That is the root of review §5.1 #6 and #7 (fires-once per ID, collisions, random refresh).
- **Simplify:**
  - a monotonic `InstanceID` assigned in `ApplyEffect`
  - a separate `FEffectSourceKey` for refresh and fires-once, keyed `(Actor, SourceKey)`
- *Partly blocked by A.*

### 2.4 Loadouts copy owned gear instead of referencing it (High payoff, High risk)

- **What:** `FWeaponLoadoutEntry` embeds a whole `FWeaponInventoryEntry` (`FWeaponLoadoutEntry.h:35`). The owned list (`InventoryComponent.h:88,92`) and `SavedLoadouts` (`:107`) are two copies that drift apart.
- **What it causes:**
  - holder identity by asset pointer (§5.1 #10)
  - dismantled weapons staying equipped
  - wear not syncing back to the owned entry
- **Simplify:** slots store the `PersistentID` FGuid and resolve it through the inventory.
- **Do it together with the save system.** Both need stable IDs.

### 2.5 Infusion — a dead charge subsystem; gameplay state in a VFX component (Medium)

- **Dead subsystem:** `UInfusionChargeManager` (about 700 lines) is only consumed by VFX binds (`InfusionVFXComponent.cpp:57-61`), and `ApplyChargeToAction` has no callers.
- **State in the wrong place:** the live infusion level is stored in `UInfusionVFXComponent::CurrentInfusionLevel`, and the menu reads it to build actions (`CombatCommandMenuSubsystem.cpp:333,1316,1455,1757`). Gameplay state lives in a presentation component.
- **Simplify:** delete ChargeManager and hold the level on the pending action; VFX only displays it.
- *Blocked by B. Check BP use first.*

### 2.6 Stones — four parallel switches over the same 14 stats (Medium payoff, Low risk; not blocked)

- **The four:**
  - `StoneTargetStat` (`CrystalEffectTable.h:640`) leaves out MaxHP/MaxEP, which `GetStoneBasePercent` (`:609`) includes.
  - the `EItemEffectType::Buff*` switch (`ItemIdentity.h:71+`)
  - the name switch (`:222+`)
  - `CrystalDescription.h:171`
- **Plus:** 14 one-line `ItemExecutor::Execute*BuffEffect` wrappers (`ItemExecutor.cpp:521-830`).
- **Simplify:** one row per stone `{Type, Stat, Buff/Debuff, Label}`, with dispatch through `ApplyStoneBuffEffect(Row)`.
- **Keep** the 17 `ECrystalType` values. They are stored in inventory and future save keys.

### 2.7 The same fact stored twice (Medium / Low)

- **Class:**
  - `LoadoutComponent::CharacterClass` (EditAnywhere, defaults to Generic) is copied once from CharacterData. The 44 switches read the copy, while the UI, executor and weather read CharacterData.
  - **Fix:** make it a getter that forwards to CharacterData.
- **Tier:**
  - Every resolver falls back from the instance tier to the asset tier (`ActionExecutor.cpp:441-471`).
  - `IEquipmentGenerator` has one implementer and is never used through the interface.
  - **Fix:** asset tier becomes a template default only; delete the interface.
- **`GetActiveLoadout()`:**
  - Returns the 256-line `FCombatLoadout` **by value**, 25 call sites.
  - **Fix:** return a const ref (low risk).

### 2.8 A third loadout validator (Medium)

- §5.4 listed two. There are three:
  - `LoadoutComponent::GetValidationErrors` (294 lines)
  - `CollectInvalidSlotFindings` (395)
  - `FSavedLoadout::GetValidationErrors` (212)
- **Simplify:** one rule set over a slot view, with two thin adapters (authoring and runtime).

### 2.9 Half-built parallel inventory (Medium)

- **Pool:** `UPoolSubsystem` holds a whole parallel owned-inventory model, inert and reachable only through the `wor.DrawFromPool` console command. It adds a second init path, `InitializeFromPool`.
- **Component spread:** each character carries five item components (Inventory, CrystalInventory, EvolutionInventory, Loadout, Currency). That split is why `EconomyService` repeats its find-components preamble 18 times.
- **Simplify:** freeze Pool until it is wired or dropped. Consider merging Crystal/EvolutionInventory (112 and 63 lines) into InventoryComponent. Medium risk, because BP references these default subobjects.

### 2.10 Small subsystems that can fold away (Low)

- **WeaponManager and RingManager** are stateless passthroughs with 2 callers each. WeaponManager's own comment says to absorb it into LoadoutComponent (`WeaponManager.h:46-50`).
- **InfusionChargeManager:** see §2.5.
- **MerchantShopSubsystem** could be a LocalPlayer subsystem, because it is UI-session state.

### 2.11 Longest functions (worth splitting marked ✂)

| Function | Lines | Nesting | Note |
|---|---|---|---|
| `SkillEffectManager::ApplyEffectLogic` `:1229` | 413 | 5 | Shrinks with §2.1 |
| `LoadoutComponent::CollectInvalidSlotFindings` `:765` | 395 | 5 | §2.8 |
| ✂ `AIDecisionManager::BuildOffensiveAction` `:1331` | 394 | 6 | §2.2 (blocked A) |
| ✂ `ActionExecutor::ApplyCommitCosts` `:6325` | 378 | 5 | §5.3 cost cluster |
| ✂ `EconomyService::Purchase` `:707` | 320 | 5 | Validate / debit / grant |
| `ActionExecutor::ExecuteSpellAsync` `:1075` | 301 | 5 | |
| `ActionExecutor::ApplySkillEffects` `:6063` | 257 | 6 | |
| `DamageCalculator::CalculateDamage` `:28` | 258 | 4 | Linear pipeline — leave |
| ✂ `CombatOrchestrator::ApplyBetweenCombatCrystalDestruction` `:1384` | 190 | **8** | Deepest in the module — early-continue + per-holder helper |

### 2.12 Debug tooling and module

- **Four parallel debug mechanisms, about 10k lines (~12% of the module), none excluded from shipping builds:**
  - 5 test actors (3.6k lines)
  - 38 `Debug/` files
  - 30 console commands
  - 109 CallInEditor buttons
  - about 1.6k lines of inline debug inside the god files
- **Standardise:** console commands + CallInEditor calling `Debug::` functions. Test actors call the same functions instead of duplicating logic.
- **Now:** wrap console commands in `#if !UE_BUILD_SHIPPING`.
- **Later:** a separate developer/debug module. That needs class redirects for placed test actors.
- **Build.cs:** MotionWarping and AIModule can move to private dependencies.

---

## 3. Combined recommendation — what to do, in what order

### Now (cheap, not blocked, high leverage)

1. **Log categories:** `LogWoRCombat`, `LogWoREffects`, `LogWoRAI`, `LogWoRLoadout`, `LogWoRUI`, … Replace `LogTemp` file by file as files are touched.
2. **Defense timing → world time** (`DefenseSystem.cpp:68,212,422`). Trivial.
3. **Per-battle `FRandomStream`** for crits, BD and AI (`DamageCalculator.cpp:230,512`, …).
4. **Stat-modifier table + `GetNetModifier`** (§2.1 step 1).
5. **Stone row table** (§2.6).
6. **Const-ref `GetActiveLoadout`, class getter** (§2.7).
7. `#if !UE_BUILD_SHIPPING` around console commands.
8. **First Automation Spec tests:** DamageCalculator, TierGap/RequirementGap scaling, TurnManager ordering. Pure functions, so tests are cheap to write.

### Decisions recorded (2026-10-25, Crown)

- **CommonUI: YES.**
  - The plugin is **not yet enabled in the committed `.uproject`**. Enable it in the editor (Plugins → Common UI) and commit the `.uproject` change.
  - Then add `CommonUI` and `CommonInput` to `Build.cs` and set the viewport client to `CommonGameViewportClient`.
  - Branch B's widget bases should be reparented to `UCommonActivatableWidget` / `UCommonButtonBase` before they settle.
- **GAS:** see §4.

### Decide now (they shape later work)

- **CommonUI: yes or no** — before branch B's widget bases settle. *Recommendation: yes*, since gamepad is required and the screen list is large.
- **Custom effects vs GAS** — before co-op. *Recommendation: stay custom*, made data-driven (§2.1 step 2), possibly keyed by Gameplay Tags.
- **Battle arena: separate map vs streamed level instance** — before co-op. *No change needed for single-player.*

### After branches A and B merge

- Single `GetLegalActions` query (§2.2), plus the AI enum fix.
- Effect identity split (§2.3).
- Infusion cleanup (§2.5).
- `ResetForCombat()` on per-fight subsystems, then move the per-actor-map ones to world scope (§1.1).
- Delete the `SubmitAction` sync router and the `ApplyDamage` fallback.
- §5.4 ActionExecutor and Orchestrator DRY items and god-file splits.

### With the save system

- Loadouts reference owned gear by `PersistentID` (§2.4).
- Stable IDs, two-phase load.
- Effect-enum collapse (§2.1 step 2).

### Before co-op

- Replicated combat state on GameState or a combat actor.
- Intent RPCs.
- Server-seeded RNG.
- Arena as a streamed level instance.
- `ServerTravel`.

---

## 4. Is it too late for the Gameplay Ability System?

**Not too late, but it would be a rewrite of the combat core, not an add-on.**

### What GAS would replace

| Ours | GAS equivalent | Size today |
|---|---|---|
| `SkillEffectManager` + `FActiveSkillEffect` + 108 effect types | GameplayEffects + GameplayTags | ~4k lines + every effect asset |
| HP/EP/stat getters on `CharacterDataComponent` (Pattern-P composition) | AttributeSets + modifiers | ~1.4k lines, ~20 readers |
| `StatusBuildupManager` bar caps | Attributes + GameplayEffects | ~0.5k |
| `ActionExecutor` skill execution (montage runner, costs, delivery) | GameplayAbilities + AbilityTasks | ~7k lines |
| Item effects / stones (`ItemExecutor`) | GameplayEffects | ~1.2k |

Roughly 15k lines touched, plus migrating every authored effect, spell and ability asset.

### What GAS would give us

- Built-in replication and prediction.
- GameplayTags everywhere.
- GameplayCues for VFX.
- Lyra/community patterns.
- A well-trodden path to co-op.

### What it would cost or not fit

- **Turn-based durations aren't native.** You end up with "infinite effect + turn counter" workarounds.
- **Its biggest strength is real-time client prediction.** A turn-based game barely needs that: server-authoritative "intent → result" replication works without GAS (§1.6).
- **It's a steep learning curve for a solo developer**, and it would stall feature work for weeks.

### Recommendation

**Stay custom, and take the parts of GAS's design that matter:**
1. GameplayTags for elements, statuses and effect families. The GameplayTags module is already a dependency.
2. Data-driven effects (§2.1 step 2) instead of a 108-case switch.
3. Server-authoritative intent RPCs when co-op starts.

That gets most of the benefit at a fraction of the cost.

**Revisit GAS only if co-op ends up needing real-time elements**, e.g. live-action exploration combat in the shared world. That decision belongs before co-op work starts, and moving to tags and data-driven effects first makes a later switch cheaper either way.
