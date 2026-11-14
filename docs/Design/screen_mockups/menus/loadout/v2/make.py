"""Loadout v2 (Crown, 2026-11-14). Three columns, as FF7 Rebirth's and Expedition 33's equip screens: what is EQUIPPED on
the left, a character frame in the middle (the live 3D model in the current gear; a placeholder here), and what can go
in the focused slot or socket on the right, always in view, with a compare (ups green, downs red). Items that can't go
in stay listed, greyed, with the reason. Details on R3. Tabs Weapons | Apparel | Skills on LB / RB.
Weapons tab: the budget IS the slots. Each budget point is one cell, an item fills as many cells as it costs (bare
weapon 1, weapon with crystal or stone 2, evolved weapon 3, adapter 1, evolved adapter 2, evolution alone 3; CL-10).
Generic: Primary row 3 cells + Secondary row 2 (CL-03); Refractor: one row of 3 + its innate spell pool shown big
(CL-04, CL-34, CL-42); Resonator: one row of 5, the weapon first, then rings, the active ring marked (CL-05). Each
shows the character's own evolution seat. Apparel: 2 slots, HP / Defence / Amp + one rolled bonus line (CL-50); a
pillar bonus raises the pillar's value ("+5% Body"), not its points. Stat values are placeholders. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

RED = ROSE_LOW
L, LR = 48, 480            # left column
MID_L, MID_R = 500, 772     # character frame
LIST_X = 792                # right column
CW, CH, G = 80, 96, 8       # cell width, height, gap
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


def cells(d, y, n, items, label, focus=None):
    """A row of n cells. items: (name, el, cost, sockets, active); an item spans `cost` cells; the rest show empty.
    sockets: [element colour or None] drawn small on the item. focus: index of the focused item, or "empty"."""
    text(d, (L, y), label, 14, True, DIM)
    y += 30
    x, used = L, 0
    for i, (name, el, cost, socks, active) in enumerate(items):
        w = cost * CW + (cost - 1) * G
        f = focus == i
        cut(d, (x, y, x + w, y + CH), TEAL if f else PLATE, CYAN if (f or active) else STEEL, 8)
        if cost > 1:
            icon(d, (x + 8, y + 8, x + 44, y + 44), ELEMENT[el])
            text(d, (x + 52, y + 10), fit(d, name, 15, True, w - 60), 15, True)
        else:
            icon(d, (x + 22, y + 8, x + 58, y + 44), ELEMENT[el])
            text(d, (x + w / 2, y + 50), fit(d, name, 11, True, w - 8), 11, True, anchor="ma")
        for j, s in enumerate(socks):
            sx = x + 8 + j * 24
            icon(d, (sx, y + CH - 28, sx + 20, y + CH - 8), s, empty=s is None)
        text(d, (x + w - 8, y + CH - 8), str(cost), 15, True, TXT, anchor="rd")
        if active:
            tw = int(d.textlength("ACTIVE", font=F(11, True))) + 10
            cut(d, (x + w / 2 - tw / 2, y - 9, x + w / 2 + tw / 2, y + 8), BG, CYAN, 4)
            text(d, (x + w / 2, y - 7), "ACTIVE", 11, True, CYAN, anchor="ma")
        x += w + G
        used += cost
    for k in range(n - used):
        f = focus == "empty" and k == 0
        cut(d, (x, y, x + CW, y + CH), TEAL if f else BG, CYAN if f else OFF, 8)
        text(d, (x + CW / 2, y + CH / 2), "+", 22, fill=TXT if f else OFF, anchor="mm")
        x += CW + G
    return y + CH + 14


def used_line(d, used, total, preview=None):
    text(d, (L, TOP), "Cells used", 17, True, DIM)
    if preview is None or preview == used:
        text(d, (L + 110, TOP), "%d of %d" % (used, total), 18, True)
    else:
        text(d, (L + 110, TOP), "%d → %d of %d" % (used, preview, total), 18, True, WARN if preview > used else GOOD)


def sockets_strip(d, y, owner, socks, focus=None):
    """The focused item's sockets, large: (kind, name, el). focus: socket index."""
    cut(d, (L, y, LR, y + 64), BG, STEEL, 8)
    text(d, (L + 12, y + 6), owner + "'s sockets", 13, True, DIM)
    for i, (kind, name, el) in enumerate(socks):
        sx = L + 12 + i * 208
        if focus == i:
            cut(d, (sx - 4, y + 24, sx + 196, y + 60), TEAL, CYAN, 6)
        icon(d, (sx, y + 28, sx + 28, y + 56), ELEMENT[el] if name else None, empty=not name)
        text(d, (sx + 36, y + 27), name or "empty", 14, True, TXT if name or focus == i else OFF)
        text(d, (sx + 36, y + 44), kind, 12, fill=TXT if focus == i else DIM)
    return y + 78


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
    text(d, (cx, 630), gear, 12, fill=DIM, anchor="ma")


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


