# Player-text audit (2026-11-14)

Report only. Nothing in code is changed. Crown approves before any job is made from this.

**Scope:** every `LOCTEXT` in `Source/world_of_refraction` on `dev` (about 1,900 strings, tests left out), plus `docs/Design/UIScreens.md` and the mockup `make.py` scripts (branch `docs/ui-screens-all`, unmerged). Lore is skipped: story, dialogue, quest and journal text, names, flavour lines.

Paths are under `Source/world_of_refraction/Private/` unless they say otherwise. Line numbers are on `dev` at `90e38b0`.

## Key points for Crown

1. **"Worn" means two things.** It means "equipped" (Inventory, Forms) and also "damaged" (Repair). "Nothing is worn." on the Repair tab reads as "you have nothing equipped". Fix: **Equipped** for gear in a loadout, **Damaged** for wear.
2. **Debug words reach players.** "(see the Output Log)", "(see the log)", `wor.Session.Find`, "No wallet to pay from". Players can't use any of these.
3. **One word per thing.** EP / Energy, Spell / Refraction, Adapter / Ring, Form / Skin, Secret / Hidden quest, Defence / Defense, Purchase / Buy, Back / Close / Cancel / Resume. The table at the end picks one word for each.
4. **Buttons name the place the screen already shows.** "Leave the trial", "Enter the trial", "Hand over host (the next player hosts; you rejoin)". The screen already shows where you are, so **Leave**, **Enter** and **Hand over host** are enough.
5. **Interact prompts are full sentences**, and they hard-code E even on a gamepad ("Press E to open the chest"). Fix: the key, the verb and the thing: **E Open · Chest** (Skyrim's prompt, and the hub mockup's own "X Talk · Blacksmith").
6. **Kept as they are:** rule text the player needs ("Full rewards only for beating the boss…", "It may come back.", "S-tier: one per loadout."), plus Crown's names (**Negated**, **\<Element\> Break**, **\<Element\> DoT**).

---

## 1. Biggest wins

