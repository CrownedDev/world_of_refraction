"""Loadout v2 (Crown, 2026-11-14; B-479 redraw for D-209 / D-210). Three columns, as FF7 Rebirth's and Expedition 33's
equip screens: what is EQUIPPED on the left, a character frame in the middle (the live 3D model in the current gear; a
placeholder here), and what can go in the focused piece or socket on the right, with a compare (ups green, downs red).
Items that can't go in stay listed, greyed, with the reason. Details on R3. Tabs Weapons | Apparel | Skills on LB / RB.
D-210: the Weapons tab is a list of ENTRIES. It starts with one empty frame, "Add gear"; the Add gear window asks weapon
or adapter, then Main hand, Off hand, Augment socket 1, Augment socket 2. The sockets belong to the entry (a dual pair
shares them): a crystal (or evolution) + a stone, or two stones (the same stat stacks). Confirming makes one entry and
a new empty frame shows the cells left ("3/5 left"), until the class limit or the cells run out. Generic: 2 weapon
entries + 1 adapter entry; Resonator: 1 weapon entry + up to 5 adapter entries; Refractor: 1 entry. Evolutions: a
Resonator only on adapters, a Generic only on weapons. Costs (D-206): gear 0, crystal 1, stone 1, evolution 2.
Dual wield: two weapons of the same type that can dual wield (D-209). Apparel: 2 slots, HP / Defence / Amp + one rolled
bonus line (D-208). Stat values are placeholders. Run: python make.py"""
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


def used_line(d, used, total, preview=None, note=""):
    text(d, (L, TOP), "Cells used", 17, True, DIM)
    if preview is None or preview == used:
        text(d, (L + 110, TOP), "%d of %d" % (used, total), 18, True)
    else:
        text(d, (L + 110, TOP), "%d → %d of %d" % (used, preview, total), 18, True, WARN if preview > used else GOOD)
    text(d, (LR, TOP + 3), note, 13, fill=DIM, anchor="ra")


def entry(d, y, title, pieces, sockets, cost, active=False, focus=None, h=64):
    """One confirmed entry. pieces: (name, el) for the main and off hand; sockets: two (name, el, kind) or None for empty.
    focus: "entry" or a socket index (0 / 1)."""
    on = focus is not None
    cut(d, (L, y, LR, y + h), TEAL if focus == "entry" else PLATE, CYAN if (on or active) else STEEL, 8)
    for i, (n, el) in enumerate(pieces):
        icon(d, (L + 8 + i * 22, y + 10 + i * 8, L + 40 + i * 22, y + 42 + i * 8), ELEMENT[el])
    tx = L + 20 + 22 * len(pieces) + 20
    text(d, (tx, y + 10), fit(d, title, 15, True, 150), 15, True)
    text(d, (tx, y + 34), fit(d, " · ".join(p[0] for p in pieces), 12, False, LR - tx - 180), 12,
         fill=TXT if focus == "entry" else DIM)
    for i, s in enumerate(sockets):
        sx = LR - 176 + i * 78
        if focus == i:
            cut(d, (sx - 4, y + 6, sx + 72, y + h - 6), TEAL, CYAN, 6)
        icon(d, (sx, y + 12, sx + 22, y + 34), ELEMENT[s[1]] if s else None, empty=s is None)
        text(d, (sx, y + 38), fit(d, s[0] if s else "empty", 11, True, 72), 11, True, TXT if s or focus == i else OFF)
        text(d, (sx + 26, y + 16), (s[2] if s else "socket %d" % (i + 1)), 10, fill=DIM)
    text(d, (LR - 12, y + h / 2), str(cost), 18, True, anchor="rm")
    if active:
        tw = int(d.textlength("ACTIVE", font=F(10, True))) + 10
        cut(d, (L + 160 - tw, y - 8, L + 160, y + 7), BG, CYAN, 4)
        text(d, (L + 160 - tw / 2, y - 6), "ACTIVE", 10, True, CYAN, anchor="ma")
    return y + h + 10


