"""Loadout v2 (Crown, 2026-11-14; B-479 redraw for D-209 / D-210 and Crown's notes). Weapons and Apparel: one list that takes
the width; X Details opens a row in place (short stat lines; effects as ET-50 nested text), X again opens its skills. Picking and Compare
(Equipped vs Selected; ups green, downs red) happen only in the Add gear / Add apparel window, where items that can't go
in stay listed, greyed, with the reason. Tabs Weapons | Skills | Apparel | Cosmetics (later) on LB / RB. Main keys:
A Edit / Equip, X Details, L3 Set active (an inactive weapon entry), Y Options (Set active, Saved loadouts, Remove all
gear, Help, later Cosmetics shortcuts), B Back; help text is not on screen.
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
L, LR = 48, 1232            # the entries list takes the width (B-479b); the Skills tab narrows it to 620
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


def add_frame(d, y, left, total, focus=False, label="Add weapon"):
    """The empty frame at the bottom of the entries."""
    cut(d, (L, y, LR, y + 44), TEAL if focus else BG, CYAN if focus else OFF, 8)
    text(d, (L + 16, y + 12), "+  " + label, 16, True, TXT if focus else DIM)
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
            text(d, (x1 - 12, y + 8), "EQUIPPED", 11, True, CYAN, anchor="ra")
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


def tree(d, x, y, lines, step=20):
    """ET-50's nested effect text (EffectText::BuildTree as a block): a trigger heading, its conditions indented
    under it, then the effects as bullets with numbers, duration and charges. Two spaces per level."""
    for line in lines:
        lvl = (len(line) - len(line.lstrip(" "))) // 2
        body = line.strip()
        if lvl == 0:
            text(d, (x, y), body, 13, True, CYAN)
        elif body.startswith("•"):
            text(d, (x + lvl * 16, y), body, 13, True)
        else:
            text(d, (x + lvl * 16, y), body, 13, fill=DIM)
        y += step
    return y


def open_body(d, y, pieces, lines, sockets, notes=(), skills=True):
    """X Details (B-479b): the highlighted entry's row grows in place under its header. pieces: (name, el, tier,
    [(label, value)]); lines: its ET-50 effect tree; sockets: (socket, name, line). Up / Down closes it; the right
    stick scrolls a tall one; X again opens the entry's skills."""
    x0 = L + 16
    yy = y + 4
    rows = []
    for name, el, t, stats in pieces:
        rows.append((yy, name, el, t, stats))
        yy += 52
    yy = (yy + 4) if lines else yy
    tree_y = yy
    yy += 20 * len(lines) + (8 if lines else 0)
    sock_y = yy
    yy += 46 if sockets else 0
    note_y = yy
    yy += 22 * len(notes) + (24 if skills else 0) + 6
    cut(d, (L, y - 8, LR, yy), PANEL, CYAN, 8)
    for ry, name, el, t, stats in rows:
        icon(d, (x0, ry + 4, x0 + 24, ry + 28), ELEMENT[el])
        text(d, (x0 + 34, ry + 6), name, 15, True)
        tier_badge(d, x0 + 34 + int(d.textlength(name, font=F(15, True))) + 14, ry + 15, t, 9)
        for k, (lab, val) in enumerate(stats):
            x = x0 + 240 + k * 220
            text(d, (x, ry + 7), lab, 13, fill=DIM)
            text(d, (x + 90, ry + 7), val, 13, True)
    if lines:
        tree(d, x0, tree_y, lines)
    colw = (LR - L - 32) // 2
    for k, (sk, name, line) in enumerate(sockets):
        x = x0 + k * colw
        socket_icon(d, (x, sock_y, x + 36, sock_y + 36), sk)
        text(d, (x + 48, sock_y + 1), name, 15, True, TXT if sk else OFF)
        text(d, (x + 48, sock_y + 20), line, 12, fill=DIM)
    for k, line in enumerate(notes):
        text(d, (x0, note_y + k * 22), line, 13, fill=DIM)
    if skills:
        text(d, (LR - 16, yy - 28), "X again: this entry's skills", 13, True, CYAN, anchor="ra")
    return yy + 8


