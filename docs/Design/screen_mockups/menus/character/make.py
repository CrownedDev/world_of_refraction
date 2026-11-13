"""Character sheet (UIScreens.md #5): pillars, the eleven substats grouped under their pillar (Rules CL-39), Max
Actions with action-point progress, resistance by damage type (D-168: All, Physical, Spell, each element), curses.
Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

im, d = canvas()
x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], 0, lead="LT")
text(d, (x + 4, 37), "RT", 16, True, CYAN)
# identity
panel(d, (48, 84, 400, 654))
icon(d, (68, 104, 188, 224), ELEMENT["Darkness"])
text(d, (204, 104), "Sera", 30, True)
tier_badge(d, 204 + int(d.textlength("Sera", font=F(30, True))) + 22, 123, "C")
text(d, (204, 146), "Darkness Refractor", 16, fill=DIM)
text(d, (204, 172), "Combat Tier C", 15, fill=DIM)
for i, (lab, val, frac, col) in enumerate([("HP", "84 / 84", 1, ROSE), ("EP", "40 / 40", 1, EP)]):
    y = 250 + i * 40
    text(d, (68, y), lab, 16, True, DIM)
    bar(d, 108, y + 4, 180, frac, col, 12)
    text(d, (380, y - 2), val, 17, True, anchor="ra")
text(d, (68, 340), "Max Actions", 16, fill=DIM)
for i in range(3):
    d.regular_polygon((250 + i * 26, 350, 10), 4, rotation=45, fill=CYAN)
text(d, (380, 338), "3", 18, True, anchor="ra")
text(d, (68, 370), "next action point", 14, fill=OFF)
bar(d, 220, 376, 100, .75, CYAN, 6)
text(d, (380, 368), "75 / 100", 14, fill=DIM, anchor="ra")
text(d, (68, 400), "Regains per turn", 16, fill=DIM)
text(d, (380, 398), "2", 18, True, anchor="ra")
d.line((68, 440, 380, 440), fill=STEEL)
text(d, (68, 454), "Curses", 16, True, DIM)
d.regular_polygon((80, 496, 10), 4, rotation=45, fill=ELEMENT["Void"])
text(d, (100, 484), "Curse name", 17, True)
wrap(d, (100, 510), "What it does, in its own words.", 270, 14, DIM)
text(d, (68, 620), "Cleared at the Spiritualist", 13, fill=OFF)
# pillars and substats
panel(d, (420, 84, 840, 654), focus=True)
PILLARS = [("Mind", 6, [("Efficiency", "112"), ("Spell Damage", "148"), ("Crit Damage", "150%"), ("Spell Speed", "104")]),
           ("Body", 3, [("Defence", "9%"), ("Action Speed", "98"), ("Raw Damage", "22"), ("Reflex", "101")]),
           ("Spirit", 5, [("Resistance", "12%"), ("Status Multiplier", "1.15"), ("Luck", "6")])]
y = 96
for name, lvl, subs in PILLARS:
    text(d, (440, y), name, 22, True)
    for i in range(7):
        d.rectangle((560 + i * 22, y + 6, 576 + i * 22, y + 22), fill=CYAN if i < lvl else TRACK)
    text(d, (820, y + 2), "%d / 7" % lvl, 17, True, anchor="ra")
    y += 38
    for lab, val in subs:
        text(d, (460, y), lab, 16, fill=DIM)
        text(d, (820, y), val, 16, True, anchor="ra")
        y += 27
    y += 14
text(d, (440, 622), "R3 on a stat: where it comes from", 13, fill=OFF)
# resistance by damage type
panel(d, (860, 84, 1232, 654))
text(d, (880, 96), "Resistance", 20, True)
text(d, (880, 126), "less damage taken, by type", 14, fill=DIM)
RES = [("All", 9, STEEL), ("Physical", 0, STEEL), ("Spell", 5, STEEL), ("Fire", -10, ELEMENT["Fire"]), ("Water", 0, ELEMENT["Water"]),
       ("Earth", 0, ELEMENT["Earth"]), ("Wind", 0, ELEMENT["Wind"]), ("Lightning", 0, ELEMENT["Lightning"]), ("Light", -25, ELEMENT["Light"]),
       ("Darkness", 40, ELEMENT["Darkness"]), ("Void", 0, ELEMENT["Void"]), ("Reality", 0, ELEMENT["Reality"])]
for i, (lab, v, col) in enumerate(RES):
    y = 160 + i * 38 + (12 if i >= 3 else 0)
    d.ellipse((880, y + 3, 894, y + 17), fill=col)
    text(d, (904, y), lab, 16, fill=TXT if v else DIM)
    mid = 1110  # a bar from the middle: right = resists, left = weak
    d.rectangle((1050, y + 6, 1170, y + 14), fill=TRACK)
    d.line((mid, y + 2, mid, y + 18), fill=STEEL)
    if v:
        d.rectangle((min(mid, mid + v * 1.2), y + 6, max(mid, mid + v * 1.2), y + 14), fill=GOOD if v > 0 else ROSE_LOW)
    text(d, (1216, y), ("%+d%%" % v) if v else "0%", 15, True, GOOD if v > 0 else (ROSE_LOW if v < 0 else OFF), anchor="ra")
footer(d, [("R3", "Where it comes from"), ("Y", "Loadout"), ("B", "Back")])
save(im, OUT, "1_character_sheet.png")
print("ok")
