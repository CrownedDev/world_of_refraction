"""Equipment page v4 (UI review 2026-11-19, docs/Design/UIReview-2026-11-19.md). Replaces the v3 grid draft.
Left: the loadout entries, the focused entry open with its slots (a filled slot shows the piece, its type and tier;
the slot word only when empty). Middle: the owned pieces as an icon grid; what fits the lit slot comes first, what
doesn't is greyed with a lock. Right (variant A): the focused piece, picture, stats compared with what the slot wears,
action buttons. Variant B drops the right panel for a wider grid and a tooltip beside the focused tile.
Values are placeholders. Run: python make.py (also imported by menus/inventory/v2/make.py for the art)."""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

TOP = 134
BOTTOM = 660
LX0, LX1 = 48, 372
LIGHT = (214, 218, 226)
DARK = (60, 50, 44)


# ---------------------------------------------------------------- item art (stand-ins for rendered pictures)
def art(d, box, kind, col=None, dim=False):
    """A flat silhouette per item kind. col: the element colour for gems, flasks, rings."""
    x0, y0, x1, y1 = box
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    s = (x1 - x0) / 70.0
    fg = OFF if dim else LIGHT
    gold = (120, 110, 90) if dim else GOLD
    dk = (50, 48, 46) if dim else DARK
    ec = OFF if dim else (col or STEEL)

    def P(pts, fill=fg):
        d.polygon([(cx + px * s, cy + py * s) for px, py in pts], fill=fill)

    def R(a, b, c, e, fill):
        d.rectangle((cx + a * s, cy + b * s, cx + c * s, cy + e * s), fill=fill)

    if kind in ("Sword", "Greatsword"):
        L, w = (24, 3) if kind == "Sword" else (29, 5)
        P([(-w, -L), (0, -L - 5), (w, -L), (w, 10), (-w, 10)])
        R(-11, 10, 11, 13, gold)
        R(-2, 13, 2, 24, dk)
    elif kind in ("Axe", "Greataxe"):
        R(-2, -24, 2, 28, dk)
        P([(2, -22), (16, -28), (16, -4), (2, -10)])
        if kind == "Greataxe":
            P([(-2, -22), (-16, -28), (-16, -4), (-2, -10)])
    elif kind == "Shield":
        P([(-19, -23), (19, -23), (19, 2), (0, 25), (-19, 2)])
        d.regular_polygon((cx, cy - 4 * s, 6 * s), 4, rotation=45, fill=gold)
    elif kind == "Bow":
        d.arc((cx - 30 * s, cy - 27 * s, cx + 10 * s, cy + 27 * s), -70, 70, fill=fg, width=max(2, int(4 * s)))
        d.line((cx + 1 * s, cy - 25 * s, cx + 1 * s, cy + 25 * s), fill=DIM, width=1)
    elif kind == "Gun":
        P([(-25, -6), (21, -6), (21, 2), (-6, 2), (-12, 17), (-21, 17), (-17, 2), (-25, 2)])
        R(21, -4, 27, 0, dk)
    elif kind == "Crystal":
        d.regular_polygon((cx, cy, 17 * s), 6, fill=ec)
        d.regular_polygon((cx - 4 * s, cy - 4 * s, 6 * s), 6, fill=(255, 255, 255) if not dim else (130, 134, 140))
    elif kind == "Stone":
        d.ellipse((cx - 17 * s, cy - 11 * s, cx + 17 * s, cy + 12 * s), fill=(150, 140, 128) if not dim else OFF)
        d.ellipse((cx - 9 * s, cy - 7 * s, cx + 1 * s, cy - 2 * s), fill=(190, 182, 170) if not dim else (130, 134, 140))
    elif kind == "Evolution":
        pts = []
        for k in range(8):  # a four-point star
            import math
            a = math.pi / 4 * k - math.pi / 2
            r = (20 if k % 2 == 0 else 7) * s
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
        d.polygon(pts, fill=ec)
    elif kind == "Adapter":
        d.ellipse((cx - 17 * s, cy - 17 * s, cx + 17 * s, cy + 17 * s), outline=gold, width=max(2, int(5 * s)))
        d.regular_polygon((cx, cy - 15 * s, 6 * s), 6, fill=ec)
    elif kind == "Potion":
        R(-4, -22, 4, -14, (150, 120, 90))
        d.ellipse((cx - 15 * s, cy - 14 * s, cx + 15 * s, cy + 18 * s), fill=ec)
        d.ellipse((cx - 9 * s, cy - 8 * s, cx - 3 * s, cy - 2 * s), fill=(255, 255, 255))
    elif kind == "Coat":
        P([(-10, -22), (10, -22), (20, -14), (16, 22), (-16, 22), (-20, -14)], fill=(120, 108, 96) if not dim else OFF)
        R(-1, -20, 1, 22, dk)
    elif kind == "Key":
        d.ellipse((cx - 16 * s, cy - 8 * s, cx - 2 * s, cy + 6 * s), outline=gold, width=max(2, int(4 * s)))
        R(-2, -2, 18, 2, gold)
        R(12, 2, 15, 8, gold)


