"""Battle-HUD mockup refine_v3d: one D-pad model. Up/down moves focus between rows
(gear, kind, school, cards); left/right changes the focused row (or the page on the cards).
Y / X / A always fire the three cards. Target/infusion images stay in refine_v3b.
Reuses refine_v3b's helpers (loading it redraws v3b and v2). Run: python make.py"""
import importlib.util
import os

OUT = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("v3b", os.path.join(OUT, "..", "refine_v3b", "make.py"))
v = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(v)
F, DIM, CYAN, STEEL = v.F, v.DIM, v.CYAN, v.STEEL


def row(d, y, label, labels, lit, focused):
    """A labelled tab row. The focused row gets a cyan frame and < > at its ends."""
    d.text((34, y + 5), label, font=F(17, True), fill=CYAN if focused else DIM)
    v.tabs(d, 130, y, labels, lit)
    if focused:
        d.rectangle((20, y - 4, 600, y + 38), outline=CYAN, width=2)
        end = 130 + sum(int(d.textlength(t, font=F(18, True))) + 34 for t in labels)
        d.text((112, y + 3), "<", font=F(20, True), fill=CYAN)
        d.text((end + 2, y + 3), ">", font=F(20, True), fill=CYAN)


HINTS = ["D-pad up/down  Row", "D-pad left/right  Change", "Y / X / A  Use skill", "R1  Defend      B  Back"]

# A: Refractor, focus on the School row: left/right changes school.
im, d = v.base()
row(d, 142, "Kind", ["Refractions", "Resonate (A)"], 0, False)
row(d, 186, "School", ["Destruction", "Enhancement", "Impairment"], 0, True)
v.skill_panel(d, "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", v.SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", v.SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", v.SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
], pager="Page %s")
v.hints(d, HINTS)
im.save(os.path.join(OUT, "a_focus_school_row.png"))

# B: Resonator, focus on the Adapter row: left/right switches adapter (free).
im, d = v.base("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))
row(d, 98, "Adapter", ["Storm Ring", "Ember Ring", "Tide Ring"], 1, True)
row(d, 142, "Kind", ["Abilities", "Resonate (A)"], 1, False)
row(d, 186, "School", ["Destruction", "Enhancement"], 1, False)
v.skill_panel(d, "Resonate (A) · Enhancement", "1/1", [
    ("Y", "Ember Veil", "0 EP", v.FIRE, "Fire · one ally · 3 turns", "Adds Fire to the ally's attacks."),
    ("X", "Kindle", "0 EP", v.FIRE, "Fire · self · 2 turns", "Raises spell damage."),
], pager="Page %s")
d.text((40, 456), "Switching adapter is free: no turn, no EP.", font=F(18), fill=DIM)
v.hints(d, HINTS)
im.save(os.path.join(OUT, "b_focus_adapter_row.png"))

# C: focus on the cards (where the menu opens): left/right turns the page.
im, d = v.base()
row(d, 142, "Kind", ["Refractions", "Resonate (A)"], 0, False)
row(d, 186, "School", ["Destruction", "Enhancement", "Impairment"], 0, False)
v.skill_panel(d, "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", v.SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", v.SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", v.SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
], pager="<  Page %s  >", edge=CYAN)
v.hints(d, HINTS)
im.save(os.path.join(OUT, "c_focus_cards_page.png"))
print("ok")
