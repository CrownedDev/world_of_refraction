"""Inventory v3 (Crown, 2026-11-20): Inventory and Loadout become one screen.
1: Inventory alone, the grid with the pop-up card (D-248 step 2). Y opens the loadout.
2: Loadout open: it slides in on the left (Equipment, Skills, Apparel, Cosmetics), the inventory moves right.
3: Equip from the inventory: the loadout opens and asks where the piece goes; the slots it fits are lit.
Reuses the v4 Equipment art and tiles. Values are placeholders. Run: python make.py"""
import importlib.util
import os

OUT = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("loadout_v4", os.path.join(OUT, "..", "..", "loadout", "v4", "make.py"))
_v4 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v4)
globals().update({k: v for k, v in vars(_v4).items() if not k.startswith("__") and k != "OUT"})

SPLIT = 452  # the loadout side's right edge when open
_v4.LX1 = SPLIT - 20  # entries() reads the v4 module's LX1

ITEMS = [  # (kind, tier, count, element, worn)
    ("Sword", "F", 1, None, True), ("Sword", "F", 2, None, False), ("Sword", "F", 1, None, True),
    ("Greatsword", "E", 1, None, True), ("Greatsword", "E", 1, None, False), ("Axe", "F", 1, None, False),
    ("Axe", "F", 1, None, False), ("Shield", "F", 1, None, False), ("Shield", "F", 1, None, False),
    ("Bow", "F", 1, None, False), ("Bow", "F", 1, None, False), ("Gun", "F", 1, None, False),
    ("Gun", "F", 1, None, False), ("Adapter", "D", 1, "Fire", True), ("Crystal", "B", 1, "Fire", False),
    ("Crystal", "D", 2, "Water", False), ("Evolution", "B", 1, "Fire", False), ("Stone", "C", 4, None, False),
    ("Coat", "C", 1, None, True),
]
FILTERS = ["All", "Sword", "Shield", ("Crystal", ELEMENT["Water"]), "Stone", "Coat"]


