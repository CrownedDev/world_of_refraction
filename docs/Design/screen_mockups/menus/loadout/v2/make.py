"""Loadout v2 (Crown, 2026-11-14; B-479 redraw for D-209). Three columns, as FF7 Rebirth's and Expedition 33's equip
screens: what is EQUIPPED on the left, a character frame in the middle (the live 3D model in the current gear; a
placeholder here), and what can go in the focused piece or socket on the right, with a compare (ups green, downs red).
Items that can't go in stay listed, greyed, with the reason. Details on R3. Tabs Weapons | Apparel | Skills on LB / RB.
Costs (D-206): a weapon or adapter itself 0; a crystal 1, an augment stone 1, an evolution 2 (in the crystal socket);
an evolution alone 2. Budgets: Generic 5 (rows of 3 and 2), Resonator 5 (one weapon, at most 5 adapters), Refractor 2
(one item). D-209: each Generic row holds weapon SETS (one weapon, or two one-handed weapons; a shield later), at most
one adapter per loadout in either row. A confirmed set shows as its own column; a row shows its free cells as a number
(no empty cells), and "Add gear" opens a window that builds a valid set. Apparel: 2 slots, HP / Defence / Amp + one
rolled bonus line (D-208; a pillar line is % of the pillar's value). Stat values are placeholders. Run: python make.py"""
import os
import sys

from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

RED = ROSE_LOW
L, LR = 48, 480            # left column
MID_L, MID_R = 500, 772     # character frame
LIST_X = 792                # right column
TOP = 134
FREE_W = 92                 # the row's free-cells box and Add gear button


def fit(d, s, size, bold, w):
    while d.textlength(s, font=F(size, bold)) > w and len(s) > 2:
        s = s[:-2] + "…"
    return s


def header(d, who, page, loadout):
    x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], who, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Weapons", "Apparel", "Skills"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)
    text(d, (1232, 36), loadout, 20, True, anchor="ra")
    text(d, (1232, 66), "Y  Saved loadouts", 15, fill=CYAN, anchor="ra")


def used_line(d, used, total, preview=None):
    text(d, (L, TOP), "Cells used", 17, True, DIM)
    if preview is None or preview == used:
        text(d, (L + 110, TOP), "%d of %d" % (used, total), 18, True)
    else:
        text(d, (L + 110, TOP), "%d → %d of %d" % (used, preview, total), 18, True, WARN if preview > used else GOOD)


def card(d, box, title, pieces, cost, active, focus):
    """One confirmed set (or adapter) as a column. pieces: (name, el, sockets); sockets = element colours or None.
    focus: None, "card" or a piece index."""
    x0, y0, x1, y1 = box
    on = focus is not None
    cut(d, box, PLATE, CYAN if (on or active) else STEEL, 8)
    compact = x1 - x0 < 120
    text(d, (x0 + 8, y0 + 6), fit(d, title, 13, True, x1 - x0 - 16), 13, True, CYAN if on else DIM)
    py = y0 + 26
    for i, (name, el, socks) in enumerate(pieces):
        pf = focus == i
        ph = 46 if compact else 34
        if pf:
            cut(d, (x0 + 4, py - 2, x1 - 4, py + ph - 4), TEAL, CYAN, 5)
        icon(d, (x0 + 8, py, x0 + 34, py + 26), ELEMENT[el])
        if compact:
            text(d, (x0 + 40, py + 1), "", 1)
            for j, s in enumerate(socks):
                sx = x0 + 40 + j * 18
                icon(d, (sx, py + 6, sx + 14, py + 20), s, empty=s is None)
            text(d, (x0 + 8, py + 29), fit(d, name, 11, True, x1 - x0 - 14), 11, True)
        else:
            alone = len(pieces) == 1  # room under the name: sockets go there, the name gets the width
            text(d, (x0 + 42, py + (0 if alone else 5)), fit(d, name, 13, True, x1 - x0 - (50 if alone else 92)), 13, True)
            for j, s in enumerate(socks):
                sx = (x0 + 42 if alone else x1 - 46) + j * 19
                sy = py + 20 if alone else py + 6
                icon(d, (sx, sy, sx + 15, sy + 15), s, empty=s is None)
        py += ph
    text(d, (x1 - 8, y1 - 6), "%d cell%s" % (cost, "" if cost == 1 else "s"), 12, True, TXT, anchor="rd")
    if active:
        tw = int(d.textlength("ACTIVE", font=F(10, True))) + 10
        cut(d, (x1 - tw - 6, y0 - 8, x1 - 6, y0 + 7), BG, CYAN, 4)
        text(d, (x1 - 6 - tw / 2, y0 - 6), "ACTIVE", 10, True, CYAN, anchor="ma")


