# UI System

## Overview

The UI System covers the combat HUD widgets — the on-screen panels that display
character state and combat flow to the player. It is built from native C++
`UUserWidget` base classes (each `UCLASS(Abstract)`) that are subclassed by
Blueprint `WBP_` widgets for visual layout, following the project rule that UI
screens are Blueprint-authored with a C++ base when behaviour is non-trivial.

The system is event-driven: widgets bind to dynamic multicast delegates from
combat subsystems and components at combat start, react to broadcasts, and
unbind at combat end (or on destruct). There is no per-tick polling.

This doc covers three current widgets:

- `UCharacterPanelWidget` — per-character HP/EP/Status panel.
- `UTurnOrderStripWidget` — current actor + upcoming turns strip.
- `UDefensePromptWidget` — block/parry/dodge defense window UI.

### Removed history: `CombatHUDRoot`

`UCombatHUDRoot` was a former top-level HUD root / container widget that owned
HUD lifecycle and was responsible for spawning and tearing down the child
panels. Git history shows it was introduced as an empty native class
(commit `dee2730`), `WBP_CombatHUD` was migrated onto it
(`b96c6df`), then character panels were moved to standalone viewport widgets
(`f49df92`), and finally `CombatHUDRoot` was deleted as no longer used
(`3d44423`, "chore: delete CombatHUDRoot — no longer used"). It no longer
exists in the codebase. The remaining widgets are now spawned individually:
`UCharacterPanelWidget` is spawned by `BP_CombatOrchestrator` as a standalone
viewport widget rather than nested in a HUD root. Some surviving comments still
reference a "HUD root" (e.g. `TeardownPanel` is described as "Called by HUD
root or on destruct") — these are stale references to the removed class.

## Architecture

### `UCharacterPanelWidget`

`UCLASS(Abstract)`, native base for `WBP_CharacterPanel`. One panel per
character. Displays HP/EP/Status bars (with text), name/class/element, world
stats, and a buff/debuff list.

**Public surface**

- `InitialiseForActor(AActor* InActor)` — binds the panel to an actor's
  components and subsystems.
- `TeardownPanel()` — unbinds all delegates.
- `GetBoundActor()` — returns the bound actor (null after teardown).
- Delegates: `OnPanelHovered` (`FOnPanelHovered`, one bool param `bIsHovered`)
  and `OnPanelClicked` (`FOnPanelClicked`, no params), both
  `BlueprintAssignable`.

**BindWidget members** (all `BindWidgetOptional`): `HPBar`/`HPText`,
`EPBar`/`EPText`, `StatusBar`/`StatusText`, `NameText`, `ClassElementText`,
`WorldStatsText`, and `EffectsList` (`UVerticalBox`, container for skill-effect
rows).

**State / private members** — `TWeakObjectPtr` handles to the bound actor,
`UCharacterDataComponent`, `USkillEffectManager`, `UStatusBuildupManager`, and
`UBrokenDarknessManager`; a `bBound` flag. Private helpers: `RefreshEffectsList`,
`SetBarSafe`, `SetTextSafe`, `RefreshEnergyBar`, `ApplyEnergyBarTint`,
`RefreshEPBarVisibility`.

**Blueprint hooks** — `RebuildEffectsList` (`BlueprintImplementableEvent`, BP
fills `EffectsList` from a passed array) and `ApplyStaticText`
(`BlueprintNativeEvent`; the native fallback fills `NameText` only,
`ClassElementText`/`WorldStatsText` are left to a BP override for enum
formatting).

The `.cpp` defines a `PanelLabels` namespace of bar prefix strings (`HP`, `EP`,
`RD` ring durability, `WD` weapon durability, `Abs` BD absorption, `SB` status
buildup).

### `UTurnOrderStripWidget`

`UCLASS(Abstract)`, native base for `WBP_TurnOrderStrip`. Shows the current
actor plus N upcoming turns.

- `InitialiseForCombat()` — spawns slots and binds to
  `UTurnManager::OnTurnStarted`.
- `TeardownStrip()` — unbinds and clears slot widgets.
- `PreviewCount` (`EditDefaultsOnly`, int32, default 4) — upcoming turns shown;
  total slots = `1 + PreviewCount`.
- `SlotWidgetClass` (`EditDefaultsOnly`, `TSubclassOf<UTurnOrderSlotWidget>`) —
  the slot widget class.
- `SlotContainer` (`BindWidgetOptional`, `UPanelWidget`) — typically a
  Horizontal Box holding the slots.
- Private: `Slots` (`TArray<UTurnOrderSlotWidget*>`), `CachedTurnManager`
  (`TWeakObjectPtr`), `bInitialised`, `CurrentTurnNumber`; helpers `SpawnSlots`
  and `RefreshSlots`; handler `HandleTurnStarted(AActor*, int32)`.

**Pattern.** Slots are pre-spawned once (`PreviewCount + 1` of them) at combat
start and reused; each `OnTurnStarted` only refreshes slot data from
`UTurnManager::PreviewTurnOrder`. The header notes this avoids a prior
`TransBuffer` leak caused by creating new slot widgets per turn — consistent
with the project's documented TransBuffer crash gotcha.

Each refresh also passes a **bonus-turn flag** (Emerald) to every slot: upcoming
slots read `FPreviewTurnEntry::bIsBonusTurn` (scheduled bonus turns appear inline,
rendered with a distinct tint), while **slot 0** (the current actor) reads
`UTurnManager::GetCurrentTurnIsBonus()` instead — an immediate/self-target Emerald
bonus *is* the current turn, so it never shows as an upcoming slot. On slot 0 the
bonus tint takes priority over the active-turn highlight.

### `UDefensePromptWidget`

`UCLASS(Abstract)`, native base for `WBP_DefensePrompt`. Shows the defense
window UI when the player must block/parry/dodge. Default state is hidden;
visible only during defense windows where the defender is the local player's
controlled actor.

- `InitialiseForCombat()` — binds to `UDefenseSystem` delegates.
- `TeardownPrompt()` — unbinds.
- Handlers: `HandleDefenseWindowOpened(AActor* Defender, float AttackSize,
  float WindowDuration)` and `HandleDefenseWindowClosed(AActor* Defender,
  const FDefenseResult& Result)`.
- Blueprint hooks (`BlueprintImplementableEvent`): `ShowPrompt(Defender,
  WindowDuration, AttackSize)` and `HidePrompt(Result)`.
- Private: `CachedDefenseSystem` (`TWeakObjectPtr<UDefenseSystem>`), `bBound`.

## How It Works

### CharacterPanelWidget data flow

1. `BP_CombatOrchestrator` spawns the panel as a standalone viewport widget and
   calls `InitialiseForActor(actor)`.
2. `InitialiseForActor` guards against double-init (`bBound`) and a null actor,
   clears `RF_Transactional`, finds the actor's `UCharacterDataComponent`
   (aborts with a warning if missing), and resolves the `USkillEffectManager`
   and `UStatusBuildupManager` game-instance subsystems.
3. It caches all references as `TWeakObjectPtr`s and binds delegates:
   - `UCharacterDataComponent`: `OnHPChanged`, `OnEPChanged`, `OnDied`.
   - `USkillEffectManager`: `OnEffectApplied`, `OnEffectRemoved`,
     `OnEffectDurationChanged`.
   - `UStatusBuildupManager`: `OnStatusBuildupChanged`.
   - If the actor has a `UBrokenDarknessManager`: `OnEnergyAbsorbed`,
     `OnOverloadStateChanged`, plus *(sweep-5)* `OnStacksChanged`,
     `OnAlignmentChanged`, and `OnTransformed` — the three BD-state broadcasts
     the panel routes back through `RefreshEffectsList` so the synthetic
     stack row stays in sync.
4. It calls `RefreshEPBarVisibility()` (one-time), sets `bBound = true`, calls
   `ApplyStaticText()`, then seeds initial snapshots: `HandleHPChanged`,
   `RefreshEnergyBar`, `ApplyEnergyBarTint`, a manual status-bar seed to 0
   (`SB:0/100` — because the buildup broadcast does not fire until the first
   hit), and `RefreshEffectsList`.
5. On each subsequent broadcast the handlers update the relevant bar/text. The
   subsystem delegates are global, so each handler filters by
   `Target == BoundActor` before acting.
6. `HandleEPChanged` does not write the bar directly — it defers to
   `RefreshEnergyBar`, which picks the correct energy source: if the character
   `IsBrokenDarkness()` it shows BD absorption energy (`Abs:` label) from the
   `UBrokenDarknessManager`, otherwise it shows regular EP (`EP:` label).
7. `ApplyEnergyBarTint` colours the EP bar — BD characters get the
   absorbed-element hybrid colour (or pure `BrokenDarkness` black if no
   absorption), other characters get `ElementColors::GetColorForElement` of
   their `InnateElement`.
   `RefreshEnergyBar` *(sweep-5)* additionally signals BD overload through
   `EPText` colour: when `CurrentEP > MaxEP`, the text escalates white →
   yellow (≥ 100%) → orange (≥ 110%) → red (≥ 120%) via
   `CombatConstants::OVERLOAD_YELLOW_THRESHOLD` /
   `OVERLOAD_ORANGE_THRESHOLD` / `OVERLOAD_RED_THRESHOLD`. These thresholds
   sit inside the **real overload window of `[1.00, 1.30]`** — `CurrentEP`
   is hard-capped at `MaxEP + 30%` by `OVERLOAD_CAPACITY_FRACTION` on the
   BD side, so future tweaks must keep the bands within that range or
   they become dead bands above the cap. The bar percent itself is clamped
   at 1.0; the text colour is the only visual signal that the underlying
   value has exceeded the cap. Resets to white when not overloaded.
8. `RefreshEPBarVisibility` collapses the EP bar + text for a Resonator with no
   usable EP-spend target (`HasUsableEPTarget()` false — pool dormant when
   unarmed); all other classes show it. This is evaluated once at init only.
9. Effect changes call `RefreshEffectsList`, which pulls
   `USkillEffectManager::GetActiveEffects(actor)` and passes it to the
   `RebuildEffectsList` Blueprint event.
   *(sweep-5)* `RefreshEffectsList` additionally appends a **synthetic
   `FActiveSkillEffect`** to that array when the bound character is a
   transformed BD with `GetCurrentStackCount() > 0`: `EffectType =
   StatusMultiplierBuff` (truthful — the stacks are a status-buildup
   multiplier on matching-element spells, not a damage buff), `Element =
   BDManager->GetCurrentAlignment()`, `bCanStack = true`, `CurrentStacks =
   GetCurrentStackCount()`, `MaxStacks = GetMaxStacks()` (3), `bPermanent =
   true`, `EffectName` = element display name. The BP row renders it via
   the existing `SkillEffectBlueprintLibrary` helpers
   (`GetEffectDisplayName`, `GetEffectStackString` → `"xN"` when stacks ≥ 2,
   `IsEffectBuff` → `true`) — **no separate widget, no BP changes**. Auto-
   clears: when stacks drop to 0 or alignment switches, the next refresh
   simply doesn't append the entry. The three BD handlers
   (`HandleBDStacksChanged` / `HandleBDAlignmentChanged` /
   `HandleBDTransformed`) all route through `RefreshEffectsList` to keep
   the synthetic row in sync.
10. `HandleDied` (filtered to the bound actor) calls `TeardownPanel`; the visual
    death response is left to Blueprint.
11. `NativeDestruct` and `BeginDestroy` both call `TeardownPanel` if still
    bound. `TeardownPanel` uses `.Get()` on each weak pointer before unbinding,
    then resets all handles and clears `bBound`.
12. Mouse input: `NativeOnMouseEnter`/`Leave` broadcast `OnPanelHovered`;
    `NativeOnMouseButtonDown` broadcasts `OnPanelClicked` and returns
    `FReply::Handled()`.

### TurnOrderStripWidget data flow

1. `InitialiseForCombat()` caches `UTurnManager`, calls `SpawnSlots()` to
   create `PreviewCount + 1` slot widgets in `SlotContainer`, and binds
   `HandleTurnStarted` to `UTurnManager::OnTurnStarted`.
2. Each `OnTurnStarted` broadcast invokes `HandleTurnStarted(actor, turnNumber)`,
   which calls `RefreshSlots()` to update the pre-spawned slots from
   `UTurnManager::PreviewTurnOrder` — passing each slot a bonus-turn flag
   (`bIsBonusTurn` for upcoming slots; `GetCurrentTurnIsBonus()` for slot 0). Slots
   are never recreated per turn.
3. `TeardownStrip()` (also reached from `NativeDestruct`/`BeginDestroy`)
   unbinds and clears the slots.

> Note: only the header for `UTurnOrderStripWidget` was available for this doc;
> the exact `SpawnSlots` / `RefreshSlots` implementation details are inferred
> from the header comments, not verified against the `.cpp`.

### DefensePromptWidget data flow

1. `InitialiseForCombat()` binds to `UDefenseSystem` delegates and caches the
   subsystem.
2. When a defense window opens, `HandleDefenseWindowOpened` fires; the widget
   filters to the local player's controlled actor and (when matched) calls the
   `ShowPrompt` Blueprint event so BP can show the countdown bar and button
   prompts.
3. When the window closes, `HandleDefenseWindowClosed` fires and calls
   `HidePrompt` with the `FDefenseResult`.
4. `TeardownPrompt()` (also via `NativeDestruct`/`BeginDestroy`) unbinds.

> Note: only the header for `UDefensePromptWidget` was available; the
> local-player filtering described in the header comment is not verified
> against an implementation file.

## Integration Points

### Delegates broadcast

- `UCharacterPanelWidget` broadcasts `OnPanelHovered` (bool) and
  `OnPanelClicked` (no params), both `BlueprintAssignable`.
- `UTurnOrderStripWidget` and `UDefensePromptWidget` broadcast no delegates;
  they are pure consumers.

### Delegates / subsystems consumed

- `UCharacterDataComponent` (per-actor component): `OnHPChanged`,
  `OnEPChanged`, `OnDied`; fields `CurrentHP`/`MaxHP`/`CurrentEP`/`MaxEP`,
  `CharacterData`, and queries `IsBrokenDarkness()`, `HasUsableEPTarget()`.
- `USkillEffectManager` (game-instance subsystem): `OnEffectApplied`,
  `OnEffectRemoved`, `OnEffectDurationChanged`, `GetActiveEffects(actor)`.
- `UStatusBuildupManager` (game-instance subsystem): `OnStatusBuildupChanged`
  (4-param: `Target`, `Current`, `Max`, `PendingElement`).
- `UBrokenDarknessManager` (per-actor component, optional): `OnEnergyAbsorbed`,
  `OnOverloadStateChanged`; queries `GetAbsorptionEnergy`,
  `GetMaxAbsorptionEnergy`, `GetHybridElement`.
- `UTurnManager` (subsystem): `OnTurnStarted`, `PreviewTurnOrder`.
- `UDefenseSystem`: defense-window open/closed delegates, `FDefenseResult`.
- `ElementColors` / `UHybridSpellColors` — colour lookups for the EP bar tint.

### Systems that depend on the UI System

- `BP_CombatOrchestrator` — spawns `UCharacterPanelWidget` instances and calls
  their lifecycle functions; presumed driver for the strip and defense-prompt
  widgets as well.
- Blueprint `WBP_` subclasses (`WBP_CharacterPanel`, `WBP_TurnOrderStrip`,
  `WBP_TurnOrderSlot`, `WBP_DefensePrompt`) provide layout and implement the
  `BlueprintImplementableEvent` / `BlueprintNativeEvent` hooks.

## Known Limitations / TODOs

- No literal `// TODO`, `// FIXME`, or `// HACK` markers exist in the four files
  documented.
- **Stale "HUD root" references** — `UCharacterPanelWidget::TeardownPanel`'s
  comment ("Called by HUD root or on destruct") still references the deleted
  `UCombatHUDRoot`. Panels are now standalone viewport widgets; the comment is
  out of date.
- **Resonator EP-bar visibility is init-only** — `RefreshEPBarVisibility` is
  called once from `InitialiseForActor`. The code comment explicitly states
  "runtime weapon-swap refresh is not currently wired", so a Resonator who
  equips/unequips a weapon mid-combat will not see the EP bar appear/disappear.
- *(resolved)* **Status-buildup bar tinting is wired** — `HandleStatusBuildupChanged`
  consumes `PendingElement` via `ApplyStatusBarTint`
  (`CharacterPanelWidget.cpp:280-312`): tints the bar per pending-cap element;
  BD attackers darken via `UHybridSpellColors::GetHybridSpellColors().BlendedColor`;
  `None` / `Generic` (physical-only, non-elemental damage) tints the **neutral brown**
  `ElementColors::GetColorForElement(None)` — matching every other surface (the old
  white early-return was changed to brown in the `Generic→None` colour-parity pass).
- **BD-without-manager fallback** — `RefreshEnergyBar` and `ApplyEnergyBarTint`
  both handle a Broken-Darkness character that has no `UBrokenDarknessManager`
  by showing an empty bar / pure black tint; this is a defensive fallback for
  an arguably invalid state rather than an intended path.
- **Coverage gap in this doc** — only headers were available for
  `UTurnOrderStripWidget` and `UDefensePromptWidget`; their `.cpp`
  implementations were not read, so behavioural detail for those two widgets is
  inferred from header comments and may be incomplete.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-28 | Sweep-5 — `CharacterPanelWidget` now binds the three BD-state broadcasts (`OnStacksChanged`/`OnAlignmentChanged`/`OnTransformed`), surfaces absorption stacks via a synthetic `StatusMultiplierBuff` row injected into `RefreshEffectsList` (no separate widget), and colours `EPText` for BD overload (yellow/orange/red at 100/110/120% within the `[1.00, 1.30]` cap). Also documented `ApplyStatusBarTint` (previously flagged as pending) — status bar now tints per pending-cap element, with BD attackers darkened. | feature/integration-gaps-sweep-5 |
