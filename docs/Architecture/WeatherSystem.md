# Weather System

## Overview

The weather system swaps the in-combat sky based on which team's *leader* dominates,
where dominance scales with the leader's current HP percentage. The C++ side
(`UWeatherStateManager`) resolves both teams' weather data assets and broadcasts a
single delegate; a Blueprint actor (`BP_WeatherController`) listens to that delegate
and drives the level's sky. The HP signal that should trigger weather updates already
flows end-to-end inside C++; the practical break point is *what the resolver returns*
and *whether the BP consumer reacts to the broadcast*, not the wiring itself.

> Scope: this document covers the C++ side (`UWeatherStateManager`,
> `UCharacterDataComponent`, `UCosmeticsData`, the orchestrator hook) and the contract
> exposed to the Blueprint consumer. The Blueprint controller is LFS-tracked
> `.uasset` and is not readable here — its bindings are inferred from the C++
> delegate surface and the April restructure plan
> (`PastDocumentation/April2026/WeatherSystem_Restructure_April2026.md`).

## Architecture

### `UWeatherStateManager` (`UGameInstanceSubsystem`, `WeatherStateManager.h:30`)

- Subsystem confirmed — `: public UGameInstanceSubsystem` (`.h:30`).
- Public API (`.h:35-46`):
  | Function | Signature |
  |---|---|
  | `Initialize` | `void Initialize(FSubsystemCollectionBase&)` |
  | `Deinitialize` | `void Deinitialize()` |
  | `InitialiseLeaders` | `void InitialiseLeaders(const TArray<AActor*>& Team0, const TArray<AActor*>& Team1)` — BlueprintCallable |
  | `EndCombat` | `void EndCombat()` — BlueprintCallable |
- Public delegate (`.h:24-27, 52-53`):
  ```cpp
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeatherChanged,
      UPrimaryDataAsset*, Team0WeatherDA,
      UPrimaryDataAsset*, Team1WeatherDA,
      float, BlendValue);
  ```
- Internal state (`.h:57-58`):
  - `TArray<FLeadershipEntry> Team0Hierarchy` / `Team1Hierarchy` — sorted descending
    by `WorldMindLevel + WorldBodyLevel + WorldSpiritLevel` (`.cpp:97-99`).
  - `FLeadershipEntry { AActor* Actor; int32 TotalWorldStats; ESpellElement Element; }`
    (`.h:9-22`). `Element` is captured at hierarchy build (`.cpp:89`) but **never
    read by `ResolveWeatherDA`** — dead data on the struct (see G2).
  - No "current weather DA" cache, no "current intensity" field, no "current leader"
    member: leader is always `Hierarchy[0].Actor`, recomputed each broadcast.
- `Initialize` clears both hierarchies and logs (`.cpp:8-11`). `Deinitialize` calls
  `EndCombat` (`.cpp:14-17`).

### `UCharacterDataComponent` — HP source (`CharacterDataComponent.h:26`)

- HP lives on the component, not the asset: `int32 CurrentHP` / `int32 MaxHP`
  (`.h:48-55`). `UCharacterData` (asset) carries the *formula* (`CalculateMaxHealth`,
  `CharacterData.h:432`), not the live value.
- HP-change delegate (`.h:14, 147`):
  ```cpp
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, int32, CurrentHP, int32, MaxHP);
  UPROPERTY(BlueprintAssignable) FOnHPChanged OnHPChanged;
  ```
- Broadcast sites (`.cpp:114, 125, 135, 144, 239, 273, 278`): `ResetToMax`,
  `ServerTakeDamage`, `ServerHeal`, `ServerSetHP`, revive intercept inside
  `CheckDeath`, `ServerResurrect`, and `OnRep_CurrentHP`.
- C++ binders (project-wide grep for `OnHPChanged.AddDynamic`):
  | Subscriber | File:line |
  |---|---|
  | `UCharacterPanelWidget::HandleHPChanged` | `UI/Combat/CharacterPanelWidget.cpp:72` |
  | `UWeatherStateManager::OnLeaderHPChanged` | `WeatherStateManager.cpp:117` |
- The weather manager **does** bind an HP delegate (`.cpp:117`).

### `UCosmeticsData` — variant storage (`CosmeticsData.h:23`)

- `EquippedWeatherVariant` is on the *cosmetics* asset, not on `UCharacterData`:
  ```cpp
  // CosmeticsData.h:71-75
  /** Weather variant equipped for when this character is team leader.
   *  Leave null to use element default. Generic and Resonator classes ignore this. */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
  UPrimaryDataAsset* EquippedWeatherVariant = nullptr;
  ```