| # | Where | Now | Shorter | Why |
|---|---|---|---|---|
| 1 | `UI/Menus/VendorScreenModel.cpp:613` | Nothing is worn. | Nothing needs repair. | "Worn" also means equipped (`InventoryScreenModel.cpp:75`). This reads as "nothing equipped". |
| 2 | `Equipment/Durability/GearRepair.cpp:238` | Nothing here is worn. | Nothing needs repair. | Same clash. Also the same message worded two ways. |
| 3 | `UI/Menus/InventoryScreenModel.cpp:75`, `:161` | Worn in a loadout. | Equipped. | Frees "worn" for durability. Equip is the RPG word (Persona 5, Final Fantasy). |
| 4 | `UI/Menus/InventoryScreenWidget.cpp:150` | E  worn in a loadout, or in use | E  Equipped | Same as above, and shorter. |
| 5 | `UI/Menus/LoadoutScreenModel.cpp:838`, `:891` | It couldn't be socketed (the Output Log says why). | Couldn't socket that. | Players have no Output Log. |
| 6 | `Trial/TrialMapFeature.cpp:317`, `:334` | The boss fight can't start (see the Output Log). | The boss fight can't start. | Same. |
| 7 | `Shop/VendorServices.cpp:248`, `Inventory/InventoryVerbs.cpp:308` | Couldn't {0} {1} (see the log) | Couldn't {0} {1}. | Same. |
| 8 | `UI/Coop/InviteScreenModel.cpp:76` | No friends list here (a local network game): a friend on your network finds your session with wor.Session.Find.\n{0} | No friends list on a local network. Friends nearby can find your session. | A console command is not player text. |
| 9 | `UI/Party/InviteWidget.cpp:160` | No friends list here (LAN): a friend on your network uses wor.Session.Find, then wor.Session.Join \<n\> | (same as #8) | Same. "LAN" is jargon too. |
| 10 | `UI/Hub/RestSpotWidget.cpp:125`, `:177` | Leave the trial | Leave | You are at a rest spot in a trial. Dark Souls' bonfire menu and Elden Ring's grace menu both end with "Leave". |
| 11 | `UI/Hub/RestSpotWidget.cpp:177` | Leave the session | Leave | The confirm box (`:189`) already says whose run you leave. |
| 12 | `UI/Hub/RestSpotWidget.cpp:128` | Confirm: leave | Leave | It's the confirm button. The box above it asks the question. |
| 13 | `UI/Hub/RestSpotWidget.cpp:121` | Hand over host (the next player hosts; you rejoin) | Hand over host | The confirm box (`UI/Coop/HandOverConfirmWidget.cpp:64`) explains it. |
| 14 | `UI/Trial/GateScreenWidget.cpp:202`, `UI/Trial/TrialGateWidget.cpp:103` | Enter the trial | Enter | You are on the Gate screen with a trial picked. |
| 15 | `Trial/TrialMapFeature.cpp:245` | Press E to open the chest | E  Open · Chest | Key + verb + thing (Skyrim; the hub mockup's "X Talk · Blacksmith"). It should use the gamepad key too (`UI/Hub/HubHUDModel.cpp:42` has both). |
| 16 | `Trial/TrialRestSpot.cpp:191` | Press E to rest | E  Rest | Same pattern. "Rest" is already in `EInteractVerb` (`WoRGameNames.cpp:762`). |
| 17 | `Trial/TrialMapFeature.cpp:281` | Press E to read: {0} | E  Read · {0} | Same. |
| 18 | `Trial/TrialMapFeature.cpp:346` | Press E to challenge {0} (boss fight) | E  Challenge · {0} | Same. "Challenge" already says it's a fight. |
| 19 | `Memory/MemoryFragment.cpp:82` | Press E to pick up the memory fragment | E  Pick up · Memory fragment | Same. |
| 20 | `Trial/PrismDropPickup.cpp:146`, `:149` | Press E to pick up your {0} {1} | E  Pick up · {0} {1} | Same. |
| 21 | `Hub/HubJournalStation.cpp:28` | Press E: memory journal, V, conversations | E  Open · Journal | The journal has its own pages. |
| 22 | `../Public/Loadout/SourceBreakRules.h:22` | Source broken: the weapon's crystal is broken. | Crystal broken. | Says "broken" twice. |
| 23 | `../Public/Loadout/SourceBreakRules.h:27` | Source broken: the evolution crystal is broken. | Evolution broken. | Same. |
| 24 | `../Public/Loadout/SourceBreakRules.h:32` | Source broken: this adapter casts F-tier spells only. | Adapter broken: F-tier spells only. | Keeps the rule, drops "Source" (jargon). |
| 25 | `UI/Menus/InventoryScreenModel.cpp:148`, `:178` | Marks "W", "A", "S", "C", "Ev", "Ap" | Icons, or "Wpn", "Adp", "Stn", "Cry" | "A" reads as the A button or A-tier, and "S" as S-tier. Both collide with the tier letters on the same tile. |
| 26 | `UI/Combat/Battle/BattleHUDView.cpp:412`, `:417` | AD / WD | an icon, or "Dur." | Players won't guess these letters mean durability. |

---

## 2. By screen

### Battle HUD and battle menu

| Where | Now | Shorter | Why |
|---|---|---|---|
| `UI/Combat/Battle/BattleHUDView.cpp:117` | Absorb | Barrier | The hit cue says **Negated** (Crown's call) and the effect is named Barrier (`WoRGameNames.cpp:285`). "Absorb" is a third word for the same thing. *Crown's check.* |
| `UI/Combat/Battle/BattleHUDView.cpp:133` | {0}, overload {1} | {0} (+{1}) | "Overload" is jargon. |
| `UI/Combat/Battle/BattleHUDWidget.cpp:406` vs `BattleHUDView.cpp:288` | Statuses / Status | Status | Same panel, two words. |
| `UI/Combat/Battle/BattleHUDView.cpp:351` | Loadout: {0}\nWeapon: {1}\nRing: {2} | …\nAdapter: {2} | Ring is one adapter form. Adapter is the word everywhere else. |
| `UI/Combat/CombatCommandMenuSubsystem.cpp:961` | Let AI Play | Auto | The Party screen says "Let AI finish" (`UI/Party/PartyControlWidget.cpp:268`). Pick one. "Auto" is the usual button (Persona 5, Honkai: Star Rail). |
| `UI/Combat/CombatCommandMenuSubsystem.cpp:1001` vs `UI/Menus/SettingsScreenModel.cpp:166` | Switch Adapter / Change adapter | Switch Adapter | Same action, two verbs. The weapon one is "Switch weapon". |
| `UI/Combat/CombatCommandMenuSubsystem.cpp:1822` | Cycle Source | Source | Pressing the button already does the cycling. |
| `UI/Combat/CombatCommandMenuSubsystem.cpp:1802` | {0}: L{1} | {0} Lv {1} | "L" alone is unclear. Lv is the standard short form (Final Fantasy, Pokémon). |
| `UI/Combat/CombatCommandMenuSubsystem.cpp:1784` vs `UI/Field/FieldSkillPickerWidget.cpp:74` | Weapon Crystal / Weapon crystal | (one case) | Same name, two cases. |
| `UI/Field/FieldSkillPickerWidget.cpp:76` | Ring | Adapter | Same as the gear tooltip. |
| `Combat/CombatOrchestrator.cpp:1577` | A Void break is playing this turn. | Void Break in progress. | Crown's name is **Void Break**, with a capital B. |
| `Combat/CombatOrchestrator.cpp:1568` | Switch a member on its own turn. | Only on their turn. | Matches `:881` "Only on your turn." Also "its" → "their". |
| `Combat/CombatOrchestrator.cpp:833` | There is no fight to flee. | (fine as is) | Players can hardly see it. |
| `Combat/Actions/ActionPipsText.cpp:117` | Ends your turn; the actions left stay banked. | (keep) | It's a rule. |

### Effect text (ET-50, `Skills/Effects/EffectText.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `:309`, `:337` | When you cast a Refraction / casts a Refraction | When you cast a spell / casts a spell | Every menu says Spell. "Refraction" is the deckbuilder's word. |
| `:891`, `:892` | {0} Refractions and Abilities cost {1} less | {0} skills cost {1} less | "Skill" already means spell or ability (Loadout's Skills page, battle menu "Skills"). |
| `:810` | Refraction | Spell | Same. |
| `:356`, `:367` | a Team Member / a Team Member's | an Ally / an Ally's | `:51` already says "an Ally". Two words for one side. |
| `:357`, `:368` | the attacked Enemy / the attacked Enemy's | the target / the target's | Shorter. Slay the Spire and Hearthstone use "the target". |
| `:334` | defeats an opponent | defeats an Enemy | Matches `:306` "When you defeat an Enemy". |
| `:302` | On Damage Received | When Hit | Short, and matches "On Hit". *Optional.* |
| `:303` | At the start of the fight | At battle start | Shorter. *Optional.* |
| `:773` | inflict {0} damage over time on {1} | inflict {0} DoT on {1} | Crown's **\<Element\> DoT** naming (Q-064a2, cloud branches). |
| `:778` | damage over time | DoT | Same. |
| `:191`, `:211`, `:705-716` | Max Energy, Energy, "gain {0} Energy" | Max EP, EP, "gain {0} EP" | The HUD, Character sheet and refusals all say **EP** (`BattleHUDView.cpp:112`, `FieldSkillPickerWidget.cpp:95`). *Crown's check:* EP or Energy. |
| `:185`, `:797`, `:882` | Defense | Defence | The rest of the game is UK English (Defence in `WoRGameNames.cpp:784`, colours, Centred, armour). |
| `:312`-`:319` | Perfect Dodge / Perfect Guard / Flawless Parry | (check with Crown) | Two words for "perfect". Keep both if the ruling wants Flawless Parry to stand apart. |

### Shared name table (`WoRGameNames.cpp`)

These names show in tooltips and menus. The bracketed notes read as notes for designers.

| Where | Now | Shorter | Why |
|---|---|---|---|
| `:78`-`:88` | Mind Buff (All Mind Sub-Stats) … | Mind Up / Mind Down | The note in brackets is for designers. "X Up / X Down" is what Octopath Traveler and Persona 5 use. |
| `:140`-`:144` | Damage Buff (Generic) / Speed Buff (Generic) | Damage Up / Speed Up | "Generic" means "inherit" in this game (CLAUDE.md), which misleads. |
| `:152` | Heal (Instant) | Heal | |
| `:158` | Cleanse (Remove All Debuffs) | Cleanse | Effect text says what it does. |
| `:166` | Stun (Attack/Defend Only) | Stun | Same. |
| `:172` | Random Skill (Loss of Control) | Confuse | Final Fantasy's and Persona's name for "acts at random". *Optional.* |
| `:256`, `:258` | Status Increase (Build Gauge) / Status Decrease (Reduce Gauge) | Buildup Up / Buildup Down | "Gauge" and "status" both show up here as other words for buildup (`EffectText.cpp:897`). |
| `:276`, `:278` | Push Back (one row) / Pull Forward (one row) | Push Back / Pull Forward | |
| `:64`-`:134`, `:174`-`:188`, `:254` | Mixes "Buff/Debuff", "Up/Down", "Increase/Reduce" (e.g. "Damage Up" next to "Increase Damage Taken") | **X Up / X Down** everywhere | Same idea in three patterns. |
| `:62`, `:208`, `:282` | Damage Over Time / DOT Immunity / Apply Damage over Time | DoT / DoT Immunity / Apply DoT | Three spellings. Crown's form is "DoT". |
| `:206` | Absorb | Barrier | Same note as the HUD. `:285` already has Barrier. |
| `:430`-`:442` | Blacksmith (weapons, augment stones) … Loadout Vendor (purchase loadout) | Blacksmith … | `UI/Menus/VendorScreenModel.cpp:288-292` already shows that line under the name. Shown together, it's said twice. |
| `:496` | Typed Essence | (check where shown) | Jargon if a player sees it. |
| `:66`, `:110` and others | Defense Debuff / Defense Buff | Defence Down / Defence Up | Spelling and pattern. |
| `:48`, `:232`, `:469` | Unknown | Reality | Known bug B-269. Listed here only so it isn't missed. |

### Pause menu (`UI/Menus/PauseMenuModel.cpp`, `PauseMenuWidget.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `PauseMenuModel.cpp:77` | Back to the game. (Resume preview) | (no preview) | "Resume" says it. |
| `:79` | Weapons, adapters and apparel, their sockets, and your saved loadouts. | Gear, sockets and saved loadouts. | Long list. |
| `:81` | Each character's gear and pouch, and the home chest. | Gear, pouch and chest. | |
| `:90` | Controls, camera, display, audio and accessibility. | (drop, or keep short) | The Settings page tabs list these. |
| `:98` | Quitting now ends the run and you lose everything from it: its Prisms, its rewards, all of it.␠ | Quitting ends the run. You lose its Prisms and rewards. | Keeps the rule. Also has a trailing space. |
| `PauseMenuWidget.cpp:171` | Hold A or Enter to quit. B or Esc goes back. | Hold A to quit. | The hint bar shows Back. |
| `PauseMenuWidget.cpp:185` | B / Esc  Resume | B / Esc  Back | Every other menu says Back. |

### Title and New Game

| Where | Now | Shorter | Why |
|---|---|---|---|
| `UI/Menus/TitleScreenWidget.cpp:284` | Up / Down  Choose        A  Select | A  Select | Moving with Up / Down needs no hint. Also "Choose" and "Select" side by side. |
| `TitleScreenWidget.cpp:269` | This build can't read saves from the older version, so the game starts fresh. This can happen until the first public release; after that, saves are always carried forward. | This build can't read your old save, so the game starts fresh. This can happen until the first public release. | Shorter. Keeps the rule. |
| `UI/Menus/NewGameScreenWidget.cpp:229` | B / Esc  Back to title | B / Esc  Back | The same Back as everywhere. |
| `NewGameScreenWidget.cpp:101` | placeholder heading | (Crown's words) | Stand-in. The heading is lore, so it's Crown's call. |
| `UI/Menus/NewGameScreenModel.cpp:95` | defence | Defence | Matches the stat name. |

### Settings (`UI/Menus/SettingsScreenModel.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `:106` | Button hints in the corner | Button hints | The explain line says where they show. |
| `:110` | AI finishes my turn when I idle | Auto-finish turn | The explain line has the detail. |
| `:122` | Hold to confirm dangerous actions | Hold to confirm | Same. |
| `:124` | Element letters on colours | Element letters | Same. |
| `:119` | Camera movement in fights | Camera motion | Same. |
| `:103` | Shows the End Turn button on the battle menu. Off hides it. | Shows the End Turn button on the battle menu. | An On/Off switch already means that. |
| `:140` | Keys are fixed for now. Rebinding comes later; every action already has its own name so it can. | Keys are fixed for now. Rebinding comes later. | The second half is a note for developers. |
| `:170` | Skip a boss cinematic | Skip cinematic | |
| `:178` | Change a setting | Change | It's under Menus. |

### Character sheet

| Where | Now | Shorter | Why |
|---|---|---|---|
| `UI/Menus/CharacterSheetModel.cpp:197` | {0} isn't here, so their stats can't be shown. | {0} isn't here. | The empty panel shows the rest. |
| `UI/Menus/LoadoutScreenModel.cpp:388`, `:571`; `UI/Menus/LoadoutSkillsModel.cpp:87`, `:179` | {0} isn't here, so their loadout can't be shown. / …gear can't be changed. / …skills can't be changed. / {0} isn't here; their skills show where they are. | {0} isn't here. | Four ways to say the same thing. |

### Loadout (`UI/Menus/LoadoutScreenModel.cpp`, `LoadoutScreenWidget.cpp`, `LoadoutSkillsModel.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `LoadoutScreenModel.cpp:203` | Change to put a piece you own here. | Empty. | The hint bar shows "A Change". |
| `LoadoutSkillsModel.cpp:31` | Change to put a skill you know here. | Empty. | Same. |
| `LoadoutScreenWidget.cpp:330` vs `:329` | X  Unequip / X  Remove | X  Unequip on both, or Remove on both | Same button, two words. |
| `LoadoutScreenWidget.cpp:328` | A  Put in | A  Equip | One verb for putting gear on. |
| `LoadoutScreenModel.cpp:420`, `:720` | Over the budget: {0} of {1}. | Over budget: {0} / {1}. | Shorter. |
| `LoadoutScreenModel.cpp:712` | It's in use: in a loadout or on other gear. | Already in use. | |
| `LoadoutScreenWidget.cpp:341` | You know nothing that fits here. / Nothing you own fits here. | Nothing fits here. | One line for both cases. |
| `LoadoutScreenWidget.cpp:502` | empty, A fills it | empty | The hint bar shows A. |
| `LoadoutSkillsModel.cpp:58` | {0}  (tier {1}; {2} less your pillar discount) | {0}  (tier {1}, −{2} pillar discount) | Shorter. |
| `LoadoutScreenModel.cpp:318`, `:554`, `:857`, `:860` | (rules) | (keep) | These are rules the player needs. |

### Inventory and Give (`UI/Menus/InventoryScreenModel.cpp`, `InventoryScreenWidget.cpp`, `Inventory/*`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `InventoryScreenModel.cpp:356` vs `:235` | Home chest / Chest | Chest | Same place, two names. |
| `InventoryScreenWidget.cpp:192` | A / Enter  Choose | A / Enter  Select | Every other screen says Select. |
| `Inventory/GearGive.cpp:225`, `Inventory/InventoryVerbs.cpp:186`, `:221`, `Shop/VendorServices.cpp:161`, `Currency/EconomyService.cpp:702` | That is a loadout's primary. Swap in another primary before giving it | Every loadout needs a weapon or adapter. Put another in first. | "Primary" is jargon, and D-110 says weapons have no primary. Five wordings of one refusal. |
| `Loadout/LoadoutComponent.cpp:1101`, `:1147` | No primary weapon is equipped. / No primary adapter is equipped. | No weapon equipped. / No adapter equipped. | Same. |
| `Inventory/GearGive.cpp:217`, `:305`, `InventoryVerbs.cpp:182`, `Currency/EconomyService.cpp:1227` | {0} is only here for the run and can't own gear | (keep, one wording) | It's a rule. Four slightly different wordings. |
| `Inventory/GearGive.cpp:379`, `:436`; `Currency/EconomyService.cpp:1221` | {0}'s skills aren't kept yet (they join at the next level) / They can't learn skills for keeps yet. | {0} keeps skills from the next level. | Jargon ("kept", "for keeps"). One wording. |
| `Inventory/GearGive.cpp:329` | They carry the same pouch: a recruit uses the host's | They share a pouch. | |
| `Inventory/GearGive.cpp:197`, `:319`, `:352` | No armoury to give from / No pouches to give from / No skills to give from | Nothing to give. | Players shouldn't see "armoury". |
| `Inventory/GearGive.cpp:266`, `:272` | Someone is wearing that. Take it off before putting it in the chest / That is in a loadout. Take it off… | Equipped. Unequip it first. | Shorter, and uses the shared verbs. |
| `InventoryVerbs.cpp:192` | It's in a loadout. Take it off first | Equipped. Unequip it first. | Same. |
| `Inventory/GearGive.cpp` (all), `InventoryVerbs.cpp` (all) | No full stops | Full stops, like the other screens | Punctuation differs from screen to screen. |

### Vendors (`UI/Menus/VendorScreenWidget.cpp`, `VendorScreenModel.cpp`, `Shop/*`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `VendorScreenWidget.cpp:201`, `:399` | X  Purchase | X  Buy | The tab is "Buy". Buy / Sell is the shop pair (Final Fantasy, Persona 5). |
| `VendorScreenWidget.cpp:399` | Left  Take back | Left  Remove | Clearer for taking something out of the cart. |
| `VendorScreenWidget.cpp:399`, `:537` | R / Y  Buying for | R / Y  For | The name is shown beside it. *Optional.* |
| `VendorScreenWidget.cpp:495` | Buy one item slot | Buy slot | You are on the Item slots tab. |
| `VendorScreenModel.cpp:702` | Left / Right picks the tier to make. | (drop) | The hint bar already shows "Left / Right  Tier". |
| `VendorScreenWidget.cpp:748` vs `UI/Hub/ItemSlotUpgradeWidget.cpp:224` | …now holds {1} kinds of item. / …now holds {1} item types. | Pouch: {1} slots. | Same message, two wordings. |
| `VendorScreenWidget.cpp:797` | Everything is repaired, for {0}. | All repaired for {0}. | |
| `VendorScreenModel.cpp:445`, `:892`, `:730`, `:792`; `Memory/CurseSubsystem.cpp:119`; `Inventory/ItemSlotUpgrade.cpp:84`; `UI/Shop/ShopWindowWidget.cpp:374` | The purchase didn't go through. / It didn't go through. / The payment didn't go through. / The payment failed. / Purchase failed | Couldn't buy that. | Five wordings. |
| `Inventory/ItemSlotUpgrade.cpp:68`; `Memory/CurseSubsystem.cpp:100`; `Trial/LevelDropPlacement.cpp:138` | You need {0} {1}. / Clearing it costs {0} {1}; you have {2}. / A point costs {0} {1} | Not enough {0}. | `EconomyService.cpp:393` already says this. Final Fantasy's "Not enough gil." |
| `Equipment/Durability/GearRepair.cpp:242`; `Inventory/ItemSlotUpgrade.cpp:59` | No wallet to pay from. | (shouldn't reach players) or "Can't pay right now." | Jargon. |
| `Inventory/ItemSlotUpgrade.cpp:64` vs `Currency/EconomyService.cpp:365` | Only the host can buy item slots. / Only the host can buy for now. | Only the host can buy for now. | One wording. |
| `VendorScreenModel.cpp:618` | Gear wears in a trial's fights. A repair brings it back to full; when the run ends it comes back whole anyway. | Gear wears in fights. Repair restores it; it's restored anyway when the run ends. | Shorter. Keeps the rule. |
| `VendorScreenModel.cpp:642`, `:714`, `:769` | It may come back. | (keep) | Rule, and the mockup quotes it. |

### Rest spot (`UI/Hub/RestSpotWidget.cpp`, `Trial/TrialRestSpot.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `RestSpotWidget.cpp:103` vs `Trial/TrialRestSpot.cpp:150` | Rest spot: the party is restored. / You rest. Your characters are restored. | The party is restored. | The screen title is "Rest spot". Also one message worded two ways. The mockup already uses the short line. |
| `RestSpotWidget.cpp:117` | Vote yes: revive the mini-boss | Yes | The panel asks "Revive the mini-boss?" (mockup). |
| `RestSpotWidget.cpp:119` | Vote no | No | Same. |
| `RestSpotWidget.cpp:123` | Invite and seat friends | Invite | The Party screen calls it "Invite" (`UI/Party/PartyScreenWidget.cpp:96`). |
| `RestSpotWidget.cpp:243` | Revive the mini-boss: one tier stronger, better drops ({0} of {1}) | Revive the mini-boss ({0} of {1}) | Move "one tier stronger, better drops" to the side panel. It's a rule, so keep it there. |
| `RestSpotWidget.cpp:183` | Earned so far: … (leaving now pays {3}%). | (keep) | Rule. |
| `Trial/TrialRestSpot.cpp:136` | Leave the trial? You keep part of what you earned. Full rewards only for beating the boss or falling in battle. | (keep) | Rule. |

### Gate (`UI/Trial/GateScreenWidget.cpp`, `GateScreenModel.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `GateScreenWidget.cpp:122` | Pick a gate, then a trial, then the difficulty | (drop) | The three headings show the steps. |
| `GateScreenWidget.cpp:208` | B / Esc  Cancel | B / Esc  Back | Every menu says Back. Cancel is for confirm boxes. |
| `GateScreenWidget.cpp:400`, `GateScreenModel.cpp:129` | That trial is still locked. | Locked. | The row is greyed and shows ???. |

### Party and co-op (`UI/Party/*`, `UI/Coop/*`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `UI/Party/PartyScreenModel.cpp:52` vs `PartyControlWidget.cpp:240` | Starts: placed for you / auto | Starts: Auto | Two wordings. |
| `PartyScreenModel.cpp:88`; `UI/Coop/JoinRequestModel.cpp:80`, `:132`; `Loadout/LoadoutComponent.cpp:1167`; `Inventory/GearGive.cpp:369`, `:421`; `Character/Forms/CharacterFormData.cpp:39` | Seats are fixed while a fight runs. / Seats are fixed during a fight: answer when it ends. / Gear can't be changed during a fight. / Skills can't change hands during a fight / You can't change form in a fight. | Not during a fight. | `Currency/EconomyService.cpp:187` and `HostHandOverModel.cpp:119` already say this. One line everywhere. |
| `PartyScreenWidget.cpp:312` | If a friend joins: they can take this seat. | Open to a friend. | |
| `PartyScreenWidget.cpp:301` | Host a session and invite a friend to take a seat. | (keep or drop) | The Invite row speaks for itself. *Optional.* |
| `UI/Coop/InviteScreenModel.cpp:66` | No session: host one so friends can join | Not hosting | The Host button is right below it. |
| `InviteScreenModel.cpp:79`; `UI/Party/InviteWidget.cpp:111` | Host a session / Host a session (this level reopens for friends) | Host | "Stop hosting" is its pair. |
| `UI/Party/InviteWidget.cpp:165` | {0} friend(s); picked {1} of {0} | {1} of {0} | Placeholder. "(s)" plural hack. |
| `UI/Party/JoinSeatWidget.cpp:181`, `:182` | Give slot {0} ({1}, AI seat held for the friend who left) to the first request | Give slot {0} | Placeholder. The card shows the rest. |
| `UI/Coop/JoinRequestModel.cpp:93` | Hold View (Tab) to answer  ·  then  A  Give this seat    X  Refuse | Hold View to answer | The answer keys show once it's open. The mockup drops "(Tab)". |

### Forms (`UI/Hub/CharacterFormWidget.cpp`, `FormsScreenModel.cpp`, `FormsScreenWidget.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `CharacterFormWidget.cpp:118` | Take the skin off | Remove form | The screen says "form" everywhere else. The rest-spot mockup's "Skins" row should say Forms too. |
| `CharacterFormWidget.cpp:116`; `FormsScreenWidget.cpp:113` | Wear this form | Wear | You are on the form. |
| `CharacterFormWidget.cpp:180`; `FormsScreenModel.cpp:168` | Change your body in the hub or at a rest spot, outside a fight. | (keep) | Rule. |
| `Character/Forms/CharacterFormData.cpp:41`; `FormsScreenWidget.cpp:102` | Change form in the hub or at a rest spot. | (keep, same as above) | Rule. Good that it's the same line twice. |

### Level drop (`UI/Hub/LevelDropWidget.cpp`, `Trial/LevelDropPlacement.cpp`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `LevelDropWidget.cpp:112`, `:185`, `:187` | Purchase point | Buy a point | The mockup's words. Buy, not Purchase. |
| `LevelDropWidget.cpp:176`; `LevelDropPlacement.cpp:125`, `:171` | over-cap | extra | Jargon. |

### Notices and popups (`Memory/*`, `Trial/*`)

| Where | Now | Shorter | Why |
|---|---|---|---|
| `Memory/HiddenQuestSubsystem.cpp:36`-`:49` | Secret quest found. Open the journal to view it. (×4 variants) | Secret quest found. | The popup shows "J  Journal" (run_end mockup). |
| `HiddenQuestSubsystem.cpp:36` vs UIScreens.md and the run_end mockup | Secret quest / Hidden quest | (pick one) | The code says Secret (`WoRGameNames.cpp:396`), the design doc says Hidden ("Hidden Quest 2", "Hidden quest discovered"). *Crown's check.* |
| `Trial/TrialMapFeature.cpp:225`, `:226` | Chest: {0} + Found {0}. | Found {0}. | Two messages for one chest. |
| `TrialMapFeature.cpp:205`, `:206` | An empty chest + The chest is empty. | The chest is empty. | Same. |

### Old placeholder panels (`UI/Hub/*Widget.cpp`, `UI/Shop/VendorServiceWidget.cpp`)

Low priority. The real screens replace these (D-024).
- **Close** on every panel. The real screens use "B / Esc  Back". Keep Close while they are click-only panels.
- `UI/Hub/GiveWidget.cpp:120`: "Ask for it (the receiver asks the giver)" → **Ask for it**.
- `GiveWidget.cpp:122`: "Teach it (another player's character, for Mentor Tokens)" → **Teach (Mentor Tokens)**.
- `UI/Hub/MemoryJournalWidget.cpp:233`: "Memory journal" → **Journal** (the real screen's title).
- `UI/Field/FieldSkillPickerWidget.cpp:254`: "No field character (open this on the map)" → **Not available here.**

---

## 3. Words to use the same way everywhere

| Use | Not | Where it's mixed now |
|---|---|---|
| **Back** (B / Esc) | Close, Resume, Cancel, Back to title | Pause, Gate, New Game, placeholders |
| **Cancel** only in a confirm box | Back | |
| **Select** (A) | Choose | Title, Inventory |
| **Leave** | Leave the trial / the session, Confirm: leave | Rest spot. Vendors already say "Leave". |
| **Buy / Sell** | Purchase | Vendor hints, Level drop, old shop |
| **Equip / Unequip**, **Equipped** | Put in, Remove, Take off, Wear (for gear), Worn (for equipped) | Loadout, Inventory, Give refusals |
| **Damaged / Broken** (durability) | Worn | Repair tab |
| **Not during a fight.** | seven other wordings | Party, Join, Loadout, Give, Forms |
| **Not enough {currency}.** | You need…, …costs; you have… | Item slots, Curses, Level drop |
| **Couldn't {verb} that.** | didn't go through, failed, (see the log) | Vendors, Give, Loadout |
| **{0} isn't here.** | four longer forms | Character, Loadout, Skills |
| **EP** (*Crown's check*) | Energy | Effect text vs HUD |
| **Spell** (and **Skill** = spell or ability) | Refraction | Effect text |
| **Ally / Enemy / the target** | Team Member, opponent, the attacked Enemy | Effect text |
| **Adapter** | Ring (it's one form of adapter) | Battle tooltip, field skill picker |
| **Form** | Skin | Forms, rest-spot mockup |
| **Chest** | Home chest | Inventory |
| **Secret quest** or **Hidden quest** (*Crown's check*) | the other one | Code vs design doc |
| **Defence** (UK spelling, like colours, armour) | Defense | Effect text, effect names |
| **X Up / X Down** for buffs | Buff / Debuff / Increase / Reduce | Effect names |
| **DoT**, **\<Element\> Break**, **Negated** (Crown's) | Damage Over Time, DOT, Void break, Absorb | Effect names, HUD, battle refusals |
| **Key + Verb · Thing** prompts | "Press E to …" | Every interact prompt |
