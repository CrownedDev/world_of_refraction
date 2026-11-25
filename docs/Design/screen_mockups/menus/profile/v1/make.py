"""Profile v1 (Crown, 2026-11-25): the character sheet in the approved picker layout (menus/loadout/v5), opened
from the Party screen (A or a click on a seat). Left: who they are and the stat groups; middle: the lit group's
stats; right: Details, what the lit stat does and where its value comes from.
Groups and sub-stats from CharacterSheetModel.cpp (Rules CL-39); values are placeholders. Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE

GROUPS = [("Mind", "2", "Efficiency, spell damage, crits"), ("Body", "7", "Defence, speed, raw damage"),
          ("Spirit", "3", "Resistance, status, luck"), ("Resistances", "", "Physical, spell, each element"),
          ("Curses", "1", "Held by you; on every sheet")]
BODY = [("Defence", "18%", 0.36), ("Action Speed", "x1.10", 0.55), ("Raw Damage", "x1.35", 0.68), ("Reflex", "x1.05", 0.52)]


def left_profile(d, lit):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    d.ellipse((GX0 + 18, TOP + 16, GX0 + 74, TOP + 72), fill=ELEMENT["Generic"])
    text(d, (GX0 + 46, TOP + 45), "G", 24, True, BG, anchor="mm")
    text(d, (GX0 + 88, TOP + 16), "Garrick", 20, True)
    tier_badge(d, GX0 + 106 + d.textlength("Garrick", font=F(20, True)), TOP + 28, "B", 9)
    text(d, (GX0 + 88, TOP + 44), "Generic  ·  AI seat", 14, fill=DIM)
    bar(d, GX0 + 18, TOP + 86, 270, 67 / 84, ROSE)
    text(d, (GX1 - 18, TOP + 86), "67 / 84", 13, True, anchor="ra")
    bar(d, GX0 + 18, TOP + 108, 270, 22 / 30, EP)
    text(d, (GX1 - 18, TOP + 108), "22 / 30", 13, True, anchor="ra")
    text(d, (GX0 + 18, TOP + 134), "Max actions", 14, fill=DIM)
    text(d, (GX1 - 18, TOP + 134), "3  ·  regains 1 a turn", 14, True, anchor="ra")
    y = TOP + 168
    for i, (name, value, sub) in enumerate(GROUPS):
        focus = i == lit
        box = (GX0 + 16, y, GX1 - 16, y + 52)
        cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 7)
        text(d, (box[0] + 16, y + 8), name, 17, True)
        text(d, (box[0] + 16, y + 31), sub, 13, fill=TXT if focus else DIM)
        if value:
            text(d, (box[2] - 16, y + 14), value, 20, True, CYAN if focus else TXT, anchor="ra")
        y += 59


def middle_group(d, lit_row):
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), "Body", 22, True)
    text(d, (DX1 - 20, TOP + 14), "7", 26, True, CYAN, anchor="ra")
    text(d, (DX0 + 20, TOP + 46), "Toughness and weapon force", 14, fill=DIM)
    y = TOP + 84
    for i, (name, value, frac) in enumerate(BODY):
        focus = i == lit_row
        box = (DX0 + 16, y, DX1 - 16, y + 64)
        cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 7)
        text(d, (box[0] + 16, y + 10), name, 16, True)
        text(d, (box[2] - 16, y + 10), value, 16, True, anchor="ra")
        bar(d, box[0] + 16, y + 40, box[2] - box[0] - 32, frac, CYAN if focus else STEEL, h=10)
        y += 72


def right_details(d):
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), "Raw Damage", 14, fill=DIM)
    wrap(d, (SX0 + 18, TOP + 66), "Multiplies the damage of weapon hits and physical skills.", SX1 - SX0 - 36, 14, TXT, gap=4)
    y = TOP + 124
    text(d, (SX0 + 18, y), "Where it comes from", 15, True)
    y += 28
    for label, value in (("Body 7", "x1.25"), ("Arming Sword", "+5%"), ("Iron vest", "+5%")):
        text(d, (SX0 + 18, y), label, 15, fill=DIM)
        text(d, (SX1 - 18, y), value, 15, True, anchor="ra")
        y += 25
    d.line((SX0 + 18, y + 2, SX1 - 18, y + 2), fill=(52, 54, 62))
    text(d, (SX0 + 18, y + 12), "Total", 15, True)
    text(d, (SX1 - 18, y + 12), "x1.35", 15, True, CYAN, anchor="ra")


def screen_profile():
    im, d = canvas("world")
    d.text((48, 26), "Profile", font=T(30), fill=TXT)
    x = tabs(d, 48, 78, ["Sera", "Garrick", "Bram"], 1, lead="LT")
    text(d, (x + 4, 85), "RT", 16, True, CYAN)
    left_profile(d, 1)
    middle_group(d, 2)
    right_details(d)
    footer(d, [("LT / RT", "Member"), ("A", "Select"), ("Y", "Loadout"), ("B", "Back to Party")])
    return im


if __name__ == "__main__":
    save(screen_profile(), OUT, "1_profile.png")
    print("ok")
