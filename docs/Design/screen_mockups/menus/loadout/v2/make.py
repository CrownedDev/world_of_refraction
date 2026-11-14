"""Loadout v2 (Crown, 2026-11-14; B-479 redraw for D-209 / D-210 and Crown's notes). Two columns: what is EQUIPPED on the
left, the owned items that fit on the right with a compare (ups green, downs red, as Expedition 33's and Persona 5's
equip screens). Items that can't go in stay listed, greyed, with the reason. Tabs Weapons | Skills | Apparel | Cosmetics
(later) on LB / RB. Main keys: A Edit, X Details (an entry's or the evolution's stats and attachments), Y Options
(Saved loadouts, Remove all gear, Help, later Cosmetics shortcuts), B Back; help text is not on screen.
D-210: the Weapons tab is a list of ENTRIES with an empty "Add weapon" frame at the bottom showing the cells left
("1/5 left"). The Add gear window asks Weapon, Adapter or Evolution, then Main hand, Off hand, Augment socket 1 and 2
(shared by the entry: a crystal or evolution + a stone, or two stones). Removing happens there: on the row of the
piece already in the focused slot, A reads Remove. The
character's own evolution is one compact line; it costs 2 cells and confirming it asks first. Entries per class:
Generic 2 weapon + 1 adapter, Resonator 1 weapon + up to 5 adapters, Refractor 1. Evolutions: a Resonator only on
adapters, a Generic only on weapons. Costs (D-206): gear 0, crystal 1, stone 1, evolution 2. Sockets show icons only:
a crystal is a hexagon, a stone a round pebble, an evolution a four-point star. Stat values are placeholders.
Run: python make.py"""
import os
import sys

from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

RED = ROSE_LOW
L, LR = 48, 620             # left column
LIST_X = 650                # right column
TOP = 134
STONE = (150, 140, 128)     # a plain stone's colour when it has no element


def fit(d, s, size, bold, w):
    while d.textlength(s, font=F(size, bold)) > w and len(s) > 2:
        s = s[:-2] + "…"
    return s


def header(d, who, page, loadout):
    x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], who, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Weapons", "Skills", "Apparel"], page, lead="LB")
    w = int(d.textlength("Cosmetics", font=F(18, True))) + 28
    cut(d, (x, 78, x + w, 112), BG, OFF, 8)
    text(d, (x + 14, 85), "Cosmetics", 18, True, OFF)
    text(d, (x + w + 6, 82), "later", 11, fill=OFF)
    text(d, (x + w + 44, 85), "RB", 16, True, CYAN)
    text(d, (1232, 36), loadout, 20, True, anchor="ra")


def socket_icon(d, box, s):
    """An augment socket, icon only. s: (kind, el) with kind crystal / stone / evolution, or None for empty."""
    if s is None:
        cut(d, box, BG, OFF, 5)
        return
    kind, el = s
    x0, y0, x1, y1 = box
    cut(d, box, PLATE, STEEL, 5)
    cx, cy, r = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) * 0.3
    col = ELEMENT[el] if el else STONE
    if kind == "crystal":
        d.regular_polygon((cx, cy, r), 6, fill=col)
    elif kind == "stone":
        d.ellipse((cx - r * 1.1, cy - r * 0.8, cx + r * 1.1, cy + r * 0.8), fill=col)
    else:
        d.polygon([(cx, cy - r * 1.3), (cx + r * 0.4, cy - r * 0.4), (cx + r * 1.3, cy), (cx + r * 0.4, cy + r * 0.4),
                   (cx, cy + r * 1.3), (cx - r * 0.4, cy + r * 0.4), (cx - r * 1.3, cy), (cx - r * 0.4, cy - r * 0.4)],
                  fill=col)


def used_line(d, used, total, note=""):
    text(d, (L, TOP), "Cells used", 17, True, DIM)
    text(d, (L + 110, TOP), "%d of %d" % (used, total), 18, True)
    text(d, (LR, TOP + 3), note, 13, fill=DIM, anchor="ra")


def evo_line(d, y, name, el, focus=False):
    """The character's own evolution, one compact line; it costs 2 cells."""
    cut(d, (L, y, LR, y + 40), TEAL if focus else PLATE, CYAN if focus else STEEL, 6)
    socket_icon(d, (L + 8, y + 6, L + 36, y + 34), ("evolution", el) if name else None)
    text(d, (L + 46, y + 10), name or "—", 15, True, TXT if name else OFF)
    return y + 50