def top(d, open_loadout, title_right):
    x = tabs(d, 48, 30, ["Generic", "Caster", "Resonator"], 0, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    text(d, (1232, 34), title_right, 17, fill=DIM, anchor="ra")
    # the Loadout toggle, top right: lit while the loadout is open
    b = (1232 - 170, 74, 1232, 108)
    cut(d, b, TEAL if open_loadout else PLATE, CYAN if open_loadout else STEEL, 8)
    w = glyph(d, b[0] + 22, 91, "Y", 11)
    text(d, (b[0] + 22 + w / 2 + 10, 91), "Close loadout" if open_loadout else "Loadout", 15, True, anchor="lm")


def inv_side(d, x0, cols, focus_i, cell=70, gap=10):
    """Inventory tabs over the item grid; returns the grid's first row y and left x."""
    tabs(d, x0, 78, ["Gear", "Pouch", "Chest"], 0, h=30, size=15, lead="LB" if x0 == 48 else None)
    gx1 = x0 + 36 + cols * (cell + gap) - gap
    items = [dict(kind=k, tier=t, count=c, col=ELEMENT.get(e) if e else None, worn=w) for k, t, c, e, w in ITEMS]
    ty = grid(d, (x0, TOP, gx1, BOTTOM), items, cols, 5, cell, gap, focus_i, "Gear", "19 of 60", FILTERS, "", legend=False)
    return ty, x0 + 18


def card(d, fx1, fy, cell, name, tier, type_line, lines, keys, picked_note=None):
    """The pop-up card beside a tile (D-248 step 2): fx1 = the tile's right edge, fy = its top."""
    tb = (fx1 + 14, fy - 6, fx1 + 14 + 300, fy + 250)
    d.polygon([(tb[0], fy + 26), (tb[0] - 12, fy + 35), (tb[0], fy + 44)], fill=CYAN)
    cut(d, tb, (26, 24, 38), CYAN, 10)
    x0, y0, x1, y1 = tb
    text(d, (x0 + 16, y0 + 14), name, 20, True)
    text(d, (x0 + 16, y0 + 42), type_line, 14, fill=DIM)
    y = y0 + 72
    if picked_note:
        text(d, (x0 + 16, y), picked_note, 13, True, CYAN)
        y += 24
    for label, val in lines:
        text(d, (x0 + 16, y), label, 15, fill=DIM)
        text(d, (x1 - 16, y), val, 15, True, anchor="ra")
        y += 23
    for _ in range(2):
        d.regular_polygon((x0 + 24, y + 11, 7), 4, rotation=45, outline=STEEL)
        text(d, (x0 + 40, y + 11), "Empty socket", 14, fill=OFF, anchor="lm")
        y += 21
    d.line((x0 + 16, y1 - 46, x1 - 16, y1 - 46), fill=(60, 58, 76))
    gx = x0 + 28
    for key, verb in keys:
        w = glyph(d, gx, y1 - 24, key, 11)
        text(d, (gx + w / 2 + 8, y1 - 24), verb, 14, True, TXT, anchor="lm")
        gx += w + 20 + d.textlength(verb, font=F(14, True))


SWORD_LINES = [("Power", "40"), ("Scaling", "Body C"), ("Skill slots", "1")]


def loadout_side(d, focus_slot, ask=None):
    """The loadout, slid in on the left: its own tabs, then the weapon sets (the v4 entries)."""
    tabs(d, 48, 78, ["Equipment", "Skills", "Apparel", "Cosmetics"], 0, h=30, size=14)
    d.line((SPLIT, TOP - 4, SPLIT, BOTTOM), fill=(60, 62, 72), width=2)
    entries(d, focus_slot)
    if ask:  # replaces the "Weapon sets" heading while a piece waits for its slot
        cut(d, (48, TOP - 8, SPLIT - 20, TOP + 20), (40, 34, 58), CYAN, 6)
        text(d, (60, TOP + 6), ask, 14, True, CYAN, anchor="lm")


def screen_alone():
    im, d = canvas("world")
    top(d, False, "Void Realm  ·  hub")
    cell, gap, cols = 70, 10, 14
    ty, tx = inv_side(d, 48, cols, 1, cell, gap)
    card(d, tx + 2 * (cell + gap) - gap, ty, cell, "Arming Sword", "F", "Sword (F)  ·  two in your bag", SWORD_LINES,
         [("A", "Equip..."), ("X", "Give")])
    footer(d, [("Y", "Loadout"), ("View", "Sort: Kind"), ("B", "Back")])
    return im


def screen_open():
    im, d = canvas("world")
    top(d, True, "Void Realm  ·  hub")
    loadout_side(d, focus_slot=-1)
    cell, gap, cols = 70, 10, 9
    ty, tx = inv_side(d, SPLIT + 20, cols, 1, cell, gap)
    card(d, tx + 2 * (cell + gap) - gap, ty, cell, "Arming Sword", "F", "Sword (F)  ·  two in your bag", SWORD_LINES,
         [("A", "Equip..."), ("X", "Give")])
    footer(d, [("D-pad ←→", "Switch side"), ("Y", "Close loadout"), ("B", "Back")])
    return im


def screen_equip():
    im, d = canvas("world")
    top(d, True, "Void Realm  ·  hub")
    loadout_side(d, focus_slot=1, ask="Equip Arming Sword: pick a slot")
    cell, gap, cols = 70, 10, 9
    inv_side(d, SPLIT + 20, cols, 1, cell, gap)  # the Arming Sword stays lit while you pick its slot
    footer(d, [("A", "Equip here"), ("B", "Cancel")])
    return im


if __name__ == "__main__":
    save(screen_alone(), OUT, "1_inventory_alone.png")
    save(screen_open(), OUT, "2_loadout_open.png")
    save(screen_equip(), OUT, "3_equip_pick_slot.png")
    print("ok")
