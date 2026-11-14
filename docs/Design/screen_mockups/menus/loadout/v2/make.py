"""Loadout v2 (Crown, 2026-11-14; B-479 redraw for D-209 / D-210 and Crown's notes). Two columns: what is EQUIPPED on the
left, the owned items that fit on the right with a compare (ups green, downs red, as Expedition 33's and Persona 5's
equip screens). Items that can't go in stay listed, greyed, with the reason. Tabs Weapons | Apparel | Skills | Cosmetics
(later) on LB / RB. Main keys: A Edit, X Details (an entry's or the evolution's stats and attachments), Y Options
(Saved loadouts, Remove all gear, Help, later Cosmetics shortcuts), B Back; help text is not on screen.
D-210: the Weapons tab is a list of ENTRIES with an empty "Add weapon" frame at the bottom showing the cells left
("1/5 left"). The Add gear window asks Weapon, Adapter or Evolution, then Main hand, Off hand, Augment socket 1 and 2
(shared by the entry: a crystal or evolution + a stone, or two stones). Removing happens there (X Remove). The
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
    x = tabs(d, 48, 78, ["Weapons", "Apparel", "Skills"], page, lead="LB")
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
    text(d, (L + 46, y + 10), "Evolution:", 15, fill=DIM)
    text(d, (L + 130, y + 10), name or "none", 15, True, TXT if name else OFF)
    if name:
        text(d, (LR - 12, y + 20), "2", 18, True, anchor="rm")
    return y + 50


def entry(d, y, title, pieces, sockets, cost, active=False, focus=False, h=62):
    """One confirmed entry: its weapons, its two shared sockets (icons only) and its cells."""
    cut(d, (L, y, LR, y + h), TEAL if focus else PLATE, CYAN if (focus or active) else STEEL, 8)
    for i, (n, el) in enumerate(pieces):
        icon(d, (L + 8 + i * 22, y + 9 + i * 8, L + 40 + i * 22, y + 41 + i * 8), ELEMENT[el])
    tx = L + 40 + 22 * len(pieces) + 10
    text(d, (tx, y + 10), title, 16, True)
    text(d, (tx, y + 34), " · ".join(p[0] for p in pieces), 13, fill=TXT if focus else DIM)
    for i, s in enumerate(sockets):
        sx = LR - 130 + i * 44
        socket_icon(d, (sx, y + 13, sx + 36, y + 49), s)
    text(d, (LR - 14, y + h / 2), str(cost), 20, True, anchor="rm")
    if active:
        tw = int(d.textlength("ACTIVE", font=F(10, True))) + 10
        tx2 = tx + int(d.textlength(title, font=F(16, True))) + 10
        cut(d, (tx2, y + 11, tx2 + tw, y + 27), BG, CYAN, 4)
        text(d, (tx2 + tw / 2, y + 13), "ACTIVE", 10, True, CYAN, anchor="ma")
    return y + h + 8


def add_frame(d, y, left, total, focus=False):
    """The empty frame at the bottom of the entries."""
    cut(d, (L, y, LR, y + 44), TEAL if focus else BG, CYAN if focus else OFF, 8)
    text(d, (L + 16, y + 12), "+  Add weapon", 16, True, TXT if focus else DIM)
    text(d, (LR - 14, y + 12), "%d/%d left" % (left, total), 16, True, CYAN if left else OFF, anchor="ra")
    return y + 54


def innate(d, y, element, used, schools):
    """A Refractor's innate spell pool, the star of its tab: element, the 24-point budget, spells per school (6 cap)."""
    panel(d, (L, y, LR, 654), focus=False)
    icon(d, (L + 14, y + 14, L + 62, y + 62), ELEMENT[element])
    text(d, (L + 74, y + 12), "Innate spells", 20, True)
    text(d, (L + 74, y + 40), "Element: " + element, 14, fill=DIM)
    text(d, (LR - 14, y + 16), "%d of 24" % used, 18, True, anchor="ra")
    text(d, (LR - 14, y + 40), "points", 12, fill=DIM, anchor="ra")
    for i, (school, n, names) in enumerate(schools):
        sy = y + 76 + i * 28
        text(d, (L + 14, sy), school, 15, True, TXT if n else OFF)
        for p in range(6):
            px = L + 146 + p * 16
            cut(d, (px, sy + 3, px + 12, sy + 16), ELEMENT[element] if p < n else TRACK, None, 2)
        text(d, (L + 256, sy + 1), fit(d, names, 13, False, LR - L - 270), 13, fill=DIM)


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
#         is the one left). The dual entry is focused; the right lists swords that could take its place.
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


