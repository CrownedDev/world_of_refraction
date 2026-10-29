# Balance Simulator — design (2026-10-29)

**Status:** proposal; built by Phase 1f jobs B-291..B-293 (the headless seams are correctness fixes for montage-less content too). Numbers can't pass until B-220 and B-224 are in dev.

---


**Status:** proposal, nothing built, no repo file changed. Code cited as `Source/world_of_refraction/...:line` on `dev` (2026-10-29, `287412b`). Targets come from `docs/analysis/BaselineNumbers.md` (Decided list) and pillar 6 (`docs/Design/DesignPillars.md`).

## 0. What exists already (and is reused)

- `FWoRTestWorld` (`Private/Tests/WoRTestWorld.h`): real `UGameInstance` (every subsystem), hand-ticked world, `SpawnCombatant(Data, bAIControlled)`, `SpawnCombatantWithLoadout(Data, Weapon, bAI)`, `AddEffectMirror` (GAS path). An unpossessed pawn is AI-routed (`CombatControl.h:20-37`, `CombatOrchestrator.cpp:1564-1576`), so a sim fight is "every combatant a pawn".
- A real `ACombatOrchestrator` drives whole fights headless today (`CombatHangFixesSpec.cpp`, `CombatRitualFireSpec.cpp`); the HUD is skipped with no player controller (`CombatOrchestrator.cpp:159-167`); the camera director is harmless.
- One seeded stream per battle: `UBattleRandomSubsystem::SeedStream` / `wor.Battle.Seed` (`BattleRandom.md`), every in-fight roll on it, so an AI-only fight replays exactly from its seed.
- Metrics sources already broadcast: `UActionExecutor::OnDamageDealt(Attacker, Target, Damage, bCritical)` (`ActionExecutor.cpp:3006`), `OnTargetKilled` (`:2083`), `OnHealingDone`, `ACombatOrchestrator::OnActionExecuted` / `OnActorTurnStarted` / `OnCombatResultReady` (`FCombatResult.TotalTurns`, survivors), `UStatusBuildupManager::OnStatusBuildupChanged`.
- Trial difficulty x zone tier is one function: `TrialGate::ApplyEnemyStrength(Enemy, Mult)` with `Mult = TrialGate::GetEnemyStrengthMultiplier(Difficulty) * AEnemyZone::GetTierStrengthMultiplier(Tier)` (`BattleGameMode.cpp:258-262`, tiers x1 / 1.15 / 1.3 / 1.5 / 1.75, `EnemyZone.cpp:13`). The sim calls the same two functions.
- GAS switch: `UWoREffectMirrorComponent::SetReadsFromGAS(bool)` (how `EffectsBaselineSpec.cpp:160-167` runs both paths).

## 1. Architecture

```
Private/Balance/            (new; #if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING)
  BalanceSimTypes.h         FBalanceArchetype, FBalanceMatchup, FBalanceFightRecord, FBalanceAggregate, FBalanceTarget
  BalanceSimRecorder.h      UBalanceSimRecorder (UObject): binds the delegates above, fills one FBalanceFightRecord
  BalanceSimRunner.h/.cpp   FBalanceSimRunner: RunMatchup(Matchup, N, BaseSeed, bReadsFromGAS) -> records; RunPreset(Name)
  BalanceArchetypes.cpp     the archetype table (transient UCharacterData / UWeaponData / UAbilityData / USpellData, as specs build them)
  BalanceSimReport.cpp      CSV (one row per fight), JSON (aggregates + targets + pass/fail), Markdown summary
Private/Tests/BalanceSimSpec.cpp      WoR.Balance.Sim.* (the nightly check; asserts targets with tolerances)
Private/Debug/Combat/BalanceSimDebug.h/.cpp   wor.Balance.Run <preset> <N> [seed] [gas=0|1|both]
docs/Architecture/BalanceSimulator.md
```

