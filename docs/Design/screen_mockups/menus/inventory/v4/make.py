"""Inventory v4 (Crown, 2026-11-25): the inventory in the approved equipment-picker layout (menus/loadout/v5).
Left: the bag (‹ Gear › Pouch; no Chest, D-250) and the owned pieces as a grid; middle: the fixed details panel
for the focused tile; right: Details, what the piece is doing now (where it is worn, fight-item pick, what it is
worth) and the actions it takes. No pop-ups. Values are placeholders. Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE


def inv_top(d):
    x = tabs(d, 48, 30, ["Generic", "Caster", "Resonator"], 0, lead="Z")
    text(d, (x + 4, 37), "C", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Inventory", "Equipment", "Skills", "Apparel"], 0, lead="Q")
    text(d, (x + 4, 85), "E", 16, True, CYAN)
    x = 880
    for label, value, col in (("Gold", "9,999", GOLD), ("Dormant Essence", "9,999", (150, 170, 190))):
        x = chip(d, x, 34, label, value, col)


def info_right(d, sub, rows, actions, note=None):
    """Details: plain facts about the focused piece, then the actions it takes (greyed with a reason if not)."""
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), sub, 14, fill=DIM)
    y = TOP + 70
    for label, value, col in rows:
        text(d, (SX0 + 18, y), label, 15, fill=DIM)
        text(d, (SX1 - 18, y), value, 15, True, col, anchor="ra")
        y += 25
    y += 8
    d.line((SX0 + 18, y, SX1 - 18, y), fill=(52, 54, 62))
    y += 14
    for key, verb, primary, dim in actions:
        action_button(d, (SX0 + 16, y, SX1 - 16, y + 38), key, verb, primary, dim)
        y += 46
    if note:
        wrap(d, (SX0 + 18, y + 2), note, SX1 - SX0 - 36, 14, WARN, gap=4)


def screen_gear():
    """Gear: weapons, adapters, evolutions, coats. The worn Arming Sword focused."""
    im, d = canvas("world")
    inv_top(d)
    items = [dict(kind="Sword", tier="F", worn=True), dict(kind="Sword", tier="F", count=2),
             dict(kind="Axe", tier="S", worn=True), dict(kind="Shield", tier="F", worn=True),
             dict(kind="Greatsword", tier="E"), dict(kind="Bow", tier="F"), dict(kind="Gun", tier="F"),
             dict(kind="Adapter", tier="D", col=ELEMENT["Fire"]), dict(kind="Evolution", tier="B", col=ELEMENT["Fire"]),
             dict(kind="Coat", tier="C", worn=True)]
    left_column(d, "Bag", "the Generic's  ·  no carry limit", ["Gear", "Pouch"], 0, items, 0, "10 pieces",
                ["All", "Sword", "Axe", "Shield", "Bow", "Coat"])
    middle_details(d, "Arming Sword", "F", "Sword", None, "Sword  ·  one hand  ·  worn",
                   [("Power", "40"), ("Scaling", "Body C"), ("Speed", "Fast"), ("Skill slots", "1")],
                   [(ELEMENT["Wind"], "Emerald (C)  ·  augment socket 1"), (None, "Augment socket 2: empty")],
                   "A plain soldier's blade. Its skill slots take sword skills.")
    info_right(d, "Arming Sword  ·  F",
               [("Worn in", "Loadout 1, main hand", TXT), ("Also in", "Loadout 2", TXT), ("Owned", "3", TXT),
                ("Durability", "60 / 60", GOOD), ("Sells for", "12 G", TXT)],
               [("A", "Equip on…", True, False), ("X", "Give to…", False, False),
                ("Y", "Sell at the Blacksmith", False, True)],
               note="Worn pieces can't be given or sold; take them off first.")
    footer(d, [("A", "Equip on…"), ("‹ ›", "Gear / Pouch"), ("X", "Give"), ("R", "Sort: Kind"), ("B", "Back")])
    return im


def screen_pouch():
    """Pouch: crystals, stones, potions. A picked stack shows the fight-item counter (D-250)."""
    im, d = canvas("world")
    inv_top(d)
    items = [dict(kind="Potion", count=14, picked=True), dict(kind="Crystal", tier="D", count=3, col=ELEMENT["Fire"], picked=True),
             dict(kind="Crystal", tier="D", count=2, col=ELEMENT["Water"]), dict(kind="Crystal", tier="S", col=ELEMENT["Wind"]),
             dict(kind="Stone", tier="C", count=4, picked=True), dict(kind="Stone", tier="E", count=2),
             dict(kind="Key")]
    left_column(d, "Bag", "the Generic's  ·  no carry limit", ["Gear", "Pouch"], 1, items, 0, "Fight items 3 / 10",
                ["All", "Potion", ("Crystal", ELEMENT["Fire"]), "Stone", "Key"])
    cell, gap, tx, ty = 62, 10, GX0 + 20, TOP + 152  # v5 left_column's grid; it draws no picked mark itself
    for i, it in enumerate(items):
        if it.get("picked"):
            r, c = divmod(i, 5)
            x1, y0 = tx + c * (cell + gap) + cell, ty + r * (cell + gap)
            cut(d, (x1 - 20, y0 + 3, x1 - 3, y0 + 18), TEAL, CYAN, 3)
            text(d, (x1 - 11, y0 + 11), "F", 10, True, TXT, anchor="mm")
    d.rectangle((GX0 + 14, BOTTOM - 30, GX1 - 14, BOTTOM - 6), fill=PANEL)
    ly = BOTTOM - 18
    cut(d, (GX0 + 18, ly - 8, GX0 + 35, ly + 7), TEAL, CYAN, 3)
    text(d, (GX0 + 26, ly), "F", 10, True, TXT, anchor="mm")
    text(d, (GX0 + 44, ly), "picked as a fight item", 13, fill=OFF, anchor="lm")
    middle_details(d, "Healing Potion", "", "Potion", None, "Fight item  ·  one target",
                   [("Heals", "35% HP"), ("Targets", "One ally"), ("Brings", "up to 10 a fight")],
                   [],
                   "A quick draught. Picked items come with you into a fight; the rest stay in the bag.")
    info_right(d, "Healing Potion  ·  x14",
               [("Fight item", "Picked (1 of 3)", GOOD), ("Brings into a fight", "10", TXT), ("Owned", "14", TXT),
                ("Slots", "3 of 10 used", TXT)],
               [("A", "Unpick (leave in the bag)", True, False), ("X", "Give to…", False, False),
                ("Y", "Use now", False, True)],
               note="More fight-item slots: the Jeweler.")
    footer(d, [("A", "Pick / unpick"), ("‹ ›", "Gear / Pouch"), ("X", "Give"), ("R", "Sort: Kind"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_gear(), OUT, "1_gear.png")
    save(screen_pouch(), OUT, "2_pouch.png")
    print("ok")
