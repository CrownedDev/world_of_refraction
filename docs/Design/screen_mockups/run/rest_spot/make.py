"""Rest spot (UIScreens.md #22-24, #34): the rest menu, Level up, the co-op revive vote, the extract confirm.
Words from RestSpotWidget.cpp / LevelDropWidget.cpp; numbers from RunRewards.h (leave early = 50%),
MiniBossRevival::MAX_REVIVALS = 3. Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

PARTY = [("Sera", "Darkness Refractor", 84, 40), ("Garrick", "Generic", 84, 30), ("Bram", "Resonator", 97, 35)]


def dim(im, k=0.55):
    im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), k)
    return im, ImageDraw.Draw(im)


def menu(d, focus, coop=False, voting=False):
    title(d, "Rest spot", "Cinder Warden's camp · Ember Trial")
    items = [("Level up", "3 to place"), ("Loadout", None), ("Inventory", None), ("Skins", None),
             ("Revive the mini-boss", "1 of 3"), ("Invite and seat friends", None)]
    if coop:
        items.append(("Hand over host", None))
    items.append(("Leave the trial", None))
    for i, (lab, right) in enumerate(items):
        y = 140 + i * 56
        off = voting and lab == "Revive the mini-boss"
        row(d, (48, y, 400, y + 48), lab, focus == i, right=("vote open" if off else right), dim=off, size=19)


def restored(d):
    panel(d, (440, 140, 860, 330))
    text(d, (460, 150), "The party is restored.", 20, True, CYAN)
    for i, (n, cls, hp, ep) in enumerate(PARTY):
        y = 190 + i * 44
        text(d, (460, y), n, 18, True)
        bar(d, 570, y + 4, 140, 1, ROSE, 8)
        bar(d, 570, y + 18, 140, 1, EP, 6)
        text(d, (842, y), "%d/%d" % (hp, hp), 16, True, anchor="ra")


def earned(d, y=350):
    panel(d, (440, y, 860, y + 150))
    text(d, (460, y + 10), "Earned so far", 16, True, DIM)
    x = chip(d, 460, y + 40, "Gold", "2,180", GOLD)
    chip(d, x, y + 40, "Gear Essence", "36", STEEL)
    chip(d, 460, y + 74, "Skill Essence", "12", STEEL)
    text(d, (460, y + 114), "Paid at the end of the run.", 15, fill=DIM)


# ---- 1: the rest menu (solo). Resting is automatic on arrival: the party is restored, then the menu opens.
im, d = canvas("world")
im, d = dim(im, 0.45)
menu(d, 0)
restored(d)
earned(d)
panel(d, (900, 140, 1232, 500), focus=True)
text(d, (920, 150), "Level up", 20, True)
wrap(d, (920, 184), "Place the pillar levels your characters have banked from fights, or buy one with Prisms.", 290, 16)
for i, (n, k) in enumerate([("Sera", 1), ("Garrick", 2), ("Bram", 0)]):
    text(d, (920, 270 + i * 30), n, 16, fill=DIM)
    text(d, (1210, 270 + i * 30), "%d to place" % k if k else "—", 16, True, CYAN if k else OFF, anchor="ra")
wrap(d, (920, 380), "Enemies stay beaten when you rest.", 290, 15, DIM)
footer(d, [("A", "Select"), ("B", "Close")])
save(im, OUT, "1_rest_menu.png")

# ---- 2: Level up: one member at a time (LT / RT), place each banked level on Mind, Body or Spirit; the preview shows
#         what the pillar lifts. Buying a point costs Prisms (GetPointPrice).
im, d = canvas("world")
im, d = dim(im, 0.6)
x = tabs(d, 48, 30, ["Sera  1", "Garrick  2", "Bram"], 1, lead="LT")
text(d, (x + 4, 37), "RT", 16, True, CYAN)
title(d, "Garrick", "Generic · 2 levels to place", y=84)
PILLARS = [("Mind", 2, "Efficiency, Spell Damage, Crit Damage, Spell Speed"),
           ("Body", 6, "Defence, Action Speed, Raw Damage, Reflex"),
           ("Spirit", 3, "Resistance, Status Multiplier, Luck")]
for i, (p, v, subs) in enumerate(PILLARS):  # the substats each pillar scales on a skill (Rules CL-39)
    y = 170 + i * 120
    f = i == 1
    cut(d, (48, y, 700, y + 108), TEAL if f else PLATE, CYAN if f else STEEL, 10)
    text(d, (68, y + 12), p, 26, True)
    text(d, (68, y + 50), "Scales " + subs, 15, fill=TXT if f else DIM)
    text(d, (68, y + 74), "on skills that need " + p, 14, fill=TXT if f else OFF)
    text(d, (580, y + 22), "%d / 7" % v, 30, True, anchor="ra")
    if f:
        text(d, (598, y + 30), "→ 7", 24, True, CYAN)
panel(d, (730, 170, 1232, 530), focus=True)
text(d, (750, 182), "Body 6 → 7", 24, True)
y = 224
for line, col in [("Guard Break (needs Body 7): full strength", GOOD), ("was x0.6 while under its requirement", DIM),
                  ("Cleave (needs Body 4): boost x1.20 → x1.30", GOOD), ("Whirl (needs Body 5): boost x1.13 → x1.20", GOOD)]:
    text(d, (750, y), line, 16, line[0] != "w", col)
    y += 30
d.line((750, y + 8, 1214, y + 8), fill=STEEL)
wrap(d, (750, y + 22), "Levels are for this run only. 7 is the cap; a level past it still banks as +1 over-cap.", 464, 15, DIM)
cut(d, (48, 540, 700, 600), PLATE, STEEL, 10)
text(d, (68, 556), "Buy a point", 20, True)
chip(d, 470, 558, "Prisms", "300", EP)
footer(d, [("A", "Place"), ("X", "Buy a point"), ("B", "Done")])
save(im, OUT, "2_level_up.png")

# ---- 3: co-op revive vote. Everyone outside a fight must agree (RestSpotWidget's rule); the starter has voted yes.
im, d = canvas("world")
im, d = dim(im, 0.45)
menu(d, 4, coop=True, voting=True)
panel(d, (440, 140, 1000, 470), focus=True)
text(d, (464, 152), "Revive the mini-boss?", 26, True)
wrap(d, (464, 194), "Cinder Warden comes back one tier stronger, with one tier better drops (revival 1 of 3). Losing to it counts as a wipe.",
     510, 16)
text(d, (464, 262), "Everyone must agree", 16, True, DIM)
for i, (n, v) in enumerate([("You (host)", "Yes"), ("Kestrel", "Yes"), ("Dune", "…")]):
    y = 292 + i * 40
    text(d, (464, y), n, 18, True)
    text(d, (976, y), v, 18, True, GOOD if v == "Yes" else DIM, anchor="ra")
text(d, (464, 420), "2 of 3 said yes", 18, True, CYAN)
footer(d, [("A", "Yes"), ("X", "No"), ("B", "Back")])
save(im, OUT, "3_revive_vote.png")

# ---- 4: extract confirm (Leave the trial). Leaving early pays half (LEAVE_EARLY_SHARE); RunStructure section 6's wording.
im, d = canvas("world")
im, d = dim(im, 0.45)
menu(d, 6)
im, d = dim(im, 0.5)
panel(d, (340, 150, 940, 560), focus=True)
text(d, (364, 164), "Leave the trial?", 28, True)
wrap(d, (364, 210), "Full rewards only for beating the boss or falling in battle. Leaving now pays half.", 552, 17)
text(d, (364, 280), "Earned so far", 16, True, DIM)
text(d, (720, 280), "You get", 16, True, DIM)
for i, (lab, a, b, col) in enumerate([("Gold", "2,180", "1,090", GOLD), ("Gear Essence", "36", "18", STEEL), ("Skill Essence", "12", "6", STEEL)]):
    y = 310 + i * 34
    d.regular_polygon((372, y + 11, 8), 4, rotation=45, fill=col)
    text(d, (388, y), lab, 17)
    text(d, (640, y), a, 17, True, DIM, anchor="ra")
    text(d, (790, y), b, 17, True, anchor="ra")
text(d, (364, 430), "You keep", 16, True, DIM)
wrap(d, (364, 456), "Items found in the run go to home storage. Run levels, Prisms and run-only recruits are lost.", 552, 15, DIM)
footer(d, [("A", "Leave"), ("B", "Stay")])
save(im, OUT, "4_extract_confirm.png")
print("ok")