**One fight** (`FBalanceSimRunner::RunFight`): build actors from the matchup in a reused `FWoRTestWorld` (one world per matchup, respawned actors per fight, the orchestrator reused: `StartCombat` force-ends a previous fight and `ForceEndCombat` resets bars, effects, grid); `BattleRandom->SeedStream(BaseSeed + FightIndex)`; apply rank HP multiplier and `TrialGate::ApplyEnemyStrength`; `Orchestrator->StartCombat(Party, Enemies, Difficulty)`; then `while (InProgress && Ticks < TICK_CAP) TestWorld->Tick()`. With the seams in §2 a turn costs about 3 ticks (next-tick timers only). Hitting `TICK_CAP` records the fight as `Stalled` (a bug, fails the spec, never silently a Draw).

**Matchup grid** (data, not code paths): `Party archetypes x Enemy archetype x EEncounterRank x EAIDifficulty x ZoneTier x TrialDifficulty`. Presets: `quick` (3 matchups x 3 fights, runs inside the normal `WoR.` suite in < 10 s), `nightly` (24 matchups x 50), `full` (x 1000). Preset and N come from `-WoRBalance=<preset>` / `-WoRBalanceN=<n>` on the command line (test.sh passes them through) or the console command's arguments.

**GAS diff mode**: every matchup runs twice per seed, `SetReadsFromGAS(false)` then `(true)`, with `AddEffectMirror` on every combatant both times. Per seed the two fight records must be identical (damage sequence, turns, winner); differences are listed in the report and fail `WoR.Balance.Sim.GASParity`. Aggregates are reported per path.

**Entry points**: the spec (nightly, `Scripts/ue/test.sh WoR.Balance.`), and `wor.Balance.Run`, which creates its own `FWoRTestWorld`, so it works in the editor outside PIE (`FTestWorldWrapper` is engine code under `WITH_DEV_AUTOMATION_TESTS`, present in Development Editor builds). Output under `Saved/Balance/<preset>_<yyyymmdd-hhmm>.{csv,json,md}`; the spec also prints the Markdown summary to the log.

**Log volume**: the executor and AI log every action at `Log`. The runner raises `LogTemp` and `LogWoRCombat` to `Warning` for the run and restores them after (`wor.Balance.Quiet 0` keeps them).

## 2. What blocks a fully headless fight today, and the minimal seams

| # | Blocker (file:line) | Cost today | Minimal seam |
|---|---|---|---|
| 1 | AI thinking delay is a real timer: `AIDecisionManager.cpp:127-140`, Easy 2.0-3.5 s (`AIDecisionConstants.h:41-51`) | 20-350 ticks per AI turn | CVar `wor.AI.ThinkDelayScale` (default 1). Keep the `FRandRange` draw (`:487`, so a seed replays the same as live), multiply the delay, and at 0 use `SetTimerForNextTick`. |
| 2 | A montage-less ability never lands an impact: the per-impact loop runs only from the Impact anim notify (`ActionExecutorMontage.cpp:350-470`); the no-animation branch (`ActionExecutor.cpp:4783-4790`) just waits for the count-based window's failsafe, `MaxWindowDuration` 8 s (`DefenseSystem.h:472`, `DefenseSystem.cpp:95`), under the 10 s action timeout (`ActionStructs.h:445`). The lumped close applies full damage **undefended**, so the AI never Blocks / Parries / Dodges headless | 800 ticks per attack, and wrong numbers (no AI defense) | Factor the body of the Impact-notify loop into `UActionExecutor::ResolveImpactFrame(int32 Index)`; in the no-animation branch, when CVar `wor.Combat.InstantImpacts` is 1 (default 0), call it for `Index = 0..ExpectedImpacts-1` then close the windows. Same code path as a real montage, one impact per call, so AI defense (`ResolveImpactDefense` -> `TrySynthesizeImpactDefense`, `ActionExecutor.cpp:1870-1880`) runs per impact. `CombatRitualFireSpec` keeps the CVar at 0 and stays as it is. |
| 3 | A montage-less spell never releases: the cast dispatches only from the `SpellRelease` notify (`ActionExecutor.cpp:5073-5091`) or a Cast notify; without it the 0.3 s lumped window times out undefended | 30 ticks and no AI defense | In the same no-animation branch, for a Spell, dispatch `CastArray[0]` immediately (`DispatchSpellCast`, `:3591`). `Instant` and `AOE` resolve synchronously through `ResolveSpellArrival` (`:3747-3815`). `Projectile` spawns a flying actor (`:1203-1226`, burst timer) - sim spells use `Instant`; projectiles stay a TODO (they would still land through the 8 s failsafe, slowly and undefended). |
| 4 | No status-break event: the cap fires `TriggerSkillEffectFromBuildup` (`StatusBuildupManager.cpp:385`, `:654-668`) with nothing to listen to | status-break rate unmeasurable | `FOnStatusTriggered(Target, Source, StatusType, Element)` dynamic delegate broadcast at `:668`. Useful for the HUD later too. |
| 5 | No round counter: a round is added inside `AdvanceSimState` (`TurnManager.cpp:684-700`), not exposed | "rounds" has to be guessed from turns | `int32 RoundCount` on `UTurnManager`, `++` where the round is added, `GetRoundCount()`; reset in `InitializeCombat`. |
| 6 | Enemy rank HP multiplier does not exist (B-224 TODO; `EncounterRank` only sits on the orchestrator, `CombatOrchestrator.h:250-324`) | boss fights are normal fights | Until B-224 lands the runner scales `UCharacterDataComponent::MaxHP` / `CurrentHP` itself (`CharacterDataComponent.h:129-135`; a written max is mirrored to GAS, B-042u) by 1 / 1.5 / 3 / 6 x TIER_POWER(enemy tier). One line to delete when B-224 ships, and a spec case that fails if both apply. |
| 7 | Weapon Power is on an unbuilt branch (B-220, Power defaults 0): an attack hits for `SkillBase` only | the Decided numbers can't be hit yet | Cluster 1 runs on today's numbers (its job is the harness). The target assertions stay `Warning`-only until B-220 + B-224 are in `dev`, then flip to errors (one constant). |
| 8 | Party-side AI difficulty is fixed at Hard (`AIDecisionConstants.h:15`, `AIDecisionManager.cpp:318-324`) | can't sweep ally difficulty | Not a blocker; recorded per matchup. A sweep needs a setter - deferred. |

