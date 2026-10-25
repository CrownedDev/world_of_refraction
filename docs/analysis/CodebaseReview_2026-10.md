# Codebase Review — 2026-10

Read-only review of `main @ 5fb764f` (Arc 1.5a Party Assembly POC, 2026-07-21). Branch `chore/codebase-review-2026-10`.
Verified against source; docs used only for cross-check. `S/` = `Source/world_of_refraction/`.
Claims marked **✔** were re-read by hand during synthesis; the rest come from per-area subagent passes with file:line.
BP/asset-dependent unknowns are routed to §7, not guessed.

---

## 1. Snapshot

| System | Status | Note |
|---|---|---|
| CombatOrchestrator | Shipped (end-path defects) | `S/Private/Combat/CombatOrchestrator.cpp` 2877 lines, 42% debug (`:1647-2877`) |
| TurnManager | Shipped | Deterministic debt scheduler, total-order tiebreak `TurnManager.cpp:352-385` |
| ActionExecutor (async montage runner) | Shipped | Sync `ExecuteAction` handles Item/Defend only (`ActionExecutor.cpp:735-747`) |
| Deferred / ritual (arm → Execution turn) | Built-unverified | Re-entrancy bug §5.1 #3 |
| DamageCalculator | Shipped | ✔ Element interaction hard-neutral (`DamageCalculator.cpp:207-208`) |
| DefenseSystem | Shipped | Wall-clock timing (`DefenseSystem.cpp:68,212,422`); zero debug tooling |
| CombatGridSubsystem | Shipped | |
| BrokenDarknessManager | Shipped | `RollForBreak` dead (`BrokenDarknessManager.cpp:174`) |
| WeatherStateManager | Shipped | Has own debug (`WeatherStateManager.h:69-75`) |
| SkillEffectManager | Shipped (lane defects) | §5.1 #4–#7 |
| Status bar-cap pipeline (9 elements) | Partial | Fire/Water/Earth/Light/Reality work; Lightning over-blocks; **Wind/Void/BD-Darkness inert** (§3) |
| Defense-outcome conditionals | Built-unverified | `SkillEffectManager.cpp:1718-1868` |
| Threshold / OnTrigger conditionals | Dead in practice | `TriggerCondition` never assigned outside test actor ✔ (`SkillEffectManagerTestActor.cpp:551` only) |
| AIDecisionManager | Partial | Self/enemy only; Easy = random; Emerald dormant (`AIDecisionConstants.h:131`) |
| LoadoutComponent | Shipped | Facade over `InventoryComponent::SavedLoadouts` (`InventoryComponent.h:106-107`) |
| BD-pool Max-sentinel gate | Done | `LoadoutComponent.cpp:1945` bounds on `< None`; BD enum value removed (`ESpellElement.h:28-34`) |
| Augment stones / fusion (weapon) | Shipped | "IsWired six" drift **resolved** — all 12 have read sites (`EquipmentDataBase.cpp:148-155`) |
| Augment stones on rings | Inert | §5.1 #10 |
| CrystalManager wear/break | Shipped | `CrystalManager.cpp:34-403` |
| Per-instance rolls | Built, never reaches combat | U1 refs never written (`FSavedLoadout.h:193-255`) |
| InfusionChargeManager (hold-to-charge) | Dead in C++ | No callers of `BeginCharge/UpdateCharge/CompleteCharge`; PC path "Left unimplemented" (`CombatPlayerController.cpp:147-150`) |
| EquipmentBonusGenerator | Stub (BP-only) | `EquipmentBonusGenerator.h:64-83` |
| CharacterDataComponent | Shipped | Replication declared but inert (§4) |
| Inventory / ItemExecutor | Shipped (result bug) | ✔ `ItemExecutor.cpp:169` |
| Currency component | Shipped, genuinely replicated | ✔ only `SetIsReplicatedByDefault` in Source (`CurrencyComponent.cpp:41`) |
| EconomyService: Purchase | Shipped | Called from `ShopWindowWidget.cpp:332` |
| EconomyService: Dismantle/Merge/LevelUp/Downgrade/BuyWorldStat | Built-unverified | Zero C++ callers — BP-only? |
| Merchant + Shop UI | Shipped (hub, Prisms only) | `MerchantShopSubsystem.cpp:11-68` |
| PoolSubsystem | Stub | Reachable only via `wor.*` console (`PoolSubsystem.cpp:432-510`) |
| Reroll economy | Not built | |
| TrialDoor / TrialRun / BattleGameMode | Shipped (PIE) | Packaged-build risk §5.1 #8 |
| EncounterComponent | Partial | ✔ Single enemy; roster from sphere overlap only (`EncounterComponent.cpp:180-197,246`) |
| `UEncounterData` | Not built | Design only (`docs/Design/EncounterCompositionSystem.md:193`) |
| Party session + recruit | POC — recruits don't fight | §3 |
| BattleConfigComponent back-ref | Dead | `SetBattleContext` no callers (`BattleConfigComponent.cpp:12`) |
| Combat command menu | Shipped | Double-targeting TODO (`CombatCommandMenuSubsystem.cpp:1100`) |
| TurnOrderStrip / CharacterPanel / CommandMenuWidget / ShopWindow | Shipped | All TransBuffer-compliant in C++ |
| DefensePromptWidget | Stub | Every bind/handler `TODO Phase 1` (`DefensePromptWidget.cpp:15-62`) |
| Victory/defeat screen | Missing | ✔ `ExitEncounter` called straight from result (`BattleGameMode.cpp:250-265`) |
| Combat camera | Missing (by design, Sequencer arc banked) | |
| Save system | Missing | 0 `USaveGame`/`SaveGameToSlot`; 88 `SaveGame` tags unused |
| Automated tests | Missing | 0 automation tests/specs; manual `[PASS]/[FAIL]` test actors |

---

## 2. Done (genuinely end-to-end)

