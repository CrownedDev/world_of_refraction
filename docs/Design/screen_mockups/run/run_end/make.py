"""After a fight and at the end of a run (UIScreens.md #25-27): reward popups (no results screen, FeelGuide 6),
a wipe, and the run-end tally (boss beaten / run over). Numbers: DefeatRules.h (5 wipes, 3 main-boss losses,
points lost 1 / 2 / 0), RunRewards.h. Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def dim(im, k=0.55):
    im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), k)
    return im, ImageDraw.Draw(im)


def toast(d, y, head, body, col=STEEL, tag=None, w=380):
    """A reward popup on the right edge: it slides in, stays a few seconds, fades. Never needs a press."""
    x = W - 48 - w
    cut(d, (x, y, x + w, y + 62), PANEL, col, 10)
    d.rectangle((x, y + 12, x + 4, y + 62), fill=col)
    text(d, (x + 18, y + 8), head, 15, True, col if col != STEEL else DIM)
    text(d, (x + 18, y + 30), body, 18, True)
    if tag:
        text(d, (x + w - 14, y + 10), tag, 13, fill=CYAN, anchor="ra")
    return y + 70


# ---- 1: back in the world after a win. Popups stack on the right and fade on their own; nothing to click.
im, d = canvas("world")
y = 150
y = toast(d, y, "Victory", "+180 Prisms")
y = toast(d, y, "Found", "Garnet (B) · crystal", ELEMENT["Fire"])
y = toast(d, y, "Level banked", "Garrick +1 · place it at a rest spot", CYAN)
y = toast(d, y, "New memory", "The Smith's Promise", GOLD, tag="J  Journal")
y = toast(d, y, "Hidden quest discovered", "Ash on the Anvil", ELEMENT["Void"], tag="only you see this")
panel(d, (48, 600, 460, 690))  # the small trial HUD (#28): run money, wipes left
x = chip(d, 64, 612, "Prisms", "1,420", EP)
chip(d, x, 612, "wipes left", "3 of 5", WARN)
text(d, (64, 652), "The Cinder Oath · beat the mini-boss without items", 14, fill=DIM)
text(d, (48, 40), "In the world, a moment after the fight", 15, fill=OFF)
save(im, OUT, "1_reward_popups.png")

# ---- 2: a wipe. Short and plain: what it cost, where you wake, how many are left. One button.
im, d = canvas("world")
im, d = dim(im, 0.7)
d.text((W / 2, 150), "The party has fallen", font=T(40), fill=TXT, anchor="mm")
text(d, (W / 2, 200), "Cinder Warden · mini-boss", 18, fill=DIM, anchor="mm")
panel(d, (380, 250, 900, 520), focus=True)
text(d, (404, 264), "Lost", 16, True, DIM)
for i, (n, what) in enumerate([("Sera", "Mind  5 → 4"), ("Garrick", "Body  7 → 6,  Mind  2 → 1"), ("Bram", "nothing to lose")]):
    text(d, (404, 294 + i * 30), n, 17, True)
    text(d, (876, 294 + i * 30), what, 17, fill=ROSE_LOW if i < 2 else DIM, anchor="ra")
d.line((404, 392, 876, 392), fill=STEEL)
text(d, (404, 406), "You wake at", 16, fill=DIM)
text(d, (876, 406), "Ash Fields rest spot", 17, True, anchor="ra")
text(d, (404, 438), "The enemy", 16, fill=DIM)
text(d, (876, 438), "is still there", 17, True, anchor="ra")
text(d, (404, 470), "Wipes", 16, fill=DIM)
for i in range(5):
    d.regular_polygon((780 + i * 22, 481, 8), 4, rotation=45, fill=WARN if i < 2 else TRACK)
text(d, (760, 470), "2 of 5", 17, True, WARN, anchor="ra")
footer(d, [("A", "Get up")])
save(im, OUT, "2_wipe.png")


def tally(d, heading, sub, col, share, rows, kept, lost, button):
    d.text((W / 2, 96), heading, font=T(40), fill=col, anchor="mm")
    text(d, (W / 2, 142), sub, 18, fill=DIM, anchor="mm")
    panel(d, (140, 190, 700, 600), focus=True)
    text(d, (164, 204), "Payout", 16, True, DIM)
    text(d, (676, 204), share, 16, True, CYAN, anchor="ra")
    for i, (lab, val, c) in enumerate(rows):
        y = 240 + i * 40
        d.regular_polygon((174, y + 12, 9), 4, rotation=45, fill=c)
        text(d, (196, y), lab, 19)
        text(d, (676, y - 2), val, 22, True, anchor="ra")
    d.line((164, 420, 676, 420), fill=STEEL)
    text(d, (164, 434), "This run", 16, True, DIM)
    for i, (lab, val) in enumerate([("Fights won", "9"), ("Mini-bosses", "2 (1 revived)"), ("Wipes", "2"), ("Time", "38 min")]):
        text(d, (164 + (i % 2) * 260, 464 + (i // 2) * 34), lab, 16, fill=DIM)
        text(d, (400 + (i % 2) * 276, 464 + (i // 2) * 34), val, 16, True, anchor="ra")
    panel(d, (730, 190, 1140, 390))
    text(d, (750, 204), "Kept", 16, True, GOOD)
    y = 234
    for k in kept:
        y = wrap(d, (750, y), "· " + k, 370, 15) + 2
    panel(d, (730, 404, 1140, 600))
    text(d, (750, 418), "Lost", 16, True, ROSE_LOW)
    y = 448
    for k in lost:
        y = wrap(d, (750, y), "· " + k, 370, 15, DIM) + 2
    footer(d, [("A", button)])


KEPT = ["Items found, sent to the home chest", "Upgrade levels on your gear", "Memories and quest steps"]
LOST = ["Run levels (Mind, Body, Spirit)", "Prisms", "Run-only recruits", "Crystals slotted during the run"]
ROWS = [("Gold", "2,180", GOLD), ("Gear Essence", "36", STEEL), ("Skill Essence", "12", STEEL), ("Fire Essence", "8", ELEMENT["Fire"])]

# ---- 3: the boss is beaten: the run's one full-screen moment. Full payout.
im, d = canvas("world")
im, d = dim(im, 0.7)
tally(d, "Trial cleared", "Ember Trial · Present · Medium", GOLD, "Full payout  x1.0", ROWS, KEPT, LOST, "Return to the Void Realm")
save(im, OUT, "3_trial_cleared.png")

# ---- 4: run over (five wipes, or three losses to the boss). Same screen, same full payout (falling pays in full).
im, d = canvas("world")
im, d = dim(im, 0.8)
tally(d, "The trial ends", "Fifth wipe · Ember Trial · Present · Medium", ROSE_LOW, "Full payout  x1.0", ROWS, KEPT, LOST,
      "Return to the Void Realm")
save(im, OUT, "4_run_over.png")
print("ok")
