"""Front end (UIScreens.md #9, #10, #12): the title / main menu, the one-time "save was reset" notice (D-060),
and the loading screen for hub <-> trial (FeelGuide 10: an item or character description). One save per player
(WoRSaveGame.h), so there is no slot screen. Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def backdrop():
    """Stand-in for the title's key art: a dark scene with one crystal lit in the centre-right."""
    im, d = canvas()
    for i in range(9):
        d.regular_polygon((880, 330, 300 - i * 30), 6, rotation=30, fill=(18 + i * 3, 24 + i * 5, 36 + i * 7))
    d.regular_polygon((880, 330, 46), 6, rotation=30, fill=CYAN)
    return im, d


# ---- 1: title / main menu. Continue is first and focused when a save exists; its line says where you are.
im, d = backdrop()
d.text((72, 96), "World of", font=T(34), fill=DIM)
d.text((72, 136), "Refraction", font=T(72), fill=TXT)
items = [("Continue", "Void Realm · Sera, Garrick, Bram · 14 h"), ("New Game", None), ("Join a friend", None), ("Settings", None),
         ("Credits", None), ("Quit", None)]
for i, (lab, sub) in enumerate(items):
    y = 290 + i * 56
    row(d, (72, y, 480, y + 48), lab, i == 0, size=20)
    if sub and i == 0:
        text(d, (500, y + 24), sub, 15, fill=DIM, anchor="lm")
text(d, (72, 690), "v0.1 · early build", 13, fill=OFF, anchor="lm")
footer(d, [("A", "Select")])
save(im, OUT, "1_title.png")

# ---- 2: the save was reset (an older save version before the first public build is wiped with a warning, D-060).
im, d = backdrop()
im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), 0.6)
d = ImageDraw.Draw(im)
panel(d, (340, 200, 940, 500), focus=True)
text(d, (364, 216), "Your save was reset", 26, True, WARN)
wrap(d, (364, 264), "This build can't read saves from the older version, so the game starts fresh. This can happen until the first "
     "public release; after that, saves are always carried forward.", 552, 16)
text(d, (364, 390), "Old save", 15, fill=DIM)
text(d, (916, 390), "version 31", 15, True, anchor="ra")
text(d, (364, 418), "This build needs", 15, fill=DIM)
text(d, (916, 418), "version 37 or newer", 15, True, anchor="ra")
footer(d, [("A", "Start fresh")])
save(im, OUT, "2_save_reset_notice.png")

# ---- 3: New Game confirm when a save exists (one save per player: starting over replaces it).
im, d = backdrop()
im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), 0.6)
d = ImageDraw.Draw(im)
panel(d, (340, 220, 940, 470), focus=True)
text(d, (364, 236), "Start a new game?", 26, True)
wrap(d, (364, 284), "There is one save. Starting over replaces it: 14 hours, 3 characters, everything in the home chest.", 552, 16)
text(d, (364, 372), "This can't be undone.", 16, True, WARN)
text(d, (364, 410), "Hold A for 2 seconds to confirm.", 15, fill=DIM)
footer(d, [("A", "Hold: start over"), ("B", "Cancel")])
save(im, OUT, "3_new_game_confirm.png")

# ---- 4: loading (hub <-> trial only). A description to read; the autosave mark sits bottom left whenever saving.
im, d = canvas()
icon(d, (120, 250, 300, 430), ELEMENT["Fire"])
text(d, (340, 250), "Ashen Blade", 30, True)
text(d, (340, 296), "Sword · Fire", 16, fill=DIM)
wrap(d, (340, 340), "The item's or character's description goes here: two or three lines of lore the player can read while the trial loads.",
     760, 18)
d.regular_polygon((1190, 660, 16), 6, rotation=30, fill=None, outline=CYAN)
d.regular_polygon((1190, 660, 7), 6, rotation=30, fill=CYAN)
text(d, (1160, 660), "Entering the Ember Trial", 15, fill=DIM, anchor="rm")
d.regular_polygon((64, 660, 9), 4, rotation=45, fill=CYAN)
text(d, (84, 660), "Saving…", 14, fill=DIM, anchor="lm")
save(im, OUT, "4_loading.png")
print("ok")
