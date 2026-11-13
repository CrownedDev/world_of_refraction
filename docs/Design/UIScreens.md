# UI Screens — every screen outside the battle HUD

**Status:** design (2026-11-13), docs and mockups only, no widgets. Branch `docs/ui-screens-all`.
**Replaces** the screen list in `UI_ScreenInventory.md` (2026-10-25), which is out of date; that file keeps its asset shopping list.
**The battle HUD is done** (`BattleHUDRefinePlan.md`, B-436) and is not in this list.
**Look:** the battle HUD's theme, used loosely (Crown via the brain, 2026-11-13): dark stone panels, one cut corner, **cyan = focus**, tab rows, coloured face-button glyphs, hints at the bottom right. Each screen is laid out for its own job. Final art comes from the Hidris art pass (`UI_Theme.md`); these mockups are layout, information and controls only.
**Mockups:** `screen_mockups/<area>/<screen>/` (one `make.py` each, run `python make.py`; shared kit `screen_mockups/_kit/wor_ui.py`; DejaVu stands in for Spectral SC and Lato).

## How to read the table

- **Code:** paths are under `Source/world_of_refraction/`. Everything cited is on `dev` (2026-11-13). The cloud branches listed in the brief (apparel, rest spot and its vote, quests, curses, item slots, End Turn hide, turn view, boss-cinematic skip) **are already merged into `dev`** and wait for Crown's PIE pass (Backlog: `REVIEW (in dev)`); no screen-facing code was found only on an unmerged branch.
- **Today:** *Real* = C++ base + WBP; *Placeholder* = plain C++ widget built in code, to be replaced by the CommonUI screen (D-024); *Console* = only a `wor.*` command; *None*.
- **Priority:** **P1** = needed to play one full run (hub → gate → trial → end) without the console, or Crown asked for it; **P2** = a hub service or co-op step the slice uses; **P3** = later or polish.
- Player-facing words are the code's (`WoRGameNames.cpp`, the widgets' `LOCTEXT`). Two notes: the **hub money shows as Gold and the run money as Prisms** (D-159; the code's `ECurrencyType` names are swapped on purpose), and **Reality** still prints "Unknown" in some tables (B-269, a known bug); mockups say Reality.

## Screens

### Menus reached from anywhere

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 1 | **Pause / game menu** | Opens with Menu / Esc. Tabs into every menu below; Resume, Settings, Quit to title. In a trial it also shows the run (Prisms, wipes left) | `IA_ToggleMenu` (InputMap, built); CommonUI stacks `UWoRRootLayout::PushMenu` `Public/UI/WoRRootLayout.h:57` | None | P1 |
| 2 | **Loadout (gear)** | Per character: weapons (Generic 2, one active), adapters, the Refractor's one item, 2 apparel slots; sockets on each (crystal or evolution; stone); the budget meter; up to 5 saved loadouts | `ULoadoutComponent` `Public/Loadout/LoadoutComponent.h:141-286` (Create/Duplicate/Rename/SetActive, `ValidateLoadout` :214, `GetValidationErrors` :218); costs `Public/Loadout/LoadoutConstants.h:31-47`; attach `UInventoryComponent::Attach*` `Public/Inventory/InventoryComponent.h:335-449`; apparel `FApparelEntry.h:70-84`, `UPoolSubsystem::AttachToApparel` `Public/Pool/PoolSubsystem.h:222` | Console (`wor.Loadout.*`, `LoadoutRulesDebug.cpp:36-458`; apparel `:217`, `:260`) | P1 |
| 3 | **Skills (loadout page)** | Pick which abilities and spells each item gives; the Refractor's innate spells; see school, element, cost | `TryAssignWeaponAbility` / `TryAssignWeaponSpell` / `TryAssignRingSpell` / `TryAssignInnateSpell` `LoadoutComponent.h:270-280` | Console (`wor.Loadout.ReplaceSkill`) | P1 |
| 4 | **Inventory and pouch** | Per character: gear and pouch; the shared home chest. Verbs Give, Store, Take, Dismantle, Convert (D-131) | `WoRInventoryVerbs` `Public/Inventory/InventoryVerbs.h:58-65`; `WoRGive` `Public/Inventory/GearGive.h:30-63` | Placeholder `UInventoryWidget` `Public/UI/Hub/InventoryWidget.h:21` (`wor.UI.Inventory`); old `UStorageWidget`, `UGiveWidget`, `UDismantleWidget`, `UDuplicateGearWidget` are OBSOLETE | P1 |
| 5 | **Character sheet** | Class, element, Mind / Body / Spirit, substats, resistances by type (D-168: All, Physical, Spell, each element), Max Actions | `UCharacterDataComponent`; resistance scopes (`ResistanceSystem.md`) | None | P2 |
| 6 | **Journal** | Pages: Memories, Lore, Quests (Main, Side, Hidden), Conversations, Talk to V. A hidden quest appears only once found and shows only its current step (D-171) | `UMemoryJournalSubsystem` `Public/Memory/MemoryJournalSubsystem.h:64-87`; `UHiddenQuestSubsystem::GetVisibleQuests` `Public/Memory/HiddenQuestSubsystem.h:100`; `EQuestKind` `HiddenQuestData.h:27` | Placeholder `UMemoryJournalWidget` `Public/UI/Hub/MemoryJournalWidget.h:40` (`wor.Journal.Open`, hub `AHubJournalStation`) | P2 |
| 7 | **Party** | Order the party (up to 3), who plays each member (you, a friend, AI and its difficulty), start cell, field skill, guard style | `UPartySessionSubsystem` `Public/Party/PartySessionSubsystem.h:146-252` | Placeholder `UPartyControlWidget` `Public/UI/Party/PartyControlWidget.h:24` (`wor.UI.PartyControl`); hub strip `UPartyDisplayWidget` (Real) | P2 |
| 8 | **Settings** | Gameplay (**control model: Shortcuts / Omni D-pad**, Turn View, hide End Turn, hide corner hints, skip boss cinematics), camera, display, audio, controls (key list), accessibility | Saved in `FSavedSettings` `Public/Save/WoRSaveTypes.h:821`; `USaveSubsystem::SetTurnViewStyle` / `SetEndTurnButtonHidden` `Public/Save/SaveSubsystem.h:127-134`; turn-view names `CombatCameraTypes.cpp:264-277`; control model is B-436l (a console variable until this screen) | Console (`wor.Camera.Style`, `wor.UI.HideEndTurn`) | P1 |

### Front end

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 9 | **Title / main menu** | Press any button; Continue, New Game, Join a friend, Settings, Credits, Quit | `USaveSubsystem` (one slot per local player, `Public/Save/WoRSaveGame.h:2`, so no slot screen) | None | P2 |
| 10 | **Save notices** | Autosave icon; the one-time "your old save was reset" warning (D-060) | `UWoRSaveGame::WasWiped` `Public/Save/WoRSaveGame.h:35` (never shown today) | None | P2 |
| 11 | **New game: first character** | Pick your character (the lords / templates), class reads from it | `DA_Character_*`; `ECharacterClass` | None | P3 (lore: who you start as is Crown's) |
| 12 | **Loading screen** | Only for hub ↔ trial (FeelGuide 10): an item or character description and a slow crystal turn | none (no loading-screen code) | None | P3 |
| 13 | **Credits** | Scroll | — | None | P3 |

### Hub

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 14 | **Hub HUD and interact prompt** | Gold / currencies on change, party strip, "E Talk / Rest / Enter" prompt | `UCurrencyComponent::OnCurrencyChanged` `Public/Currency/CurrencyComponent.h:118`; `UPartyDisplayWidget` | Party strip Real; prompts are debug text | P2 |
| 15 | **Shop (every vendor)** | Buy (for yourself or another member, D-111), sell, cart; the vendor's tabs add its services | `UMerchantShopSubsystem::OpenForMerchant` `Public/Shop/MerchantShopSubsystem.h:36`; `UEconomyService::Purchase` / `PurchaseFor` `Public/Currency/EconomyService.h:262-273`; vendors `EMerchantType` `Public/Merchant/MerchantData.h:26-35` | Real `UShopWindowWidget` `Public/UI/Shop/ShopWindowWidget.h:54` (old UMG modal, not CommonUI) | P2 |
| 16 | **Blacksmith services** | Weapons and augment stones (Stone Smith merged in, D-173): dismantle, merge | `DismantleWeapon` `EconomyService.h:151`; `MergeCrystals` :141 | Console (`wor.Economy.Merge`) | P2 |
| 17 | **Jeweler services** | Adapters; **buy item slots** (+1 pouch slot per character) | `ItemSlotUpgrade::TryBuy` `Public/Inventory/ItemSlotUpgrade.h:40` | Placeholder `UItemSlotUpgradeWidget` `Public/UI/Hub/ItemSlotUpgradeWidget.h:21` (`wor.UI.ItemSlots`) | P2 |
| 18 | **Spiritualist services** | Evolutions, crystals, **repair** (quote then pay), remove an evolution, **clear a curse** | `GearRepair::RepairParty` `Public/Equipment/Durability/GearRepair.h:45`; `RemovePrimaryEvolution` `EconomyService.h:206`; `UCurseSubsystem::PayToClearCurse` `Public/Memory/CurseSubsystem.h:42` | Curse: Placeholder `UCurseClearWidget` `Public/UI/Hub/CurseClearWidget.h:18` (`wor.UI.ClearCurse`); repair: Console | P2 |
| 19 | **Skins (forms)** | Form meters, reveal at 100%, wear a skin; hub and rest spots only | `UCharacterFormSubsystem::TryWearForm` `Public/Character/Forms/CharacterFormSubsystem.h:58` | Placeholder `UCharacterFormWidget` `Public/UI/Hub/CharacterFormWidget.h:20` (`wor.UI.Forms`) | P3 |
| 20 | **NPC talk (V, characters)** | Speaker, line, choices; V's lines and character conversations | `UStoryLinesData` `Public/Story/StoryLinesData.h` | Journal only | P3 (the words are Crown's) |

### Run (trial)

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 21 | **Trial gate** | Pick the variation and the difficulty (Easy / Medium / Hard: enemy damage and payout); see who's going; Enter | `UTrialRunSubsystem::SetGateChoice` `Public/Trial/TrialRunSubsystem.h:77`; `ETrialDifficulty` `Public/Trial/TrialGate.h:19` | Placeholder `UTrialGateWidget` `Public/UI/Trial/TrialGateWidget.h:20` (from `ATrialDoor`) | P1 |
| 22 | **Rest spot** | Rested (heal, revive); Level up; Inventory / Loadout; Revive the mini-boss (co-op: a vote); Invite; Hand over host; Leave the trial (extract, reduced payout) | `WoRRestSpot::Rest` / `LeaveRun` `Public/Trial/TrialRestSpot.h:25-41`; `AWoRGameState::StartReviveVote` / `CastReviveVote` `Public/Player/WoRGameState.h:55-66`; `GetRunEndPayout` `TrialRunSubsystem.h:280` | Placeholder `URestSpotWidget` `Public/UI/Hub/RestSpotWidget.h:18` (`wor.Trial.Rest`, `wor.Trial.Vote`) | P1 |
| 23 | **Level up (place levels)** | Place each banked pillar level on Mind / Body / Spirit, per member; buy a point | `WoRLevelDrops::TryPlaceLevel` / `TryBuyPoint` `Public/Trial/LevelDropPlacement.h:40-48` | Placeholder `ULevelDropWidget` `Public/UI/Hub/LevelDropWidget.h:20` (`wor.UI.LevelDrop`) | P1 |
| 24 | **Extract confirm** | "Full rewards only for beating the boss or falling in battle" (RunStructure §6); shows earned so far and the cut | `GetLeaveConfirmText` `TrialRestSpot.h:41` | Inside the rest placeholder | P1 |
| 25 | **Reward popups (after a fight)** | No results screen (FeelGuide 6): popups fade on their own; level-ups, rare drops, memories, quests | `OnCombatResultReady` `Public/Combat/CombatOrchestrator.h:98`; `OnLevelDropsReady` :109; `OnMemoryRecorded` `MemoryJournalSubsystem.h:34`; quest notices `HiddenQuestSubsystem.cpp:35-64` | `UCombatResultsWidget` (VICTORY / DEFEAT text only) | P1 |
| 26 | **Wipe and run over** | A wipe: pillar points lost, back at the last rest spot, wipes left (FeelGuide 7). Run over: the payout, what you keep and lose | `RecordWipe` `TrialRunSubsystem.h:313`; `DefeatRules` `Public/Trial/DefeatRules.h`; `FRunPayout` `Public/Trial/RunRewards.h:21` | Console (`wor.Trial.PrintLosses`, `wor.Trial.PrintRewards`) | P1 |
| 27 | **Run end (boss beaten / extracted)** | The payout tally, then back to the hub | `GetRunEndPayout` | None (needs a run-end delegate) | P1 |
| 28 | **Trial HUD** | Small: Prisms, wipes left, party HP; a quest step line when tracked | `GetRunTally` `TrialRunSubsystem.h:277` | None | P2 |
| 29 | **Recruit card** | A character met in the trial: who they are, Invite for this run | `UPartySessionSubsystem::InviteRunMember` `PartySessionSubsystem.h:146` | Console (`wor.Party.RecruitForRun`) | P2 |
| 30 | **World-stat draft** | Pick 3 of 5 | `OnWorldStatDraftReady` `CombatOrchestrator.h:106` | None (nothing listens; may be retired by levelling at rest spots, B-363) | P3, ask |

### Co-op

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 31 | **Invite / host** | Host, friends list, Invite, Seat a friend | `UWoRSessionSubsystem` `Public/Online/WoRSessionSubsystem.h:41-78` | Placeholder `UInviteWidget` `Public/UI/Party/InviteWidget.h:20` (`wor.UI.Invite`) | P2 |
| 32 | **Join request (host side)** | A player asks to join: give them an AI seat or refuse | `URunStateComponent::AcceptJoin` / `RefuseJoin` `Public/Player/RunStateComponent.h:152-157` | Placeholder `UJoinSeatWidget` `Public/UI/Party/JoinSeatWidget.h:20` (`wor.UI.JoinSeat`) | P2 |
| 33 | **Host hand-over / host lost** | Planned hand-over from the rest menu; "Host left, reconnecting" when it isn't planned | `HostSuccession::BeginHandOver` / `GetStep` `Public/Online/HostSuccession.h:88-102` | Console (`wor.Coop.SimulateHostDrop`) | P2 |
| 34 | **Revive vote** | Everyone votes yes / no on reviving the mini-boss; tally shown | `OnReviveVoteChanged` `WoRGameState.h:66` | Inside the rest placeholder | P2 (drawn with 22) |

## Mock order

Highest priority first. Ticked = mocked on this branch (folder in brackets).

1. [ ] Pause / game menu (`menus/pause`)
2. [ ] Loadout and skills (`menus/loadout`)
3. [ ] Inventory and pouch (`menus/inventory`)
4. [ ] Rest spot, level up, extract, revive vote (`run/rest_spot`)
5. [ ] After a fight, wipe, run over, run end (`run/run_end`)
6. [ ] Settings (`menus/settings`)
7. [ ] Trial gate (`run/gate`)
8. [ ] Shop and the vendor services (`hub/vendors`)
9. [ ] Journal (`menus/journal`)
10. [ ] Party and co-op (`menus/party`, `coop/`)
11. [ ] Title and save notices (`front/title`)
12. [ ] Character sheet (`menus/character`)
13. [ ] Hub HUD, prompt, loading, NPC talk (`hub/hud`)

## Shared menu controls

Every menu uses the same buttons, so nothing has to be learned twice (Expedition 33, Persona 5 and Elden Ring all keep one tab / confirm / back set across menus). Gamepad first (pillar 9); mouse clicks every tab and hint.

| Verb | Gamepad | Keyboard / mouse |
|---|---|---|
| Select | A | Enter / click |
| Back | B | Esc / right click |
| Page tabs (top row) | LB / RB | Q / E |
| Sub-tabs (second row, e.g. a character) | LT / RT | Z / C |
| The screen's own action (e.g. Unequip, Give) | X | F |
| The screen's second action (e.g. Compare, Sort) | Y | R |
| Details / description | R3 (right stick click) | V / hover |
| Move | D-pad / left stick | Arrows / WASD / mouse |

## Proposed InputMap rows

**Not added to `InputMap.md`** (the brief); a builder adds each row there in the job that builds the screen. Menus run in their own mapping context (proposed `UI menu`, priority above exploration and combat), so Q / E / R / F here don't clash with the field keys of the same letters.

| Action | Keyboard / mouse | Gamepad | Input action | Screen |
|---|---|---|---|---|
| Open the game menu (exists) | Esc | Menu (Options) | `IA_ToggleMenu` | 1 |
| Menu select | Enter / click | A (Cross) | CommonUI Click | all |
| Menu back | Esc / right click | B (Circle) | CommonUI Back | all |
| Page tab previous / next | Q / E | LB / RB (L1 / R1) | `IA_UI_TabPrev`, `IA_UI_TabNext` | all |
| Sub-tab previous / next | Z / C | LT / RT (L2 / R2) | `IA_UI_SubTabPrev`, `IA_UI_SubTabNext` | all |
| Screen action | F | X (Square) | `IA_UI_Action` | all |
| Screen second action | R | Y (Triangle) | `IA_UI_Action2` | all |
| Details | V | R3 | `IA_UI_Details` | all |

Screen-specific rows are added below as each screen is mocked.

## Open questions for Crown

Collected as screens are drawn; lore and story questions are listed, not answered.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-13 | Created: 34 screens from `dev`'s code (the cloud features are already merged), mock order, shared menu controls, proposed input rows | docs/ui-screens-all |
