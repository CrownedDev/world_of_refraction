"""Trial gate (UIScreens.md #21): pick the variation and the difficulty, see who is going, enter.
Difficulty changes how hard enemies hit and the payout, not their HP (TrialGate.h / .cpp: payout x0.75 / x1.0 / x1.5).
Variation names are the examples from RunStructure section 3 (Past, Present, Future); the real ones are Crown's.
Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

DIFF = [("Easy", "x0.8", "x0.75"), ("Medium", "x1.0", "x1.0"), ("Hard", "x1.2", "x1.5")]


def gate(name, var_lit, diff_lit, focus_row, coop=False):
    im, d = canvas("world")
    im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), 0.45)
    d = ImageDraw.Draw(im)
    d.ellipse((60, 46, 84, 70), fill=ELEMENT["Fire"])
    title(d, "Ember Trial", "The Garnet Lord's trial · Fire", x=98)
    # variations: big cards, because this is the story choice (the boss and its story change, the rules don't)
    text(d, (48, 136), "Variation", 16, True, CYAN if focus_row == 0 else DIM)
    for i, (v, boss, line) in enumerate([("Past", "A younger lord", "Before the forge went cold."),
                                         ("Present", "The Garnet Lord", "As the trial stands today."),
                                         ("Future", "???", "Not reached yet.")]):
        x = 48 + i * 232
        locked = i == 2
        lit = i == var_lit
        cut(d, (x, 164, x + 220, 330), TEAL if lit and focus_row == 0 else PLATE if not locked else BG,
            CYAN if lit else (OFF if locked else STEEL), 12)
        text(d, (x + 16, 176), v, 24, True, OFF if locked else TXT)
        text(d, (x + 16, 214), "Boss", 13, fill=OFF if locked else DIM)
        text(d, (x + 16, 232), boss, 16, True, OFF if locked else TXT)
        wrap(d, (x + 16, 268), line, 190, 14, OFF if locked else (TXT if lit else DIM))
        if lit:
            text(d, (x + 204, 180), "✓", 18, True, CYAN, anchor="ra")
    # difficulty: three plain tabs with what each changes, in the code's words
    text(d, (48, 356), "Difficulty", 16, True, CYAN if focus_row == 1 else DIM)
    for i, (n, dmg, pay) in enumerate(DIFF):
        x = 48 + i * 232
        lit = i == diff_lit
        cut(d, (x, 384, x + 220, 470), TEAL if lit and focus_row == 1 else PLATE, CYAN if lit else STEEL, 10)
        text(d, (x + 16, 392), n, 20, True)
        text(d, (x + 16, 422), "Enemy damage " + dmg, 14, fill=TXT if lit else DIM)
        text(d, (x + 16, 442), "Payout " + pay, 14, fill=GOOD if pay == "x1.5" else (TXT if lit else DIM))
    wrap(d, (48, 486), "Enemies hit harder and fight smarter; they don't get more HP. You can't change this once inside.", 680, 14, DIM)
    # who is going
    panel(d, (780, 136, 1232, 470))
    text(d, (800, 148), "Going in", 16, True, DIM)
    party = [("Sera", "Darkness Refractor", "Darkness", "C", "You"), ("Garrick", "Generic", "Generic", "B", "AI · Medium"),
             ("Bram", "Resonator", "Fire", "C", "Kestrel" if coop else "AI · Medium")]
    for i, (n, cls, el, t, who) in enumerate(party):
        y = 180 + i * 62
        icon(d, (800, y, 852, y + 52), ELEMENT[el])
        text(d, (866, y + 2), n, 19, True)
        tier_badge(d, 866 + int(d.textlength(n, font=F(19, True))) + 18, y + 14, t, 11)
        text(d, (866, y + 28), cls, 14, fill=DIM)
        text(d, (1212, y + 14), who, 15, True, CYAN if who in ("You", "Kestrel") else DIM, anchor="ra")
    d.line((800, 376, 1212, 376), fill=STEEL)
    text(d, (800, 388), "Party Combat Tier", 15, fill=DIM)
    tier_badge(d, 1198, 398, "C", 12)
    text(d, (800, 418), "Wipes allowed", 15, fill=DIM)
    text(d, (1212, 418), "5  (3 at the boss)", 15, True, anchor="ra")
    if coop:
        text(d, (800, 442), "Kestrel is ready", 14, fill=GOOD)
    # enter
    f = focus_row == 2
    cut(d, (780, 486, 1232, 546), TEAL if f else PLATE, CYAN if f else STEEL, 12)
    text(d, (1006, 516), "Enter the trial", 22, True, anchor="mm")
    text(d, (48, 560), "A run takes about 30 to 45 minutes. It can't be saved: leave by extracting at a rest spot.", 14, fill=OFF)
    footer(d, [("X", "Party"), ("Y", "Loadout"), ("A", "Select"), ("B", "Cancel")])
    save(im, OUT, name)


gate("1_gate_variation.png", 1, 1, 0)
gate("2_gate_ready_coop.png", 0, 2, 2, coop=True)
print("ok")