def entry(d, y, title, pieces, sockets, cost, active=False, focus=False, h=62):
    """One confirmed entry: its weapons, its two shared sockets (icons only) and its cells."""
    cut(d, (L, y, LR, y + h), TEAL if focus else PLATE, CYAN if (focus or active) else STEEL, 8)
    top = y + (h - 32 - 8 * (len(pieces) - 1)) / 2
    for i, (n, el) in enumerate(pieces):
        icon(d, (L + 8 + i * 22, top + i * 8, L + 40 + i * 22, top + 32 + i * 8), ELEMENT[el])
    tx = L + 40 + 22 * (len(pieces) - 1) + 14
    title = " + ".join(p[0] for p in pieces)  # names only; types and stats live in X Details
    text(d, (tx, y + h / 2 - 10), title, 16, True)
    for i, s in enumerate(sockets):
        sx = LR - 94 + i * 44  # no cell number: the filled sockets show the cost
        socket_icon(d, (sx, y + 13, sx + 36, y + 49), s)
    if active:
        tw = int(d.textlength("ACTIVE", font=F(10, True))) + 10
        tx2 = tx + int(d.textlength(title, font=F(16, True))) + 10
        cut(d, (tx2, y + h / 2 - 8, tx2 + tw, y + h / 2 + 8), BG, CYAN, 4)
        text(d, (tx2 + tw / 2, y + h / 2 - 6), "ACTIVE", 10, True, CYAN, anchor="ma")
    return y + h + 8


def add_frame(d, y, left, total, focus=False):
    """The empty frame at the bottom of the entries."""
    cut(d, (L, y, LR, y + 44), TEAL if focus else BG, CYAN if focus else OFF, 8)
    text(d, (L + 16, y + 12), "+  Add weapon", 16, True, TXT if focus else DIM)
    text(d, (LR - 14, y + 12), "%d/%d left" % (left, total), 16, True, CYAN if left else OFF, anchor="ra")
    return y + 54


def item_rows(d, x0, x1, y, rows, focus, step=55):
    """List rows: (name, el, tier, sub, right, state); state None, "worn" or a reason it can't go in (greyed)."""
    for i, (n, el, t, sub, right, state) in enumerate(rows):
        greyed = state not in (None, "worn")
        f = i == focus
        cut(d, (x0, y, x1, y + 50), TEAL if f else (BG if greyed else PLATE), CYAN if f else (OFF if greyed else STEEL), 8)
        if isinstance(el, tuple):  # a socket item: its shape says crystal / stone / evolution
            socket_icon(d, (x0 + 8, y + 7, x0 + 44, y + 43), el)
        else:
            icon(d, (x0 + 8, y + 7, x0 + 44, y + 43), ELEMENT[el])
        text(d, (x0 + 54, y + 5), n, 16, True, OFF if greyed else TXT)
        tier_badge(d, x0 + 54 + int(d.textlength(n, font=F(16, True))) + 15, y + 14, t, 10)
        text(d, (x0 + 54, y + 28), fit(d, state if greyed else sub, 12, False, x1 - x0 - 120), 12,
             fill=WARN if greyed else (TXT if f else DIM))
        if state == "worn":
            text(d, (x1 - 12, y + 8), "ON NOW", 11, True, CYAN, anchor="ra")
        text(d, (x1 - 12, y + 28), right, 13, True, OFF if greyed else TXT, anchor="ra")
        y += step
    return y


def choices(d, heading, rows, focus, compare):
    """The right column: the list, then the compare for the focused row: [(label, from, to, good)]."""
    panel(d, (LIST_X, TOP, 1232, 654), focus=True)
    text(d, (LIST_X + 18, TOP + 10), heading, 16, True, DIM)
    y = item_rows(d, LIST_X + 12, 1220, TOP + 40, rows, focus, step=56)
    d.line((LIST_X + 18, y + 4, 1214, y + 4), fill=STEEL)
    text(d, (LIST_X + 18, y + 14), "If you equip " + rows[focus][0], 15, True, DIM)
    y += 42
    for lab, a, b, good in compare:
        col = TXT if good is None else (GOOD if good else RED)
        arrow = "" if good is None else ("  ▲" if good else "  ▼")
        text(d, (LIST_X + 18, y), lab, 15, fill=DIM)
        text(d, (LIST_X + 210, y), a, 15)
        text(d, (LIST_X + 340, y), "→", 15, fill=DIM)
        text(d, (LIST_X + 366, y), b + arrow, 15, True, col)
        y += 26


