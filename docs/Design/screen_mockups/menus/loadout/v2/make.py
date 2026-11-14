"""Loadout v2 (Crown, 2026-11-14): tabs Weapons | Apparel | Skills on LB / RB. What is EQUIPPED is on the left (slot rows
with their sockets and costs); what can go in the focused slot or socket is on the right, always in view, replacing
v1's detail panel and picker popup. The focused right-hand item shows how it compares: the budget change and stat
ups (green) / downs (red), as in Expedition 33's and Persona 5's equip screens. Items that can't go in stay listed,
greyed, with the reason. Details on R3. Apparel has no budget.
One Weapons tab per class, laid out by docs/Rules/01-classes-and-loadouts.md: Generic a primary (weapon, adapter or
evolution) + a secondary weapon, budget 5 (CL-03, CL-12); Refractor one primary item, budget 3, plus its innate
spell pool by school (CL-04, CL-34, CL-42); Resonator a primary weapon or evolution + adapters, budget 5 (CL-05,
CL-10). Costs: weapon frame 1, adapter frame 0, crystal / stone +1, evolution on gear +2, evolution alone 3 (CL-10).
Apparel: HP / Defence / Amp + one rolled bonus line (CL-50, CL-50c). Stat values are placeholders. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

RED = ROSE_LOW
LEFT_R, LIST_X = 620, 650  # left column's right edge; right column's left edge


def header(d, who, page, loadout):
    x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], who, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Weapons", "Apparel", "Skills"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)
    text(d, (1232, 36), loadout, 20, True, anchor="ra")
    text(d, (1232, 66), "Y  Saved loadouts", 15, fill=CYAN, anchor="ra")


def budget(d, used, total, preview=None, x=48, y=134):
    """The budget pips. preview: the total if the focused right-hand item went in (amber if it rises, green if it falls)."""
    text(d, (x, y), "Budget", 17, True, DIM)
    for i in range(total):
        px = x + 90 + i * 40
        on = i < (min(used, preview) if preview is not None else used)
        change = preview is not None and min(used, preview) <= i < max(used, preview)
        col = CYAN if on else ((WARN if preview > used else GOOD) if change else TRACK)
        cut(d, (px, y - 2, px + 32, y + 22), col, None, 6)
    if preview is None or preview == used:
        text(d, (x + 100 + total * 40, y), "%d of %d" % (used, total), 18, True)
    else:
        text(d, (x + 100 + total * 40, y), "%d → %d of %d" % (used, preview, total), 18, True, WARN if preview > used else GOOD)


def slot(d, y, kind, name, el, cost, sockets, focus=None, tag=None, h=78, sock_x=340):
    """One equipped slot. focus: "slot", or the index of the focused socket."""
    on = focus == "slot"
    cut(d, (48, y, LEFT_R, y + h), TEAL if on else PLATE, CYAN if on else STEEL, 10)
    if name:
        icon(d, (58, y + 9, 118, y + 69), ELEMENT[el])
        text(d, (130, y + 10), name, 19, True)
        text(d, (130, y + 38), kind, 14, fill=TXT if on else DIM)
        if tag:
            nx = 140 + int(d.textlength(name, font=F(19, True)))
            tw = int(d.textlength(tag, font=F(12, True))) + 12
            cut(d, (nx, y + 12, nx + tw, y + 32), BG, CYAN, 5)
            text(d, (nx + 6, y + 14), tag, 12, True, CYAN)
    else:
        icon(d, (58, y + 9, 118, y + 69), empty=True, letter="+")
        text(d, (130, y + 26), "Empty " + kind.lower(), 18, fill=OFF)
    for i, (sk, sname, scol) in enumerate(sockets):
        sx = sock_x + i * 118
        if focus == i:
            cut(d, (sx - 6, y + 6, sx + 112, y + h - 6), TEAL, CYAN, 6)
        if sname:
            icon(d, (sx, y + 14, sx + 32, y + 46), scol)
            text(d, (sx + 38, y + 14), sname, 14, True)
        else:
            icon(d, (sx, y + 14, sx + 32, y + 46), empty=True, letter="+")
            text(d, (sx + 38, y + 14), "empty", 14, fill=OFF if focus != i else TXT)
        text(d, (sx + 38, y + 34), sk, 12, fill=TXT if focus == i else DIM)
    if cost is not None:
        text(d, (LEFT_R - 14, y + h / 2), cost, 20, True, TXT if name else OFF, anchor="rm")


def innate(d, y, element, used, schools):
    """A Refractor's innate spell pool: its element, the 24-point budget, and spells per school (cap 6 each)."""
    panel(d, (48, y, LEFT_R, y + 196))
    icon(d, (64, y + 14, 96, y + 46), ELEMENT[element])
    text(d, (108, y + 14), "Innate spells", 18, True)
    text(d, (108, y + 38), "Innate element: " + element, 13, fill=DIM)
    text(d, (LEFT_R - 16, y + 18), "%d of 24 points" % used, 16, True, anchor="ra")
    for i, (school, n) in enumerate(schools):
        sx, sy = 64 + (i % 2) * 280, y + 74 + (i // 2) * 34
        text(d, (sx, sy), school, 15, fill=TXT if n else OFF)
        for p in range(6):
            px = sx + 128 + p * 18
            cut(d, (px, sy + 3, px + 13, sy + 16), ELEMENT[element] if p < n else TRACK, None, 3)
    text(d, (64, y + 176), "Edit them on the Skills tab.", 13, fill=DIM)


def choices(d, heading, rows, focus, compare, top=134):
    """The right column: what fits the focused slot or socket. rows: (name, el, tier, sub, right, state) with state
    None, "worn" (on now) or a reason it can't go in (greyed). compare: [(label, from, to, good)] for the focused row."""
    panel(d, (LIST_X, top, 1232, 654), focus=True)
    text(d, (LIST_X + 18, top + 10), heading, 16, True, DIM)
    y = top + 40
    for i, (n, el, t, sub, right, state) in enumerate(rows):
        greyed = state not in (None, "worn")
        f = i == focus
        cut(d, (LIST_X + 12, y, 1220, y + 50), TEAL if f else (BG if greyed else PLATE), CYAN if f else (OFF if greyed else STEEL), 8)
        icon(d, (LIST_X + 20, y + 7, LIST_X + 56, y + 43), ELEMENT[el])
        text(d, (LIST_X + 66, y + 5), n, 17, True, OFF if greyed else TXT)
        tier_badge(d, LIST_X + 66 + int(d.textlength(n, font=F(17, True))) + 16, y + 15, t, 10)
        text(d, (LIST_X + 66, y + 28), state if greyed else sub, 13, fill=WARN if greyed else (TXT if f else DIM))
        if state == "worn":
            text(d, (1206, y + 8), "ON NOW", 12, True, CYAN, anchor="ra")
        text(d, (1206, y + 28), right, 14, True, OFF if greyed else TXT, anchor="ra")
        y += 56
    d.line((LIST_X + 18, y + 4, 1214, y + 4), fill=STEEL)
    text(d, (LIST_X + 18, y + 14), "If you equip " + rows[focus][0], 15, True, DIM)
    y += 42
    for lab, a, b, good in compare:
        col = TXT if good is None else (GOOD if good else RED)
        arrow = "" if good is None else ("  ▲" if good else "  ▼")
        text(d, (LIST_X + 18, y), lab, 15, fill=DIM)
        text(d, (LIST_X + 210, y), a, 15)
        text(d, (LIST_X + 330, y), "→", 15, fill=DIM)
        text(d, (LIST_X + 356, y), b + arrow, 15, True, col)
        y += 26


FOOT = [("A", "Equip"), ("X", "Unequip"), ("R3", "Details"), ("B", "Back")]
GARNET, CITRINE = ("Crystal", "Garnet", ELEMENT["Fire"]), ("Crystal", "Citrine", ELEMENT["Earth"])
NO_STONE = ("Stone", None, None)

# ---- 1: Garrick (Generic): a primary slot (weapon, adapter or evolution) and a secondary weapon; the active one gives
#         its abilities, Switch Weapon in a fight. Primary focused; swapping a weapon leaves its crystal on it.
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
budget(d, 4, 5, preview=3)
slot(d, 172, "Primary · Sword", "Ashen Blade", "Fire", "2", [GARNET, NO_STONE], focus="slot", tag="ACTIVE")
slot(d, 258, "Secondary · Hammer", "Quarry Maul", "Earth", "2", [CITRINE, NO_STONE])
wrap(d, (48, 354), "Two items. The primary holds a weapon, an adapter or an evolution; the secondary a weapon or "
     "nothing. Only the active weapon gives its abilities: switch in a fight (free).", 560, 14, DIM)
choices(d, "Fits the primary slot  ·  weapon, adapter, evolution", [
    ("Ashen Blade", "Fire", "B", "Sword · Garnet", "cost 2", "worn"),
    ("Gale Spear", "Wind", "C", "Spear", "cost 1", None),
    ("Ember Ring", "Fire", "C", "Adapter · casts its spells", "cost 1", None),
    ("Quarry Maul", "Earth", "C", "", "", "In the secondary slot"),
    ("Night Scythe", "Darkness", "A", "Scythe · Jet", "cost 2", None)], 1, [
    ("Budget", "4 of 5", "3 of 5", True), ("Power", "42", "36", False), ("Scaling", "Body B", "Body A", True),
    ("Element", "Fire (Garnet)", "none", None), ("Abilities", "Cleave, Whirl…", "Pierce, Sweep…", None)])
footer(d, FOOT)
save(im, OUT, "1_weapons_generic.png")

# ---- 2: Sera (Refractor): one item in the primary slot, budget 3, and the innate spell pool by school. Her innate
#         element is Fire; a crystal on her gear channels its element for her spells too (CL-36).
im, d = canvas()
header(d, 0, 0, "Pyre  ·  1 of 5")
budget(d, 2, 3, preview=2)
slot(d, 172, "Primary · Staff", "Cinder Staff", "Fire", "2", [GARNET, NO_STONE], focus="slot")
innate(d, 268, "Fire", 18, [("Destruction", 3), ("Conjuration", 0), ("Enhancement", 1),
                            ("Restoration", 2), ("Impairment", 1), ("Manipulation", 0)])
choices(d, "Fits the primary slot  ·  one item", [
    ("Cinder Staff", "Fire", "C", "Staff · Garnet", "cost 2", "worn"),
    ("Tidal Pike", "Water", "C", "Spear · Sapphire", "cost 2", None),
    ("Ember Ring", "Fire", "B", "Adapter · Phoenix evolution", "cost 2", None),
    ("Phoenix Heart", "Fire", "B", "Evolution on its own", "cost 3", None),
    ("Quarry Maul", "Earth", "C", "", "", "Worn by Garrick in this loadout")], 1, [
    ("Budget", "2 of 3", "2 of 3", None), ("Power", "28", "34", True),
    ("Elements she casts", "Fire", "Fire, Water", True), ("Innate spells", "18 points", "18 points", None)])
footer(d, FOOT)
save(im, OUT, "2_weapons_refractor.png")

# ---- 3: Bram (Resonator): a primary weapon or evolution (never an adapter) and his adapters; he casts the active one.
im, d = canvas()
header(d, 2, 0, "Ember Rings  ·  1 of 5")
budget(d, 5, 5, preview=4)
slot(d, 172, "Primary · Fists", "Cinder Fists", "None", "1", [("Crystal", None, None), NO_STONE])
slot(d, 258, "Adapter · Ring", "Ember Ring", "Fire", "1", [GARNET, NO_STONE], focus="slot", tag="ACTIVE")
slot(d, 344, "Adapter · Ring", "Storm Ring", "Lightning", "1", [("Crystal", "Amber", ELEMENT["Lightning"]), NO_STONE])
slot(d, 430, "Adapter · Ring", "Tide Ring", "Water", "2", [("Evolution", "Tidal", ELEMENT["Water"]), NO_STONE])
slot(d, 516, "Adapter", None, None, "0", [])
choices(d, "Adapters that fit  ·  5 owned", [
    ("Ember Ring", "Fire", "C", "Ring · Garnet", "cost 1", "worn"),
    ("Gale Band", "Wind", "C", "Ring · Wind", "cost 0", None),
    ("Frost Ring", "Water", "D", "Ring · Water", "cost 0", None),
    ("Storm Ring", "Lightning", "C", "", "", "In adapter slot 2"),
    ("Tide Ring", "Water", "B", "", "", "In adapter slot 3")], 1, [
    ("Budget", "5 of 5", "4 of 5", True), ("Spells", "Flame Lance…", "Gust, Updraft…", None),
    ("Durability", "64 / 80", "80 / 80", True)])
footer(d, FOOT)
save(im, OUT, "3_weapons_resonator.png")

# ---- 4: a crystal socket focused (Garrick's secondary, empty): the right lists the crystals that fit. The S-tier one
#         stays listed, greyed, with its reason (one S-tier per loadout, CL-13b; his Iron Coat holds one).
im, d = canvas()
header(d, 1, 0, "Bruiser  ·  2 of 5")
budget(d, 3, 5, preview=4)
slot(d, 172, "Primary · Sword", "Ashen Blade", "Fire", "2", [GARNET, NO_STONE], tag="ACTIVE")
slot(d, 258, "Secondary · Hammer", "Quarry Maul", "Earth", "1", [("Crystal", None, None), NO_STONE], focus=0)
wrap(d, (48, 354), "A crystal keys the weapon to its element and powers its elemental abilities.", 560, 14, DIM)
choices(d, "Crystals that fit  ·  14 owned", [
    ("Garnet", "Fire", "B", "Fire crystal", "+1", None),
    ("Citrine", "Earth", "C", "Earth crystal", "+1", None),
    ("Sapphire", "Water", "D", "Water crystal", "+1", None),
    ("Onyx", "Darkness", "S", "", "", "S-tier: one per loadout (on Iron Coat)"),
    ("Quartz", "None", "E", "Crystal", "+1", None)], 0, [
    ("Budget", "3 of 5", "4 of 5", False), ("Element", "Earth (weapon)", "Fire", None),
    ("Elemental abilities", "off", "on", True), ("Fire sources", "1", "2 (they stack)", True)])
footer(d, [("A", "Socket"), ("X", "Empty it"), ("R3", "Details"), ("B", "Back")])
save(im, OUT, "4_crystal_socket.png")

# ---- 5: Apparel tab: two slots, no budget. Each piece: HP / Defence / Amp + one rolled bonus line (a sub-stat or a
#         pillar), so the compare shows it. Its socket takes a crystal or an evolution (no stone).
im, d = canvas()
header(d, 1, 1, "Bruiser  ·  2 of 5")
text(d, (48, 134), "Apparel doesn't use the budget.", 16, fill=DIM)
slot(d, 172, "HP 40 · Def 14 · Amp +2 · +2 Body", "Iron Coat", "Darkness", None,
     [("Crystal", "Onyx", ELEMENT["Darkness"])], focus="slot", sock_x=440)
slot(d, 258, "HP 25 · Def 6 · Amp +1 · +4% Crit Rate", "Shade Hood", "None", None, [("Crystal", None, None)], sock_x=440)
choices(d, "Apparel that fits  ·  5 owned", [
    ("Iron Coat", "Darkness", "C", "HP 40 · Def 14 · Amp +2 · +2 Body", "", "worn"),
    ("Ember Cloak", "Fire", "C", "HP 30 · Def 10 · Amp +4 · +6% Crit Damage", "", None),
    ("Warden Plate", "None", "B", "HP 55 · Def 20 · Amp +0 · +1 Spirit", "", None),
    ("Shade Hood", "None", "D", "", "", "In apparel slot 2"),
    ("Prisma's Mantle", "Void", "B", "", "", "Bound to Amethyst Prisma: unlock that Spirit")], 1, [
    ("HP", "40", "30", False), ("Defence", "14", "10", False), ("Amp", "+2", "+4", True),
    ("Body", "+2", "—", False), ("Crit Damage", "—", "+6%", True), ("Resistance", "Darkness", "Fire", None)])
footer(d, FOOT)
save(im, OUT, "5_apparel.png")
print("ok")
