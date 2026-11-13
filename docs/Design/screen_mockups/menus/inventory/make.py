"""Inventory and pouch (UIScreens.md #4): one screen, three tabs (Gear, Pouch, Chest), one verb menu
(Give, Store, Take, Dismantle, Convert: D-131, WoRInventoryVerbs). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def header(d, who, page, place="Void Realm · hub"):
    x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], who, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Gear", "Pouch", "Chest"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)
    text(d, (1232, 40), place, 16, fill=DIM, anchor="ra")


def grid(d, items, focus, x0=48, y0=176, cols=6, size=96, gap=12, slots=None, locked=0):
    """An icon grid. items: (element, letter, count or None, worn). Empty and locked slots after them."""
    n = slots if slots else len(items)
    for i in range(n + locked):
        cx, cy = x0 + (i % cols) * (size + gap), y0 + (i // cols) * (size + gap)
        box = (cx, cy, cx + size, cy + size)
        if i < len(items):
            el, letter, count, worn = items[i]
            icon(d, box, ELEMENT[el], letter)
            if i == focus:
                cut(d, box, None, CYAN, 8)
                cut(d, (cx - 2, cy - 2, cx + size + 2, cy + size + 2), None, CYAN, 9)
            if count:
                text(d, (cx + size - 8, cy + size - 24), "x%d" % count, 15, True, anchor="ra")
            if worn:
                text(d, (cx + 8, cy + 6), "E", 14, True, CYAN)
        elif i < n:
            icon(d, box, empty=True)
        else:
            cut(d, box, BG, TRACK, 8)
            text(d, (cx + size / 2, cy + size / 2), "Jeweler", 12, fill=OFF, anchor="mm")


def detail(d, name, tier, kind, lines, note=None):
    panel(d, (730, 128, 1232, 654), focus=True)
    text(d, (750, 140), name, 26, True)
    if tier:
        tier_badge(d, 750 + int(d.textlength(name, font=F(26, True))) + 22, 156, tier)
    text(d, (750, 176), kind, 16, fill=DIM)
    y = 212
    for lab, val in lines:
        text(d, (750, y), lab, 16, fill=DIM)
        text(d, (900, y), val, 16, True)
        y += 30
    if note:
        d.line((750, y + 8, 1214, y + 8), fill=STEEL)
        wrap(d, (750, y + 20), note, 464, 15, DIM)


def verbs(d, x, y, items, focus):
    """The verb menu (A on an item): every verb listed; one that doesn't apply is greyed with its reason under it."""
    h = 22 + sum(52 + (22 if why else 0) for _, why in items)
    panel(d, (x, y, x + 300, y + h), focus=True)
    ry = y + 12
    for i, (v, why) in enumerate(items):
        row(d, (x + 12, ry, x + 288, ry + 44), v, focus=(i == focus), dim=bool(why), size=18)
        ry += 52
        if why:
            text(d, (x + 16, ry - 4), why, 14, fill=OFF)
            ry += 22


GEAR = [("Fire", "W", None, True), ("Earth", "W", None, True), ("Lightning", "R", None, True), ("Fire", "W", None, False),
        ("Water", "R", None, False), ("Void", "Ev", None, False), ("Fire", "C", 3, False), ("Earth", "C", 1, False),
        ("Darkness", "C", 1, False), ("None", "Ap", None, True), ("None", "Ap", None, False), ("Generic", "S", 2, False)]

# ---- 1: Gear tab. Filter row on top; E marks what a loadout wears; the verb menu is open on a spare sword.
im, d = canvas()
header(d, 1, 0)
tabs(d, 48, 128, ["All", "Weapons", "Adapters", "Crystals", "Evolutions", "Apparel"], 0, h=30, size=15)
grid(d, GEAR, 3, size=96)
text(d, (48, 520), "E  worn in a loadout", 15, fill=DIM)
detail(d, "Ashen Blade", "D", "Sword · Fire · a second copy (weaker)",
       [("Power", "28"), ("Durability", "100 / 100"), ("Abilities", "Cleave, Whirl +1")],
       "You own a better Ashen Blade (B). The weaker copy can convert to Gold or be dismantled (one copy per character, D-115).")
verbs(d, 750, 410, [("Give", None), ("Store", None), ("Dismantle", None), ("Convert to 140 Gold", None)], 3)
footer(d, [("A", "Choose"), ("Y", "Sort"), ("B", "Back")])
save(im, OUT, "1_gear.png")

# ---- 2: Pouch tab: 10 slots to start (20 with the Jeweler's upgrades), stacks of 10. Give: pick who receives.
POUCH = [("Wind", "Po", 6, False), ("Water", "Po", 3, False), ("Fire", "Bo", 2, False), ("Light", "El", 1, False),
         ("None", "Ke", 1, False), ("Earth", "Ti", 10, False), ("Void", "Ch", 1, False)]
im, d = canvas("world")
header(d, 0, 1, "Ember Trial · rest spot")
text(d, (48, 136), "Pouch  7 of 10 slots", 17, True, DIM)
grid(d, POUCH, 0, slots=10, locked=2, size=96)
detail(d, "Mending Draught", None, "Item · restores HP · usable in a fight",
       [("Stack", "6 of 10"), ("Restores", "35% HP to one ally")],
       "Each character carries their own pouch (D-117). Give moves it to another member's pouch.")
verbs(d, 750, 360, [("Give", None), ("Store", "Home chest: in the hub only")], 0)
panel(d, (430, 440, 712, 640), focus=True)
text(d, (446, 450), "Give to", 16, True, DIM)
for i, (n, room) in enumerate([("Garrick", "4 slots free"), ("Bram", "full")]):
    row(d, (446, 478 + i * 52, 698, 522 + i * 52), n, focus=(i == 0), size=18)
    text(d, (446 + 252 - 12, 500 + i * 52), room, 13, fill=DIM if i == 0 else WARN, anchor="rm")
text(d, (446, 590), "X  Whole stack: on", 15, fill=CYAN)
footer(d, [("A", "Give"), ("X", "Whole stack"), ("B", "Back")])
save(im, OUT, "2_pouch_give.png")

# ---- 3: Dismantle: one confirm with what you get and what you lose (D-016's warning text, GetDismantleWarning).
im, d = canvas()
header(d, 1, 0)
tabs(d, 48, 128, ["All", "Weapons", "Adapters", "Crystals", "Evolutions", "Apparel"], 2, h=30, size=15)
grid(d, [GEAR[2], GEAR[4]], 1, size=96)
from PIL import Image, ImageDraw  # noqa: E402
im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), 0.55)  # the screen dims behind a confirm
d = ImageDraw.Draw(im)
panel(d, (360, 200, 920, 520), focus=True)
text(d, (384, 214), "Dismantle Tide Ring?", 26, True)
text(d, (384, 252), "Adapter · Water · tier C", 16, fill=DIM)
text(d, (384, 296), "You get", 16, True, DIM)
chip(d, 384, 322, "Gear Essence", "18", STEEL)
chip(d, 560, 322, "Water Essence", "4", ELEMENT["Water"])
text(d, (384, 370), "You lose", 16, True, DIM)
wrap(d, (384, 396), "The ring and its upgrade level. Its Sapphire comes off first and stays yours.", 512, 16)
text(d, (384, 460), "This can't be undone.", 16, True, WARN)
footer(d, [("A", "Dismantle"), ("B", "Cancel")])
save(im, OUT, "3_dismantle_confirm.png")
print("ok")