def set_row(d, y, label, total, sets, focus=None, add=True, h=128):
    """A row: its sets as columns, then the free cells as a number and the Add gear button. sets: (title, pieces, cost,
    active, width). focus: (set index, piece index or "card"), or "add"."""
    text(d, (L, y), label, 14, True, DIM)
    y += 22
    cut(d, (L, y, LR, y + h), BG, STEEL, 10)
    x = L + 8
    for i, (t, pieces, cost, active, w) in enumerate(sets):
        f = focus[1] if isinstance(focus, tuple) and focus[0] == i else None
        card(d, (x, y + 10, x + w, y + h - 8), t, pieces, cost, active, f)
        x += w + 8
    free = total - sum(s[2] for s in sets)
    fx = LR - FREE_W - 8
    text(d, (fx + FREE_W / 2, y + 8), str(free), 30, True, TXT if free else OFF, anchor="ma")
    text(d, (fx + FREE_W / 2, y + 46), "free of %d" % total, 12, fill=DIM, anchor="ma")
    if add:
        on = focus == "add"
        cut(d, (fx, y + h - 50, fx + FREE_W, y + h - 10), TEAL if on else PLATE, CYAN if on else STEEL, 6)
        text(d, (fx + FREE_W / 2, y + h - 30), "+ Add gear", 13, True, TXT, anchor="mm")
    else:
        text(d, (fx + FREE_W / 2, y + h - 30), "one item", 12, fill=DIM, anchor="mm")
    return y + h + 12


def sockets_strip(d, y, owner, socks, focus=None):
    """The focused piece's sockets, large: (kind, name, el). focus: socket index."""
    cut(d, (L, y, LR, y + 64), BG, STEEL, 8)
    text(d, (L + 12, y + 6), owner + "'s sockets", 13, True, DIM)
    for i, (kind, name, el) in enumerate(socks):
        sx = L + 12 + i * 208
        if focus == i:
            cut(d, (sx - 4, y + 24, sx + 196, y + 60), TEAL, CYAN, 6)
        icon(d, (sx, y + 28, sx + 28, y + 56), ELEMENT[el] if name else None, empty=not name)
        text(d, (sx + 36, y + 27), name or "empty", 14, True, TXT if name or focus == i else OFF)
        text(d, (sx + 36, y + 44), kind, 12, fill=TXT if focus == i else DIM)
    return y + 76


def seat(d, y, name, el):
    cut(d, (L, y, LR, y + 52), PLATE, STEEL, 8)
    icon(d, (L + 10, y + 10, L + 42, y + 42), ELEMENT[el] if name else None, empty=not name)
    text(d, (L + 54, y + 7), "Own evolution seat", 12, fill=DIM)
    text(d, (L + 54, y + 24), name or "empty", 16, True, TXT if name else OFF)
    text(d, (LR - 12, y + 26), "on the character", 12, fill=DIM, anchor="rm")
    return y + 64


