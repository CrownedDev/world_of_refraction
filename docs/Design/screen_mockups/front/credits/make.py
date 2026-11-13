"""Credits (UIScreens.md #13): a slow scroll over the title backdrop; B leaves, hold A speeds it up.
Names are placeholders except the asset credits CREDITS.md already lists. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

im, d = canvas()
for i in range(9):
    d.regular_polygon((880, 330, 300 - i * 30), 6, rotation=30, fill=(18 + i * 2, 22 + i * 3, 32 + i * 4))
d.text((320, 90), "World of Refraction", font=T(30), fill=TXT, anchor="ma")
y = 170
for head, names in [("Created by", ["Crown"]), ("UI art", ["Hidris"]), ("Fonts", ["Spectral SC, Lato (SIL Open Font License)"]),
                    ("Assets", ["HD Spell VFX: ARCLIGHT · godboyhappy", "Classic Fantasy RPG UI Kit · PONETI"]), ("Built with", ["Unreal Engine"])]:
    text(d, (320, y), head, 14, True, CYAN, anchor="ma")
    y += 26
    for n in names:
        text(d, (320, y), n, 19, anchor="ma")
        y += 30
    y += 22
text(d, (320, 650), "▲ scrolls on its own", 13, fill=OFF, anchor="ma")
footer(d, [("A", "Hold: faster"), ("B", "Back")])
save(im, OUT, "1_credits.png")
print("ok")