def more_below(d):
    """The list goes on past the screen: it scrolls (the open row moves with it)."""
    d.rectangle((0, 662, 1280, 720), fill=BG)
    text(d, ((L + LR) // 2, 664), "▼  more", 12, True, DIM, anchor="ma")


FOOT = [("A", "Edit"), ("X", "Details"), ("Y", "Options"), ("B", "Back")]
FOOT_L3 = [("A", "Edit"), ("X", "Details"), ("L3", "Set active"), ("Y", "Options"), ("B", "Back")]  # an inactive weapon
QUARRY = dict(pieces=[("Quarry Maul", "Earth", "C", [("Type", "Hammer"), ("Power", "50"), ("Scaling", "Body A"),
                                                     ("Speed", "Slow")])],
              lines=["On Critical Hit", "  While the target is Staggered",
                     "    • Delay the target's next turn, once per fight"],
              sockets=[(("stone", None), "Whetstone  C", "Power +9%"), (None, "Empty", "Crystal or evolution socket")],
              notes=["Hammer stance · not active · 4 ability slots"])

# ---- 1 / 1c: Garrick (Generic): his evolution, two weapon entries and the Add weapon frame (1/5 left). The list takes
#         the width; Quarry Maul is focused and not active (L3 Set active, D-207). 1c: X Details opens it in place.
def weapons_g(opened=False):
    im, d = canvas()
    header(d, 1, 0, "Bruiser  ·  2 of 5")
    used_line(d, 4, 5, note="2 of 2 weapon entries · 0 of 1 adapter")
    y = evo_line(d, 168, "Ironheart", "Earth")
    y = entry(d, y, "Sword + Sword", [("Ashen Blade", "Fire"), ("Cinder Edge", "None")],
              [("crystal", "Fire"), None], 1, active=True)
    y = entry(d, y, "Hammer", [("Quarry Maul", "Earth")], [("stone", None), None], 1, focus=True)
    if opened:
        y = open_body(d, y, **QUARRY)
    add_frame(d, y, 1, 5)
    footer(d, FOOT_L3)
    return im


im = weapons_g()
save(im, OUT, "1_weapons_generic.png")
im1 = im  # 1a sits over this screen
save(weapons_g(True), OUT, "1c_weapons_open.png")

# ---- 1b: the evolution line focused: on the character it gives everything (Amp by tier, passives, spells).
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 4, 5, note="2 of 2 weapon entries · 0 of 1 adapter")
y = evo_line(d, 168, "Ironheart", "Earth", focus=True)
y = open_body(d, y - 2, [("Ironheart", "Earth", "A", [("Element", "Earth"), ("Amp", "Earth +2"), ("Spell slots", "6")])],
              ["Passive", "  • Gain +10% Defense",
               "When you take Earth damage", "  While your HP is below 50%",
               "    • Gain +15% Earth buildup for 2 turns, stacks up to 3 times"], [],
              ["Garrick's own evolution · 2 cells · on gear instead: these passives only, at infusion 2"], skills=False)
y = entry(d, y, "Swords", [("Ashen Blade", "Fire"), ("Cinder Edge", "None")], [("crystal", "Fire"), None], 1, active=True)
y = entry(d, y, "Hammer", [("Quarry Maul", "Earth")], [("stone", None), None], 1)
add_frame(d, y, 1, 5)
footer(d, FOOT)
save(im, OUT, "1b_evolution.png")

# ---- 1a: Y opens the Options pop-up over the Generic tab.
opt = Image.blend(im1, Image.new("RGB", im.size, BG), 0.55)
d = ImageDraw.Draw(opt)
box = (860, 288, 1232, 654)
panel(d, box, focus=True)
text(d, (880, 302), "Options", 18, True)
y = 336
for i, (lab, sub, on) in enumerate([("Set active", "Quarry Maul becomes the active weapon (L3)", True),
                                     ("Saved loadouts", "Load, save or rename", False),
                                     ("Remove all gear", "Asks first", False),
                                     ("Help", "What each part of this screen does", False),
                                     ("Cosmetics shortcuts", "Later", None)]):
    cut(d, (872, y, 1220, y + 46), TEAL if on else (PLATE if on is False else BG), CYAN if on else (STEEL if on is False else OFF), 6)
    text(d, (886, y + 5), lab, 15, True, OFF if on is None else TXT)
    text(d, (886, y + 26), sub, 11, fill=OFF if on is None else (TXT if on else DIM))
    y += 52
footer(d, [("A", "Select"), ("B", "Close")], y=624, right=1220)
save(opt, OUT, "1a_options.png")



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


# ---- 2: the Add gear window, Weapon: main and off hand set, socket 1 focused. A is Pick (Remove on the on-now row),
#         X Compare.
im, d = window(screen_g(), "Add gear", "3/5 left",
               [("A", "Pick"), ("X", "Compare"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 0)
y = build_slot(d, 172, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword")
y = build_slot(d, y, "Off hand  ·  a sword that pairs, or empty", "Cinder Edge", "None", "Sword")
y = build_slot(d, y, "Augment socket 1", None, ("crystal", None), "A crystal or an evolution, or a stone", focus=True)
y = build_slot(d, y, "Augment socket 2", None, ("stone", None), "A stone (or a crystal if socket 1 has a stone)")
cut(d, (146, y + 2, 560, y + 42), BG, STEEL, 8)
text(d, (160, y + 12), "Sword + Sword", 16, True)
text(d, (546, y + 12), "0 cells so far", 15, True, CYAN, anchor="ra")
text(d, (590, 128), "Augment socket 1  ·  shared by both swords", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Garnet", ("crystal", "Fire"), "B", "Fire Amp +2 · Burn 13% max HP/turn", "1 cell", None),
    ("Phoenix", ("evolution", "Fire"), "B", "At infusion 2: revive once at 20% HP", "2 cells", None),
    ("Whetstone", ("stone", None), "C", "Power +9%", "1 cell", None),
    ("Sapphire", ("crystal", "Water"), "D", "Water Amp +1 · Survive one KO", "1 cell", None),
    ("Onyx", ("crystal", "Darkness"), "S", "", "", "S-tier: one per loadout (on Iron Coat)"),
    ("Tidal", ("evolution", "Water"), "B", "", "", "On Bram's Tide Ring")], 0)
save(im, OUT, "2_add_gear_weapon.png")

# ---- 2a: the Add gear window, Evolution: one per character, on the character, 2 cells; confirming asks first.
im, d = window(screen_g(), "Add gear", "3/5 left",
               [("A", "Pick"), ("X", "Compare"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 2)
build_slot(d, 172, "Garrick's evolution  ·  one per character", "Ironheart", ("evolution", "Earth"), "Earth Amp +2 · Defence +10% · 2 cells",
           focus=True)
text(d, (590, 128), "Evolutions you own", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Ironheart", ("evolution", "Earth"), "A", "Earth Amp +2 · Defence +10% · Earth buildup +15%", "2 cells", None),
    ("Phoenix", ("evolution", "Fire"), "B", "Fire Amp +2 · Revive once at 20% HP", "2 cells", None),
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

# ---- 2b: A Edit on the dual entry opens the window for it. Moving onto the piece already in the focused slot (EQUIPPED)
#          turns A into Remove; elsewhere A is Pick.
im, d = window(screen_g(False), "Edit  ·  Ashen Blade + Cinder Edge", "1/5 left",
               [("A", "Remove"), ("X", "Compare"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 0)
y = build_slot(d, 172, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword")
y = build_slot(d, y, "Off hand", "Cinder Edge", "None", "Sword")
y = build_slot(d, y, "Augment socket 1", "Garnet", ("crystal", "Fire"), "Fire Amp +2 · Burn 13%/turn", focus=True)
y = build_slot(d, y, "Augment socket 2", None, ("stone", None), "A stone (or a crystal if socket 1 has a stone)")
text(d, (590, 128), "Augment socket 1", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Garnet", ("crystal", "Fire"), "B", "Fire Amp +2 · Burn 13% max HP/turn", "1 cell", "worn"),
    ("Sapphire", ("crystal", "Water"), "D", "Water Amp +1 · Survive one KO", "1 cell", None),
    ("Whetstone", ("stone", None), "C", "Power +9%", "1 cell", None),
    ("Phoenix", ("evolution", "Fire"), "B", "At infusion 2: revive once at 20% HP", "2 cells", None),
    ("Onyx", ("crystal", "Darkness"), "S", "", "", "S-tier: one per loadout (on Iron Coat)")], 0)
save(im, OUT, "2b_add_gear_remove.png")

# ---- 2c: X Compare in Add gear: the equipped piece (Cinder Edge, the off hand) beside the highlighted pick (Frost Fang),
#          with what you gain (green) and lose (red). Picking and comparing happen only here.
base = screen_g(False)
im, d = window(base, "Edit  ·  Ashen Blade + Cinder Edge", "1/5 left",
               [("A", "Pick"), ("X", "Close compare"), ("Y", "Confirm"), ("B", "Cancel")])
kind_tabs(d, 0)
y = build_slot(d, 172, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword")
y = build_slot(d, y, "Off hand", "Cinder Edge", "None", "Sword", focus=True)
text(d, (590, 128), "Off hand  ·  swords that pair", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Cinder Edge", "None", "C", "Sword", "", "worn"),
    ("Frost Fang", "Water", "C", "Sword", "", None)], 1)
cut(d, (300, 230, 1100, 600), PANEL, CYAN, 12)
text(d, (326, 248), "Compare  ·  off hand", 18, True)
for i, (lab, n, el, t) in enumerate([("Equipped", "Cinder Edge", "None", "C"), ("Selected", "Frost Fang", "Water", "C")]):
    x = 520 + i * 290
    text(d, (x, 286), lab, 12, True, CYAN if i else DIM)
    icon(d, (x, 304, x + 36, 340), ELEMENT[el])
    text(d, (x + 46, 312), n, 16, True)
for j, (lab, a, b, good) in enumerate([("Power", "30", "34", True), ("Speed", "Fast", "Normal", False),
                                        ("Element", "none", "Water", None), ("Cells", "1", "1 (sockets stay)", None)]):
    yy = 360 + j * 30
    text(d, (326, yy), lab, 15, fill=DIM)
    text(d, (520, yy), a, 15, True)
    col = TXT if good is None else (GOOD if good else RED)
    text(d, (810, yy), b + ("" if good is None else ("  ▲" if good else "  ▼")), 15, True, col)
d.line((326, 486, 1074, 486), fill=STEEL)
for j, (lab, a, b) in enumerate([("Passive", "1st hit Crit +15%", "Chill buildup +10%"),
                                  ("Built-in", "Quick Cut", "Frost Bite")]):
    yy = 498 + j * 34
    text(d, (326, yy), lab, 15, fill=DIM)
    text(d, (520, yy), "- " + a, 15, True, RED)
    text(d, (810, yy), "+ " + b, 15, True, GOOD)
save(im, OUT, "2c_add_gear_compare.png")

# ---- 3: Sera (Refractor): one entry, 2 cells; the entry is full, so no frame. Innate spells live on the Skills tab.
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
used_line(d, 1, 2, note="1 entry")
y = evo_line(d, 168, None, None)
y = entry(d, y, "Staff", [("Cinder Staff", "Fire")], [("crystal", "Fire"), None], 1, active=True, focus=True)
footer(d, FOOT)
save(im, OUT, "3_weapons_refractor.png")

# ---- 4: Bram (Resonator): his evolution, one weapon entry and his ring entries (up to 5), then the Add weapon frame.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
used_line(d, 4, 5, note="1 weapon entry · 3 of 5 rings")
y = evo_line(d, 168, None, None)
y = entry(d, y, "Fists", [("Cinder Fists", "None")], [None, None], 0, h=56)
y = entry(d, y, "Ring", [("Ember Ring", "Fire")], [("crystal", "Fire"), None], 1, active=True, focus=True, h=56)
y = open_body(d, y, [("Ember Ring", "Fire", "C", [("Type", "Ring"), ("Spells", "Fire"), ("Durability", "64 / 80"),
                                                   ("Spell slots", "4")])],
              ["On Spell Cast", "  While your EP is above 50%", "    • Gain +1 Fire Amp for 1 turn"],
              [(("crystal", "Fire"), "Garnet  C", "Fire Amp +1 · Burn 11% max HP/turn"), (None, "Empty", "Stone socket")])
y = entry(d, y, "Ring", [("Storm Ring", "Lightning")], [("crystal", "Lightning"), None], 1, h=56)
y = entry(d, y, "Ring", [("Tide Ring", "Water")], [("evolution", "Water"), None], 2, h=56)
add_frame(d, y, 1, 5)
more_below(d)
footer(d, FOOT)
save(im, OUT, "4_weapons_resonator.png")


# ---- 5: Apparel tab, the same way as Weapons: entries on the left, an Add apparel frame at the bottom (2 slots, no
#         cells), the focused piece's details on the right. Each piece: HP / Defence / Amp + one rolled bonus (a sub-stat,
#         or a pillar as a % of its value, never points); its socket takes a crystal or an evolution.
im, d = canvas()
header(d, 1, 2, "Bruiser  ·  2 of 5")
text(d, (L, TOP), "Apparel doesn't use the cells.", 16, fill=DIM)
y = entry(d, 168, "", [("Iron Coat", "Darkness")], [("crystal", "Darkness")], 0, focus=True)
y = open_body(d, y, [("Iron Coat", "Darkness", "C", [("HP", "40"), ("Defence", "14"), ("Amp", "+2"),
                                                     ("Bonus", "+5% Body")])], [],
              [(("crystal", "Darkness"), "Onyx  S", "Darkness Amp +3 · Darkness resistance · Silence")], skills=False)
add_frame(d, y, 1, 2, label="Add apparel")
footer(d, FOOT)
save(im, OUT, "5_apparel.png")

# ---- 5a: the Add apparel window: pick a piece and its socket; X Compare opens equipped vs selected.
base, d = canvas()
header(d, 1, 2, "Bruiser  ·  2 of 5")
y = entry(d, 168, "", [("Iron Coat", "Darkness")], [("crystal", "Darkness")], 0)
add_frame(d, y, 1, 2, focus=True, label="Add apparel")
im, d = window(base, "Add apparel", "1/2 left", [("A", "Pick"), ("X", "Compare"), ("Y", "Confirm"), ("B", "Cancel")])
y = build_slot(d, 140, "Apparel", None, None, "A coat, cloak, hood…", focus=True)
y = build_slot(d, y, "Socket", None, ("crystal", None), "A crystal or an evolution (no stone)")
text(d, (590, 128), "Apparel you own", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Ember Cloak", "Fire", "C", "HP 30 · Def 10 · Amp +4 · +6% Crit Damage", "", None),
    ("Warden Plate", "None", "B", "HP 55 · Def 20 · Amp +0 · +5% Efficiency", "", None),
    ("Shade Hood", "None", "D", "HP 25 · Def 6 · Amp +1 · +4% Crit Rate", "", None),
    ("Iron Coat", "Darkness", "C", "", "", "Already worn"),
    ("Prisma's Mantle", "Void", "B", "", "", "Bound to Amethyst Prisma: unlock that Spirit")], 0)
save(im, OUT, "5a_add_apparel.png")


def skill_rows(d, rows, focus, top=TOP + 40, opened=None):
    """The focused entry's skill slots. rows: ("head", text) for a weapon's sub-heading, or (name, el, kind, ep) with
    kind "built-in", "slot" (picked) or "empty". opened: the focused row's details (X Details), drawn in place."""
    y = top
    for i, r in enumerate(rows):
        if r[0] == "head":
            text(d, (LIST_X + 18, y + 4), r[1], 14, True, CYAN)
            y += 28
            continue
        n, el, kind, ep = r[:4]
        f = i == focus
        if f and opened:
            y = open_skill(d, LIST_X + 12, 1220, y, *opened) + 6
            continue
        empty = kind == "empty"
        cut(d, (LIST_X + 12, y, 1220, y + 44), TEAL if f else (BG if empty else PLATE), CYAN if f else (OFF if empty else STEEL), 6)
        if empty:
            text(d, (LIST_X + 28, y + 12), "Empty ability slot", 15, fill=TXT if f else OFF)
        else:
            d.regular_polygon((LIST_X + 34, y + 22, 8), 6, fill=ELEMENT[el])
            text(d, (LIST_X + 52, y + 11), n, 16, True)
            if kind == "built-in":
                text(d, (LIST_X + 52 + int(d.textlength(n, font=F(16, True))) + 12, y + 14), "built-in", 12, fill=DIM)
            if len(r) > 4:  # the one-line short form (EffectText::BuildShortForm)
                text(d, (LIST_X + 240, y + 14), r[4], 12, fill=DIM)
            text(d, (1206, y + 12), "%s EP" % ep, 14, True, anchor="ra")
        y += 50


def open_skill(d, x0, x1, y, name, el, tier, sub, stats, effects, req, ep=""):
    """X Details: the highlighted skill's row grows in place (Crown). Its header stays the row; under it the stat lines
    in three columns, the ET-50 effect tree and the requirement. Up / Down closes it; the right stick scrolls a tall one."""
    h = 44 + 10 + 22 * ((len(stats) + 2) // 3) + 8 + 20 * len(effects) + (26 if req else 0) + 6
    cut(d, (x0, y, x1, y + h), PANEL, CYAN, 6)
    cut(d, (x0, y, x1, y + 44), TEAL, CYAN, 6)
    d.regular_polygon((x0 + 22, y + 22, 8), 6, fill=ELEMENT[el])
    text(d, (x0 + 40, y + 11), name, 16, True)
    nx = x0 + 40 + int(d.textlength(name, font=F(16, True))) + 15
    tier_badge(d, nx, y + 21, tier, 10)
    text(d, (nx + 20, y + 14), sub, 12, fill=TXT)
    if ep:
        text(d, (x1 - 14, y + 12), ep, 14, True, anchor="ra")
    col = (x1 - x0 - 28) // 3
    yy = y + 54
    for k, (lab, val) in enumerate(stats):
        x = x0 + 14 + (k % 3) * col
        text(d, (x, yy + (k // 3) * 22), lab, 13, fill=DIM)
        text(d, (x + 72, yy + (k // 3) * 22), val, 13, True)
    yy += 22 * ((len(stats) + 2) // 3) + 8
    yy = tree(d, x0 + 14, yy, effects)
    if req:
        txt, met = req
        text(d, (x0 + 14, yy + 4), txt + ("  ✓ met" if met else "  ✗ not met"), 13, True, GOOD if met else RED)
    return y + h


FLAME_ARC = ("Flame Arc", "Fire", "C", "Ability · Fire from Garnet",
             [("EP", "6"), ("Element", "Fire"), ("Power", "120%"), ("Scaling", "Body B"),
              ("Targets", "Row"), ("Buildup", "Burn 20")],
             ["Activate", "  • Deal Fire damage to a row of Enemies",
              "On Hit", "  While the target is Burning", "    and your HP is above 50%",
              "      • Give the target -10% Defense for 2 turns, once per fight"],
             ("Needs Body 3 · Garrick has 4", True), "6 EP")

LR = 620  # the Skills tab keeps two columns: entries left, slots right

# ---- 6 / 6b: Skills tab (Generic): the left lists the same entries as the Weapons tab, names only; the right shows the
#         focused entry's skill slots, a dual entry as one list. X Details opens the highlighted row in place (6b).
FOOT_SKILLS = [("A", "Change"), ("X", "Details"), ("Y", "Options"), ("B", "Back")]
SKILLS_G = [
    ("Slash", "None", "built-in", 0), ("Quick Cut", "None", "built-in", 0),
    ("Twin Rend", "None", "slot", 5, "Dual-blade · Activate: Hit twice"),
    ("Flame Arc", "Fire", "slot", 6, "On Hit*: Give the target -10% Defense"),
    ("", None, "empty", 0)]


def skills_g(opened=None):
    im, d = canvas()
    header(d, 1, 1, "Bruiser  ·  2 of 5")
    text(d, (L, TOP), "Abilities by entry", 17, True, DIM)
    y = entry(d, 168, "", [("Ashen Blade", "Fire"), ("Cinder Edge", "None")], [("crystal", "Fire"), None], 1,
              active=True)
    entry(d, y, "", [("Quarry Maul", "Earth")], [("stone", None), None], 1)
    panel(d, (LIST_X, TOP, 1232, 654), focus=True)
    text(d, (LIST_X + 18, TOP + 10), "Ashen Blade + Cinder Edge", 16, True, DIM)
    skill_rows(d, SKILLS_G, 3, top=TOP + 40, opened=opened)
    footer(d, FOOT_SKILLS)
    return im


im = skills_g()
save(im, OUT, "6_skills_generic.png")
save(skills_g(FLAME_ARC), OUT, "6b_skill_details_open.png")

# ---- 6a: A on an empty slot opens the picker: abilities that fit; X Details opens the highlighted one in place.
im, d = window(im, "Pick an ability  ·  Ashen Blade + Cinder Edge", "",
               [("A", "Put in slot"), ("X", "Details"), ("B", "Cancel")])
text(d, (146, 128), "Abilities that fit", 15, True, DIM)
y = open_skill(d, 146, 1134, 156, "Ember Thrust", "Fire", "C", "Ability · Fire from Garnet",
               [("EP", "5"), ("Element", "Fire"), ("Power", "140%"), ("Scaling", "Body C"),
                ("Targets", "One"), ("Buildup", "Burn 15")],
               ["Activate", "  • Ignore 20% of the target's Defense",
                "On Critical Hit", "  While the target is Burning",
                "    • Deal 4 Fire damage to the target each turn for 2 turns"],
               ("Needs Body 2 · Garrick has 4", True), ep="5 EP")
item_rows(d, 146, 1134, y + 6, [
    ("Parry Edge", "None", "D", "3 EP · Activate: Gain +30% Defense for 1 turn", "", None),
    ("Inferno Dance", "Fire", "A", "", "", "Needs Mind 4 · Garrick has 2"),
    ("Flame Arc", "Fire", "C", "", "", "Already in this list")], -1)
save(im, OUT, "6a_skill_picker.png")

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
text(d, (LIST_X + 18, TOP + 10), "Innate spells", 16, True, DIM)
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
footer(d, FOOT_SKILLS)
save(im, OUT, "7_skills_refractor.png")
print("ok")
