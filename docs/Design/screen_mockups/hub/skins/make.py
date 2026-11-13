"""Spirit of {character} (was Skins / Forms; D-184. UIScreens.md #19; CharacterForms.md): each authored character's meter fills as you win fights wearing
their pieces; at 100% the skin unlocks and their loadout is revealed. Hub and rest spots only.
Words from CharacterFormWidget.cpp. Character and piece names are placeholders. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

im, d = canvas()
title(d, "Spirit of Sera", "Wearing: your own form (base body Quinn)")
FORMS = [("your own form", "None", 100, True), ("Garnet Lord", "Fire", 100, False), ("Amethyst Prisma", "Void", 64, False),
         ("Onyx Lord", "Darkness", 12, False), ("???", "None", 0, False)]
for i, (n, el, pct, worn) in enumerate(FORMS):
    y = 130 + i * 84
    f = i == 2
    unknown = n == "???"
    cut(d, (48, y, 600, y + 74), TEAL if f else (BG if unknown else PLATE), CYAN if f else (OFF if unknown else STEEL), 10)
    icon(d, (60, y + 9, 116, y + 65), ELEMENT[el], empty=unknown)
    text(d, (132, y + 10), n, 20, True, OFF if unknown else TXT)
    if i == 0:
        text(d, (132, y + 40), "always yours", 14, fill=DIM)
    elif not unknown:
        bar(d, 132, y + 46, 300, pct / 100, GOLD if pct == 100 else CYAN, 10)
        text(d, (444, y + 40), "%d%%" % pct, 15, True, GOLD if pct == 100 else TXT)
    if worn:
        text(d, (584, y + 14), "WORN", 13, True, CYAN, anchor="ra")
    elif pct == 100:
        text(d, (584, y + 14), "unlocked", 13, fill=GOLD, anchor="ra")
panel(d, (630, 130, 1232, 620), focus=True)
cut(d, (650, 150, 850, 430), BG, STEEL, 10)
text(d, (750, 290), "form preview", 15, fill=OFF, anchor="mm")
text(d, (872, 150), "Amethyst Prisma", 24, True)
text(d, (872, 186), "64%", 20, True, CYAN)
wrap(d, (872, 220), "Locked. Wear their pieces in fights you win to fill the meter.", 340, 15)
text(d, (872, 290), "Their pieces you own", 15, True, DIM)
for i, (p, have) in enumerate([("Prisma's Staff", True), ("Violet Ring", True), ("?", False), ("?", False), ("?", False), ("?", False)]):
    x, y = 872 + (i % 3) * 112, 318 + (i // 3) * 56
    icon(d, (x, y, x + 44, y + 44), ELEMENT["Void"], empty=not have, letter=None if have else "?")
    text(d, (x + 50, y + 14), "worn" if have and i == 0 else ("owned" if have else ""), 12, fill=CYAN if i == 0 else DIM)
d.line((650, 450, 1212, 450), fill=STEEL)
wrap(d, (650, 464), "At 100% the form unlocks and their full loadout is revealed. A form changes looks and makes their bound gear work; "
     "it gives no stats.", 562, 15, DIM)
text(d, (650, 560), "Change form in the hub or at a rest spot.", 14, fill=OFF)
text(d, (650, 584), "Y  Base body: Quinn / Manny", 14, fill=CYAN)
footer(d, [("A", "Wear this form"), ("Y", "Base body"), ("B", "Back")])
save(im, OUT, "1_forms.png")
print("ok")
