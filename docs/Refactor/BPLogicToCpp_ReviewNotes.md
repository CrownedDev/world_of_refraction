# BP-logic → C++ overhaul — review notes

Branch `refactor/bp-logic-to-cpp` (from main `bce1317`). Written in a cloud session with **no Unreal Engine**: nothing here has been compiled or run in PIE. Expect a short fix-up round on the first build.

Direction: the review's §8 rule. C++ owns every flow; Blueprints and WBPs carry only layout, visuals and tuning values.

---

## How the cut-over works

- Every C++ → BP hook that held logic went from `BlueprintImplementableEvent` to **`BlueprintNativeEvent` with the same signature**, and gained a full C++ `_Implementation`.
- **While a BP override exists, it still wins**, so nothing changes in-game until you delete it.
- **Delete the override and the C++ takes over.** The signatures are unchanged, so existing BP graphs keep compiling.

| Hook | C++ now does | BP override to delete (to cut over) |
|---|---|---|
| `ACombatOrchestrator::OnCombatStartedUI` | Builds the whole combat HUD via `UCombatHUD` | `BP_CombatOrchestrator` → OnCombatStartedUI |
| `UCharacterPanelWidget::RebuildEffectsList` | Pooled text rows, spawned once (no TransBuffer risk) | `WBP_CharacterPanel` → RebuildEffectsList |
| `UCharacterPanelWidget::ApplyStaticText` | Name + "Class \| Element" + live M/B/S | `WBP_CharacterPanel` → ApplyStaticText (if overridden) |
| `UTurnOrderSlotWidget::InitialiseSlot` | Name, turn number, "+" bonus badge, active scale/opacity | `WBP_TurnOrderSlot` → InitialiseSlot |
| `UDefensePromptWidget::ShowPrompt/HidePrompt` (+ new `ShowCue`) | Whole prompt; works with **no WBP** | none (no WBP existed) |

Changes that are **live immediately** (no BP involved):
- **Results screen.** `ABattleGameMode::HandleCombatEnd` now shows `UCombatResultsWidget` and waits for a key/click or a 6 s timeout before `ExitEncounter`. If the widget can't be created, it returns immediately as before.
- **Death dim.** Character panels dim when their character dies.

Opt-in by reparenting:
- **`AExplorationPlayerController`.** E interacts with the nearest door, merchant or recruit, and `OnPossess` calls `EnsureLocalParty`.

---

## Commits (each ≤3 files, revertable on its own)

| Commit | What |
|---|---|
| `f7d70c1` | DefensePrompt: native, bound to DefenseSystem cue/open/close, builds its own text |
| `0f4cf4c` | `UCombatHUD`: creates panels/strip/menu/prompt as standalone viewport widgets |
| `d62e1b9` | Orchestrator: `OnCombatStartedUI` → NativeEvent building `UCombatHUD`; teardown on ForceEnd/EndPlay |
| `f5b2206` | CharacterPanel: native effects list + static text + death dim |
| `820a5d5` | TurnOrderSlot: native data binding |
| `122f8fc` | `UCombatResultsWidget` (native overlay) |
| `f6d088c` | BattleGameMode shows results before returning to trial |
| `1014628` | `AExplorationPlayerController` (hub/trial interact + party ensure) |
| `270e2ac` | `NativeUIDebug` + `wor.UIState` console command |

New files:
- `UI/Combat/CombatHUD.h/.cpp`
- `UI/Combat/CombatResultsWidget.h/.cpp`
- `Hub/ExplorationPlayerController.h/.cpp`
- `Debug/UI/NativeUIDebug.h/.cpp`

---

## On your return — in order

### 1. Build
- Compile in the IDE and fix whatever surfaces. The compile-review findings are already folded in (see the bottom of this doc).
- If UHT complains about a NativeEvent, check that the `_Implementation` signature matches exactly.

