"""Pause v2 (Crown, 2026-11-25): the game menu in the approved picker layout (menus/loadout/v5).
Left: the pages (A opens one); middle: the party at a glance, always shown (HP, EP, the active loadout's weapon);
right: Details, the run's state in a trial (or the hub's in the hub) and what the focused page holds.
Values are placeholders. Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE

PAGES = [("Resume", "Back to the game"), ("Equipment", "Gear, skills, apparel"), ("Inventory", "Bag and fight items"),
         ("Party", "Seats and start places"), ("Journal", "Quests and memories"), ("Settings", "Game, camera, controls"),
         ("Leave trial", "Back to the hub; the run ends")]
PARTY = [  # name, class, element, HP, EP, weapon kind, weapon
    ("Sera", "Caster", "Darkness", (18, 84), (30, 40), "Crystal", "Innate spells"),
    ("Garrick", "Generic", "Generic", (67, 84), (22, 30), "Sword", "Arming Sword"),
    ("Bram", "Resonator", "Fire", (87, 97), (35, 35), "Adapter", "Garnet ring"),
]
MSB = [(6, 3, 5), (2, 7, 3), (4, 4, 5)]  # Mind, Body, Spirit per member


def pages_left(d, focus_i):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), "Menu", 17, True)
    y = TOP + 48
    for i, (name, sub) in enumerate(PAGES):
        focus = i == focus_i
        last = i == len(PAGES) - 1
        box = (GX0 + 16, y, GX1 - 16, y + 52)
        cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 7)
        text(d, (box[0] + 16, y + 8), name, 17, True, WARN if last and not focus else TXT)
        text(d, (box[0] + 16, y + 31), sub, 13, fill=TXT if focus else DIM)
        y += 59 if not i == len(PAGES) - 2 else 71


def party_middle(d):
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), "Party", 22, True)
    y = TOP + 52
    for (name, cls, elem, hp, ep, kind, weapon), msb in zip(PARTY, MSB):
        box = (DX0 + 16, y, DX1 - 16, y + 132)
        cut(d, box, PLATE, STEEL, 8)
        d.ellipse((box[0] + 14, y + 14, box[0] + 62, y + 62), fill=ELEMENT[elem])
        text(d, (box[0] + 38, y + 38), name[0], 22, True, BG, anchor="mm")
        text(d, (box[0] + 76, y + 10), name, 18, True)
        text(d, (box[0] + 76, y + 34), cls, 13, fill=DIM)
        mx = box[0] + 76
        for label, value in zip(("Mind", "Body", "Spirit"), msb):  # MSB: the three stats
            text(d, (mx, y + 56), label, 13, fill=DIM)
            mx += d.textlength(label, font=F(13)) + 6
            text(d, (mx, y + 56), str(value), 13, True)
            mx += d.textlength(str(value), font=F(13, True)) + 16
        frac = hp[0] / hp[1]
        bar(d, box[0] + 14, y + 86, box[2] - box[0] - 120, frac, ROSE_LOW if frac < 0.3 else ROSE)
        text(d, (box[2] - 14, y + 86), "%d / %d" % hp, 13, True, KILL if frac < 0.3 else TXT, anchor="ra")
        bar(d, box[0] + 14, y + 108, box[2] - box[0] - 120, ep[0] / ep[1], EP)
        text(d, (box[2] - 14, y + 108), "%d / %d" % ep, 13, True, anchor="ra")
        art(d, (box[2] - 58, y + 10, box[2] - 14, y + 54), kind, ELEMENT[elem] if kind != "Sword" else None)
        text(d, (box[2] - 66, y + 22), weapon, 12, fill=DIM, anchor="rm")
        y += 142


def run_right(d):
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), "Ember Trial  ·  this run", 14, fill=DIM)
    rows = [("Fights won", "3", TXT), ("Next", "Mini-boss", WARN), ("Run gold", "240", GOLD), ("Fight items", "6 left", TXT),
            ("Time", "18:42", TXT), ("Saved", "at the last rest spot", DIM)]
    y = TOP + 70
    for label, value, col in rows:
        text(d, (SX0 + 18, y), label, 15, fill=DIM)
        text(d, (SX1 - 18, y), value, 15, True, col, anchor="ra")
        y += 25
    y += 10
    d.line((SX0 + 18, y, SX1 - 18, y), fill=(52, 54, 62))
    y += 14
    text(d, (SX0 + 18, y), "Equipment", 15, True)
    y = wrap(d, (SX0 + 18, y + 26), "Change gear, skills and apparel. In a trial, only at a rest spot; here you can look.",
             SX1 - SX0 - 36, 14, DIM, gap=4)
    text(d, (SX0 + 18, y + 6), "Sera is low on HP.", 14, True, WARN)


def screen_pause():
    im, d = canvas("world")
    d.text((48, 26), "Paused", font=T(30), fill=TXT)
    text(d, (48, 66), "Ember Trial", 15, fill=DIM)
    pages_left(d, 1)
    party_middle(d)
    run_right(d)
    footer(d, [("A", "Open"), ("B", "Resume")])
    return im


if __name__ == "__main__":
    save(screen_pause(), OUT, "1_pause_trial.png")
    print("ok")
