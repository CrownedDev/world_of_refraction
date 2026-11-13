"""Rest spot (UIScreens.md #22-24, #34): the rest menu, Level up, the co-op revive vote, the extract confirm.
Crown's words (D-182 to D-186): short labels, a currency strip top right (GE SE EE G P), pending levels as "+1",
values inside the party bars, no buying points here, "Extract" only at a boss's rest spot. Numbers from RunRewards.h (leave early = 50%),
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


def vbar(d, x, y, w, frac, col, value, h=20):
    """A bar with its value centred inside: plain white text on the darker bar (D-185; the battle HUD's style)."""
    bar(d, x, y, w, frac, col, h)
    text(d, (x + w / 2, y + h / 2), value, 13, True, anchor="mm")


def currencies(d):
    """The currency strip, top right: Gear, Skill and Elemental Essence, Gold, Prisms (D-182)."""
    x = 742
    for label, value, col in [("GE", "36", STEEL), ("SE", "12", STEEL), ("EE", "8", ELEMENT["Fire"]), ("G", "2,180", GOLD), ("P", "1,420", EP)]:
        x = chip(d, x, 44, label, value, col)


def menu(d, focus, coop=False, voting=False):
    title(d, "Rest spot", "Cinder Warden's camp · Ember Trial")
    currencies(d)
    # Extract shows only at a boss's rest spot (D-186); this camp is one.
    items = [("Level up", "+3"), ("Loadout", None), ("Inventory", None), ("Spirit of Sera", None),
             ("Revive boss", "1 of 3"), ("Invite", None)]
    if coop:
        items.append(("Hand over host", None))
    items.append(("Extract", None))
    for i, (lab, right) in enumerate(items):
        y = 140 + i * 56
        off = voting and lab == "Revive boss"
        row(d, (48, y, 400, y + 48), lab, focus == i, right=("vote open" if off else right), dim=off, size=19)


def restored(d):
    panel(d, (440, 140, 860, 330))
    text(d, (460, 150), "The party is restored.", 20, True, CYAN)
    for i, (n, cls, hp, ep) in enumerate(PARTY):
        y = 190 + i * 44
        text(d, (460, y), n, 18, True)
        vbar(d, 560, y, 136, 1, ROSE, "%d/%d" % (hp, hp))
        vbar(d, 704, y, 136, 1, EP, "%d/%d" % (ep, ep))


# ---- 1: the rest menu (solo). Resting is automatic on arrival: the party is restored, then the menu opens.
im, d = canvas("world")
im, d = dim(im, 0.45)
menu(d, 0)
restored(d)
panel(d, (900, 140, 1232, 500), focus=True)
text(d, (920, 150), "Level up", 20, True)
wrap(d, (920, 184), "Place the pillar levels your characters have banked from fights.", 290, 16)
for i, (n, k) in enumerate([("Sera", 1), ("Garrick", 2), ("Bram", 0)]):
    text(d, (920, 270 + i * 30), n, 16, fill=DIM)
    text(d, (1210, 270 + i * 30), "+%d" % k if k else "—", 16, True, CYAN if k else OFF, anchor="ra")
wrap(d, (920, 380), "Enemies stay beaten when you rest.", 290, 15, DIM)
footer(d, [("A", "Select"), ("B", "Close")])
save(im, OUT, "1_rest_menu.png")

# ---- 2: Level up: one member at a time (LT / RT), place each banked level on Mind, Body or Spirit; the preview shows
#         Simple (D-183): no list of the skills a level affects, and no buying points here (a trial NPC sells them).
im, d = canvas("world")
im, d = dim(im, 0.6)
x = tabs(d, 48, 30, ["Sera  +1", "Garrick  +2", "Bram"], 1, lead="LT")
text(d, (x + 4, 37), "RT", 16, True, CYAN)
title(d, "Garrick", "Generic · +2", y=84)
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
panel(d, (730, 170, 1232, 330), focus=True)
text(d, (750, 182), "Body 6 → 7", 24, True)
d.line((750, 226, 1214, 226), fill=STEEL)
wrap(d, (750, 240), "Levels are for this run only. 7 is the cap; a level past it still banks as +1 over-cap.", 464, 15, DIM)
footer(d, [("A", "Place"), ("B", "Done")])
save(im, OUT, "2_level_up.png")

# ---- 3: co-op revive vote. Everyone outside a fight must agree (RestSpotWidget's rule); the starter has voted yes.
im, d = canvas("world")
im, d = dim(im, 0.45)
menu(d, 4, coop=True, voting=True)
panel(d, (440, 140, 1000, 470), focus=True)
text(d, (464, 152), "Revive boss?", 26, True)
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

# ---- 4: extract confirm. Extracting pays half (LEAVE_EARLY_SHARE) and keeps the run's upgrades (D-186, D-189).
im, d = canvas("world")
im, d = dim(im, 0.45)
menu(d, 6)
im, d = dim(im, 0.5)
panel(d, (340, 150, 940, 560), focus=True)
text(d, (364, 164), "Extract?", 28, True)
wrap(d, (364, 210), "Extracting now pays half of what the run has earned, and you keep the run's upgrades. Beating the boss pays in full.", 552, 17)
text(d, (364, 280), "This run", 16, True, DIM)
text(d, (720, 280), "You get", 16, True, DIM)
for i, (lab, a, b, col) in enumerate([("Gold", "2,180", "1,090", GOLD), ("Gear Essence", "36", "18", STEEL), ("Skill Essence", "12", "6", STEEL)]):
    y = 310 + i * 34
    d.regular_polygon((372, y + 11, 8), 4, rotation=45, fill=col)
    text(d, (388, y), lab, 17)
    text(d, (640, y), a, 17, True, DIM, anchor="ra")
    text(d, (790, y), b, 17, True, anchor="ra")
text(d, (364, 430), "You keep", 16, True, DIM)
wrap(d, (364, 456), "Items found in the run go to home storage. Run levels, Prisms and run-only recruits are lost.", 552, 15, DIM)
footer(d, [("A", "Extract"), ("B", "Stay")])
save(im, OUT, "4_extract_confirm.png")
print("ok")