### 2. Before deleting anything, look
- Open the `BP_CombatOrchestrator` OnCombatStartedUI graph. **If it does anything besides building widgets**, port that part first or keep it in a smaller BP event: weather, input mode, camera, `SetCombatOrchestrator`, etc. That was the main unknown in review §9.
- Do the same check for the hub PC's interact graph.

### 3. Cut over, one at a time, PIE between each
1. **Combat HUD.** Delete the `BP_CombatOrchestrator` → OnCombatStartedUI override, then PIE a battle and run `wor.UIState`.
   - **Expect:** panels bottom-left (yours) and top-right (enemies), turn strip top-centre, command menu bottom-right.
   - **Watch for a double command menu.** The menu header says "called by the owning character actor after AddToViewport", so a character BP may also create one. If so, delete that too.
2. **Effects list.** Delete the `WBP_CharacterPanel` → RebuildEffectsList override. If the second `/Game/UI/Combat/WBP_CharacterPanel` is the live one, set `CharacterPanelClass` in config (see below).
3. **Turn slot.** Delete the `WBP_TurnOrderSlot` → InitialiseSlot override.
4. **Hub PC.**
   - Class Settings → Parent Class → `ExplorationPlayerController` on `BP_HubPlayerController`, and on the trial PC.
   - Delete their IA_Interact and `EnsureLocalParty` nodes.
   - Run `wor.UIState` near a door to see the candidates.

### 4. Tune
- Layout: `CombatHUDLayout` in `CombatHUD.cpp`.
- Colours and sizes: `PanelStyle`, `TurnSlotStyle`, `CombatResultsStyle`, `DefensePromptText`.
- All marked `// TODO: style pass` / `PIE-tune`.

### 5. Clean up later (not done, per the incremental rule)
- Delete the dead BP graph nodes.
- Delete the now-unused container WBPs (`WBP_PlayerPanelContainer`, `WBP_EnemyPanelContainer`, `WBP_TurnOrderStripContainer`) if nothing else references them.
- Delete the duplicate `WBP_CharacterPanel`.

---

## Config overrides (no editor needed)

`UCombatHUD` is `Config=Game`. If an asset path is wrong or moves, add this to `Config/DefaultGame.ini`:

```ini
[/Script/world_of_refraction.CombatHUD]
CharacterPanelClass=/Game/UI/Combat/WBP_CharacterPanel.WBP_CharacterPanel_C
TurnOrderStripClass=/Game/UI/Combat/Turn/WBP_TurnOrderStrip.WBP_TurnOrderStrip_C
CommandMenuClass=/Game/UI/Combat/CommandMenu/WBP_CombatMenu.WBP_CombatMenu_C
; DefensePromptClass=  (empty = native prompt)
```

Defaults in code:
- **Character panel:** tries `/Game/UI/Combat/Character/WBP_CharacterPanel` first, then `/Game/UI/Combat/WBP_CharacterPanel`. A class that isn't a `UCharacterPanelWidget` child resolves to null, so the wrong copy falls through.
- **Turn strip:** `WBP_TurnOrderStrip`, not the `…Container` variant.

---

## Known gaps / judgment calls (flag if wrong)

- **Positions are guesses.** The C++ HUD places widgets by anchor instead of using the container WBPs, whose contents were unknown.
- **Win path leaves the HUD up** under the results screen; the level swap removes it. Aborts (`ForceEndCombat`) tear it down immediately.
- **Results screen:**
  - Keyboard-to-skip depends on widget focus under the battle PC's input mode. The auto-continue timer guarantees it never strands you.
  - Victory and Defeat still return to the same place (review Q2).
- **Defense prompt** text is generic ("DEFEND! Block / Parry / Dodge"). It doesn't name the key bindings.
- **No Prisms/reward on win.** That's a design call (review Q6), not part of this BP pass.
- **Not touched:** shop rows (already C++), command button/menu cosmetic hooks (`OnDataChanged`, `OnButtonsRefreshed`, kept as cosmetic), and Arc A bug fixes (separate branch).
