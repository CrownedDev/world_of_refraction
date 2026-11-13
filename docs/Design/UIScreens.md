# UI Screens — every screen outside the battle HUD

**Status:** design (2026-11-13), docs and mockups only, no widgets. Branch `docs/ui-screens-all`.
**Replaces** the screen list in `UI_ScreenInventory.md` (2026-10-25), which is out of date; that file keeps its asset shopping list.
**The battle HUD is done** (`BattleHUDRefinePlan.md`, B-436) and is not in this list.
**Look:** the battle HUD's theme, used loosely (Crown via the brain, 2026-11-13): dark stone panels, one cut corner, **cyan = focus**, tab rows, coloured face-button glyphs, hints at the bottom right. Each screen is laid out for its own job. Final art comes from the Hidris art pass (`UI_Theme.md`); these mockups are layout, information and controls only.
**Mockups:** `screen_mockups/<area>/<screen>/` (one `make.py` each, run `python make.py`; shared kit `screen_mockups/_kit/wor_ui.py`; DejaVu stands in for Spectral SC and Lato).
**PNGs not committed yet:** the cloud session that drew them couldn't reach GitHub's LFS host (`lfs.github.com` blocked by its network policy), and PNG is LFS-tracked. To get the images, run each `make.py` (Python 3 + Pillow; output lands next to it), then commit the PNGs from a machine with LFS access. A personal cloud environment `world_of_refraction` that allows the LFS hosts now exists for later sessions.

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

1. [x] Pause / game menu (`menus/pause`)
2. [x] Loadout and skills (`menus/loadout`)
3. [x] Inventory and pouch (`menus/inventory`)
4. [x] Rest spot, level up, extract, revive vote (`run/rest_spot`)
5. [x] After a fight, wipe, run over, run end (`run/run_end`)
6. [x] Settings (`menus/settings`)
7. [x] Trial gate (`run/gate`)
8. [ ] Shop and the vendor services (`hub/vendors`)
9. [ ] Journal (`menus/journal`)
10. [ ] Party and co-op (`menus/party`, `coop/`)
11. [ ] Title and save notices (`front/title`)
12. [ ] Character sheet (`menus/character`)
13. [ ] Hub HUD, prompt, loading, NPC talk (`hub/hud`)

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
- **One screen, three tabs** (Gear, Pouch, Chest) and **one verb menu** on A: Give, Store, Take, Dismantle, Convert (D-131). A verb that doesn't apply is **greyed with the code's own reason under it** ("Home chest: in the hub only"), never hidden (`WoRInventoryVerbs::GetRefusal`).
  *Ref:* Baldur's Gate 3's camp chest and per-character inventories (D-117 is modelled on it); Elden Ring's item menu opens one short verb list on the item.
- **Icon grid, not a list:** gear and items are visual and many; a grid shows 12 at a glance (Diablo IV, BG3). E marks gear a loadout wears. Filter row for gear kinds.
- **Pouch:** 10 slots to start, the Jeweler's extra ones drawn as locked "Jeweler" slots up to 20, stacks of 10 (`InventoryConstants.h:51-57`). Give asks who receives and shows their room; X toggles the whole stack (the placeholder's "Whole stack" toggle).
- **Dismantle** is one confirm with what you get and lose, and "can't be undone" (D-016's warning, `GetDismantleWarning`). The placeholder's "press again to confirm" becomes this dialog.

### 22–24, 34. Rest spot (`run/rest_spot/`)

- `1_rest_menu.png`, `2_level_up.png`, `3_revive_vote.png` (co-op), `4_extract_confirm.png`.
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
- **Proposed, not in code yet** (drawn so the page isn't empty; each needs Crown's yes): text size, camera movement in fights (the motion-sickness switch of D-078), screen shake, hold to confirm dangerous actions, element letters on colours (colour-blind aid), subtitles, tutorial callouts, "AI finishes my turn when I idle" (`SetLetAIFinish`).

### 21. Trial gate (`run/gate/`)

- `1_gate_variation.png` (solo, choosing), `2_gate_ready_coop.png` (co-op, on Enter).
- **Three steps, top to bottom, one screen:** Variation (big cards: the boss and a line each, because this is the story choice; one not yet reached shows as "???"), Difficulty (Easy / Medium / Hard with exactly what changes: enemy damage x0.8 / x1.0 / x1.2, payout x0.75 / x1.0 / x1.5, `TrialGate.cpp:26-36`), then Enter the trial. On the right, **who is going in**: each member, class, Combat Tier, and who plays them (You, a friend's name, AI and its level).
  *Ref:* Hades' Pact of Punishment (what each step changes, and the reward, on one screen); Nightreign's expedition select (target boss + party on one screen); Expedition 33 states that difficulty changes enemy damage, not HP, which this game copies (Q-293c).
- **Said once, here:** a run is 30–45 minutes, can't be saved, and is left by extracting (pillar 5, D-172); wipes allowed 5 (3 at the boss).
- X opens Party and Y Loadout without leaving the gate, so a last change doesn't mean walking back.

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

1. **Quit to title in the middle of a run** (pause, #1): there are no saved runs (D-172). Does quitting count as extracting (reduced payout), as a loss, or is it refused until you extract? *Suggested:* ask first ("Quitting now ends the run: you keep what extracting would give"), like Hades, whose pause menu "Give Up" ends the run as if you fell, after a confirm.
2. **Stone sockets** (loadout, #2): drawn because the code has them (D-141), but the battle HUD plan says you're considering removing augment stones. If they go, each gear row loses its second socket and nothing else changes.
3. **Main hand / off hand** (loadout, #2): the code has two equal weapon slots and a wield mode per weapon, not hand slots. The mockup follows the code; say if you want hand slots shown.
4. **Level-up preview** (rest spot, #23): a pillar level changes skill requirements and boosts, not flat stats, so the screen lists the character's affected skills. Is that the read you want, or should a level also show a plain number going up?
5. **Words for the end screens** (#26, #27): "The party has fallen", "Get up", "Trial cleared", "The trial ends", "Return to the Void Realm" are stand-ins. These are story-facing words: yours to choose.
6. **World-stat draft** (#30, pick 3 of 5): nothing listens to it today and levelling now happens at rest spots (B-363). Not mocked; say if it's still wanted.
7. **Settings not in code** (#8): which of the proposed rows do you want (text size, screen shake, hold to confirm, element letters on colours, subtitles)? And is **difficulty** only chosen at the gate, or also a setting?
8. **Gate words and locks** (#21, lore): the variation names (Past, Present, Future are the doc's examples), each variation's one-line teaser and boss name, and **whether an unreached variation shows as "???" or is hidden**. Yours to decide.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-13 | Created: 34 screens from `dev`'s code (the cloud features are already merged), mock order, shared menu controls, proposed input rows | docs/ui-screens-all |