def add_frame(d, y, left, total, sub, focus=False, done=False):
    """The empty frame after the entries: Add gear, with the cells left."""
    cut(d, (L, y, LR, y + 56), TEAL if focus else BG, CYAN if focus else OFF, 8)
    if done:
        text(d, (L + 16, y + 9), "Loadout full", 16, True, OFF)
    else:
        text(d, (L + 16, y + 9), "+  Add gear", 16, True, TXT if focus else DIM)
    text(d, (LR - 14, y + 9), "%d/%d left" % (left, total), 16, True, CYAN if left else OFF, anchor="ra")
    text(d, (L + 16, y + 33), sub, 12, fill=TXT if focus else DIM)
    return y + 68


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
GEAR_G = "Ashen Blade + Cinder Edge · Iron Coat"
DUAL = ("Sword + Sword", [("Ashen Blade", "Fire"), ("Cinder Edge", "None")],
        [("Garnet", "Fire", "crystal"), ("Whetstone", "None", "stone")], 2)
HAMMER = ("Hammer", [("Quarry Maul", "Earth")], [("Ironstone", "Earth", "stone"), ("Ironstone", "Earth", "stone")], 2)

# ---- 1: Garrick (Generic) at the start: one empty frame, Add gear. The right side says what an entry is.
im, d = canvas()
header(d, 1, 0, "New loadout  ·  3 of 5")
used_line(d, 0, 5, note="2 weapon entries · 1 adapter entry")
y = add_frame(d, 168, 5, 5, "A weapon or an adapter", focus=True)
seat(d, y, "Ironheart", "Earth")
frame(d, "Garrick", "Generic", "Iron Coat")
panel(d, (LIST_X, TOP, 1232, 654), focus=True)
text(d, (LIST_X + 16, TOP + 10), "Add gear", 18, True)
y = TOP + 44
for head, body in [
        ("An entry", "A main hand and an off hand (a twin of the same type, or empty), and two augment sockets."),
        ("The two sockets", "Shared by the entry: a crystal or an evolution, plus a stone; or two stones. Two of the "
                            "same stone stack."),
        ("Garrick, a Generic", "Two weapon entries and one adapter entry, in 5 cells. Evolutions go on weapons only."),
        ("Cells", "Gear is free. A crystal or a stone fills 1 cell, an evolution 2.")]:
    text(d, (LIST_X + 16, y), head, 15, True, CYAN)
    y = wrap(d, (LIST_X + 16, y + 22), body, 404, 14, TXT) + 16
