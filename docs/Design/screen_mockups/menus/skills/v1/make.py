"""Skills v1 (Crown, 2026-11-25): the loadout's Skills tab in the approved picker layout (menus/loadout/v5).
1: the tab. Left, the focused entry's skill slots (‹ entry ›, built-in, picked, empty); middle, the highlighted
   skill's full details, always open; right, Details: the entry's slots and what fits it.
2: picking a skill for a slot. Left, the learned skills (the ones that can't go here greyed, with the reason in the
   details); middle, the focused skill; right, Details: the slot's skill now -> this one.
No pop-ups. Values are placeholders. Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE

SLOTS = [  # name, short form, EP, kind (built-in / picked / empty)
    ("Force Strike", "1 target  ·  Hit", "12", "built-in"),
    ("Rally", "Self  ·  Damage +25%, 2 turns", "10", "picked"),
    ("Ritual Strike", "1 target  ·  fires in 2 turns", "20", "picked"),
    ("Shield Bash", "1 target  ·  Stagger", "14", "picked"),
    ("Whirlwind", "1 target  ·  3 hits", "22", "picked"),
    (None, None, None, "empty"), (None, None, None, "empty"),
]


def tabs_top(d):
    x = tabs(d, 48, 30, ["Generic", "Caster", "Resonator"], 0, lead="Z")
    text(d, (x + 4, 37), "C", 16, True, CYAN)
    x = tabs(d, 48, 78, ["Inventory", "Equipment", "Skills", "Apparel"], 2, lead="Q")
    text(d, (x + 4, 85), "E", 16, True, CYAN)
    text(d, (1232, 34), "Loadout 1", 20, True, anchor="ra")


def skill_row(d, y, i, focus):
    name, short, ep, kind = SLOTS[i]
    box = (GX0 + 28, y, GX1 - 16, y + 48)
    if kind == "empty":
        cut(d, box, BG, (60, 62, 70), 7)
        text(d, (box[0] + 16, y + 25), "Empty slot", 15, fill=OFF, anchor="lm")
        text(d, (box[2] - 14, y + 25), "A  Add", 13, True, OFF, anchor="rm")
        return
    cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 7)
    text(d, (box[0] + 16, y + 8), name, 16, True)
    text(d, (box[0] + 16, y + 30), short, 13, fill=TXT if focus else DIM)
    text(d, (box[2] - 14, y + 8), "%s EP" % ep, 14, True, EP, anchor="ra")
    if kind == "built-in":
        text(d, (box[2] - 14, y + 30), "built-in", 12, True, OFF, anchor="ra")


def left_slots(d, focus_i):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), "Skill slots", 17, True)
    text(d, (GX1 - 16, TOP + 16), "2 entries", 13, True, DIM, anchor="ra")
    text(d, (GX0 + 18, TOP + 38), "each entry opens and closes", 14, fill=DIM)
    # each entry is a drop-down (Crown): click / A on its header opens or closes its slots
    y = TOP + 60
    for entry, used, is_open in (("Arming Sword + Round Shield", "5 / 7", True), ("Hand Axe", "2 / 6", False)):
        cut(d, (GX0 + 16, y, GX1 - 16, y + 32), PLATE, STEEL, 6)
        text(d, (GX0 + 30, y + 16), "▼" if is_open else "▶", 13, True, CYAN, anchor="lm")
        text(d, (GX0 + 50, y + 16), entry, 15, True, anchor="lm")
        text(d, (GX1 - 30, y + 16), used, 13, True, DIM, anchor="rm")
        y += 38
        if is_open:
            for i in range(len(SLOTS)):
                skill_row(d, y, i, i == focus_i)
                y += 54
            y += 4


def skill_details(d, name, tier, line, stats, effects, req, req_ok, note=None):
    """The highlighted skill, always open: numbers, the effects as a tree, the requirement met or not."""
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), name, 22, True)
    tier_badge(d, DX0 + 38 + d.textlength(name, font=F(22, True)), TOP + 27, tier, 11)
    text(d, (DX0 + 20, TOP + 46), line, 14, fill=DIM)
    y = TOP + 80
    for label, value in stats:
        text(d, (DX0 + 20, y), label, 15, fill=DIM)
        text(d, (DX1 - 20, y), value, 15, True, anchor="ra")
        y += 24
    y += 6
    d.line((DX0 + 20, y, DX1 - 20, y), fill=(52, 54, 62))
    y += 12
    text(d, (DX0 + 20, y), "Effects", 15, True)
    y += 26
    for depth, t in effects:
        x = DX0 + 26 + depth * 18
        d.ellipse((x - 3, y + 6, x + 3, y + 12), fill=CYAN if depth == 0 else STEEL)
        y = wrap(d, (x + 12, y), t, DX1 - x - 32, 14, TXT if depth == 0 else DIM, gap=4)
    y += 8
    text(d, (DX0 + 20, y), "Needs", 15, fill=DIM)
    text(d, (DX1 - 20, y), req, 15, True, GOOD if req_ok else KILL, anchor="ra")
    if note:
        wrap(d, (DX0 + 20, y + 30), note, DX1 - DX0 - 40, 14, WARN, gap=4)


def screen_tab():
    im, d = canvas("world")
    tabs_top(d)
    left_slots(d, 2)
    skill_details(d, "Ritual Strike", "E", "Sword skill  ·  ritual  ·  one target",
                  [("EP", "20 (paid when armed)"), ("Power", "90"), ("Scaling", "Body B"), ("Fires", "in 2 of your turns"),
                   ("Buildup", "8"), ("Infusion", "Wind, up to L2")],
                  [(0, "Arm now: charges with the air distortion"), (1, "The infusion picked now is locked in"),
                   (0, "On fire: one heavy strike"), (1, "Takes the locked infusion's element")],
                  "Sword", True)
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), "Arming Sword + Round Shield", 14, fill=DIM)
    rows = [("Slots", "7  (pair 1 + Emerald 6)"), ("Used", "5"), ("Takes", "Sword, Sword & shield"),
            ("Infusion from", "Emerald (Wind)"), ("Innate spells", "none (Generic)")]
    y = TOP + 70
    for label, value in rows:
        text(d, (SX0 + 18, y), label, 15, fill=DIM)
        text(d, (SX1 - 18, y), value, 14, True, anchor="ra")
        y += 25
    y += 10
    for key, verb, primary in (("A", "Change this slot", True), ("Y", "Take it out", False), ("X", "Short / full", False)):
        action_button(d, (SX0 + 16, y, SX1 - 16, y + 38), key, verb, primary)
        y += 46
    footer(d, [("A", "Change / open"), ("‹ ›", "Entry"), ("Y", "Take out"), ("X", "Short / full"), ("B", "Back")])
    return im


def screen_pick():
    im, d = canvas("world")
    tabs_top(d)
    items = [dict(kind="Sword", tier="E"), dict(kind="Sword", tier="E", worn=True), dict(kind="Sword", tier="D"),
             dict(kind="Shield", tier="E"), dict(kind="Sword", tier="D"), dict(kind="Axe", tier="S", locked=True),
             dict(kind="Greatsword", tier="E", locked=True), dict(kind="Bow", tier="F", locked=True)]
    left_column(d, "Slot 6  ·  empty", "Arming Sword + Round Shield", ["Fits this slot", "All learned"], 1, items, 0,
                "8 learned", ["All", "Sword", "Shield", "Axe", "Greatsword"])
    d.rectangle((GX0 + 14, BOTTOM - 30, GX1 - 14, BOTTOM - 6), fill=PANEL)  # legend: slotted, not worn
    ly = BOTTOM - 18
    check_corner(d, GX0 + 18, ly - 9, 14)
    text(d, (GX0 + 38, ly), "slotted already", 13, fill=OFF, anchor="lm")
    lock(d, GX0 + 152, ly - 3)
    text(d, (GX0 + 164, ly), "can't go here (why: in the details)", 13, fill=OFF, anchor="lm")
    skill_details(d, "Over Under", "E", "Sword skill  ·  one target  ·  2 hits",
                  [("EP", "18"), ("Power", "40 + 60"), ("Scaling", "Body C"), ("Buildup", "5"), ("Infusion", "Wind, up to L2")],
                  [(0, "Two cuts, rising then falling"), (1, "The second hit lands harder")],
                  "Sword", True)
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), "Slot 6 on Arming Sword + Round Shield", 14, fill=DIM)
    text(d, (SX0 + 18, TOP + 62), "If you put in Over Under", 12, True, CYAN)
    y = TOP + 90
    for label, old, new, better in (("Skills", "5", "6", True), ("Slots free", "2", "1", None),
                                    ("Sword skills", "4", "5", True), ("Cheapest EP", "10", "10", None)):
        stat_line(d, SX0 + 18, SX1 - 18, y, label, old, new, better)
        y += 24
    footer(d, [("A", "Put in slot"), ("‹ ›", "Fits / all"), ("X", "Short / full"), ("B", "Cancel")])
    return im


if __name__ == "__main__":
    save(screen_tab(), OUT, "1_skills_tab.png")
    save(screen_pick(), OUT, "2_pick_skill.png")
    print("ok")
