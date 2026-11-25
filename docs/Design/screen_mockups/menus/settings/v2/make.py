"""Settings v2 (Crown, 2026-11-25): the settings in the approved picker layout (menus/loadout/v5).
Pages on top. Left: the page's options (label, value, ‹ › changes it); middle: a preview of the focused option, always
shown; right: Details, every choice it has, what each does, and the default. Keys are fixed for now (D-063).
Setting names from the code; values are placeholders. Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE

PAGES = ["Gameplay", "Camera", "Display", "Audio", "Controls", "Accessibility"]


def settings_top(d, page):
    d.text((48, 26), "Settings", font=T(30), fill=TXT)
    x = tabs(d, 48, 78, PAGES, page, lead="Q")
    text(d, (x + 4, 85), "E", 16, True, CYAN)


def options_left(d, title_l, rows, focus_i, selector=None, arrows=True):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), title_l, 17, True)
    y = TOP + 48
    if selector:
        kind_selector(d, GX0 + 16, TOP + 44, selector, 0)
        y = TOP + 92
    for i, (label, value) in enumerate(rows):
        focus = i == focus_i
        box = (GX0 + 16, y, GX1 - 16, y + 44)
        cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 7)
        text(d, (box[0] + 14, y + 22), label, 15, True, anchor="lm")
        if value in (True, False):
            sw = (box[2] - 58, y + 12, box[2] - 14, y + 32)
            d.rounded_rectangle(sw, 10, fill=(GOOD if value else TRACK))
            kx = sw[2] - 10 if value else sw[0] + 10
            d.ellipse((kx - 8, y + 14, kx + 8, y + 30), fill=TXT)
        else:
            text(d, (box[2] - 14, y + 22), ("‹ %s ›" % value) if focus and arrows else value, 14, True,
                 CYAN if focus else DIM, anchor="rm")
        y += 51


def preview_middle(d, name, line, draw, blurb):
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), name, 22, True)
    text(d, (DX0 + 20, TOP + 46), line, 14, fill=DIM)
    pic = (DX0 + 20, TOP + 74, DX1 - 20, TOP + 300)
    cut(d, pic, BG, STEEL, 10)
    draw(d, pic)
    wrap(d, (DX0 + 20, TOP + 318), blurb, DX1 - DX0 - 40, 15, DIM, gap=5)


def choices_right(d, sub, choices, lit, note=None):
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), sub, 14, fill=DIM)
    y = TOP + 68
    for i, (name, what, default) in enumerate(choices):
        on = i == lit
        box = (SX0 + 16, y, SX1 - 16, y + 74)
        cut(d, box, (40, 34, 58) if on else PLATE, CYAN if on else STEEL, 7)
        text(d, (box[0] + 14, y + 10), name, 15, True)
        if default:
            text(d, (box[2] - 12, y + 11), "default", 12, True, OFF, anchor="ra")
        wrap(d, (box[0] + 14, y + 34), what, box[2] - box[0] - 28, 13, DIM if not on else TXT, gap=3)
        y += 82
    if note:
        wrap(d, (SX0 + 18, y + 4), note, SX1 - SX0 - 36, 14, DIM, gap=4)


def turn_view_pic(d, pic):
    """A top-down sketch: the party left, enemies right, the camera's cone for the lit choice."""
    x0, y0, x1, y1 = pic
    cy = (y0 + y1) / 2
    for i, yy in enumerate((cy - 50, cy, cy + 50)):
        d.ellipse((x0 + 90, yy - 12, x0 + 114, yy + 12), fill=CYAN if i == 1 else STEEL)
        d.ellipse((x1 - 114, yy - 12, x1 - 90, yy + 12), fill=KILL)
    cam = (x0 + 40, cy)
    d.polygon([cam, (x1 - 40, y0 + 20), (x1 - 40, y1 - 20)], outline=GOLD)
    d.rectangle((cam[0] - 10, cam[1] - 7, cam[0] + 10, cam[1] + 7), fill=GOLD)
    text(d, (cam[0], cam[1] + 16), "camera", 12, fill=GOLD, anchor="ma")


def key_pic(d, pic):
    x0, y0, x1, y1 = pic
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    for dx, key in ((-90, "G"), (90, "◄")):
        b = (cx + dx - 34, cy - 34, cx + dx + 34, cy + 34)
        cut(d, b, PLATE, STEEL, 8)
        text(d, (cx + dx, cy), key, 28, True, anchor="mm")
        text(d, (cx + dx, cy + 50), "Keyboard" if key == "G" else "D-pad Left", 13, fill=DIM, anchor="ma")


def screen_gameplay():
    im, d = canvas("world")
    settings_top(d, 0)
    options_left(d, "Gameplay", [("Battle menu controls", "Shortcuts"), ("Turn View", "Centred Wide"),
                                 ("End Turn button", True), ("Button hints", True), ("Boss cinematics", "Skippable"),
                                 ("Tutorial callouts", True)], 1)
    preview_middle(d, "Turn View", "Where the camera sits on your turn", turn_view_pic,
                   "Changes only the view while you choose. Attacks and spells keep their own cameras.")
    choices_right(d, "Turn View  ·  3 choices",
                  [("Centred Wide", "The whole fight from behind your side; everyone on screen.", True),
                   ("Over the Shoulder", "Close behind the fighter whose turn it is.", False),
                   ("Tactical", "High above the grid, rows and places easy to read.", False)], 0)
    footer(d, [("‹ ›", "Change"), ("A", "Next choice"), ("Y", "Default"), ("B", "Back")])
    return im


def screen_controls():
    im, d = canvas("world")
    settings_top(d, 4)
    options_left(d, "Controls  ·  Battle", [("Attack", "1 / A"), ("Skills", "2 / X"), ("Items", "3 / Y"),
                                            ("Defend", "R / RB"), ("End Turn", "G / ◄"), ("Back", "Esc / B")], 4,
                 selector=["Battle", "Field", "Menus"], arrows=False)
    preview_middle(d, "End Turn", "Battle  ·  main menu only", key_pic,
                   "Ends your turn and keeps any action pips you didn't use. Only on the battle's main menu.")
    choices_right(d, "End Turn  ·  keys",
                  [("Keyboard", "G", False), ("Controller", "D-pad Left", False), ("Mouse", "Click the End Turn hint", False)],
                  -1, note="Keys are fixed for now; rebinding comes later.")
    footer(d, [("‹ ›", "Field / Battle / Menus"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_gameplay(), OUT, "1_gameplay.png")
    save(screen_controls(), OUT, "2_controls.png")
    print("ok")
