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

**New battle HUD (B-391, `wor.UI.NewHUD` 1, the default; in dev, built 2026-11-12).**
`UCombatHUD::Build` reads the cvar at each fight. At 1 it builds, with no WBP:
- `UBattleHUDWidget` (`UI/Combat/Battle/`): one full-screen plain C++ widget. Party cards bottom-left
  (name, HP bar, "HP x / y", "EP x / y", status chips), enemy cards top-right (name, HP bar without numbers,
  status chips), the turn order down the left edge (`BattleHUDView::ReadTurnStrip`: the replicated turn state,
  else the TurnManager), and a Statuses button that opens every status in full. `Initialise` binds and fills
  plain-data state before `AddToViewport`; `RebuildWidget` builds one card per combatant once; delegates only
  rewrite texts (no Tick, no widgets recreated).
  B-391b: floating damage numbers from each card character's `UCombatPresentationComponent::OnHitDetailShown`
  (the server's hits and heals, on every machine), styled by `DamageNumberView` (Q-111 / Q-391b), from a pool of
  16 text blocks built once and stepped by one timer that stops when none are live; a Broken Darkness card's EP
  line is "Absorb x / y" with its overload. Debug: `wor.UI.DamageNumber <amount> [type|Heal] [mult]`.
  B-391c: every card has a status buildup bar under its HP bar, fed as the old panel's (`UStatusBuildupManager::
  OnStatusBuildupChanged` on the server, the replicated `UStatusBarStateComponent::OnBarChanged` on a guest), tinted
  by `BattleHUDView::GetBuildupColour`, with Q-111 A's cracks (a row of `CRACK_COUNT` marks over the bar, built once,
  shown past each `GetCrackPositions` point); party cards add "Status x / y", a "weapon · ring" line and a loadout
  hover tooltip (`BattleHUDView::ReadGearNames`; refreshed on `ULoadoutComponent::OnLoadoutChanged` /
  `OnActiveWeaponChanged`; party card frames take the mouse for it). A 0 hit on Broken Darkness floats "Negated"
  (`DamageNumberView::SHOW_NEGATED_CUE`, Q-391c).
- `UBattleCommandMenuWidget`: a view of `UCombatCommandMenuSubsystem` (`OnCommandMenuReady` / `Closed`), a pool
  of 16 rows built once, each row's key hint from `ACombatPlayerController::GetMenuKeys`
  (`BattleHUDView::AssignMenuKeys`); a click goes to `HandleSelection` through `UBattleMenuRowRelay`.
  B-391c: under the title, the old `UDurabilityHeaderWidget`'s line ("AD x / y", "WD x / y";
  `BattleHUDView::ReadDurability` / `MakeDurabilityLines`), refreshed per menu open and on `UCrystalManager`'s
  `OnCrystalDurabilityChanged` / `OnCrystalBroken` for the acting character.
- the native `UDefensePromptWidget`.
At 0 it builds the old WBP panels, strip and menu described below (the fallback until Crown's PIE pass).
The main-menu category keys (Attack, Skills, Spells, Items, Flee, switches; Q-391) press their button through
`UCombatCommandMenuSubsystem::PressMenuKey` with either HUD. `wor.UI.Print` prints both new widgets.
**Action pips (B-170e, Q-170e2; only when the fight runs `wor.Turns.ActionPips` 1, `ActionPipsText::IsActionPipsFight`):**
party cards get "Actions ●●○ 2 · Max 3 (+75/100)" under EP (`ActionPipsText::MakeActionsLine`), bound to each party
character's `OnActionsChanged` and refreshed at turn start; the subsystem adds an **End Turn** button
(`EPieMenuCategory::EndTurn` → `EActionType::EndTurn` through `SubmitConfirmedAction`, main menu only; no key yet,
Crown 2026-11-12; hidden when the player sets `wor.UI.HideEndTurn 1`, `ActionPipsText::ShouldShowEndTurnButton`) and `ShowActionCost` labels every Attack / ability / spell / item button with its cost
and greys it with `ActionPipsText::GetCostRefusedReason`. Both HUDs draw the subsystem's buttons; only the new one has
the pips line.

**Not covered here:** the hub shop UI (`UShopWindowWidget` /
`UShopRowWidget` and their `WBP_ShopWindow` / `WBP_StockRow` / `WBP_CartRow`
Blueprints, plus the `UMerchantShopSubsystem` window-lifecycle owner) lives in
[`MerchantShopSystem.md`](./MerchantShopSystem.md) with the rest of the
merchant pipeline. It follows the same authoring rule (all logic in the C++
base, WBP layout-only via `BindWidgetOptional`) but is hub/modal UI, not
combat HUD.

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
7. `ApplyEnergyBarTint` colours the EP bar — BD characters get the **active-pool**
   hybrid colour, `UHybridSpellColors::GetHybridSpellColors(GetActivePool()).BlendedColor`:
   the base Darkness pool → pure BD near-black, an absorbed Fire → dark red, Wind → dark
   green, etc. (the Darkness case routes through the same path — no special-case). Other
   characters get `ElementColors::GetColorForElement` of their `InnateElement`. The bar
   **re-tints on active-pool rotation** — `HandleBDAlignmentChanged` calls `ApplyEnergyBarTint`
   so absorbing a new element updates the bar colour live (Dark Fire → absorb Wind → Dark Wind).
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

- **New battle HUD (B-391):** placeholder looks (text chips, no icons); picking inside a submenu or a target list
  is a click (gamepad navigation there comes with CommonUI, D-024); damage numbers and the Broken Darkness
  absorption line came in B-391b, the buildup bar, gear and durability header in B-391c. The old WBP widgets stay
  until Crown's PIE pass of B-391.
- **No cards or numbers for combatants that join mid-fight (B-391c).** The new HUD binds the combatants
  `UCombatHUD` passes at fight start. Nothing in the code adds a combatant mid-fight or announces one (no summon
  system; no orchestrator or TurnManager join event), so there is nothing to bind to. When summons land, they need a
  join event, and the HUD a spare card pool built once (TransBuffer rule).
- **Buildup bar (B-391c):** it cracks (Q-111 A) but doesn't shatter or show shards during a break yet (art pass).
  Its seed at Initialise has no attacker, so a bar part-filled by Broken Darkness before the HUD exists shows the
  plain element colour until the next hit. Gear and the durability header follow server-side loadout and crystal
  events, as the old HUD: a guest refreshes them on its next menu open.
- **"Negated" is inferred (B-391c):** `FCombatHitShown` doesn't say why a hit took 0; any 0 hit on a Broken
  Darkness target reads "Negated" (a ward stopping a whole hit included).
- **Two Broken Darkness checks (B-391c review):** the "Negated" cue reads the target's
  `UCharacterDataComponent::IsBrokenDarkness()` (replicated flag); the buildup tint reads the attacker's
  `UBrokenDarknessManager::IsInBDForm()` (the manager's flag, as the old panel's `ApplyStatusBarTint`). Each matches
  its old source, but they can disagree for a born-in-BD character before the manager mirrors the flag
  (`InitializeBornBrokenDarkness`), or on a client if the manager's flag isn't replicated.

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
| 2026-06-21 | EP/Absorb bar tints to the BD **active-pool** hybrid colour (`GetHybridSpellColors(GetActivePool()).BlendedColor`) — dropped the redundant Darkness special-case (it routes to the same near-black) so the EP bar matches `ApplyStatusBarTint`. `HandleBDAlignmentChanged` now calls `ApplyEnergyBarTint`, so the bar re-tints live on active-pool rotation (was only re-tinting incidentally via the co-firing `OnEnergyAbsorbed`). Updated §7. | feature/bd-value-deletion |
| 2026-07-11 | Scope note added: the new hub shop UI (`UShopWindowWidget`/`UShopRowWidget` + WBPs, `UMerchantShopSubsystem`) is documented in `MerchantShopSystem.md`, not here — this doc stays combat-HUD-only. | feature/hub-merchants |
| 2026-10-25 | **BP-logic → C++ overhaul (unverified — not yet compiled/PIE'd).** New `UCombatHUD` (Config=Game UObject owned by the orchestrator) builds panels / turn strip / command menu / defense prompt as standalone viewport widgets; `OnCombatStartedUI` is now a `BlueprintNativeEvent` whose native body builds it (BP override still wins until deleted). `RebuildEffectsList`, `InitialiseSlot` → NativeEvents with C++ defaults (pooled effect rows; slot name/number/bonus/active). `ApplyStaticText` native fills class/element + world stats. `UDefensePromptWidget` implemented (binds DefenseSystem cue/open/close, native text, no WBP needed). New `UCombatResultsWidget` shown by `ABattleGameMode` before `ExitEncounter`. Debug: `wor.UI.Print`. Cut-over steps: `docs/Refactor/BPLogicToCpp_ReviewNotes.md`. | refactor/bp-logic-to-cpp |
| 2026-10-26 | Command menu: new `EPieMenuCategory::Defend` (appended last). The main menu always offers **Defend**, which submits a target-less Defend and relies on `HandleActionExecuted` to close the menu, because Defend runs synchronously. A **stunned** actor's main menu is only Attack + Defend (D-001), with no infusion controls and no infusion stamp (D-023 A, `IsInfusionLockedForPendingAction`). `ActionExecutor` enforces the same rule. | fix/B-092-stunned-menu-defend |
| 2026-10-26 | UI rebuild pipeline (D-024/D-025): mockup tool `Tools/ui_drafts/`, ported from the deckbuilder. It reads the kit from `WOR_UI_KIT` and writes plain + annotated 1280×720 drafts to `docs/Design/screen_mockups/<area>/<screen>/vN/` (LFS, versions frozen). New screens are drafted there and approved by Crown before they're built in CommonUI. | feature/B-100-mockup-tool |
| 2026-10-26 | Combat HUD mockup v1 (B-101, screens 31–40): `combat/hud/v1/` in four states, `command`, `target`, `hit` and `stunned`. Layout: a vertical turn strip on the left, a floating enemy plate, party cards bottom-right with BD stacks and the overload gauge, and the command diamond (Defend included; Attack + Defend only when stunned). It waits for Crown approval (B-102) before any CommonUI build. `kit.nine_slice` was added for framed panels. | feature/B-101-combat-hud-mockup |
| 2026-10-26 | **B-003 cut-over.** The `BP_CombatOrchestrator` OnCombatStartedUI, `WBP_CharacterPanel` RebuildEffectsList and `WBP_TurnOrderSlot` InitialiseSlot overrides are deleted, so `UCombatHUD` builds the HUD and the native panel effects list / turn slot run. `UCombatHUD` soft paths corrected to `/Game/UI/UMG/Combat/...`. `UTurnOrderSlotWidget` gains an optional `BackgroundBorder` (bonus green / active dark / waiting light, the WBP's values). The win path keeps the HUD up behind the results (B-002 choice; the old BP removed it). Hub controller not reparented (D-076). | chore/B-003-bp-cpp-cutover |
| 2026-10-26 | **B-110:** confirming a target no longer calls `Close()` after `SubmitConfirmedAction`; that closed the menu a refused action had just reopened (the orchestrator re-requests the turn synchronously). `SubmitConfirmedAction` now closes only while the action is still running (`ACombatOrchestrator::IsWaitingForAsyncAction`, new getter) or when its actor no longer has the turn (combat ended, submission dropped); a finished instant action is closed by `HandleActionExecuted`. Spec `WoR.UI.CommandMenu.Submit`. | fix/B-110-menu-close-after-refusal |
| 2026-10-27 | **B-103 CommonUI base layer.** `UWoRRootLayout` gains `EWoRUILayer` (Game / Menu / Modal), `PushToLayer` / `PopLayer` / `GetLayerStack` / `GetRootLayoutString`; `ModalLayer` is `BindWidgetOptional` (TODO: add it to the root layout WBP in the editor; until then modal pushes go to MenuLayer with a warning). `UWoRButtonBase` (`UCommonButtonBase`): `EWoRButtonJob` Play / Confirm / Danger / Navigate, `JobStyles` picks each job's `UCommonButtonStyle` (authored Style for a job without one). `UWoRTestScreen`: C++-built empty screen, Back closes it; `wor.UI.TestScreen [Game|Menu|Modal]`. Keyboard + gamepad routing is `UWoRActivatableWidget::GetDesiredInputConfig` (Menu mode) plus CommonInput's Enhanced Input support (`DefaultGame.ini`); Back needs `BP_CommonInputData` to name a back action. Spec `WoR.UI.Stack`. | feature/B-103-commonui-base |
| 2026-10-27 | Combat HUD mockup v2 (B-111): `combat/hud/v2/` (`header_candidates`, `command`, `hit`, `stunned`, `status_bars`), script `Tools/ui_drafts/combat_hud_v2.py`. Crystal pass (faceted tube bars, crystal-cut corners, element gems), five header-bar candidates A-E (D-017), BD EP as `N (xS)` with overload on the EP bar, damage-number resistance gradient, cracking / shattering status bars, stun chip `Stunned · 1 turn` (D-090), element gem strip with a drawn Reality gem. `kit.vendor_piece` reads vendor originals the library lacks from the deckbuilder's Godot import cache (read-only). B-102 is back with Crown. | feature/B-111-hud-mockup-v2 |
| 2026-10-27 | B-205: `UCombatHUD`'s widget classes and `AExplorationPlayerController::DefaultInteractAction` (new `Config` soft ref, used when the BP leaves `InteractAction` empty) are set in `Config/DefaultGame.ini`, no longer `/Game/` literals in code; an unresolved one logs an error. `FallbackCharacterPanelClass` is left empty (its asset is gone; TODO remove after PIE). Spec `WoR.Config.AssetPaths` checks each path exists. B-118's cvar removal still waits for PIE. | chore/B-205-config-asset-paths |
| 2026-10-27 | B-126: `WoRGameNames` (`Public/WoRGameNames.h`), the D-062 LOCTEXT name table: `GetElementName`, `GetSkillEffectTypeName`, `GetCharacterClassName`, one case per enum value (generated from the UMETA DisplayNames), so names survive cooking. Reality reads **Unknown** (D-095). Read by `SkillEffectDisplayNames` (fallback), `USpellData` (element prefix), `UCharacterPanelWidget` (class / element label, BD stack). Spec `WoR.Text.GameNames` fails when a new enum value has no case. Remaining enum-metadata labels: B-207. | fix/B-126-cooked-name-table |
| 2026-10-27 | B-207: `WoRGameNames` also names `EDefenseType` (legacy Parry reads Guard), `ECombatRow`, `ESpellSchool`, `EMerchantType`. Read by `UDefensePromptWidget` (Guard! / Dodge!), `UShopWindowWidget` (element, school, merchant; the gem element comes from `CrystalTypeHelpers::GetElement`, none shown for Quartz), `UPartyControlWidget` (row), `FSkillEffect::DescribePayload`, the loadout tier-cap Removed message (`ItemIdentity` names) and `AMerchantInteractable::GetHintText`. Remaining `GetDisplayValueAsText` uses are log/debug only. |
| 2026-10-27 | B-093 (Q-093): menu keys. `UCombatCommandMenuSubsystem::PressMenuKey(Category)` presses the enabled button the menu would show now (Defend: main menu or submenu; CycleSource / CycleLevel: the target picker) through `HandleSelection`. `ACombatPlayerController::BuildMenuKeys` builds three Boolean `UInputAction`s (`IA_Combat_MenuDefend` / `InfusionSource` / `InfusionLevel`, G / R / T, pad Y / LB / RB) in `IMC_Combat_MenuKeys` at runtime, priority 1, `bConsumeInput = false`. `wor.Input.MenuKeys`. Spec `WoR.UI.CommandMenu.Keys`. | feature/B-093-menu-keys | fix/B-207-cooked-labels |
| 2026-10-27 | B-034: the command menu owns the pending infusion level (`GetPendingInfusionLevel`, BlueprintPure); CycleLevel, the picker labels, the resets and the action stamp use it instead of reading the VFX component. | refactor/B-034-infusion-level-on-pending-action |
| 2026-11-02 | B-254 (A11): the command menu's infusion controls (level cycle, source cycle, the `<source>: L<n>` label) are one helper used by the single-target and group pickers (two ~75-line copies before). | refactor/B-254-infusion-controls |
| 2026-11-02 | B-256 (A16 / A17): `WoRInteract::FindNearestOverlapping<T>` replaces the interact actors' copied nearest-overlap loops; debug prints share `GetDebugOpponent`. The hint actor base is B-256b. | refactor/B-256-interact-debug-repeats |
| 2026-11-02 | B-042ye: `UCharacterPanelWidget::RefreshEffectsList` lists `GetHeldEffects` (what GAS holds at `wor.Effects.UseGAS 1`; clients keep the stored list, Q-042ye). | refactor/B-042ye-gas-effect-reads |
| 2026-11-03 | B-256b (A16): `WoRInteract::ShowHint` / `HideHint` (one `HINT_DURATION`, key = owner's unique id) replace the copied on-screen hint code in `TrialDoor`, `MerchantInteractable`, `TrialMapFeature` and `UPartyInteractionComponent`; same colours and texts. | refactor/B-256b-interact-hint |
| 2026-11-03 | B-294 (D-106, Q-294): `ElementColors` holds the tuned colour-blind palette (Darkness near-black grey, Void light violet, Reality dark indigo, new `Ice`, `Physical`; `NeedsTextOutline` / `TextOutline` for Darkness text); the element table's Ice and Physical rows read them; the obsolete `GetDisplayInfo` reads the table. `ElementColorVision` (Machado simulation, CIEDE2000) backs spec `WoR.Elements.Palette` and `wor.Elements.Palette` / `UElementColorDebugComponent::GetElementPaletteString`. Draft `combat/hud/v3`. Gems and the markers setting: B-294b. | feature/B-294-element-palette |
| 2026-11-03 | B-325 (D-118): `UCharacterPanelWidget`'s live class line ends with "Tier X" (Combat Tier, `FText`), refreshed on `OnLoadoutChanged` / `OnActiveWeaponChanged`. Unbuilt (cloud). | feature/B-325-combat-tier |
| 2026-11-03 | B-316 (D-111 buy-for, Q-316): the shop window's placeholder buy-for verb. `UShopWindowWidget::HandleBuyForClicked` cycles the receiver (yourself, then each other party member, AI included; `BUY_FOR_MIN_PARTY`); optional `BuyForButton` / `BuyForText` (BindWidgetOptional, the WBP can add them in the editor) and the console placeholder `wor.Shop.BuyFor`; "Buying for: yourself" uses `WoRGameNames::GetYourselfWord` (D-062); Confirm calls `UEconomyService::PurchaseFor`. Folds into B-339's one inventory screen (D-131). Written unbuilt in a cloud session. | feature/B-316-give-request-buyfor |
| 2026-11-03 | B-339 (D-131): one placeholder inventory screen. `WoRInventoryVerbs` (Inventory/InventoryVerbs.h) lists a member's gear and pouch, then the chest's gear and stored stacks, says which verbs apply (`GetVerbsFor`) and runs them (`Run`: Give / Store / Take through `WoRGive` and the pool's storage moves, Dismantle / Convert through `UEconomyService` on the member's spawned actor). `UInventoryWidget` (`wor.UI.Inventory`): character / item / receiver cycles, one button per verb enabled where it applies, a whole-stack toggle; `wor.Inventory.PrintVerbs`. `UGiveWidget`, `UStorageWidget`, `UDismantleWidget`, `UDuplicateGearWidget` flagged OBSOLETE (B-339). Spec `WoR.Inventory.Verbs`. Written unbuilt in a cloud session. | feature/B-339-inventory-screen |
| 2026-11-04 | B-356c (D-146 corrected): the spell menu's school buttons read Conjuration / Enhancement / Restoration from the LOCTEXT name table (`WoRGameNames::GetSpellSchoolName`; a Destruction-tagged spell shows and groups as Conjuration). `EPieMenuSpellSchool::Destruction` is OBSOLETE (no spell maps to it; `ToSpellSchool` reads it as Conjuration); the Conjuration button takes the damage tint. | feature/B-356c-conjuration-damage-school |
| 2026-11-04 | B-339 review fixes: `WoRInventoryVerbs::GetRefusal` (worn gear not stored, no give to yourself / of a primary / of gear to a recruit, no dismantle or convert of home gear in a run or of a primary) drives which verb buttons are on and Run's reason; D-016 confirm restored (`GetConfirmText`, second press); verb names in `WoRGameNames::GetInventoryVerbName`. Built and tested on the desktop 2026-11-04 (`WoR.Inventory` green); Convert's confirm carries D-016's equipped warning; the five as-built choices are Q-339. | feature/B-339-inventory-screen |
| 2026-11-04 | B-158 (D-076 = A): one hub controller. `AWoRHubController` binds `IA_Interact` (`InteractAction`, else the `Config` `DefaultInteractAction`, B-205) to `HubInteract::InteractWithNearest` (nearest door / merchant / recruit / map feature; shared with the retired `AExplorationPlayerController`); `GetInteractionStateString` and `NativeUIDebug::GetNativeUIStateString` print the candidates. Editor step: delete the Blueprint's IA_Interact nodes (`docs/PIE/B-158.md`). | feature/B-158-one-hub-controller |
| 2026-11-04 | B-356b: `FPieMenuButtonData::MakeSchoolButton` sets ID and tint only; `BuildSchoolButtons` owns the name and count from the LOCTEXT table (D-061). | feature/B-356b-three-schools-part2 |
| 2026-11-09 | B-396: `UCombatHUD::GetHUDStateString` (`wor.UI.Print`) prints each HUD widget's in-viewport, visibility, opacity, drawn position and size, and the game viewport size (Crown's invisible-HUD report). `UTrialGateWidget::Open` sets the gate focusable before UIOnly focuses it. | fix/B-396-combat-hud-visibility |
| 2026-11-09 | B-391 (Q-391): new plain C++ battle HUD behind `wor.UI.NewHUD` (default 1; 0 = the old WBP HUD): `UBattleHUDWidget` (party / enemy cards, turn order, status chips and a view-all panel), `UBattleCommandMenuWidget` (the subsystem's buttons with key hints), `BattleHUDView` text helpers; main-menu category keys through `PressMenuKey` (`ACombatPlayerController::GetMenuKeys`); `wor.UI.Print` prints both widgets. Spec `WoR.UI.BattleHUD`. Damage numbers: B-391b. | feature/B-391-battle-hud-cloud (UNBUILT) |
| 2026-11-09 | B-170e (Q-170e2): action pips on the new HUD behind `wor.Turns.ActionPips`: party-card pips and Max Actions line (`ActionPipsText`, `OnActionsChanged`), End Turn button and key (Backspace / View), action costs on every skill / spell / item, greyed with the reason; Defend's tooltip; `wor.UI.Print` shows `actionPips` and the lines. Specs `WoR.Combat.ActionPipsText`, BattleHUD / CommandMenu.Keys cases. | feature/B-170e-action-pips-ui-cloud (UNBUILT) |
| 2026-11-12 | B-170e (Crown, 2026-11-12): the End Turn row has no key (it comes later); the main menu hides it when the player set `wor.UI.HideEndTurn 1` (`ActionPipsText::ShouldShowEndTurnButton`, saved in `FSavedSettings`). | feature/B-170e-action-pips-ui-cloud |
| 2026-11-12 | B-391 desktop build (`feature/B-391-battle-hud`): `UBattleHUDButton` (`UI/Combat/Battle/BattleHUDButton.h`), a never-focusable `UButton` for the menu rows and the Statuses button (`UButton::InitIsFocusable` is protected in 5.8); the category keys act only when `UCombatHUD::WantsNewHUD()` (the old menu has its own A / B). |
| 2026-11-09 | B-400b: `UBattleCommandMenuWidget` rows bind `OnHovered` (`UBattleMenuRowRelay::HandleHovered`); `HoverRow` highlights a per-actor target row's enemy (`UCombatCommandMenuSubsystem::HighlightTarget`, B-400's camera follows); `GetRowTarget` for debug / specs. Rows stay non-focusable (B-391). | `feature/B-400b-new-hud-target-highlight` (built 2026-11-12) |
| 2026-11-09 | B-391b (Q-391b): damage numbers. `UCombatPresentationComponent` hits carry `FCombatHitShown` (element multiplier, damage type, heal; `Multicast_ShowHit` RPC changed, new `OnHitDetailShown`, `SendHeal` from `USkillEffectManager::BroadcastHealing`); `DamageNumberView` (colour by damage type, Physical white; size / brightness by multiplier; immune grey; heals green); `UBattleHUDWidget` floats them from a pooled set of text blocks on one timer (no Tick) and shows a Broken Darkness card's "Absorb x / y, overload z%" (`BattleHUDView::FormatAbsorbLine` / `GetOverloadColour`). `wor.UI.DamageNumber`; specs `WoR.UI.DamageNumber`, `WoR.UI.BattleHUD`, `WoR.Net.Presentation`. Buildup bar, weapon name, durability header: B-391c. | feature/B-391b-hud-pieces-cloud (UNBUILT) |
| 2026-11-12 | B-391b built on the desktop (`feature/B-391b-hud-pieces`): merged with B-170e's action-pips card code (both kept: the absorb line and tint, then the actions line). Full suite 1802/0; wor-reviewer no must-fix. Left: Teardown skips the number timer clear when unbound (no numbers can start unbound); a mid-fight summon gets no card or numbers (B-391c); a widget rebuild would orphan the number text blocks, as the cards. | `feature/B-391b-hud-pieces` |
| 2026-11-09 | B-391c (Q-391c): the new HUD gets the old HUD's missing pieces. `UBattleHUDWidget`: a status buildup bar on every card (manager / replicated bar, old-panel tint, Q-111 A cracks past half; "Status x / y" on party cards), a party "weapon · ring" line and loadout hover tooltip (party card frames now hit-testable), "Absorbed" over a 0 hit on Broken Darkness (`DamageNumberView::SHOW_ABSORB_CUE`, new `EDamageNumberTier::Absorbed`). `UBattleCommandMenuWidget`: the durability header ("AD" / "WD", old slot rule; a Generic / Caster ring reads its primary ring). Helpers in `BattleHUDView` (buildup, cracks, gear, durability). `wor.UI.Print` prints them; spec `WoR.UI.BattleHUD`. Mid-fight joiners: a known limitation (no join event exists). | feature/B-391c-hud-pieces-2-cloud (UNBUILT) |
| 2026-11-12 | B-391c (Crown, Q-391c): the 0-hit cue on Broken Darkness reads **"Negated"**, not "Absorbed" (which reads as Broken Darkness's absorption); on by default. Names follow: `EDamageNumberTier::Negated`, `SHOW_NEGATED_CUE`, `NEGATED_SCALE`, `IsNegatedHit`, `GetNegatedLabel`, `MakeNegatedStyle`. | feature/B-391c-hud-pieces-2-cloud |