def frame(d, name, cls, gear):
    """The character frame: a live 3D render in the current gear (placeholder silhouette)."""
    panel(d, (MID_L, TOP, MID_R, 654))
    cx = (MID_L + MID_R) // 2
    d.ellipse((cx - 70, 600, cx + 70, 624), fill=TRACK)
    d.ellipse((cx - 30, 190, cx + 30, 254), fill=STEEL)
    d.polygon([(cx - 26, 258), (cx + 26, 258), (cx + 64, 300), (cx + 52, 450), (cx + 30, 450), (cx + 26, 610),
               (cx + 4, 610), (cx, 470), (cx - 4, 610), (cx - 26, 610), (cx - 30, 450), (cx - 52, 450),
               (cx - 64, 300)], fill=STEEL)
    text(d, (cx, 148), name, 20, True, anchor="ma")
    text(d, (cx, 172), cls, 13, fill=DIM, anchor="ma")
    text(d, (cx, 630), fit(d, gear, 12, False, MID_R - MID_L - 16), 12, fill=DIM, anchor="ma")


def innate(d, y, element, used, schools):
    """A Refractor's innate spell pool, the star of its tab: element, the 24-point budget, spells per school (6 cap)."""
    panel(d, (L, y, LR, 654), focus=False)
    icon(d, (L + 14, y + 14, L + 62, y + 62), ELEMENT[element])
    text(d, (L + 74, y + 12), "Innate spells", 20, True)
    text(d, (L + 74, y + 40), "Element: " + element, 14, fill=DIM)
    text(d, (LR - 14, y + 16), "%d of 24" % used, 18, True, anchor="ra")
    text(d, (LR - 14, y + 40), "points", 12, fill=DIM, anchor="ra")
    for i, (school, n, names) in enumerate(schools):
        sy = y + 76 + i * 26
        text(d, (L + 14, sy), school, 14, True, TXT if n else OFF)
        for p in range(6):
            px = L + 136 + p * 14
            cut(d, (px, sy + 3, px + 10, sy + 15), ELEMENT[element] if p < n else TRACK, None, 2)
        text(d, (L + 228, sy + 1), fit(d, names, 12, False, LR - L - 242), 12, fill=DIM)
    text(d, (L + 14, 632), "Edit them on the Skills tab.", 12, fill=DIM)


def item_rows(d, x0, x1, y, rows, focus, step=55):
    """List rows: (name, el, tier, sub, right, state); state None, "worn" or a reason it can't go in (greyed)."""
    for i, (n, el, t, sub, right, state) in enumerate(rows):
        greyed = state not in (None, "worn")
        f = i == focus
        cut(d, (x0, y, x1, y + 50), TEAL if f else (BG if greyed else PLATE), CYAN if f else (OFF if greyed else STEEL), 8)
        icon(d, (x0 + 8, y + 7, x0 + 44, y + 43), ELEMENT[el])
        text(d, (x0 + 54, y + 5), n, 16, True, OFF if greyed else TXT)
        tier_badge(d, x0 + 54 + int(d.textlength(n, font=F(16, True))) + 15, y + 14, t, 10)
        text(d, (x0 + 54, y + 28), fit(d, state if greyed else sub, 12, False, x1 - x0 - 140), 12,
             fill=WARN if greyed else (TXT if f else DIM))
        if state == "worn":
            text(d, (x1 - 12, y + 8), "ON NOW", 11, True, CYAN, anchor="ra")
        text(d, (x1 - 12, y + 28), right, 13, True, OFF if greyed else TXT, anchor="ra")
        y += step
    return y


def choices(d, heading, rows, focus, compare):
    """The right column: the list, then the compare for the focused row: [(label, from, to, good)]."""
    panel(d, (LIST_X, TOP, 1232, 654), focus=True)
    text(d, (LIST_X + 16, TOP + 10), heading, 15, True, DIM)
    y = item_rows(d, LIST_X + 10, 1222, TOP + 38, rows, focus)
    d.line((LIST_X + 16, y + 2, 1216, y + 2), fill=STEEL)
    text(d, (LIST_X + 16, y + 10), "If you equip " + rows[focus][0], 14, True, DIM)
    y += 36
    for lab, a, b, good in compare:
        col = TXT if good is None else (GOOD if good else RED)
        arrow = "" if good is None else (" ▲" if good else " ▼")
        text(d, (LIST_X + 16, y), lab, 14, fill=DIM)
        text(d, (LIST_X + 168, y), a, 14)
        text(d, (LIST_X + 284, y), "→", 14, fill=DIM)
        text(d, (LIST_X + 304, y), b + arrow, 14, True, col)
        y += 24


