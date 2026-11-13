"""Settings (UIScreens.md #8): Gameplay (control model, Turn View, hide End Turn, hints), Controls (the key list,
fixed keys for now: D-063), Accessibility. Setting names from the code: "Turn View" with Centred Wide /
Over the Shoulder / Tactical (CombatCameraTypes.cpp), End Turn hide (SaveSubsystem). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

PAGES = ["Gameplay", "Camera", "Display", "Audio", "Controls", "Accessibility"]


def header(d, page):
    title(d, "Settings")
    x = tabs(d, 48, 96, PAGES, page, lead="LB")
    text(d, (x + 4, 103), "RB", 16, True, CYAN)


def option(d, y, label, value, focus=False, choices=None, lit=0, toggle=None):
    """One setting row: label left, value right. A choice shows < value >; a toggle shows On / Off."""
    cut(d, (48, y, 720, y + 50), TEAL if focus else PLATE, CYAN if focus else STEEL, 8)
    text(d, (66, y + 25), label, 19, True, anchor="lm")
    if toggle is not None:
        for i, t in enumerate(("Off", "On")):
            on = (i == 1) == toggle
            cut(d, (580 + i * 64, y + 9, 638 + i * 64, y + 41), CYAN if on else BG, CYAN if on else OFF, 6)
            text(d, (609 + i * 64, y + 25), t, 15, True, BG if on else OFF, anchor="mm")
    else:
        text(d, (700, y + 25), ("‹  %s  ›" % value) if focus else value, 18, True, TXT if focus else DIM, anchor="rm")


def explain(d, head, body, extra=None):
    panel(d, (750, 150, 1232, 654), focus=True)
    text(d, (770, 162), head, 22, True)
    y = wrap(d, (770, 200), body, 442, 16)
    if extra:
        extra(d, y + 12)


def control_model(d, y, picked=0):
    """A small picture of what each model's D-pad does: the reason the setting is understandable at a glance."""
    for i, (name, lines, on) in enumerate([
        ("Shortcuts", ["L2 / R2   kind", "D-pad ◄ ►   the row under it", "D-pad ▲ ▼   more skills", "Y / X / A   use a skill"], picked == 0),
        ("Omni D-pad", ["D-pad ▲ ▼   pick a row", "D-pad ◄ ►   change it", "L2 / R2   kind (still works)", "Y / X / A   use a skill"], picked == 1)]):
        by = y + i * 150
        cut(d, (770, by, 1212, by + 138), PLATE if not on else BG, CYAN if on else STEEL, 8)
        text(d, (786, by + 8), name, 18, True, CYAN if on else TXT)
        if on:
            text(d, (1196, by + 10), "selected" + (" · default" if i == 0 else ""), 13, fill=CYAN, anchor="ra")
        elif i == 0:
            text(d, (1196, by + 10), "default", 13, fill=DIM, anchor="ra")
        for j, ln in enumerate(lines):
            text(d, (786, by + 38 + j * 23), ln, 15, fill=TXT if on else DIM)


# ---- 1: Gameplay. The control model is the first row; the side panel shows both models' buttons.
im, d = canvas()
header(d, 0)
option(d, 150, "Battle menu controls", "Shortcuts", focus=True)
option(d, 208, "Turn View", "Centred Wide")
option(d, 266, "End Turn button", None, toggle=True)
option(d, 324, "Button hints in the corner", None, toggle=True)
option(d, 382, "Boss cinematics", "Play, skippable")
option(d, 440, "AI finishes my turn when I idle", None, toggle=False)
option(d, 498, "Tutorial callouts", None, toggle=True)
explain(d, "Battle menu controls",
        "How you move through the skill menu in a fight. Both reach every skill; Shortcuts is faster, Omni D-pad uses fewer buttons.",
        control_model)
footer(d, [("A", "Change"), ("Y", "Reset page"), ("B", "Back")])
save(im, OUT, "1_gameplay_control_model.png")

# ---- 2: Controls: read-only key list for now (fixed keys, D-063), switchable between gamepad and keyboard.
im, d = canvas()
header(d, 4)
tabs(d, 48, 150, ["Gamepad", "Keyboard and mouse"], 0, h=30, size=15)
tabs(d, 420, 150, ["Field", "Battle", "Menus"], 1, h=30, size=15)
ROWS = [("Attack", "A"), ("Skills", "X"), ("Items", "Y"), ("Back", "B"), ("Defend", "RB"), ("Kind previous / next", "LT / RT"),
        ("Infusion level", "LB"), ("Guard (timed)", "—"), ("Dodge (timed)", "—"), ("Skip boss cinematic", "Menu")]
for i, (act, key) in enumerate(ROWS):
    y = 192 + i * 44
    cut(d, (48, y, 720, y + 38), PLATE, STEEL, 6)
    text(d, (66, y + 19), act, 17, anchor="lm")
    if key in GLYPH:
        glyph(d, 690, y + 19, key, 12)
    else:
        text(d, (704, y + 19), key, 16, True, DIM if key == "—" else TXT, anchor="rm")
explain(d, "Controls", "Keys are fixed for now. Rebinding comes later; every action already has its own name so it can.",
        lambda d, y: wrap(d, (770, y), "Guard and Dodge show whatever the editor's battle context binds; that row is filled when the "
                          "screen is built.", 442, 15, DIM))
footer(d, [("LT", "RT  Section"), ("B", "Back")])
save(im, OUT, "2_controls.png")

# ---- 3: Accessibility: the same control-model row is repeated here (people look for it in both places).
im, d = canvas()
header(d, 5)
option(d, 150, "Battle menu controls", "Omni D-pad", focus=True)
option(d, 208, "Text size", "Large")
option(d, 266, "Camera movement in fights", None, toggle=True)
option(d, 324, "Screen shake", "Low")
option(d, 382, "Hold to confirm dangerous actions", None, toggle=False)
option(d, 440, "Element letters on colours", None, toggle=True)
option(d, 498, "Subtitles", None, toggle=True)
explain(d, "Battle menu controls", "The same setting as on the Gameplay page. Omni D-pad moves through the battle menu with the "
        "D-pad alone: up and down pick a row, left and right change it.", lambda d, y: control_model(d, y, 1))
footer(d, [("A", "Change"), ("Y", "Reset page"), ("B", "Back")])
save(im, OUT, "3_accessibility.png")
print("ok")