- Access path: `UCharacterData::Cosmetics` (`CharacterData.h:149-150`) →
  `UCosmeticsData::EquippedWeatherVariant`. No dedicated `UWeatherData` type — the
  variant is typed as raw `UPrimaryDataAsset*`, so designers can wire any DA.
- `UWeatherData` does not exist (`Glob *WeatherData*` returns no matches).

### `ACombatOrchestrator` — hook site (`CombatOrchestrator.h:86`)

- The only place that drives the weather manager from C++:
  ```cpp
  // CombatOrchestrator.cpp:177-181
  // Initialise weather leaders
  if (UWeatherStateManager* WeatherManager = GetGameInstance()->GetSubsystem<UWeatherStateManager>())
  {
      WeatherManager->InitialiseLeaders(Team0Combatants, Team1Combatants);
  }
  ```
  and the symmetric tear-down in `ForceEndCombat` (`.cpp:228-231`).
- Called from `StartCombat` *after* grid placement and *before* the turn manager is
  initialised (`.cpp:165-202`).

### `UTurnManager` — leader awareness?

- **No**. `TurnManager.h:85` exposes `InitializeCombat(Team1, Team2)`; grep for
  `Leader` inside `TurnManager.h` returns no matches. Leader concept lives **only**
  on `UWeatherStateManager`.

## Wiring Map (current state)

```
ServerTakeDamage / ServerHeal / ServerSetHP / OnRep_CurrentHP
        ↓ Broadcast
UCharacterDataComponent::OnHPChanged(CurrentHP, MaxHP)        [CharacterDataComponent.cpp:125, ...]
        ↓ AddDynamic at InitialiseLeaders → BindToLeader      [WeatherStateManager.cpp:117]
UWeatherStateManager::OnLeaderHPChanged(CurrentHP, MaxHP)     [.cpp:169]
        ↓
RecalculateWeather()                                          [.cpp:132]
  Team0Dominance = T0Leader.CurrentHP / MaxHP
  Team1Dominance = T1Leader.CurrentHP / MaxHP
  BlendValue     = Team0Dominance - Team1Dominance
  Team0DA = ResolveWeatherDA(Team0Hierarchy)                  [.cpp:219]
  Team1DA = ResolveWeatherDA(Team1Hierarchy)
        ↓ Broadcast                                           [.cpp:161]
FOnWeatherChanged(Team0DA, Team1DA, BlendValue)
        ↓ (Blueprint-only)
BP_WeatherController  → set sky                               [.uasset, not readable]
```

### Leader identification

- **Stat-based**, not first-alive or designated. `BuildHierarchy` sorts each team
  descending by `WorldMindLevel + WorldBodyLevel + WorldSpiritLevel`
  (`.cpp:97-99`, `GetTotalWorldStats` `.cpp:198-210`). `Hierarchy[0]` is the
  current leader.
- Decision happens entirely inside `UWeatherStateManager` — no other system is
  informed of the leader. `TurnManager`, `CombatOrchestrator`, AI all run without a
  leader concept.
- On leader KO: `UCharacterDataComponent::OnDied` (`CharacterDataComponent.cpp:250`)
  → `OnTeam0LeaderDied` / `OnTeam1LeaderDied` (`.cpp:174, 187`) → remove from
  hierarchy, `BindToLeader` on the next entry, `RecalculateWeather`.
- On leader HP change (no death): `OnLeaderHPChanged` (`.cpp:169`) →
  `RecalculateWeather`. Leader identity doesn't move on HP — only on death.

### Variant resolution (`ResolveWeatherDA`, `.cpp:219-242`)

```cpp
// .cpp:231-241
if (Data->CharacterClass == ECharacterClass::Generic ||
    Data->CharacterClass == ECharacterClass::Resonator)
    return nullptr;

if (Data->Cosmetics && Data->Cosmetics->EquippedWeatherVariant)
    return Data->Cosmetics->EquippedWeatherVariant;

return nullptr;  // No equipped variant — sky stays as level default
```

- No `SetActiveWeather(UWeatherData*)` function exists — the manager *resolves and
  broadcasts*; it never calls into the controller directly.
- The Blueprint consumer (`BP_WeatherController`) is the only caller of any
  "set sky" action, via its `OnWeatherChanged` binding (inferred — file is LFS).
- The April restructure plan
  (`PastDocumentation/April2026/WeatherSystem_Restructure_April2026.md:108-117`)
  specifies the controller's intended logic: choose `Team0DA` vs `Team1DA` based on
  `BlendValue`'s sign with a `±0.05` dead-zone, fall back to a default when the
  selected DA is null.

