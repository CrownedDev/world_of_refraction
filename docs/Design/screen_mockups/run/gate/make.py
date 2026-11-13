"""The Gate (UIScreens.md #21; D-191, D-192): pick the gate, then the trial, then the difficulty; see who is going, enter.
One gate stands in the hub and every gate is open to everyone (the Stone Gate, the Silver Gate, the crystal-named gates).
Trials are a vertical list: a name and a line each, never the boss; a locked one reads "???" with its unlock condition.
Difficulty changes how hard enemies hit and the payout, not their HP (TrialGate.h / .cpp: payout x0.75 / x1.0 / x1.5).
Gate and trial names are stand-ins; the real ones are Crown's. Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

DIFF = [("Easy", "x0.8", "x0.75"), ("Medium", "x1.0", "x1.0"), ("Hard", "x1.2", "x1.5")]
GATES = ["Stone Gate", "Silver Gate", "Garnet Gate", "Sapphire Gate", "Onyx Gate"]
# (name, line, unlock condition or None). A locked trial hides its name until it is unlocked.
TRIALS = [("Past", "Before the forge went cold.", None),
          ("Present", "As the trial stands today.", None),
          ("Future", "What the forge becomes.", None),
          ("???", None, "Unlocks with the quest: The Cinder Oath"),
          ("???", None, "Unlocks after clearing Past on Hard")]


def gate(name, trial_lit, diff_lit, focus_row, coop=False):
    im, d = canvas("world")
    im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), 0.45)
    d = ImageDraw.Draw(im)
    title(d, "The Gate", "Pick a gate, then a trial, then the difficulty")
    # the gate first (D-192): every gate is open to everyone
    x = tabs(d, 48, 118, GATES, 2, lead="LB")
    text(d, (x + 4, 125), "RB", 16, True, CYAN)
    # the trials of that gate: a vertical list (there will be more than three), name and line only
    text(d, (48, 170), "Trial", 16, True, CYAN if focus_row == 0 else DIM)
    for i, (n, line, unlock) in enumerate(TRIALS):
        y = 196 + i * 52
        lit = i == trial_lit
        cut(d, (48, y, 740, y + 46), TEAL if lit and focus_row == 0 else (BG if unlock else PLATE),
            CYAN if lit else (OFF if unlock else STEEL), 8)
        text(d, (66, y + 23), n, 20, True, OFF if unlock else TXT, anchor="lm")
        text(d, (190, y + 23), unlock or line, 15, fill=OFF if unlock else (TXT if lit else DIM), anchor="lm")
        if lit:
            text(d, (724, y + 23), "✓", 18, True, CYAN, anchor="rm")
    # difficulty: three plain tabs with what each changes, in the code's words
    text(d, (48, 466), "Difficulty", 16, True, CYAN if focus_row == 1 else DIM)
    for i, (n, dmg, pay) in enumerate(DIFF):
        x = 48 + i * 232
        lit = i == diff_lit
        cut(d, (x, 492, x + 228, 574), TEAL if lit and focus_row == 1 else PLATE, CYAN if lit else STEEL, 10)
        text(d, (x + 16, 498), n, 20, True)
        text(d, (x + 16, 526), "Enemy damage " + dmg, 14, fill=TXT if lit else DIM)
        text(d, (x + 16, 546), "Payout " + pay, 14, fill=GOOD if pay == "x1.5" else (TXT if lit else DIM))
    wrap(d, (48, 588), "Enemies hit harder and fight smarter; they don't get more HP. You can't change this once inside.", 700, 14, DIM)
    # who is going: an AI member shows nothing under "who plays them" (D-191)
    panel(d, (780, 170, 1232, 504))
    text(d, (800, 182), "Going in", 16, True, DIM)
    party = [("Sera", "Darkness Refractor", "Darkness", "C", "You"), ("Garrick", "Generic", "Generic", "B", ""),
             ("Bram", "Resonator", "Fire", "C", "Kestrel" if coop else "")]
    for i, (n, cls, el, t, who) in enumerate(party):
        y = 214 + i * 62
        icon(d, (800, y, 852, y + 52), ELEMENT[el])
        text(d, (866, y + 2), n, 19, True)
        tier_badge(d, 866 + int(d.textlength(n, font=F(19, True))) + 18, y + 14, t, 11)
        text(d, (866, y + 28), cls, 14, fill=DIM)
        text(d, (1212, y + 14), who, 15, True, CYAN, anchor="ra")
    d.line((800, 410, 1212, 410), fill=STEEL)
    text(d, (800, 422), "Party Combat Tier", 15, fill=DIM)
    tier_badge(d, 1198, 432, "C", 12)
    text(d, (800, 452), "Wipes allowed", 15, fill=DIM)
    text(d, (1212, 452), "5  (3 at the boss)", 15, True, anchor="ra")
    if coop:
        text(d, (800, 476), "Kestrel is ready", 14, fill=GOOD)
    # enter
    f = focus_row == 2
    cut(d, (780, 514, 1232, 574), TEAL if f else PLATE, CYAN if f else STEEL, 12)
    text(d, (1006, 544), "Enter the trial", 22, True, anchor="mm")
    text(d, (48, 640), "A run takes about 30 to 45 minutes. It can't be saved: leave by extracting at a boss's rest spot.", 14, fill=OFF)
    footer(d, [("X", "Party"), ("Y", "Loadout"), ("A", "Select"), ("B", "Cancel")])
    save(im, OUT, name)


gate("1_gate_trial.png", 1, 1, 0)
gate("2_gate_ready_coop.png", 0, 2, 2, coop=True)
print("ok")