# ---- 2: the Add gear window, Weapon: main and off hand set, socket 1 focused. X Remove takes a piece out here.
im, d = window(screen_g(), "Add gear", "3/5 left",
               [("A", "Pick"), ("X", "Remove"), ("Y", "Confirm"), ("View", "Help"), ("B", "Cancel")])
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
               [("A", "Pick"), ("X", "Remove"), ("Y", "Confirm"), ("View", "Help"), ("B", "Cancel")])
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

# ---- 3: Sera (Refractor): one entry, 2 cells; the innate spell pool is the star. Her one entry is full, so no frame.
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
used_line(d, 1, 2, note="1 entry")
y = evo_line(d, 168, None, None)
y = entry(d, y, "Staff", [("Cinder Staff", "Fire")], [("crystal", "Fire"), None], 1, focus=True)
innate(d, y + 4, "Fire", 18, [
    ("Destruction", 3, "Flame Lance, Ember, Scorch"), ("Conjuration", 0, ""),
    ("Enhancement", 1, "Kindle"), ("Restoration", 2, "Warm Light, Cauterise"),
    ("Impairment", 1, "Smoulder"), ("Manipulation", 0, "")])
choices(d, "Her one entry  ·  2 cells", [
    ("Cinder Staff", "Fire", "C", "Staff · Garnet", "1 cell", "worn"),
    ("Tidal Pike", "Water", "C", "Spear · Sapphire", "1 cell", None),
    ("Ember Ring", "Fire", "B", "Adapter · evolution", "2 cells", None),
    ("Pyre Pike", "Fire", "A", "", "", "Evolution + stone: needs 3 cells, 2 here"),
    ("Twin Fangs", "Wind", "C", "", "", "Twin only: needs two, she holds one item")], 1, [
    ("Cells", "1 of 2", "1 of 2", None), ("Power", "28", "34", True),
    ("Elements she casts", "Fire", "Fire, Water", True), ("Innate spells", "18 points", "18 points", None)])
footer(d, FOOT)
save(im, OUT, "3_weapons_refractor.png")

# ---- 4: Bram (Resonator): his evolution, one weapon entry and his ring entries (up to 5), then the Add weapon frame.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
used_line(d, 4, 5, note="1 weapon entry · 3 of 5 rings")
y = evo_line(d, 168, None, None)
y = entry(d, y, "Fists", [("Cinder Fists", "None")], [None, None], 0, h=56)
y = entry(d, y, "Ring", [("Ember Ring", "Fire")], [("crystal", "Fire"), None], 1, active=True, focus=True, h=56)
y = entry(d, y, "Ring", [("Storm Ring", "Lightning")], [("crystal", "Lightning"), None], 1, h=56)
y = entry(d, y, "Ring", [("Tide Ring", "Water")], [("evolution", "Water"), None], 2, h=56)
add_frame(d, y, 1, 5)
choices(d, "Rings that fit this entry  ·  A Edit to change it", [
    ("Ember Ring", "Fire", "C", "Ring · casts Fire spells", "", "worn"),
    ("Gale Band", "Wind", "C", "Ring · casts Wind spells", "", None),
    ("Frost Ring", "Water", "D", "Ring · casts Water spells", "", None),
    ("Storm Ring", "Lightning", "C", "", "", "Already an entry"),
    ("Cinder Fists", "None", "C", "", "", "A weapon: his one weapon entry is used")], 1, [
    ("Spells", "Flame Lance…", "Gust, Updraft…", None), ("Durability", "64 / 80", "80 / 80", True),
    ("Element", "Fire (crystal)", "Fire (crystal)", None)])
footer(d, FOOT)
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
header(d, 1, 1, "Bruiser  ·  2 of 5")
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
print("ok")