FOOT = [("A", "Edit"), ("X", "Details"), ("Y", "Options"), ("B", "Back")]

# ---- 1: Garrick (Generic): his evolution, two weapon entries and the Add weapon frame (1/5 left; the adapter entry
#         is the one left). The dual entry is focused (A Edit; on the Add weapon frame A reads Equip); the right lists swords that could take its place.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 4, 5, note="2 of 2 weapon entries · 0 of 1 adapter")
y = evo_line(d, 168, "Ironheart", "Earth")
y = entry(d, y, "Sword + Sword", [("Ashen Blade", "Fire"), ("Cinder Edge", "None")],
          [("crystal", "Fire"), None], 1, active=True, focus=True)
y = entry(d, y, "Hammer", [("Quarry Maul", "Earth")], [("stone", None), None], 1)
add_frame(d, y, 1, 5)
choices(d, "Swords that fit this entry  ·  A Edit to change it", [
    ("Ashen Blade", "Fire", "B", "Sword · main hand", "", "worn"),
    ("Frost Fang", "Water", "C", "Sword · single or dual", "", None),
    ("Tempest Sabre", "Lightning", "A", "Sword · single or dual", "", None),
    ("Gale Dagger", "Wind", "D", "", "", "Pairs only with a sword"),
    ("Greatblade", "None", "B", "", "", "Can't be dual wielded")], 1, [
    ("Power", "42 + 30", "36 + 30", False), ("Speed", "Normal", "Fast", True), ("Element", "Fire (crystal)", "Fire (crystal)", None),
    ("Abilities", "Cleave, Whirl…", "Pierce, Lunge…", None)])
footer(d, FOOT)
save(im, OUT, "1_weapons_generic.png")

# ---- 1a: Y opens the Options pop-up over the Generic tab.
opt = Image.blend(im, Image.new("RGB", im.size, BG), 0.55)
d = ImageDraw.Draw(opt)
box = (860, 340, 1232, 654)
panel(d, box, focus=True)
text(d, (880, 354), "Options", 18, True)
y = 388
for i, (lab, sub, on) in enumerate([("Saved loadouts", "Load, save or rename", True),
                                     ("Remove all gear", "Asks first", False),
                                     ("Help", "What each part of this screen does", False),
                                     ("Cosmetics shortcuts", "Later", None)]):
    cut(d, (872, y, 1220, y + 46), TEAL if on else (PLATE if on is False else BG), CYAN if on else (STEEL if on is False else OFF), 6)
    text(d, (886, y + 5), lab, 15, True, OFF if on is None else TXT)
    text(d, (886, y + 26), sub, 11, fill=OFF if on is None else (TXT if on else DIM))
    y += 52
footer(d, [("A", "Select"), ("B", "Close")], y=624, right=1220)
save(opt, OUT, "1a_options.png")

# ---- 1b: X Details on the dual entry: each weapon's type and stats, the entry's sockets, and a Skills button that
#          jumps to this entry on the Skills tab.
det = Image.blend(im, Image.new("RGB", im.size, BG), 0.6)
d = ImageDraw.Draw(det)
panel(d, (240, 120, 1040, 640), focus=True)
text(d, (266, 138), "Ashen Blade + Cinder Edge", 22, True)
text(d, (266, 170), "Dual wield · Sword stance (the main hand's) · ACTIVE", 13, fill=DIM)
for i, (n, el, t, lines) in enumerate([
        ("Ashen Blade", "Fire", "B", [("Type", "Sword · single or dual"), ("Power", "42"), ("Scaling", "Body B"),
                                       ("Speed", "Normal")]),
        ("Cinder Edge", "None", "C", [("Type", "Sword · single or dual"), ("Power", "30"), ("Scaling", "Body C"),
                                       ("Speed", "Fast")])]):
    x = 266 + i * 380
    cut(d, (x, 200, x + 356, 380), PLATE, STEEL, 8)
    icon(d, (x + 12, 212, x + 52, 252), ELEMENT[el])
    text(d, (x + 64, 214), n, 17, True)
    tier_badge(d, x + 64 + int(d.textlength(n, font=F(17, True))) + 16, 224, t, 10)
    for j, (lab, val) in enumerate(lines):
        text(d, (x + 16, 266 + j * 26), lab, 14, fill=DIM)
        text(d, (x + 120, 266 + j * 26), val, 14, True)
