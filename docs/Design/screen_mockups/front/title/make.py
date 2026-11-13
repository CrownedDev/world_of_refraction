"""Front end (UIScreens.md #9, #10, #12): the title / main menu, the one-time "save was reset" notice (D-060),
and the loading screen for hub <-> trial (FeelGuide 10: an item or character description). One save per player
(WoRSaveGame.h), so there is no slot screen. Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


# The deckbuilder's title look (Crown, 2026-11-14; its tools/ui_drafts/title_v2.py): the starry nebula, the spinning
# vortex, the spaced serif title and the ornament divider. The art is copied to SourceArt/UI/Title.
from PIL import ImageChops, ImageFont  # noqa: E402

ART = os.path.join(OUT, "..", "..", "..", "..", "..", "SourceArt", "UI", "Title")
NEBULA = Image.open(os.path.join(ART, "title_nebula.png")).convert("RGB")
RULE = Image.open(os.path.join(ART, "title_rule.png")).convert("RGBA")
PORTAL = [Image.open(os.path.join(ART, "portal_arcane_swirl", n)).convert("RGBA")
          for n in sorted(os.listdir(os.path.join(ART, "portal_arcane_swirl")))]
HEX, HEX_R, VORTEX = (880, 330), 290, 900  # where the hexagon sat; the vortex fills it and is cut to its shape
INK, VIOLET = (240, 234, 222), (170, 100, 240)


def serif(size):
    try:
        return ImageFont.truetype("constan.ttf", size)  # the deckbuilder's placeholder display face (Windows)
    except OSError:
        return T(size)


def spaced(d, xy, s, font, fill, tracking):
    x, y = xy
    for ch in s:
        d.text((x, y), ch, font=font, fill=fill)
        x += d.textlength(ch, font=font) + tracking
    return x


def backdrop(at=9.0):
    """The nebula with the vortex turning inside the hexagon. at: the vortex frame (fractions blend two frames)."""
    im = NEBULA.copy()
    i, frac = int(at) % len(PORTAL), at - int(at)
    frame = Image.blend(PORTAL[i], PORTAL[(i + 1) % len(PORTAL)], frac).resize((VORTEX, VORTEX), Image.BICUBIC)
    glow = Image.composite(frame.convert("RGB"), Image.new("RGB", frame.size), frame.getchannel("A"))
    mask = Image.new("L", im.size)
    ImageDraw.Draw(mask).regular_polygon((HEX[0], HEX[1], HEX_R), 6, rotation=30, fill=255)
    lit = im.copy()
    box = (HEX[0] - VORTEX // 2, HEX[1] - VORTEX // 2)
    lit.paste(ImageChops.add(im.crop(box + (box[0] + VORTEX, box[1] + VORTEX)), glow), box)  # additive, as a light
    im = Image.composite(lit, im, mask)
    d = ImageDraw.Draw(im)
    for r, col in ((HEX_R, VIOLET), (HEX_R + 8, (70, 44, 110))):
        d.regular_polygon((HEX[0], HEX[1], r), 6, rotation=30, outline=col, width=2)
    return im, d


def title_screen(at=9.0):
    """Title / main menu. Continue is first and focused when a save exists; its line says where you are."""
    im, d = backdrop(at)
    spaced(d, (72, 112), "WORLD OF REFRACTION", serif(40), INK, 5)
    rule = RULE.resize((440, int(RULE.height * 440 / RULE.width)), Image.LANCZOS)
    im.paste(rule, (72, 168), rule)
    spaced(d, (72, 212), "STORIES OF THE VOID", F(17), (222, 224, 236), 7)
    items = [("Continue", "Void Realm · Sera, Garrick, Bram · 14 h"), ("New Game", None), ("Join a friend", None),
             ("Settings", None), ("Credits", None), ("Quit", None)]
    for i, (lab, sub) in enumerate(items):
        y = 290 + i * 56
        row(d, (72, y, 480, y + 48), lab, i == 0, size=20)
        if sub and i == 0:  # the Continue line sits inside its row now, clear of the vortex
            text(d, (466, y + 24), sub, 12, fill=TXT, anchor="rm")
    text(d, (72, 690), "v0.1 · early build", 13, fill=OFF, anchor="lm")
    footer(d, [("A", "Select")])
    return im


# ---- 1: the title, as a still and as a short loop of the vortex's spin (one turn: 64 frames at 12 fps, half size).
title_screen().save(os.path.join(OUT, "1_title.png"), optimize=True)  # full colour: the kit's 128-colour save bands the nebula
frames = [title_screen(f / 2).resize((640, 360), Image.LANCZOS) for f in range(2 * len(PORTAL))]
pal = frames[0].quantize(colors=255, method=Image.Quantize.FASTOCTREE)
frames = [fr.quantize(palette=pal, dither=Image.Dither.NONE) for fr in frames]
frames[0].save(os.path.join(OUT, "1_title_spin.gif"), save_all=True, append_images=frames[1:], duration=83, loop=0, optimize=True)

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
