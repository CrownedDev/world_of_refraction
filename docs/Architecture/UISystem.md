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

## Menu screens (CommonUI, from `Design/UIScreens.md`)

The screens outside the battle HUD are CommonUI screens (`UWoRActivatableWidget`) pushed onto `UWoRRootLayout`'s menu layer, built in C++ from the approved mockups, one job each (D-181). Each has a **model** with no widget in it (what it shows, what each choice does) that the headless specs read, and a **widget** that draws it.

### The game menu (B-439, UIScreens #1)

- **Model:** `WoRPauseMenu` (`UI/Menus/PauseMenuModel.h`): `BuildPages` (the eight pages, each with its side-panel text and, when it can't open, why), `BuildHeader` (the hub's Gold and essences, or a run's Prisms, Gear Essence and wipes left), `BuildParty` (name, class line, HP, EP, pillars, Combat Tier when spawned), `GetQuitWarning`, `Quit`.
- **Widget:** `UPauseMenuWidget` (`UI/Menus/PauseMenuWidget.h`). One selection, moved by keys or the mouse; rows are plain borders and the screen itself takes the keys (`InputMap.md`, Menus). Party cards are built once and refilled on each open.
- **Opening:** `IA_ToggleMenu` → `AWoRHubController::OpenGameMenu` → `UPauseMenuWidget::Open` (refused while any screen is up). Console `wor.UI.Pause`. Hub and trial map; a battle runs on another controller.
- **Loadout** pushes the loadout screen over the menu (B-440a); Back returns to the menu on that row.
- **Inventory, Character, Journal and Settings** push their screens over the menu the same way (B-441, B-442, B-463, B-452).
- **Pages without a CommonUI screen yet:** Party closes the menu and opens its old viewport placeholder (B-458).
- **Quit (D-176):** the first press asks, then a 2 second hold (`QUIT_HOLD_SECONDS`). In a run: `UTrialRunSubsystem::AbandonRun` (the end pays nothing, no Mentor Tokens; back to the hub). Outside one: saves and closes the game until the title screen exists. A co-op guest in a run is refused (only the host ends it).
- **Nothing pauses** (Q-439).
- **Debug:** `wor.UI.PrintPause` (`GetPauseMenuString`: header, pages, party).

### The settings screen (B-452, UIScreens #8)

- **Model:** `WoRSettingsScreen` (`UI/Menus/SettingsScreenModel.h`): six pages (`ESettingsPage`); `BuildRows(World, Page)` lists every approved row (D-190) with its value, its plain-words explanation and, when the setting isn't built, why it can't change; `ChangeSetting(World, Id, Direction)` steps a setting; `BuildKeyRows` is the Controls page (the built keys of `InputMap.md`, by section); `SaveChanges` writes the save file.
- **What works:** Turn View (`USaveSubsystem::SetTurnViewStyle`, cycled through `ETurnViewStyle`) and the End Turn button (`SetEndTurnButtonHidden`). The rest say "not built yet": the eight new settings are B-453, the battle control model B-436l; Camera, Display and Audio have no rows yet.
- **Widget:** `USettingsScreenWidget` (`UI/Menus/SettingsScreenWidget.h`): page tabs (LB / RB), rows with their value, a side panel explaining the focused row, a scrolling key list on Controls. A change is live at once; the save file gets it when the screen closes (a full save, as a rest spot's). Opened by the game menu's Settings page or `wor.UI.Settings`.
- **Debug:** `wor.UI.PrintSettings` (`GetSettingsString`).

### The hub HUD: interact prompt and money chip (B-443, UIScreens #14)

- **Prompts:** each interactable answers for itself with an `FInteractPrompt` (`Hub/InteractPrompt.h`: a verb, `EInteractVerb`, and what it acts on): `ATrialMapFeature::GetPrompt` (none by default, so a memory spot stays unannounced; overridden by the chest, lore object, arena, rest spot, Prism pickup, journal station and memory fragment), `UPartyInteractionComponent::GetPrompt`, and doors and vendors in `HubInteract::GatherCandidates`, which now gives each candidate its prompt, its place and whether it is a vendor. A vendor is named by role (D-178); the arena names the opponent, never a rank (D-187). The verbs' words are `WoRGameNames::GetInteractVerbName`.
- **Model:** `WoRHubHUD` (`UI/Hub/HubHUDModel.h`): `BuildPrompt(Pawn)` is the nearest candidate's prompt (the one the interact key acts on; a nearest thing with no prompt shows none), `GetInteractKeyLabel` ("X" on a gamepad, "E" on the keyboard), `BuildMoney` (the place's money: Gold in the hub, Prisms in a run), `ShouldShowMoney` (near a vendor, or for `MONEY_SHOWN_SECONDS` after it changed).
- **Widget:** `UHubHUDWidget` (`UI/Hub/HubHUDWidget.h`), a plain viewport widget made by `AWoRHubController::BeginPlay` for the local player. A 0.1 s timer reads what is in reach (each kind's overlap search; no all-actors walk); `NativeTick` only keeps a shown prompt over its world spot as the camera moves. It follows the wallet in use and listens to `OnCurrencyChanged`. The root layout hides it, with the party strip, while a menu screen is up.
- **Debug:** `wor.UI.PrintHubHUD` (`GetHubHUDString`).

### The character sheet (B-442, UIScreens #5)

- **Model:** `WoRCharacterSheet` (`UI/Menus/CharacterSheetModel.h`): `BuildPage(World, MemberIndex)`: who they are (the game menu's party read), Max Actions with action-point progress and regain, `BuildPillars` (Mind, Body, Spirit with their sub-stats in the grouping of CL-39), `BuildResistances` (D-168: All = the Defence line, Physical, Spell, then each element), the curses held (held by the player, so on every sheet). A member that isn't spawned has no stats, with the reason.
- **The numbers (Q-442):** each sub-stat is the value the fight uses now, read from the fight's own getters where the stat has one (they are GAS-aware) and from `GetStatLayers(...).Compose()` otherwise; a multiplier reads "x1.48", a share "9%". Each resistance line carries its cap (Defence's ceiling; `WoRDamageMath::ELEMENT_DAMAGE_RESIST_CAP` for the scope and element lines) and its bar is full there.
- **Widget:** `UCharacterSheetWidget` (`UI/Menus/CharacterSheetWidget.h`): character tabs (LT / RT), three panels built once and refilled. Opened by the game menu's Character page or `wor.UI.Sheet`.
- **Names:** `WoRGameNames::GetSubStatName`.
- **Debug:** `wor.UI.PrintSheet` (`GetCharacterSheetString`).

### The loadout screen (B-440, UIScreens #2, #3)

Built in pieces, one at a time with Crown's PIE review between: **B-440a the gear page, read only (built)**, B-440b change gear, B-440c the socket picker, B-440d the Skills page, B-440e saved loadouts.

- **Model:** `WoRLoadoutScreen` (`UI/Menus/LoadoutScreenModel.h`): `BuildMembers` (the character tabs) and `BuildGearPage`: the evolution seat, each weapon and adapter the fight reads (`FCombatLoadout::RebuildSlotsFromLegacy`, so never stale), then apparel. Each row has its name, kind line (the wield mode in it, D-180), tier, sockets (crystal or evolution; stone), cost (`FLoadoutRules::SlotCost`) spelled as frame + attachment (Q-357), detail lines and a note; the page has the class's budget (`LoadoutClassTable`), what is spent, and the saved loadout's title. A member that isn't spawned in the level has no page, with the reason.
- **Widget:** `ULoadoutV2ScreenWidget` (`UI/Menus/LoadoutV2ScreenWidget.h`; see the v2 changelog lines), opened by the game menu's Loadout page, the gate screen's loadout key, or `wor.UI.LoadoutV2`. The first widget, `ULoadoutScreenWidget` (and `wor.UI.Loadout`), was **retired by B-488** (Crown); this model stays, the v2 screen reads it.
- **Names:** `WoRGameNames::GetCrystalTypeName`, `GetWeaponTypeName`, `GetWieldModeName`, `GetTierLetter` (D-062); the `FString` tables stay for logs and the old widgets.
- **Shared look:** `Private/UI/Menus/MenuScreenStyle.h` (`WoRMenuStyle`): the colours, font sizes, menu keys and small builders every C++-built menu screen uses.
- **Debug:** `wor.UI.PrintLoadout` (`GetLoadoutScreenString`: every member's rows, sockets and costs).

### Key hints: the device in use (B-481)

Every key a code-built menu names (a footer, a tab row's key label, a hold hint, the hub prompt) goes through one helper, `WoRKeyHints` (`Public/UI/WoRKeyHintText.h`).

- **A hint is a pattern** with key names in braces: `"{Confirm}  Select        {Back}  Back"`. `WoRKeyHints::GetKeyNames(bGamepad)` is the one table (gamepad / keyboard): Confirm A / Enter, Back B / **Backspace**, Toggle X / X, Second Y / R, Tabs "LB / RB" / "Q / E" (TabPrev, TabNext), SubTabs "LT / RT" / "Z / C" (SubTabPrev, SubTabNext), SetActive L3 / 6, Arrows D-pad / Arrows, Interact X / E, ViewButton View / Tab. `Format(Pattern, bGamepad, Extra)` fills them.
- **The device** is CommonUI's: `IsGamepad(Widget)` reads `UCommonInputSubsystem::GetCurrentInputType()` of the widget's local player (keyboard when there is none, as in headless specs).
- **Live switch:** `UWoRKeyHintText` (a `UTextBlock`) keeps its pattern and shows it again on `OnInputMethodChangedNative`. A screen makes one with `WoRMenuStyle::MakeHintText` and fills it with `WoRKeyHints::SetHint(Text, Pattern)`; `MakeKeyLabel` takes a pattern too. No screen has device code of its own.
- **Mixed patterns:** a pattern that also carries the caller's own value takes it by name (`FText::FormatNamed(..., TEXT("Verb"), Verb)`), then the hint pass fills the keys. A numbered argument (`{0}`) would fill the first key name's place.
- **Key caps (B-483):** a footer is made with `WoRMenuStyle::MakeHintBar` (and a tab row's key label with `MakeKeyLabel`): the same pattern, drawn as caps. `WoRKeyHints::ParseHints` reads it: hints four or more spaces apart, each "KEYS  Label" (two spaces), KEYS one or more of `{Name}` or a word (Up, Left) joined by " / "; `{Tabs}` and `{SubTabs}` become their two caps; a part that isn't laid out so (a sentence) stays text. A keyboard key and a shoulder button are a framed cap; a gamepad's face buttons are circles in the mockups' colours (A green, B red, X blue, Y yellow). The caps are Slate built by `UWoRKeyHintText::BuildCapsRow` (no UObject is made on a refresh).
- **Clickable hints (B-483):** each key name has a `ClickKey` in the table. A click on a cap or on a hint's label calls `UWoRKeyHintText::ClickKey`, which on the next frame focuses the owning screen and sends that key down and up through Slate, so the screen does exactly what the key does: no screen has click code. A pair's caps click on their own; its label clicks "next".
- **Back on keyboard** is Backspace in the footers (Esc ends PIE in the editor); `WoRMenuStyle::IsBackKey` takes Backspace, Esc and gamepad B, so Esc still works.
- **Debug:** `wor.UI.KeyHints.Print` (both key sets and the device in use). Spec `WoR.UI.KeyHints`.
- **TODO:** glyphs per pad brand with the CommonUI input data (the gamepad side is the Xbox letters); the old UMG hub widgets (`UI/Hub/*Widget` buttons) and the battle HUD don't use it yet.

### Icons: one table of soft textures (B-485)

The UI's small icons live in one data asset, `UWoRIconTable` (`Public/UI/WoRIconTable.h`), filled by the editor step B-486.

- **Fields** (every one a `TSoftObjectPtr<UTexture2D>`, D-057): `ElementBadges` by `ESpellElement` (a crystal or evolution in a socket), `EmptySocket`, `StoneSocket`, `TierBadges` by `EItemTier`, `WeaponTypes` by `EWeaponType`, `EffectKinds` by `ESkillEffectType` (each an `FWoREffectIcons`: a Default icon and By Element variants, so "Fire Amp" and "Water Amp" can differ; an element without a variant takes the Default).
- **Crystals (Crown, 2026-11-14):** a crystal's badge is its element's (the diamond badges are placeholders). Ice has no crystal of its own: here Ice is a hit kind keyed to Water, not an `ESpellElement`, so an Ice crystal shows Water's badge. Effects keep their own icons.
- **The active table** is the asset at `/Game/UI/Icons/DA_UIIcon_Table` (`UWoRIconTable::GetActive`, the same lookup as `UCombatTuningData`; an AlwaysCook rule in `DefaultGame.ini`). A getter loads its texture when first asked.
- **No icon, no change:** with no table, or an icon left unset, a getter returns null and the screen keeps its text. `WoRIcons::Get...` are the null-safe calls a screen makes.
- **Drawing:** `WoRMenuStyle::MakeIcon(Tree, Size)` and `SetIcon(Icon, Texture, Name)` (the name is the icon's tooltip; no texture hides the icon).
- **What an icon stands for (B-487):** `FWoRIcon` (`Public/UI/WoRIcon.h`: a weapon type, an element, the empty socket, a stone, an effect's type; and the icon's name for its tooltip). A screen's model fills one per thing it shows; its widget calls `WoRIcons::Find(Icon)`. `WoRIcons::MakeItemEffectIcon(Item)` gives a crystal's or stone's effect icon (`GetItemEffectKind`: the item's `EItemEffectType` as the `ESkillEffectType` the icon is keyed by).
- **Users:** the v2 loadout screen (B-485: its `FLoadoutV2Icon` is `FWoRIcon`; rows, box lines), the inventory screen (B-487: `FInventoryScreenTile::Icon` / `bTierBadge` / `EffectIcon`: the tile's icon in its letter's place, a tier badge, the effect's icon beside the note) and the vendor screen (B-487: `FVendorScreenRow` the same, copied from the tile where the row is one).
- **Debug:** `wor.UI.Icons.Print`, the asset's **Print icons** button (`GetIconTableString`: per group, what is set and what is missing). Spec `WoR.UI.IconTable`.

### Chip rows: tabs and characters as framed chips (B-482)

A row of framed chips between its two key hints ("LB  [Equipment] [Skills] [Apparel] [Cosmetics] later  RB") is one shared piece, for tabs, characters and filters.

- **Types** (`Public/UI/Menus/MenuChipRow.h`): `WoRMenuStyle::FChipState` (name, greyed, a small note) and `FChipRow` (the row's chips, labels and notes, weak pointers into the screen's widget tree).
- **Builders** (`Private/UI/Menus/MenuScreenStyle.h`): `MakeChipRow(Tree, Count, PrevKeyPattern, NextKeyPattern, OutRow)` builds it (the key hints are `WoRKeyHints` patterns); `SetChips(Row, States, Current, Hovered)` names and colours the chips on every refresh (the current one `FocusColor`, the one under the mouse `HoverColor`, a greyed one never filled, the rest of the row hidden); `FindChipUnder(Row, ScreenPosition)` for a click or a mouse move.
- **In a panel (B-483b):** `MakeChipRowPanel` builds the same row inside its own panel frame, as wide as its chips; add it left-aligned.
- **First user:** the v2 loadout screen (characters; Equipment / Skills / Apparel / Cosmetics, Cosmetics greyed; the Equipment tab was Weapons until D-220). The older screens still build their tab rows by hand with `MakeTab`; move them over when they are next touched.
- **Spec:** `WoR.UI.MenuChipRow`.

### Root layout: input back to the game (B-439)

CommonUI keeps the last screen's Menu input config once every screen has closed, which blocks game input. `UWoRRootLayout` listens to its stacks (`OnDisplayedWidgetChanged`) and sets the Game config when none shows a screen. `RunWhenAllClosed(Action)` runs one action at that point: an old viewport widget that sets its own input mode must open only after the stack has let the closed screen go (its transition), or the hand-back would undo it.

## Known Limitations / TODOs

- **Settings screen (B-452):** only Turn View and the End Turn button work; no control-model cards in the side panel; Controls is one list (no device or section tabs) and leaves out Guard and Dodge (their keys are set in the editor); no "Reset page". The mockup's "AI finishes my turn when I idle" is not `SetLetAIFinish` (that is "when your fighters fall", on the Party screen), so it is a not-built row.
- **Hub HUD (B-443):** the old on-screen debug hints (`WoRInteract::ShowHint`, each interactable's `GetHintText`) still show: OBSOLETE once the prompt passes PIE (the D-124 sweep). One prompt height for every interactable (`PROMPT_HEIGHT_CM`). The key shows as a letter, not a glyph. No place name. A HUD made while a menu is already open stays visible until the next screen change (not reachable today: it is made at BeginPlay).
- **Character sheet (B-442):** no portrait; no "where it comes from" (R3) and no jump to the loadout (Y) yet; the scope line and the element line add up under one cap in a fight but are shown as separate lines; Spirit's "Resistance" sub-stat (status buildup) shares its word with the damage panel's title.
- **Loadout screen (B-440a):** read only until B-440b; no icons; no Scaling line in the detail panel; the Skills tab is off (B-440d). The cost shown is the code's `SlotCost`, which counts the crystal / evolution socket and not the stone socket.
- **Game menu (B-439):** flat colours and the default font until the Hidris art pass; no portraits, no tracked quest or loadout preview in the side panel; `BP_HubPlayerController` still has its own `IA_ToggleMenu` nodes until the editor step in `docs/PIE/B-439.md`; the old party strip is hidden while any menu screen is up (the root layout sits under it, and hides it: `UWoRRootLayout::SetPartyStripHidden`). Pouch items found in a run are not taken back when the run is quit (Q-439).

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
| 2026-11-12 | B-391c built on the desktop (`feature/B-391c-hud-pieces-2`): merged with B-391b's numbers and B-170e's pips (card rows: buildup, gear, then actions; debug string both). Full suite 1820/0; wor-reviewer no code must-fix. PIE: party card frames are hit-testable for the tooltip; check they don't block mouse picking near the party column. | `feature/B-391c-hud-pieces-2` |
| 2026-11-12 | B-129: input mapping-context priorities named in `Public/Player/InputPriorities.h` (`WoRInputPriority::EXPLORATION` / `COMBAT` / `COMBAT_MENU_KEYS`); `ACombatPlayerController` and `AExplorationPlayerController` use them (`CombatMenuKeys::CONTEXT_PRIORITY` removed). Same values; InputMap.md "Mapping contexts". Spec `WoR.Input.Priorities`. | `feature/B-129-input-priorities` (built 2026-11-12) |
| 2026-11-13 | B-424 PIE fix: `UCombatHUD::PlaceInViewport` sets the position before the anchor and alignment (UE 5.8's `SetPositionInViewport` resets the anchors to the top-left, which put the battle menu off screen; the old HUD's panels, strip and menu are placed the same way). `BuildNewHUD` sets game-and-UI input with the cursor shown, since the new menu picks inside a submenu by click (Q-391). | `feature/B-424-light-hit-airburst` |
| 2026-11-15 | B-503u (D-220): the v2 loadout's Weapons tab reads Equipment (`ELoadoutV2Tab::Weapons` keeps its name), the add row "+ Add equipment", the window "Add equipment"; `WoRLoadoutV2::BuildAddKinds` names the first step Add a weapon / Add an adapter / Add an evolution and greys a kind over its class limit with that limit (`LoadoutClassTable`). The Edit window's box follows the lit or hovered slot row (`ULoadoutV2ScreenWidget::BuildSlotDetails`; model `BuildSocketDetails`, `BuildOffHandDetails`, `MakeNoteLine`), no whole-entry summary; a crystal socket row reads "Garnet (F)". Spec cases in `WoR.UI.LoadoutV2` | feature/B-503u-equipment-tab |
| 2026-11-14 | B-476b: the v2 loadout screen, part 1. `ULoadoutV2ScreenWidget` (code-built, beside `ULoadoutScreenWidget`; `wor.UI.LoadoutV2`): the Weapons tab from `WoRLoadoutV2::BuildWeaponsPage` (entries, sockets, ACTIVE, the Add weapon frame, cells) and the Add gear window's Main hand list (`BuildMainHandChoices`, new: each owned weapon not held, checked as a draft), A Pick / Remove, X Compare (`BuildCompare`), Y Confirm through `TryChangeGear` + `ApplyDraft`; spec `WoR.UI.LoadoutV2Screen`. The rest of v2 is B-476c | `feature/B-476b-loadout-v2-weapons-tab-cloud` |
| 2026-11-14 | B-476c: the v2 loadout screen, part 2. `ULoadoutV2ScreenWidget` takes the B-479c layout (entries left, `WoRLoadoutV2::BuildEntryDetails` in a box right; X Simple / Full, a session static) and a mode machine (`ELoadoutV2Mode`: List, Options, WindowKind, WindowSlots, WindowItems, WindowAsk): the Add gear window for a new entry (`BuildAddKinds`, `BuildMainHandChoices`, `BuildAdapterChoices`, `BuildEvolutionChoices`, `CheckDraft` / `ApplyDraft`) or an edited one (`DraftFromEntry`, `CheckEntryEdit`, `ReplaceEntry`; A Remove: `RemoveEntry`), sockets through today's `WoRLoadoutScreen::BuildSocketChoices` / `ApplySocketChoice` / `TakeOffSocket`, the evolution's ask, L3 / 6 Set active (`SetRowActive`), Y Options with `RemoveAllGear`. Q-476c. The tabs are B-476d | `feature/B-476c-loadout-v2-rest-cloud` (UNBUILT) |
| 2026-11-14 | B-440e (Q-440e): saved loadouts, on the v2 screen. New `WoRLoadoutSaved` (`LoadoutSavedModel.h`): `BuildList` (each saved loadout, the active and lore ones marked, then a New line while the player has fewer than `PoolConstants::MAX_CUSTOM_LOADOUTS` of their own), `SetActive`, `CreateNew` ("Loadout N"), `Duplicate` ("<name> copy", the player's own), `Rename` (trimmed, at most `MAX_LOADOUT_NAME_LENGTH`; a lore one refused); each returns the refusal as FText. `ULoadoutV2ScreenWidget`: Options' Saved loadouts opens `ELoadoutV2Mode::Saved` (A use / new, X duplicate, Y rename in a `UEditableTextBox`, B back to Options; other keys wait while a name is typed); the loadout's name shows top right before the cells; `wor.UI.LoadoutSaved.Print`; spec `WoR.UI.LoadoutSaved`. The old screen (`ULoadoutScreenWidget`) is untouched: the cloud branch's Y list there was not brought over | `feature/B-440e-saved-loadouts-v2` |
| 2026-11-14 | B-440f (Q-440f, D-164): change apparel, on the v2 screen. New `WoRLoadoutApparel` (`LoadoutApparelModel.h`): `BuildApparelChoices` (the member's owned pieces, `UPoolSubsystem::GetOwnedApparel` by owner, less those worn; a stoned socket refused), `PutApparel` / `TakeOffApparel` (pure), `ApplyApparelChoice` / `TakeOffRow` (through `ULoadoutComponent::TryChangeGear` + `NotifyApparelChanged`; a lore loadout refused until B-380e), `GetMemberApparelString`; `WoRLoadoutScreen::BuildApparelRow` (a piece as a gear row, `GearID` = its owned id). `ULoadoutV2ScreenWidget`: `ELoadoutV2Tab` (Weapons, Apparel) on LB / RB; the Apparel tab lists the worn pieces and an Add apparel frame ("n/2 left"), the box shows the focused piece (tier, socket, element, bonus line); A opens `ELoadoutV2Mode::ApparelItems` (A Pick, A Remove on EQUIPPED). `wor.UI.LoadoutApparel.Print`; spec `WoR.UI.LoadoutApparel`. Left for B-476d: the apparel socket from this tab, X Compare, the HP / Defence / Amp lines in the box. The old screen is untouched | `feature/B-440f-loadout-apparel-v2` |
| 2026-11-14 | B-481 (Crown's PIE note): key hints follow the device in use. New `WoRKeyHints` + `UWoRKeyHintText` (`UI/WoRKeyHintText.h`; see "Key hints" above); `WoRMenuStyle::MakeHintText`, `MakeKeyLabel` takes a pattern, `MakeText` takes a text class. Every code-built menu footer, tab key label and hold hint is now a pattern (v2 and old loadout, inventory, journal, game menu, title, vendor, settings, character sheet, credits, new game, forms, party, recruit card, gate, invite, hand over, host lost, join request); the hub prompt reads the same table. Keyboard Back reads Backspace; the Settings Keys page lists it first. `wor.UI.KeyHints.Print`; spec `WoR.UI.KeyHints` | `feature/B-481-device-key-hints` |
| 2026-11-14 | B-482 (Crown's PIE notes on v2): the shared chip row (`MenuChipRow.h`, `MakeChipRow` / `SetChips` / `FindChipUnder`, `HoverColor`; see "Chip rows" above). `ULoadoutV2ScreenWidget`: the header is the mockup's (a chip per character and per tab, Skills and Cosmetics greyed with "later", LT / RT and LB / RB beside their row, the loadout's title top right, "Cells used" under them); the details box is hidden when it has nothing to say (the Add weapon / Add apparel frames); the mouse (`ClickTabChip`, `ClickMemberChip`, `ClickLine`: focus, again A; hover lights a chip or a line). Specs `WoR.UI.MenuChipRow`, a case in `WoR.UI.LoadoutV2Screen`. The footer's hints are not clickable (one text) | `feature/B-482-loadout-v2-chips-mouse` |
| 2026-11-14 | B-482b (Crown's PIE notes): v2 entry rows. `FLoadoutV2Entry::SocketName` (what augment socket 1 holds, from `LoadoutV2Build::AttachmentName`, the function the details box's socket line uses: one source); a row reads "Name (Garnet)", nothing when empty (apparel rows too); the row's detail is its right end ("ACTIVE", "n/5 left"), right-aligned in the accent colour; the old "Crystal · empty" (the kinds of the two sockets) is gone. The greyed chips carry no "later" note. The header is sized to the list + box width so the loadout's title ends where the box does | `fix/B-482b-loadout-v2-entry-rows` |
| 2026-11-14 | B-483 (Crown's PIE notes): key caps and clickable hints in the shared helper (`ParseHints`, `FWoRKeyHint` / `FWoRKeyCap`, `UWoRKeyHintText` caps mode and `ClickKey`; `WoRMenuStyle::MakeHintBar`, `MakeKeyLabel` draws a cap; see "Key hints" above); every code-built menu footer is a hint bar (18 screens and cards, the vendor's confirm hint). Loadout v2: the left side (Cells used, a list's title, the lines) in one panel, the details box its own, the footer in a thin bar. Spec cases in `WoR.UI.KeyHints` | `feature/B-483-key-caps-section-frames` |
| 2026-11-14 | B-483b (Crown's PIE notes): the footer's bar is drawn by the shared hint bar itself (`UWoRKeyHintText::SetKeyCaps(true, bBar)`; `MakeHintBar(..., bBar = true)`): a thin panel that hugs the hints, right-aligned in its slot, in every menu; key labels and the vendor's in-page confirm hint have none. `WoRMenuStyle::MakeChipRowPanel` frames a chip row to its chips; loadout v2's character row and tab row each sit in one, and its own full-width footer bar is gone | `fix/B-483b-chip-row-frames-footer-hug` |
| 2026-11-14 | B-484 (Crown's PIE notes): the Add gear window's box. Model: `WoRLoadoutV2::BuildWeaponDetails` / `BuildAdapterDetails` / `BuildAttachmentDetails` (one piece on its own; `BuildEntryDetails` reads the same helpers `AddWeaponSet` / `AddAdapter`). `ULoadoutV2ScreenWidget`: in a slot's list (and the apparel list) the box shows the line under the mouse, else the focused line (`GetBoxLine`, `BuildItemDetails`), that piece alone, not the entry's slots; a row has a tag at its right end (`GetLineTag`: ACTIVE, EQUIPPED, PICKED) apart from its name and detail, which end in an ellipsis when too long | `feature/B-484-add-gear-box-follows-row` |
| 2026-11-14 | B-485 (Crown approved `menus/loadout/icons`): the icon table `UWoRIconTable` (soft textures per element badge, empty / stone socket, tier, weapon type, effect kind; `GetActive`, `WoRIcons::`, `wor.UI.Icons.Print`; see "Icons" above); `WoRMenuStyle::MakeIcon` / `SetIcon`. Loadout v2: `FLoadoutV2Icon` on entries (`TypeIcon`, `SocketBadges`) and detail lines (`Icon`, `bHasTier`, `Tier`), `MakeSocketIcon`, `GetAttachmentTier`; the widget draws a weapon-type icon on a row, its sockets as badges (instead of "(Garnet)"), a tier badge by a piece's name, icons beside effect and socket lines, badges on Add gear's crystal lines; each icon has its name as a tooltip; with no icon the text stays. The assets are B-486 (editor step) | `feature/B-485-loadout-icons` |
| 2026-11-14 | B-487b (Crown's PIE notes): the details box layout. v2 loadout: a Piece line carries its type (`FLoadoutV2DetailLine::TypeName`; the "Type" Stat row is gone) and a filled Socket line its tier. The widget draws the first piece as the header: a small quiet row (`BoxTypeRow`: the piece's icon and type), then the name and tier badge (`BoxTitleText`, `BoxTitleTier`), in place of a plain name above the piece line; a Socket line whose badge is drawn shows "Garnet (F)" / "Empty" with no label (no badge: the label stays); rows sit `BoxLineGap` apart. Inventory: the kind line sits on top beside the icon, the name and tier badge under it; `FLoadoutScreenDetail::Icon` (a filled socket's badge) is drawn in its label's place (`DetailLineIcons`). Vendor: no such rows, unchanged. | `fix/B-487b-details-box-one-header` |
| 2026-11-14 | B-487 (Crown): the inventory and vendor screens use the icon table. `FWoRIcon` / `EWoRIconKind` (`UI/WoRIcon.h`, shared; the v2 loadout's icon type is now this), `WoRIcons::Find`, `GetItemEffectKind`, `MakeItemEffectIcon`. Inventory: a tile's icon in its letter's place (a weapon's type, a crystal's or evolution's element diamond, a stone), a tier badge on the tile and after the name in the panel, the effect's icon beside the note. Vendor: the row's icon before its name, the tier badge in the tier column, the effect sentence beside its icon in the panel. Each icon has its name as a tooltip; with no icon the letter and the words stay | `feature/B-487-inventory-vendor-icons` |
| 2026-11-14 | B-476d (Q-476d): the v2 loadout screen's Skills tab, between Weapons and Apparel (`ELoadoutV2Tab::Skills`, the Skills chip built; LB / RB and the chips turn the three in a ring, `SwitchTab(Delta)`, `ShowTab`). `WoRLoadoutV2Skills` (`UI/Menus/LoadoutV2SkillsModel`: each v2 entry with skills lists its tab of today's Skills page, `WoRLoadoutSkills`, under "Name · filled/total skills"; a Refractor's Innate spells entry; `BuildSkillDetails`); the widget's SkillEntries / SkillRows / SkillPicker modes (A Slots, A Change, A Put in slot through `WoRLoadoutSkills::ApplySkill`, X / V full or short text). On the desktop the tab was matched to the Weapons tab: `FLoadoutV2SkillEntry::WeaponEntry` and `ULoadoutV2ScreenWidget::GetRowEntry` (a Skills entry's row draws its Weapons entry's type icon, socket badges and ACTIVE; "filled/total" at the row's right end; "Skills by entry" above the list), `BuildSkillDetails(Skill, bSimple, KindLine)` (the B-487b header: the kind on the quiet line, the name and tier badge), the footer's X hint as on Weapons (`{ViewName}`). Spec `WoR.UI.LoadoutV2Skills` | `feature/B-476d-loadout-v2-skills-tab` (the cloud's `feature/B-476d-skills-tab-v3-cloud` merged in) |
| 2026-11-14 | B-488 (Crown: retire the first loadout screen): `ULoadoutScreenWidget` (`LoadoutScreenWidget.h/.cpp`) and its `wor.UI.Loadout` command deleted, with its one widget case in `LoadoutScreenSpec.cpp` (the model's cases stay). `UPauseMenuWidget::OpenPage` (Loadout) and `UGateScreenWidget`'s loadout key open `ULoadoutV2ScreenWidget::Open`. The first screen's take-a-skill-out moved to v2: `PressConfirm` in `SkillRows` (Y / R) calls `WoRLoadoutSkills::ApplySkill(..., nullptr)`. `WoRLoadoutScreen` and `WoRLoadoutSkills` (models) stay | `chore/B-488-retire-old-loadout-screen` |
| 2026-11-14 | B-491 (D-213: the player-text audit, applied): wording only, in about 60 files (menus, vendors, rest spot, gate, party and co-op, forms, level drop, battle HUD and menu, effect text, the shared name tables). New `WoRHubHUD::FormatPromptLine(Prompt, bGamepad)` (`UI/Hub/HubHUDModel.h`): an `FInteractPrompt` as one line, "E  Open · Chest"; every `GetHintText` (chest, lore, boss arena, rest spot, memory fragment, Prisms drop, journal, merchant, trial door, party invite) reads it from its own `GetPrompt`, so the hint and the floating prompt can't differ. `WoRGameNames`: buffs and debuffs are "X Up / X Down", EP, Defence, DoT, Barrier; a vendor's name has no note (its wares are `WoRVendorScreen::GetVendorLine`) | `chore/B-491-player-text-audit` |
| 2026-11-14 | B-439 (D-181, D-176): the game menu. `WoRPauseMenu` (model: pages, header, party, quit) and `UPauseMenuWidget` (CommonUI screen built in C++ from the mockup) under `UI/Menus/`; `AWoRHubController` binds `IA_ToggleMenu` to `OpenGameMenu`; `UWoRRootLayout` hands input back to the game when every screen has closed and gains `HasActiveScreen` / `RunWhenAllClosed`; `UTrialRunSubsystem::AbandonRun` (a quit run pays nothing). Spec `WoR.UI.PauseMenu` (10 cases). New section "Menu screens" | feature/B-439-pause-menu |
| 2026-11-14 | B-440a (D-181, D-180): the loadout screen, gear page, read only. `WoRLoadoutScreen` (model) and `ULoadoutScreenWidget` under `UI/Menus/`; name tables `GetCrystalTypeName`, `GetWeaponTypeName`, `GetWieldModeName`, `GetTierLetter`; `MenuScreenStyle.h` shares the menu look; the game menu Loadout page opens it and keeps its selection on return; `UWoRRootLayout` hides the old party strip while any menu screen is up. Spec `WoR.UI.LoadoutScreen` (9 cases) | feature/B-440-loadout-screen |
| 2026-11-14 | B-442 (D-181): the character sheet. `WoRCharacterSheet` (model) and `UCharacterSheetWidget` under `UI/Menus/`; `WoRGameNames::GetSubStatName`; the bar with its value inside, tabs and key labels move to `MenuScreenStyle.h`; HP and EP bars darker (D-194); the game menu Character page opens it. Spec `WoR.UI.CharacterSheet` (5 cases) | feature/B-442-character-sheet |
| 2026-11-14 | B-443 (D-181): the hub HUD interact prompt and money chip. `FInteractPrompt` / `EInteractVerb`, a `GetPrompt` on each interactable, `HubInteract` candidates carry it; `WoRHubHUD` (model) and `UHubHUDWidget` under `UI/Hub/`; the hub controller makes it, the root layout hides it under menus. Bar values plain white, no backing (D-197). Spec `WoR.UI.HubHUD` (5 cases) | feature/B-443-hub-hud |
| 2026-11-14 | B-452 (D-190): the settings screen. `WoRSettingsScreen` (model) and `USettingsScreenWidget` under `UI/Menus/`; Turn View and the End Turn button change and are saved on closing; the other approved rows are listed as not built (B-453, B-436l); a read-only key list on Controls; the game menu Settings page opens it, so every page of the menu now opens something. Spec `WoR.UI.Settings` (6 cases) | feature/B-452-settings-screen |
| 2026-11-14 | B-441 (D-181, D-177, D-195, Q-441): the inventory screen. `WoRInventoryScreen` (model: `BuildPage`, `BuildVerbs`, `BuildReceivers`, `GetMoveCount`; debug `wor.UI.PrintInventory`) and `UInventoryScreenWidget` under `UI/Menus/` (`wor.UI.InventoryScreen`): character tabs, Gear / Pouch / Chest tabs, a tile grid, the detail panel, the verb menu (Give, Store; Take only on the Chest tab) and Give to; X is whole stack (`WoRMenuStyle::IsToggleKey`). `WoRLoadoutScreen::BuildGearRow` describes a piece for both screens. `WoRInventoryVerbs::GetRefusal` refuses the chest during a run (gear and stacks) and says a chest row is already stored. The game menu's Inventory page opens it; the placeholder `UInventoryWidget` is kept until PIE. Review: Take of a stack is greyed when the pouch has no room, and a whole-stack Give or Take moves what fits (`GetMoveCount`). Spec `WoR.UI.InventoryScreen` (7 cases) | feature/B-441-inventory-screen |
| 2026-11-14 | B-456a (D-193, D-181): the vendor screen, piece 1. `WoRVendorScreen` (model: `GetTabs`, `IsBuilt`, `GetService`, `BuildPage`, `GetTakeCount`, `BuildChips`; debug `wor.UI.PrintVendor`) and `UVendorScreenWidget` under `UI/Menus/` (`wor.UI.VendorScreen <vendor>`): the vendor and the money, service tabs per vendor (the ones not built yet are listed and say so), character tabs, a row per thing `WoRVendorServices` takes (named by the inventory screen's tiles; a refused row greyed with its reason) and the detail panel. Sell and Dismantle run through `WoRVendorServices::Run`: the first A asks, the second does it; X is the whole stack. The placeholder `UVendorServiceWidget` is kept until PIE. Next: Buy (B-456b, which also makes talking to a vendor open this screen), Item slots (B-456c), Repair, Curses, Merge (B-456d). Spec `WoR.UI.VendorScreen` (5 cases) | feature/B-456-vendor-screen |
| 2026-11-14 | B-456b (D-193, Q-456b): the vendor screen, piece 2: Buy. `WoRVendorScreen::BuildBuyPage` lists the vendor's stock (the run's roll at a trial vendor; an empty line is skipped) in the loadout and inventory screens' words, each line with its cost, a weapon's Power against the receiver's worn one, and the purchase's own refusal (`UEconomyService::GetPurchaseRefusal`) when it can't be bought on top of the cart. The cart (`AddToCart`, `RemoveFromCart`, `BuildCartSummary`, `PurchaseCart`): each line is for a party member; one member's lines are bought together through `PurchaseFor`. `UVendorScreenWidget`: on Buy the character tabs give way to "Buying for" (R / Y, LT / RT); A or Right adds the focused line, Left takes one back, X buys; a cart panel under the details. Every vendor has a Buy tab (one D-193 doesn't name has Buy alone) and `UMerchantShopSubsystem::OpenForMerchant` opens this screen; the old `UShopWindowWidget` is the fallback until PIE. `WoRMenuStyle::IsSecondActionKey` (R / Y). Spec `WoR.UI.VendorScreen` (9 cases) | feature/B-456b-vendor-buy |
| 2026-11-14 | B-456c (D-193, D-097): the vendor screen, piece 3: the Jeweler's Item slots. `WoRVendorScreen::BuildItemSlotsPage`: a row per party member (`FVendorScreenRow::Member`, `Slots`, `SlotsMax`) with the next slot's price, the one after it, what the pouch will hold, and `ItemSlotUpgrade::GetRefusal`'s reason when it can't be bought; `FVendorScreenPage::Intro` is said above the rows; `BuyItemSlot` buys through `ItemSlotUpgrade::TryBuy` with the payer's wallet; `GetItemSlotConfirmText`. `UVendorScreenWidget`: on Item slots the character tabs are hidden (`HasMemberTabs`), each row shows a mark per slot (lit for each the pouch has), the first A asks and the second buys (D-016); X and LT / RT do nothing there. `wor.UI.PrintVendor` lists the tab once, for the party. The placeholder `UItemSlotUpgradeWidget` (`wor.UI.ItemSlots`) stays until PIE. Spec `WoR.UI.VendorScreen` (11 cases) | feature/B-456c-item-slots |
| 2026-11-14 | B-456d (D-193, Q-456d): the vendor screen, piece 4: the Spiritualist's Repair, Curses and Merge; every D-193 tab is built (`IsBuilt` is true for all). `WoRVendorScreen::BuildRepairPage` (a row per `GearRepair::GatherWornPieces` piece: owner and holder, "35%" / "broken" in `FVendorScreenRow::State`, `FVendorScreenPage::AllPay` / `AllRefusal` for everything), `BuildCursesPage` (the player's held curses, price, `UCurseSubsystem::GetClearRefusal`; hub only), `BuildMergePage` (a row per crystal kind the member holds, stat stones left out; the target tier in `Row.Item`, default the highest it reaches; uses / makes / change back from `UEconomyService::GetMergeRefusal`'s plan), `StepMergeTarget`, `GetConfirmText`, `RunService`, `GetDoneText`, `RepairAll`. `UVendorScreenWidget`: no character tabs on Repair and Curses; A asks and the second A does it; on Repair X asks "Repair everything?" and the second X (or A) does it (`PressRepairAll`, `bConfirmingAll`); on Merge Left / Right picks the tier (`MoveMergeTarget`, `MergeTargets`, reset on a new character). The placeholder `UCurseClearWidget` stays until PIE. Spec `WoR.UI.VendorScreen` (14 cases) | feature/B-456d-spiritualist-services |
| 2026-11-14 | B-462 (D-201): the accent is violet. `WoRMenuStyle::FocusColor` is `#5B3A99` (focused row, lit tab, the hub HUD's key chip) and `CyanColor` `#CDB8FF` (key labels, marks); the name `CyanColor` is kept so no screen changed. Every C++-built screen reads these two; none sets its own teal. The battle HUD is the UI Designer's (B-436) | feature/B-462-violet-accent |
| 2026-11-14 | B-467 (D-202, D-060, Q-467): the title screen. `WoRTitleScreen` (model: `BuildRows`, `Summarize` / `SummarizeSave`, `GetPlayTimeText`, `GetNewGameWarning`, `GetOldVersionText`, `GetNeededVersionText`; debug `wor.UI.PrintTitle`) and `UTitleScreenWidget` under `UI/Menus/` (`wor.UI.Title`; outside the editor `AWoRHubController::BeginPlay` opens it once a launch through `OpenAtLaunch`, until the player leaves it). Continue closes it (the level under it is the save's); New Game over a save shows a confirm panel and needs A held 2 seconds (a core ticker, `UpdateHold`), then `USaveSubsystem::DeleteSave` (the fresh game begins at the next start until B-469); Settings opens `USettingsScreenWidget` over it; Quit quits; Back never closes it. "Your save was reset" shows first, once a launch, when the load wiped an old save. Save side: `FWoRSaveData::PlaySeconds` (added to in `CaptureInto`, no version bump), `UWoRSaveGame::GetWipedFromVersion`, `USaveSubsystem::HasSave` / `WasWiped` / `GetWipedFromVersion` / `GetPlaySeconds`. Not built: the backdrop art (B-467b), Join a friend (B-459), Credits (B-470). Spec `WoR.UI.TitleScreen` (6 cases) | feature/B-467-title-screen |
| 2026-11-14 | B-470: the credits screen. `WoRCredits` (model: `GetGameTitle`, `BuildSections`, a C++ table kept in step with the `CREDITS.md` files; `GetScrollStep`, `SCROLL_SPEED`, `FASTER_SCALE`; debug `wor.UI.PrintCredits`) and `UCreditsScreenWidget` under `UI/Menus/` (`wor.UI.Credits`; the title's Credits row opens it): a centred column in a scroll box moved by a core ticker (`UpdateScroll`), faster while Select is held, Back leaves, and it closes itself when the last line has scrolled away. `WoRMenuStyle::FrontBackdropColor` is the title's and the credits' flat backdrop until B-467b. Spec `WoR.UI.Credits` (2 cases) | feature/B-470-credits-screen |
| 2026-11-14 | B-463 (D-181, D-196, D-198, D-199, Q-463): the journal screen. `WoRJournalScreen` (model: `GetPageName`, `BuildView` for a page under a sub-tab, `ToggleSecret`, `HideAllSecrets`, `PlayNextLine`; debug `wor.UI.PrintJournal`) and `UJournalScreenWidget` under `UI/Menus/` (`wor.UI.Journal`; the game menu's Journal page opens it). Pages Quests, Memories, Lore, Conversations: sub-tabs and a list on the left, a reader on the right; sub-tabs, rows and step lines are pooled (made as first needed, refilled after). `FJournalState` holds what the open journal remembers (shown secret quests, the talk being played) and is cleared when the screen closes (Q-397b). `FVisibleQuest::DoneGoals` gives the steps already done (none for a secret quest, D-171). The placeholder `UMemoryJournalWidget` stays for the hub's journal station (V's lines) until B-471. Spec `WoR.UI.Journal` (9 cases) | feature/B-463-journal-screen |
| 2026-11-14 | B-461 (D-181, Q-461): the inventory screen, piece 2. `WoRInventoryScreen`: the Gear tab also lists the member's evolutions (its spawned `UEvolutionInventoryComponent`) and apparel (`UPoolSubsystem::GetOwnedApparel` by owner), their verbs refused (Q-323); `GetFilters(Tab)` (All and the kinds the tab can hold; none on the Pouch), `BuildPage(World, Member, Tab, Filter, Sort)`, `GetNextSort` (Kind, Tier, Name); a tile carries `Kind` and `TierValue` (`IsStack()` replaces "not gear"; the vendor's `FindTile` uses it). `UInventoryScreenWidget`: focus state `Filters` (Up from the top row of tiles; Left / Right filters; Down, A or B returns; a click picks), `CycleSort` on `WoRMenuStyle::IsSecondActionKey`, the place name top right from `WoRPauseMenu::BuildHeader`. Spec `WoR.UI.InventoryScreen` (11 cases, 4 new) | feature/B-461-inventory-piece-2 |
| 2026-11-14 | B-440b (UIScreens #2): the loadout screen, piece 2: change gear. `WoRLoadoutScreen`: each gear row carries its place (`ELoadoutScreenPlace`: Primary, Secondary, Adapter with its index) and owned piece; a free place has an `EmptyGear` row ("Empty slot", a Generic's "Empty weapon", a Resonator's "Empty adapter"). `BuildGearChoices` (owned weapons / adapters the place takes, not already in the loadout, each with `NewSpent`; refused lines stay with their reason: over the budget, worn by another member; "Put … in hand" first for a Generic's other weapon), `PutGearChoice`, `TakeOffGear` (the primary never empties, D-116: a Generic's second weapon moves up), `ApplyGearChoice` / `UnequipRow` through the new `ULoadoutComponent::TryChangeGear` (the pick rule check, then gear effects and max pools re-read; refused in a fight). `ULoadoutScreenWidget`: A opens the picker in the list (amber budget preview, the reason in the detail panel), A puts in, B cancels; X unequips; a click is A. Apparel is B-440f. Debug `wor.UI.PrintGearChoices`. Spec `WoR.UI.LoadoutScreen` (13 cases, 4 new) | feature/B-440b-loadout-change-gear |
| 2026-11-14 | B-444 (Q-444, D-184): the Spirit of {character} screen. New `WoRFormsScreen` (`UI/Hub/FormsScreenModel.h`: `BuildPage`, `Wear`, `SwitchBaseBody`; debug `wor.UI.PrintForms`) and `UFormsScreenWidget` (`UI/Hub/FormsScreenWidget.h`, a `UWoRActivatableWidget` on the menu layer, `wor.UI.Forms`). The placeholder `UCharacterFormWidget` is `wor.UI.FormsOld` until Crown's PIE pass. Spec `WoR.UI.FormsScreen` (3 cases) | feature/B-444-spirit-forms-screen |
| 2026-11-14 | B-458 (Q-458, D-200): the party screen. New `WoRPartyScreen` (`UI/Party/PartyScreenModel.h`: `BuildPage`, `ToggleSeat`, `MoveOnGrid`, `GetNextCell`; debug `wor.UI.PrintParty`) and `UPartyScreenWidget` (`UI/Party/PartyScreenWidget.h`, menu layer, `wor.UI.Party`), opened by the game menu's Party page. Field movement names move to `WoRGameNames::GetFieldMovementName` (D-062). The placeholder `UPartyControlWidget` stays on `wor.UI.PartyControl` until Crown's PIE pass. Spec `WoR.UI.PartyScreen` (3 cases) | feature/B-458-party-screen |
| 2026-11-14 | B-459a (Q-459a): the host-lost card and Hand over host?. New `WoRHostHandOverScreens` (`UI/Coop/HostHandOverModel.h`; debug `wor.Coop.PrintHandOverCard`), `UHostLostCardWidget` with `UHostLostCardPresenter` (a game-instance subsystem that shows the card in whichever world is up, from the lost-host signal and each new world) and `UHandOverConfirmWidget` (menu layer, `wor.UI.HandOver`; the rest menu's hand-over row opens it). Spec `WoR.UI.HostHandOver` (3 cases) | feature/B-459-coop-session-screens |
| 2026-11-14 | B-459b (Q-459b): Play together. New `WoRInviteScreen` (`UI/Coop/InviteScreenModel.h`: `BuildPage`, `GetSeatWho`, `Invite`, `ToggleHosting`; debug `wor.UI.PrintInvite`) and `UInviteScreenWidget` (`UI/Coop/InviteScreenWidget.h`, menu layer, `wor.UI.PlayTogether`), opened by the party screen's Invite row and the rest menu's Invite. The placeholder `UInviteWidget` stays on `wor.UI.Invite` until Crown's PIE pass. Spec `WoR.UI.InviteScreen` (3 cases) | feature/B-459b-invite-screen |
| 2026-11-14 | B-459c (Q-459c): the host's join request card. `URunStateComponent::OnJoinRequestsChanged` (server; every request, answer, refusal and leaver). New `WoRJoinRequestCard` (`UI/Coop/JoinRequestModel.h`: `BuildCard`, `Give`, `Refuse`; debug `wor.UI.PrintJoinRequest`) and `UJoinRequestCardWidget` + `UJoinRequestCardPresenter` (`UI/Coop/JoinRequestCardWidget.h`, a world subsystem on the host: top-right corner card, Z 40; hold View / Tab adds focus through a code-built Enhanced Input hold only while a request waits; `wor.UI.AnswerJoinRequest`). The placeholder `UJoinSeatWidget` stays on `wor.UI.JoinSeat` until Crown's PIE pass. Spec `WoR.UI.JoinRequestCard` (2 cases) | feature/B-459c-join-request-card |
| 2026-11-14 | B-463b (Q-463b): track a quest. `UHiddenQuestSubsystem::TrackQuest` / `GetTrackedQuest` / `GetTrackedLine` (saved in `FSavedHiddenQuests::TrackedQuest`, no version bump); the journal's A on a quest (`WoRJournalScreen::ToggleTrack`); the game menu's Journal preview (`WoRPauseMenu::BuildPages`' `TrackedLine`, `GetTrackedLine`); the trial map HUD's line (`WoRHubHUD::BuildQuestLine`, `UHubHUDWidget` QuestBox). Specs: `WoR.UI.JournalScreen`, `WoR.UI.PauseMenu` | feature/B-463b-track-quest |
| 2026-11-14 | B-463c (Q-463 points 4, 5): journal details. `UMemoryJournalSubsystem` keeps where each memory and lore entry was found (`GetPlace`, `GetCurrentPlace`: trial + variation, else the hub), the lore's kind (`ELoreKind`, `ATrialLoreObject::Kind`) and which entries are new (`IsNew` / `MarkOpened`), all in `FSavedJournal` with no version bump (an older save: nothing new, no place). The journal screen tags NEW (cleared when the row is focused, `WoRJournalScreen::MarkOpened`), shows the place, the lore kind with place sub-tabs, and a finished quest's reward (`FVisibleQuest::Rewards`, `DescribeReward`). Spec `WoR.UI.Journal` | feature/B-463c-journal-details |
| 2026-11-14 | B-466 (D-191, D-192, Q-466): the Gate screen. `UGateScreenWidget` (C++ built, menu layer; `ATrialDoor::Interact` opens it, the placeholder `UTrialGateWidget` is the fallback without a root layout; `wor.UI.Gate`) over `WoRGateScreen` (`GatherGates`: every `UTrialData` with a level; `BuildPage`; `IsUnlocked` by `FTrialVariation::UnlockQuestId`; `GetEnterRefusal` / `Enter`; `wor.UI.PrintGate`). A locked trial is "???" with `UnlockHint`; no boss shown; "Going in" from `WoRPartyScreen::BuildPage`. Spec `WoR.UI.GateScreen` | feature/B-466-gate-screen |
| 2026-11-14 | B-469 (Q-469): the new game screen. `UNewGameScreenWidget` (C++ built, menu layer; the title's New Game opens it with no save or after the hold deletes the save; `wor.UI.NewGame`) over `WoRNewGame` (`NewGameScreenModel.h`: words, `FindStartCharacter` over `UPartySessionSubsystem::NewGameCharacters` (DefaultGame.ini, the lords), `Begin`). Begin calls `UTrialRunSubsystem::ExitTrial` then `USaveSubsystem::StartFreshGame` (every save-backed state in memory reset, the picked lord the only member, the slot written); `ATrialGameMode::SpawnDefaultPawnAtTransform` gives the local leader's pawn the first member's CharacterData before BeginPlay. `UTitleScreenWidget::MarkLeftThisLaunch`. Spec `WoR.UI.NewGame` | feature/B-469-new-game-screen |
| 2026-11-14 | B-453c (D-190, Q-453c): hold to confirm dangerous actions. `FSavedSettings::bPressToConfirm` (no bump), `USaveSubsystem::IsHoldToConfirmOn` / `SetHoldToConfirmOn`; off, the game menu's quit and the title's start over are confirmed by the press on their confirm (the confirm step stays, D-176), the hint and hold bar follow; the settings screen's Accessibility row; spec case in `WoR.UI.Settings` | `feature/B-453c-hold-to-confirm-cloud` |
| 2026-11-14 | B-453d (D-190, Q-453d): text size. New `WoRTextSize` (`UI/Settings/TextSizeSetting.h`): `ETextSizeLevel` (Normal, Large, Larger: 1, 1.15, 1.3 times the project's own `UUserInterfaceSettings::ApplicationScale`, which scales the game viewport's UI only), `Apply` / `Restore`; `FSavedSettings::TextSize` (no bump); `USaveSubsystem::Get/SetTextSizeLevel` (Set applies; the load applies the saved one; `Deinitialize` restores); the Accessibility row; spec case in `WoR.UI.Settings`; `wor.UI.TextSize`. Known limitation: it writes the config-backed `UUserInterfaceSettings` default, so saving Project Settings while scaled (or a crash before the game instance ends) keeps the scaled value in `DefaultEngine.ini` | `feature/B-453d-text-size-cloud` |
| 2026-11-14 | B-453f (D-190, Q-453f): element letters on colours. `WoRGameNames::GetElementLetter` (two letters: Fi, Wa, Ea, Wi, Li, Da, Lt, Vo, Ge; "?" for the unknown element); with `FSavedSettings::bElementLetters` on (no bump), `DamageNumberView::MakeStyle` / `MakeNegatedStyle` tag elemental numbers (`AddElementLetter`, none on physical hits) and the battle HUD's status line names the pending element; the Accessibility row; spec cases in `WoR.UI.Settings` and `WoR.UI.DamageNumber` | `feature/B-453f-element-letters-cloud` |
| 2026-11-14 | B-440d (Q-440d): the loadout screen, piece 4: the Skills page. New `WoRLoadoutSkills` (`LoadoutSkillsModel.h`): `BuildSkillsPage` (a Refractor's innate school tabs on the points budget, then a tab per weapon's skill slots and adapter's spell slots), `BuildSkillChoices` (learned skills, a weapon's built-ins; innate refused over the points), `PutSkill` / `ApplySkill` (through `TryChangeGear`, then `WriteLoadoutToOwned`); rows are `ELoadoutScreenRowKind::Skill` / `EmptySkill` with `SkillSlot`, `Skill`, `bPick`. `WoRLoadoutScreen` exposes `FindRowGear`, `FindMemberLoadout`. `ULoadoutScreenWidget`: `SwitchPage` (LB / RB), `MoveSkillTab` (Left / Right), the points line in the budget row. Debug `wor.UI.PrintSkills`. Spec `WoR.UI.LoadoutSkills` (4 cases) | feature/B-440d-loadout-skills-page |
| 2026-11-14 | B-440c (Q-440c): the loadout screen, piece 3: the socket picker. `WoRLoadoutScreen`: `LoadoutScreenSockets` (CRYSTAL, STONE), `FLoadoutScreenSocket::bEvolution`; `BuildSocketChoices` (carried gems or stones from the member's pouch, owned evolutions; budget after each; refused: over budget, a second S-tier, an evolution in use), `PutSocketChoice` (the preview), `ApplySocketChoice` (`UInventoryComponent::AttachCrystalTo*` / `AttachEvolutionTo*`), `GetTakeOffSocketWarning` and `TakeOffSocket` (`RemoveCrystalFrom*`, both sockets, used up, CL-28b; `RemoveEvolutionFrom*`). `ULoadoutScreenWidget`: Left / Right lights a socket (`MoveSocket`), A on an empty socket opens the picker in the list, X asks then takes off. Spec `WoR.UI.LoadoutScreen` (15 cases, 2 new) | feature/B-440c-loadout-socket-picker |
| 2026-11-14 | B-474 (D-207): the battle menu's Abilities list adds the other weapon's skills after the weapon in hand's, named "Skill (Weapon)", while "Switch to the weapon of the skill you use" is on (`BuildAbilitySubmenu`, `FCombatCapabilities::OtherWeaponAbilities`); Switch Weapon's gamepad key is L3. The loadout screen: L3 / 6 Set active (`ULoadoutScreenWidget::PressSetActive`), in the hint line. | `feature/B-474-weapon-follows-skill-cloud` (UNBUILT) |
| 2026-11-14 | B-476, first part (D-206, D-209, D-210; UIScreens #2-3 v2): the v2 loadout screen's model, `UI/Menus/LoadoutV2Model` (`WoRLoadoutV2`): `BuildWeaponsPage` (entries by name, a pair "A + B", socket icons, the weapon in hand, the evolution line, "Cells used n of m", the Add frame's "n/m left", `bCanAdd` by the class's entry limits), the Add gear draft (`FLoadoutV2Draft`; `ApplyDraft` to the next free place, `CheckDraft` with the refusals in the game's words), `BuildOffHandChoices` (greyed with the reason), `BuildCompare` (power, built-ins gained / lost). Debug `wor.Loadout.PrintV2`; spec `WoR.UI.LoadoutV2`. The widget is B-476b; today's screen stays. | `feature/B-476-loadout-v2-model-cloud` (UNBUILT) |
| 2026-11-14 | B-450 (D-187): no enemy rank in player text: the boss arena's hints and messages name the foe ("Press E to challenge {0}", "{0} is beaten", "No one is waiting here"), the extract confirm says "finishing the trial", the Gate's wipes line says "in the final fight". `WoRDefeat::FWipeResult::LastBlowBy` (recorded by `ABattleGameMode::HandleTargetKilled`, `GetLastWipeResult`) for the wipe card (B-465). The revive rows and vote lines wait on Q-450; the settings row "Boss cinematics" names no enemy (D-190's name). Spec `WoR.UI.NoRankWords`. | `fix/B-450-no-rank-words-cloud` (UNBUILT) |
| 2026-11-15 | B-503g: the hub HUD (`UHubHUDWidget`, hub and trial maps) draws the mockup party strip bottom left: `MAX_PARTY_MEMBERS` cards built once, filled on the 0.1 s refresh from `WoRHubHUD::BuildPartyStrip` (name, `GetElement` colour dot, carried HP over max). `UPartyDisplayWidget` (the Blueprint's old debug cards) collapses itself unless the console variable `wor.UI.DebugPartyBox` is 1. | `feature/B-503g-trial-hud-party-strip` |
| 2026-11-15 | B-503i (Crown's PIE of B-503g / B-503h): `UHubHUDCardFrame` (in `HubHUDWidget.h`, a paint-only `UUserWidget` behind each card in a `UOverlay`) draws the mockup's card: dark fill, thin light outline, the top-left corner cut (`CardCutSize`). `MakeCard` sizes a card to its content between `WoRHubHUD::PARTY_CARD_MIN_WIDTH` / `_MAX_WIDTH` (the chip: `FIELD_ATTACK_CHIP_*`), names ellipsize, the HP bar fills the card's inner width; the gap is `PARTY_STRIP_GAP`. The chip comes from `WoRHubHUD::BuildFieldAttackChip` (the item's icon, else `WoRIcons::Find` of the weapon type / element badge, else the word; the element as a dot); `FFieldAttackEntry` gained `WeaponType`. The hub map hides the HP bar (D-216, `WoRPauseMenu::IsInRun`). `wor.UI.PrintHubHUD` lists the chip. | `fix/B-503i-party-strip-cards` |
| 2026-11-15 | B-503h (D-215): `UHubHUDWidget`'s field attack chip is the last child of the party strip's row and always up while the pawn has a `UFieldMovementComponent` (`RefreshFieldAttack` on the 1 s timer reads `GetFieldAttackEntry`); `ShowFieldAttack` (R / LB) sets the entry and lights the panel for `FieldAttackFlashSeconds`. `FieldSkillRules::FIELD_ATTACK_POPUP_SECONDS` is now unused (OBSOLETE, delete after PIE). | `feature/B-503h-field-attack-chip` |
| 2026-11-15 | B-503j (D-216): one party card for both HUDs. `FBattleHUDCard` moved to `UI/Combat/Battle/BattleHUDCard.h/.cpp` with `EBattleCardLevel` (Hub / Trial / Battle), `BattleHUDCard::GetParts(Level, bParty)` (what a level shows: Hub the name; Trial the HP bar, the build-up bar, the HP / EP lines; Battle everything, enemies no numbers), `Build` (the widgets once, into the owner's tree; returns the frame) and `Push`; the look is `BattleCardStyle`. `UBattleHUDWidget` builds at Battle (unchanged; `BuildCardWidgets` / `BuildBuildupWidgets` and the card half of `PushToWidgets` moved). `UHubHUDWidget` builds `MAX_PARTY_MEMBERS` cards at `WoRHubHUD::GetCardLevel` (a run: Trial, else Hub; read once, a map makes a new HUD) and fills them from `WoRHubHUD::BuildPartyStrip`, now `TArray<FBattleHUDCard>` (carried vitals, none = full; build-up 0, SB-18). The chip's frame is a `UBorder` in the card's colour (`MakeChipCard`); `UHubHUDCardFrame` is OBSOLETE (delete after PIE). Specs: `WoR.UI.BattleHUD` (a level each, and Build follows GetParts), `WoR.UI.HubHUD` (the trial numbers, the level per map). | `feature/B-503j-shared-party-card` |
| 2026-11-15 | B-503k (D-217): the field attack chip is gone. `UHubHUDWidget` builds each card in a column (`PartyColumns`, shown and hidden whole); at the Trial level only (`WoRHubHUD::ShowsFieldAttackStrip`) a strip in the card's look sits above each card, and the one above the played member's card (`FindFieldAttackCard` / `GetFieldAttackCardIndex`) shows `FFieldAttackEntry::Name`; `ShowFieldAttack` flashes it. Cards are bottom-aligned; `FBattleCardParts::bFixedWidth` is off at the Hub level, so the name-only card fits its name (no wrap). The battle card builds no gear line (`GearText` gone; `bGear` is the hover tooltip; `GearLine` stays as data). `FHubHUDFieldAttackChip` / `BuildFieldAttackChip` OBSOLETE (delete after PIE). Specs: `WoR.UI.BattleHUD`, `WoR.UI.HubHUD`. | `feature/B-503k-field-attack-strip` |
| 2026-11-15 | B-503m (Crown's PIE of B-503k): `WoRHubHUD::GetCardLevel` is by the map (`UTrialRunSubsystem::IsHubWorld`: Hub; any other map: Trial, run or not; no world: Hub), not `IsInRun` (a closed run's restore on the hub's first frame read as Trial; a trial map played from the editor had no run). `BattleHUDCard`: the HP number (`HPText`) is drawn on the HP bar and EP is its own bar (`EPBar`, `FBattleHUDCard::EPFraction`) with `EPText` on it, both `NumberBarHeight`; the separate lines are gone; an enemy HP bar keeps no number. Every card bar's empty part is `EmptyBarColor`; the build-up bar has a dim Status label. `ShowFieldAttack` no longer flashes the strip (`FieldAttackTimer` gone). Specs: `WoR.UI.BattleHUD`, `WoR.UI.HubHUD`. | `fix/B-503m-field-card-bars` |
| 2026-11-15 | B-503m4 (D-217, D-216): the field attack wheel sits above the strip, bottom left (`HubHUDStyle::WheelCentreFromBottomLeft`, mockup hub/hud/v2): one slice per loadout entry (`FieldSkillRules::GetFieldSliceStarts`, up to 9), the entry's name under a ◆ in its element's colour, the picked slice `FocusColor`; the middle (`WheelHub`) shows the option (`OptionName`, ◄ ► when the slice has more than one) over the strip's reading; `WheelHints` bottom right for the device in use (plain text). `WoRHubHUD::BuildPartyStrip(Party, PlayedSlot)` puts the played member's card first (`FindPlayedMemberSlot`); `BattleHUDCard::GetParts` builds the build-up bar at the Battle level only | `feature/B-503m4-wheel-on-rb` |
| 2026-11-15 | B-503m5 (D-218, Q-503m5): the field wheel is one round wheel: `UHubHUDWheelRing` (paint only, in `HubHUDWidget.h`) draws a dark disc, the picked slice lit, lines between slices and a middle circle (custom verts + lines); the slice names sit on it with no boxes; the middle shows only the strip's reading, between ◄ ► when the slice has options (`WheelResult` and the option word are gone). `UHubHUDWidget::GetFieldWheelSliceUnderCursor` reads the ring's geometry for the mouse. The controller makes the wheel a toggle (`IA_Field_WheelConfirm` / `IA_Field_WheelClose`), shows the cursor on keyboard and mouse (GameAndUI while up) and ignores move / look input while it is up | feature/B-503m5-wheel-toggle |
| 2026-11-15 | B-513 (Q-513): the field wheel's weapon slices show the weapon's name (`UWeaponData::GetDisplayName`, the type when empty); the centre and the strip keep the type. New `WoRGameNames::GetHeldWeaponTypeName(Weapon, bPaired)`: OffHandShield "{type} and Shield", a Dual weapon or a same-type pair "Dual {types}", else the type; read by `FieldMovementAttack` (swing names, so "Fire Sword and Shield") and the Equipment screen (`LoadoutV2Model` type icon and piece detail, `LoadoutV2ScreenWidget` pick-window lines). Spec in `WoR.Text.GameNames` | `feature/B-513-wheel-piece-names` |
| 2026-11-15 | B-513b: `WoRGameNames::GetHeldPiecesName(MainHand, OffHand)`, the pieces in hand by name ("{0} and {1}" for a separate off hand; a piece with no name gives its type); the field wheel's weapon slice and `LoadoutV2Model`'s entry row (was "A + B") read it. Spec in `WoR.Text.GameNames` | `feature/B-513b-joined-piece-names` |
| 2026-11-15 | B-514 (D-222): the field wheel is a quick strip. `UHubHUDWidget::ShowFieldAttackWheel(Entries, Picked, InHand)` builds a centred row of icon tiles (`StripTiles` and friends; the tile icon: `FFieldAttackEntry::SliceIcon` / `SliceOffHandIcon`, else `WoRIcons::GetWeaponType`, else `GetElementBadge`, else the name), the picked tile double-framed with up / down arrows, IN HAND on the kept entry, a dim behind, one frame around the row and its text: the piece's name, then `FieldSkillRules::GetFieldStripTypeLine` ("Ring · Lightning", from `FFieldAttackEntry::SliceType`). The cursor hit-test reads each tile's geometry. `AWoRHubController`: `IA_Field_StripMove` / `IA_Field_StripSource` (1D) replace the stick pick and the element step; the mouse wheel moves along the pieces; the hover picks only when the cursor moves. `UHubHUDWheelRing`, `GetFieldWheelSliceAt`, `GetFieldWheelIndexFromStick` OBSOLETE | `feature/B-514-quick-strip` |
| 2026-11-15 | B-514b (D-223): `WoRGameNames::GetAdapterName(Adapter, Socket)`: an adapter named by its socket ("Phoenix Ring", "Fire Ring"; empty: "Adapter Ring"). Read by the quick strip's ring slice (`FieldMovementAttack`) and `LoadoutV2Model` / `LoadoutV2ScreenWidget` (entry row, detail line, pick window, slot line). Spec in `WoR.Text.GameNames` | `feature/B-514b-adapter-socket-names` |
| 2026-11-16 | B-528 (D-218): the quick strip works in the hub. `UHubHUDWidget::BuildTree` calls `BuildFieldAttackWheel` on every map (it was built only with the trial card strips, so the hub opened nothing). The card strip above the played card stays trial only (D-216). Spec in `HubHUDSpec` | feature/B-528-hub-quick-strip |
| 2026-11-16 | B-531 (D-230): the loadout screen's Inventory tab, first in the row (the screen still opens on Equipment). `WoRLoadoutV2::BuildInventoryPage` (`LoadoutV2InventoryModel`) lists the member's owned pieces not in the active loadout, the evolutions not in use and the pouch, grouped under headers; A on a piece opens the Add window with it picked (`PutPieceInDraft`, shared with `PickItem`). Edit on an entry asks `BuildEditKinds` first; another kind replaces it on Y only (`ReplaceEntry` all or nothing). Stacking: `GetStackKey` + `GroupStacks`, one helper for the tab and the Add window's weapon and adapter lists. Look-only, step 1. | `feature/B-531-inventory-tab` |
| 2026-11-16 | B-530 (D-229): the adapter icon. `EWoRIconKind::Adapter`: `WoRIcons::Find` gives the socket frame (the table's EmptySocket), `FindCentre` the crystal's element badge inside (null: empty); `MakeIconStack` (MenuScreenStyle) layers them. Shown on the quick strip tile and the Equipment rows (entries, the Add window's adapter lines); look-only, step 1 for Crown. | `fix/B-530-hub-adapter-pick` |
| 2026-11-16 | B-535: the title opens at start, PIE included. `UTitleScreenWidget::OpenAtLaunch` (called by `AWoRHubController` after the root layout, no longer gated off in the editor) keys on the game instance, so once per play session and only on its first map load; `WoRTitleScreen::ShouldOpenAtStart` opens it only on `GameDefaultMap`, not under automation tests, and `wor.UI.TitleAtStart 0` skips it. `GTitleLeftThisLaunch` is no longer read (obsolete, retire after PIE) | `feature/B-535-title-at-start` |
| 2026-11-16 | B-536: the game menu's second key and Forms. `AWoRHubController::BuildGameMenuTabKey` makes `IA_UI_GameMenuTab` (Tab, Tap trigger, not eating the key) in `IMC_UI_GameMenuTab` at `FIELD_KEYS`, bound on Triggered to `HandleToggleMenu`; `UPauseMenuWidget` closes on Tab as on Back. `EPauseMenuPage::Forms` (after Character) opens `UFormsScreenWidget`. The screens table is in `InputMap.md` | `feature/B-536-menu-reaches-screens` |
| 2026-11-16 | B-537 (D-232): the menu level. `AWoRFrontEndGameMode` (no pawn, `PlayerCanRestart` false; a Blueprint child names the hub controller) is the level's game mode; `AWoRHubController::BeginPlay` opens the title there every load and skips the hub HUD. `WoRFrontEnd` (same header): `HasFrontEnd` (GameDefaultMap is not `HubLevel`), `IsFrontEndWorld`, `TravelToFrontEnd`, `GetQuitTarget`, `ShouldFallBackToFrontEnd`. Title Continue opens the hub from the menu level; the game menu Quit outside a run goes back (its warning says so); `AWoRGameModeBase` sends Play with no save to the menu level; `USaveSubsystem` settles a saved run in the first map after it. `OpenAtLaunch` (B-535) is obsolete once the ini step is done | `feature/B-537-front-end-level` |
| 2026-11-16 | B-538 (D-233): the mouse on the front-end screens. `UWoRActivatableWidget::ShowCursorNextTick` re-applies the screen input config one tick after activation (`RefreshUIInputConfig`), so a controller Blueprint BeginPlay that hides the cursor on the level first frame does not win; the title and new-game screens call it. `UNewGameScreenWidget`: hover colour on the card, chip or Begin under the cursor; `ClickPart` picks a body, class or element and takes the focus, Begin runs `Confirm` | `feature/B-538-mouse-front-screens` |
| 2026-11-16 | B-539 (D-234 point 3): the quick strip hint reads Enter Select (pad A Select); `IA_Field_WheelConfirm` gains Enter, and `HandleFieldWheelConfirm` takes the tile under the cursor only for a left click. `UNewGameScreenWidget::MoveSide`: Left from Class or Begin goes to Body | `feature/B-539-strip-enter-select` |
| 2026-11-16 | B-541 (D-234 points 1, 2): `WoRGameNames::GetBaseFormName` (Male Nobody / Female Nobody); `FFormsScreenRow::bBaseBody` / `BaseBody`: the Spirit page starts with the two own spirits, and `WoRFormsScreen::Wear` on one takes the form off then sets the base body; new-game label Starting spirit; pause row Spirit; `AHubMirror` (`Hub/HubMirror.h`, a `ATrialMapFeature`) opens `UFormsScreenWidget` | `feature/B-541-appearance-mirror` |
| 2026-11-16 | B-543 (D-230 addition, D-235): `EInventoryScreenTab::Loadout` (tabs Gear, Loadout, Pouch, Chest) opens `ULoadoutV2ScreenWidget` on the same member (`ClickMemberChip`); Gear tiles draw `WoRIcons::MakeAdapterIcon` (frame plus crystal, `IconCentre`); `UWoRRootLayout::SetPartyStripHidden` pauses `GEngine` on-screen messages (the old interact hints) while a screen is open, restored on close and in `NativeDestruct`; `{Help}` key hint (H / View) and `WoRMenuStyle::IsHelpKey`; the game menu hides its description panel until Help; the header chips sit right and list every balance (`GetPlaceCurrency` first, then Diamond, Gear, Skill, Elemental, Stat; `MaxChips` 7); `GetCurrencyName(PillarEssence)` reads Stat Essence. Known: Help state is widget-only (not in `GetPauseMenuString`); two root layouts share the one engine flag | `feature/B-543-menu-ui` |
| 2026-11-16 | B-544 (D-236): `WoRGameNames::GetHubPlaceName` (The Hub) is the hub place name for the game menu header (no subtitle; an empty subtitle collapses), the inventory place line (no separator without a subtitle), `UMemoryJournalSubsystem::GetCurrentPlace` and the title summary. Saved memory places keep their old text | `fix/B-544-hub-place-name` |
| 2026-11-16 | B-545: `AWoRHubController` takes its own input only while it has a pawn: `BeginPlay` with none calls `DisableInput(this)`; `SetPawn` enables it with a pawn and disables it without one (server and client). BP_HubPlayerController's look events (GetPawn, AddControllerPitch/YawInput) no longer run on the menu level or between levels. Screens read keys through Slate and CommonUI, so they are unaffected. No editor step | `fix/B-545-frontend-look-input` |
| 2026-11-16 | B-546 (D-228, D-229, stage 1, look only): the title is the character slots. `WoRTitleScreen::BuildRows(Save, MaxSlots)` gives the filled slot (leader name; `GetRunLevelsText`: the save's run levels for the leader, dashes with no run), the + while `GetMaxSlots` (`[WoR.Title] MaxCharacterSlots`, else `WoRNewGame::GetClassChoiceCount`, 12) allows, then Settings, Credits, Quit with `bButton`; `GetDeleteWarning`. `UTitleScreenWidget`: all centred (`TitleScreenStyle::SlotsAlignment` moves the slots left once a character figure stands on the right), bottom buttons, `MoveSide`, `RequestDelete` (R / Y, right-click), yes / no panels opening on No (`ChooseAnswer`); the 2-second hold is OBSOLETE. `WoRNewGame`: `GetRefractorChoices` (BD after Darkness), `GetWeaponChoices`, `GetCrystalChoices`, `TogglePick` (two picks, the oldest gives way), `GetBeginRefusal`; `FNewGamePick` gains `Name`, `bBrokenDarkness`, `Weapons`, `Crystals`. `UNewGameScreenWidget`: a name box (`UEditableTextBox`, 16 letters) and one pick grid per class. Begin still starts as a matching lord | `feature/B-546-title-slots-stage1` |
| 2026-11-16 | B-551 (D-231 addition): the wear detail line has no label (`LoadoutScreenModel` row and socket choice, `InventoryScreenModel` evolution tile); `ULoadoutV2ScreenWidget`'s detail box and the vendor screen's detail lines show a label-less detail as its value alone (a vendor gear row copies the loadout row's details) | `fix/B-551-wear-number-only` |
| 2026-11-17 | B-553 (D-233): the mouse on the remaining screens, each through the keys' own path: `ULoadoutV2ScreenWidget::NativeOnMouseWheel` (`MoveSelection`); `URecruitCardWidget` hover / click (`MoveSelection`, `PressInvite`); `UJoinRequestCardWidget` seat hover / click (`PressGive`, only while answering), its hints drawn as clickable caps (`MakeHintBar`), and the presenter shows the cursor while answering on keyboard and mouse (`CursorShownFor`, hidden again on let-go even if the card is gone); `UCharacterSheetWidget` member tab hover colour (`PaintMemberTabs`, no page rebuild); `UGateScreenWidget` hover (`MoveFocus`) and a gate tab click (`MoveGate`); `UInviteScreenWidget` hover (`LightRow`, no scroll under the cursor) | feature/B-553-mouse-remaining-screens |
| 2026-11-17 | B-554 (D-229): the title lists every filled character slot (`WoRTitleScreen::BuildSlotRows`, `SummarizeSlots`; `FTitleScreenRow::Slot`, `FTitleSaveSummary::Slot`), then the + while there is room; Continue calls `USaveSubsystem::SwitchToSlot`, the + always opens the new game screen, delete calls `DeleteSlot`. The slots are read from disk when the title opens and after a delete. The start-over yes / no is OBSOLETE (kept). The new game screen Begin builds the player into the first free slot and refuses when all are in use. | `feature/B-554-player-from-picks` |
| 2026-11-17 | B-559 (D-228): `WoRNewGame::GetWeaponChoices` (now by value) offers only the weapon types with a weapon under the class table Pick Weapons (`GetWeaponChoicesFor`); every type without any. | `feature/B-559-stocked-weapon-picks` |
| 2026-11-17 | B-562 (D-229, Q-561): a co-op guest joins from the game menu: `EPauseMenuPage::Join` (a client only, before Quit, refused once seated; `WoRPauseMenu::BuildPages` overload; unused page rows hidden); `UTitleScreenWidget::OpenToJoin` (the slot list without buttons, `WoRTitleScreen::BuildJoinRows`; a slot sends `ReadSlotPicks` via `SendJoin`; Back closes it); `UNewGameScreenWidget::OpenToJoin` (Begin sends the picks). Spec `WoR.Coop.JoinMenu`. | `feature/B-562-join-with-my-character` |
| 2026-11-17 | B-565 (D-225, trial pool stage 1): `UEnemyKitCardWidget` (the kit card, B closes) and `UDuplicateDropWidget` (Replace / Add / Convert; Back keeps both, Q-565; mouse hover and click), both on the root layout's menu layer; `UCombatResultsWidget::SetDroppedLines` (`DroppedText`, under the detail line). Previews `wor.UI.PreviewKitCard`, `wor.UI.PreviewDuplicate`, `wor.UI.PreviewFightReward`. Built unseen. See `TrialPool.md`. | `feature/B-565-trial-pool-look` |
| 2026-11-17 | D-229 one of each kind: `WoRNewGame::IsSameKind`, `ReadMadeCharacters` (each slot's own picks), `GetKindRefusal`; `Begin` refuses a kind already made; the new-game widget reads the made characters once per open and shows the refusal on the Begin card, dimmed. | `feature/D-229-one-of-each-kind` |
| 2026-11-17 | D-229 card names: `FBattleHUDCard::DetailLine` / `DetailText` (under the name, collapsed when empty); `BattleHUDView::GetCombatantName` reads `WoRPlayerBuild::GetCardName` (cards, turn order, `TurnOrderSlotWidget`); the turn title reads Your turn; `WoRHubHUD::BuildPartyStrip` takes the world; the pause menu party uses the card name; `WoRGameNames::GetYouWord`, `GetYourTurnText`; the battle HUD re-reads party card names at each turn-strip refresh (a claim or tag arriving late). | `feature/D-229-card-names` |