- **Combat loop**: `BattleGameMode::BootstrapCombat` (`Trial/BattleGameMode.cpp:31`) → `StartCombat` (`CombatOrchestrator.cpp:134`) → `TurnManager::InitializeCombat` → `HandleTurnStarted` (`:606`) → `ProceedWithTurnStart` DOT/BD (`:627`) → `RequestActionFromActor` (`:992`) → AI (`AIDecisionManager.cpp:147`) or menu (`CombatCommandMenuSubsystem.cpp:1808`) → `SubmitActionAsync` (`:463`) → `ExecuteActionAsync` (`ActionExecutor.cpp:780`) → montage/notify impacts → `ApplyHit` (`:2830`) → `CalculateDamage` (`DamageCalculator.cpp:28`) → finalize → `OnActionCompleted` (`:539`) → win check.
- **Hub → trial → encounter → battle → trial**: door `EnterTrial` (`TrialRunSubsystem.cpp:34-37`), sphere join window, battle, `ExitEncounter`.
- **Skill-effect lifecycle**: apply/stack/merge-by-def-identity (`SkillEffectManager.cpp:81-266`), tick (`:1077`), cleared at combat end (`CombatOrchestrator.cpp:570`). HealBlock, LastStand, lifesteal, reflect/absorb live (`ActionExecutor.cpp:2889-2940`). GuaranteedCrit/IgnoreDefense/DoubleHit are **live**, not stubs (`DamageCalculator.cpp:228,252`; `ActionExecutor.cpp:2988`).
- **Three classes differ mechanically**: Generic dual-weapon (`LoadoutComponent.cpp:2341`), Caster innate spells + budget (`:659-735`) + element gate (`ActionExecutor.cpp:238`), Resonator ring array/switch (`LoadoutComponent.cpp:2145,2949`) + dormant EP without weapon (`CharacterDataComponent.cpp:324,349`); class resistance rows (`ClassInnateResistanceTable.h:116-122`).
- **Gear → stats → damage**: inventory → loadout → per-class tier-scaled aggregation (`LoadoutComponent.cpp:3217-3373`) → Pattern-P getters → damage; crystal wear/break post-cast.
- **Shop purchase**: merchant → `ShopWindowWidget` → `EconomyService::Purchase` with rollback.
- **Turn-order + HUD**: strip and panels built once, refreshed per turn (`TurnOrderStripWidget.cpp:100-119`).

---

## 3. Missing for the pitch demo (ranked by demo impact)