## Where the chain breaks

The HP→manager half of the chain is intact and verifiable from C++. The most likely
break points are downstream:

1. **`ResolveWeatherDA` returns `nullptr` for the realistic test setup**
   (`WeatherStateManager.cpp:231-241`). If either leader is **Generic** or
   **Resonator**, that team's DA is `nullptr`; if a leader is a **Caster** but the
   `UCosmeticsData::EquippedWeatherVariant` field on `UCharacterData::Cosmetics` is
   unset (or `Cosmetics` itself is unset), the DA is `nullptr`. In both cases the
   delegate fires with `(nullptr, nullptr, BlendValue)` and `BP_WeatherController`'s
   intended logic (April plan, lines 109-117) collapses to "set default weather" on
   every broadcast — visually a no-op. **Cite:** `WeatherStateManager.cpp:231-241`,
   `CosmeticsData.h:71-75`.

2. **Leader is selected by world stats, not by player role**
   (`WeatherStateManager.cpp:97-99`). For two evenly statted test pawns or two
   Generic pawns, the "leader" is whichever sorted first — both teams may resolve to
   `nullptr` DAs regardless of which actor the player thinks of as the leader.

3. **`FOnWeatherChanged` has zero C++ subscribers** (`grep -n
   "OnWeatherChanged.AddDynamic"` returns no matches across `Source/`). The only
   listener is whatever `BP_WeatherController` binds in Blueprint. We cannot
   verify the binding from the source tree; if the BP binding has regressed (e.g.
   the Bind Event node was disconnected, the controller doesn't exist in the
   current level, or the binding targets the wrong delegate), the C++ side fires
   into a void. **Cite:** `WeatherStateManager.h:52-53`, broadcast
   `WeatherStateManager.cpp:161`.

4. **Sky only changes when `BlendValue`'s sign crosses zero, not on every HP tick**
   (per the intended controller logic, April plan lines 109-117). If both leaders'
   DAs are non-null and the user is expecting weather to *visibly* shift on each
   damage instance, the design itself only swaps the displayed DA when the balance
   of power flips. Damage that doesn't move the sign keeps the same DA selected.

5. **Not a timing issue.** `InitialiseLeaders` runs synchronously inside
   `StartCombat` *after* the team arrays are stored and grid placement is done
   (`CombatOrchestrator.cpp:170-181`); `UCharacterDataComponent::BeginPlay` runs
   when the actor spawns, well before `StartCombat`. By the time `BindToLeader`
   queries `FindComponentByClass<UCharacterDataComponent>`, the component exists.
   `MaxHP` is set by `RecomputeMaxPools` in `BeginPlay` (`.cpp:55`), so the
   `MaxHP > 0` guard in `RecalculateWeather` (`.cpp:144, 152`) passes.

**Most likely single cause:** #1 — either the test characters' classes (Generic /
Resonator) zero out the variant, or `Cosmetics->EquippedWeatherVariant` is unset on
the data asset that backs the leader.

## Problems found (broader)

### G1 — `OnHPChanged` binding leaked across combats

`EndCombat` (`WeatherStateManager.cpp:41-65`) unbinds **only** `OnDied`. The
`OnHPChanged` binding from `BindToLeader` (`.cpp:117`) is **not** removed. If the
same `UCharacterDataComponent` survives across multiple combats (it lives on the
actor, not on per-encounter state), the next `InitialiseLeaders` will call
`AddDynamic` again. `AddDynamic` is idempotent for the same `(UObject, UFUNCTION)`
pair, so this is technically safe — but it is asymmetric with the `OnDied` cleanup
and surprising. Symmetric cleanup is the right fix.

### G2 — `FLeadershipEntry::Element` is dead data

`BuildHierarchy` populates `Entry.Element` from `CharacterData->InnateElement`
(`.cpp:88-91`) and `BindToLeader` prints it in the log (`.cpp:128`), but
`ResolveWeatherDA` never reads it. Resolution is purely via
`Cosmetics->EquippedWeatherVariant`. Either drop the field or wire it into a
fallback "element-default DA" lookup as the April plan intended (table in
`WeatherSystem_Restructure_April2026.md:96-107`).

### G3 — `EndCombat` does not broadcast a final state

`EndCombat` clears hierarchies but does not broadcast
`OnWeatherChanged(nullptr, nullptr, 0)`. The sky is left in whatever state the
last broadcast set it to. Known limitation, already pinned in
`Architecture_2026-05-14.md:710` and `WeatherSystem_Restructure_April2026.md`.