FOOT = [("A", "Equip"), ("X", "Unequip"), ("R3", "Details"), ("B", "Back")]
FIRE, EARTH, WATER, LIGHTNING = (ELEMENT[e] for e in ("Fire", "Earth", "Water", "Lightning"))

# The Generic's tab: a dual set (two one-handed swords) in the Primary; a hammer and the one adapter in the Secondary.
DUAL = ("Sword + Sword", [("Ashen Blade", "Fire", [FIRE, None]), ("Cinder Edge", "None", [None, None])], 1, True, 224)
HAMMER = ("Hammer", [("Quarry Maul", "Earth", [EARTH, None])], 1, False, 156)
RING = ("Adapter", [("Storm Ring", "Lightning", [LIGHTNING, None])], 1, False, 156)
GEAR_G = "Ashen Blade + Cinder Edge · Iron Coat"


def generic_rows(d, focus_p=None, focus_s=None, dual=DUAL):
    y = set_row(d, 172, "Primary  ·  3 cells  ·  sets", 3, [dual], focus_p)
    return set_row(d, y, "Secondary  ·  2 cells  ·  sets", 2, [HAMMER, RING], focus_s)


# ---- 1: Garrick (Generic): the off-hand sword of his dual set is focused; the right lists one-handed weapons.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 3, 5, preview=4)
y = generic_rows(d, focus_p=(0, 1))
y = sockets_strip(d, y, "Cinder Edge", [("Crystal", None, None), ("Stone", None, None)])
seat(d, y, "Ironheart", "Earth")
frame(d, "Garrick", "Generic", GEAR_G)
choices(d, "Off hand  ·  swords that pair", [
    ("Cinder Edge", "None", "C", "Sword · dual wield", "0 cells", "worn"),
    ("Frost Fang", "Water", "C", "Sword · Sapphire", "1 cell", None),
    ("Gale Dagger", "Wind", "D", "", "", "Pairs only with a dagger"),
    ("Greatblade", "None", "B", "", "", "Can't be dual wielded"),
    ("Night Scythe", "Darkness", "A", "", "", "Can't be dual wielded")], 1, [
    ("Cells", "3 of 5", "4 of 5", None), ("Primary free", "2", "1", None), ("Power", "42 + 30", "42 + 34", True),
    ("Elements", "Fire", "Fire, Water", True), ("Abilities", "Cleave, Whirl…", "+ Frost Bite…", True)])
footer(d, FOOT)
save(im, OUT, "1_weapons_generic.png")


def window(base, title, free, hints):
    """The Add gear window over the dimmed screen."""
    im = Image.blend(base, Image.new("RGB", base.size, BG), 0.72)
    d = ImageDraw.Draw(im)
    panel(d, (120, 60, 1160, 660), focus=True)
    text(d, (146, 78), title, 22, True)
    text(d, (1134, 80), free, 18, True, CYAN, anchor="ra")
    d.line((146, 116, 1134, 116), fill=STEEL)
    footer(d, hints, y=632, right=1140)
    return im, d


def build_slot(d, y, label, name, el, sub, focus=False, x0=146, x1=560):
    """A piece of the set being built; name None = still to pick."""
    text(d, (x0, y), label, 13, True, DIM)
    y += 20
    cut(d, (x0, y, x1, y + 52), TEAL if focus else (PLATE if name else BG), CYAN if focus else (STEEL if name else OFF), 8)
    icon(d, (x0 + 10, y + 8, x0 + 46, y + 44), ELEMENT[el] if name else None, empty=not name)
    text(d, (x0 + 56, y + 7), name or ("choosing…" if focus else "—"), 16, True, TXT if name or focus else OFF)
    text(d, (x0 + 56, y + 29), sub, 12, fill=TXT if focus else DIM)
    return y + 64