1. **Fight can hard-lock** — stun, silence, unaffordable Easy-AI picks, ritual re-entrancy (§5.1 #1–#3). *Not in Roadmap/TODO.*
2. **No result beat** — level swaps one tick after `OnCombatResultReady`; win = loss (`BattleGameMode.h:11-13`); no reward; enemy respawns. *Roadmap lists persistence, not a results screen.*
3. **Recruits never fight** — battle roster = actors overlapping the arena sphere (`EncounterComponent.cpp:182-197`); nothing spawns party members into the trial; `UParty::GetMemberCharacterData` has no callers (`Party.cpp:7`). Every pawn spawns as `DefaultPlayerPawnClass`, ignoring `FPartyMember.PawnClass` (`BattleGameMode.cpp:133`). Non-leader party pawns become AI (`CombatOrchestrator.cpp:1198-1209`). *Missing from Roadmap — it reads Arc 1.5a as done.*
4. **One enemy only** (`EncounterComponent.cpp:246`) — Encounter Composition Arc 2 (Roadmap "Immediate next" #1–2) is correctly ranked.
5. **3 of 9 element bar-caps do nothing**: Wind `SkipTurn` stored, never enforced (only AI reads it, `AIDecisionManager.cpp:1746,1974`); Void `RandomSkill` log-only (`SkillEffectManager.cpp:1403-1408`); BD-Darkness `DrainEnergy` has no case → `default: return` (`SkillEffectManager.cpp:~2530`). Lightning stun blocks *all* actions incl. Defend (`ActionExecutor.cpp:166-171`) vs enum "Attack/Defend only" (`ESkillEffectType.h:122-123`). *Not in Roadmap/TODO.*
6. **No elemental damage identity** — `ElementMultiplier = 1.0f` always (`DamageCalculator.cpp:207-208`). Elements differ only via status buildup. Pitch-narrative risk; design call (Q5).
7. **Defense input UI is a stub** (`DefensePromptWidget.cpp:15-62`); infusion input on PC unimplemented (`CombatPlayerController.cpp:81,147`). Whether BP covers this is unknown (Q7).
8. **No camera** — Sequencer-per-skill arc banked (Roadmap). View = PC0 pawn wherever the grid places it.
9. **No damage numbers / element feedback in C++** — `OnDamageDealt(Attacker, Target, Damage, bCritical)` carries no element/weakness (`ActionExecutor.h:82`); `FActionResult` totals only (`ActionStructs.h:215-223`).
10. **No Prisms source** — wallet starts at 0 (`CurrencyComponent.h:129-145`); only refunds/essence call `Add` (`CombatOrchestrator.cpp:1470,1517`; `EconomyService.cpp:89,224,401,887`). Shop can't sell unless BP seeds it (Q6).
11. **Content holes already on Roadmap**: Aura spells inert (cast buff element = None except StatusInc/Dec/DOT, `ActionExecutor.cpp:6234-6240`); Cleanse — note `ESkillEffectType::Cleanse` has **no handler** in `ApplyEffectLogic`, and AI's `FindCleanseSpell` seeks it (`AIDecisionManager.cpp:1912`) → AI casts a no-op.
12. **AI variety thin**: offense uses Destruction school only (`AIDecisionManager.cpp:1415,1594`); never casts buffs/debuffs; status scoring ignores element/immunity/resistance (`:806-845`, TODO `:1961`).
13. **Anim dependency**: every demo rig needs a `UCombatAnimInstance` ABP or each action waits for the 10 s timeout (`ActionExecutor.cpp:5094-5104,2164`; `ActionStructs.h:445`) (Q8).

---

## 4. Missing long-term (readiness honest)

| Area | Rating | Evidence |
|---|---|---|
| **Save / persistence** | **0/5** | No `USaveGame` anywhere. `SaveGame`-tagged entry fields sit in containers that aren't tagged (`InventoryComponent.h:87-92,106`). `PersistentID` re-minted every spawn (`InventoryComponent.cpp:305,441`). Effect `DefID = GetUniqueID()` session-local (`EquipmentDataBase.cpp:282`). Wallet/inventory/loadout on actor components die on every `OpenLevel`. WSP pool and BD strain also reset (`BrokenDarknessManager.cpp:108`). |
| **Networking / replication** | **1/5** | Roadmap's "replication-aware from the ground up" (`docs/Gaps/Roadmap.md:45`) is overstated. Real: `UCurrencyComponent` (FastArray + OnRep). `UCharacterDataComponent` has `DOREPLIFETIME` (`CharacterDataComponent.cpp:140-148`, omits MaxHP/MaxEP) but **never `SetIsReplicatedByDefault`** (ctor `:19-28`). 0 Server/Client/NetMulticast RPCs; `Server*` names are plain `BlueprintCallable` (`CharacterDataComponent.h:182-202`). All combat authority lives in `UGameInstanceSubsystem`s (per-process, unreplicable) — e.g. effect state `TMap<TWeakObjectPtr<AActor>,…>` (`SkillEffectManager.h:419`); `InfusionChargeManager` holds one global charging actor. Non-seamless `OpenLevel`. Global `FRand` for crits/BD/AI (`DamageCalculator.cpp:230,512`) — no seeded determinism. |
| **Account vs character scope** | 1/5 | Pool is account-flat (`PoolSubsystem.h:134-147`); `InitializeFromPool` copies the whole pool into one pawn (`InventoryComponent.cpp:1446-1503`) — 3 members would get duplicate PersistentIDs. Prisms/Diamond routing TODO (`CurrencyComponent.h:21-23`). |
| **Camera** | 0/5 (intentional greenfield) | Deleted 2026-07-20; Sequencer design banked. |
| **Cooperative AI** | 0/5 | Every AI action targets self or `GetLivingEnemies` (`AIDecisionManager.cpp:183,1343`). No ally enumeration; `GetDeadAllies` absent. Team queries exist on TurnManager to build on (used at `SkillEffectManager.cpp:1551,2295`). |
| **Testing** | 0/5 | 0 automation tests; test actors (~3.6k lines) ship in Runtime with no `WITH_EDITOR`/shipping guard. |

Biggest structural takeaway for networking: the "service in a GameInstanceSubsystem" rule in CLAUDE.md is the main obstacle — authoritative per-actor state (effects, buildup, infusion charge) will need to move onto replicated actor components or a replicated GameState. Worth a design note before more state lands in subsystems.

---

## 5. Improvements

### 5.1 Bugs / correctness risks (highest first)

1. **✔ Turn hard-lock on invalid action.** `SubmitActionAsync` is `void` (`CombatOrchestrator.h:165`); clears auto-advance (`CombatOrchestrator.cpp:483`) then returns on failed validation (`:486-490`). AI caller (`AIDecisionManager.cpp:147`) can't recover. Triggers: stunned actor (`ValidateAction` rejects everything, `ActionExecutor.cpp:167-171`; AI `BuildAction` never checks stun); silenced AI choosing a spell (`AIDecisionManager.cpp:1067-1088,1594`); Easy AI with no affordability check (`:196-227`). Stun expires at end-of-turn, which never comes.
2. **✔ Normal win skips half the teardown.** `UnbindTurnManagerEvents()` (`CombatOrchestrator.cpp:565`) runs before `TurnManagerRef->EndCombat()` (`:581`), so `HandleCombatEnded` never fires → `OnWorldStatDraftReady` never fires on a real win (`:764-767`), per-actor `OnDied` unbind skipped (`:771-785`), plus no weather `EndCombat`, grid clear, status-bar reset (bars carry into next fight), `ResetBattleState`, AI/menu deregistration — only `ForceEndCombat` (`:252-341`) does those. Three teardowns disagree (`:252`, `:562`, `:743`).
3. **✔ Ritual-arm callback wiped by re-entrancy.** `FinishArmTurn` does `AsyncActionCallback.Execute(...)` then `.Unbind()` (`ActionExecutor.cpp:5523-5526`). If the advance fires a ready Execution turn, its fresh callback is bound inside `Execute` and then unbound → `bWaitingForAsyncAction` stuck true. `CompleteAsyncActionFinal` already guards this (`:2343-2350`) — copy that guard.
4. **✔ Array mutated inside range-for.** `ProcessEffectsWithTiming` range-for (`SkillEffectManager.cpp:1065-1074`) → DOT with `BuildupPerTick` (Garnet, `ItemExecutor.cpp:277`) → bar-cap → `ApplyEffect` on same actor → `Effects.Add` (`:225`) → invalidated reference / ensure. Same via Cleanse/Remove*Debuff `RemoveAt` (`:1367-1377,1539`).
5. **Conditional effects inert or always-on.** ✔ `TriggerCondition` only ever assigned in the test actor; `ProcessTriggerEffects` requires it (`SkillEffectManager.cpp:1025`), so promoted OnTrigger effects (`ActiveSkillEffect.h:336-342`) never fire. Conversely `GetTotalStatModifier`/`HasEffectOfType` ignore `bTriggerActive` (`:1940-1958`) → conditional stat buffs always on.
6. **Fires-once per match is per EffectID, not per actor** ✔ (`SkillEffectManager.h:423`, check `:100`) — two enemies with the same gear: second is rejected. Instants return at `:122` before the record at `:263` → "once" instants fire every time.
7. **Effect-ID collisions.** `FindEffectByID` matches ID only (`:2307-2322`). Bar-cap IDs use `FMath::Rand()` (`:2405`, 0..32767 on Windows) → can collide with packed IDs, and repeated bar-caps stack instead of refresh. Garnet Burn ID = `GetTypeHash(Fire)^DOT = 0` (`ItemExecutor.cpp:274`) collides with `PackEffectID(0,…)`.
8. **✔ TrialRun weak pointer — packaged-build abort risk.** `TWeakObjectPtr<UTrialData> ActiveTrial` (`TrialRunSubsystem.h:103`); the comment's "asset-registry owned" is wrong — the registry doesn't root UObjects. Only hard ref is the hub door (`TrialDoor.h:38-39`). After hub unload GC may null it → encounter aborts "no active trial" (`EncounterComponent.cpp:228-237`). Likely masked in PIE. One-line fix: `UPROPERTY() TObjectPtr`.
9. **✔ Failed item use reports success.** `Result.bSuccess = true` unconditionally (`ItemExecutor.cpp:169`), overriding handler failures (`:217,236`); BD bonus applied and loadout charge consumed (`ActionExecutor.cpp:2737-2741`).
10. **✔ Holder identity = asset pointer.** `FindAttachedItemByHolder` returns first slot whose `Weapon/Ring` asset matches (`LoadoutComponent.cpp:2826-2849`); ownership validation also by asset (`:498-512,590-625`). Duplicate weapon/ring → wear and post-battle break/repair land on slot 1 twice (`CombatOrchestrator.cpp:1416,1444,1617`). This is the concrete form of TODO's "double-reference hazard".
11. **Gear effects frozen at combat start** (`CombatOrchestrator.cpp:1163-1170`). `ToggleEquipment`/`SetActiveRingIndex` (`LoadoutComponent.cpp:2341,2949`) are called mid-combat (`CombatCommandMenuSubsystem.cpp:887,915`) but only recompute pools/speed → old gear's effects persist, new gear's never apply. Hits the Resonator showcase.
12. **Ring stones/fusions inert.** All stone reads go through `GetActiveWeaponAttachment` (`CharacterDataComponent.cpp:738-741,929-931`; `TurnManager.cpp:241`; `StatusBuildupManager.cpp:243`), yet `RingData.cpp:40-44` allows ring fusions.
13. **Cleanse strips own gear.** On-hit `ApplyBurn/Chill/StunToTarget` classed as debuffs (`ESkillEffectType.h:391-393`) → `CleanseSelf` removes the attacker's gear on-hits; Iolite strips an enemy's permanent gear buffs (`ItemExecutor.cpp:1029-1041`); nothing re-applies.
14. **Double turn-advance** for a montage-less ritual Ability: sync branch rejects after commit costs (`ActionExecutor.cpp:722-728`), `OnActionCompleted` advances (`CombatOrchestrator.cpp:458`) and `FireScheduledExecution` advances again (`:729-734`).
15. **First turn fires before HUD/InProgress** — `InitializeCombat` (`:246`) dispatches turn 1 before `OnCombatStartedUI` (`:248`) and `SetCombatState(InProgress)` (`:250`). Death at turn-1 start → `OnActionCompleted` bails "not in progress" (`:547`). Also contradicts the CLAUDE.md widget-lifecycle gotcha intent.
16. **`StartCombat` re-entry** registers AI/menu (`:140-152`) then `ForceEndCombat` clears them (`:154-158,321-337`).
17. **Corpse animation** — `PlayReturnStep` (`ActionExecutor.cpp:5468-5503`) and facing reassert (`:2326`) lack alive checks (TODO's corpse-walk-back, relocated; `SignalActionComplete` no longer exists).
18. **Anim-wait hang**: Ritual/Return montage set but no `SkillMontage` → wait flag set, `PlaySkillAnimation` returns early (`:4000-4004`); `FinishMontageChain` with null actor skips unbind (`:5562-5566`).
19. **Smaller**: TurnManager derefs `Combatant.Actor` unchecked (`TurnManager.cpp:253,578,594,608`); DoubleHit second hit not added to `DamageDealt` (`ActionExecutor.cpp:2989-3004`); `GI` deref outside null check (`CombatOrchestrator.cpp:89`); raw `this` in timer lambdas (`:100`; `ActionExecutor.cpp:5079`); Defend validates/broadcasts/BD-checks twice (`:717/854,722/861`); AI MaxHP/MaxEP divide (`AIDecisionManager.cpp:1047-1050`); recruit duplicate via CDO-vs-instance CharacterData (`PartySessionSubsystem.cpp:143-176,233-247` vs `PartyInteractionComponent.cpp:81-91`); currency `+=` no clamp (`CurrencyComponent.cpp:74-99`); dismantle ignores equipped (`EconomyService.cpp:250-300`) and owned↔loadout copies diverge; shop soft-lock if widget removed externally (`MerchantShopSubsystem.h:41`); door/merchant 1e9-s hints, no `EndPlay` (`TrialDoor.cpp:144`, `MerchantInteractable.cpp:128`); **Generic can cast weapon-crystal spells** ✔ (`UI/Combat/FCombatCapabilities.cpp:62-68`) vs `CanUseSpells()==false` (Q9); Resonator "≤3 same element" unenforced, "6 slots" comment vs budget 5 (`ECharacterClass.h:24`, `LoadoutConstants.h:31`); legacy pillar PostLoad re-applies whenever new field is 0 (`EvolutionItemData.cpp:406-417`); Healing/Durability stones count as `IsStatStone` → inert HealingStone+HealingStone fusion (`CrystalTypeHelpers.h:54-103`).

Checked clean: no static mutable state, no cached subsystem pointers across PIE, no gameplay `Tick` (`SkillProjectile::Tick` is flight), no UI Tick, UI widgets compliant with the TransBuffer rule in C++, BattleGameMode/ShopWindow delegate unbinds paired.

### 5.2 Dead code + stale TODOs (zero C++ callers; "BP?" = BlueprintCallable, BP use unknown)

- `UDamageCalculator::CalculateAttackDamage` (`DamageCalculator.cpp:287`) — BP?
- `SubmitAction` hybrid router still live (`CombatOrchestrator.cpp:391-437`); production uses async only; callers = `FireScheduledExecution` (`:729`), `DebugExecuteTestAction` (`:1829`), 7 test-actor sites. Roadmap's `:741` ref is stale.
- `CanUseAbility/CanUseSpell` (`ActionExecutor.h:668-669`), `GetUniqueEffectID` (`.cpp:5911`), `RollForBreak` (`BrokenDarknessManager.cpp:174`).
- `ApplyDamage`/`ProcessMultiHit` only when DefenseSystem null (`ActionExecutor.cpp:1660,3575,3680,3947`).
- `CreateFromSkillEffect`, `CreatePersistent` (`ActiveSkillEffect.h:409,584`); `NextInstanceID/GenerateInstanceID` (`SkillEffectManager.h:432-435`); `ESkillEffectTiming::Instant`.
- BP?: `ApplyInfusionDOT`, `ApplyWeaponInfusionDOT`, `ApplyPhysicalDamageEffect`, `RemoveAll*`, `RemoveEffectsBy*`, `GetEffectsSummary` (`SkillEffectManager.h:126-171`). Note `RemoveAllEffects` unused ⇒ dead actors keep effects.
- Enum values with no runtime handler: `Heal`, `Cleanse`, `RetaliationDamage`, `SkipTurn`, `RandomSkill`, `Speed{Buff,Debuff}`, `SpellSize{Buff,Debuff}`, `ModifyHealing`. Unreachable legacy branches in `ApplyTriggeredSkillEffect` (`:2447-2490`).
- `CharacterClassHelpers` namespace (`ECharacterClass.h:40-104`); `UCharacterData::CanUseSpells/CanUseAbilities/UsesRings/CanDualWield` (`CharacterData.h:260-273`) — class gating instead lives in 44 switches in `LoadoutComponent.cpp`.
- `GetActiveLoadoutRef` (copies despite name, `LoadoutComponent.cpp:76`); `HasRingInSecondary` always false (`:2483-2509`); `GetCombinedStatBonus/Resistance` (`EquipmentDataBase.cpp:315,338`); `CalculateRequirementPenalty` (`SkillDataBase.cpp:251`, comment at `DamageCalculator.cpp:321-323` wrongly says BD uses it); `FWorldStatRequirements::GetMinimumRank/GetSimpleString` (`WorldStatRequirements.h:56,62`).
- Infusion hold-to-charge (`InfusionChargeManager` Begin/Update/Complete) — only VFX binds delegates (`InfusionVFXComponent.cpp:57-61`).
- Economy/hub: `EItemEffectType::Healing` (`ItemEffectType.h:20`), `UParty::GetMemberCharacterData`, `SetDisplayName`, `UBattleConfigComponent::SetBattleContext/SetGridPosition`, `UMerchantData::AppearsInHub/AppearsInTrial` (`MerchantData.h:102-109`), `OnItemUsed` (no C++ listener).
- **TODO.md items now stale**: `CreateFromSpellEffect` clamp (function deleted, `ActiveSkillEffect.h:238`); `ExecuteHealingEffect` rename (done → `ExecuteHealingStoneEffect`, `ItemExecutor.cpp:345`); fusion "IsWired six" drift (resolved); RequirementGapScaling "pending merge" (merged, `docs/Design/Completed/RequirementGapScaling.md:3`); `feature/starting-effects` "in flight"; SessionDoc 2026-06-20 lists GuaranteedCrit/IgnoreDefense/DoubleHit/Revive as stubs (live/renamed). `FWorldStatRequirements` "double header" — only one header exists (`Skills/Definitions/WorldStatRequirements.h`); clarify or drop.
- Source TODOs: 49 total (Combat 20, UI 8). Notable: spell items never consumed (`ActionExecutor.cpp:2816`), defense window 0.3 hardcoded (`:1296`, orphan window w/ null attacker `:3943-3946`), ally targeting assumes self (`:6036`), crystal `StatBonus` not applied (`:6773`), deprecated crystal maps in save path (`InventoryData.h:78`), placeholder quality on shop rolls (`FWeaponInventoryEntry.cpp:47`).
- Magic numbers vs CLAUDE.md: `SkillEffectManager.cpp:1685,2420,2436,2524`.

### 5.3 God-file breakup (only where it pays)

- **CombatOrchestrator.cpp** → move debug block `:1647-2877` to `Debug/Combat/CombatOrchestratorDebug.cpp` (also satisfies debug rule; −42%). Optionally settlement `:1384-1645` → loadout/economy helper. 2 clusters.
- **ActionExecutor.cpp** (pays off — 6.9k): montage runner `:4866-5908`; cost resolution (`:313-417` + `:4636-4800` + `ApplyCommitCosts :6325-6742`, shared by AI/UI); infusion source/element `:4081-4470`; spell delivery spawners `:3217-3990`. Do montage runner + cost first. ~4 clusters.
- **SkillEffectManager.cpp**: console/debug `:333-515` → Debug/; defense-trigger handler `:1718-1868` → own file; bar-cap factory `:2392-2541` beside `BarCapTriggerResolver`. Leave `ApplyEffectLogic`.
- **AIDecisionManager.cpp**: estimator `:580-946` → `AIDamageEstimator` (dedupes, see 5.4).
- **LoadoutComponent.cpp**: validation `:425-1332` and aggregation `:3215-3767` only.
- **EconomyService.cpp**: not a split — extract the ~18× repeated null/authority/find-components preamble (`e.g. :1177-1199`).
- Not worth it: ItemExecutor (already consolidated), UI files.

### 5.4 Duplication (DRY)

- Three combat-end teardowns (`CombatOrchestrator.cpp:252,562,743`) → one `TeardownCombat(bCompleted)`. Fixes §5.1 #2 at the same time.
- `SubmitAction` vs `SubmitActionAsync` guards/validation (`:347-386` vs `:463-502`).
- `ApplyHit` vs `ApplyDamage` (`ActionExecutor.cpp:2830` vs `:3022`); three per-impact resolve blocks (`:3530-3570,3650-3685,3890-3940`); three reflect blocks (`:2896-2948`).
- AI `EstimateSpellDamage` vs `EstimateAbilityDamage` near-copies (`AIDecisionManager.cpp:630-714` vs `:716-791`) **and drifted from execution**: spell estimate uses `Spell->Element` (`:675`) vs exec `ResolveSpellCastElement` (`ActionExecutor.cpp:1185`); ability estimate uses None (`:752`) vs exec InnateElement (`:1518-1534`).
- Heal-amplify formula ×4 (`SkillEffectManager.cpp:1298,1447,1672`; `ActionExecutor.cpp:6211`).
- Per-class "which gear contributes" switch ×6 (`LoadoutComponent.cpp:3268,3388,3494,3581,3661`; `SkillEffectManager.cpp:442-497`) → one gear visitor.
- Two parallel loadout validators (`LoadoutComponent.cpp:470-764` vs `765-1160`).
- Inventory Weapon/Ring attach/remove near-duplicates (`InventoryComponent.cpp:503-1065`).
- `GetBrokenDarknessManager` ×3 (Orchestrator `:1247`, Executor `:4472`, Calculator `:571`); `HasServerAuthority` ×2 + raw `HasAuthority` (`CurrencyComponent.cpp:231`, `CharacterDataComponent.cpp:412`); `FindNearestInRange`+hint code ×3 (`TrialDoor.cpp:104`, `MerchantInteractable.cpp:88`, `PartyInteractionComponent.cpp:117`).
- `UTurnManager::GetTeamMembers/GetActorTeam` defined in `CombatOrchestrator.cpp:1345-1382` (misplaced).

### 5.5 Debug-tool gaps vs CLAUDE.md rule

- **No debug tooling at all**: AIDecisionManager, DefenseSystem, StatusBuildupManager, MerchantShopSubsystem, PartySessionSubsystem/UParty, RingManager, WeaponManager, ItemExecutor, EncounterComponent, BattleConfigComponent, CombatCharacter, TrialGameMode, BattleGameMode, TrialDoor.
- **Inline-only, no `<System>Debug` pair**: TurnManager, ActionExecutor, CombatGrid, CrystalManager (`CrystalManager.cpp:641-835`), EconomyService, Pool, SkillEffectManager (`:2099-2177`), BrokenDarkness, Loadout (`LoadoutComponent.cpp:2996,3023`), Orchestrator, TrialRun, InfusionCharge.
- **Misplaced**: `CurrencyComponentDebug` (`Public/Currency/`), `ElementColorDebugComponent` (`Public/Infusion/`).
- **Zero `Compare*` functions** project-wide.
- **8 of 11 data-asset classes lack `CallInEditor` Print**: CharacterData, StanceData, CosmeticsData, TrialData, InventoryData, InfusionDisplayData, Skill/Spell/AbilityData, (Weapon/Ring inherit roll buttons only). `WeaponDataDebug`, `CharacterDataDebug`, `EquipmentRollDebug` exist but have no callers/buttons.
- **Hygiene**: all 1794 `UE_LOG` on `LogTemp`, 0 custom categories; 22 `FAutoConsoleCommand`s mixed `wor.`/`WoR.` (`BrokenDarknessManager.cpp:1314-1327`), none behind `!UE_BUILD_SHIPPING`; test actors (~3.6k lines) unguarded in Runtime; Warning-level spam (`CombatActionMenuBase.cpp:18`, `TrialDoor.cpp:61`).
- Highest value adds for the demo: `AIDecisionDebug` (decision trace console cmd) and `StatusBuildupDebug` (bars per actor) — those are the two systems the soft-lock and element bugs live in.

### 5.6 Doc drift (doc says X → source says Y)

- `Roadmap.md:45` "replication-aware from the ground up" → only Currency replicates (§4). Same in `CurrencySystem.md:10`.
- `Roadmap.md` SubmitAction at `:741` → router at `CombatOrchestrator.cpp:391-437`.
- `TurnManager.md:218-220,249-252`, `CombatOrchestrator.md:275,308` → `FOnTurnEnded/OnTurnEnded/HandleTurnEnded/EndCurrentTurn` don't exist. TurnManager.md omits Execution turns.
- `CombatOrchestrator.md:94` `ShouldUseAI()` → `IsPlayerControlled` (`:1209`); `:108` AOE/Instant sync → async (`:391-415`); `:69` `ApplyEvolutionEffects` → `ArmConditionalEffects`+`ApplyEquipmentEffects`; `:213` ForceEnd ends camera → camera gone.
- `DamageCalculator.md:50,65` defense [0,0.5], BonusDefense deferred → clamp 0.85, BonusDefense applied (`DamageCalculator.cpp:436,460`; `CombatConstants.h:208`); `:68,111` `GetElementInteractionMultiplier/IsWeakTo` → don't exist.
- `BrokenDarkness.md:620` strain resets at combat start → only in `BeginPlay`.
- `SkillEffectSystem.md:110,137,170` cite deleted `CreateFromSpellEffect`; `:197,252` stubs → live; `:191` Stun blocks non-Attack/Defend → blocks all.
- `AISystem.md:83` SubmitAction → SubmitActionAsync; `:46` "stun (SkipTurn)".
- `StatusBuildupSystem.md:78` reset to Generic → None (`StatusBuildupManager.cpp:496`).
- `LoadoutSystem.md:263-265` `PrepareForBattle` calls `ResetForBattle()` / fail leaves flag false → no such fn; soft-fail always sets `bIsReadyForBattle=true` (`LoadoutComponent.cpp:1351-1371`), ignores `Inventory` param.
- `UISystem.md:120-127` DefensePrompt binds DefenseSystem; `:280` "no literal TODO" → stub with 7 TODOs. Doc covers 3 widgets; 5 undocumented (DurabilityHeader, CommandMenuWidget, CommandButtonWidget, PieMenuButtonData, CombatActionMenuBase).
- `WeaponSystem.md:164` `ApproachData (UMovementData*)` → retired (`CharacterData.h:550`).
- `InfusionSystem.md:5-7` hold-to-charge live → dead in C++; `CycleInfusionLevel` is the path.
- `CharacterDataSystem.md:59-60` `GetDisplayElement` returns BD → returns `GetElement()` (`CharacterDataComponent.cpp:466-472`).
- `PartySystem.md` "solo-of-one only" stale; omits that recruits don't reach battle; claims BattleConfigComponent back-ref set → never set.
- `INDEX.md:3` "3 .json" → 0; several Upd dates behind.
- **CLAUDE.md**: lists 16 Architecture docs, 27 exist (11 missing). Gotcha "`InnateElement` defaults to Generic = no innate ability" → Generic now means *inherit* (`ESpellElement.h:28-31`), `CharacterData.h:128,279` still default/return Generic — reconcile the rule.
- Source comments: `TrialRunSubsystem.h:101` "asset-registry owned" (wrong); `CombatPlayerController.cpp:136-138` "possesses nothing"; `AIDecisionManager.cpp:1129` "30%" vs `0.50`; `CrystalType.h:20` Sapphire "Healing"; `CrystalTypeHelpers.h:48-50` 7 stones vs 17; `FWeaponInventoryEntry.h:62-69` "unread" Tier/Quality (read at `LoadoutComponent.cpp:3263`); `SkillEffectManager.cpp:1528-1535` "pending" hook is live.

Config hygiene (FYI): `Config/DefaultEngine.ini:4` commits an AndroidFileServer `SecurityToken`; `:24` `bCheckForLevelGCBeforePIE=false` can mask widget leaks; ~100 CoreRedirects incl. chains. `.uproject` enables `ModelContextProtocol`/`AllToolsets` for non-editor targets. 0 tracked non-LFS binaries (LFS gap is latent, not live). `S/.vscode/c_cpp_properties.json` committed.

---

## 6. Recommended next 3 arcs

### Arc A — "Fight never hangs" (combat stability)
- **Why**: every demo-run soft-lock path (§5.1 #1–#4, #14–#16) sits in 3 files. A publisher demo that freezes once is worse than a missing feature.
- **Clusters (≤3 files each)**:
  1. Invalid-action fallback: `SubmitActionAsync` → on validation fail, pass turn (or re-request for player) + stunned actors auto-pass at turn start. (`CombatOrchestrator.cpp/.h`, `ActionExecutor.cpp`)
  2. AI `BuildAction` filters by `ValidateAction` (stun/silence/EP) before submitting. (`AIDecisionManager.cpp`)
  3. Single `TeardownCombat` used by all three end paths; restores WSP draft, OnDied unbind, bar reset, weather end. (`CombatOrchestrator.cpp/.h`)
  4. Ritual callback guard (copy `:2343-2350`) + `ProcessEffectsWithTiming` index loop / deferred-apply. (`ActionExecutor.cpp`, `SkillEffectManager.cpp`)
  5. `AIDecisionDebug` + `StatusBuildupDebug` pairs (proves 1–4 without trial-and-error PIE).
- **Risk**: Medium — touches the turn loop. Mitigate with the existing `CombatOrchestratorTestActor` and new debug cmds. **~5 clusters.**

### Arc B — "Demo loop that reads" (result beat + party in battle)
- **Why**: right now the pitch shows a 1v1 that ends with a hard cut. Party Assembly POC exists but stops at the hub.
- **Clusters**:
  1. Results hold: `BattleGameMode::HandleCombatEnd` → broadcast for a BP results widget, then `ExitEncounter` on a confirm/timer; split Victory vs Defeat return. (`BattleGameMode.cpp/.h`)
  2. Party → battle roster: build `PendingBattleParty` from `UPartySessionSubsystem` at encounter start (not only sphere overlap) and spawn `FPartyMember.PawnClass`. (`EncounterComponent.cpp`, `BattleGameMode.cpp`, `Party.cpp`)
  3. `TrialRunSubsystem::ActiveTrial` → strong ref; Prisms reward on win (source for the shop). (`TrialRunSubsystem.h`, `BattleGameMode.cpp`, `CurrencyComponent` call only)
  4–6. Encounter Composition Arc 2 (`UEncounterData`, multi-enemy, AI formation) — already Roadmap "Immediate next" #1–2; slot it here.
- **Risk**: Medium — BP coupling (results widget, `OnCombatStartedUI`, possession of non-leader members — Q3). **~6 clusters.**

### Arc C — "Nine elements, nine identities"
- **Why**: the pitch is the 9-element system; 3 of 9 bar-caps do nothing, Lightning over-blocks, damage is element-neutral, and there's no element feedback to the player.
- **Clusters**:
  1. Wind `SkipTurn` enforcement at turn start (shares Arc A's auto-pass), Lightning semantics per Q1. (`CombatOrchestrator.cpp`, `ActionExecutor.cpp`)
  2. BD-Darkness `DrainEnergy` case + Void `RandomSkill` minimal implementation in `ApplyTriggeredSkillEffect` (no BD-state migration — just the missing switch cases). (`SkillEffectManager.cpp`, `BarCapTriggerResolver`)
  3. Aura element-inheritance + Cleanse handler (Roadmap Design Gaps). (`ActionExecutor.cpp`, `SkillEffectManager.cpp`)
  4. `OnDamageDealt` carries element (+ weakness if Q5 says yes) for BP damage numbers. (`ActionExecutor.h/.cpp`, `DamageCalculator.cpp`)
- **Risk**: Low–Medium; mostly additive switch cases. Depends on Q1/Q4/Q5. **~4 clusters.**

Deliberately not next: save/persistence keystone and replication refactor — both large, both blocked on design (account scope; subsystem→component state move). Bank a one-page design note for state ownership before more authoritative state lands in subsystems.

---

## 7. Questions for Crown (ranked, one-line answers)

1. **Stun**: block everything and auto-pass the turn, or allow Attack/Defend as `ESkillEffectType.h:122` says?
2. **Results**: does any BP show victory/defeat before `ExitEncounter` (≈1 tick), and should demo win ≠ loss?
3. **Party in battle**: should recruits be injected from the party list at encounter start (not sphere overlap), using `FPartyMember.PawnClass`, and are non-leaders meant to be AI companions?
4. **Demo scope**: which elements + enemies does the vertical-slice fight actually use?
5. **Element identity**: should damage have elemental weakness/resist, or stay buildup-only (`DamageCalculator.cpp:207`)?
6. **Prisms**: does any BP seed Prisms/Gold today (how did `CurrencySystem.md:80` get 120)?
7. **Defense input**: is defense prompting handled in BP, or is the stub `DefensePromptWidget` the only UI?
8. **Anim BPs**: do all demo character ABPs derive from `UCombatAnimInstance`?
9. **Generic class**: is Generic meant to cast weapon-crystal spells (`FCombatCapabilities.cpp:62-68`)?
10. **Mid-combat swap**: should weapon toggle / ring switch re-gather gear effects, or are effects locked at combat start by design?

---

## 8. Recommendation — all gameplay logic in C++ (added 2026-10-25, Crown-approved direction)

**Rule (proposed for CLAUDE.md, not yet edited):** C++ owns every flow, rule and decision. Blueprints/assets carry only **layout, visuals, content references and tuning values** — zero graph logic. A `BlueprintImplementableEvent` is allowed only for purely cosmetic reactions (animate a row, play a sound), never to build, wire, or decide anything.

**Why**: every BP-dependent question in §7 becomes either a C++ task or disappears; behaviour becomes diffable/reviewable; no more silent BP override detaching on C++ rename (Roadmap: `OnCombatStartedUI` rename broke the HUD with no error); no `.uasset` merge risk for logic.

**What stays as assets (not logic)**: WBP layouts (bound via `BindWidget`, 43 sites), one minimal ABP per rig (idle + montage slot — engine needs it for montages; zero logic), character BP subclasses *only* as mesh/material/VFX config, data assets, maps, materials, Niagara, input action assets.

**Pushed back on**: building UI layout in pure C++/Slate — loses the visual designer and locks out the planned artist hire. Not before the demo.

---

## 9. Blueprint-logic audit — pass 1 (source side)

Method: scanned headers for the C++→BP seams. Script: `bpscan.py` (scratchpad, not committed). Conservative — a name mentioned in a comment counts as a "caller", so zero-caller lists are lower bounds.

### 9.1 Where C++ hands control to BP (`BlueprintImplementableEvent`/`NativeEvent`)

| Hook | Fired from | What BP is expected to do | Logic or cosmetic? | Action |
|---|---|---|---|---|
| `ACombatOrchestrator::OnCombatStartedUI` (`Combat/CombatOrchestrator.h:141`) | `CombatOrchestrator.cpp:247` | **Build the entire combat HUD** (comment: "override in BP to create HUD") | **Logic — the big one** | Move to C++ (see 9.3) |
| `UCharacterPanelWidget::RebuildEffectsList` (`UI/Combat/CharacterPanelWidget.h:155`) | `CharacterPanelWidget.cpp:363,402` | Fill `EffectsList` with `WBP_EffectRow`s on every change | Logic + **TransBuffer risk** if it creates rows each call | Move to C++ with a row pool (same pattern as `TurnOrderStripWidget.cpp:107-119`) |
| `UCharacterPanelWidget::ApplyStaticText` (NativeEvent, `:162`) | `CharacterPanelWidget.cpp:108` | Optional format override; C++ fallback fills NameText only | Cosmetic-ish | Make C++ fill all static text; BP override optional |
| `UCombatCommandButtonWidget::OnDataChanged` (`UI/Combat/CommandMenu/CombatCommandButtonWidget.h:52`) | `CombatCommandButtonWidget.cpp:59` | Push label/icon/colour into the button | Cosmetic | Keep, or set bound text/image in C++ |
| `UCombatCommandMenuWidget::OnButtonsRefreshed` (`CombatCommandMenuWidget.h:63`) | `CombatCommandMenuWidget.cpp:143,162` | Layout/animate active buttons | Cosmetic | Keep |
| `UTurnOrderSlotWidget::InitialiseSlot` (`UI/Combat/TurnOrderSlotWidget.h:40`) | `TurnOrderStripWidget.cpp:137,151` | Show portrait/name/active/bonus state | Data → display | Move data binding to C++; keep BP for anim |
| `UShopRowWidget::OnEntrySet` (`UI/Shop/ShopRowWidget.h:93`) | `ShopRowWidget.cpp:43` | Populate a row | Data → display | Move to C++ |
| `UDefensePromptWidget::ShowPrompt/HidePrompt` (`UI/Combat/DefensePromptWidget.h:47,50`) | **never** (`DefensePromptWidget.cpp:57,62` are TODO comments) | — | Unbuilt | Build in C++ |

### 9.2 Production flows with **no C++ entry point** (only BP can be driving them)

- **Combat HUD assembly.** No production C++ creates or initialises any combat widget: `CreateWidget`/`AddToViewport` for panels/strip appear only in `Testing/HUDTestActor.cpp:52-121`; `UCharacterPanelWidget::InitialiseForActor` and `UTurnOrderStripWidget/UCombatCommandMenuWidget::InitialiseForCombat` have no production C++ callers. ⇒ `BP_CombatOrchestrator`'s `OnCombatStartedUI` (or a container WBP) does it all.
- **Hub interact input (E).** `ATrialDoor/AMerchantInteractable/UPartyInteractionComponent::Interact()` have zero C++ callers; there is no C++ hub PlayerController (`CombatPlayerController` is the only `APlayerController` subclass). ⇒ lives in `Core/Hub/BP_HubPlayerController` (the "IA_Interact party branch" commit `4939128`). `UPartySessionSubsystem::EnsureLocalParty` likewise has no C++ caller.
- **Shop window content**: C++ creates it (`MerchantShopSubsystem.cpp:48-60`) — fine.
- **Currency seeding**: nothing in C++ grants Prisms (§3 #10) — either a BP does, or nothing does.

### 9.3 BP-callable surface with zero C++ callers

- **812** `BlueprintCallable/Pure` functions (excluding `CallInEditor`); **253** have no C++ caller — by area: Debug 49, Combat 40, Infusion 24, Equipment 23, Currency 22, Loadout 21, Character 21, Skills 16, UI 15, Inventory 13, other 9.
- These are either (a) called from a BP graph (= hidden logic), or (b) dead. The ones that matter — gameplay *mutators*, not getters:
  - **Economy**: `EconomyService` `DismantleCrystal/MergeCrystals/DismantleWeapon/DismantleRing/DismantleSpell/DismantleAbility/RemovePrimaryEvolution/LevelUp*/Downgrade*` (`Currency/EconomyService.h:67-279`); `CurrencyComponent::SpendGearEssence/SpendSkillEssence` (`:87,98`).
  - **Inventory/loadout editing**: `InventoryComponent::Attach*/Remove*` for weapons and rings (`Inventory/InventoryComponent.h:203-308`), `AddEvolutionInstance` (`:354`); `LoadoutComponent::SetActiveLoadoutIndex/CreateAndConfigureLoadout/DeleteLoadout/DuplicateLoadout/RenameLoadout/ClearLoadout/ApplyWearToCrystalEntryByHolder` (`Loadout/LoadoutComponent.h:171-651`).
  - **Combat mutators**: `CharacterDataComponent::ResetToMax/ServerSetHP/ServerSetEP/ResetRunWorldStats/SetHeadStartAllocation` (`Character/CharacterDataComponent.h:167-572`); `BrokenDarknessManager::ForceTransformation/RollForBreak` (`Combat/Mechanics/BrokenDarknessManager.h:99,150`); `CombatGridSubsystem::PushActorBack/PullActorForward/SwapActorPositions` (`Combat/Grid/CombatGridSubsystem.h:236-255`); `SkillEffectManager::ApplyEffects/ApplyInfusionDOT/ApplyWeaponInfusionDOT/RemoveEffectsByName/RemoveAllDOTs` (`Skills/Effects/SkillEffectManager.h:117-208`); `ActionExecutor::DoWeaponStatsApply` (`Combat/Actions/ActionExecutor.h:204`); `InfusionVFXComponent::ToggleInfusion` (`Infusion/InfusionVFXComponent.h:158`); `InfusionChargeManager::ApplyChargeToAction` (`Infusion/InfusionChargeManager.h:218`); `EquipmentBonusGenerator::Generate/Reroll` (`Equipment/EquipmentBonusGenerator.h:64,83`).
  - **Class gating never consulted by C++**: `UCharacterData::CanUseSpells/CanUseAbilities/UsesRings/CanDualWield` (`Character/CharacterData.h:260-272`) — if BP uses them for UI gating while C++ doesn't, that's the Generic-casts-spells mismatch (§5.1 #19).
  - Full list in scratchpad `bpscan.txt` (getters, colour helpers, debug — mostly harmless).

### 9.4 Delegates nobody listens to in C++ (29 of 65 `BlueprintAssignable`)

BP may bind these; if not, they're dead. Demo-relevant ones: `ACombatOrchestrator::OnWorldStatDraftReady` (never fires on a win anyway, §5.1 #2), `OnActorTurnStarted`, `OnActionCompleted`; `UActionExecutor::OnTargetKilled/OnHealingDone/OnImpactFrame`; `UDefenseSystem::OnDefenseWindowOpened/OnDefenseCueTriggered/OnDefensePerfect/OnParryReflect` (natural feed for the missing defense prompt); `UWeatherStateManager::OnWeatherChanged`; `UPartySessionSubsystem::OnPartyChanged`; `UCharacterPanelWidget::OnPanelClicked/OnPanelHovered` (target-by-clicking-panel?). Note `OnDamageDealt` *is* bound in C++ (`SkillEffectManager.cpp:44`), so damage numbers can hang off it.

### 9.5 Asset-side inventory (what to open in the editor)

58 BP/WBP/ABP assets tracked. Ones that can hold logic, ranked by risk:
1. `Blueprints/BP_CombatOrchestrator` — `OnCombatStartedUI` HUD build.
2. `Core/Hub/BP_HubPlayerController` — hub input → `Interact()`, likely `EnsureLocalParty`.
3. `UI/Combat/Character/WBP_CharacterPanel` (+ a second `UI/Combat/WBP_CharacterPanel` — **duplicate name, two paths**), `WBP_PlayerPanelContainer`, `WBP_EnemyPanelContainer`, `WBP_TurnOrderStripContainer` — containers have no C++ base, so any spawning logic lives there.
4. `Blueprints/GameModes/*` + `Core/GameModes/BP_CombatGameMode` + `Level/BP_Combat_GameMode` + `Core/Hub/BP_Hub_GameMode` — **5 game-mode BPs**, two hub and three combat variants (Roadmap already flags the hub dup).
5. `Blueprints/BP_WeatherController` — the expected (only) listener of `OnWeatherChanged` (`WeatherStateManager.cpp:332,459`); drives the sky. Visual, so acceptable in BP if it only maps weather → sky params.
6. 12 `BP_CombatCharacter_*` + `BP_EnemyBase/BP_Enemy_Generic` + 5 legacy `BP_CombatCharacterBase*`/`BP_TestCombatCharacter_*` under `Data/Characters/Models/` — should be config only.
7. Test BPs (`Testing/*`, `BP_*DataTester` ×4, `BP_CombatOrchestratorTestActor`) — candidates for deletion.

### 9.6 Pass 2 — what I need from Crown (≈5 min in the editor)

For each of items 1–5 above: open the Event Graph, select all (Ctrl+A), copy (Ctrl+C), paste the text here. That T3D text is fully readable, so I can port each graph to C++ exactly and nothing gets guessed. (Alternatively: get the `unreal-mcp` server connecting — it was refused this session — and I can read BPs directly.)

### 9.7 Port plan (after pass 2; ≤3 files per cluster)

1. **C++ combat HUD root** — new `UCombatHUDWidget` (C++ base for a WBP layout with `BindWidget` slots for strip, menu, panel containers); `ACombatOrchestrator` creates it after `InitializeCombat` and calls the `Initialise*` functions. Removes `OnCombatStartedUI` logic (keep the event as a cosmetic hook). *Also fixes §5.1 #15 ordering.*
2. **Effects list in C++** — pooled `WBP_EffectRow` children, data set in C++; `RebuildEffectsList` becomes cosmetic-only.
3. **Hub player controller in C++** — `AHubPlayerController` binding `IA_Interact` → nearest interactable `Interact()`, calls `EnsureLocalParty` at a defined point; dedupes the 3× `FindNearestInRange` (§5.4).
4. **Turn slot / shop row data binding** → C++.
5. **Defense prompt** built in C++ off `UDefenseSystem` delegates (§9.4).
6. **Results screen** in C++ off `OnCombatResultReady` (ties to Arc B).
7. Cleanup: delete orphan test BPs / duplicate game modes once 1–3 are verified in PIE (per CLAUDE.md incremental rule).
