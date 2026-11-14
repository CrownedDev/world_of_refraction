# UI Screens — every screen outside the battle HUD

**Status:** layouts approved for now by Crown (2026-11-13); final art from the Hidris pass. Crown approves the screens one by one; the approved ones and their build jobs are in **Approved screens** below. Branch `docs/ui-screens-all`, merged into `dev`.
**Replaces** the screen list in `UI_ScreenInventory.md` (2026-10-25), which is out of date; that file keeps its asset shopping list.
**The battle HUD is done** (`BattleHUDRefinePlan.md`, B-436) and is not in this list.
**Look:** the battle HUD's theme, used loosely (Crown via the brain, 2026-11-13): dark stone panels, one cut corner, **cyan = focus**, tab rows, coloured face-button glyphs, hints at the bottom right. Each screen is laid out for its own job. Final art comes from the Hidris art pass (`UI_Theme.md`); these mockups are layout, information and controls only.
**Mockups:** `screen_mockups/<area>/<screen>/` (one `make.py` each, run `python make.py`; shared kit `screen_mockups/_kit/wor_ui.py`; DejaVu stands in for Spectral SC and Lato).
**PNGs:** committed through LFS from a machine that can reach it; a cloud session on the Default environment can't push them (`lfs.github.com` is blocked there; the `world_of_refraction` environment allows it).

## How to read the table