### G4 — Stale documentation: `EquippedWeatherVariant` location

`docs/Architecture_2026-05-14.md:506` states the field is "`UPrimaryDataAsset*` on
`CharacterData.h:195`". It is actually on `CosmeticsData.h:75`, reached via
`UCharacterData::Cosmetics`. `WeatherStateManager.cpp:237-238` confirms the access
path. Update the long-form architecture doc when next touched.

### G5 — Unstable sort for tied leaders

`Hierarchy.Sort` (`.cpp:97-99`) uses a strict-greater comparator; ties keep
implementation-defined order. Two characters with identical world stats will pick a
leader by array order — fine for now but worth a deterministic tiebreaker (e.g.
spawn index or actor name hash) before character creation lands.

### G6 — Missing null-actor guard in death handlers

`OnTeam0LeaderDied` / `OnTeam1LeaderDied` (`.cpp:174-185, 187-196`) dereference
`DeadActor->GetName()` without a guard. `OnDied` is broadcast with `GetOwner()`
which is non-null at the call site, so this is defensive only — but the
established project pattern is to guard.

### G7 — No subsystem-pointer null guards at hook sites

`CombatOrchestrator.cpp:178` does early-out if the subsystem is missing, which is
correct. `CombatOrchestrator.cpp:228` (in `ForceEndCombat`) does the same. No issue
here — flagging that this is the *only* C++ call site, so any future caller should
copy the same guard.

### G8 — Resolver returns `nullptr` instead of a positive "no weather" signal

The current contract — `Team0DA = nullptr` means "use default sky" — is implicit;
the Blueprint must interpret null itself. A defaulted level-fallback DA reference
on `UWeatherStateManager` (or an `ESpellElement`-keyed default map per the April
plan) would make the BP consumer simpler and remove an interpretation step that's
easy to get wrong in Blueprint.

### G9 — Weather logic runs on every HP broadcast

`OnLeaderHPChanged` unconditionally calls `RecalculateWeather` (`.cpp:169-172`),
which re-resolves both DAs and broadcasts. This is cheap today but means the BP
controller is called on every point of damage. If the BP-side "set sky" call is
expensive or animated, throttling at this layer (skip when `BlendValue` hasn't
crossed a threshold since last broadcast) would be cleaner than relying on the BP
to debounce.

## Debug tool gaps

- **No `PrintWeatherState()` / `DebugLogHierarchy()` / `GetWeatherStateString()`**
  on `UWeatherStateManager`. Grep for `Debug` / `CallInEditor` inside
  `WeatherStateManager.h` returns no matches. The only inspection surface today is
  `UE_LOG(LogTemp, Log, ...)` lines at `.cpp:11, 36, 64, 125, 163, 176, 189`.
- **No `CallInEditor` button** on any weather-related data asset. `UCosmeticsData`
  (`CosmeticsData.h`) exposes only the property; no debug-print on the asset.
- **No `WeatherDataDebug` pair** (compare `CharacterDataDebug.h/.cpp` — the project
  convention from CLAUDE.md). This system fails the CLAUDE.md rule: "If a system
  can't be inspected without launching PIE and triggering the exact path, debug
  tools are missing."

What would help, in order of value before any fix attempt:

1. `UFUNCTION(CallInEditor)` `DebugPrintWeatherState` on `UWeatherStateManager`
   that logs both hierarchies (actor name, class, total world stats, resolved DA),
   the current `BlendValue`, and which DA each team currently broadcasts.
2. `UFUNCTION(CallInEditor)` on `ACombatOrchestrator` that calls the above —
   inspection from the level actor that owns combat.
3. A snapshot log line on every `RecalculateWeather` broadcast that also names
   `Cosmetics->EquippedWeatherVariant` (today's `.cpp:163` log only names the
   resolved `Team0DA`/`Team1DA`, which is the *output* — useful, but doesn't tell
   you why null came out).

## Smallest possible fix

Before changing any code, confirm in PIE with two probes added to
`UWeatherStateManager::RecalculateWeather`:
(a) Log the **leader actor**, the leader's **`CharacterClass`**, the leader's
`Cosmetics` pointer, and `Cosmetics->EquippedWeatherVariant` for each team — so the
"why is the DA null?" question is answerable from the Output Log; and (b) log when
`OnWeatherChanged` is broadcast and with what `BlendValue`, then watch whether
`BP_WeatherController` reacts. If the BP receives the event but doesn't change the
sky, the fix is on the BP side (probably a regression in the Bind Event node or
the `WeatherMap` lookup mentioned in the April plan). If the BP never receives the
event, the binding is missing. If the DA is consistently `nullptr` for the leader
the player expects, the fix is data: ensure the leader's `UCharacterData->Cosmetics`
asset is set and its `EquippedWeatherVariant` is populated, **and** confirm that
character is a `Caster` (Generic / Resonator return `nullptr` by design,
`WeatherStateManager.cpp:231-234`).

