"""Hub and field overlays (UIScreens.md #14, #20, #29): the hub HUD with the interact prompt, talking to an NPC
(V, a character), and the recruit card for a character met in a trial. Lines of dialogue are placeholders: the words
are Crown's. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def party_strip(d, x=48, y=640):
    for i, (n, el, hp) in enumerate([("Sera", "Darkness", 1), ("Garrick", "Generic", 1), ("Bram", "Fire", 1)]):
        px = x + i * 150
        cut(d, (px, y, px + 138, y + 44), PANEL, STEEL, 8)
        d.ellipse((px + 10, y + 14, px + 26, y + 30), fill=ELEMENT[el])
        text(d, (px + 34, y + 6), n, 15, True, DIM)
        bar(d, px + 34, y + 30, 92, hp, ROSE, 5)


def prompt(d, cx, cy, key, verb, what):
    """The interact prompt floats by the thing: the key, a verb, and what it is."""
    w = int(d.textlength(verb + "  " + what, font=F(17, True))) + 70
    cut(d, (cx - w // 2, cy, cx + w // 2, cy + 40), PANEL, CYAN, 8)
    glyph(d, cx - w // 2 + 26, cy + 20, key, 12)
    text(d, (cx - w // 2 + 48, cy + 20), verb, 17, True, anchor="lm")
    text(d, (cx - w // 2 + 56 + int(d.textlength(verb, font=F(17, True))), cy + 20), what, 17, fill=DIM, anchor="lm")


# ---- 1: the hub. Almost nothing on screen: the party strip, a prompt by what you face. Money shows only when it changes.
im, d = canvas("world")
d.rectangle((600, 250, 680, 520), fill=(10, 11, 16))  # an NPC silhouette
d.ellipse((614, 200, 666, 252), fill=(10, 11, 16))
prompt(d, 640, 150, "X", "Talk", "Blacksmith")
party_strip(d)
cut(d, (1010, 40, 1232, 84), PANEL, STEEL, 8)  # the money chip: fades in on a change or near a vendor, then out
chip(d, 1026, 50, "Gold", "8,420", GOLD)
text(d, (1232, 96), "shown near vendors and when it changes", 12, fill=OFF, anchor="ra")
text(d, (48, 40), "Void Realm", 15, fill=OFF)
save(im, OUT, "1_hub_hud_prompt.png")

# ---- 2: talking. Speaker and line at the bottom, choices on the right; the world stays visible (no portrait box).
im, d = canvas("world")
d.rectangle((600, 250, 680, 520), fill=(10, 11, 16))
d.ellipse((614, 200, 666, 252), fill=(10, 11, 16))
panel(d, (160, 520, 1120, 660), focus=True)
text(d, (184, 532), "Blacksmith", 22, True, CYAN)
wrap(d, (184, 572), "The speaker's line goes here, one or two sentences at a time, so it reads at a glance.", 900, 19)
# D-178: the top choices are short actions (Shop, Talk, Leave); topics and new lines sit under Talk.
for i, c in enumerate(["Shop", "Talk", "Leave"]):
    y = 350 + i * 50
    row(d, (940, y, 1120, y + 42), c, False, right=">" if i == 1 else None, size=16)
    if i == 1:
        cut(d, (940, y, 1120, y + 42), None, CYAN, 8)  # Talk is open: its topics are to the left
        text(d, (1040, y + 21), "NEW", 12, True, CYAN, anchor="rm")
for i, (c, new) in enumerate([("Trials", False), ("(a new line, after a memory)", True)]):
    y = 400 + i * 50
    row(d, (600, y, 930, y + 42), c, i == 0, size=16)
    if new:
        text(d, (916, y + 21), "NEW", 12, True, CYAN, anchor="rm")
footer(d, [("A", "Choose"), ("Y", "Log"), ("B", "Leave")], y=694)
save(im, OUT, "2_npc_talk.png")

# ---- 3: a character met in the trial: a card with who they are, then Invite for this run. Played by: NPC or Player (D-179).
im, d = canvas("world")
d.rectangle((600, 250, 680, 520), fill=(10, 11, 16))
d.ellipse((614, 200, 666, 252), fill=(10, 11, 16))
panel(d, (760, 150, 1232, 560), focus=True)
icon(d, (780, 170, 880, 270), ELEMENT["Wind"])
text(d, (900, 172), "Vael", 28, True)
tier_badge(d, 900 + int(d.textlength("Vael", font=F(28, True))) + 22, 190, "B")
text(d, (900, 212), "Wind Refractor", 16, fill=DIM)
text(d, (900, 238), "Mind 5 · Body 2 · Spirit 4", 14, fill=DIM)
wrap(d, (780, 292), "Joins for this run only, with no gear of their own: they use what your party carries.", 432, 15)
text(d, (780, 360), "Your party", 15, True, DIM)
text(d, (1212, 360), "2 of 3 · room for one", 15, True, GOOD, anchor="ra")
text(d, (780, 390), "Played by", 15, fill=DIM)
text(d, (1212, 388), "‹  NPC  ›", 17, True, CYAN, anchor="ra")
cut(d, (780, 490, 1212, 540), TEAL, CYAN, 8)
text(d, (996, 515), "Invite for this run", 18, True, anchor="mm")
party_strip(d)
footer(d, [("A", "Invite"), ("R3", "Character sheet"), ("B", "Not now")], y=694)
save(im, OUT, "3_recruit_card.png")
print("ok")