Everything else already runs headless: grid placement, weather, first strike, Broken Darkness, effects, GAS mirror, item use. Timers left after the seams are next-tick only (`CombatOrchestrator.cpp:835`, `ActionExecutor.cpp:4949/4977`, `CombatCameraDirector.cpp:329`).

## 3. Metrics and targets

Per fight (CSV row): preset, matchup id, seed, gas path, winner, turns, rounds, player turns, per-side damage dealt / taken, hits, crits, crit rate, mean / p10 / p90 hit, hits-to-kill per enemy (hits landed on it before death), status breaks (by element / physical), AI defenses attempted / matched, EP spent, survivors, stalled flag.

| Metric (aggregate per matchup) | Target (Decided list) | Tolerance (nightly) |
|---|---|---|
| Player turns to clear a Normal fight (3 v 2-3) | 3-5 | mean in [2.5, 5.5]; p90 <= 7 |
| Rounds for a Mini-boss (3 v 1, x3 HP) | 8-10 | mean in [7, 11] |
| Rounds for a Boss (3 v 1, x6 HP) | 12-14 | mean in [11, 15.5]; p90 <= 18 |
| Hits to kill a same-tier Normal enemy | 3-5 (§2.9) | mean in [2.5, 5.5] |
| Elite / Mini-boss / Boss hits to kill | 5-7 / 10-14 / 25-35 | +-20 % |
| Basic attack vs the §3 worked examples (F/C/S) | 44 / 132 / 382 (no crit, Medium) | +-10 % on the uncrit median |
| Crit rate, un-invested Luck | 5 % base chance; crit = x1.25 | rate in [3, 8] %; crit/uncrit median in [1.2, 1.3] |
| Defense reduction cap | <= 75 % with gear + buffs | max observed (1 - taken/raw) <= 0.76 |
| Pillar 6: low level + S gear vs the high-level F-gear party | both sides win sometimes | win rate in [35, 65] % (Medium, Normal) |
| Pillar 6 reverse: high level + F gear vs same-tier enemies | still viable, slower | clears, mean turns <= 2 x the matched-gear archetype |
| Win rate, party vs Normal at Medium | high but not certain | [85, 99] % |
| Win rate, party vs Boss at Medium | "challenging but fair" (pillar 3) | [50, 80] % |
| Stalled fights | 0 | 0 (error) |
| GAS parity (per seed, both paths) | identical records | 0 differences (error) |
| Same seed twice (determinism) | identical records | 0 differences (error) |

