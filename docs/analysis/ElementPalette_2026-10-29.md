# Element Palette and Colour-Blind Check (2026-10-29)

**Status:** proposal (Machado 2009 simulation + CIEDE2000; scripts in the session scratchpad). Being decided with Crown.

---


Scripts: `cvd_sim.py` (simulation + ΔE tables, `python3 cvd_sim.py --full`) and `tune.py` (search) next to this file.

**Method.** Machado, Oliveira & Fernandes 2009, *A Physiologically-based Model for Simulation of Color Vision
Deficiency* (IEEE TVCG 15(6)), severity-1.0 matrices applied in linear RGB (the matrices used by `colorspacious`
and DaltonLens). Differences are CIEDE2000 (Sharma, Wu & Dalal 2005) in CIELAB/D65. Thresholds used here:
ΔE < 10 = indistinguishable at damage-number size, 10–20 = risky, ≥ 20 = readable. Pure Python.

## 1. Where the element colours live, and where they disagree

| Element | `ElementColors.h` (linear → sRGB) | `SkillEffectDisplayNames.h` status colour | Draft `combat_hud_v1/v2.py` | Deckbuilder `CardPalette.cs` |
| --- | --- | --- | --- | --- |
| Fire | (1,0,0) `#FF0000` red | (1,.3,0) `#FF9800` orange | `#EB4632` | `#C4503A` |
| Lightning | (1,.5,0) `#FFBC00` orange | (.9,.9,.3) `#F4F494` yellow | `#FF9628` | `#E8892E` |
| Earth | (1,1,0) `#FFFF00` yellow | (.4,.6,.2) `#A8C87C` olive | `#E6C83C` | `#D9B52B` |
| Wind | (0,1,0) `#00FF00` green | (.7,1,.9) `#D9FFF4` mint | `#5AC86E` | `#6FC2B0` |
| Water | (0,.5,1) `#00BCFF` blue | same as code | `#3C8CFF` | `#3F7FB5` |
| Ice | — (no colour; `EWoRDamageType::Ice` keyed Water) | — | — | — |
| Light | (1,1,1) `#FFFFFF` | (1,1,.8) `#FFFFE8` | `#F0F0F5` | `#F2F0E8` |
| Darkness / BD | (.02,.02,.02) `#262626` | (.3,0,.5) **indigo** | `#7668A0` (lifted) | `#1A1A1E` |
| Void | (.6,0,1) `#CB00FF` violet | (.5,0,.5) purple | `#A03CFF` | `#5A3A7A` |
| Reality | (.3,0,.5) `#9500BC` indigo | (.8,.8,.8) **grey** | `#4E46C8` | `#A878C8` (retired) |
| Generic/None | (.6,.4,.2) brown | white | `#AA7D50` | Neutral |
| Physical | "white" (Q-111, Crown 2026-10-27; no constant yet) | — | `#F8F4EC` | — |

Sources: `Source/world_of_refraction/Public/Infusion/ElementColors.h:12-22`; `Public/Skills/Effects/SkillEffectDisplayNames.h:298-335`;
`Tools/ui_drafts/combat_hud_v1.py:23-25`, `combat_hud_v2.py:32-35,50`; `docs/DesignDecisions.md:358` (D-024: Reality indigo, Void violet),
`:1242` (D-095), `:1983-1990` (Q-111); deckbuilder `wor.game/scenes/components/CardFace/CardPalette.cs:56-70`.
`HybridSpellColors.cpp` holds no colours of its own: it reads `ElementColors::` and blends toward `DarknessBase` (0.05 grey).
Readers of `ElementColors::GetColorForElement`: `CharacterPanelWidget.cpp:626,645,864` (status bar fill, EP bar),
`CombatCommandMenuSubsystem.cpp:2052` (pie-menu element tint), `EncounterComponent.cpp:262` (arena material),
`InfusionVFXComponent.cpp`, `ElementColorDebugComponent.cpp`, `TrialDoor.cpp`. No weather colour table exists (`WeatherSystem.md` has none).

