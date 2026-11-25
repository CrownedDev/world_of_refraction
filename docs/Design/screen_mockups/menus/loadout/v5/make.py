"""Equipment picker v5 (Crown, 2026-11-25): the v4 look in Elden Ring's equipment-screen layout, no pop-ups.
Picking a piece for a slot fills the screen with three columns:
left, the slot being filled and the owned pieces as a grid (a kind selector on top cycles Weapon / Adapter /
Evolution, B-614); middle, a fixed details panel for the focused tile, always shown (B-598c: no hover card);
right, the character's status with what the focused piece would change. Hints along the bottom.
Values are placeholders. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "v4"))
from make import *  # noqa: E402,F401,F403  (v4's art, tiles, kit)

OUT = os.path.dirname(os.path.abspath(__file__))  # v4's import set its own OUT

TOP, BOTTOM = 134, 660
GX0, GX1 = 48, 448      # left: slot + grid
DX0, DX1 = 466, 878     # middle: details
SX0, SX1 = 896, 1232    # right: status


def kind_selector(d, x, y, kinds, lit, refused=None):
    """One line that cycles the kind with Left / Right (or a click): ‹ Weapon › with the other kinds dim beside it."""
    cut(d, (x, y, GX1 - 16, y + 34), PLATE, STEEL, 6)
    text(d, (x + 14, y + 17), "‹", 20, True, CYAN, anchor="lm")
    text(d, (GX1 - 30, y + 17), "›", 20, True, CYAN, anchor="rm")
    cx = x + 40
    for i, k in enumerate(kinds):
        lit_k = i == lit
        col = TXT if lit_k else (OFF if refused and k in refused else DIM)
        f = F(16, True) if lit_k else F(15)
        d.text((cx, y + 17), k, font=f, fill=col, anchor="lm")
        if lit_k:
            w = d.textlength(k, font=f)
            d.line((cx, y + 29, cx + w, y + 29), fill=CYAN, width=2)
        cx += d.textlength(k, font=f) + 22


def left_column(d, slot, current, kinds, lit_kind, items, focus_i, count_label, filters, refused=None):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), slot, 17, True)
    text(d, (GX0 + 18, TOP + 38), current, 14, fill=DIM)
    text(d, (GX1 - 16, TOP + 16), count_label, 13, True, DIM, anchor="ra")
    kind_selector(d, GX0 + 16, TOP + 62, kinds, lit_kind, refused)
    icon_filters(d, GX0 + 18, TOP + 108, filters, 0, "", size=30)
    cell, gap, cols, rows = 62, 10, 5, 5
    ty, tx = TOP + 152, GX0 + 20
    for i in range(cols * rows):
        r, c = divmod(i, cols)
        b = (tx + c * (cell + gap), ty + r * (cell + gap), tx + c * (cell + gap) + cell, ty + r * (cell + gap) + cell)
        if b[3] > BOTTOM - 34:
            break
        if i >= len(items):
            empty_tile(d, b)
            continue
        it = items[i]
        tile(d, b, it["kind"], it.get("tier"), it.get("count", 1), it.get("col"), it.get("worn", False),
             it.get("locked", False), i == focus_i)
    scroll_bar(d, GX1 - 12, ty, BOTTOM - 40, 0.0, 0.75)
    ly = BOTTOM - 18
    check_corner(d, GX0 + 18, ly - 9, 14)
    text(d, (GX0 + 38, ly), "worn", 13, fill=OFF, anchor="lm")
    lock(d, GX0 + 92, ly - 3)
    text(d, (GX0 + 104, ly), "can't go here (why: in the details)", 13, fill=OFF, anchor="lm")


def middle_details(d, name, tier, kind, col, type_line, stats, sockets, blurb, note=None):
    """Always shown; follows the focused tile. Title, big picture, stats, sockets, a short description."""
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), name, 22, True)
    if tier:
        tier_badge(d, DX0 + 38 + d.textlength(name, font=F(22, True)), TOP + 27, tier, 11)
    text(d, (DX0 + 20, TOP + 46), type_line, 14, fill=DIM)
    pic = (DX0 + 20, TOP + 74, DX1 - 20, TOP + 234)
    cut(d, pic, BG, STEEL, 10)
    d.rectangle((pic[0] + 6, pic[1] + 6, pic[2] - 6, pic[3] - 6), outline=(52, 54, 62))
    cx = (pic[0] + pic[2]) / 2
    art(d, (cx - 75, pic[1] + 5, cx + 75, pic[3] - 5), kind, col)
    y = TOP + 250
    for label, value in stats:
        text(d, (DX0 + 20, y), label, 15, fill=DIM)
        text(d, (DX1 - 20, y), value, 15, True, anchor="ra")
        y += 24
    for s_col, s_text in sockets:
        if s_col:
            d.regular_polygon((DX0 + 28, y + 9, 7), 6, fill=s_col)
            text(d, (DX0 + 44, y + 9), s_text, 14, fill=TXT, anchor="lm")
        else:
            d.regular_polygon((DX0 + 28, y + 9, 7), 4, rotation=45, outline=STEEL)
            text(d, (DX0 + 44, y + 9), s_text, 14, fill=OFF, anchor="lm")
        y += 21
    y += 8
    d.line((DX0 + 20, y, DX1 - 20, y), fill=(52, 54, 62))
    y = wrap(d, (DX0 + 20, y + 10), blurb, DX1 - DX0 - 40, 15, DIM, gap=5)
    if note:
        wrap(d, (DX0 + 20, y + 4), note, DX1 - DX0 - 40, 14, WARN, gap=4)


def right_status(d, who, rows, cells_used, cells_after, compare_with):
    """The character's status; a stat the focused piece changes shows worn → new, green up, red down."""
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), who, 14, fill=DIM)
    text(d, (SX0 + 18, TOP + 62), compare_with, 12, True, CYAN)
    y = TOP + 90
    for item in rows:
        if item is None:
            y += 8
            d.line((SX0 + 18, y, SX1 - 18, y), fill=(52, 54, 62))
            y += 12
            continue
        label, old, new, better = item
        stat_line(d, SX0 + 18, SX1 - 18, y, label, old, new, better)
        y += 24
    y += 10
    text(d, (SX0 + 18, y + 8), "Cells", 15, fill=DIM)
    px = SX1 - 18 - 5 * 22
    for i in range(5):
        filled = i < cells_used
        amber = cells_after is not None and cells_used <= i < cells_after
        c = WARN if amber else (CYAN if filled else STEEL)
        if filled or amber:
            d.regular_polygon((px + 10 + i * 22, y + 9, 8), 4, rotation=45, fill=c)
        else:
            d.regular_polygon((px + 10 + i * 22, y + 9, 8), 4, rotation=45, outline=c)


