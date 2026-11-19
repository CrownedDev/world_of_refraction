"""Inventory v2 (UI review 2026-11-19, docs/Design/UIReview-2026-11-19.md): Crown's classic RPG reference.
A big icon grid with counts and tiers on the left; on the right a detail panel with the picture, name, type,
description and the action buttons (Equip, Give, Store). Same tiles and art as the Equipment page v4
(menus/loadout/v4/make.py), so the two pages read as one. Values are placeholders. Run: python make.py"""
import importlib.util
import os

OUT = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("loadout_v4", os.path.join(OUT, "..", "..", "loadout", "v4", "make.py"))
_v4 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v4)
globals().update({k: v for k, v in vars(_v4).items() if not k.startswith("__") and k != "OUT"})  # kit, art(), tile()...

GX0, GX1 = 48, 812
DX0, DX1 = 832, 1232


def inv_header(d, page, place):
    x = tabs(d, 48, 30, ["Generic", "Caster", "Resonator"], 0, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Gear", "Loadout", "Pouch", "Chest"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)
    text(d, (1232, 34), place, 17, fill=DIM, anchor="ra")
    cx = 1232 - 300
    cx = chip(d, cx, 80, "Gold", "1,240")
    chip(d, cx, 80, "Essence", "36", STEEL)


GEAR = [  # (kind, tier, count, element, worn)
    ("Sword", "F", 1, None, True), ("Sword", "F", 2, None, False), ("Sword", "F", 1, None, True),
    ("Greatsword", "E", 1, None, True), ("Greatsword", "E", 1, None, False), ("Greataxe", "F", 1, None, False),
    ("Greataxe", "F", 1, None, False), ("Shield", "F", 1, None, False), ("Shield", "E", 1, None, False),
    ("Shield", "F", 1, None, False), ("Bow", "F", 1, None, False), ("Bow", "F", 1, None, False),
    ("Gun", "F", 1, None, False), ("Gun", "F", 1, None, False), ("Adapter", "D", 1, "Fire", True),
    ("Adapter", "F", 5, None, False), ("Crystal", "B", 1, "Fire", False), ("Crystal", "D", 3, "Fire", False),
    ("Crystal", "D", 2, "Water", False), ("Crystal", "C", 1, "Lightning", False), ("Crystal", "E", 3, "Wind", False),
    ("Evolution", "B", 1, "Fire", False), ("Stone", "C", 4, None, False), ("Stone", "E", 2, None, False),
    ("Coat", "C", 1, None, True), ("Coat", "E", 1, None, False),
]
GEAR_FILTERS = ["All", "Sword", "Shield", ("Adapter", ELEMENT["Fire"]), ("Crystal", ELEMENT["Water"]),
                ("Evolution", ELEMENT["Void"]), "Stone", "Coat"]


def inv_grid(d, items, focus_i, title_l, title_r, filters, cols=8, rows=4, cell=80, gap=10, extra=None, legend=True):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), title_l, 16, True)
    text(d, (GX1 - 16, TOP + 16), title_r, 14, True, DIM, anchor="ra")
    icon_filters(d, GX0 + 18, TOP + 44, filters, 0, "")
    ty = TOP + 96
    for i in range(cols * rows):
        r, c = divmod(i, cols)
        b = (GX0 + 18 + c * (cell + gap), ty + r * (cell + gap), GX0 + 18 + c * (cell + gap) + cell,
             ty + r * (cell + gap) + cell)
        if extra and i in extra:
            extra[i](d, b)
        elif i >= len(items):
            empty_tile(d, b)
        else:
            it = items[i]
            tile(d, b, it[0], it[1], it[2], ELEMENT.get(it[3]) if it[3] else None, it[4], False, i == focus_i)
    scroll_bar(d, GX1 - 14, ty, ty + rows * (cell + gap) - gap, 0.0, 0.8)
    if not legend:
        return
    ly = BOTTOM - 22
    check_corner(d, GX0 + 18, ly - 9, 14)
    text(d, (GX0 + 38, ly), "worn in a loadout", 13, fill=OFF, anchor="lm")


def screen_gear():
    im, d = canvas("world")
    inv_header(d, 0, "Void Realm  ·  hub")
    inv_grid(d, GEAR, 1, "Gear", "26 of 60", GEAR_FILTERS)
    details_panel(
        d, (DX0, TOP, DX1, BOTTOM), "Sword", None, "Arming Sword", "F", "Sword  ·  one hand  ·  pairs with a sword",
        None,
        [("Power", None, "46", None), ("Speed", None, "Normal", None), ("Scaling", None, "Body C", None),
         ("Skill slots", None, "1", None)],
        [(None, "Empty socket"), (None, "Empty socket")],
        "A plain soldier's blade. Not worn: two in your bag.",
        [[("A", "Equip...", True, False)], [("X", "Give", False, False), ("Y", "Store", False, False)]])
    footer(d, [("View", "Sort: Kind"), ("B", "Back")])
    return im


POUCH = [("Potion", None, 6, "Wind", False), ("Potion", None, 3, "Water", False), ("Potion", None, 2, "Fire", False),
         ("Potion", None, 1, "Light", False), ("Key", None, 1, None, False), ("Potion", None, 10, "Earth", False),
         ("Potion", None, 1, "Void", False)]
POUCH_FILTERS = ["All", ("Potion", ELEMENT["Wind"]), ("Key", None)]


def jeweler_tile(d, b):
    cut(d, b, (20, 22, 28), (44, 46, 54), 6)
    lock(d, (b[0] + b[2]) / 2, b[1] + 28)
    text(d, ((b[0] + b[2]) / 2, b[3] - 18), "Jeweler", 12, fill=OFF, anchor="mm")


def screen_pouch():
    im, d = canvas("world")
    inv_header(d, 2, "Ember Trial  ·  rest spot")
    inv_grid(d, POUCH, 0, "Pouch", "7 of 10 slots", POUCH_FILTERS, cols=7, rows=3, cell=92, gap=10,
             extra={**{i: jeweler_tile for i in range(10, 20)}, 20: lambda d, b: None}, legend=False)  # 20 slots at most
    details_panel(
        d, (DX0, TOP, DX1, BOTTOM), "Potion", ELEMENT["Wind"], "Mending Draught", None, "Item  ·  usable in a fight",
        None,
        [("Restores", None, "35% HP, one ally", None), ("Stack", None, "6 of 10", None)],
        [],
        "Each character carries their own pouch. Give moves it to another member's pouch.",
        [[("A", "Give...", True, False)], [("X", "Whole stack", False, False), ("Y", "Store", False, True)]],
        note="Store: the home chest is in the hub only.")
    footer(d, [("View", "Sort: Kind"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_gear(), OUT, "1_gear_grid.png")
    save(screen_pouch(), OUT, "2_pouch_grid.png")
    print("ok")
