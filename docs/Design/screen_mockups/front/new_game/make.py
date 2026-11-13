"""New game (UIScreens.md #11): the base body (Manny or Quinn), the class and, for a Refractor, its innate element
(CharacterForms.md section 1; Crown, 2026-11-13: the element choices sit under the Refractor description, and the
title and description change with the element: "Fire Refractor", ...). Each element's line is what its full status
bar does (Rules SB-19 to SB-28). Layout only: how the opening presents this is Crown's. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

CLASSES = [("Generic", "Weapons", "Two weapons, switch between them freely. Budget 5."),
           ("Refractor", "Innate magic", "Your own element and spell pool; one item. Budget 3."),
           ("Resonator", "Adapters", "Any number of adapters and one weapon. Budget 5.")]
ABOUT = {"Generic": "Fights with what it holds. Abilities come from the weapon; elements from its crystal.",
         "Resonator": "Channels through adapters, each with its own element and spells; switch between them in a fight."}
# The game's element order (ESpellElement). name -> what filling an enemy's status bar with it does.
ELEMENTS = [("Fire", "Burns: a full bar deals 8% of the target's max HP at its turn."),
            ("Water", "Slows: a full bar cuts the target's Spell Speed."),
            ("Earth", "Cracks armour: a full bar lowers the target's defence by 30%."),
            ("Wind", "Staggers: a full bar slows Action Speed and takes an action away."),
            ("Light", "Dazzles: a full bar stops the target landing critical hits."),
            ("Darkness", "Silences: a full bar seals half the target's EP."),
            ("Lightning", "Stuns: a full bar leaves the target only Attack and Defend."),
            ("Void", "Unravels: a full bar makes the target act at random for a turn."),
            ("Reality", "Breaks: a full bar deals 25% of the target's max HP at once.")]


def screen(name, body, cls, focus_row, element=None):
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
    for i, (n, what, line) in enumerate(CLASSES):
        y = 164 + i * 88
        lit = i == cls
        cut(d, (460, y, 880, y + 78), TEAL if lit and focus_row == 1 else PLATE, CYAN if lit else STEEL, 10)
        text(d, (478, y + 8), n, 21, True)
        text(d, (864, y + 12), what, 14, True, CYAN if lit else DIM, anchor="ra")
        wrap(d, (478, y + 40), line, 380, 14, TXT if lit else DIM)
    cname = CLASSES[cls][0]
    refractor = cname == "Refractor"
    panel(d, (910, 164, 1232, 596 if refractor else 420), focus=True)
    if refractor:
        el, line = ELEMENTS[element]
        d.ellipse((930, 180, 952, 202), fill=ELEMENT[el])
        text(d, (962, 176), el + " Refractor", 22, True)   # the title follows the element
        y = wrap(d, (930, 214), "Casts %s from within. The only class with an innate element and spells of its own." % el, 284, 15)
        y = wrap(d, (930, y + 6), line, 284, 15, ELEMENT[el] if el != "Reality" else (150, 135, 235))
        text(d, (930, y + 14), "Element", 16, True, CYAN if focus_row == 2 else DIM)
        gy = y + 42
        for i, (n, _) in enumerate(ELEMENTS):   # 3 x 3, in the game's element order
            x, yy = 930 + (i % 3) * 96, gy + (i // 3) * 46
            lit = i == element
            cut(d, (x, yy, x + 90, yy + 40), TEAL if lit and focus_row == 2 else PLATE, CYAN if lit else STEEL, 6)
            d.ellipse((x + 8, yy + 13, x + 22, yy + 27), fill=ELEMENT[n])
            text(d, (x + 27, yy + 20), n, 11, True, TXT if lit else DIM, anchor="lm")
        wrap(d, (930, gy + 146), "Your spells, your infusions and your status bar all use this element.", 284, 13, DIM)
    else:
        text(d, (930, 176), cname, 22, True)
        y = wrap(d, (930, 214), ABOUT[cname], 284, 15)
        wrap(d, (930, y + 14), "Gear, skills and forms are earned by playing.", 284, 14, DIM)
    f = focus_row == 3
    cut(d, (460, 440, 880, 496), TEAL if f else PLATE, CYAN if f else STEEL, 12)
    text(d, (670, 468), "Begin as %s %s" % (ELEMENTS[element][0] if refractor else "a", cname) if refractor else "Begin as a " + cname,
         20, True, anchor="mm")
    footer(d, [("A", "Select"), ("B", "Back to title")])
    save(im, OUT, name)


screen("1_new_game.png", 1, 1, 2, element=5)            # Refractor: choosing the element (Darkness)
screen("2_new_game_fire_refractor.png", 1, 1, 2, element=0)  # the same panel after moving to Fire: title and lines change
screen("3_new_game_generic.png", 0, 0, 1)               # a class with no innate element: no element row
print("ok")