footer(d, [("A", "Add gear"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "1_weapons_generic_start.png")

# ---- 1a: two weapon entries and the "1/5 left" frame (only the adapter entry is left). The dual entry's socket 2 is
#          focused: socket 1 holds a crystal, so socket 2 takes a stone; a second Whetstone would stack.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 4, 5, note="2 of 2 weapon entries · 0 of 1 adapter")
y = entry(d, 172, *DUAL, active=True, focus=1)
y = entry(d, y, *HAMMER)
y = add_frame(d, y, 1, 5, "An adapter (the weapon entries are full)")
seat(d, y, "Ironheart", "Earth")
frame(d, "Garrick", "Generic", GEAR_G)
choices(d, "Socket 2  ·  a stone (socket 1 holds a crystal)", [
    ("Whetstone", "None", "C", "Stone · Power +4%", "", "worn"),
    ("Whetstone", "None", "B", "Stone · Power +6%", "1 cell", None),
    ("Keenstone", "Wind", "C", "Stone · Crit Rate +4%", "1 cell", None),
    ("Sapphire", "Water", "D", "", "", "Socket 1 holds a crystal: this one takes a stone"),
    ("Phoenix", "Fire", "B", "", "", "Socket 1 holds a crystal: no evolution here")], 1, [
    ("Cells", "4 of 5", "4 of 5", None), ("Power", "+4%", "+6%", True), ("Applies to", "both swords", "both swords", None)])
footer(d, FOOT)
save(im, OUT, "1a_weapons_generic_entries.png")


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
    """A piece of the entry being built; name None = still to pick."""
    text(d, (x0, y), label, 13, True, DIM)
    y += 20
    cut(d, (x0, y, x1, y + 50), TEAL if focus else (PLATE if name else BG), CYAN if focus else (STEEL if name else OFF), 8)
    icon(d, (x0 + 10, y + 7, x0 + 46, y + 43), ELEMENT[el] if name else None, empty=not name)
    text(d, (x0 + 56, y + 6), name or ("choosing…" if focus else "—"), 16, True, TXT if name or focus else OFF)
    text(d, (x0 + 56, y + 28), sub, 12, fill=TXT if focus else DIM)
    return y + 60


# ---- 1b: the Add gear window for a weapon entry: weapon picked over adapter; the main and off hand are set; socket 1
#          (crystal or evolution, or a stone) is focused. Socket 2 then takes a stone.
base, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 0, 5)
add_frame(d, 172, 5, 5, "A weapon or an adapter", focus=True)
frame(d, "Garrick", "Generic", "Iron Coat")
im, d = window(base, "Add gear", "5/5 left",
               [("A", "Pick"), ("X", "Remove"), ("Y", "Confirm entry"), ("B", "Cancel")])
for i, (lab, on) in enumerate([("Weapon", True), ("Adapter", False)]):
    bx = 146 + i * 128
    cut(d, (bx, 128, bx + 120, 160), TEAL if on else PLATE, CYAN if on else STEEL, 6)
    text(d, (bx + 60, 144), lab, 15, True, anchor="mm")
text(d, (412, 136), "asked first", 12, fill=DIM)
y = build_slot(d, 172, "Main hand  ·  sets the stance", "Ashen Blade", "Fire", "Sword · single or dual")
y = build_slot(d, y, "Off hand  ·  a sword that pairs, or empty", "Cinder Edge", "None", "Sword · single or dual")
y = build_slot(d, y, "Augment socket 1", None, None, "A crystal or an evolution, or a stone", focus=True)
y = build_slot(d, y, "Augment socket 2", None, None, "A stone (or a crystal if socket 1 has a stone)")
cut(d, (146, y + 2, 560, y + 42), BG, STEEL, 8)
text(d, (160, y + 12), "Sword + Sword", 16, True)
text(d, (546, y + 12), "0 cells so far", 15, True, CYAN, anchor="ra")
text(d, (590, 128), "Augment socket 1  ·  shared by both swords", 15, True, DIM)
item_rows(d, 590, 1134, 156, [
    ("Garnet", "Fire", "B", "Crystal · Fire", "1 cell", None),
    ("Phoenix", "Fire", "B", "Evolution · its passives at infusion 2", "2 cells", None),
    ("Whetstone", "None", "C", "Stone · Power +4%", "1 cell", None),
    ("Sapphire", "Water", "D", "Crystal · Water", "1 cell", None),
    ("Onyx", "Darkness", "S", "", "", "S-tier: one per loadout (on Iron Coat)"),
    ("Ironheart", "Earth", "A", "", "", "In Garrick's own evolution seat")], 0)
text(d, (590, 500), "The sword pair shares these two sockets: each weapon gives its", 13, fill=DIM)
text(d, (590, 520), "abilities and stats, the sockets count once.", 13, fill=DIM)
save(im, OUT, "1b_add_gear_weapon.png")

# ---- 2: Sera (Refractor): one entry, 2 cells; the innate spell pool is the star. Her evolution may go on a weapon or an
#         adapter.
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
used_line(d, 1, 2, note="1 entry")
y = entry(d, 172, "Staff", [("Cinder Staff", "Fire")], [("Garnet", "Fire", "crystal"), None], 1, focus="entry")
y = seat(d, y, None, None)
innate(d, y + 4, "Fire", 18, [
    ("Destruction", 3, "Flame Lance, Ember, Scorch"), ("Conjuration", 0, ""),
    ("Enhancement", 1, "Kindle"), ("Restoration", 2, "Warm Light, Cauterise"),
    ("Impairment", 1, "Smoulder"), ("Manipulation", 0, "")])
frame(d, "Sera", "Refractor  ·  Fire", "Cinder Staff · Ember Cloak")
choices(d, "Her one entry  ·  2 cells", [
    ("Cinder Staff", "Fire", "C", "Staff · Garnet", "1 cell", "worn"),
    ("Tidal Pike", "Water", "C", "Spear · Sapphire", "1 cell", None),
    ("Ember Ring", "Fire", "B", "Adapter · evolution", "2 cells", None),
    ("Phoenix Heart", "Fire", "B", "Evolution on its own", "2 cells", None),
    ("Pyre Pike", "Fire", "A", "", "", "Evolution + stone: needs 3 cells, 2 here"),
    ("Twin Fangs", "Wind", "C", "", "", "Twin only: needs two, she holds one item")], 1, [
    ("Cells", "1 of 2", "1 of 2", None), ("Power", "28", "34", True),
    ("Elements she casts", "Fire", "Fire, Water", True), ("Innate spells", "18 points", "18 points", None)])
footer(d, FOOT)
save(im, OUT, "2_weapons_refractor.png")

# ---- 3: Bram (Resonator): one weapon entry and his ring entries (up to 5), then the frame with the cells left. His
#         weapon's socket 1 is focused: an evolution is greyed, Resonators put evolutions on adapters only.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
used_line(d, 4, 5, preview=5, note="1 weapon entry · 3 of 5 rings")
y = entry(d, 172, "Fists", [("Cinder Fists", "None")], [None, None], 0, focus=0, h=58)
y = entry(d, y, "Ring", [("Ember Ring", "Fire")], [("Garnet", "Fire", "crystal"), None], 1, active=True, h=58)
y = entry(d, y, "Ring", [("Storm Ring", "Lightning")], [("Amber", "Lightning", "crystal"), None], 1, h=58)
y = entry(d, y, "Ring", [("Tide Ring", "Water")], [("Tidal", "Water", "evolution"), None], 2, h=58)
y = add_frame(d, y, 1, 5, "A ring (2 more at most)")
seat(d, y, "Emberheart", "Fire")
frame(d, "Bram", "Resonator", "Cinder Fists · Ember Ring")
choices(d, "Cinder Fists  ·  augment socket 1", [
    ("Ironstone", "Earth", "C", "Stone · Defence +5%", "1 cell", None),
    ("Garnet", "Fire", "B", "Crystal · Fire", "1 cell", None),
    ("Keenstone", "Wind", "C", "Stone · Crit Rate +4%", "1 cell", None),
    ("Phoenix", "Fire", "B", "", "", "Resonators put evolutions on adapters only"),
    ("Onyx", "Darkness", "S", "", "", "S-tier: one per loadout (on his coat)")], 1, [
    ("Cells", "4 of 5", "5 of 5", None), ("Element", "none", "Fire", None),
    ("Elemental abilities", "off", "on", True), ("Fire sources", "1 (ring)", "2", True)])
footer(d, [("A", "Socket"), ("X", "Empty it"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "3_weapons_resonator.png")


def apparel(d, y, name, el, lines, bonus, sock, focus=False):
    cut(d, (L, y, LR, y + 112), TEAL if focus else PLATE, CYAN if focus else STEEL, 10)
    icon(d, (L + 10, y + 10, L + 66, y + 66), ELEMENT[el])
    text(d, (L + 78, y + 10), name, 19, True)
    text(d, (L + 78, y + 38), lines, 14, fill=TXT if focus else DIM)
    text(d, (L + 78, y + 60), bonus, 14, True, CYAN)
    icon(d, (L + 10, y + 76, L + 36, y + 102), ELEMENT[sock[1]] if sock[0] else None, empty=not sock[0])
    text(d, (L + 44, y + 80), (sock[0] or "empty") + "  ·  Crystal / evolution", 13, fill=DIM)


# ---- 4: Apparel tab: two slots, no budget. Each piece: HP / Defence / Amp + one rolled bonus (a sub-stat, or a pillar
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
save(im, OUT, "4_apparel.png")
print("ok")