# ---- 1a: Add gear on an empty Primary: building a dual set. The main sword and its evolution are picked; the off hand
#          is being picked. Pieces that don't fit stay listed, greyed, with the reason.
base, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 2, 5)
y = set_row(d, 172, "Primary  ·  3 cells  ·  sets", 3, [], "add")
set_row(d, y, "Secondary  ·  2 cells  ·  sets", 2, [HAMMER, RING])
frame(d, "Garrick", "Generic", "Quarry Maul · Iron Coat")
im, d = window(base, "Add gear  ·  Primary", "3 free",
               [("A", "Pick"), ("X", "Remove piece"), ("Y", "Confirm set"), ("B", "Cancel")])
text(d, (146, 128), "A set: one weapon, or a dual-wield pair of one type.", 13, fill=DIM)
y = build_slot(d, 156, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword · dual wield · its stance leads")
y = build_slot(d, y, "Main hand socket", "Phoenix", "Fire", "Evolution · 2 cells")
y = build_slot(d, y, "Off hand", None, None, "A sword that dual wields, or leave it empty", focus=True)
y = build_slot(d, y, "Off hand socket", None, None, "Crystal, stone or evolution")
cut(d, (146, y + 6, 560, y + 50), BG, STEEL, 8)
text(d, (160, y + 16), "Sword + Sword", 16, True)
text(d, (546, y + 16), "2 cells  ·  1 free left", 15, True, CYAN, anchor="ra")
text(d, (590, 156 - 28), "Off hand  ·  swords that pair", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Cinder Edge", "None", "C", "Sword · dual wield", "0 cells", None),
    ("Frost Fang", "Water", "C", "Sword · Sapphire", "1 cell", None),
    ("Tempest Sabre", "Lightning", "A", "", "", "Evolved: needs 2 cells, 1 free"),
    ("Gale Dagger", "Wind", "D", "", "", "Pairs only with a dagger"),
    ("Quarry Maul", "Earth", "C", "", "", "Can't be dual wielded"),
    ("Storm Ring", "Lightning", "C", "", "", "An adapter: one per loadout, already in Secondary"),
    ("Ashen Blade", "Fire", "B", "", "", "Already the main hand")], 0)
save(im, OUT, "1a_add_gear_dual.png")

# ---- 1c: Add gear for an adapter, in the Secondary (Quarry Maul there, 1 free; no adapter yet in this loadout).
#          Left: a ready-made adapter. Right: an empty adapter plus the crystal to put in it.
base, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 2, 5)
y = set_row(d, 172, "Primary  ·  3 cells  ·  sets", 3, [DUAL])
set_row(d, y, "Secondary  ·  2 cells  ·  sets", 2, [HAMMER], "add")
frame(d, "Garrick", "Generic", GEAR_G)
im, d = window(base, "Add gear  ·  Secondary  ·  adapter", "1 free",
               [("A", "Pick"), ("LB", "Ready-made"), ("RB", "Empty + crystal"), ("Y", "Confirm"), ("B", "Cancel")])
text(d, (146, 128), "One adapter per loadout. It casts its spells; its crystal sets their element.", 13, fill=DIM)
cut(d, (140, 150, 620, 616), BG, STEEL, 10)
text(d, (156, 160), "Ready-made", 17, True)
text(d, (604, 162), "with its crystal", 12, fill=DIM, anchor="ra")
item_rows(d, 150, 610, 190, [
    ("Ember Ring", "Fire", "C", "Ring · Garnet", "1 cell", None),
    ("Storm Ring", "Lightning", "C", "Ring · Amber", "1 cell", None),
    ("Tide Ring", "Water", "B", "Ring · Sapphire + stone", "", "Needs 2 cells, 1 free"),
    ("Pyre Ring", "Fire", "B", "Ring · evolved", "", "Evolved: needs 2 cells, 1 free")], None)
