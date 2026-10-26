# UI reuse from wor-deckbuilder

**Decision (Crown, 2026-10-25):**
- The deckbuilder's UI assets get migrated into this project.
- Its screen layouts are reused **where relevant**.
- This doc maps every screen in `UI_ScreenInventory.md` to the deckbuilder layout and kit pieces that cover it, lists the gaps, and sets out the import steps.

**Source:** `CrownedDev/wor-deckbuilder` (Godot 4 / C#, private), checked 2026-10-25. All paths in this doc are in that repo:
- **Art direction:** `docs/design/WoR_Art_Direction.md`
- **Kit pieces chosen per UI role:** `docs/design/UI_Asset_Picks.md`
- **Mockup index:** `docs/design/screen_mockups/index.md`
- **Mockups:** `docs/design/screen_mockups/<area>/<screen>/vN/`, each a `_plain.png` and an `_annotated.png` (1280×720)
- **Mockup generator:** `tools/ui_drafts/` (Python + Pillow, draws from the kits)
- **Pack list and licences:** `CREDITS.md`

---

## 1. Art direction — adopt the deckbuilder's, with one adjustment

**Base:** the deckbuilder's *"Dark stone and steel, lit by crystal"*:
- **Colours:** steel `#8C9098` frames, platinum for tier and premium, pale cyan-white crystal light, dark stone backgrounds.
- **Fonts:** Spectral SC for titles/display, Lato for everything else. Lato Bold for buttons.
- **Button colours:** orange = play / continue, green = confirm, red = danger, purple = navigate.
- **Motion:** one loud effect at a time, ambient motes and slow light sweeps, everything eases, and a "Reduce effects" setting.
- **Background:** the Kitsinu galaxy background on hub screens; the title screen keeps the nebula + swirl portal.

**Adjustment for this game:** the deckbuilder rule is "element colours belong to cards; UI is neutral". Here, the equivalent of a card is the character, the skill and the status:
- Element colour **is** allowed on:
  - character portrait frames (inner glow)
  - status-buildup bars
  - skill buttons and skill icons
  - status-effect icons
  - proc banners ("BURN!")
- Chrome stays steel: panels, headers, menus, generic buttons.

This replaces the "faceted glass + element glow on all frames" idea in `UI_ScreenInventory.md` → *Art direction reference*. That section's **Expedition 33 combat layout** (vertical turn strip, party cards bottom-right, enemy plate near target, face-button diamond) still stands for the combat HUD, which the deckbuilder has no equivalent for, because it's a card table.

---

## 2. Screen mapping

**Reuse levels:**
- **Direct** — same layout, swap content.
- **Adapt** — same structure and kit pieces, different content or arrangement.
- **Template** — build on the deckbuilder's *Shared template* (`shared/template/v1`).
- **New** — no deckbuilder equivalent; compose from the kit.

### Front end
| # | Screen | Reuse | Deckbuilder source |
|---|---|---|---|
| 1 | Splash / logos | New | — (simple fade; use the title backdrop) |
| 2 | Title screen | **Direct** | `start/title/v2` (nebula, swirl portal, crystal emblem, Spectral SC title, "Tap to start" breathing; gamepad: "Press any button") |
| 3 | Main menu | **Adapt** | `start/home/v4`. The 2×2 big gem buttons become Continue / New Game / Load / Settings; the Continue button pulses |
| 4 | Save slot select | Template | `shared/template/v1` + `collection/deck_list/v1` (list of slots) |
| 5 | New game / character select | **Adapt** | `onboarding/character_select/v1` (portrait cards with HP/EN strip + element gems) → 3 class cards + element pick |
| 6 | Settings | **Direct** | `account/settings/v6` (incl. Accessibility: Reduce effects, Text size). Add a Controls tab for gamepad rebinding |
| 7 | Loading screen | New | Galaxy background + tip text + crystal spinner (kit) |
| 8 | Confirm / message dialog | **Direct** | `modals/confirm/v2` (main · alternate · Cancel; green/slate/red) |
| 9 | Credits | Adapt | Template + `CREDITS.md` structure (it was written to feed a Credits screen) |

### Hub
| # | Screen | Reuse | Deckbuilder source |
|---|---|---|---|
| 10 | Hub HUD | Adapt | Prisms chip (Picks #10), nameplate/header bar (Picks #2); party portraits from `onboarding/character_select` |
| 11 | Interaction prompt | New | Small `popup` frame + input glyphs (`icons/glyphs/v1`) |
| 12 | Merchant shop | **Adapt** | `shop/shop/v3` (tabs, tiles, Prisms chip, purchase). Already built in UE — restyle with the kit |
| 13 | Party management | Adapt | `play/room/v4.10` (seats, swap, owner/host controls) → 3 party slots, leader locked |
| 14 | Recruit dialog / character card | Adapt | `collection/card_detail/v2` layout with a character in place of a card |
| 15 | Trial select (door) | **Adapt** | `play/encounter_popup/v5.2` (variant + difficulty in a row, Go) and `play/solo_events/v3` (event rows with art) |
| 16 | NPC dialogue box | New | `popup_big_bar` frame (Picks #4) + portrait |

### Menus (gear & progression)
| # | Screen | Reuse | Deckbuilder source |
|---|---|---|---|
| 17 | Pause / game menu | **Adapt** | `battle/menu/v4` (Back · How to Play · Settings · Quit) |
| 18 | Character sheet | Adapt | `account/profile/v5` (overview layout, rank crystal badge) → stats, pillars, resistances |
| 19 | Inventory | **Adapt** | `collection/deck_list/v1` + `collection/deck_editor/v4` (grid + filters + detail panel) |
| 20 | Loadout / equip | **Adapt** | `collection/deck_editor/v4` (collection on one side, equipped set on the other). Loadout slots from Shop v3's "Loadout slot" |
| 21 | Crystal attachment | Adapt | `collection/card_detail/v2` (two faces + Flip → item + attached crystal) |
| 22 | Fusion / augment stones | Adapt | Same as 21, with the dismantle pop-up's live preview |
| 23 | Workshop (dismantle / merge / level-up) | **Direct** | `collection/dismantle/{popup, same_face, bulk}/v1` (quantity stepper, live essence preview, warnings, PvP-won flag) |
| 24 | Spell & ability book | Adapt | `collection/deck_list` + `modals/deck_preview/v2` (list + big detail on the right) |
| 25 | World-stat draft (pick 3 of 5) | **Adapt** | `battle/choice/v4` (choose among options, "None" option) |
| 26 | Reroll | Adapt | Dismantle pop-up structure (cost preview → confirm) |
| 27 | Codex / element chart | Adapt | `onboarding/practice/v2` (lesson list) + element gems |

### Trial
| # | Screen | Reuse | Deckbuilder source |
|---|---|---|---|
| 28 | Trial HUD | Adapt | Header bar + Prisms chip as hub |
| 29 | Encounter warning / join countdown | **Adapt** | `play/encounter_popup/v5.2` + `play/room/v4.10` countdown |
| 30 | Battle transition | Adapt | Pack reveal VFX (`shop/pack_opening/reveal/card_back`: swirl portal) as the transition |

### Combat
| # | Element | Reuse | Deckbuilder source |
|---|---|---|---|
| 31 | Character panels | Adapt | `battle/battle/v15.2` nameplates ("You · HP 97/100 · EN 4 · ACT 2") + Expedition 33 party-card layout |
| 32 | Turn-order strip | New | Expedition 33 vertical strip; portrait frames from the kit, rank-crystal style badges for bonus/ritual turns |
| 33 | Command menu | **Adapt** | `battle/battle/v15.2` command strip (GUARD purple · END TURN orange) + `battle/recharge/v6` (verb + cost on each button). Arrange as a gamepad diamond |
| 34 | Target selection | **Adapt** | `battle/target_choice/v2.3` (targets in soft crystal glow, timeout strip) + `battle/attack/v4` |
| 35 | Skill tooltip | **Adapt** | `battle/card_preview/v7` (side panel: name, type, stat deltas, attached crystal) |
| 36 | Defense prompt | New | Turn-start banner style (`battle/turn_start/v3`) as a short centred band + glyphs + shrinking ring |
| 37 | Damage / heal numbers | New | Lato Bold, outlined; element-coloured variants |
| 38 | Status-effect icons | **Direct** | `icons/effects/v1` (a kit icon for every effect type, tinted) + `battle/effects/v5` (icon placement and status list: "Amp +2 · 2 turns", "Protected · 2 charges") |
| 39 | Element proc banner | **Adapt** | `battle/turn_start/v3` banner (dimmed board, glowing band, big outlined text) → "BURN!" in the element colour |
| 40 | Broken Darkness gauges | New | Progress bar (Picks #11) + action-pip styles (`cosmetics/table/action_pips`) for stacks |
| 41 | Weather indicator | New | Small kit icon + chip |
| 42 | Combat log | **Direct** | `battle/battle/v15.2` left Battle Log panel + `modals/log_popup/v2` |
| 43 | Combat pause | **Direct** | `battle/menu/v4` |
| 44 | Results | **Direct** | `battle/result/v6` (VICTORY ribbon, per-character grade crystals, rank progress, achievements, +Prisms, Home / Stats / Find again) + `battle/match_stats/v4` |
| 45 | Loot / rewards | **Adapt** | `shop/pack_opening/summary/v1` + `shop/pack_opening/reveal/c_burst_fan/v3` (reveal by tier) |
| 46 | Retry / return | Direct | `battle/result/v6` `defeat_*` shots |
| 47 | Tutorial callouts | Adapt | `onboarding/practice/v2` + `battle/interactions/v3` (cues on things that can act) |

### Online / later
| # | Screen | Reuse | Deckbuilder source |
|---|---|---|---|
| 48 | Lobby / co-op invite | **Direct** | `play/room_popup/v2.2` + `play/room/v4.10` (host code, seats, swap, ready) |
| 49 | Matchmaking / PvP challenge | **Direct** | `play/versus/v5.2` (incl. searching / match found / cooldown) + `play/counterpick/v2.2` + `play/rps/v1.2` |
| 50 | Leaderboards | Adapt | `account/profile/v5` history + rank crystals |
| 51 | Store / cosmetics | **Direct** | `shop/shop/v3` + `cosmetics/*` (card back → character cosmetics; table parts → battle arena parts) |
| 52 | Achievements | **Direct** | `account/profile/v5` achievements + `account/missions/v2` |

**Totals:**
- 13 Direct
- 29 Adapt
- 1 Template
- 9 New

The **New** ones are mostly combat-specific: turn strip, defense prompt, damage numbers, BD gauges, weather, interaction prompt, splash, loading, NPC dialogue.

---

## 3. Shared kit pieces (from `UI_Asset_Picks.md`)

| Role | Deckbuilder pick |
|---|---|
| Background | Kitsinu `Galaxy Background/dfhb.png`; title: code nebula + ARCLIGHT `portal_arcane_swirl_portal` |
| Header bar | `text_frame_silver` / `text_frame_gold` / `Main_big_bar_horizontal` (PONETI) — pick one |
| Panels | `Basic_window`, `Basic_window_2`; raised: `Main_big_bar` |
| Pop-up | `popup_big_bar` (~45% opaque fill) |
| Buttons | Grey gem + silver border, tinted by job (Picks "Button construction"); square icon buttons 512 px; tabs at Little size |
| Prisms chip | `text_frame_silver` plaque + platinum crystal `#D6DEEC` |
| Progress bar | `hp_enemy_mini`, recoloured to steel |
| Dividers | `separate_line_silver`, `separate_lineLG…` |
| Banners | `Flag_red` + gold crystal bars |
| Rank / tier crystals | Square rank crystals, one colour per letter (Picks "Rank crystals") — reuse for item tiers F–S |
| Icons | PONETI `UIicons/Flat/White` (tinted) + Fantasy Icons Megapack |
| VFX | HD Spell VFX packs (ARCLIGHT etc.) |
| SFX | Cards Sounds Vol. II, Glass Sound Effects Vol. I, PONETI's CC0 sounds |
| Fonts | Spectral SC (Medium, SemiBold), Lato (Regular, Bold) — OFL, in the deckbuilder repo |

---

## 4. Migrating the assets into Unreal

The deckbuilder's art is **raw image, font and sound files**, not Unreal assets, so it's an **import**, not Unreal's *Migrate*.

### 1. Licences
- Check each pack in `CREDITS.md` allows use in Unreal and in a commercial game.
  - **PONETI packs:** Unity Asset Store EULA.
  - **itch.io packs:** per-author terms.
  - **Fonts:** OFL, fine.
- Record each pack in a `CREDITS.md` in this repo in the same change that adds it (the deckbuilder's rule).

### 2. Git first
- This repo LFS-tracks only `*.uasset` / `*.umap`.
- If raw source files (`.png`, `.psd`, `.ttf`, `.wav`) are committed, add them to LFS first.
- Or keep source files out of git (as the deckbuilder does) and commit only the imported `.uasset`s.
- Licensed packs must stay in a **private** repo either way.

### 2a. Decision — "save what we use" (Crown, 2026-10-25)
- **In git (LFS):** the original file of every asset the game **actually uses**, under `SourceArt/<Pack>/…`, mirroring `Content/UI/Vendor/<Pack>/`. That includes edited and recoloured pieces and our own art.
- **Not in git:** the full downloaded vendor packs. Keep them in a local backup folder; each is listed in `CREDITS.md` so it can be re-downloaded.
- **LFS rules added to `.gitattributes`:** `png jpg jpeg tga psd exr fbx ttf otf wav ogg mp3`. This also closes the CLAUDE.md "LFS coverage is incomplete" note.
- The repo stays **private**, because the used files include licensed art.

### 3. Folder layout
`Content/UI/Vendor/<Pack>/` for imported textures/fonts/sounds, `Content/UI/Library/` for our composed pieces (buttons, panels as Slate brushes / styles).

### 4. Import settings
- **UI textures:**
  - Texture Group = UI
  - Compression = UserInterface2D
  - Mip Gen = NoMipmaps
  - sRGB on
  - Never Stream
- **9-slice:** frames and panels use **Box** draw type with margins set on the brush (matching the deckbuilder's 9-slice cuts).
- **Fonts:** import the TTFs → Font Face assets → one composite Font per family (Spectral SC, Lato).
- **Big kit images (4096–6144 px):** downscale on import (Max Texture Size 2048 or 1024) or make atlases. UI textures that size waste memory.

### 5. CommonUI styles
Build **CommonUI** style assets once — button styles per job (play / confirm / danger / navigate), text styles (title / body / button) and border styles — so every screen uses the same pieces (CommonUI decided 2026-10-25).

### 6. Mockups
- Copy the relevant `_plain.png` mockups into `docs/design/screen_mockups/` in this repo as layout references (private repo; they contain licensed art).
- Or link to the deckbuilder paths.

---

## 5. Reusing the mockup pipeline

`tools/ui_drafts/` (Python + Pillow) draws plain + annotated 1280×720 drafts from the kits, versioned `vN/`, with a regenerate/check tool.

The same pipeline can draw this game's **New** screens before they're built in UMG:
- turn strip
- defense prompt
- damage numbers
- Expedition 33 party cards
- BD gauges

Same approval flow as the deckbuilder: plain + annotated draft → Crown approves → build.

---

## Open items
1. **Header bar:** pick one of the four candidates (it was still open in the deckbuilder).
2. **Licences:** confirm each pack's terms for Unreal / commercial use.
3. ~~Source art~~ **Decided:** save what we use (§4, 2a).
4. ~~Mockup pipeline~~ **Decided (D-025 A):** the tool is copied to `Tools/ui_drafts/` here (B-100). It reads the kit from `WOR_UI_KIT`, and drafts are committed under `docs/Design/screen_mockups/` (LFS).
