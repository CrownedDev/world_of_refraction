"""Battle-HUD mockup refine_v3c: the school row is a labelled line like the gear row.
Only the two skill-menu images change; the target/infusion images stay in refine_v3b.
Reuses refine_v3b's helpers (importing it redraws v3b and v2). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
import importlib.util  # noqa: E402  (every draft script is called make.py, so load v3b under its own name)

_spec = importlib.util.spec_from_file_location("v3b", os.path.join(OUT, "..", "refine_v3b", "make.py"))
v3b = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(v3b)
base, tabs, skill_panel, hints, F, DIM, SHADE, FIRE = (
    v3b.base, v3b.tabs, v3b.skill_panel, v3b.hints, v3b.F, v3b.DIM, v3b.SHADE, v3b.FIRE)

# A: Refractor. Kind tabs (ride on L2/R2 past the last page), then the labelled school row (D-pad up/down).
im, d = base()
tabs(d, 20, 150, ["Refractions", "Resonate (A)"], 0)
tabs(d, 20, 190, ["Destruction", "Enhancement", "Impairment"], 0, lead="School  D-pad up/down")
skill_panel(d, "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
])
hints(d, ["D-pad up/down  School", "L2 / R2  Page, then next kind", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "a_spell_school_row.png"))

# B: Resonator. Adapter row (D-pad left/right) and school row (D-pad up/down) at once: no shared button.
im, d = base("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))
tabs(d, 20, 110, ["Storm Ring", "Ember Ring", "Tide Ring"], 1, lead="Adapter  D-pad left/right")
tabs(d, 20, 150, ["Abilities", "Resonate (A)"], 1)
tabs(d, 20, 190, ["Destruction", "Enhancement"], 1, lead="School  D-pad up/down")
skill_panel(d, "Resonate (A) · Enhancement", "1/1", [
    ("Y", "Ember Veil", "0 EP", FIRE, "Fire · one ally · 3 turns", "Adds Fire to the ally's attacks."),
    ("X", "Kindle", "0 EP", FIRE, "Fire · self · 2 turns", "Raises spell damage."),
])
d.text((40, 456), "Switching adapter is free: no turn, no EP.", font=F(18), fill=DIM)
hints(d, ["D-pad left/right  Adapter", "D-pad up/down  School", "L2 / R2  Page, then next kind", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "b_gear_and_school_rows.png"))
print("ok")
