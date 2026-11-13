"""Loadout (UIScreens.md #2 and #3): gear slots with their sockets and the budget, the socket picker, and the
Refractor's spell page. Costs are the code's (LoadoutConstants.h: weapon 1, adapter 0, crystal / stone +1,
evolution on gear +2, evolution alone 3). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def header(d, who, page, loadout):
    x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], who, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Gear", "Skills"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)
    panel(d, (760, 26, 1232, 112))
    text(d, (778, 34), "Loadout", 15, True, DIM)
    text(d, (778, 58), loadout, 22, True)
    text(d, (1216, 62), "Y  Saved", 15, fill=CYAN, anchor="ra")


def budget(d, used, total, x=48, y=134, preview=None):
    text(d, (x, y), "Budget", 17, True, DIM)
    for i in range(total):
        px = x + 90 + i * 40
        on = i < used
        new = preview is not None and used <= i < preview
        cut(d, (px, y - 2, px + 32, y + 22), CYAN if on else (WARN if new else TRACK), None, 6)
    shown = preview if preview is not None else used
    text(d, (x + 100 + total * 40, y), "%d of %d" % (shown, total), 18, True, WARN if preview else TXT)


def slot(d, y, kind, name, el, cost, sockets, focus=False, tag=None, outside=False):
    """One gear slot: icon, name, kind, its sockets (crystal or evolution; stone), its cost on the right."""
    box = (48, y, 700, y + 84)
    cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 10)
    if name:
        icon(d, (60, y + 10, 124, y + 74), ELEMENT[el])
        text(d, (138, y + 10), name, 21, True)
        text(d, (138, y + 40), kind, 15, fill=DIM if not focus else TXT)
        if tag:
            tw = int(d.textlength(tag, font=F(13, True))) + 14
            nx = 148 + int(d.textlength(name, font=F(21, True)))
            cut(d, (nx, y + 12, nx + tw, y + 34), BG, CYAN, 5)
            text(d, (nx + 7, y + 14), tag, 13, True, CYAN)
        for i, (sk, sname, scol) in enumerate(sockets):
            sx = 380 + i * 128
            if sname:
                icon(d, (sx, y + 14, sx + 34, y + 48), scol)
                text(d, (sx + 40, y + 16), sname, 15, True)
            else:
                icon(d, (sx, y + 14, sx + 34, y + 48), empty=True, letter="+")
                text(d, (sx + 40, y + 16), "empty", 15, fill=OFF)
            text(d, (sx + 40, y + 36), sk, 13, fill=DIM)
    else:
        icon(d, (60, y + 10, 124, y + 74), empty=True, letter="+")
        text(d, (138, y + 28), "Empty " + kind.lower(), 20, fill=OFF)
    text(d, (686, y + 30), "outside budget" if outside else cost, 16 if outside else 22, True,
         DIM if outside else (TXT if name else OFF), anchor="ra")


# ---- 1: Garrick (Generic): two equal weapon slots (one active), one adapter, two apparel outside the budget.
im, d = canvas()
header(d, 1, 0, "Bruiser  (2 of 5)")
budget(d, 5, 5)
slot(d, 172, "Weapon · Sword", "Ashen Blade", "Fire", "2", [("Crystal", "Garnet", ELEMENT["Fire"]), ("Stone", None, None)],
     focus=True, tag="ACTIVE")
slot(d, 264, "Weapon · Hammer", "Quarry Maul", "Earth", "2", [("Crystal", "Citrine", ELEMENT["Earth"]), ("Stone", None, None)])
slot(d, 356, "Adapter · Ring", "Storm Ring", "Lightning", "1", [("Crystal", "Amber", ELEMENT["Lightning"]), ("Stone", None, None)])
text(d, (48, 452), "Apparel", 17, True, DIM)
slot(d, 478, "Apparel", "Iron Coat", "None", "", [("Crystal", "Garnet", ELEMENT["Fire"])], outside=True)
slot(d, 570, "Apparel", None, None, "", [], outside=True)
panel(d, (730, 172, 1232, 654), focus=True)
text(d, (750, 182), "Ashen Blade", 26, True)
tier_badge(d, 750 + int(d.textlength("Ashen Blade", font=F(26, True))) + 22, 198, "B")
text(d, (750, 218), "Sword · Fire (Garnet) · active weapon", 16, fill=DIM)
y = 254
for lab, val in [("Power", "42"), ("Scaling", "Body B · Mind D"), ("Durability", "88 / 100"), ("Abilities", "Cleave, Whirl, Guard Break +3")]:
    text(d, (750, y), lab, 16, fill=DIM)
    text(d, (900, y), val, 16, True)
    y += 30
d.line((750, y + 6, 1214, y + 6), fill=STEEL)
text(d, (750, y + 18), "Cost", 16, True, DIM)
text(d, (750, y + 46), "Weapon 1  +  Crystal 1  =  2", 18, True)
text(d, (750, y + 76), "A stone would add 1: no budget left.", 16, fill=WARN)
d.line((750, y + 112, 1214, y + 112), fill=STEEL)
wrap(d, (750, y + 124), "Only the active weapon gives its abilities. Switch between the two in a fight (free, once per turn).",
     464, 15, DIM)
footer(d, [("A", "Change"), ("X", "Unequip"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "1_gear_generic.png")

# ---- 2: picking a crystal for the Hammer's socket. The list shows each crystal's cost and what the budget becomes;
#         an S-tier crystal is refused when one is already equipped (MAX_EQUIPPED_S_TIER_CRYSTALS = 1).
im, d = canvas()
header(d, 1, 0, "Bruiser  (2 of 5)")
budget(d, 4, 5, preview=5)
text(d, (48, 176), "Quarry Maul · crystal socket (empty)", 22, True)
text(d, (48, 206), "A crystal keys the weapon to its element and powers its elemental abilities.", 15, fill=DIM)
rows = [("Garnet", "Fire", "B", "+1", True, False), ("Citrine", "Earth", "C", "+1", False, False),
        ("Sapphire", "Water", "D", "+1", False, False), ("Onyx", "Darkness", "S", "S-tier: one per loadout", False, True),
        ("Quartz", "None", "E", "+1", False, False)]
for i, (n, el, t, right, f, off) in enumerate(rows):
    y = 238 + i * 64
    row(d, (48, y, 700, y + 56), n, focus=f, right=right, sub=el + " crystal", dot=ELEMENT[el], dim=off, size=19)
    tier_badge(d, 230, y + 28, t, 12)
text(d, (48, 562), "Sort: tier  ·  5 of 14 owned shown (fits this socket)", 15, fill=OFF)
panel(d, (730, 172, 1232, 654), focus=True)
text(d, (750, 182), "Compare", 16, True, DIM)
text(d, (750, 210), "Empty  →  Garnet", 24, True)
y = 256
for lab, a, b, col in [("Element", "Earth (weapon)", "Fire", TXT), ("Tier", "—", "B", GOOD), ("Budget", "4 of 5", "5 of 5", WARN),
                       ("Elemental abilities", "off", "on", GOOD), ("Wear", "—", "each infused use", DIM)]:
    text(d, (750, y), lab, 16, fill=DIM)
    text(d, (940, y), a, 16)
    text(d, (1060, y), "→", 16, fill=DIM)
    text(d, (1090, y), b, 16, True, col)
    y += 32
d.line((750, y + 8, 1214, y + 8), fill=STEEL)
wrap(d, (750, y + 20), "Two Fire sources on one character stack (Garnet on the Sword, Garnet here): an infused Fire attack uses both.",
     464, 15, DIM)
footer(d, [("A", "Socket"), ("Y", "Sort"), ("B", "Back")])
save(im, OUT, "2_socket_picker.png")

# ---- 3: Sera (Refractor) skills page: innate spells by school, the spell budget (tier = points, F 1 .. S 7), 6 per school.
im, d = canvas()
header(d, 0, 1, "Shadow Set  (1 of 5)")
text(d, (48, 134), "Innate spells", 17, True, DIM)
text(d, (200, 134), "18 of 24 points", 18, True)
bar(d, 380, 140, 300, 18 / 24, CYAN, 12)
tabs(d, 48, 172, ["Destruction  3", "Enhancement  1", "Impairment  1"], 0, size=17)
equipped = [("Shade Lance", "D", 3, "one enemy · builds Blind"), ("Umbral Rain", "C", 4, "all enemies · 3 hits"),
            ("Night Fang", "C", 4, "one enemy · eats a Shade stack")]
for i, (n, t, p, sub) in enumerate(equipped):
    y = 222 + i * 64
    row(d, (48, y, 700, y + 56), n, focus=(i == 1), right="%d pts" % p, sub="Darkness · " + sub, dot=ELEMENT["Darkness"], size=19)
    tier_badge(d, 560, y + 28, t, 12)
for i in range(3):
    y = 222 + (3 + i) * 64
    cut(d, (48, y, 700, y + 56), BG, OFF, 8)
    text(d, (64, y + 18), "Empty spell slot", 17, fill=OFF)
panel(d, (730, 134, 1232, 654), focus=True)
text(d, (750, 144), "Umbral Rain", 26, True)
tier_badge(d, 750 + int(d.textlength("Umbral Rain", font=F(26, True))) + 22, 160, "C")
text(d, (750, 180), "Darkness · Destruction · all enemies", 16, fill=DIM)
y = wrap(d, (750, 214), "Three falls of shadow on every enemy. Light damage per hit; each hit builds Blind.", 464, 16)
for lab, val in [("EP", "12"), ("Hits", "3"), ("Points", "4  (tier C; 5 less your Mind discount)")]:
    text(d, (750, y + 10), lab, 16, fill=DIM)
    text(d, (850, y + 10), val, 16, True)
    y += 30
d.line((750, y + 16, 1214, y + 16), fill=STEEL)
wrap(d, (750, y + 28), "Only Refractors have innate spells. Up to 6 per school; the points are shared across schools. Tabs show only the schools you hold a spell in.", 464, 15, DIM)
footer(d, [("A", "Change"), ("X", "Remove"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "3_skills_refractor.png")
# ---- 4: Bram (Resonator): one weapon and any number of adapters; adapters cost only what sits in them.
im, d = canvas()
header(d, 2, 0, "Ember Rings  (1 of 5)")
budget(d, 5, 5)
slot(d, 172, "Weapon · Gauntlets", "Cinder Fists", "None", "1", [("Crystal", None, None), ("Stone", None, None)])
slot(d, 264, "Adapter · Ring", "Ember Ring", "Fire", "1", [("Crystal", "Garnet", ELEMENT["Fire"]), ("Stone", None, None)],
     focus=True, tag="ACTIVE")
slot(d, 356, "Adapter · Ring", "Storm Ring", "Lightning", "1", [("Crystal", "Amber", ELEMENT["Lightning"]), ("Stone", None, None)])
slot(d, 448, "Adapter · Ring", "Tide Ring", "Water", "2", [("Evolution", "Tidecaller", ELEMENT["Water"]), ("Stone", None, None)])
slot(d, 540, "Adapter", None, None, "0", [])
text(d, (48, 632), "Apparel  (2, outside budget)  ↓", 16, True, DIM)
panel(d, (730, 172, 1232, 654), focus=True)
text(d, (750, 182), "Ember Ring", 26, True)
tier_badge(d, 750 + int(d.textlength("Ember Ring", font=F(26, True))) + 22, 198, "C")
text(d, (750, 218), "Adapter · Fire (Garnet) · active adapter", 16, fill=DIM)
y = 254
for lab, val in [("Spells", "Flame Lance, Ember Veil, Kindle"), ("Durability", "64 / 80")]:
    text(d, (750, y), lab, 16, fill=DIM)
    text(d, (880, y), val, 16, True)
    y += 30
d.line((750, y + 6, 1214, y + 6), fill=STEEL)
text(d, (750, y + 18), "Cost", 16, True, DIM)
text(d, (750, y + 46), "Adapter 0  +  Crystal 1  =  1", 18, True)
text(d, (750, y + 76), "An empty adapter costs nothing.", 16, fill=DIM)
d.line((750, y + 112, 1214, y + 112), fill=STEEL)
wrap(d, (750, y + 124), "Switch the active adapter in a fight from the Resonate tab (free).", 464, 15, DIM)
footer(d, [("A", "Change"), ("X", "Unequip"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "4_gear_resonator.png")
print("ok")