cut(d, (636, 150, 1144, 616), BG, CYAN, 10)
text(d, (652, 160), "Empty + crystal", 17, True, CYAN)
y = build_slot(d, 190, "Adapter", "Plain Ring", "None", "Empty ring · 0 cells", x0=652, x1=1128)
text(d, (652, y), "Crystal for it", 13, True, DIM)
item_rows(d, 646, 1134, y + 20, [
    ("Garnet", "Fire", "B", "Fire crystal", "+1 cell", None),
    ("Sapphire", "Water", "D", "Water crystal", "+1 cell", None),
    ("Onyx", "Darkness", "S", "", "", "S-tier: one per loadout (on Iron Coat)")], 1)
text(d, (652, 588), "Plain Ring + Sapphire  ·  1 cell  ·  0 free left", 14, True, TXT)
save(im, OUT, "1c_add_gear_adapter.png")

# ---- 2: Sera (Refractor): one item, 2 cells; the gear small, the innate spell pool the star.
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
used_line(d, 1, 2)
y = set_row(d, 172, "Gear  ·  2 cells  ·  one item", 2,
            [("Staff", [("Cinder Staff", "Fire", [FIRE, None])], 1, False, 224)], (0, 0), add=False, h=96)
y = seat(d, y, None, None)
innate(d, y + 4, "Fire", 18, [
    ("Destruction", 3, "Flame Lance, Ember, Scorch"), ("Conjuration", 0, ""),
    ("Enhancement", 1, "Kindle"), ("Restoration", 2, "Warm Light, Cauterise"),
    ("Impairment", 1, "Smoulder"), ("Manipulation", 0, "")])
frame(d, "Sera", "Refractor  ·  Fire", "Cinder Staff · Ember Cloak")
choices(d, "Fits the gear slot  ·  2 cells", [
    ("Cinder Staff", "Fire", "C", "Staff · Garnet", "1 cell", "worn"),
    ("Tidal Pike", "Water", "C", "Spear · Sapphire", "1 cell", None),
    ("Ember Ring", "Fire", "B", "Adapter · evolved", "2 cells", None),
    ("Phoenix Heart", "Fire", "B", "Evolution on its own", "2 cells", None),
    ("Pyre Pike", "Fire", "A", "", "", "Evolution + stone: needs 3 cells, 2 here")], 1, [
    ("Cells", "1 of 2", "1 of 2", None), ("Power", "28", "34", True),
    ("Elements she casts", "Fire", "Fire, Water", True), ("Innate spells", "18 points", "18 points", None)])
footer(d, FOOT)
save(im, OUT, "2_weapons_refractor.png")

# ---- 3: Bram (Resonator): one row of 5 cells: the weapon, then up to 5 rings; he casts the active ring.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
used_line(d, 4, 5)
y = set_row(d, 172, "Weapon, then rings  ·  5 cells  ·  up to 5 rings", 5, [
    ("Fists", [("Cinder Fists", "None", [None, None])], 0, False, 72),
    ("Ring", [("Ember Ring", "Fire", [FIRE, None])], 1, True, 72),
    ("Ring", [("Storm Ring", "Lightning", [LIGHTNING, None])], 1, False, 72),
    ("Ring", [("Tide Ring", "Water", [WATER, None])], 2, False, 76)], (1, 0), h=110)
y = sockets_strip(d, y, "Ember Ring", [("Crystal", "Garnet", "Fire"), ("Stone", None, None)])
seat(d, y, "Emberheart", "Fire")
frame(d, "Bram", "Resonator", "Cinder Fists · Ember Ring")
choices(d, "Rings  ·  6 owned", [
    ("Ember Ring", "Fire", "C", "Ring · Garnet", "1 cell", "worn"),
    ("Gale Band", "Wind", "C", "Ring · Emerald", "1 cell", None),
    ("Frost Ring", "Water", "D", "Ring · empty", "0 cells", None),
    ("Pyre Ring", "Fire", "B", "Ring · evolved", "2 cells", None),
    ("Storm Ring", "Lightning", "C", "", "", "Already in the row")], 1, [
    ("Cells", "4 of 5", "4 of 5", None), ("Spells", "Flame Lance…", "Gust, Updraft…", None),
    ("Durability", "64 / 80", "80 / 80", True)])
