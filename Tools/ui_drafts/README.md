# UI drafts (mockup tool)

These scripts draw flat 1280×720 screen drafts, a **plain** one and an **annotated** one, before a screen is built in UMG/CommonUI. The flow is: draft → Crown approves → build (D-024, D-025, `docs/Design/UI_DeckbuilderReuse.md` §5).

The tool is ported from `wor-deckbuilder/tools/ui_drafts/`. Only its shared helpers came across; none of the deckbuilder's screens did.

## Setup (once per machine)

1. **Point `WOR_UI_KIT` at the licensed UI kit library.** In the deckbuilder clone it's `wor.game/assets/ui/library`. The kit is never copied into this repo.

   ```powershell
   [Environment]::SetEnvironmentVariable("WOR_UI_KIT", "C:\Users\<you>\Documents\github\wor-deckbuilder\wor.game\assets\ui\library", "User")
   ```

   Reopen the shell afterwards. Every script stops with a clear message if `WOR_UI_KIT` is unset or wrong.
2. **Install Python 3.9 or later, with Pillow and NumPy:** `pip install pillow numpy`.
3. **Fonts are found automatically.** Spectral SC (titles) and Lato (everything else) are always used, read from the deckbuilder's `wor.game/assets/fonts`, next to the library. If they aren't there, Windows fonts stand in, and the script prints a note saying so.
   - **How this differs from the deckbuilder:** there, `GAME_FONTS` was opt-in, so old drafts would rebuild unchanged. This repo has no old drafts, so the game fonts are always on.

## Draw a draft

From the repo root, one script per screen version:

```
python Tools/ui_drafts/test_draft.py
```

- **Output:** `docs/Design/screen_mockups/<area>/<screen>/vN/<name>_plain.png` + `<name>_annotated.png`. The PNGs are committed and stored in LFS, so Crown can review them on the phone.
- **Versions are frozen.** A script refuses to write into a `vN/` that already exists. A revision goes in `vN+1/`. To redraw an existing version on purpose, run with `UI_DRAFTS_RESTORE=1`.
  - **How this differs from the deckbuilder:** its `FROZEN` list named each delivered version by hand. Here, any non-empty `vN/` counts as delivered. Both images are rendered before anything is written, so a crash can't freeze a half-written version.
- **The output is deterministic,** because randomness is seeded and the art is fixed. A draft therefore rebuilds pixel-identical.
- **Draw somewhere else:** set `UI_DRAFTS_OUT=<folder>` to draw into another folder, for scratch work.

## Files

| File | What it is |
|---|---|
| `kit.py` | Shared pieces. It covers the kit loader (`piece(kind, name)`), fonts, tint/steel recolour, five-slice stretch, background, vignette, glow, motes, text with shadow, the annotation legend, frozen version folders and `save_draft` |
| `test_draft.py` | The B-100 smoke test. It touches every kit kind the HUD drafts use. Output: `tool/test_draft/v1/` |

## Kit gaps found so far

- There is no `badges/element_reality` in the library. It has badges for 8 of this game's 9 elements (`ESpellElement`: Fire … Void, plus Reality). B-101 needs a stand-in, or new art, for Reality.
