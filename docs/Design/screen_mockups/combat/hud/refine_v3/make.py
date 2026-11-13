"""Battle-HUD mockup refine_v3: skill cards with kind tabs, gear tabs, infusion on the target picker.
Reuses refine_v2's helpers (importing it redraws v2). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "refine_v2"))
from make import base, cut, bar, F, ENEMIES, PANEL, PLATE, STEEL, TEAL, CYAN, TXT, DIM, ROSE, EP  # noqa: E402

GLYPH = {"Y": (240, 200, 70), "X": (90, 140, 240), "A": (110, 200, 110)}
SHADE, FIRE, STORM = (167, 155, 194), (229, 72, 58), (242, 162, 58)


def tabs(d, x, y, labels, lit, h=34, lead=None):
    """A row of tabs; the lit one is cyan. Returns the x after the row."""
    if lead:
        d.text((x, y + 5), lead, font=F(17, True), fill=CYAN)
        x += int(d.textlength(lead, font=F(17, True))) + 10
    for i, lab in enumerate(labels):
        w = int(d.textlength(lab, font=F(18, True))) + 28
        cut(d, (x, y, x + w, y + h), TEAL if i == lit else PLATE, CYAN if i == lit else STEEL, 8)
        d.text((x + 16, y + 4), lab, font=F(18, True), fill=TXT if i == lit else DIM)
        x += w + 6
    return x


def cards(d, rows, top):
    for i, (g, name, cost, col, l1, l2) in enumerate(rows):
        y = top + i * 92
        cut(d, (36, y, 584, y + 84), PLATE, STEEL, 10)
        d.ellipse((48, y + 26, 80, y + 58), outline=GLYPH[g], width=3)
        d.text((58, y + 30), g, font=F(17, True), fill=GLYPH[g])
        d.ellipse((94, y + 12, 112, y + 30), fill=col)
        d.text((120, y + 4), name, font=F(24, True), fill=TXT)
        d.text((500, y + 6), cost, font=F(20, True), fill=EP)
        d.text((94, y + 36), l1, font=F(18), fill=DIM)
        d.text((94, y + 58), l2, font=F(18), fill=TXT)


def hints(d, lines):
    for i, t in enumerate(lines):  # corner hints, clickable; a setting hides them
        d.text((960, 704 - 26 * (len(lines) - i)), t, font=F(18), fill=DIM)


# A: skills. Kind tabs above the panel; L2/R2 walk pages, then tabs.
im, d = base()
tabs(d, 20, 190, ["Refractions", "Resonate (A)"], 0)
cut(d, (20, 230, 600, 536), PANEL, STEEL)
d.text((40, 236), "Refractions", font=F(20, True), fill=DIM)
d.text((440, 236), "< L2   1/2   R2 >", font=F(18, True), fill=CYAN)
cards(d, [
    ("Y", "Shade Lance", "6 EP", SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Night Veil", "4 EP", SHADE, "Darkness · self · 3 turns", "Raises evasion. Consumes 1 Shade stack."),
    ("A", "Umbral Rain", "12 EP", SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
], 266)
hints(d, ["L2 / R2  Page, then next tab", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "a_skills_tabs.png"))

# B: gear tabs (Switch Weapon / Switch Adapter). One gear row, above the kind tabs; D-pad left/right flips it, free.
im, d = base("Bram", "Resonator:")
tabs(d, 20, 150, ["Storm Ring", "Ember Ring", "Tide Ring"], 1, lead="Adapter  < D-pad >")
tabs(d, 20, 190, ["Abilities", "Resonate (A)"], 1)
cut(d, (20, 230, 600, 536), PANEL, STEEL)
d.text((40, 236), "Resonate (A) · Ember Ring", font=F(20, True), fill=DIM)
d.text((440, 236), "< L2   1/1   R2 >", font=F(18, True), fill=CYAN)
cards(d, [
    ("Y", "Flame Lance", "6 EP", FIRE, "Fire · 1 hit · one enemy", "Heavy damage. Builds Burn."),
    ("X", "Ember Veil", "4 EP", FIRE, "Fire · one ally · 3 turns", "Adds Fire to the ally's attacks."),
], 266)
d.text((40, 470), "Switching is free: no turn, no EP.", font=F(18), fill=DIM)
hints(d, ["D-pad < >  Switch adapter", "L2 / R2  Page, then next tab", "R1  Defend      B  Back"])
im.save(os.path.join(OUT, "b_gear_tabs.png"))


def target(hold):
    im, d = base()
    x, y, r, name = ENEMIES[1]
    cut(d, (x - 170, y - r - 138, x + 170, y - r - 44), PANEL, CYAN)
    d.text((x - 152, y - r - 134), name, font=F(26, True), fill=TXT)
    d.text((x + 70, y - r - 128), "412/600", font=F(20, True), fill=TXT)
    d.text((x - 152, y - r - 98), "Rank 2 · Tier C", font=F(18), fill=DIM)
    bar(d, x - 152, y - r - 68, 304, .68, ROSE, 12)
    d.polygon([(x, y - r - 8), (x - 18, y - r - 36), (x + 18, y - r - 36)], fill=CYAN)
    if not hold:
        d.text((ENEMIES[0][0] - 110, 370), "<", font=F(54, True), fill=CYAN)
        d.text((ENEMIES[2][0] + 80, 380), ">", font=F(54, True), fill=CYAN)
    # action banner + infusion strip, above the active card
    cut(d, (20, 408, 520, 536), PANEL, CYAN)
    d.text((36, 414), "Shade Lance  ·  choose a target", font=F(22, True), fill=TXT)
    d.text((36, 452), "Infusion", font=F(18, True), fill=DIM)
    if hold:
        tabs(d, 120, 448, ["Innate", "Weapon Crystal", "Adapter"], 1, h=32)
    else:
        d.text((120, 452), "Innate", font=F(18, True), fill=TXT)
    d.text((36, 492), "L1", font=F(17, True), fill=CYAN)
    for i in range(3):  # level pips L0 L1 L2, current lit
        lit = i == 1
        cut(d, (70 + i * 62, 488, 124 + i * 62, 520), TEAL if lit else PLATE, CYAN if lit else STEEL, 6)
        d.text((84 + i * 62, 491), "L%d" % i, font=F(17, True), fill=TXT if lit else DIM)
    d.text((270, 492), "x1.15 damage · 6 EP", font=F(18, True), fill=EP)
    if hold:
        hints(d, ["Holding L1:  < >  Source", "Release L1 to go back to targets"])
    else:
        hints(d, ["< >  Target      A  Confirm", "L1  Tap: level   Hold + < >: source", "R1  Defend      B  Back"])
    return im


target(False).save(os.path.join(OUT, "c_target_infusion.png"))
target(True).save(os.path.join(OUT, "d_target_infusion_hold_l1.png"))
print("ok")