footer(d, FOOT)
save(im, OUT, "3_weapons_resonator.png")

# ---- 4: a crystal socket focused (the dual set's off-hand sword, empty): the crystals that fit; a crystal adds a cell.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 3, 5, preview=4)
y = generic_rows(d, focus_p=(0, 1))
y = sockets_strip(d, y, "Cinder Edge", [("Crystal", None, None), ("Stone", None, None)], focus=0)
seat(d, y, "Ironheart", "Earth")
frame(d, "Garrick", "Generic", GEAR_G)
choices(d, "Crystals that fit  ·  14 owned", [
    ("Garnet", "Fire", "B", "Fire crystal", "+1 cell", None),
    ("Citrine", "Earth", "C", "Earth crystal", "+1 cell", None),
    ("Sapphire", "Water", "D", "Water crystal", "+1 cell", None),
    ("Onyx", "Darkness", "S", "", "", "S-tier: one per loadout (on Iron Coat)"),
    ("Quartz", "None", "E", "Crystal", "+1 cell", None)], 0, [
    ("Cells", "3 of 5", "4 of 5", None), ("Primary free", "2", "1", None),
    ("Elemental abilities", "off", "on", True), ("Fire sources", "1", "2 (they stack)", True)])
footer(d, [("A", "Socket"), ("X", "Empty it"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "4_crystal_socket.png")


def apparel(d, y, name, el, lines, bonus, sock, focus=False):
    cut(d, (L, y, LR, y + 112), TEAL if focus else PLATE, CYAN if focus else STEEL, 10)
    icon(d, (L + 10, y + 10, L + 66, y + 66), ELEMENT[el])
    text(d, (L + 78, y + 10), name, 19, True)
    text(d, (L + 78, y + 38), lines, 14, fill=TXT if focus else DIM)
    text(d, (L + 78, y + 60), bonus, 14, True, CYAN)
    icon(d, (L + 10, y + 76, L + 36, y + 102), ELEMENT[sock[1]] if sock[0] else None, empty=not sock[0])
    text(d, (L + 44, y + 80), (sock[0] or "empty") + "  ·  Crystal / evolution", 13, fill=DIM)


# ---- 5: Apparel tab: two slots, no budget. Each piece: HP / Defence / Amp + one rolled bonus (a sub-stat, or a pillar
#         as a % of its value, never points, so it never meets a skill requirement).
im, d = canvas()
header(d, 1, 1, "Bruiser  ·  2 of 5")
text(d, (L, TOP), "Apparel doesn't use the cells.", 16, fill=DIM)
apparel(d, 168, "Iron Coat", "Darkness", "HP 40 · Def 14 · Amp +2", "+5% Body", ("Onyx", "Darkness"), focus=True)
apparel(d, 292, "Shade Hood", "None", "HP 25 · Def 6 · Amp +1", "+4% Crit Rate", (None, None))
frame(d, "Garrick", "Generic", GEAR_G)
choices(d, "Apparel that fits  ·  5 owned", [
    ("Iron Coat", "Darkness", "C", "HP 40 · Def 14 · Amp +2 · +5% Body", "", "worn"),
    ("Ember Cloak", "Fire", "C", "HP 30 · Def 10 · Amp +4 · +6% Crit Damage", "", None),
    ("Warden Plate", "None", "B", "HP 55 · Def 20 · Amp +0 · +5% Efficiency", "", None),
    ("Shade Hood", "None", "D", "", "", "In apparel slot 2"),
    ("Prisma's Mantle", "Void", "B", "", "", "Bound to Amethyst Prisma: unlock that Spirit")], 1, [
    ("HP", "40", "30", False), ("Defence", "14", "10", False), ("Amp", "+2", "+4", True),
    ("Body", "+5%", "—", False), ("Crit Damage", "—", "+6%", True), ("Resistance", "Darkness", "Fire", None)])
footer(d, FOOT)
save(im, OUT, "5_apparel.png")
print("ok")
