"""Pause / game menu (UIScreens.md #1). The hub of every menu: a page list on the left, the party on the right,
the run's state on top when in a trial. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

PAGES = ["Resume", "Loadout", "Inventory", "Character", "Party", "Journal", "Settings", "Leave"]
PARTY = [  # name, class line, element, HP, max HP, EP, max EP, Mind/Body/Spirit, tier
    ("Sera", "Darkness Refractor", "Darkness", 18, 84, 30, 40, (6, 3, 5), "C"),
    ("Garrick", "Generic", "Generic", 67, 84, 22, 30, (2, 7, 3), "B"),
    ("Bram", "Resonator", "Fire", 87, 97, 35, 35, (4, 4, 5), "C"),
]


def page_list(d, focus, last):
    for i, p in enumerate(PAGES):
        y = 150 + i * 56
        lab = last if (i == len(PAGES) - 1) else p
        row(d, (48, y, 330, y + 48), lab, focus == i)


def party(d, x=400, y=150, full=False):
    for i, (n, cls, el, hp, mhp, ep, mep, mbs, tier) in enumerate(PARTY):
        if full:  # the hub restores everyone
            hp, ep = mhp, mep
        top = y + i * 150
        panel(d, (x, top, x + 520, top + 136), focus=(i == 0))
        icon(d, (x + 16, top + 16, x + 96, top + 96), ELEMENT[el])  # portrait stand-in
        text(d, (x + 112, top + 12), n, 26, True)
        tier_badge(d, x + 112 + int(d.textlength(n, font=F(26, True))) + 22, top + 28, tier)
        text(d, (x + 112, top + 46), cls, 17, fill=DIM)
        text(d, (x + 112, top + 76), "HP", 15, True, DIM)
        bar(d, x + 146, top + 80, 170, hp / mhp, ROSE_LOW if hp / mhp < .3 else ROSE, 10)
        text(d, (x + 326, top + 72), "%d/%d" % (hp, mhp), 17, True, ROSE_LOW if hp / mhp < .3 else TXT)
        text(d, (x + 112, top + 102), "EP", 15, True, DIM)
        bar(d, x + 146, top + 106, 170, ep / mep, EP, 10)
        text(d, (x + 326, top + 98), "%d/%d" % (ep, mep), 17, True)
        for j, (lab, v) in enumerate(zip(("Mind", "Body", "Spirit"), mbs)):
            text(d, (x + 430, top + 18 + j * 34), lab, 15, fill=DIM)
            text(d, (x + 500, top + 16 + j * 34), str(v), 18, True, anchor="ra")


# ---- 1: in a trial. The run's state sits on top: where you are, the run money, wipes left.
im, d = canvas("world")
title(d, "Ember Trial", "Present · Medium")
x = chip(d, 400, 44, "Prisms", "1,240", EP)
x = chip(d, x, 44, "Gear Essence", "36", STEEL)
chip(d, x, 44, "wipes left", "3 of 5", WARN)
page_list(d, 0, "Quit to title")
party(d)
panel(d, (960, 150, 1232, 440))
text(d, (978, 162), "Tracked quest", 16, True, DIM)
text(d, (978, 190), "The Cinder Oath", 20, True)
text(d, (978, 216), "Main", 15, fill=CYAN)
wrap(d, (978, 246), "Beat the mini-boss in the ash fields without using items.", 236, 16)
wrap(d, (978, 372), "To leave the trial, extract at a rest spot.", 236, 15, OFF)
footer(d, [("A", "Select"), ("B", "Resume")])
save(im, OUT, "1_pause_trial.png")

# ---- 2: in the hub. No run state; the hub money (Gold) instead; Leave reads "Quit to title".
im, d = canvas("world")
title(d, "Void Realm", "The hub")
x = chip(d, 400, 44, "Gold", "8,420", GOLD)
x = chip(d, x, 44, "Gear Essence", "112", STEEL)
chip(d, x, 44, "Skill Essence", "48", STEEL)
page_list(d, 1, "Quit to title")
party(d, full=True)
panel(d, (960, 150, 1232, 440))
text(d, (978, 162), "Loadout", 16, True, DIM)
text(d, (978, 190), "Sera · Shadow Set", 20, True)
text(d, (978, 222), "Budget 3 of 3", 16, fill=TXT)
bar(d, 978, 250, 236, 1.0, CYAN, 10)
for i, (n, c) in enumerate([("Night Staff", "Darkness"), ("Veil Cloak", "Darkness"), ("Ash Mantle", "None")]):
    icon(d, (978 + i * 80, 280, 1046 + i * 80, 348), ELEMENT[c])
text(d, (978, 360), "1 item · 2 apparel", 15, fill=DIM)
footer(d, [("A", "Open"), ("B", "Close")])
save(im, OUT, "2_pause_hub.png")
print("ok")