The win-rate bands for bosses have no Decided number; they ship as the reference-backed default (Expedition 33 / Hades: a first boss attempt is lost more often than a normal fight, never a coin flip at Normal) and are listed in the hand-off for Crown to move.

## 4. Archetype set (built in code, numbers from `BaselineNumbers.md` §3-4)

Player archetypes (`FBalanceArchetype`: class, pillar levels, sub-stat points, weapon {tier, Power, grades, requirement}, skills):

| Id | Class | Pillars | Points | Weapon | Skills |
|---|---|---|---|---|---|
| P-EarlyGenericF | Generic | Body 2 | RawDamage 6, Defense 4, MaxHealth 7 | F sword, Power 40, Body C, req 0 | attack (SkillBase 0), F ability SkillBase 20, Slash |
| P-MidResonatorC | Resonator | Mind 8, Spirit 5, Body 3 | RawDamage 8, MaxHealth 15, Defense 10 | C staff, Power 88, Mind B / Spirit D, req Mind 3 | C ability SkillBase 30; C Destruction spell SkillBase 30, Instant, Fire |
| P-LateCasterS | Caster (Fire) | Mind 14, Spirit 6 | SpellDamage 20, MaxHealth 20, Luck 10 | S staff, Power 192, Mind S / Spirit E, req Mind 7 | S spell SkillBase 40, Instant; S ability SkillBase 40 |
| P-LowLevelSGear (pillar 6) | Generic | Body 4, Spirit 2 | RawDamage 6 | S greatsword, req Body 7 (factor floors at 0.5) | attack, F ability |
| P-HighLevelFGear (reverse) | Generic | Body 14 | RawDamage 20 | F sword, Body C | attack, F ability |

Parties: `Early` = 3 x P-EarlyGenericF; `Mid` = P-MidResonatorC + 2 x P-EarlyGenericF at C gear; `Late` = P-LateCasterS + 2 Generic at S gear; `Pillar6` = 3 x P-LowLevelSGear vs 3 x P-HighLevelFGear (party vs party, both AI).

Enemy archetypes (per §3): `E-F` Body 1, Defense 4, MaxHealth 7, F weapon; `E-C` Body 5, Defense 15, MaxHealth 20, C; `E-S` Body 7 (asset max), Defense 25, MaxHealth 35, S. Ranks: Normal x1 (2-3 enemies), Elite x1.5 (2), Mini-boss x3 (1), Boss x6 (1, `UBossPhaseComponent` optional later). Difficulty {Easy, Medium, Hard}; zone tier {0, 2, 4}; trial difficulty {Normal}. The `nightly` grid: {Early x E-F, Mid x E-C, Late x E-S} x 4 ranks x {Medium, Hard} = 24 matchups, plus the two pillar-6 matchups.

Attacks and abilities are montage-less transient `UAbilityData` (`bIsAttack`, `PhysicalDamageType`, `StatusBuildup`, `Tier`), spells transient `USpellData` with one `Instant` cast entry. No `.uasset` is needed; a later preset can load real assets by path once Crown wants authored content checked.

## 5. Implementation plan (clusters of <= 3 files; CC writes, Crown's desktop builds)

