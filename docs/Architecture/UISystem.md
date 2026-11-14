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
- **Widget:** `ULoadoutScreenWidget` (`UI/Menus/LoadoutScreenWidget.h`): character tabs (LT / RT), page tabs, budget pips, the rows in a scroll box (made as a page first needs them, refilled after), the detail panel. Opened by the game menu's Loadout page or `wor.UI.Loadout`.
- **Names:** `WoRGameNames::GetCrystalTypeName`, `GetWeaponTypeName`, `GetWieldModeName`, `GetTierLetter` (D-062); the `FString` tables stay for logs and the old widgets.
- **Shared look:** `Private/UI/Menus/MenuScreenStyle.h` (`WoRMenuStyle`): the colours, font sizes, menu keys and small builders every C++-built menu screen uses.
- **Debug:** `wor.UI.PrintLoadout` (`GetLoadoutScreenString`: every member's rows, sockets and costs).

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
| 2026-11-14 | B-476b: the v2 loadout screen, part 1. `ULoadoutV2ScreenWidget` (code-built, beside `ULoadoutScreenWidget`; `wor.UI.LoadoutV2`): the Weapons tab from `WoRLoadoutV2::BuildWeaponsPage` (entries, sockets, ACTIVE, the Add weapon frame, cells) and the Add gear window's Main hand list (`BuildMainHandChoices`, new: each owned weapon not held, checked as a draft), A Pick / Remove, X Compare (`BuildCompare`), Y Confirm through `TryChangeGear` + `ApplyDraft`; spec `WoR.UI.LoadoutV2Screen`. The rest of v2 is B-476c | `feature/B-476b-loadout-v2-weapons-tab-cloud` |
| 2026-11-14 | B-476c: the v2 loadout screen, part 2. `ULoadoutV2ScreenWidget` takes the B-479c layout (entries left, `WoRLoadoutV2::BuildEntryDetails` in a box right; X Simple / Full, a session static) and a mode machine (`ELoadoutV2Mode`: List, Options, WindowKind, WindowSlots, WindowItems, WindowAsk): the Add gear window for a new entry (`BuildAddKinds`, `BuildMainHandChoices`, `BuildAdapterChoices`, `BuildEvolutionChoices`, `CheckDraft` / `ApplyDraft`) or an edited one (`DraftFromEntry`, `CheckEntryEdit`, `ReplaceEntry`; A Remove: `RemoveEntry`), sockets through today's `WoRLoadoutScreen::BuildSocketChoices` / `ApplySocketChoice` / `TakeOffSocket`, the evolution's ask, L3 / 6 Set active (`SetRowActive`), Y Options with `RemoveAllGear`. Q-476c. The tabs are B-476d | `feature/B-476c-loadout-v2-rest-cloud` (UNBUILT) |
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
| 2026-11-14 | B-440d (Q-440d): the loadout screen, piece 4: the Skills page. New `WoRLoadoutSkills` (`LoadoutSkillsModel.h`): `BuildSkillsPage` (a Refractor's innate school tabs on the points budget, then a tab per weapon's skill slots and adapter's spell slots), `BuildSkillChoices` (learned skills, a weapon's built-ins; innate refused over the points), `PutSkill` / `ApplySkill` (through `TryChangeGear`, then `WriteLoadoutToOwned`); rows are `ELoadoutScreenRowKind::Skill` / `EmptySkill` with `SkillSlot`, `Skill`, `bPick`. `WoRLoadoutScreen` exposes `FindRowGear`, `FindMemberLoadout`. `ULoadoutScreenWidget`: `SwitchPage` (LB / RB), `MoveSkillTab` (Left / Right), the points line in the budget row. Debug `wor.UI.PrintSkills`. Spec `WoR.UI.LoadoutSkills` (4 cases) | feature/B-440d-loadout-skills-page |
| 2026-11-14 | B-440c (Q-440c): the loadout screen, piece 3: the socket picker. `WoRLoadoutScreen`: `LoadoutScreenSockets` (CRYSTAL, STONE), `FLoadoutScreenSocket::bEvolution`; `BuildSocketChoices` (carried gems or stones from the member's pouch, owned evolutions; budget after each; refused: over budget, a second S-tier, an evolution in use), `PutSocketChoice` (the preview), `ApplySocketChoice` (`UInventoryComponent::AttachCrystalTo*` / `AttachEvolutionTo*`), `GetTakeOffSocketWarning` and `TakeOffSocket` (`RemoveCrystalFrom*`, both sockets, used up, CL-28b; `RemoveEvolutionFrom*`). `ULoadoutScreenWidget`: Left / Right lights a socket (`MoveSocket`), A on an empty socket opens the picker in the list, X asks then takes off. Spec `WoR.UI.LoadoutScreen` (15 cases, 2 new) | feature/B-440c-loadout-socket-picker |
| 2026-11-14 | B-474 (D-207): the battle menu's Abilities list adds the other weapon's skills after the weapon in hand's, named "Skill (Weapon)", while "Switch to the weapon of the skill you use" is on (`BuildAbilitySubmenu`, `FCombatCapabilities::OtherWeaponAbilities`); Switch Weapon's gamepad key is L3. The loadout screen: L3 / 6 Set active (`ULoadoutScreenWidget::PressSetActive`), in the hint line. | `feature/B-474-weapon-follows-skill-cloud` (UNBUILT) |
| 2026-11-14 | B-476, first part (D-206, D-209, D-210; UIScreens #2-3 v2): the v2 loadout screen's model, `UI/Menus/LoadoutV2Model` (`WoRLoadoutV2`): `BuildWeaponsPage` (entries by name, a pair "A + B", socket icons, the weapon in hand, the evolution line, "Cells used n of m", the Add frame's "n/m left", `bCanAdd` by the class's entry limits), the Add gear draft (`FLoadoutV2Draft`; `ApplyDraft` to the next free place, `CheckDraft` with the refusals in the game's words), `BuildOffHandChoices` (greyed with the reason), `BuildCompare` (power, built-ins gained / lost). Debug `wor.Loadout.PrintV2`; spec `WoR.UI.LoadoutV2`. The widget is B-476b; today's screen stays. | `feature/B-476-loadout-v2-model-cloud` (UNBUILT) |