- **Code:** paths are under `Source/world_of_refraction/`. Everything cited is on `dev` (2026-11-13). The cloud branches listed in the brief (apparel, rest spot and its vote, quests, curses, item slots, End Turn hide, turn view, boss-cinematic skip) **are already merged into `dev`** and wait for Crown's PIE pass (Backlog: `REVIEW (in dev)`); no screen-facing code was found only on an unmerged branch.
- **Today:** *Real* = C++ base + WBP; *Placeholder* = plain C++ widget built in code, to be replaced by the CommonUI screen (D-024); *Console* = only a `wor.*` command; *None*.
- **Priority:** **P1** = needed to play one full run (hub → gate → trial → end) without the console, or Crown asked for it; **P2** = a hub service or co-op step the slice uses; **P3** = later or polish.
- Player-facing words are the code's (`WoRGameNames.cpp`, the widgets' `LOCTEXT`). Two notes: the **hub money shows as Gold and the run money as Prisms** (D-159; the code's `ECurrencyType` names are swapped on purpose), and **Reality** still prints "Unknown" in some tables (B-269, a known bug); mockups say Reality.

## Screens

### Menus reached from anywhere

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 1 | **Pause / game menu** | Opens with Menu / Esc. Tabs into every menu below; Resume, Settings, Quit to title. In a trial it also shows the run (Prisms, wipes left) | `IA_ToggleMenu` (InputMap, built); CommonUI stacks `UWoRRootLayout::PushMenu` `Public/UI/WoRRootLayout.h:57` | Real, C++ (B-439): `UPauseMenuWidget` `Public/UI/Menus/PauseMenuWidget.h`; waits for Crown's PIE pass | P1 |
| 2 | **Loadout (gear)** | Per character: weapons (Generic 2, one active), adapters, the Refractor's one item, 2 apparel slots; sockets on each (crystal or evolution; stone); the budget meter; up to 5 saved loadouts | `ULoadoutComponent` `Public/Loadout/LoadoutComponent.h:141-286` (Create/Duplicate/Rename/SetActive, `ValidateLoadout` :214, `GetValidationErrors` :218); costs `Public/Loadout/LoadoutConstants.h:31-47`; attach `UInventoryComponent::Attach*` `Public/Inventory/InventoryComponent.h:335-449`; apparel `FApparelEntry.h:70-84`, `UPoolSubsystem::AttachToApparel` `Public/Pool/PoolSubsystem.h:222` | Console (`wor.Loadout.*`, `LoadoutRulesDebug.cpp:36-458`; apparel `:217`, `:260`) | P1 |
| 3 | **Skills (loadout page)** | Pick which abilities and spells each item gives; the Refractor's innate spells; see school, element, cost | `TryAssignWeaponAbility` / `TryAssignWeaponSpell` / `TryAssignRingSpell` / `TryAssignInnateSpell` `LoadoutComponent.h:270-280` | Console (`wor.Loadout.ReplaceSkill`) | P1 |
| 4 | **Inventory and pouch** | Per character: gear and pouch; the shared home chest. Verbs **Give, Store, Take** (D-177: Dismantle and Convert moved to the vendors; changes D-131) | `WoRInventoryVerbs` `Public/Inventory/InventoryVerbs.h:58-65`; `WoRGive` `Public/Inventory/GearGive.h:30-63` | Placeholder `UInventoryWidget` `Public/UI/Hub/InventoryWidget.h:21` (`wor.UI.Inventory`); old `UStorageWidget`, `UGiveWidget`, `UDismantleWidget`, `UDuplicateGearWidget` are OBSOLETE | P1 |
| 5 | **Character sheet** | Class, element, Mind / Body / Spirit, substats, resistances by type (D-168: All, Physical, Spell, each element), Max Actions | `UCharacterDataComponent`; resistance scopes (`ResistanceSystem.md`) | Real, C++ (B-442): `UCharacterSheetWidget` `Public/UI/Menus/CharacterSheetWidget.h`; waits for Crown's PIE pass | P2 |
| 6 | **Journal** | Pages: Memories, Lore, Quests (Main, Side, Hidden), Conversations, Talk to V. A hidden quest appears only once found and shows only its current step (D-171) | `UMemoryJournalSubsystem` `Public/Memory/MemoryJournalSubsystem.h:64-87`; `UHiddenQuestSubsystem::GetVisibleQuests` `Public/Memory/HiddenQuestSubsystem.h:100`; `EQuestKind` `HiddenQuestData.h:27` | Placeholder `UMemoryJournalWidget` `Public/UI/Hub/MemoryJournalWidget.h:40` (`wor.Journal.Open`, hub `AHubJournalStation`) | P2 |
| 7 | **Party** | Order the party (up to 3), who plays each member (you, a friend, AI and its difficulty), start cell, field skill, guard style | `UPartySessionSubsystem` `Public/Party/PartySessionSubsystem.h:146-252` | Real, C++ (B-458): `UPartyScreenWidget` `Public/UI/Party/PartyScreenWidget.h` (game menu > Party, `wor.UI.Party`); waits for Crown's PIE pass (the placeholder `UPartyControlWidget`, `wor.UI.PartyControl`, stays until then); hub strip `UPartyDisplayWidget` (Real) | P2 |
| 8 | **Settings** | Gameplay (**control model: Shortcuts / Omni D-pad**, Turn View, hide End Turn, hide corner hints, skip boss cinematics), camera, display, audio, controls (key list), accessibility | Saved in `FSavedSettings` `Public/Save/WoRSaveTypes.h:821`; `USaveSubsystem::SetTurnViewStyle` / `SetEndTurnButtonHidden` `Public/Save/SaveSubsystem.h:127-134`; turn-view names `CombatCameraTypes.cpp:264-277`; control model is B-436l (a console variable until this screen) | Real, C++ (B-452): `USettingsScreenWidget` `Public/UI/Menus/SettingsScreenWidget.h` (Turn View and End Turn button; the rest B-453, B-436l); waits for Crown's PIE pass | P1 |

### Front end

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 9 | **Title / main menu** | Press any button; Continue, New Game, Join a friend, Settings, Credits, Quit | `USaveSubsystem` (one slot per local player, `Public/Save/WoRSaveGame.h:2`, so no slot screen) | Built (B-467, in dev: `UTitleScreenWidget`, `PIE/B-467.md`; the backdrop is B-467b, Join a friend B-459) | P2 |
| 10 | **Save notices** | Autosave icon; the one-time "your old save was reset" warning (D-060) | `UWoRSaveGame::WasWiped` `Public/Save/WoRSaveGame.h:35` (never shown today) | None | P2 |
| 11 | **New game: first character** | Pick your character (the lords / templates), class reads from it | `DA_Character_*`; `ECharacterClass` | None | P3 (lore: who you start as is Crown's) |
| 12 | **Loading screen** | Only for hub ↔ trial (FeelGuide 10): an item or character description and a slow crystal turn | none (no loading-screen code) | None | P3 |
| 13 | **Credits** | Scroll | — | Built (B-470, in dev: `UCreditsScreenWidget`, `PIE/B-470.md`) | P3 |

### Hub

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 14 | **Hub HUD and interact prompt** | Gold / currencies on change, party strip, "E Talk / Rest / Enter" prompt | `UCurrencyComponent::OnCurrencyChanged` `Public/Currency/CurrencyComponent.h:118`; `UPartyDisplayWidget` | Party strip Real; prompt and money chip Real, C++ (B-443): `UHubHUDWidget` `Public/UI/Hub/HubHUDWidget.h`; waits for Crown's PIE pass (the old debug hints stay until then) | P2 |
| 15 | **Shop (every vendor)** | Buy (for yourself or another member, D-111), sell, cart; the vendor's tabs add its services | `UMerchantShopSubsystem::OpenForMerchant` `Public/Shop/MerchantShopSubsystem.h:36`; `UEconomyService::Purchase` / `PurchaseFor` `Public/Currency/EconomyService.h:262-273`; vendors `EMerchantType` `Public/Merchant/MerchantData.h:26-35` | Real, C++ (B-456a, B-456b): `UVendorScreenWidget` `Public/UI/Menus/VendorScreenWidget.h`, which talking to a vendor opens; waits for Crown's PIE pass (the old `UShopWindowWidget` stays as the fallback until then) | P2 |
| 16 | **Blacksmith services** | Weapons and augment stones (Stone Smith merged in, D-173): dismantle, merge | `DismantleWeapon` `EconomyService.h:151`; `MergeCrystals` :141 | Console (`wor.Economy.Merge`) | P2 |
| 17 | **Jeweler services** | Adapters; **buy item slots** (+1 pouch slot per character) | `ItemSlotUpgrade::TryBuy` `Public/Inventory/ItemSlotUpgrade.h:40` | Placeholder `UItemSlotUpgradeWidget` `Public/UI/Hub/ItemSlotUpgradeWidget.h:21` (`wor.UI.ItemSlots`) | P2 |
| 18 | **Spiritualist services** | Evolutions, crystals, **repair** (quote then pay), remove an evolution, **clear a curse** | `GearRepair::RepairParty` `Public/Equipment/Durability/GearRepair.h:45`; `RemovePrimaryEvolution` `EconomyService.h:206`; `UCurseSubsystem::PayToClearCurse` `Public/Memory/CurseSubsystem.h:42` | Curse: Placeholder `UCurseClearWidget` `Public/UI/Hub/CurseClearWidget.h:18` (`wor.UI.ClearCurse`); repair: Console | P2 |
| 19 | **Spirit of {character}** (was "Skins"; forms, D-184) | Form meters, reveal at 100%, wear a form; hub and rest spots only | `UCharacterFormSubsystem::TryWearForm` `Public/Character/Forms/CharacterFormSubsystem.h:58` | Placeholder `UCharacterFormWidget` `Public/UI/Hub/CharacterFormWidget.h:20` (`wor.UI.Forms`) | P3 |
| 20 | **NPC talk (V, characters)** | Speaker, line, choices; V's lines and character conversations | `UStoryLinesData` `Public/Story/StoryLinesData.h` | Journal only | P3 (the words are Crown's) |

### Run (trial)

| # | Screen | What the player does | Code it talks to | Today | Pri |
|---|---|---|---|---|---|
| 21 | **The Gate** (was "Trial gate"; D-191, D-192) | Pick the gate, then the **trial** (the code's "variation") and the difficulty (Easy / Medium / Hard: enemy damage and payout); see who's going; Enter | `UTrialRunSubsystem::SetGateChoice` `Public/Trial/TrialRunSubsystem.h:77`; `ETrialDifficulty` `Public/Trial/TrialGate.h:19` | Placeholder `UTrialGateWidget` `Public/UI/Trial/TrialGateWidget.h:20` (from `ATrialDoor`) | P1 |
| 22 | **Rest spot** | Rested (heal, revive); Level up (pending levels as "+1"); Inventory / Loadout; **Revive boss** (co-op: a vote); **Invite**; Hand over host; **Extract** (only at a boss's rest spot, D-186; half payout); a currency strip top right (D-182) | `WoRRestSpot::Rest` / `LeaveRun` `Public/Trial/TrialRestSpot.h:25-41`; `AWoRGameState::StartReviveVote` / `CastReviveVote` `Public/Player/WoRGameState.h:55-66`; `GetRunEndPayout` `TrialRunSubsystem.h:280` | Placeholder `URestSpotWidget` `Public/UI/Hub/RestSpotWidget.h:18` (`wor.Trial.Rest`, `wor.Trial.Vote`) | P1 |
| 23 | **Level up (place levels)** | Place each banked pillar level on Mind / Body / Spirit, per member. **No buying a point here** (D-183: points are bought only from NPCs found in the trial) | `WoRLevelDrops::TryPlaceLevel` / `TryBuyPoint` `Public/Trial/LevelDropPlacement.h:40-48` | Placeholder `ULevelDropWidget` `Public/UI/Hub/LevelDropWidget.h:20` (`wor.UI.LevelDrop`) | P1 |
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
| 33 | **Host hand-over / host lost** | Planned hand-over from the rest menu; "Host left, reconnecting" when it isn't planned | `HostSuccession::BeginHandOver` / `GetStep` `Public/Online/HostSuccession.h:88-102` | Real, C++ (B-459a): `UHostLostCardWidget` `Public/UI/Coop/HostLostCardWidget.h` (shown by `UHostLostCardPresenter`), `UHandOverConfirmWidget` `Public/UI/Coop/HandOverConfirmWidget.h` (rest menu, `wor.UI.HandOver`); waits for Crown's PIE pass | P2 |
| 34 | **Revive vote** | Everyone votes yes / no on reviving the mini-boss; tally shown | `OnReviveVoteChanged` `WoRGameState.h:66` | Inside the rest placeholder | P2 (drawn with 22) |

## Mock order

Highest priority first. Ticked = mocked on this branch (folder in brackets).

1. [x] Pause / game menu (`menus/pause`)
2. [x] Loadout and skills (`menus/loadout`)
3. [x] Inventory and pouch (`menus/inventory`)
4. [x] Rest spot, level up, extract, revive vote (`run/rest_spot`)
5. [x] After a fight, wipe, run over, run end (`run/run_end`)
6. [x] Settings (`menus/settings`)
7. [x] Trial gate (`run/gate`)
8. [x] Shop and the vendor services (`hub/vendors`)
9. [x] Journal (`menus/journal`)
10. [x] Party and co-op (`menus/party`, `coop/session`)
11. [x] Title, save notices, loading (`front/title`)
12. [x] Character sheet (`menus/character`)
13. [x] Hub HUD, prompt, NPC talk, recruit card (`hub/hud`; loading is with mock 11)

14. [x] Skins / forms (`hub/skins`)

15. [x] The rest (2026-11-13, second pass): New game (`front/new_game`), Credits (`front/credits`), Blacksmith Dismantle and Spiritualist Merge tabs (`hub/vendors` 5, 6), planned host hand-over (`coop/session` 4), journal Lore and Conversations (`menus/journal` 4, 5).

**Not mocked:** the World-stat draft (#30). The code marks it OBSOLETE: level drops replaced it (`CombatOrchestrator.h:296-300`, B-223, D-102), and mock 4's Level up is that screen.

## Mockup notes

One block per mocked screen: the images, why the layout, the references.

### 1. Pause / game menu (`menus/pause/`)

- `1_pause_trial.png`: in a trial. `2_pause_hub.png`: in the hub.
- **Layout:** page list on the left, the party on the right of it (HP, EP, Mind / Body / Spirit, Combat Tier), one side panel that previews the focused page (the tracked quest; the loadout). Opening the menu is one press and every page is one more.
  *Ref:* Expedition 33's main menu keeps the party in view beside a short page list; Persona 5's pause shows the party's HP / SP on the same screen as the pages, so you check the party without opening anything.
- **Top line is the place:** in a trial, the run's state (Prisms, Gear Essence, **wipes left "3 of 5"**: FeelGuide 7's five-wipe rule); in the hub, Gold and essences. Hades shows the run's money and Death Defiances the same way, on the pause screen.
- **No "Leave the trial" here:** you leave only by extracting at a rest spot (RunStructure §6, D-172); the side panel says so.
- Pages (code names): Resume, Loadout, Inventory, Character, Party, Journal, Settings, Quit to title.

### 2–3. Loadout and skills (`menus/loadout/`)

- `1_gear_generic.png` (Garrick: two weapons, one active, an adapter, two apparel), `2_socket_picker.png` (a crystal for an empty socket), `3_skills_refractor.png` (Sera's innate spells), `4_gear_resonator.png` (Bram: one weapon, any number of adapters).
- **Layout:** the character on LT / RT, the page (Gear, Skills) on LB / RB, the saved loadout name top right (Y opens the five saved ones, D-111). Slots on the left, one detail panel on the right that always shows the focused thing's **cost sum** ("Weapon 1 + Crystal 1 = 2").
  *Ref:* Expedition 33's Pictos / Lumina screen: a points budget always on screen, each piece showing its cost, so trying a build is fast. Diablo IV and Elden Ring show the compare panel next to the list.
- **Budget** as pips (Generic and Resonator 5, Refractor 3); picking something shows the new total in amber before you confirm. Costs are the code's: weapon 1, adapter 0, crystal or stone +1, evolution on gear +2, evolution alone 3 (`LoadoutConstants.h:38-44`).
- **Each gear slot shows its two sockets** (crystal or evolution; stone, D-141). **Apparel** has its own two slots under a heading, marked "outside budget", with its one socket (D-164).
- **Weapons show as two equal slots, one ACTIVE** (D-110: no primary / secondary difference). The brief's "main hand / off hand" isn't a slot in the code: a shield or second blade is the weapon's wield mode (`EWeaponWieldMode.h:32`), shown in its kind line when it has one.
- **Refusals show in place:** the S-tier crystal row is greyed with its reason ("S-tier: one per loadout", `MAX_EQUIPPED_S_TIER_CRYSTALS`), not hidden (UI_Theme §5: disabled stays focusable to show why).
- **Skills page** (Refractor): innate spells by school (only schools you hold a spell in), 6 per school, a points bar (24, tier = points F 1 to S 7, less the pillar discount; `SpellPoolConstants.h`). Generic and Resonator use the same page for each weapon's abilities and each adapter's spells.

### 4. Inventory and pouch (`menus/inventory/`)

- `1_gear.png` (Gear tab, verb menu on a weaker duplicate), `2_pouch_give.png` (Pouch tab at a rest spot, Give to another member), `3_dismantle_confirm.png`.
- **Changed by Crown (D-177, 2026-11-13):** the verb menu is **Give, Store, Take** only. Dismantle and Convert-to-gold happen at the vendor who does them, and vendors buy items (Sell). `3_dismantle_confirm.png` is the vendor's confirm now (mock 8), and the verb list in `1_gear.png` is redrawn without the two.
- **One screen, three tabs** (Gear, Pouch, Chest) and **one verb menu** on A: Give, Store, Take (D-177; was five verbs, D-131). A verb that doesn't apply is **greyed with the code's own reason under it** ("Home chest: in the hub only"), never hidden (`WoRInventoryVerbs::GetRefusal`).
  *Ref:* Baldur's Gate 3's camp chest and per-character inventories (D-117 is modelled on it); Elden Ring's item menu opens one short verb list on the item.
- **Icon grid, not a list:** gear and items are visual and many; a grid shows 12 at a glance (Diablo IV, BG3). E marks gear a loadout wears. Filter row for gear kinds.
- **Pouch:** 10 slots to start, the Jeweler's extra ones drawn as locked "Jeweler" slots up to 20, stacks of 10 (`InventoryConstants.h:51-57`). Give asks who receives and shows their room; X toggles the whole stack (the placeholder's "Whole stack" toggle).
- **Dismantle** is one confirm with what you get and lose, and "can't be undone" (D-016's warning, `GetDismantleWarning`). The placeholder's "press again to confirm" becomes this dialog.

### 22–24, 34. Rest spot (`run/rest_spot/`)

- `1_rest_menu.png`, `2_level_up.png`, `3_revive_vote.png` (co-op), `4_extract_confirm.png`.
- **Changed by Crown (2026-11-13, D-182, D-183, D-185):** the rest menu's labels are short: **"Revive boss", "Invite", "Extract"** ("Leave Trial" was renamed, D-186). **Only a rest spot at a boss is an extract point**; a rest spot out in the wild has no Extract row. Extracting pays the half share, as now. The **"Earned so far" panel is gone**; a **currency strip sits top right** with Gear Essence, Skill Essence, Elemental Essence, Gold and Prisms (GE, SE, EE, G, P; Prisms were missing from the mock). **Pending levels read "+1" / "+2"** on the Level up row and per character (not "1 to place"). **Level up is simple:** three pillar rows, **no list of affected skills** (the Skills page already shows whether requirements are met), **no "X buy a point"** and no "or buy one with Prisms" (points are bought only from NPCs found in the trial). On the party panel **HP and EP values sit centred inside their own bars** on a dark backing, as on the battle HUD. The four images are redrawn to match; the notes below describe the first draft.
- **Resting needs no button:** arriving restores the party, then the menu opens with "The party is restored." and "Earned so far" always in view. The list is the placeholder's, in the same words: Level up, Loadout, Inventory, Skins, Revive the mini-boss (1 of 3), Invite and seat friends, Hand over host (co-op only), Leave the trial.
  *Ref:* Elden Ring's Site of Grace and Expedition 33's expedition flags: sit, you're healed, one short list. Dark Souls II's Bonfire Ascetic for the revive (RunStructure §4).
- **Level up:** one member at a time on LT / RT (the tab shows how many each has to place). Three big pillar rows; the side panel says **what the level changes for this character's own skills**. A pillar level gives no flat stats in this game (Rules CL-37); it meets skill requirements (under one, the skill is x0.6) and raises the requirement boost (CL-39), so that is what's shown. Buy a point is X, priced in Prisms.
  *Ref:* Elden Ring's level-up screen previews every change before you confirm; Expedition 33's attribute screen does the same per attribute.
- **Revive vote** (co-op): one panel for everyone, names and answers, "2 of 3 said yes"; everyone outside a fight must agree (the code's rule). A / X answer. *Ref:* Monster Hunter's and Deep Rock Galactic's ready checks: every name with its answer, no chat needed.
- **Extract confirm:** RunStructure §6's sentence, then two columns, earned so far and what you get (half, `LEAVE_EARLY_SHARE`), then what's kept and lost. *Ref:* Hades and Nightreign show the run's tally before you commit to ending it.

### 25–28. After a fight, a wipe, the end of a run (`run/run_end/`)

- `1_reward_popups.png`, `2_wipe.png`, `3_trial_cleared.png`, `4_run_over.png`.
- **No results screen after a fight** (FeelGuide 6): popups stack on the right, stay a few seconds and fade; none needs a press. One popup per thing, colour = kind (Prisms grey, a drop in its element colour, a banked level cyan, a memory gold, a hidden quest violet and marked "only you see this", D-171). The words are the code's notices ("New memory: …", "Hidden quest discovered: …"). The small **trial HUD** (#28) sits bottom left: Prisms, wipes left, the tracked quest step.
  *Ref:* Expedition 33 returns you to the world with fading pickups; Elden Ring's item popups; Hades' boon and resource toasts.
- **Changed by Crown (2026-11-13, D-187, D-188, D-189):** **no enemy rank is ever shown to the player** ("mini-boss", "Elite", "Boss": it sets expectations and spoils the encounter): the wipe card **names the opponent that dealt the last blow**. "The party has fallen" reads **"Your possibilities end here"**. The end-of-run screen has **no Kept / Lost columns**; the payout **lists what the run gave** (upgrades, memories, quests and so on) as **one centred line at the bottom**; its button reads **"Return"**. **What each ending pays (D-189):** the boss beaten: full; extracting: half plus the run's upgrades; out of wipes: 25% of the run's currencies and no upgrades; quitting: nothing. The four images are redrawn to match; the notes below describe the first draft.
- **A wipe** is one plain card: what each member lost (pillar points: up to 1 to a normal enemy, 2 to a mini-boss, 0 to the main boss), where you wake, "the enemy is still there", wipes as five pips. One button, "Get up". *Ref:* Hades' short death beat, Elden Ring's respawn at the last grace: fast back to play (pillar 3, second chances).
- **The end of a run is the one full-screen tally:** the payout with its share ("Full payout x1.0"; extracting shows x0.5), the run in four numbers, and **Kept / Lost** side by side, so the rule is taught every time. "Trial cleared" (gold) and "The trial ends" (rose) are the same screen; falling pays in full (RunStructure §6).
  *Ref:* Hades' and Nightreign's end-of-run summaries; Slay the Spire's run tally.
- **Needs code:** nothing announces a run end, a wipe or a level-up today (no delegate), and `UCombatResultsWidget` only prints VICTORY / DEFEAT. A builder adds those signals first.

### 8. Settings (`menus/settings/`)

- `1_gameplay_control_model.png`, `2_controls.png`, `3_accessibility.png`.
- **Pages on LB / RB:** Gameplay, Camera, Display, Audio, Controls, Accessibility. Rows on the left, and a side panel that **explains the focused setting** in plain words.
  *Ref:* The Last of Us Part II and God of War Ragnarök explain every option beside it and are the most praised settings menus for it; Expedition 33 and Persona 5 use the same bumper-paged layout.
- **Battle menu controls: Shortcuts / Omni D-pad** (BattleHUDRefinePlan decision 10) is the **first row of Gameplay and is repeated on Accessibility**, with a small card per model listing what each button does, the picked one lit. Default: Shortcuts.
- **The code's settings, by their code names:** Turn View (Centred Wide default, Over the Shoulder, Tactical: B-398), End Turn button on / off (B-170e), button hints in the corner (HUD plan decision 7), boss cinematics.
- **Controls** is a read-only key list for now (fixed keys, D-063), by section (Field, Battle, Menus) and by device. Rebinding slots into the same rows later.
- **Approved as drawn, with all eight proposed settings (Crown, 2026-11-13, D-190).** Difficulty is chosen only at the gate, not here.
- **Proposed, not in code yet** (drawn so the page isn't empty; all approved, D-190): text size, camera movement in fights (the motion-sickness switch of D-078), screen shake, hold to confirm dangerous actions, element letters on colours (colour-blind aid), subtitles, tutorial callouts, "AI finishes my turn when I idle" (`SetLetAIFinish`).

### 21. Trial gate (`run/gate/`)

- `1_gate_trial.png` (solo, choosing), `2_gate_ready_coop.png` (co-op, on Enter).
- **Changed by Crown (2026-11-13, D-191, D-192):** the screen is **the Gate**. The player-facing word is **"Trial", not "Variation"**. The trials are a **vertical list** (there will be more than three), not three big cards; each row shows **only the trial's name and its line, never its boss** (it spoils the surprise). A **locked trial shows as "???" with its unlock condition** (for example the quest that unlocks it) and gets its real name once unlocked. An **AI member's "who plays them" shows nothing** (no "AI · Medium"). **Gates are not tied to a class:** every gate (the Stone Gate, the Silver Gate, the crystal-named gates) is open to everyone; **there is one gate in the hub**, and interacting opens this screen, where you pick the gate, then the trial, then the difficulty; the hub gate's glow takes the colour of your last trial's gate (default, Crown can veto). The two images are redrawn to match; the notes below describe the first draft.
- **Three steps, top to bottom, one screen:** Variation (big cards: the boss and a line each, because this is the story choice; one not yet reached shows as "???"), Difficulty (Easy / Medium / Hard with exactly what changes: enemy damage x0.8 / x1.0 / x1.2, payout x0.75 / x1.0 / x1.5, `TrialGate.cpp:26-36`), then Enter the trial. On the right, **who is going in**: each member, class, Combat Tier, and who plays them (You, a friend's name, AI and its level).
  *Ref:* Hades' Pact of Punishment (what each step changes, and the reward, on one screen); Nightreign's expedition select (target boss + party on one screen); Expedition 33 states that difficulty changes enemy damage, not HP, which this game copies (Q-293c).
- **Said once, here:** a run is 30–45 minutes, can't be saved, and is left by extracting (pillar 5, D-172); wipes allowed 5 (3 at the boss).
- X opens Party and Y Loadout without leaving the gate, so a last change doesn't mean walking back.

### 15–18. Vendors (`hub/vendors/`)

- `1_shop_buy.png` (Blacksmith, Buy), `2_jeweler_item_slots.png`, `3_spiritualist_repair.png`, `4_spiritualist_curses.png`.
- **Approved by Crown (2026-11-13, D-193), with the tabs set:** one shop screen, services as tabs, and vendors buy (the Sell tab stays). **Blacksmith:** Buy, Sell, Dismantle (gear and crystals). **Jeweler:** Buy, Sell, Item slots. **Spiritualist:** Buy, Sell, Repair, Curses, Dismantle (evolutions), Merge. **Dismantling is hub-only** (never during a run), and a dismantled item is gone until you find it again (no ban, no restore). Merge (the crystal tier-up) is the Spiritualist's, not the Blacksmith's.
- **One shop screen for every vendor; each vendor's services are extra tabs on it** (LB / RB): Blacksmith = Buy, Sell, Dismantle, Merge; Jeweler = Buy, Sell, Item slots; Spiritualist = Buy, Sell, Repair, Curses. So the placeholder service widgets (`UItemSlotUpgradeWidget`, `UCurseClearWidget`) and the console-only repair and merge fold into the vendor who offers them. Dismantle and Merge reuse the inventory's confirm (mock 3).
  *Ref:* Hades' House Contractor and Elden Ring's Roundtable smith: the service is a tab on the person who does it. Monster Hunter's smithy keeps forge and upgrade on one NPC.
- **Buy:** stock on the left, the focused item with a compare line ("+14 vs Garrick's sword"), **Buying for ‹ Garrick ›** (D-111 buy-for) and the cart with totals on the right. A row that can't be bought stays visible with its reason ("Garrick already owns one": one copy per character, D-115). Prices are in **Gold** in the hub and **Prisms** in a run (the same screen; D-047: every service also works in runs).
- **Item slots:** one row per member with 20 pips, the price rising per slot (200 + 100 each, `ItemSlotUpgrade.h:20-23`).
- **Repair:** the quote is the screen: every worn piece with its wear bar and price, "Repair all" with the total (`GearRepair`'s quote-then-pay). Broken shows in red with what it costs you in a fight.
- **Curses:** each curse held, who holds it, what it does (the curse's own `Title` and `Description`; placeholders drawn, the words are lore), the price, and the code's line "It is cleared. It may come back."

### 6. Journal (`menus/journal/`)

- `1_quests.png`, `2a_secret_quest_hidden.png`, `2b_secret_quest_shown.png`, `3_memories.png`. Lore and Conversations use the Memories layout (a list, a reader).
- **Pages:** Quests, Memories, Lore, Conversations (the placeholder's four, HubAndStory §4). "Talk to V" is not a journal page here: V is someone you talk to in the hub (#20).
- **Quests:** one list with the kind on each row in the code's words and colours (Main gold, Side grey, Hidden violet), a filter row, and the steps on the right: done ones ticked, the current one lit. A lets you **track** one quest; its step shows on the trial HUD and the pause menu.
  *Ref:* The Witcher 3's and Expedition 33's journals (list left, steps right, one tracked quest).
- **Changed by Crown (2026-11-13, D-196, D-198, D-199):** the player-facing name is **"Secret quest"**. A secret quest has **no "only you see this" line**. Showing and hiding it is **one toggle on X** whose hint flips between **"Show"** and **"Hide"**; A stays Select; **Y is "Hide all"** (every shown secret quest at once), and the quest filter row is on **LT / RT**, the shared sub-tab buttons. **Every quest (Main and Side too) shows only the steps you have reached**: later steps are not shown at all, not even as "…". The journal images are redrawn to match; the notes below describe the first draft.
- **Hidden quests follow D-171 and Q-397b exactly:** never listed until found; then listed as "Hidden Quest 2", **masked until you press A to reveal** (so a shared screen or stream doesn't spoil it), X masks it again; revealed, it shows **only the current step**: no later steps, no reward, "only you see this". *Ref:* Shangri-La Frontier's Unique Scenarios (Crown's model); Elden Ring's "no quest log" secrecy for the unfound part.
- **Memories:** by character (LT / RT), newest first with a NEW mark, a replay window and the text; unfound memories are not listed and not counted ("3 found", never "3 of 12": pillar 10).
- **Rewards stay hidden until earned** on every quest kind ("Shown when the quest is done").

### 7, 31–33. Party and co-op (`menus/party/`, `coop/session/`)

- `menus/party/1_party.png`; `coop/session/1_invite.png`, `2_join_request.png`, `3_host_lost.png`. The revive vote is with the rest spot (mock 4).
- **Approved by Crown (2026-11-13, D-200):** the party and co-op screens. Two changes: an **AI member shows no level text anywhere** (as at the Gate), and the row "Invite and seat friends" reads **"Invite"**.
- **Party:** three member rows with **Played by** (You (leader), AI seat and its level, or a friend's name), the start row and field skill; under them "When your fighters fall" (Let AI finish / Take over) and "Invite and seat friends". The side panel is the **3 × 3 starting grid** seen from above (Back / Middle / Front Row, enemies to the right); X moves the focused member on it. Words are `PartyControlWidget.cpp`'s. Field skill names in the image are made up.
  *Ref:* Final Fantasy XIV's and Baldur's Gate 3's party panels (who controls whom at a glance); Darkest Dungeon's formation row; Expedition 33 has no grid, so the grid picture follows Fire Emblem / Darkest Dungeon's "see the line-up before the fight".
- **Invite:** friends on the left with their state, the three seats on the right; a friend brings their own character and takes an AI seat (D-150); in a run they're seated at a rest spot (the code's line). *Ref:* Nightreign's and Monster Hunter's lobby cards.
- **Join request** is a **small card in the corner of the host's screen, not a full screen**, so it never interrupts play: hold View to answer, then pick the seat or refuse. *Ref:* Elden Ring's summon notices, Deep Rock Galactic's join prompt.
- **Host left:** one calm notice for everyone with the hand-over's steps as pips and "Your run, gear and progress are kept" (`HostSuccession::GetStep`). A planned hand-over from the rest menu shows the same card titled "Handing over host". *Ref:* Monster Hunter's host migration notice.
- **Needs code:** nothing announces a host loss to the UI today.

### 9, 10, 12. Title, save notices, loading (`front/title/`)

- `1_title.png`, `2_save_reset_notice.png`, `3_new_game_confirm.png`, `4_loading.png`.
- **Title = main menu, no "press any button" step:** the list is there at once, **Continue first** with one line saying where you are (place, party, hours). New Game, Join a friend, Settings, Credits, Quit. One save per player in the code, so **no slot screen**.
  *Ref:* Hades and Expedition 33 open straight onto Continue; Hollow Knight's and Elden Ring's Continue shows the save's place and time.
- **"Your save was reset"** (D-060): shown once on the title when an old save was wiped, with both version numbers and one button. Today the wipe is silent (`WasWiped()` is never read).
- **New Game with a save present** says what is replaced and needs a 2-second hold. *Ref:* Hades' and Elden Ring's delete confirms; hold-to-confirm is the standard for destructive actions (Destiny 2's dismantle).
- **Saving:** no save prompt in play. The game autosaves; a small crystal mark with "Saving…" shows bottom left while it writes (drawn on the loading image). *Ref:* the standard autosave icon (Elden Ring, Hades). There are no saved runs (D-172), so there is no "save and quit" in a trial.
- **Loading** (hub ↔ trial only, FeelGuide 10): one item or character with its description to read. *Ref:* Elden Ring's and Dark Souls' item-lore loading screens.

### 5. Character sheet (`menus/character/`)

- `1_character_sheet.png`. Three columns: who they are (HP, EP, **Max Actions** with action-point progress and the regain per turn, curses held), **pillars with their substats grouped under them** (the grouping of Rules CL-39), and **resistance by damage type** (D-168): All, Physical, Spell, then each element, as a bar from the middle (green right = resists, rose left = weak).
  *Ref:* Elden Ring's status screen (attributes left, defences by damage type right); Expedition 33's attribute page; Diablo IV's "where it comes from" breakdown, here on R3.
- Body's "Defence" and the Resistance column's "All" are the same number (D-168 makes Defence the All line). When that rename lands in code, the Body row can read "Resistance (All)".
- Only your own characters' weaknesses are shown; enemy weaknesses are never listed anywhere (FeelGuide 5).

### 14, 20, 29. Hub HUD, NPC talk, recruit card (`hub/hud/`)

- `1_hub_hud_prompt.png`, `2_npc_talk.png`, `3_recruit_card.png`.
- **Hub HUD is nearly empty** (pillar 1, immersion): the party strip bottom left, a floating **interact prompt** by what you face (key + verb + what: "X Talk · Blacksmith"; the same prompt does "Rest", "Enter", "Open"), and the money chip only near a vendor or when it changes.
  *Ref:* Expedition 33's and Elden Ring's clean exploration screens; Hades' House shows currencies only where they matter.
- **Talking:** the speaker's **role** ("Blacksmith", D-178) and line in one box at the bottom, choices stacked on the right, the world left visible; Y opens the log.
  **Changed by Crown (D-178, 2026-11-13):** the top-level choices are short actions: **Shop, Talk, Leave**. "Show me your weapons" is **Shop**; topics sit under **Talk** by a short name ("About the Ember Trials" is **Trials**), and a new line is marked NEW under Talk, not at the top level. `2_npc_talk.png` is redrawn to match.
  *Ref:* Hades' short lines with NEW-dialogue marks; Expedition 33's camp conversations; Persona 5's log on a button.
- **Recruit card** (a character met in a trial): who they are (class, pillars, Combat Tier), the rule in one line (joins for this run only, uses what your party carries: D-026, B-361), who plays them, and Invite for this run.
  **Changed by Crown (D-179, 2026-11-13):** "Played by" reads **NPC** or **Player**. With a full party the recruit asks **which NPC steps out** (or decline); a real player is never swapped out, and if every other slot is a player the offer is refused. `3_recruit_card.png` gets the full-party case.
  *Ref:* Fire Emblem's recruit cards; Baldur's Gate 3's "join the party" prompt.

### 19. Spirit of {character} (was Skins / forms) (`hub/skins/`)

- **Renamed by Crown (2026-11-13, D-184):** the screen and its menu rows read **"Spirit of {character}"** ("Spirit of Sera"), not "Skins".

- `1_forms.png`. A list of forms with each character's **meter** (the placeholder's "{name}: {n}%"), "your own form" always first, unmet characters as "???". The side panel shows the form, the code's line ("Locked. Wear their pieces in fights you win to fill the meter."), which of their pieces you own or wear (unknown ones as "?"), and what a form does (looks and bound gear, no stats). Y switches the base body (Quinn / Manny).
  *Ref:* Mortal Shell's shells (Crown's model, `CharacterForms.md`); Hades' keepsake and Monster Hunter's mastery meters (visible progress after every fight).

### Second pass: the rest

- **New game** (`front/new_game/`: `1_new_game.png`, `2_new_game_fire_refractor.png`, `3_new_game_generic.png`): Body (Manny / Quinn, with a preview), Class (Generic, Refractor, Resonator, each with its identity in one line and its budget, D-110), then Begin. **Refractor adds the element choice (Crown, 2026-11-13):** the nine elements sit under the class description in the side panel, in the game's order; the panel's title, colour and text follow the pick ("Darkness Refractor", "Fire Refractor"), with one line saying what that element's full status bar does (Rules SB-19 to SB-28), and the button reads "Begin as Darkness Refractor". A class with no innate element shows no element row. *Ref:* Elden Ring's and Hades II's short starts; Mortal Shell has no creator at all (Crown's model); Pokémon's starter pick shows the choice and what it means on one screen. The heading and how the opening leads here are Crown's.
  **PNGs to re-render:** `1_new_game.png` in git is the older image and `2_`, `3_` are not in git (this session can't push LFS). Run `front/new_game/make.py` and commit the three PNGs. The review page has the new ones.
- **Credits** (`front/credits/1_credits.png`): a centred scroll over the title backdrop; hold A to speed up, B to leave. *Ref:* the standard (Hades, Expedition 33).
- **Blacksmith · Dismantle** (`hub/vendors/5_blacksmith_dismantle.png`): a member's gear with what each piece gives; a piece in a loadout is greyed with the code's reason ("It's in a loadout. Take it off first").
  **Changed by Crown (2026-11-14):** the Blacksmith's tabs are **Buy, Sell, Dismantle** (gear and crystals); the Spiritualist's are **Buy, Sell, Repair, Curses, Dismantle (evolutions), Merge** (`7_spiritualist_dismantle.png`); the Jeweler's stay Buy, Sell, Item slots. Dismantle screens say it is **hub-only**.
- **Spiritualist · Merge** (`hub/vendors/6_merge_crystals.png`): pick the crystal, pick the tier to make; the panel shows what it uses, what it makes, the change that comes back and the price (`MergeCrystals`, ER-28). Merge sits with the Spiritualist because crystals are theirs in the vendor names; say if the Blacksmith should have it. *Ref:* Diablo's gem combining, Monster Hunter's melding: choose the result, see the cost.
- **Planned hand-over** (`coop/session/4_planned_handover.png`): a confirm naming the next host, then the host-left progress card.
- **Journal · Lore** (`menus/journal/4_lore.png`): by place; a page-like reader with no scene window. **Conversations** (`5_conversations.png`): Ready and Played; a ready talk says who to speak to in the hub (A shows where), a played one is kept as a script. *Ref:* Expedition 33's camp talks, Hades' codex, Mass Effect's codex reader.

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

Screen-specific rows:

| Action | Keyboard / mouse | Gamepad | Input action | Screen |
|---|---|---|---|---|
| Answer a notice (join request, vote) from the world | Hold N | Hold View (Touchpad) | `IA_UI_AnswerNotice` | 32, 34 |
| Open the journal from a popup ("New memory") | J (exists, proposed) | D-pad Up | `IA_Field_Journal` | 25 |

Hold View would share the button with Map (`IA_Field_Map`, a tap): tap = map, hold = answer. If that's unwanted, the notice can wait in the pause menu instead.

## Open questions for Crown

Collected as screens are drawn; lore and story questions are listed, not answered.

1. **Quit to title in the middle of a run** (pause, #1): there are no saved runs (D-172). Does quitting count as extracting (reduced payout), as a loss, or is it refused until you extract? *Suggested:* ask first ("Quitting now ends the run: you keep what extracting would give"), like Hades, whose pause menu "Give Up" ends the run as if you fell, after a confirm.
   **Answered (Crown, 2026-11-13, D-176):** quitting **loses everything** from the run. The only way out with loot is extracting at a designated point or NPC. Quit needs a strong confirm.
2. **Stone sockets** (loadout, #2): drawn because the code has them (D-141), but the battle HUD plan says you're considering removing augment stones. If they go, each gear row loses its second socket and nothing else changes.
   **Answered (Crown, 2026-11-13, D-180):** the other augment stones stay; only the Ability Stone was removed (D-175, B-437). The socket stays as drawn.
3. **Main hand / off hand** (loadout, #2): the code has two equal weapon slots and a wield mode per weapon, not hand slots. The mockup follows the code; say if you want hand slots shown.
   **Answered (Crown, 2026-11-13, D-180):** keep two equal weapon slots, the wield mode in the kind line. A shield weapon stands in the shield stance by default (B-446).
4. **Level-up preview** (rest spot, #23): a pillar level changes skill requirements and boosts, not flat stats, so the screen lists the character's affected skills. Is that the read you want, or should a level also show a plain number going up?
   **Answered (Crown, 2026-11-13, D-183):** keep the level screen simple: it does **not** list affected skills (the Skills page shows whether requirements are met). No buying a point at the rest spot.
5. **Words for the end screens** (#26, #27) (**partly answered, D-188:** "Your possibilities end here" and "Return"; the other words stay stand-ins): "The party has fallen", "Get up", "Trial cleared", "The trial ends", "Return to the Void Realm" are stand-ins. These are story-facing words: yours to choose.
6. ~~World-stat draft~~ Settled by the code: it is marked OBSOLETE, replaced by level drops (B-223, D-102). Not mocked.
7. **Settings not in code** (#8): which of the proposed rows do you want (text size, screen shake, hold to confirm, element letters on colours, subtitles)? And is **difficulty** only chosen at the gate, or also a setting?
   **Answered (Crown, 2026-11-13, D-190):** all eight proposed settings; difficulty only at the gate.
8. **Gate words and locks** (#21, lore): the variation names (Past, Present, Future are the doc's examples), each variation's one-line teaser and boss name, and **whether an unreached variation shows as "???" or is hidden**. Yours to decide.
   **Partly answered (Crown, 2026-11-13, D-191):** a locked trial shows as "???" with its unlock condition; no boss name on the gate; the word is "Trial". The trial names and lines stay Crown's.
9. **Selling** (#15): the mockup has a Sell tab, but the code only buys, dismantles and converts. Do vendors buy things back, or is Dismantle / Convert the only way to turn gear into currency?
   **Answered (Crown, 2026-11-13, D-177):** vendors **do buy items** (Sell). Dismantle and Convert-to-gold are not in the inventory; they happen only at the relevant vendor. The inventory's verbs are Give, Store, Take.
10. **Journal words** (#6, lore) (**partly answered, D-199:** quests show only the steps reached, nothing for later ones; the titles and texts stay Crown's): quest titles, step texts, memory texts and whether Main / Side quests show their later steps as "…" or nothing. All placeholder text in the mockups is made up.
11. **Title and loading words** (#9, #12, lore): the logo and key art, and which descriptions appear on loading screens (any item or character, or only ones you've met?).
12. **New game** (#11, lore): the choice itself is decided (Manny or Quinn, and class; `CharacterForms.md`). How it's presented isn't mocked: it is the opening scene (HubAndStory §4), yours.
15. **Forms list** (#19, lore): do characters you haven't met show as "???" (as drawn) or not at all? And the form names shown are placeholders.
    **Answered (Crown, 2026-11-13, D-181):** unmet characters show as "???", as drawn.
16. **Refractor elements at the start** (#11): are all nine open to a new Refractor, or are some (Void, Reality, Broken Darkness) earned later? All nine are drawn. The one-line element descriptions are plain-words versions of the status rules; reword as you like.
13. **Who speaks** (#20, lore): the service NPCs are V in different forms and who V is stays hidden (HubAndStory §2, §3). Does the talk box name them by role ("Blacksmith"), as V, or with no name? The mockup uses the role.
    **Answered (Crown, 2026-11-13, D-178):** the talk box shows the **role** ("Blacksmith").
14. **Recruiting with a full party** (#29): the party cap is 3. If you meet a recruit with three members, is the offer refused, or does someone step out? The mockup shows only the case with room.
    **Answered (Crown, 2026-11-13, D-179):** if the party is only you and NPCs, the recruit asks which NPC steps out (or decline). A real player is never swapped out; if every other slot is a player, the offer is refused.

## Approved screens (Crown, 2026-11-13)

Approved as drawn, one build job each; built one at a time on the CommonUI root (`UWoRRootLayout::PushMenu`), the pause menu first.

| Screen | # | Mock set | Job | Note |
|---|---|---|---|---|
| Pause / game menu | 1 | `menus/pause` | B-439 (built, in dev; `PIE/B-439.md`) | Quit in a run loses everything (D-176) |
| Loadout and skills | 2, 3 | `menus/loadout` | B-440 (piece B-440a built, in dev; `PIE/B-440a.md`) | Stone socket stays, two equal weapon slots (D-180) |
| Inventory and pouch | 4 | `menus/inventory` | B-441 (built, in dev; `PIE/B-441.md`), B-461 (filter row, Sort, evolution and apparel tiles, place name: built, in dev; `PIE/B-461.md`) | Verbs Give, Store, Take (D-177); Take is listed only at storage (D-195); no chest during a run (Q-441); each tab's filters are what it can hold, Sort is Kind / Tier / Name (Q-461) |
| Character sheet | 5 | `menus/character` | B-442 (built, in dev; `PIE/B-442.md`) | The fight's own numbers (Q-442) |
| Hub HUD and interact prompt | 14 | `hub/hud` 1 | B-443 (built, in dev; `PIE/B-443.md`) | |
| Party | 7 | `menus/party` | B-458 | No AI level text; the row is "Invite" (D-200) |
| Co-op session (invite, join request, host hand-over) | 31 to 33 | `coop/session` | B-459 (after B-458) | D-200 |
| Vendors (shop and services) | 15 to 18 | `hub/vendors` | B-456a (built, in dev: the screen, Sell, Dismantle; `PIE/B-456a.md`), B-456b (built, in dev: Buy, the cart, "Buying for"; talking to a vendor opens the screen; `PIE/B-456b.md`), B-456c (built, in dev: the Jeweler's Item slots; `PIE/B-456c.md`), B-456d (built, in dev: the Spiritualist's Repair, Curses, Merge; `PIE/B-456d.md`) | One shop screen, services as tabs; who does what: D-193 |
| Settings | 8 | `menus/settings` | B-452 (the screen: built, in dev; `PIE/B-452.md`), B-453 (the eight new settings) | Approved with all eight proposed settings (D-190) |
| Spirit of {character} (was Skins / forms) | 19 | `hub/skins` | B-444 | Unmet characters "???" (D-181) |

Answered, not approved yet (the mockup is redrawn first): NPC talk (#20, D-178), the recruit card (#29, D-179), the rest spot and level up (#22, #23; D-182, D-183).

**Every menu's party bars (D-185):** HP and EP values sit centred inside their own bars on a dark backing, as on the battle HUD's cards. The pause menu is built that way (B-439).

## Summary (2026-11-13)

**Layouts approved for now by Crown (2026-11-13); final art from the Hidris pass.** The open questions below stay listed for Crown.

**Review page (all images, private to Crown):** https://claude.ai/artifact/CX3ECucBuxgmJ3c2SnMvZC — one section per screen group, each image with its layout note and the game it's based on, and the 15 questions at the end (lore ones marked "Crown's call").

**Done:** `UIScreens.md` (34 screens with code, state and priority) and **49 images in 15 mock sets**, each backed by named games: pause menu; loadout and skills; inventory and pouch; rest spot (menu, level up, revive vote, extract); after a fight, wipe, run end; settings (control model: Shortcuts / Omni D-pad); trial gate; vendors (shop, dismantle, item slots, repair, curses, merge); journal (quests, hidden quests, memories, lore, conversations); party and co-op (invite, join request, hand-over, host left); title, new game, save notices, loading, credits; character sheet; hub HUD, NPC talk, recruit card; skins.

**Left:** nothing to mock. The world-stat draft is obsolete in code (question 6).

**PNGs are in git** (LFS), rendered from the `make.py` scripts and committed from a machine with LFS access (commit 29f36679c). Re-run a `make.py` and commit its PNGs the same way after any change.

**For builders, found on the way:** nothing announces a run end, a wipe, a level-up, a reset save or a lost host, so those screens need a signal first. `ECurrencyType_Prisms` printing "Gold" is the D-159 swap, not a bug.

**Open questions for Crown:** the 16 above (16 is a design one). **Answered (2026-11-13):** 1, 2, 3, 4, 9, 13, 14, 15 (D-176 to D-185). **Answered since:** 7 (D-190), part of 5 (D-188), part of 8 (D-191). Still open: lore 5 (the rest), 8, 10, 11, 12; design 16. Settled: 6.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-14 | The Spiritualist's tabs are built (B-456d): Repair (one piece on A, everything on X), Curses (clear a held curse), Merge (one list, the tier on Left / Right; Q-456d); every vendor tab works | feature/B-456d-spiritualist-services |
| 2026-11-14 | The title screen is built (B-467): the rows, Continue's line, New Game's hold, the "save was reset" notice (Q-467); the backdrop art is B-467b | feature/B-467-title-screen |
| 2026-11-14 | The credits are built (B-470): the title's Credits row opens them | feature/B-470-credits-screen |
| 2026-11-14 | The vendor screen's Item slots tab is built (B-456c): a row per party member, the pouch's slots as marks, the next slot's price; A asks, the second A buys | feature/B-456c-item-slots |
| 2026-11-14 | The vendor screen's Buy tab is built (B-456b): the stock, the cart, "Buying for"; talking to any vendor opens the screen (Q-456b) | feature/B-456b-vendor-buy |
| 2026-11-14 | The vendor screen's first piece is built (B-456a): the screen with each vendor's tabs, Sell and Dismantle; Buy and the other services follow (B-456b to B-456d) | feature/B-456-vendor-screen |
| 2026-11-14 | The hub HUD's prompt and money chip are built (B-443). Crown: bar values are plain white, no backing (D-197); secret quests: no "only you see this", one Show / Hide toggle on X (D-198); quests show only the steps reached (D-199) | feature/B-443-hub-hud |
| 2026-11-14 | The character sheet is built (B-442). Crown: Take is listed only at storage (D-195); HP and EP bars darker (D-194); "Hidden quest" reads "Secret quest" (D-196) | feature/B-442-character-sheet |
| 2026-11-14 | Vendors approved (D-193): one shop screen with service tabs; who dismantles what; dismantling hub-only; Merge at the Spiritualist | feature/B-442-character-sheet |
| 2026-11-14 | Mockups: HP and EP bars are a darker red and a darker blue (kit `bar()`); "Hidden quest" drawn as "Secret quest" (`2_hidden_quest.png` is now `2a_secret_quest_hidden.png`, `2b_secret_quest_shown.png`); inventory lists Take only on the Chest tab; vendor tabs; new game images re-rendered from its script | docs/ui-screens-redraw |
| 2026-11-14 | Mockups redrawn for Crown's answers (D-177 to D-179, D-182 to D-189, D-191, D-192): the Gate (`1_gate_variation.png` is now `1_gate_trial.png`), rest spot, run end, NPC talk, recruit card, inventory verbs, "Spirit of {character}" | docs/ui-screens-redraw |
| 2026-11-14 | The Gate (D-191, D-192): "Trial" not "Variation", a vertical list, "???" with the unlock condition, no boss names, no AI level text; one hub gate, every gate open to everyone | docs/ui-screens-answers-4 |
| 2026-11-14 | Crown's answers on the wipe and end-of-run screens (D-187 no enemy rank shown, D-188 words and layout, D-189 what each ending pays) and settings (D-190: approved with all eight proposed settings) | feature/B-440-loadout-screen |
| 2026-11-14 | Extracting (D-186): "Leave Trial" is "Extract", only at a boss's rest spot; the half share stays | docs/ui-screens-answers-3 |
| 2026-11-14 | Crown's rest spot and level-up answers (D-182, D-183), "Skins" is "Spirit of {character}" (D-184), values inside every menu party bar (D-185); question 4 answered | docs/ui-screens-answers-2 |
| 2026-11-13 | Crown's answers to questions 1, 2, 3, 9, 13, 14, 15 and the NPC talk choices (D-176 to D-181); six screens approved as drawn, build jobs B-439 to B-444 | docs/ui-screens-answers |
| 2026-11-13 | New game: element choices under the Refractor description, title and text follow the element (Crown) | docs/ui-screens-all |
| 2026-11-13 | Second pass (the screens left), 49 images in all, the review page link | docs/ui-screens-all |
| 2026-11-13 | 14 mock sets (42 images as `make.py` scripts), mockup notes, proposed input rows, 15 questions, summary | docs/ui-screens-all |
| 2026-11-13 | Created: 34 screens from `dev`'s code (the cloud features are already merged), mock order, shared menu controls, proposed input rows | docs/ui-screens-all |