def choices(d, heading, rows, focus, compare):
    """The right column. rows: (name, el, tier, sub, right, state); state None, "worn" or a reason it can't go in
    (greyed). compare: [(label, from, to, good)] for the focused row."""
    panel(d, (LIST_X, TOP, 1232, 654), focus=True)
    text(d, (LIST_X + 16, TOP + 10), heading, 15, True, DIM)
    y = TOP + 38
    for i, (n, el, t, sub, right, state) in enumerate(rows):
        greyed = state not in (None, "worn")
        f = i == focus
        cut(d, (LIST_X + 10, y, 1222, y + 50), TEAL if f else (BG if greyed else PLATE), CYAN if f else (OFF if greyed else STEEL), 8)
        icon(d, (LIST_X + 18, y + 7, LIST_X + 54, y + 43), ELEMENT[el])
        text(d, (LIST_X + 64, y + 5), n, 16, True, OFF if greyed else TXT)
        tier_badge(d, LIST_X + 64 + int(d.textlength(n, font=F(16, True))) + 15, y + 14, t, 10)
        text(d, (LIST_X + 64, y + 28), fit(d, state if greyed else sub, 12, False, 300), 12,
             fill=WARN if greyed else (TXT if f else DIM))
        if state == "worn":
            text(d, (1210, y + 8), "ON NOW", 11, True, CYAN, anchor="ra")
        text(d, (1210, y + 28), right, 13, True, OFF if greyed else TXT, anchor="ra")
        y += 55
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

# ---- 1: Garrick (Generic): Primary row 3 cells, Secondary row 2. The primary item is focused; a swap leaves its crystal
#         on the old weapon, so Gale Spear (bare) fills 1 cell.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 4, 5, preview=3)
y = cells(d, 168, 3, [("Ashen Blade", "Fire", 2, [FIRE, None], True)], "Primary  ·  weapon, adapter or evolution", focus=0)
y = cells(d, y, 2, [("Quarry Maul", "Earth", 2, [EARTH, None], False)], "Secondary  ·  a weapon or nothing")
y = sockets_strip(d, y, "Ashen Blade", [("Crystal", "Garnet", "Fire"), ("Stone", None, None)])
seat(d, y, "Ironheart", "Earth")
frame(d, "Garrick", "Generic", "Ashen Blade · Quarry Maul · Iron Coat")
choices(d, "Fits the primary  ·  3 cells", [
    ("Ashen Blade", "Fire", "B", "Sword · Garnet", "2 cells", "worn"),
    ("Gale Spear", "Wind", "C", "Spear", "1 cell", None),
    ("Ember Ring", "Fire", "C", "Adapter · casts its spells", "1 cell", None),
    ("Quarry Maul", "Earth", "C", "", "", "In the secondary slot"),
    ("Stormbreaker", "Lightning", "A", "Hammer · evolved", "3 cells", None)], 1, [
    ("Cells", "4 of 5", "3 of 5", True), ("Power", "42", "36", False), ("Scaling", "Body B", "Body A", True),
    ("Element", "Fire (Garnet)", "none", None), ("Abilities", "Cleave, Whirl…", "Pierce, Sweep…", None)])
footer(d, FOOT)
save(im, OUT, "1_weapons_generic.png")

# ---- 2: Sera (Refractor): one row of 3 cells, the gear small; the innate spell pool is the star.
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
used_line(d, 2, 3)
y = cells(d, 168, 3, [("Cinder Staff", "Fire", 2, [FIRE, None], False)], "Gear  ·  one item", focus=0)
y = seat(d, y, None, None)
innate(d, y + 4, "Fire", 18, [
    ("Destruction", 3, "Flame Lance, Ember, Scorch"), ("Conjuration", 0, ""),
    ("Enhancement", 1, "Kindle"), ("Restoration", 2, "Warm Light, Cauterise"),
    ("Impairment", 1, "Smoulder"), ("Manipulation", 0, "")])
