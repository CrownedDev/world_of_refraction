"""Party (UIScreens.md #7): the three members, who plays each (You, a friend, an AI seat and its level), where each
starts on the 3 x 3 grid (Front / Middle / Back Row), and what happens when your fighters fall
(Let AI finish / Take over). Words from PartyControlWidget.cpp. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

PARTY = [("Sera", "Darkness Refractor", "Darkness", "C", "You (leader)", (2, 1)),
         ("Garrick", "Generic", "Generic", "B", "", (0, 1)),
         ("Bram", "Resonator", "Fire", "C", "", (1, 0))]
ROWS = ["Front Row", "Middle Row", "Back Row"]


def grid(d, x, y, focus_cell=None, cell=64):
    """The party's side of the battle grid, seen from above: enemies are to the right."""
    text(d, (x, y - 26), "Starting places", 16, True, DIM)
    text(d, (x + 3 * cell + 40, y + cell * 1.5), "enemies  ►", 14, fill=OFF, anchor="lm")
    for c in range(3):  # columns drawn back to front, left to right
        row_name = ROWS[2 - c]
        text(d, (x + c * cell + cell / 2, y + 3 * cell + 8), row_name.split()[0], 13, fill=DIM, anchor="ma")
        for r in range(3):
            box = (x + c * cell, y + r * cell, x + (c + 1) * cell - 6, y + (r + 1) * cell - 6)
            cut(d, box, PLATE, CYAN if focus_cell == (2 - c, r) else TRACK, 6)
    for n, _, el, _, _, (row, col) in PARTY:
        cx, cy = x + (2 - row) * cell + (cell - 6) / 2, y + col * cell + (cell - 6) / 2
        d.regular_polygon((cx, cy, 20), 6, fill=ELEMENT[el])
        text(d, (cx, cy + 1), n[0], 18, True, BG, anchor="mm")


im, d = canvas()
title(d, "Party", "Up to 3 fight together")
for i, (n, cls, el, t, who, _) in enumerate(PARTY):
    y = 130 + i * 126
    f = i == 1
    cut(d, (48, y, 700, y + 114), TEAL if f else PLATE, CYAN if f else STEEL, 10)
    icon(d, (62, y + 14, 146, y + 98), ELEMENT[el])
    text(d, (164, y + 12), n, 24, True)
    tier_badge(d, 164 + int(d.textlength(n, font=F(24, True))) + 20, y + 27, t)
    text(d, (164, y + 46), cls, 15, fill=TXT if f else DIM)
    text(d, (164, y + 76), "Played by", 14, fill=TXT if f else DIM)
    text(d, (250, y + 74), ("‹  %s  ›" % who) if f else who, 16, True, CYAN if (f or i == 0) else TXT)
    text(d, (684, y + 16), "Starts: " + ROWS[PARTY[i][5][0]], 14, fill=TXT if f else DIM, anchor="ra")
    text(d, (684, y + 76), ["", "Field skill: Dash", "Field skill: Grapple"][i] or "Field skill: Glide", 14, fill=TXT if f else DIM, anchor="ra")
cut(d, (48, 512, 700, 562), PLATE, STEEL, 8)
text(d, (66, 537), "When your fighters fall", 17, True, anchor="lm")
text(d, (684, 537), "Let AI finish", 17, True, DIM, anchor="rm")
cut(d, (48, 572, 700, 622), PLATE, STEEL, 8)
text(d, (66, 597), "Invite", 17, True, anchor="lm")
text(d, (684, 597), "no session", 15, fill=OFF, anchor="rm")
panel(d, (730, 130, 1232, 622), focus=True)
grid(d, 780, 196, focus_cell=(0, 1))
d.line((750, 440, 1212, 440), fill=STEEL)
text(d, (750, 454), "Garrick", 18, True)
for i, (lab, val) in enumerate([("Guard", "Weapon guard"), ("If a friend joins", "they can take this seat")]):
    text(d, (750, 488 + i * 28), lab, 15, fill=DIM)
    text(d, (1212, 488 + i * 28), val, 15, True, anchor="ra")
wrap(d, (750, 580), "The leader can't be given to the AI.", 460, 14, DIM)
footer(d, [("A", "Change"), ("X", "Move on grid"), ("Y", "Swap order"), ("B", "Back")])
save(im, OUT, "1_party.png")
print("ok")
