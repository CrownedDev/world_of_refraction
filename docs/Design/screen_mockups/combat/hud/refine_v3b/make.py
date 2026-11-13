"""Battle-HUD mockup refine_v3b: kind + school tabs, gear tabs, infusion strip (fixed source vs source choice).
Reuses refine_v2's helpers (importing it redraws v2). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "refine_v2"))
from make import base, cut, bar, F, ENEMIES, PANEL, PLATE, STEEL, TEAL, CYAN, TXT, DIM, ROSE, EP  # noqa: E402

GLYPH = {"Y": (240, 200, 70), "X": (90, 140, 240), "A": (110, 200, 110)}
SHADE, FIRE = (167, 155, 194), (229, 72, 58)
SERA_ALLIES = (("Garrick", .8, "67/84"), ("Bram", .9, "87/97"))


def tabs(d, x, y, labels, lit, h=34, lead=None):
    if lead:
        d.text((x, y + 5), lead, font=F(17, True), fill=CYAN)
        x += int(d.textlength(lead, font=F(17, True))) + 10
    for i, lab in enumerate(labels):
        w = int(d.textlength(lab, font=F(18, True))) + 28
        cut(d, (x, y, x + w, y + h), TEAL if i == lit else PLATE, CYAN if i == lit else STEEL, 8)
        d.text((x + 16, y + 4), lab, font=F(18, True), fill=TXT if i == lit else DIM)
        x += w + 6


def skill_panel(d, title, page, rows, pager="< L2   %s   R2 >", edge=STEEL):
    cut(d, (20, 230, 600, 536), PANEL, edge)
    d.text((40, 236), title, font=F(20, True), fill=DIM)
    d.text((440, 236), pager % page, font=F(18, True), fill=CYAN)
    for i, (g, name, cost, col, l1, l2) in enumerate(rows):
        y = 266 + i * 88
        cut(d, (36, y, 584, y + 82), PLATE, STEEL, 10)
        d.ellipse((48, y + 25, 80, y + 57), outline=GLYPH[g], width=3)
        d.text((58, y + 29), g, font=F(17, True), fill=GLYPH[g])
        d.ellipse((94, y + 12, 112, y + 30), fill=col)
        d.text((120, y + 4), name, font=F(24, True), fill=TXT)
        d.text((500, y + 6), cost, font=F(20, True), fill=EP)
        d.text((94, y + 34), l1, font=F(18), fill=DIM)
        d.text((94, y + 56), l2, font=F(18), fill=TXT)


def hints(d, lines):
    for i, t in enumerate(lines):  # corner hints, clickable; a setting hides them
        d.text((930, 704 - 26 * (len(lines) - i)), t, font=F(18), fill=DIM)


# A: spells. Kind tabs, then a school tab per school that has a spell equipped.
im, d = base()
tabs(d, 20, 150, ["Refractions", "Resonate (A)"], 0)
tabs(d, 20, 190, ["Destruction", "Enhancement", "Impairment"], 0)
skill_panel(d, "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
])
hints(d, ["D-pad up/down  Next / previous tab", "L2 / R2  Page", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "a_spell_school_tabs.png"))

# B: gear row on top (D-pad left/right, free), then kind tabs, then school tabs.
im, d = base("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))
tabs(d, 20, 110, ["Storm Ring", "Ember Ring", "Tide Ring"], 1, lead="Adapter  < D-pad >")
tabs(d, 20, 150, ["Abilities", "Resonate (A)"], 1)
tabs(d, 20, 190, ["Destruction", "Enhancement"], 1)
skill_panel(d, "Resonate (A) · Enhancement", "1/1", [
    ("Y", "Ember Veil", "0 EP", FIRE, "Fire · one ally · 3 turns", "Adds Fire to the ally's attacks."),
    ("X", "Kindle", "0 EP", FIRE, "Fire · self · 2 turns", "Raises spell damage."),
])
d.text((40, 456), "Switching adapter is free: no turn, no EP.", font=F(18), fill=DIM)
hints(d, ["D-pad left/right  Switch adapter", "D-pad up/down  Next / previous tab", "L2 / R2  Page", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "b_gear_tabs.png"))


WARN, KILL = (242, 162, 58), (255, 70, 70)


def target(im, d, action, sources, lit, hold, level, readout, warn=None, warn_col=WARN):
    x, y, r, name = ENEMIES[1]
    cut(d, (x - 170, y - r - 138, x + 170, y - r - 44), PANEL, CYAN)
    d.text((x - 152, y - r - 134), name, font=F(26, True), fill=TXT)
    d.text((x + 70, y - r - 128), "412/600", font=F(20, True), fill=TXT)
    d.text((x - 152, y - r - 98), "Rank 2 · Tier C", font=F(18), fill=DIM)
    bar(d, x - 152, y - r - 68, 304, .68, ROSE, 12)
    d.polygon([(x, y - r - 8), (x - 18, y - r - 36), (x + 18, y - r - 36)], fill=CYAN)
    cut(d, (20, 380, 640, 536), PANEL, CYAN)
    d.text((36, 386), "%s  ·  choose a target" % action, font=F(22, True), fill=TXT)
    d.text((36, 424), "Infusion", font=F(18, True), fill=DIM)
    if hold:
        tabs(d, 120, 420, sources, lit, h=32)
    else:  # fixed source: a plain label, no tabs
        d.text((120, 424), sources[lit], font=F(18, True), fill=TXT)
    for i, lab in enumerate(["—", "I", "II"]):  # level pips, current lit (names not final)
        on = i == level
        cut(d, (36 + i * 56, 460, 84 + i * 56, 492), TEAL if on else PLATE, CYAN if on else STEEL, 6)
        w = d.textlength(lab, font=F(17, True))
        d.text((60 + i * 56 - w / 2, 463), lab, font=F(17, True), fill=TXT if on else DIM)
    d.text((220, 464), readout, font=F(18, True), fill=EP)
    if warn:  # level II only: HP backlash or crystal wear; red if it would kill
        d.text((36, 502), warn, font=F(18, True), fill=warn_col)
    return im


# C: a spell is origin-bound: fixed source label, L1 only steps the level. Level II shows the HP backlash.
im, d = base()
target(im, d, "Shade Lance", ["Innate"], 0, False, 2, "x1.30 damage · size x2.0 · 12 EP", "Level II backlash:  -11 HP")
hints(d, ["< >  Target      A  Confirm", "L1  Infusion level", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "c_target_spell_fixed_source.png"))

# D: an attack or ability: None | Raw | whatever the character has. Crystal sources cost no EP but wear the crystal.
im, d = base("Garrick", "Generic:", (("Sera", .22, "18/84"), ("Bram", .9, "87/97")))
target(im, d, "Cleave", ["None", "Raw", "Weapon Crystal", "Primary Adapter"], 2, True, 2,
       "Fire · x1.30 damage · 0 EP", "Level II:  crystal wear, may break")
hints(d, ["Holding L1:  < >  Source", "Release L1 to go back to targets", "Tap L1  Infusion level"])
im.save(os.path.join(OUT, "d_target_ability_hold_l1.png"))

# E: the one spell with a choice: a Broken Darkness / Reality character's evolution spell (Evolution | Innate).
im, d = base()
target(im, d, "Eclipse", ["Evolution", "Innate"], 0, True, 2, "x1.30 damage · size x2.0 · 16 EP",
       "Level II backlash:  -24 HP  ·  this would kill you", KILL)
hints(d, ["Holding L1:  < >  Source", "Release L1 to go back to targets", "Tap L1  Infusion level"])
im.save(os.path.join(OUT, "e_target_evolution_spell_lethal.png"))
print("ok")
