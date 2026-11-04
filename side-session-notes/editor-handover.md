# Editor worker hand-over (2026-11-04)

## 1. Simplicity audit (done)
- **Branch:** `chore/simplicity-audit`, worktree `D:\github\wor-audit`, pushed, not merged into dev.
- **File:** `docs/analysis/SimplicityAudit.md`. Round 1 is §1-11, round 2 is §12-25, round 3 is §26-36, and §37 covers skills on gear. Session log entries are in `docs/sessions/2026-11-03.md`.
- **Status:** Crown has decided rounds 1-3 (D-120 to D-142 and later). Cosmetics §35 (skins and outfits) is parked. Don't re-raise decided items.

## 2. MetaHuman test, Eric Citrine (paused)
- **Branch and folder:** `chore/eric-metahuman-test` in `D:\github\world_of_refraction`. That folder has Crown's uncommitted files; don't use it for branch work.
- **Asset:** `Content/Characters/MetaHumans/EricCitrine/MHC_EricCitrine.uasset`, saved. `Config/DefaultEngine.ini` has three skin-cache lines from the plugin. Neither is committed. `SourceArt/` is not mine; don't touch it.
- **v1 state:** Trey preset base, slim body (182 cm), green eyes, lighter skin, short black S_Coil hair, no facial hair, default white tee and shorts.
- **Likeness gaps:**
  - Hair: the sheet has longer brown twists with a fringe over the forehead. No installed groom has that fringe, so try a browner colour and the CurlyFade and TwistedBraids grooms.
  - Skin: still slightly darker than #ca9772.
  - Face: realistic base; the sheet is narrow with heavy-lidded eyes.
  - Clothes: MetaHuman only has the default garment. The lab coat, shirt and trousers need a Fab pack or a custom mesh.
- **Pending:**
  - A painterly master material (flat albedo, painted gradient, brush-stroke mask, flat roughness; no toon or outline) tried on skin and clothes, per VisualDirection §1.
  - Realistic vs painterly screenshots to Crown.
  - Commit and push the asset and ini, and write `docs/sessions/editor-2026-11-03.md`.
  - Hand back a 3-line report.
- **Recipes (editor Python):** run `py <script>` in the editor console. Skin and eyes via `commit_skin_settings` / `commit_eyes_settings`, hair via `try_add_slot_selection`. Reopen the asset editor to see wardrobe changes. Scripts are in the session scratchpad (`eric_*.py`).
- **Do not reuse:** `eric_reopen.py` (it closes the asset editor from its own console and crashed the editor at 01:09).

## Rules
- **Use unreal-mcp only for `.uasset` work** (save through its AssetTools; targeted saves can false-fail, so save all dirty and check the mtime). It had ECONNREFUSED after the editor was reopened.
- **Never launch Unreal through computer-use** (`open_application`). Focus the existing window with PowerShell user32 calls by PID or handle.
- **Never force-kill the editor**; save first, and ask Crown before closing it.
- **Never close an asset editor from a script run in its own console.**
- **Don't resave the 3 broken LFS pointer assets** (BP_CombatOrchestrator, WBP_CharacterPanel, WBP_TurnOrderSlot).
- **Stay off the protected branches:** don't touch the main branch, don't merge into dev, don't stash, reset or switch branches. Commit only your own files.
- **Keep replies short.** Crown signs into Epic cloud services themself.
