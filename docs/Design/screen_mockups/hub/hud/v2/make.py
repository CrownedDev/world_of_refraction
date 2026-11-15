"""Hub and trial HUD v2 (D-216, D-217, Q-503l): one party card in the battle card's style, showing what the place needs
(hub: the name; trial: HP, EP and the status build-up), the field attack strip above the played card (trial only),
and the LB wheel. The place name, the money chip and the interact prompt stay. Values are placeholders.
Run: python make.py"""
import math
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

X0, BOTTOM, GAP = 48, 700, 12
PARTY = [("Sera", "Darkness", (52, 84), (30, 40), 0.0, "Fire"),  # name, element, HP, EP, build-up, build-up element
         ("Garrick", "Generic", (67, 84), (18, 30), 0.35, "Earth"),
         ("Bram", "Fire", (87, 97), (40, 52), 0.1, "Fire")]


def on_bar(d, x, y, w, h, frac, col, label):
    """A bar with its number on it (battle card style)."""
    cut(d, (x, y, x + w, y + h), TRACK, None, 3)
    if frac > 0:
        cut(d, (x, y, x + max(6, int(w * frac)), y + h), col, None, 3)
    d.text((x + w - 6, y + h / 2), label, font=F(h - 3, True), fill=TXT, anchor="rm", stroke_width=2, stroke_fill=BG)


def build_up(d, x, y, w, frac, col):
    """The one status build-up bar: thin, chevron-ended, with a quiet label."""
    text(d, (x, y - 2), "Build-up", 11, fill=OFF)
    bx = x + 58
    bw = w - 58
    d.polygon([(bx, y + 4), (bx + 5, y), (bx + bw - 5, y), (bx + bw, y + 4), (bx + bw - 5, y + 8), (bx + 5, y + 8)],
              fill=TRACK, outline=STEEL)
    if frac > 0:
        fw = int((bw - 10) * frac)
        d.polygon([(bx, y + 4), (bx + 5, y), (bx + 5 + fw, y), (bx + 5 + fw, y + 8), (bx + 5, y + 8)], fill=col)


def hub_cards(d, played):
    """Hub: the name only; each card shrinks to fit it."""
    x = X0
    for i, (name, *_rest) in enumerate(PARTY):
        w = int(d.textlength(name, font=F(17, True))) + 32
        cut(d, (x, BOTTOM - 40, x + w, BOTTOM), TEAL if i == played else PANEL, CYAN if i == played else STEEL, 8)
        text(d, (x + 16, BOTTOM - 20), name, 17, True, anchor="lm")
        x += w + GAP


CARD_W, CARD_H = 196, 96


def trial_cards(d, played):
    """Trial: name, HP on its bar, EP on its bar, the build-up bar. Returns the played card's box."""
    box = None
    for i, (name, el, hp, ep, bu, bu_el) in enumerate(PARTY):
        x = X0 + i * (CARD_W + GAP)
        y = BOTTOM - CARD_H
        cut(d, (x, y, x + CARD_W, BOTTOM), PANEL, CYAN if i == played else STEEL, 10)
        text(d, (x + 14, y + 8), name, 17, True)
        on_bar(d, x + 14, y + 34, CARD_W - 28, 18, hp[0] / hp[1], ROSE, "%d/%d" % hp)
        on_bar(d, x + 14, y + 56, CARD_W - 28, 14, ep[0] / ep[1], EP, "%d/%d" % ep)
        build_up(d, x + 14, y + 80, CARD_W - 28, bu, ELEMENT[bu_el])
        if i == played:
            box = (x, y, x + CARD_W, BOTTOM)
    return box


def diamond(d, x, y, r, col):
    d.polygon([(x, y - r), (x + r, y), (x, y + r), (x - r, y)], fill=col, outline=(230, 234, 242))


def strip(d, box, label, el):
    """The field attack strip, directly above the played card: what the field attack is now; LB changes it."""
    x0, y0, x1, _ = box
    cut(d, (x0, y0 - 36, x1, y0 - 6), PLATE, CYAN, 6)
    diamond(d, x0 + 18, y0 - 21, 7, ELEMENT[el])
    text(d, (x0 + 34, y0 - 21), label, 16, True, anchor="lm")
    cut(d, (x1 - 40, y0 - 31, x1 - 8, y0 - 11), BG, STEEL, 4)
    text(d, (x1 - 24, y0 - 21), "LB", 11, True, CYAN, anchor="mm")
    return y0 - 36