def top_tabs(d):
    x = tabs(d, 48, 30, ["Generic", "Caster", "Resonator"], 0, lead="Z")
    text(d, (x + 4, 37), "C", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Inventory", "Equipment", "Skills", "Apparel"], 1, lead="Q")
    text(d, (x + 4, 85), "E", 16, True, CYAN)
    text(d, (1232, 34), "Loadout 1", 20, True, anchor="ra")


def screen_1():
    """Filling the Off hand of the Arming Sword entry; the S Hand Axe focused."""
    im, d = canvas("world")
    top_tabs(d)
    items = [dict(kind="Axe", tier="S", worn=False), dict(kind="Sword", tier="S", worn=True),
             dict(kind="Sword", tier="F", count=2), dict(kind="Axe", tier="E"), dict(kind="Shield", tier="F"),
             dict(kind="Shield", tier="E"), dict(kind="Greatsword", tier="E", locked=True),
             dict(kind="Greataxe", tier="F", locked=True), dict(kind="Bow", tier="F", locked=True),
             dict(kind="Gun", tier="F", locked=True)]
    left_column(d, "Off hand", "Arming Sword's entry  ·  now: empty", ["Weapon", "Adapter", "Evolution"], 0,
                items, 0, "10 weapons", ["All", "Sword", "Axe", "Shield", "Greatsword", "Bow"])
    middle_details(d, "Hand Axe", "S", "Axe", None, "Axe  ·  one hand  ·  pairs with a sword",
                   [("Power", "40"), ("Scaling", "Body C"), ("Speed", "Normal"), ("Skill slots", "6")],
                   [(None, "Augment socket: empty"), (None, "Augment socket: empty")],
                   "A short woodsman's axe, balanced for the off hand. Its skill slots take axe skills.")
    right_status(d, "Generic  ·  Loadout 1",
                 [("Body", "12", "12", None), ("Mind", "8", "8", None), ("Spirit", "6", "6", None), None,
                  ("HP", "119", "119", None), ("EP", "71", "71", None), ("Attack power", "52", "92", True),
                  ("Skill slots", "6", "12", True), ("Speed", "Fast", "Normal", False)],
                 2, 2, "If you equip the Hand Axe")
    footer(d, [("A", "Equip"), ("‹ ›", "Kind"), ("X", "Short / full"), ("Y", "Sort: Kind"), ("B", "Back")])
    return im


def screen_2():
    """Filling the Hand Axe's augment socket: the selector opens on Crystal, the S Emerald focused."""
    im, d = canvas("world")
    top_tabs(d)
    items = [dict(kind="Crystal", tier="S", col=ELEMENT["Wind"]), dict(kind="Crystal", tier="D", count=3, col=ELEMENT["Fire"]),
             dict(kind="Crystal", tier="D", count=2, col=ELEMENT["Water"]), dict(kind="Stone", tier="C", count=4),
             dict(kind="Stone", tier="E", count=2), dict(kind="Crystal", tier="A", col=ELEMENT["Fire"], worn=True, locked=True)]
    left_column(d, "Augment socket 1", "Hand Axe  ·  now: empty", ["Crystal", "Stone", "Evolution"], 0,
                items, 0, "6 pieces", ["All", ("Crystal", ELEMENT["Wind"]), "Stone"])
    middle_details(d, "Emerald", "S", "Crystal", ELEMENT["Wind"], "Wind crystal  ·  1 cell",
                   [("Element", "Wind"), ("Infusion", "Wind, up to L2"), ("Status", "Bonus turn"), ("Skill slots", "+6")],
                   [],
                   "Wind skills and Wind infusion for the Hand Axe and the sword it pairs with.",
                   note="Only one S crystal per loadout. This would be it.")
    right_status(d, "Generic  ·  Loadout 1",
                 [("Body", "12", "12", None), ("Mind", "8", "8", None), ("Spirit", "6", "6", None), None,
                  ("HP", "119", "119", None), ("EP", "71", "71", None), ("Attack power", "92", "92", None),
                  ("Skill slots", "12", "18", True), ("Infusion", "none", "Wind", True)],
                 2, 3, "If you put in the Emerald")
    footer(d, [("A", "Put in"), ("‹ ›", "Kind"), ("X", "Short / full"), ("Y", "Sort: Tier"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_1(), OUT, "1_pick_weapon.png")
    save(screen_2(), OUT, "2_pick_crystal.png")
    print("ok")