frame(d, "Sera", "Refractor  ·  Fire", "Cinder Staff · Ember Cloak")
choices(d, "Fits the gear slot  ·  3 cells", [
    ("Cinder Staff", "Fire", "C", "Staff · Garnet", "2 cells", "worn"),
    ("Tidal Pike", "Water", "C", "Spear · Sapphire", "2 cells", None),
    ("Ember Ring", "Fire", "B", "Adapter · evolved", "2 cells", None),
    ("Phoenix Heart", "Fire", "B", "Evolution on its own", "3 cells", None),
    ("Quarry Maul", "Earth", "C", "", "", "Worn by Garrick in this loadout")], 1, [
    ("Cells", "2 of 3", "2 of 3", None), ("Power", "28", "34", True),
    ("Elements she casts", "Fire", "Fire, Water", True), ("Innate spells", "18 points", "18 points", None)])
footer(d, FOOT)
save(im, OUT, "2_weapons_refractor.png")

# ---- 3: Bram (Resonator): one row of 5 cells, the weapon first, then rings; he casts the active ring.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
used_line(d, 5, 5)
y = cells(d, 168, 5, [("Cinder Fists", "None", 1, [None, None], False), ("Ember Ring", "Fire", 1, [FIRE], True),
                      ("Storm Ring", "Lightning", 1, [LIGHTNING], False), ("Tide Ring", "Water", 2, [WATER], False)],
          "Weapon, then rings  ·  no ring cap, only the cells", focus=1)
y = sockets_strip(d, y, "Ember Ring", [("Crystal", "Garnet", "Fire"), ("Stone", None, None)])
seat(d, y, "Emberheart", "Fire")
frame(d, "Bram", "Resonator", "Cinder Fists · Ember Ring")
choices(d, "Rings for this cell  ·  6 owned", [
    ("Ember Ring", "Fire", "C", "Ring · Garnet", "1 cell", "worn"),
    ("Gale Band", "Wind", "C", "Ring · Wind", "1 cell", None),
    ("Frost Ring", "Water", "D", "Ring · Water", "1 cell", None),
    ("Pyre Ring", "Fire", "B", "", "", "Evolved: needs 2 cells, this one has 1"),
    ("Storm Ring", "Lightning", "C", "", "", "Already in the row")], 1, [
    ("Cells", "5 of 5", "5 of 5", None), ("Spells", "Flame Lance…", "Gust, Updraft…", None),
    ("Durability", "64 / 80", "80 / 80", True)])
footer(d, FOOT)
save(im, OUT, "3_weapons_resonator.png")

# ---- 4: a crystal socket focused (Garrick's Quarry Maul, bare): the crystals that fit; a crystal adds a cell.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
used_line(d, 3, 5, preview=4)
y = cells(d, 168, 3, [("Ashen Blade", "Fire", 2, [FIRE, None], True)], "Primary  ·  weapon, adapter or evolution")
y = cells(d, y, 2, [("Quarry Maul", "Earth", 1, [None, None], False)], "Secondary  ·  a weapon or nothing", focus=0)
y = sockets_strip(d, y, "Quarry Maul", [("Crystal", None, None), ("Stone", None, None)], focus=0)
seat(d, y, "Ironheart", "Earth")
frame(d, "Garrick", "Generic", "Ashen Blade · Quarry Maul · Iron Coat")
choices(d, "Crystals that fit  ·  14 owned", [
    ("Garnet", "Fire", "B", "Fire crystal", "+1 cell", None),
    ("Citrine", "Earth", "C", "Earth crystal", "+1 cell", None),
    ("Sapphire", "Water", "D", "Water crystal", "+1 cell", None),
    ("Onyx", "Darkness", "S", "", "", "S-tier: one per loadout (on Iron Coat)"),
    ("Quartz", "None", "E", "Crystal", "+1 cell", None)], 0, [
    ("Cells", "3 of 5", "4 of 5", None), ("Element", "Earth (weapon)", "Fire", None),
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
frame(d, "Garrick", "Generic", "Ashen Blade · Quarry Maul · Iron Coat")
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