What to verify in PIE before writing any fix: open a combat with one Caster and one
Generic, take damage on each side, watch the Output Log for the new
`RecalculateWeather` probe lines, and reconcile against the broadcast lines from
`BP_WeatherController`. If a Caster vs Caster combat with `EquippedWeatherVariant`
set on both sides *also* shows no visible sky change, the issue is the BP
consumer's response to `BlendValue` (the April plan's `±0.05` dead-zone or its sign
test), not the C++ resolver. That is the cheapest signal-routing decision to make
before touching any code.

## Known Limitations / TODOs

- **No weather restore on combat end** (G3) — see
  `WeatherSystem_Restructure_April2026.md`.
- **`OnHPChanged` not unbound in `EndCombat`** (G1).
- **`FLeadershipEntry::Element` unused** (G2).
- **Stale doc reference** in `Architecture_2026-05-14.md:506` (G4).
- **No debug print** on the manager or weather variant DAs.
- **No element-default fallback** — the April plan's element→default-DA map is not
  implemented; `nullptr` is the only "no variant" signal.
- **No dedicated `UWeatherData` type** — variants are raw `UPrimaryDataAsset*`. The
  type loss means the resolver can't validate that a designer wired in a sky DA
  versus, say, a stance DA.

## Changelog

- **2026-05-20 (`feature/weather-team-hp-and-debug`)** — `ComputeTeamHPPercent` correction: dead members contribute 0 HP to numerator; their MaxHP stays in denominator for persistent post-death pressure (PIE showed the previous skip-the-dead-from-both produced a wrong 100% read after a teammate died).
- **2026-05-20 (`feature/weather-team-hp-signal`)** — Signal refactor: BlendValue
  now reflects whole-team HP, not the leader's individual HP. For each team,
  `ComputeTeamHPPercent` sums `CurrentHP` / `MaxHP` across all hierarchy entries
  where the character's `bIsAlive` flag is `true` (skip-the-dead — hierarchy is
  not pruned, so a resurrected character naturally re-enters the sum).
  `ComputeBlendValue` applies a deadzone (`WEATHER_DEADZONE_GAP = 0.05f`) and
  linear ramp to full saturation (`WEATHER_RAMP_END_GAP = 0.20f`); both constants
  live in an anonymous namespace at the top of `WeatherStateManager.cpp`. Sign
  of `BlendValue` is preserved (positive = Team 0 winning, magnitude = intensity).
  **Leader-DA / team-intensity split is intentional:** `ResolveWeatherDA` still
  asks the highest-statted alive member for their `Cosmetics->EquippedWeatherVariant`,
  so the *displayed* sky is leader-driven; the team-HP sum drives *how strongly*
  it displays. Per-member `OnHPChanged` / `OnDied` bindings replace the previous
  leader-only bindings — damage on any teammate now triggers `RecalculateWeather`.
  `BindToLeader` was renamed to `BindToTeam`; the two death handlers were renamed
  `OnTeam{0,1}MemberDied` and no longer prune the hierarchy or rebind a new leader
  (the team is already fully bound at combat start). `GetCurrentLeader` was changed
  from "return `Hierarchy[0].Actor`" to "return first ALIVE entry" (Option X in
  today's planning) so the resolver picks up a new leader's DA after a death
  without any explicit hierarchy mutation. **Known limitations carried forward:**
  tied world-stats still produce nondeterministic leader selection (G5);
  broadcast throttling is still absent (G9, slightly worsened — AOE attacks now
  fire `RecalculateWeather` once per damaged target rather than only when the
  leader was a target). G6 (death-handler null guard) is fixed as part of this
  rewrite; G1 (asymmetric `EndCombat` cleanup) remains fixed and now iterates
  every entry on both teams.
- **2026-05-20 (`feature/weather-system-doc-bootstrap`)** — Initial document. Maps
  the HP-to-sky chain after the April restructure landed in C++. Captures the
  resolver's actual variant access path (`Cosmetics->EquippedWeatherVariant`, not
  `CharacterData::EquippedWeatherVariant` as referenced by the older long-form
  architecture doc). Catalogues the seven downstream problems found while reading
  the code (G1-G9) and the debug-tool gap.