def lock(d, x, y, col=OFF):
    """A small padlock: 'can't go here' (the reason is in the details, never hidden)."""
    d.rectangle((x - 6, y - 1, x + 6, y + 8), fill=col)
    d.arc((x - 5, y - 9, x + 5, y + 3), 180, 360, fill=col, width=2)


def check_corner(d, x, y, size=18):
    """Violet corner with a tick: worn in a loadout."""
    d.polygon([(x, y), (x + size, y), (x, y + size)], fill=TEAL)
    d.line((x + 3, y + 7, x + 6, y + 10, x + 11, y + 3), fill=TXT, width=2)


def tile(d, box, kind, tier=None, count=1, col=None, worn=False, locked=False, focus=False, picked=False):
    """One grid tile: picture, tier bottom-left, count bottom-right, worn tick top-left, lock top-right."""
    x0, y0, x1, y1 = box
    if focus:
        d.rectangle((x0 - 3, y0 - 3, x1 + 3, y1 + 3), outline=CYAN, width=2)
    cut(d, box, (40, 34, 58) if focus else ((26, 28, 34) if locked else PLATE), CYAN if focus else (STEEL if not locked else (60, 62, 70)), 6)
    pad = (x1 - x0) * 0.12
    art(d, (x0 + pad, y0 + pad, x1 - pad, y1 - pad), kind, col, dim=locked)
    if tier:
        tier_badge(d, x0 + 12, y1 - 12, tier, 8)
    if count > 1:
        text(d, (x1 - 6, y1 - 4), "x%d" % count, 13, True, OFF if locked else TXT, anchor="rd")
    if worn:
        check_corner(d, x0 + 1, y0 + 1)
    if locked:
        lock(d, x1 - 12, y0 + 12)
    if picked:
        cut(d, (x1 - 22, y0 + 2, x1 - 2, y0 + 18), TEAL, CYAN, 3)


def empty_tile(d, box):
    cut(d, box, (22, 24, 30), (44, 46, 54), 6)


def scroll_bar(d, x, y0, y1, frac0, frac1):
    d.rectangle((x, y0, x + 5, y1), fill=TRACK)
    d.rectangle((x, y0 + (y1 - y0) * frac0, x + 5, y0 + (y1 - y0) * frac1), fill=STEEL)


