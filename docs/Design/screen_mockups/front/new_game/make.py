"""New game (UIScreens.md #11): the only choices are the base body (Manny or Quinn) and the class
(CharacterForms.md section 1). Layout only: how the opening presents this is Crown's. Class lines are the code's
class identity (D-110): Generic = weapons, Refractor = innate magic, Resonator = adapters. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def screen(name, body, cls, focus_row):
    im, d = canvas()
    title(d, "Who wakes", "placeholder heading")
    text(d, (48, 136), "Body", 16, True, CYAN if focus_row == 0 else DIM)
    for i, n in enumerate(["Manny", "Quinn"]):
        x = 48 + i * 196
        lit = i == body
        cut(d, (x, 164, x + 184, 420), TEAL if lit and focus_row == 0 else PLATE, CYAN if lit else STEEL, 12)
        cut(d, (x + 22, 184, x + 162, 370), BG, STEEL, 8)  # the body preview (the void / galaxy material)
        text(d, (x + 92, 277), "preview", 14, fill=OFF, anchor="mm")
        text(d, (x + 92, 394), n, 20, True, anchor="mm")
    text(d, (460, 136), "Class", 16, True, CYAN if focus_row == 1 else DIM)
    CLASSES = [("Generic", "Weapons", "Two weapons, switch between them freely. Budget 5."),
               ("Refractor", "Innate magic", "Your own element and spell pool; one item. Budget 3."),
               ("Resonator", "Adapters", "Any number of adapters and one weapon. Budget 5.")]
    for i, (n, what, line) in enumerate(CLASSES):
        y = 164 + i * 88
        lit = i == cls
        cut(d, (460, y, 880, y + 78), TEAL if lit and focus_row == 1 else PLATE, CYAN if lit else STEEL, 10)
        text(d, (478, y + 8), n, 21, True)
        text(d, (864, y + 12), what, 14, True, CYAN if lit else DIM, anchor="ra")
        wrap(d, (478, y + 40), line, 380, 14, TXT if lit else DIM)
    panel(d, (910, 164, 1232, 420), focus=True)
    text(d, (930, 176), CLASSES[cls][0], 22, True)
    y = wrap(d, (930, 214), {0: "Fights with what it holds. Abilities come from the weapon; elements from its crystal.",
                             1: "Casts from within. The only class with an innate element and spells of its own.",
                             2: "Channels through adapters, each with its own element and spells; switch between them in a fight."}[cls],
             284, 15)
    wrap(d, (930, y + 14), "Gear, skills and forms are earned by playing.", 284, 14, DIM)
    f = focus_row == 2
    cut(d, (460, 440, 880, 496), TEAL if f else PLATE, CYAN if f else STEEL, 12)
    text(d, (670, 468), "Begin", 22, True, anchor="mm")
    footer(d, [("A", "Select"), ("B", "Back to title")])
    save(im, OUT, name)


screen("1_new_game.png", 1, 1, 1)
print("ok")
