"""Party v3 (Crown, 2026-11-25): v2 plus the seat cards of B-628b (Mind/Body/Spirit, HP, EP, weapon) and A opens the
member's Profile (menus/profile/v1). Party v2: the party screen in the approved picker layout (menus/loadout/v5).
Left: the three seats (PA-01) and the co-op invite; middle: the fixed details of the focused member; right: Details,
who plays them (You / a friend / an AI seat, PA-11), where they start on the 3 x 3 grid (PA-07) and the actions.
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

PARTY = [  # name, class, element, tier, played by, HP, EP, (row, col)
    ("Sera", "Caster", "Darkness", "C", "You  ·  leader", (119, 119), (71, 71), (2, 1)),
    ("Garrick", "Generic", "Generic", "B", "AI seat", (140, 140), (55, 55), (0, 1)),
    ("Bram", "Resonator", "Fire", "C", "Friend  ·  Kai", (104, 104), (80, 80), (1, 0)),
]
ROWS = ["Front", "Middle", "Back"]


MSB = [(6, 3, 5), (2, 7, 3), (4, 4, 5)]
WEAPON = ["Innate spells", "Arming Sword", "Garnet ring"]


def seat_card(d, y, i, focus):
    name, cls, elem, tier, who, hp, ep, _ = PARTY[i]
    box = (GX0 + 16, y, GX1 - 16, y + 140)
    cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 8)
    d.ellipse((box[0] + 14, y + 14, box[0] + 62, y + 62), fill=ELEMENT[elem])
    text(d, (box[0] + 38, y + 39), name[0], 22, True, BG, anchor="mm")
    text(d, (box[0] + 76, y + 10), name, 18, True)
    tier_badge(d, box[0] + 94 + d.textlength(name, font=F(18, True)), y + 21, tier, 9)
    text(d, (box[0] + 76, y + 34), "%s  ·  %s" % (cls, who), 13, fill=TXT if focus else DIM)
    text(d, (box[2] - 12, y + 12), "Seat %d" % (i + 1), 12, True, OFF, anchor="ra")
    text(d, (box[2] - 12, y + 58), WEAPON[i], 12, fill=DIM, anchor="ra")
    mx = box[0] + 76
    for label, value in zip(("Mind", "Body", "Spirit"), MSB[i]):
        text(d, (mx, y + 58), label, 13, fill=DIM)
        mx += d.textlength(label, font=F(13)) + 6
        text(d, (mx, y + 58), str(value), 13, True)
        mx += d.textlength(str(value), font=F(13, True)) + 16
    bar(d, box[0] + 14, y + 90, box[2] - box[0] - 110, hp[0] / hp[1], ROSE)
    text(d, (box[2] - 12, y + 90), "%d / %d" % hp, 13, True, anchor="ra")
    bar(d, box[0] + 14, y + 112, box[2] - box[0] - 110, ep[0] / ep[1], EP)
    text(d, (box[2] - 12, y + 112), "%d / %d" % ep, 13, True, anchor="ra")


def left_party(d, focus_i):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), "Party", 17, True)
    text(d, (GX1 - 16, TOP + 16), "3 of 3", 13, True, DIM, anchor="ra")
    text(d, (GX0 + 18, TOP + 38), "recruit in the hub or a trial", 14, fill=DIM)
    y = TOP + 70
    for i in range(3):
        seat_card(d, y, i, i == focus_i)
        y += 148
    # no co-op line: who plays each seat (You / a friend's name / AI) already says it (Crown)


def start_grid(d, x, y, cell, mine):
    """The party's side of the battle grid seen from above, enemies to the right; the member's cell lit."""
    for c in range(3):
        text(d, (x + c * cell + (cell - 6) / 2, y + 3 * cell + 4), ROWS[2 - c], 12, fill=DIM, anchor="ma")
        for r in range(3):
            b = (x + c * cell, y + r * cell, x + (c + 1) * cell - 6, y + (r + 1) * cell - 6)
            cut(d, b, BG, (52, 54, 62), 4)
    for i, (name, _, elem, _, _, _, _, (row, col)) in enumerate(PARTY):
        c = 2 - row
        cx, cy = x + c * cell + (cell - 6) / 2, y + col * cell + (cell - 6) / 2
        if i == mine:
            d.rectangle((x + c * cell - 3, y + col * cell - 3, x + (c + 1) * cell - 3, y + (col + 1) * cell - 3), outline=CYAN, width=2)
        d.ellipse((cx - 14, cy - 14, cx + 14, cy + 14), fill=ELEMENT[elem] if i == mine else (70, 72, 80))
        text(d, (cx, cy + 1), name[0], 14, True, BG, anchor="mm")
    text(d, (x + 3 * cell + 4, y + 1.5 * cell), "enemies ►", 13, fill=OFF, anchor="lm")


def info_right(d, focus_i):
    name = PARTY[focus_i][0]
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), name, 14, fill=DIM)
    text(d, (SX0 + 18, TOP + 68), "Played by", 15, fill=DIM)
    text(d, (SX1 - 18, TOP + 68), "‹  AI seat  ›", 15, True, CYAN, anchor="ra")
    text(d, (SX0 + 18, TOP + 124), "Starts", 15, fill=DIM)
    text(d, (SX1 - 18, TOP + 124), "Front row, middle", 15, True, anchor="ra")
    start_grid(d, SX0 + 34, TOP + 154, 52, focus_i)
    y = TOP + 330
    for key, verb, primary, dim in (("A", "View profile", True, False), ("X", "Move start place", False, False),
                                    ("Y", "Open loadout", False, False)):
        action_button(d, (SX0 + 16, y, SX1 - 16, y + 38), key, verb, primary, dim)
        y += 46
    wrap(d, (SX0 + 18, y + 2), "An AI seat defends itself and plays its own turns. The leader can't be dismissed.",
         SX1 - SX0 - 36, 14, DIM, gap=4)


LOADOUTS = [  # name, worn, [(kind, piece, tier, element or None, skills)]
    ("Loadout 1", True, [("Sword", "Arming Sword", "F", "Wind", "Force Strike, Rally, Ritual Strike"),
                         ("Shield", "Round Shield", "F", None, "Shield Bash"),
                         ("Axe", "Hand Axe", "S", None, "Whirlwind, Slash Volley")]),
]  # the active loadout only (Crown: "we just need their active loadout")


def middle_loadouts(d, name, tier, sub, hp, ep):
    """The member's loadout list: every loadout, its entries (piece, element, slotted skills); the worn one lit."""
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), name, 22, True)
    tier_badge(d, DX0 + 38 + d.textlength(name, font=F(22, True)), TOP + 27, tier, 11)
    text(d, (DX0 + 20, TOP + 46), sub, 14, fill=DIM)
    text(d, (DX1 - 20, TOP + 18), "HP %d / %d" % hp, 14, True, ROSE, anchor="ra")
    text(d, (DX1 - 20, TOP + 40), "EP %d / %d" % ep, 14, True, EP, anchor="ra")
    y = TOP + 76
    for lname, worn, entries in LOADOUTS:
        text(d, (DX0 + 20, y), lname, 16, True, TXT if worn else DIM)
        if worn:
            check_corner(d, DX0 + 24 + d.textlength(lname, font=F(16, True)) + 8, y + 1, 14)
            text(d, (DX1 - 20, y + 2), "in use", 13, True, CYAN, anchor="ra")
        y += 26
        for kind, piece, ptier, elem, skills in entries:
            box = (DX0 + 16, y, DX1 - 16, y + 62)
            cut(d, box, PLATE if worn else (30, 32, 38), STEEL if worn else (60, 62, 70), 7)
            art(d, (box[0] + 8, y + 8, box[0] + 54, y + 54), kind, None, dim=not worn)
            text(d, (box[0] + 66, y + 9), piece, 16, True, TXT if worn else DIM)
            tier_badge(d, box[0] + 76 + d.textlength(piece, font=F(16, True)), y + 18, ptier, 8)
            if elem:
                d.regular_polygon((box[2] - 18, y + 18, 7), 6, fill=ELEMENT[elem])
            text(d, (box[0] + 66, y + 36), skills, 13, fill=DIM if worn else OFF)
            y += 70
        y += 8


def screen_party():
    im, d = canvas("world")
    d.text((48, 28), "Party", font=T(30), fill=TXT)
    text(d, (48, 66), "Who fights, who plays them, where they start", 15, fill=DIM)
    left_party(d, 1)
    _, cls, elem, tier, _, hp, ep, _ = PARTY[1]
    middle_loadouts(d, "Garrick", tier, "Generic", hp, ep)
    info_right(d, 1)
    footer(d, [("A", "Profile"), ("X", "Move start"), ("‹ ›", "Played by"), ("Y", "Loadout"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_party(), OUT, "1_party.png")
    print("ok")