def wheel(d, cx, cy, entries, lit, r=118, hole=46):
    """The LB wheel (hold LB): every source and element in one ring; the stick or mouse wheel picks, release confirms."""
    d.ellipse((cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4), fill=BG)
    n = len(entries)
    step = 360 / n
    for i, (label, el, src) in enumerate(entries):
        a0 = -90 - step / 2 + i * step
        d.pieslice((cx - r, cy - r, cx + r, cy + r), a0, a0 + step, fill=TEAL if i == lit else PANEL,
                   outline=CYAN if i == lit else STEEL, width=2)
    d.ellipse((cx - hole, cy - hole, cx + hole, cy + hole), fill=BG, outline=STEEL, width=2)
    for i, (label, el, src) in enumerate(entries):
        a = math.radians(-90 + i * step)
        lx, ly = cx + math.cos(a) * (r + hole) / 2, cy + math.sin(a) * (r + hole) / 2
        diamond(d, lx, ly - 13, 6, ELEMENT[el])
        for k, word in enumerate(label.split(" ")[-2:] if len(label) > 9 else [label]):
            text(d, (lx, ly + 1 + k * 15), word, 13, True, TXT if i == lit else DIM, anchor="ma")
    name, el, src = entries[lit]
    text(d, (cx, cy - 6), "from", 10, fill=OFF, anchor="mm")
    text(d, (cx, cy + 9), src, 12, True, CYAN, anchor="mm")


def field_hints(d, wheel_open=False):
    if wheel_open:
        footer(d, [("L-stick", "Pick"), ("LB", "Let go: confirm")], y=694)
    else:
        footer(d, [("RB", "Attack"), ("LB", "Infusion (hold: wheel)"), ("X", "Interact")], y=694)


def top(d, place, gold=True):
    text(d, (48, 40), place, 15, fill=OFF)
    if gold:
        cut(d, (1010, 40, 1232, 84), PANEL, STEEL, 8)
        chip(d, 1026, 50, "Gold", "8,420", GOLD)


def prompt(d, cx, cy, key, verb, what):
    w = int(d.textlength(verb + "  " + what, font=F(17, True))) + 70
    cut(d, (cx - w // 2, cy, cx + w // 2, cy + 40), PANEL, CYAN, 8)
    glyph(d, cx - w // 2 + 26, cy + 20, key, 12)
    text(d, (cx - w // 2 + 48, cy + 20), verb, 17, True, anchor="lm")
    text(d, (cx - w // 2 + 56 + int(d.textlength(verb, font=F(17, True))), cy + 20), what, 17, fill=DIM, anchor="lm")


def trial_scene():
    im, d = canvas("world")
    d.rectangle((0, 0, W, 300), fill=(26, 18, 40))  # the trial's sky: violet, not the hub's blue
    for x, h in ((140, 160), (260, 110), (980, 190), (1130, 130)):
        d.rectangle((x, 300 - h, x + 46, 520), fill=(14, 12, 22))
    d.ellipse((760, 380, 840, 460), fill=(8, 8, 12))  # an enemy wandering ahead
    d.ellipse((782, 410, 792, 420), fill=(255, 120, 60))
    d.ellipse((808, 410, 818, 420), fill=(255, 120, 60))
    d.rectangle((560, 330, 610, 520), fill=(18, 16, 28))  # a rest spot shrine by the path
    return im, d


# ---- 1: the hub. Names only; no HP, EP, status or strip.
im, d = canvas("world")
d.rectangle((600, 250, 680, 520), fill=(10, 11, 16))
d.ellipse((614, 200, 666, 252), fill=(10, 11, 16))
prompt(d, 640, 150, "X", "Talk", "Blacksmith")
top(d, "Void Realm")
hub_cards(d, 1)
footer(d, [("X", "Interact")], y=694)
save(im, OUT, "1_hub.png")

# ---- 2: the trial. Full trial cards; Garrick is played: his strip reads Fire Sword (Garnet's infusion).
im, d = trial_scene()
prompt(d, 585, 270, "X", "Rest", "Shrine")
top(d, "Ashen Hollow  ·  floor 2")
box = trial_cards(d, 1)
strip(d, box, "Fire Sword", "Fire")
field_hints(d)
save(im, OUT, "2_trial.png")

# ---- 3: LB held: the wheel opens above the strip. Garrick's choices: plain, then each source (Q-503k: one stop per element).
im, d = trial_scene()
top(d, "Ashen Hollow  ·  floor 2")
box = trial_cards(d, 1)
y = strip(d, box, "Fire Sword", "Fire")
wheel(d, (box[0] + box[2]) // 2, y - 132, [("Sword", "None", "the weapon"), ("Fire Sword", "Fire", "Garnet"),
                                           ("Earth Sword", "Earth", "Ironheart"), ("Lightning Sword", "Lightning", "Storm Ring")], 1)
field_hints(d, wheel_open=True)
save(im, OUT, "3_trial_wheel_open.png")

# ---- 4: a Refractor (Sera, the Cinder Staff's Garnet): no weapon line, the strip names the element only.
im, d = trial_scene()
prompt(d, 585, 270, "X", "Rest", "Shrine")
top(d, "Ashen Hollow  ·  floor 2")
box = trial_cards(d, 0)
strip(d, box, "Fire", "Fire")
field_hints(d)
save(im, OUT, "4_trial_caster.png")
print("ok")