text(d, (266, 396), "Sockets (shared)", 15, True, CYAN)
for i, (sk, n, sub) in enumerate([(("crystal", "Fire"), "Garnet", "Fire crystal · Fire for both swords' infused abilities"),
                                  (None, "Empty", "A stone or a second crystal fits here")]):
    y = 424 + i * 46
    socket_icon(d, (266, y, 302, y + 36), sk)
    text(d, (314, y + 1), n, 15, True, TXT if sk else OFF)
    text(d, (314, y + 20), sub, 12, fill=DIM)
text(d, (266, 526), "Abilities: 7 slots (both swords)  ·  Cells: 1", 14, fill=DIM)
cut(d, (266, 566, 466, 610), TEAL, CYAN, 6)
text(d, (366, 588), "Skills  →", 16, True, anchor="mm")
text(d, (480, 580), "Opens this entry on the Skills tab", 12, fill=DIM)
footer(d, [("A", "Skills"), ("B", "Close")], y=588, right=1020)
save(det, OUT, "1b_details.png")


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


def kind_tabs(d, lit):
    for i, lab in enumerate(["Weapon", "Adapter", "Evolution"]):
        bx = 146 + i * 136
        cut(d, (bx, 128, bx + 128, 160), TEAL if i == lit else PLATE, CYAN if i == lit else STEEL, 6)
        text(d, (bx + 64, 144), lab, 15, True, anchor="mm")


def build_slot(d, y, label, name, sock, sub, focus=False, x0=146, x1=560):
    """A piece of the entry being built; name None = still to pick. sock: (kind, el) to draw a socket icon."""
    text(d, (x0, y), label, 13, True, DIM)
    y += 20
    cut(d, (x0, y, x1, y + 50), TEAL if focus else (PLATE if name else BG), CYAN if focus else (STEEL if name else OFF), 8)
    if sock and sock[0] in ("crystal", "stone", "evolution"):
        socket_icon(d, (x0 + 10, y + 7, x0 + 46, y + 43), sock if name else None)
    else:
        icon(d, (x0 + 10, y + 7, x0 + 46, y + 43), ELEMENT[sock] if name else None, empty=not name)
    text(d, (x0 + 56, y + 6), name or ("choosing…" if focus else "—"), 16, True, TXT if name or focus else OFF)
    text(d, (x0 + 56, y + 28), sub, 12, fill=TXT if focus else DIM)
    return y + 60


def screen_g(focus_add=True):
    base, d = canvas()
    header(d, 1, 0, "Bruiser  ·  2 of 5")
    used_line(d, 2, 5)
    y = evo_line(d, 168, None, None)
    y = entry(d, y, "Hammer", [("Quarry Maul", "Earth")], [("stone", None), ("stone", None)], 2)
    add_frame(d, y, 3, 5, focus=focus_add)
    return base