| # | Cluster | Files | Done when |
|---|---|---|---|
| 1 | **One fight, one metric.** Think-delay seam + a spec that runs 1 AI pawn v 1 AI pawn with a montage-less attack to the end and reports turns | `AIDecisionManager.cpp` (+ CVar in `AIDecisionConstants.h`), `Tests/BalanceSimSpec.cpp` (`WoR.Balance.Sim.OneFight`), `docs/Architecture/AISystem.md` | the fight ends (someone dies) in < 100 ticks, turns logged, suite green |
| 2 | Instant impacts seam (blocker 2) | `ActionExecutorMontage.cpp` (factor `ResolveImpactFrame`), `ActionExecutor.cpp` (no-animation branch + CVar), `ActionExecutor.h` | spec case: at `InstantImpacts 1` a Hard AI defender matches at least one impact over 20 seeds; `WoR.Combat.RitualFire` unchanged |
| 3 | Spell release seam (blocker 3) | `ActionExecutor.cpp`, `BalanceSimSpec.cpp` | an Instant spell lands synchronously headless with AI defense |
| 4 | Status-break delegate + round counter (blockers 4, 5) | `StatusBuildupManager.h/.cpp`, `TurnManager.h/.cpp` -> split 4a (status, + spec case) / 4b (rounds, + spec case) | delegate fires on a cap; `GetRoundCount` equals the hand count in `WoR.TurnManager` |
| 5 | Sim types + recorder | `Balance/BalanceSimTypes.h`, `Balance/BalanceSimRecorder.h`, `BalanceSimSpec.cpp` | one fight's record filled from the delegates |
| 6 | Runner: N fights per matchup, seeds, reused world, rank HP stand-in, `ApplyEnemyStrength`, stall cap, quiet logging; CSV writer | `Balance/BalanceSimRunner.h/.cpp`, `Balance/BalanceSimReport.cpp` | 100 fights of one matchup < 10 s; same seed twice identical |
| 7 | Archetypes + presets | `Balance/BalanceArchetypes.cpp`, `BalanceSimTypes.h`, `BalanceSimSpec.cpp` | `quick` preset runs in the suite |
| 8 | Targets, tolerances, JSON + Markdown summary; `WoR.Balance.Sim.Targets` (warnings until B-220/B-224) | `BalanceSimReport.cpp`, `BalanceSimSpec.cpp`, `docs/Architecture/BalanceSimulator.md` | summary lists every target with pass / warn |
| 9 | GAS diff mode + `WoR.Balance.Sim.GASParity` | `BalanceSimRunner.cpp`, `BalanceSimSpec.cpp`, `docs/Architecture/GASSystem.md` | 0 differences on `nightly` |
| 10 | Console command + scripts + docs | `Debug/Combat/BalanceSimDebug.h/.cpp`, `Scripts/ue/balance.sh` (test.sh filter `WoR.Balance.` with `-WoRBalance=nightly`), `docs/Architecture/BalanceSimulator.md`, `docs/PIE/B-xxx.md` | `wor.Balance.Run nightly 50` writes the three files from the editor |

Order 1-3 first (they are the seams; each is also a correctness fix for montage-less content). 5-10 are pure additions. Each cluster: build + `test.sh`, `wor-reviewer`, commit.

## 6. Risks

- **AI determinism.** Every roll is on the battle stream, but order matters: the AI iterates `GetLivingEnemies` (array, stable) and scores targets with ties broken by order; effect IDs use `FMath` (identity only). GAS: `FActiveGameplayEffectsContainer` iteration could reorder equal-magnitude effects. Mitigation: the "same seed twice" and GAS-parity checks are errors from cluster 6 on, so any nondeterminism shows up with a seed to replay (`wor.Battle.Seed`).
- **Run time** (target 1000 fights < 2 min = 120 ms per fight). Estimate after the seams: ~30 turns x ~3-4 ticks x ~0.2 ms (6 actors, `-nullrhi`) = ~25 ms, plus 6 spawns and `StartCombat` (~10 ms). World creation is the expensive part (hundreds of ms), hence one world per matchup. Measured in cluster 6; if a reused world leaks state between fights (a subsystem field D-064 hasn't moved yet), fall back to one world per 50 fights.
- **Flakiness.** A stranded async turn would hang forever; the tick cap turns it into a `Stalled` error with its seed. Log volume at `Log` would reach GB per `full` run; the quiet switch is in cluster 6. Tolerance bands are means over >= 50 fights; `quick` (3 fights) only checks "ends, no stall, parity".
- **Numbers move.** Until B-220 (Power) and B-224 (rank HP, floor 0.5, Defense 75 %, crit 1.25) land, the targets can't pass; the spec warns instead of failing, and the stand-in rank multiplier in the runner must be removed with B-224 (spec case guards the double-apply).
- **Fidelity gaps** (recorded, not fixed here): projectiles and burst spells not simulated; party AI fixed at Hard; no items / crystals / stones in the archetypes yet; boss phases (`UBossPhaseComponent`) not attached; player timing play (Perfect Guard) is modelled only by AI defense rates.
