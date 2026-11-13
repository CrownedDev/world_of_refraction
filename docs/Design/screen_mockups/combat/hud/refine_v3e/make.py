"""Battle-HUD mockup refine_v3e: no row labels. Top row = the SOURCE (kind and gear merged, one page per source);
the row under it = that source's schools; then the cards. Target/infusion images stay in refine_v3b.
Reuses refine_v3b's helpers (loading it redraws v3b and v2). Run: python make.py"""
import importlib.util
import os

OUT = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("v3b", os.path.join(OUT, "..", "refine_v3b", "make.py"))
v = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(v)
F, DIM, CYAN = v.F, v.DIM, v.CYAN
STEELY = (150, 160, 175)


def sources(d, y, labels, lit):
    """Source row: the lit tab carries the full name, the others the shortest name that tells them apart."""
    v.tabs(d, 20, y, labels, lit)
    d.text((640, y + 5), "L2 <  %d/%d  > R2" % (lit + 1, len(labels)), font=F(17, True), fill=CYAN)


HINTS = ["L2 / R2  Source", "D-pad left/right  School", "D-pad up/down  More skills", "Y / X / A  Use skill", "R1  Defend      B  Back"]
HINTS_NO_SCHOOL = [HINTS[0]] + HINTS[2:]

# A: Refractor. Two sources; the innate one has three schools.
im, d = v.base()
sources(d, 150, ["Refractions", "Resonate (A)"], 0)
v.tabs(d, 20, 190, ["Destruction", "Enhancement", "Impairment"], 0)
v.skill_panel(d, "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", v.SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", v.SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", v.SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
], pager="Skills %s")
v.hints(d, HINTS)
im.save(os.path.join(OUT, "a_refractor_sources.png"))

# B: Resonator. Each adapter is its own source page; turning to it makes it the active adapter (free).
im, d = v.base("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))
sources(d, 150, ["Abilities", "Storm Ring", "Resonate (A) · Ember Ring", "Tide Ring"], 2)
v.tabs(d, 20, 190, ["Destruction", "Enhancement"], 1)
v.skill_panel(d, "Ember Ring · Enhancement", "1/1", [
    ("Y", "Ember Veil", "0 EP", v.FIRE, "Fire · one ally · 3 turns", "Adds Fire to the ally's attacks."),
    ("X", "Kindle", "0 EP", v.FIRE, "Fire · self · 2 turns", "Raises spell damage."),
], pager="Skills %s")
d.text((40, 456), "Ember Ring is now the active adapter (free).", font=F(18), fill=DIM)
v.hints(d, HINTS)
im.save(os.path.join(OUT, "b_resonator_adapters_as_sources.png"))

# C: Generic with two weapons. Each weapon's abilities are a source page; abilities have no school row.
im, d = v.base("Garrick", "Generic:", (("Sera", .22, "18/84"), ("Bram", .9, "87/97")))
sources(d, 190, ["Abilities · Iron Sword", "Resonate (W)", "War Hammer"], 0)
v.skill_panel(d, "Abilities · Iron Sword", "1/2", [
    ("Y", "Cleave", "4 EP", STEELY, "Slash · 1 hit · one enemy", "Heavy damage."),
    ("X", "Whirl", "8 EP", STEELY, "Slash · 1 hit · all enemies", "Medium damage to the front row."),
    ("A", "Guard Break", "5 EP", STEELY, "Impact · 1 hit · one enemy", "Lowers the target's defence for 2 turns."),
], pager="Skills %s")
v.hints(d, HINTS_NO_SCHOOL)
im.save(os.path.join(OUT, "c_generic_two_weapons.png"))
print("ok")