# ---- 2: the Add gear window, Weapon: main and off hand set, socket 1 focused. A is Pick (Remove on the on-now row).
im, d = window(screen_g(), "Add gear", "3/5 left",
               [("A", "Pick"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 0)
y = build_slot(d, 172, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword · single or dual")
y = build_slot(d, y, "Off hand  ·  a sword that pairs, or empty", "Cinder Edge", "None", "Sword · single or dual")
y = build_slot(d, y, "Augment socket 1", None, ("crystal", None), "A crystal or an evolution, or a stone", focus=True)
y = build_slot(d, y, "Augment socket 2", None, ("stone", None), "A stone (or a crystal if socket 1 has a stone)")
cut(d, (146, y + 2, 560, y + 42), BG, STEEL, 8)
text(d, (160, y + 12), "Sword + Sword", 16, True)
text(d, (546, y + 12), "0 cells so far", 15, True, CYAN, anchor="ra")
text(d, (590, 128), "Augment socket 1  ·  shared by both swords", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Garnet", ("crystal", "Fire"), "B", "Crystal · Fire", "1 cell", None),
    ("Phoenix", ("evolution", "Fire"), "B", "Evolution · its passives at infusion 2", "2 cells", None),
    ("Whetstone", ("stone", None), "C", "Stone · Power +4%", "1 cell", None),
    ("Sapphire", ("crystal", "Water"), "D", "Crystal · Water", "1 cell", None),
    ("Onyx", ("crystal", "Darkness"), "S", "", "", "S-tier: one per loadout (on Iron Coat)"),
    ("Tidal", ("evolution", "Water"), "B", "", "", "On Bram's Tide Ring")], 0)
save(im, OUT, "2_add_gear_weapon.png")

# ---- 2a: the Add gear window, Evolution: one per character, on the character, 2 cells; confirming asks first.
im, d = window(screen_g(), "Add gear", "3/5 left",
               [("A", "Pick"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 2)
build_slot(d, 172, "Garrick's evolution  ·  one per character", "Ironheart", ("evolution", "Earth"), "Evolution · Earth · 2 cells",
           focus=True)
text(d, (590, 128), "Evolutions you own", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Ironheart", ("evolution", "Earth"), "A", "Evolution · Earth", "2 cells", None),
    ("Phoenix", ("evolution", "Fire"), "B", "Evolution · Fire", "2 cells", None),
    ("Emberheart", ("evolution", "Fire"), "B", "", "", "Bram's own evolution"),
    ("Tidal", ("evolution", "Water"), "B", "", "", "On Bram's Tide Ring")], 0)
cut(d, (380, 300, 900, 470), PANEL, CYAN, 12)
text(d, (640, 330), "Attach Ironheart to Garrick?", 20, True, anchor="ma")
text(d, (640, 366), "Uses 2 cells: 3 left → 1 left.", 15, fill=DIM, anchor="ma")
for i, (k, lab, on) in enumerate([("A", "Attach", True), ("B", "Not now", False)]):
    bx = 470 + i * 180
    cut(d, (bx, 410, bx + 160, 448), TEAL if on else PLATE, CYAN if on else STEEL, 6)
    text(d, (bx + 80, 429), k + "  " + lab, 15, True, anchor="mm")
save(im, OUT, "2a_add_gear_evolution.png")

# ---- 2b: A Edit on the dual entry opens the window for it. Moving onto the piece already in the focused slot (ON NOW)
#          turns A into Remove; elsewhere A is Pick.
im, d = window(screen_g(False), "Edit  ·  Ashen Blade + Cinder Edge", "1/5 left",
               [("A", "Remove"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 0)
y = build_slot(d, 172, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword · single or dual")
y = build_slot(d, y, "Off hand", "Cinder Edge", "None", "Sword · single or dual")
y = build_slot(d, y, "Augment socket 1", "Garnet", ("crystal", "Fire"), "Crystal · Fire · 1 cell", focus=True)
y = build_slot(d, y, "Augment socket 2", None, ("stone", None), "A stone (or a crystal if socket 1 has a stone)")
text(d, (590, 128), "Augment socket 1", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Garnet", ("crystal", "Fire"), "B", "Crystal · Fire  ·  A takes it out", "1 cell", "worn"),
    ("Sapphire", ("crystal", "Water"), "D", "Crystal · Water", "1 cell", None),
    ("Whetstone", ("stone", None), "C", "Stone · Power +4%", "1 cell", None),
    ("Phoenix", ("evolution", "Fire"), "B", "Evolution", "2 cells", None),
    ("Onyx", ("crystal", "Darkness"), "S", "", "", "S-tier: one per loadout (on Iron Coat)")], 0)
save(im, OUT, "2b_add_gear_remove.png")

# ---- 3: Sera (Refractor): one entry, 2 cells; the entry is full, so no frame. Innate spells live on the Skills tab.
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
used_line(d, 1, 2, note="1 entry")
y = evo_line(d, 168, None, None)
y = entry(d, y, "Staff", [("Cinder Staff", "Fire")], [("crystal", "Fire"), None], 1, focus=True)
choices(d, "One entry  ·  2 cells", [
    ("Cinder Staff", "Fire", "C", "Staff · Garnet", "1 cell", "worn"),
    ("Tidal Pike", "Water", "C", "Spear · Sapphire", "1 cell", None),
    ("Ember Ring", "Fire", "B", "Adapter · evolution", "2 cells", None),
    ("Pyre Pike", "Fire", "A", "", "", "Evolution + stone: needs 3 cells, 2 here"),
    ("Twin Fangs", "Wind", "C", "", "", "Twin only: needs two, one item here")], 1, [
    ("Cells", "1 of 2", "1 of 2", None), ("Power", "28", "34", True),
    ("Elements cast", "Fire", "Fire, Water", True)])
footer(d, FOOT)
save(im, OUT, "3_weapons_refractor.png")

# ---- 4: Bram (Resonator): his evolution, one weapon entry and his ring entries (up to 5), then the Add weapon frame.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
used_line(d, 4, 5, note="1 weapon entry · 3 of 5 rings")
y = evo_line(d, 168, None, None)
y = entry(d, y, "Fists", [("Cinder Fists", "None")], [None, None], 0, h=56)
y = entry(d, y, "Ring", [("Ember Ring", "Fire")], [("crystal", "Fire"), None], 1, active=True, h=56)
y = entry(d, y, "Ring", [("Storm Ring", "Lightning")], [("crystal", "Lightning"), None], 1, h=56)
y = entry(d, y, "Ring", [("Tide Ring", "Water")], [("evolution", "Water"), None], 2, h=56)
add_frame(d, y, 1, 5, focus=True)
choices(d, "Rings you could add  ·  1/5 left", [
    ("Gale Band", "Wind", "C", "Ring · Emerald", "1 cell", None),
    ("Frost Ring", "Water", "D", "Ring · empty", "0 cells", None),
    ("Pyre Ring", "Fire", "B", "", "", "Evolved: needs 2 cells, 1 left"),
    ("Ember Ring", "Fire", "C", "", "", "Already an entry"),
    ("Cinder Fists", "None", "C", "", "", "A weapon: the one weapon entry is used")], 0, [
    ("Cells", "4 of 5", "5 of 5", None), ("Rings", "3 of 5", "4 of 5", None),
    ("Spell elements", "3", "4 (+ Wind)", True)])
footer(d, [("A", "Equip"), ("X", "Details"), ("Y", "Options"), ("B", "Back")])
save(im, OUT, "4_weapons_resonator.png")


def apparel(d, y, name, el, lines, bonus, sock, focus=False):
    cut(d, (L, y, LR, y + 92), TEAL if focus else PLATE, CYAN if focus else STEEL, 10)
    icon(d, (L + 10, y + 10, L + 66, y + 66), ELEMENT[el])
    text(d, (L + 78, y + 10), name, 19, True)
    text(d, (L + 78, y + 38), lines, 14, fill=TXT if focus else DIM)
    text(d, (L + 78, y + 60), bonus, 14, True, CYAN)
    socket_icon(d, (LR - 50, y + 28, LR - 14, y + 64), sock)


# ---- 5: Apparel tab: two slots, no budget. Each piece: HP / Defence / Amp + one rolled bonus (a sub-stat, or a pillar
#         as a % of its value, never points). Its socket takes a crystal or an evolution.
im, d = canvas()
header(d, 1, 2, "Bruiser  ·  2 of 5")
text(d, (L, TOP), "Apparel doesn't use the cells.", 16, fill=DIM)
apparel(d, 168, "Iron Coat", "Darkness", "HP 40 · Def 14 · Amp +2", "+5% Body", ("crystal", "Darkness"), focus=True)
apparel(d, 270, "Shade Hood", "None", "HP 25 · Def 6 · Amp +1", "+4% Crit Rate", None)
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


def skill_rows(d, rows, focus, top=TOP + 40):
    """The focused entry's skill slots. rows: ("head", text) for a weapon's sub-heading, or (name, el, kind, ep) with
    kind "built-in", "slot" (picked) or "empty"."""
    y = top
    for i, r in enumerate(rows):
        if r[0] == "head":
            text(d, (LIST_X + 18, y + 4), r[1], 14, True, CYAN)
            y += 28
            continue
        n, el, kind, ep = r
        f = i == focus
        empty = kind == "empty"
        cut(d, (LIST_X + 12, y, 1220, y + 44), TEAL if f else (BG if empty else PLATE), CYAN if f else (OFF if empty else STEEL), 6)
        if empty:
            text(d, (LIST_X + 28, y + 12), "Empty ability slot", 15, fill=TXT if f else OFF)
        else:
            d.regular_polygon((LIST_X + 34, y + 22, 8), 6, fill=ELEMENT[el])
            text(d, (LIST_X + 52, y + 11), n, 16, True)
            if kind == "built-in":
                text(d, (LIST_X + 52 + int(d.textlength(n, font=F(16, True))) + 12, y + 14), "built-in", 12, fill=DIM)
            text(d, (1206, y + 12), "%s EP" % ep, 14, True, anchor="ra")
        y += 50


# ---- 6: Skills tab (Generic): the left lists the same entries as the Weapons tab, names only; the right shows the
#         focused entry's skill slots. A dual entry shows both swords' slots.
im, d = canvas()
header(d, 1, 1, "Bruiser  ·  2 of 5")
text(d, (L, TOP), "Abilities by entry", 17, True, DIM)
y = entry(d, 168, "", [("Ashen Blade", "Fire"), ("Cinder Edge", "None")], [("crystal", "Fire"), None], 1,
          active=True, focus=True)
entry(d, y, "", [("Quarry Maul", "Earth")], [("stone", None), None], 1)
panel(d, (LIST_X, TOP, 1232, 654), focus=True)
text(d, (LIST_X + 18, TOP + 10), "Ashen Blade + Cinder Edge  ·  7 slots", 16, True, DIM)
skill_rows(d, [
    ("head", "Ashen Blade  ·  4 slots"),
    ("Slash", "None", "built-in", 0), ("Cleave", "None", "slot", 4), ("Flame Arc", "Fire", "slot", 6),
    ("", None, "empty", 0),
    ("head", "Cinder Edge  ·  3 slots"),
    ("Quick Cut", "None", "built-in", 0), ("Whirl", "None", "slot", 5), ("", None, "empty", 0)], 3)
footer(d, [("A", "Change"), ("X", "Details"), ("Y", "Options"), ("B", "Back")])
save(im, OUT, "6_skills_generic.png")

# ---- 7: Skills tab (Refractor): an "Innate spells" entry above the gear entry. Focused, the right shows the school rows:
#         each school's spells, 6 at most, on a 24-point budget (tier = points, F 1 .. S 7).
im, d = canvas()
header(d, 0, 1, "Pyre  ·  1 of 5")
text(d, (L, TOP), "Skills by entry", 17, True, DIM)
cut(d, (L, 168, LR, 230), TEAL, CYAN, 8)
icon(d, (L + 8, 183, L + 40, 215), ELEMENT["Fire"])
text(d, (L + 54, 189), "Innate spells", 16, True)
text(d, (LR - 14, 199), "18 of 24", 16, True, anchor="rm")
entry(d, 238, "", [("Cinder Staff", "Fire")], [("crystal", "Fire"), None], 1)
panel(d, (LIST_X, TOP, 1232, 654), focus=True)
text(d, (LIST_X + 18, TOP + 10), "Innate spells  ·  Fire  ·  18 of 24 points", 16, True, DIM)
y = TOP + 44
for i, (school, spells) in enumerate([
        ("Destruction", ["Flame Lance", "Ember", "Scorch"]), ("Conjuration", []), ("Enhancement", ["Kindle"]),
        ("Restoration", ["Warm Light", "Cauterise"]), ("Impairment", ["Smoulder"]), ("Manipulation", [])]):
    f = i == 0
    cut(d, (LIST_X + 12, y, 1220, y + 66), TEAL if f else PLATE, CYAN if f else STEEL, 6)
    text(d, (LIST_X + 26, y + 8), school, 16, True, TXT if spells else OFF)
    text(d, (1206, y + 8), "%d of 6" % len(spells), 13, True, TXT if spells else OFF, anchor="ra")
    for p in range(6):
        px = LIST_X + 26 + p * 86
        cut(d, (px, y + 34, px + 80, y + 58), (BG if p >= len(spells) else PANEL), OFF if p >= len(spells) else STEEL, 4)
        if p < len(spells):
            text(d, (px + 6, y + 38), fit(d, spells[p], 11, True, 70), 11, True)
    y += 74
footer(d, [("A", "Change"), ("X", "Details"), ("Y", "Options"), ("B", "Back")])
save(im, OUT, "7_skills_refractor.png")
print("ok")
