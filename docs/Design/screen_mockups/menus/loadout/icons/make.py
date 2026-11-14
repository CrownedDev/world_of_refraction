"""Loadout icons trial (Crown, 2026-11-14): the deckbuilder's badges and effect icons on the loadout screen as the
PIE build draws it today (LoadoutV2ScreenWidget: chip rows, the list panel with Cells used, rows with ACTIVE at the
right end, + Add weapon, the details box, key caps). Icon sources: README.md in this folder.
Choice: a crystal is its element badge INSTEAD of the "(Garnet)" text, right after the name; the empty socket is the
socket badge. Stat values are placeholders.
Run: python make.py   (needs the deckbuilder clone at D:/github/wor-deckbuilder)"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

LIB = "D:/github/wor-deckbuilder/wor.game/assets/ui/library/"
PONETI = "D:/github/wor-deckbuilder/wor.game/assets/ui/vendor/PONETI/ClassicFantasyRPG_UI/ARTWORKS/UIicons/Colored/"
L, LR, BX = 48, 620, 650


def art(path, size):
    """An icon, fitted into a size x size square (aspect kept)."""
    im = Image.open(path).convert("RGBA")
    im.thumbnail((size, size), Image.LANCZOS)
    return im


def put(im, path, x, y, size):
    """Paste an icon centred vertically on y, from x; returns the x after it."""
    a = art(path, size)
    im.paste(a, (int(x), int(y - a.height / 2)), a)
    return x + a.width


def element(el):
    return LIB + "badges/element_%s.png" % el if el else LIB + "badges/element_socket.png"


def tier(t):
    return LIB + "badges/tier_%s.png" % t.lower()


def effect(name):
    return LIB + "icons/effect_%s.png" % name


SWORD = PONETI + "13_Sword.png"


def chips(d, page):
    x = tabs(d, 48, 30, ["Sera", "Garrick", "Bram"], 1, lead="LT")
    text(d, (x + 4, 37), "RT", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Weapons", "Skills", "Apparel", "Cosmetics"], page, lead="LB")
    text(d, (x + 4, 85), "RB", 16, True, CYAN)


def row(im, d, y, name, sockets, tag="", focus=False):
    """A list row: weapon icon, name, then each socket as a badge (crystal = its element, empty = the socket badge)."""
    cut(d, (L + 12, y, LR - 12, y + 46), TEAL if focus else PLATE, CYAN if focus else STEEL, 6)
    put(im, SWORD, L + 22, y + 23, 30)
    text(d, (L + 60, y + 13), name, 16, True)
    x = L + 66 + d.textlength(name, font=F(16, True))
    for el in sockets:
        x = put(im, element(el), x, y + 23, 26) + 4
    if tag:
        text(d, (LR - 26, y + 15), tag, 13, True, CYAN, anchor="ra")


def box_line(im, d, y, icon_path, label, value, size=24, lit=False):
    """A details line: an icon (badge or effect), the label, the value."""
    if icon_path:
        put(im, icon_path, BX + 22, y + 10, size)
    text(d, (BX + 56, y), label, 15, lit, TXT if lit else DIM)
    if value:
        text(d, (BX + 210, y), value, 15, True)
    return y + 32


# ---- 1: the Weapons tab, as PIE draws it, with the icons.
im, d = canvas()
d = ImageDraw.Draw(im)
chips(d, 0)
panel(d, (L, 134, LR, 654))
text(d, (L + 16, 148), "Cells used", 16, True, DIM)
text(d, (L + 120, 148), "1 of 5", 16, True)
row(im, d, 184, "Generic Primary Weapon", ["fire", None], tag="ACTIVE", focus=True)
row(im, d, 238, "Generic Secondary Weapon", [None, None])
cut(d, (L + 12, 292, LR - 12, 338), BG, OFF, 6)
text(d, (L + 28, 305), "+ Add weapon", 16, True, DIM)
text(d, (LR - 26, 305), "4/5 left", 15, True, CYAN, anchor="ra")

panel(d, (BX, 134, 1232, 654), focus=True)
text(d, (BX + 18, 150), "Generic Primary Weapon", 20, True)
put(im, tier("C"), BX + 30 + d.textlength("Generic Primary Weapon", font=F(20, True)), 162, 30)
y = 196
y = box_line(im, d, y, SWORD, "Sword", "tier C", lit=True)
y = box_line(im, d, y, None, "Power", "30")
y = box_line(im, d, y, None, "Scaling", "Body C")
y = box_line(im, d, y, None, "Speed", "Normal")
y += 6
text(d, (BX + 18, y), "Effects", 14, True, CYAN)
y += 28
y = box_line(im, d, y, effect("amp_up_down_fire"), "Fire Amp +1", "", size=22)
y = box_line(im, d, y, effect("damage_over_time_fire"), "On Hit*: Burn 11% max HP each turn, 2 turns", "", size=22)
y += 6
text(d, (BX + 18, y), "Sockets", 14, True, CYAN)
y += 28
y = box_line(im, d, y, element("fire"), "Socket 1", "Garnet", size=26)
put(im, tier("C"), BX + 214 + d.textlength("Garnet", font=F(15, True)), y - 22, 22)
y = box_line(im, d, y, element(None), "Socket 2", "empty", size=26)
y += 6
box_line(im, d, y, None, "Skill slots", "8")
footer(d, [("A", "Edit"), ("X", "Simple"), ("Y", "Options"), ("B", "Back")])
im.save(os.path.join(OUT, "1_weapons_icons.png"), optimize=True)

# ---- 2: the Add gear window on Augment socket 1: each crystal row shows its element badge and tier badge.
base = im
im = Image.blend(base, Image.new("RGB", base.size, BG), 0.72)
d = ImageDraw.Draw(im)
panel(d, (120, 60, 1160, 660), focus=True)
text(d, (146, 78), "Edit  ·  Generic Primary Weapon  ·  Augment socket 1", 20, True)
d.line((146, 116, 1134, 116), fill=STEEL)
text(d, (146, 128), "Crystals you own", 15, True, DIM)
CRYSTALS = [("Garnet", "fire", "C", "Fire Amp +1 · Burn 11%/turn", "EQUIPPED"),
            ("Sapphire", "water", "D", "Water Amp +1 · Survive one KO", ""),
            ("Citrine", "lightning", "B", "Lightning Amp +2 · Restore EP", ""),
            ("Emerald", "wind", "C", "Wind Amp +1 · Turn speed +18%", ""),
            ("Amber", "earth", "E", "Earth Amp +0 · Defense up", ""),
            ("Onyx", "darkness", "S", "One S-tier crystal per loadout", "greyed")]
y = 156
for i, (name, el, t, line, tag) in enumerate(CRYSTALS):
    grey = tag == "greyed"
    f = i == 1
    cut(d, (146, y, 640, y + 56), TEAL if f else (BG if grey else PLATE), CYAN if f else (OFF if grey else STEEL), 6)
    put(im, element(el), 156, y + 28, 40)
    text(d, (206, y + 8), name, 16, True, OFF if grey else TXT)
    put(im, tier(t), 212 + d.textlength(name, font=F(16, True)), y + 18, 22)
    text(d, (206, y + 32), line, 12, fill=WARN if grey else (TXT if f else DIM))
    if tag == "EQUIPPED":
        text(d, (626, y + 10), tag, 11, True, CYAN, anchor="ra")
    y += 62
# The box: the highlighted crystal's own details (B-484), with its badges and effect icons.
cut(d, (664, 156, 1134, 520), PANEL, STEEL, 10)
put(im, element("water"), 680, 190, 44)
text(d, (734, 170), "Sapphire", 20, True)
put(im, tier("D"), 744 + d.textlength("Sapphire", font=F(20, True)), 182, 26)
text(d, (734, 196), "Water crystal · 1 cell", 12, fill=DIM)
y = 236
for path, line in [(effect("amp_up_down_water"), "Water Amp +1"),
                   (effect("revive"), "Once per fight: survive a KO at 1 HP"),
                   (effect("resistance_up_down_water"), "Water resistance +10%")]:
    put(im, path, 682, y + 10, 22)
    text(d, (716, y), line, 14, True)
    y += 32
text(d, (680, y + 10), "Fits: None, Generic and Water abilities", 12, fill=DIM)
footer(d, [("A", "Pick"), ("X", "Compare"), ("Y", "Confirm"), ("B", "Back")], y=632, right=1140)
im.save(os.path.join(OUT, "2_add_gear_crystal_icons.png"), optimize=True)
print("ok")