**Disagreements to settle (not CVD-specific):**
1. `SkillEffectDisplayNames.h` is a second, contradicting table: **Darkness = indigo** there (= code's Reality), **Reality = grey**, Fire orange, Lightning yellow, Earth olive. Any screen that reads `GetDisplayInfo().Color` paints a different Darkness/Reality than the bars. B-271 (one element table) is the right place to collapse it.
2. `ElementColors.h` is a pure-primary palette (`#FF0000`, `#FFFF00`, `#00FF00`): fine for VFX emissive, poor for text. The drafts and the deckbuilder already use a toned palette; code and mockup disagree on every element.
3. **Light = `#FFFFFF` = Physical white.** Under Q-111 a Light hit and a Physical hit print the same colour for every viewer (ΔE 0). Draft: 4.6. Must be fixed by palette or glyph.
4. Darkness `#262626` on dark stone is invisible as a number; the draft lifts it to `#7668A0`, which lands between Reality and Void (ΔE 9.8 / 11.3 under protan).

## 2. Simulation results — current palettes (min ΔE2000 over normal + protan + deutan + tritan)

**Code (`ElementColors.h`):** Light/Physical **0**; Lightning/Wind **2.3**; Earth/Wind **4.1**; Lightning/Earth **10.0**; Earth/Light 10.3; Reality/Void 13.1 (also 14 in *normal* vision); Water/Void 13.3; Wind/Water 14.2; Fire/Lightning 17.8; Fire/Wind 19.4.
→ Under protan/deutan the warm half (Fire, Lightning, Earth, Wind) collapses to two or three yellows; Reality/Void is already weak for everyone.

**Draft v2 (`combat_hud_v1.py` + v2 overrides):** Light/Physical 4.6; Lightning/Earth **5.1**; Water/Void **5.8** (deutan); Earth/Wind 9.1; Reality/Void 9.6; Lightning/Wind 9.7; Reality/Darkness 9.8; Void/Darkness 11.3; Fire/Wind 12.1; Fire/Lightning 13.3; Water/Darkness 14.7; Wind/Water 15.0 (tritan); Fire/Earth 18.0; Water/Reality 18.5.
→ Better hues, but Lightning orange ≈ Earth yellow and Water ≈ Void under deutan; the three purples (Reality/Void/lifted Darkness) sit within 10–18 of each other even in normal vision.

**Deckbuilder (`CardPalette.cs`, for reference):** Water/Reality **3.8**; Lightning/Earth **5.6**; Wind/Light 12.8; Fire/Lightning 15.3; Water/Void 16.5; Wind/Water 17.9. (Reality is retired there, so only Lightning/Earth matters.)

Per-condition flags are in the script output; the pattern is the same everywhere: **(a)** Fire/Lightning/Earth/Wind need a *lightness* ladder, not hue, to survive protan/deutan; **(b)** Water/Reality/Void need a lightness ladder too (all three go blue); **(c)** Wind vs Water is the tritan problem (teal ≈ blue); **(d)** Light vs Physical can't be solved by colour alone if Physical stays white.

## 3a. Proposed palette (identity kept, separation maximised)

Search: coordinate descent inside a per-element hue/saturation/value box (Fire stays red, Lightning yellow-amber,
Earth brown/ochre, Wind green-teal, Water blue, Reality indigo, Void violet, Light warm white, Darkness near-black
purple), minimising shortfalls below ΔE 25 across all four conditions, with L* ≥ 40 for everything but Darkness so
numbers read on dark stone (`UI_DeckbuilderReuse.md` §1). Light/Physical and Ice/Physical were weighted low: they are
whites by ruling and the glyph (3b) carries them. Result, hand-rounded:

| Element | sRGB | `FLinearColor` | L* | Identity note |
| --- | --- | --- | --- | --- |
| Fire | `#B02A18` | (0.434, 0.023, 0.009) | 40 | deep red; the *darkest* warm colour |
| Lightning | `#FFCC00` | (1.000, 0.604, 0.000) | 84 | bright amber-yellow; the *lightest* warm colour (draft had orange) |
| Earth | `#D0782E` | (0.631, 0.188, 0.027) | 59 | ochre/terracotta, mid (code had pure yellow) |
| Wind | `#2EAE92` | (0.027, 0.423, 0.287) | 64 | teal-green (deckbuilder's `#6FC2B0` lineage) |
| Water | `#2A78E0` | (0.023, 0.188, 0.745) | 51 | mid blue |
| Ice (sub-element) | `#C4F1FF` | (0.552, 0.880, 1.000) | 93 | pale cyan-white; **new** — Ice has no colour today |
| Reality ("Unknown") | `#5A2ED4` | (0.102, 0.027, 0.658) | 36 | indigo, dark (D-024 kept) |
| Void | `#C48CFF` | (0.552, 0.262, 1.000) | 68 | violet, *light* (D-024 kept; the lightness gap to Reality is the point) |
| Light | `#FFFBD8` | (1.000, 0.965, 0.687) | 98 | warm cream, off-white |
| Darkness / BD | `#321E36` | (0.032, 0.013, 0.037) | 15 | near-black purple; **always drawn with a pale crystal outline** when used as text (the deckbuilder's `KeylineFor` rule: light ink where the shade is near-black) |
| Physical | `#FFFFFF` | (1, 1, 1) | 100 | ruling (Q-111) |
| Generic/None | keep `#CB9B7C`-ish brown; never a damage-number colour | | | |

**Min ΔE2000 over all four conditions, proposed palette** (full 11×11 tables per condition: `python3 cvd_sim.py --full`):

| pair | min ΔE | where | | pair | min ΔE | where |
| --- | --- | --- | --- | --- | --- | --- |
| Light / Physical | **4.5** | tritan | | Fire / Earth | 17.9 | deutan |
| Ice / Physical | **8.3** | protan | | Lightning / Earth | 19.2 | deutan |
| Water / Void | 10.8 | protan | | Lightning / Void | 19.5 | tritan |
| Water / Reality | 12.3 | deutan | | every other pair | ≥ 20 | |
| Wind / Water | 13.9 | tritan | | | | |

Among the **nine elements** no pair drops below 10 under any deficiency (code: four pairs did, two of them ≤ 4;
draft: seven did). Normal vision: smallest element pair is Light/Lightning 23, Water/Reality 24. The remaining
risky pairs are the structural ones (two blues + violet under protan/deutan; teal vs blue under tritan) and are what
the glyph fallback is for. Caveats: VFX emissive uses the same constants; Fire at L* 40 is darker than today's
`#FF0000` glow, so VFX may want a brighter emissive multiplier rather than a different hue (`EMISSIVE_INTENSITY_*`
in `HybridSpellColors.cpp`). Check the colours on the real stone background in the next draft (B-111 v3 / B-105).

**Interaction with Q-111 "resisted = dimmer":** dimming moves a number *down the lightness ladder*, which is the
very channel that separates Lightning → Earth → Fire and Void → Water → Reality for protan/deutan viewers. A
resisted Lightning hit at 70 % brightness lands on Earth. Recommend showing resistance as **size + alpha + glow**
(weak: bigger + glow; resisted: smaller + 60 % alpha, same hue; immune: grey + small), not a darker shade. Not a ruling change, just how "dimmer" is implemented.

## 3b. Non-colour fallback and a setting

Principle: WCAG 1.4.1 *Use of Color* and Game Accessibility Guidelines (basic tier) — "no essential information conveyed by colour alone". Proven examples: Honkai: Star Rail and Persona 5 Royal put the element/affinity **icon** beside every number, bar and weakness marker (praised for at-a-glance reads); Octopath's shield break is a shape change, not a colour; Pokémon type icons.

1. **Element gem glyph on damage numbers**, leading edge, ~0.8× cap height: the kit's `element_<name>` badges the v2 mockup already uses on the party card (`combat_hud_v2.py:43`, `GEM`), Reality drawn as the indigo crystal (`drawn_gem`), Physical = no gem (so Light vs Physical is "gem vs no gem"; Ice = Water gem with a frost tick, or its own gem later). **Default on** for everyone (reference-backed; it also carries non-CVD readability on busy fights); Crown vetoes.
2. **Status-buildup bars:** gem at the bar head (the party card already has one; the enemy plate needs it) plus the crack/shatter already ruled (Q-111) — shape, not colour, says "about to break". In colour-blind mode the bar also gets a 1-px **pale crystal keyline** so a Darkness/Reality fill reads on the trough.
3. **BD overload band:** the pulse/glow at the top step D-024 floated, promoted from "suggestion" to the fallback.
4. **Weather / portrait glow / pie-menu tint:** weather is already named text in the HUD plan — keep the name next to the icon; the portrait's element is the gem beside the name (mockup), so the glow is decorative.
5. **Setting:** one toggle, *Settings → Accessibility → "Element markers: always / numbers only"* (`UI_ScreenInventory.md` row 1.6 already lists "colour-blind element markers"), stored in a per-player settings save struct (D-059 explicit struct; Lyra's `ULyraSettingsShared` pattern, `docs/analysis/BestPractice_2026-10.md:155`). No daltonised alternative palettes: the proposed palette plus glyphs covers all three types, and alternate palettes would fork every VFX/material.

## 3c. Which uses need the fallback most

1. Damage numbers (short-lived, small, moving: no time to compare hues; Light vs Physical is ΔE 0 today). Gem glyph first.
2. Status-buildup bars on enemy plates (a "which status is close to breaking" read across 3+ enemies). Gem at bar head.
3. Reality/Void/Darkness anywhere (three purples; weak even in normal vision at 13–15 today). Lightness ladder + glyph.
4. BD EP bar in the absorbed element's colour: the *current* absorbed element is a tactical read. Gem on the EP bar head when BD.
5. Pie-menu element tints and portrait glows: decorative; the label text already carries the element. No fallback needed.

## 4. Files to change (≤3 per cluster)

- **C1 — code palette:** `Source/world_of_refraction/Public/Infusion/ElementColors.h` (new values, add `Ice`, `Physical`, a `GetTextOutlineForElement` or "needs outline" flag for Darkness), `Public/Skills/Effects/SkillEffectDisplayNames.h` (drop its own table, forward to `ElementColors`), `Private/Infusion/ElementColorDebugComponent.cpp` (add `GetElementPaletteString()`: every element's hex + the three simulated hexes, so the palette is inspectable without PIE). **Best done inside B-271's one-element-table job** (it already lists "colour" as a column); if B-271 lands first, C1 is one table edit.
- **C2 — mockups + design doc:** `Tools/ui_drafts/combat_hud_v1.py` (`ELEMENT`), `Tools/ui_drafts/combat_hud_v2.py` (remove the v1 overrides; `DAMAGE_GRADIENT` → size/alpha per Q-111), `docs/Design/UI_DeckbuilderReuse.md` §1 (element palette row, cite this check). Redraw `combat/hud/v3/` for Crown.
- **C3 — glyph fallback (with B-105's damage-number and bar widgets, which don't exist yet):** the damage-number widget `.h/.cpp` (gem slot + size/alpha rule), the status-bar widget (gem at head + keyline), `docs/Architecture/UISystem.md` changelog.
- **C4 — setting:** new `Public/Settings/WoRPlayerSettings.h` + `.cpp` (explicit save struct, `bElementMarkersAlways`), the Accessibility tab of the Settings screen (B-10x, not built yet); `docs/Design/UI_ScreenInventory.md` row 1.6.
- **Docs:** a `Q-<job>` entry in `docs/DesignDecisions.md` recording the palette and "markers default on" as reference-backed defaults (Star Rail / Persona 5 icons; Genshin's colour-only numbers are the cautionary case), and a `D-024` note that Reality indigo / Void violet are kept but Void is the *light* one. Deckbuilder and this game now differ on Reality (retired there) and on exact hexes: flag, don't "fix".