# ---------------------------------------------------------------- shared frame
def header(d, page, cells_used=2, cells_after=None):
    x = tabs(d, 48, 30, ["Generic", "Caster", "Resonator"], 0, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Inventory", "Equipment", "Skills", "Apparel"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)
    text(d, (1232, 34), "Loadout 1", 20, True, anchor="ra")
    # cells: pips, the pick's change in amber before you confirm (v1's rule, Expedition 33's Pictos points)
    px = 1232 - 5 * 22
    text(d, (px - 12, 92), "Cells", 15, True, DIM, anchor="rm")
    for i in range(5):
        filled = i < cells_used
        amber = cells_after is not None and cells_used <= i < cells_after
        c = WARN if amber else (CYAN if filled else TRACK)
        d.regular_polygon((px + 10 + i * 22, 92, 8), 4, rotation=45, fill=c if (filled or amber) else None,
                          outline=c if not (filled or amber) else None)
        if not (filled or amber):
            d.regular_polygon((px + 10 + i * 22, 92, 8), 4, rotation=45, outline=STEEL)


SET1 = [  # (slot word, piece or None, kind, tier, element colour, what an empty slot takes)
    ("Main hand", "Simple Sword", "Sword", "F", None, ""),
    ("Off hand", "Short Sword", "Sword", "F", None, ""),
    ("Augment socket", None, "Crystal", None, None, "Crystal, evolution or stone"),
    ("Augment socket", None, "Stone", None, None, "Stone"),
]


def slot_row(d, y, slot, piece, kind, tier, col, takes, focus):
    box = (LX0 + 10, y, LX1 - 10, y + 48)
    if focus:
        cut(d, box, TEAL, CYAN, 6)
    ib = (box[0] + 6, y + 4, box[0] + 46, y + 44)
    if piece:
        if not focus:
            cut(d, box, PLATE, STEEL, 6)
        cut(d, ib, BG, STEEL, 5)
        art(d, (ib[0] + 3, ib[1] + 3, ib[2] - 3, ib[3] - 3), kind, col)
        text(d, (ib[2] + 10, y + 6), piece, 16, True)
        tier_badge(d, ib[2] + 24 + d.textlength(piece, font=F(16, True)), y + 15, tier, 9)
        text(d, (ib[2] + 10, y + 27), kind, 13, fill=TXT if focus else DIM)
    else:
        if not focus:
            cut(d, box, BG, (70, 72, 80), 6)
        cut(d, ib, BG, OFF, 5)
        text(d, ((ib[0] + ib[2]) / 2, y + 24), "+", 20, True, OFF, anchor="mm")
        text(d, (ib[2] + 10, y + 6), slot, 16, True, TXT if focus else DIM)
        text(d, (ib[2] + 10, y + 27), takes, 13, fill=TXT if focus else OFF)
    if focus:
        d.polygon([(box[2] + 2, y + 16), (box[2] + 12, y + 24), (box[2] + 2, y + 32)], fill=CYAN)  # points at the grid


def entries(d, focus_slot):
    text(d, (LX0, TOP), "Weapon sets", 15, True, DIM)
    text(d, (LX1, TOP), "2 of 2  ·  1 adapter", 14, fill=OFF, anchor="ra")
    y = TOP + 24
    cut(d, (LX0, y, LX1, y + 266), PANEL, CYAN, 10)
    text(d, (LX0 + 16, y + 12), "Simple Sword + Short Sword", 16, True)
    cut(d, (LX1 - 68, y + 33, LX1 - 12, y + 51), BG, CYAN, 3)
    text(d, (LX1 - 40, y + 42), "ACTIVE", 11, True, CYAN, anchor="mm")
    text(d, (LX0 + 16, y + 34), "Dual swords  ·  3 skill slots", 13, fill=DIM)
    y += 60
    for i, s in enumerate(SET1):
        slot_row(d, y, *s, focus=i == focus_slot)
        y += 52
    y += 8
    # a closed entry: icon, name, type; its sockets as small icons
    for name, kind, tier, sub, col in (("Claymore", "Greatsword", "E", "Greatsword", None),
                                        ("Ember Ring", "Adapter", "D", "Adapter  ·  Fire", ELEMENT["Fire"])):
        cut(d, (LX0, y, LX1, y + 50), PLATE, STEEL, 8)
        ib = (LX0 + 8, y + 5, LX0 + 48, y + 45)
        cut(d, ib, BG, STEEL, 5)
        art(d, (ib[0] + 3, ib[1] + 3, ib[2] - 3, ib[3] - 3), kind, col)
        text(d, (LX0 + 58, y + 7), name, 15, True)
        tier_badge(d, LX0 + 72 + d.textlength(name, font=F(15, True)), y + 15, tier, 8)
        text(d, (LX0 + 58, y + 28), sub, 13, fill=DIM)
        for k in range(2):
            sb = (LX1 - 64 + k * 28, y + 13, LX1 - 40 + k * 28, y + 37)
            cut(d, sb, BG, OFF, 4)
        y += 56
    cut(d, (LX0, y, LX1, y + 36), BG, OFF, 8)
    text(d, (LX0 + 16, y + 18), "+  Add weapon set", 15, True, DIM, anchor="lm")
    text(d, (LX1 - 14, y + 18), "3/5 cells left", 13, True, CYAN, anchor="rm")


# ---------------------------------------------------------------- the grid
WEAPONS = [  # (name, kind, tier, count, worn, fits the Off hand of a Sword main hand)
    ("Short Sword", "Sword", "F", 1, True, True), ("Arming Sword", "Sword", "F", 2, False, True),
    ("Simple Sword", "Sword", "F", 1, True, True), ("Plain Shield", "Shield", "F", 1, False, True),
    ("Guard Shield", "Shield", "E", 1, False, True), ("Round Shield", "Shield", "F", 1, False, True),
    ("Claymore", "Greatsword", "E", 1, True, False), ("Longsword", "Greatsword", "E", 1, False, False),
    ("Knight Axe", "Greataxe", "F", 1, False, False), ("War Axe", "Greataxe", "F", 1, False, False),
    ("Falconer Bow", "Bow", "F", 1, False, False), ("Mechanical Bow", "Bow", "F", 1, False, False),
    ("Hunter Gun", "Gun", "F", 1, False, False), ("Hunter Gun II", "Gun", "F", 1, False, False),
]
FILTERS = ["All", "Sword", "Greatsword", "Greataxe", "Shield", "Bow", "Gun"]


def icon_filters(d, x, y, kinds, lit, label, size=36):
    """Filter chips as icons; the lit chip's word shows beside the row (icons rather than text)."""
    for i, k in enumerate(kinds):
        box = (x, y, x + size + 8, y + size)
        cut(d, box, TEAL if i == lit else PLATE, CYAN if i == lit else STEEL, 5)
        if k == "All":
            text(d, ((box[0] + box[2]) / 2, y + size / 2), "All", 12, True, TXT, anchor="mm")
        else:
            art(d, (box[0] + 6, y + 3, box[2] - 6, y + size - 3), k[0] if isinstance(k, tuple) else k,
                k[1] if isinstance(k, tuple) else None)
        x += size + 14
    text(d, (x + 2, y + size / 2), label, 14, True, DIM, anchor="lm")
    return x


def grid(d, box, items, cols, rows, cell, gap, focus_i, title_l, title_r, filters, filter_label, legend=True):
    gx0, gy0, gx1, gy1 = box
    panel(d, box)
    text(d, (gx0 + 18, gy0 + 14), title_l, 16, True)
    text(d, (gx1 - 16, gy0 + 16), title_r, 14, True, DIM, anchor="ra")
    icon_filters(d, gx0 + 18, gy0 + 44, filters, 0, filter_label)
    ty = gy0 + 98
    tx = gx0 + 18
    for i in range(cols * rows):
        r, c = divmod(i, cols)
        b = (tx + c * (cell + gap), ty + r * (cell + gap), tx + c * (cell + gap) + cell, ty + r * (cell + gap) + cell)
        if i >= len(items):
            empty_tile(d, b)
            continue
        it = items[i]
        tile(d, b, it["kind"], it.get("tier"), it.get("count", 1), it.get("col"), it.get("worn", False),
             it.get("locked", False), i == focus_i, it.get("picked", False))
    grid_bottom = ty + rows * (cell + gap) - gap
    scroll_bar(d, gx1 - 14, ty, grid_bottom, 0.0, 0.7)
    if legend:
        ly = gy1 - 22
        check_corner(d, gx0 + 18, ly - 9, 14)
        text(d, (gx0 + 38, ly), "worn", 13, fill=OFF, anchor="lm")
        lock(d, gx0 + 92, ly - 3)
        text(d, (gx0 + 104, ly), "can't go in this slot (why: in the details)", 13, fill=OFF, anchor="lm")
    return ty


def weapon_items():
    return [dict(name=n, kind=k, tier=t, count=c, worn=w, locked=not f) for n, k, t, c, w, f in WEAPONS]


# ---------------------------------------------------------------- the details panel
def stat_line(d, x0, x1, y, label, old, new, better):
    """Label, the worn value, an arrow, the new value; green up / red down (UIScreens: ups green, downs red)."""
    text(d, (x0, y), label, 15, fill=DIM)
    col = GOOD if better is True else (KILL if better is False else TXT)
    text(d, (x1, y), new, 15, True, col, anchor="ra")
    if old is not None and old != new:
        nx = x1 - d.textlength(new, font=F(15, True)) - 10
        text(d, (nx, y + 1), "→", 14, fill=OFF, anchor="ra")
        text(d, (nx - 22, y), old, 15, fill=OFF, anchor="ra")


def action_button(d, box, key, verb, primary=False, dim=False):
    cut(d, box, TEAL if primary else PLATE, CYAN if primary else STEEL, 7)
    cy = (box[1] + box[3]) / 2
    w = glyph(d, box[0] + 24, cy, key, 11)
    text(d, (box[0] + 24 + w / 2 + 10, cy), verb, 15, True, OFF if dim else TXT, anchor="lm")


def details_panel(d, box, pic_kind, pic_col, name, tier, type_line, vs, stats, sockets, blurb, buttons, note=None):
    x0, y0, x1, y1 = box
    panel(d, box)
    pic = (x0 + 18, y0 + 16, x1 - 18, y0 + 136)
    cut(d, pic, BG, STEEL, 10)
    art(d, ((pic[0] + pic[2]) / 2 - 60, pic[1] + 2, (pic[0] + pic[2]) / 2 + 60, pic[3] - 2), pic_kind, pic_col)
    y = y0 + 148
    text(d, (x0 + 20, y), name, 22, True)
    if tier:
        tier_badge(d, x0 + 38 + d.textlength(name, font=F(22, True)), y + 13, tier, 11)
    y += 32
    text(d, (x0 + 20, y), type_line, 14, fill=DIM)
    y += 28
    if vs:
        text(d, (x0 + 20, y), vs, 12, True, CYAN)
        y += 20
    for label, old, new, better in stats:
        stat_line(d, x0 + 20, x1 - 20, y, label, old, new, better)
        y += 23
    for s_col, s_text in sockets:
        y += 2
        if s_col:
            d.regular_polygon((x0 + 28, y + 9, 7), 6, fill=s_col)
            text(d, (x0 + 44, y + 9), s_text, 14, fill=TXT, anchor="lm")
        else:
            d.regular_polygon((x0 + 28, y + 9, 7), 4, rotation=45, outline=STEEL)
            text(d, (x0 + 44, y + 9), s_text, 14, fill=OFF, anchor="lm")
        y += 20
    y += 8
    d.line((x0 + 20, y, x1 - 20, y), fill=(52, 54, 62))
    y = wrap(d, (x0 + 20, y + 8), blurb, x1 - x0 - 40, 14, DIM, gap=5)
    if note:
        wrap(d, (x0 + 20, y + 2), note, x1 - x0 - 40, 13, WARN, gap=4)
    by = y1 - 16
    for row_buttons in reversed(buttons):
        n = len(row_buttons)
        w = (x1 - x0 - 36 - (n - 1) * 8) / n
        for k, (key, verb, primary, dim) in enumerate(row_buttons):
            bx = x0 + 18 + k * (w + 8)
            action_button(d, (bx, by - 38, bx + w, by), key, verb, primary, dim)
        by -= 46


def nav_footer(d, extra=()):
    footer(d, list(extra) + [("B", "Back")])


# ---------------------------------------------------------------- screens
def screen_1a():
    """Variant A: entries | grid | details with compare and buttons. Off hand lit, Arming Sword focused."""
    im, d = canvas("world")
    header(d, 1)
    entries(d, focus_slot=1)
    grid(d, (392, TOP, 852, BOTTOM), weapon_items(), 6, 5, 62, 10, 1, "Off hand", "14 weapons",
         FILTERS, "")
    details_panel(
        d, (872, TOP, 1232, BOTTOM), "Sword", None, "Arming Sword", "F", "Sword  ·  one hand  ·  pairs with a sword",
        "Compared with the worn Short Sword",
        [("Power", "40", "46", True), ("Speed", "Fast", "Normal", False), ("Scaling", "Body C", "Body C", None),
         ("Skill slots", "1", "1", None)],
        [(None, "Empty socket"), (None, "Empty socket")],
        "A plain soldier's blade. Two in your bag.",
        [[("A", "Equip to Off hand", True, False)], [("X", "Full compare", False, False), ("Y", "Sort: Kind", False, False)]])
    nav_footer(d, [("R3", "Details")])
    return im


def screen_1b():
    """Variant B: entries | a wide grid; the focused tile's details float beside it as a tooltip."""
    im, d = canvas("world")
    header(d, 1)
    entries(d, focus_slot=1)
    cell, gap, cols = 70, 10, 10
    gx0 = 392
    ty = grid(d, (gx0, TOP, 1232, BOTTOM), weapon_items(), cols, 5, cell, gap, 1, "Off hand", "14 weapons",
              FILTERS, "")
    # tooltip to the right of the focused tile (row 0, col 1): flips to the left near the screen edge
    fx1 = gx0 + 18 + 2 * (cell + gap) - gap
    tb = (fx1 + 14, ty - 6, fx1 + 14 + 330, ty + 330)
    d.polygon([(tb[0], ty + 26), (tb[0] - 12, ty + 35), (tb[0], ty + 44)], fill=CYAN)
    cut(d, tb, (26, 24, 38), CYAN, 10)
    x0, y0, x1, _ = tb
    text(d, (x0 + 16, y0 + 14), "Arming Sword", 20, True)
    tier_badge(d, x0 + 30 + d.textlength("Arming Sword", font=F(20, True)), y0 + 26, "F", 10)
    text(d, (x1 - 16, y0 + 18), "x2", 15, True, DIM, anchor="ra")
    text(d, (x0 + 16, y0 + 44), "Sword  ·  one hand  ·  pairs with a sword", 13, fill=DIM)
    text(d, (x0 + 16, y0 + 70), "Compared with the worn Short Sword", 12, True, CYAN)
    y = y0 + 92
    for label, old, new, better in [("Power", "40", "46", True), ("Speed", "Fast", "Normal", False),
                                    ("Scaling", "Body C", "Body C", None), ("Skill slots", "1", "1", None)]:
        stat_line(d, x0 + 16, x1 - 16, y, label, old, new, better)
        y += 23
    for _ in range(2):
        d.regular_polygon((x0 + 24, y + 11, 7), 4, rotation=45, outline=STEEL)
        text(d, (x0 + 40, y + 11), "Empty socket", 14, fill=OFF, anchor="lm")
        y += 21
    d.line((x0 + 16, y + 8, x1 - 16, y + 8), fill=(60, 58, 76))
    wrap(d, (x0 + 16, y + 16), "A plain soldier's blade. Two in your bag.", x1 - x0 - 32, 14, DIM)
    d.line((x0 + 16, tb[3] - 46, x1 - 16, tb[3] - 46), fill=(60, 58, 76))
    gx = x0 + 28
    for key, verb in (("A", "Equip to Off hand"), ("X", "Compare")):
        w = glyph(d, gx, tb[3] - 24, key, 11)
        text(d, (gx + w / 2 + 8, tb[3] - 24), verb, 14, True, TXT, anchor="lm")
        gx += w + 20 + d.textlength(verb, font=F(14, True))
    nav_footer(d, [("Y", "Sort: Kind"), ("R3", "Pin details")])
    return im


SOCKET_ITEMS = [  # (name, kind, tier, count, element, worn, locked)
    ("Garnet", "Crystal", "B", 1, "Fire", False, False), ("Garnet", "Crystal", "D", 3, "Fire", False, False),
    ("Sapphire", "Crystal", "D", 2, "Water", False, False), ("Topaz", "Crystal", "C", 1, "Lightning", False, False),
    ("Emerald", "Crystal", "E", 3, "Wind", False, False), ("Phoenix", "Evolution", "B", 1, "Fire", False, False),
    ("Whetstone", "Stone", "C", 4, None, False, False), ("Grindstone", "Stone", "E", 2, None, False, False),
    ("Onyx", "Crystal", "S", 1, "Darkness", False, True), ("Tidal", "Evolution", "B", 1, "Water", True, True),
    ("Ruby", "Crystal", "A", 1, "Fire", True, True),
]
SOCKET_FILTERS = ["All", ("Crystal", ELEMENT["Fire"]), ("Evolution", ELEMENT["Void"]), "Stone"]


def screen_2():
    """Variant A on a socket: crystals, evolutions, stones; the refused ones greyed, the reason in the panel."""
    im, d = canvas("world")
    header(d, 1, cells_used=2, cells_after=3)
    entries(d, focus_slot=2)
    items = [dict(name=n, kind=k, tier=t, count=c, col=ELEMENT.get(e), worn=w, locked=lk)
             for n, k, t, c, e, w, lk in SOCKET_ITEMS]
    grid(d, (392, TOP, 852, BOTTOM), items, 6, 5, 62, 10, 0, "Augment socket 1", "11 pieces",
         SOCKET_FILTERS, "")
    details_panel(
        d, (872, TOP, 1232, BOTTOM), "Crystal", ELEMENT["Fire"], "Garnet", "B", "Fire crystal  ·  shared by both swords",
        None,
        [("Fire Amp", None, "+2", None), ("Burn", None, "13% max HP / turn", None), ("Skill slots", "3", "8", True),
         ("Cells", "2 of 5", "3 of 5", None)],
        [],
        "Fire spells become your swords' skills. The other socket still takes a stone.",
        [[("A", "Put in socket 1", True, False)], [("X", "Compare", False, True), ("Y", "Sort: Tier", False, False)]])
    nav_footer(d, [("R3", "Details")])
    return im


def screen_2_locked():
    """The same grid with a refused piece focused: still focusable, the reason in orange (UI_Theme section 5)."""
    im, d = canvas("world")
    header(d, 1)
    entries(d, focus_slot=2)
    items = [dict(name=n, kind=k, tier=t, count=c, col=ELEMENT.get(e), worn=w, locked=lk)
             for n, k, t, c, e, w, lk in SOCKET_ITEMS]
    grid(d, (392, TOP, 852, BOTTOM), items, 6, 5, 62, 10, 8, "Augment socket 1", "11 pieces",
         SOCKET_FILTERS, "")
    details_panel(
        d, (872, TOP, 1232, BOTTOM), "Crystal", ELEMENT["Darkness"], "Onyx", "S", "Darkness crystal",
        None,
        [("Darkness Amp", None, "+5", None), ("Blind", None, "40% on hit", None), ("Skill slots", "3", "12", None)],
        [],
        "A crystal from the deep dark.",
        [[("A", "Can't put in", False, True)], [("X", "Compare", False, True), ("Y", "Sort: Tier", False, False)]],
        note="Only one S-tier crystal per loadout: the Iron Coat already holds one.")
    nav_footer(d, [("R3", "Details")])
    return im


if __name__ == "__main__":
    save(screen_1a(), OUT, "1a_equipment_grid_panel.png")
    save(screen_1b(), OUT, "1b_equipment_grid_tooltip.png")
    save(screen_2(), OUT, "2a_socket_crystal.png")
    save(screen_2_locked(), OUT, "2b_socket_refused.png")
    print("ok")
