"""B-294: combat HUD mockup v3 (D-106, element palette and colour-blind readability).

Same fight and pieces as v2 (combat_hud_v2.py), with:
  - the tuned element palette (analysis/ElementPalette_2026-10-29.md section 3a, as ElementColors.h): identity kept,
    lightness spread so no element pair falls under CIEDE2000 10 for protan, deutan or tritan viewers.
    Darkness stays near-black grey (no purple tint) and gets a pale outline as text; Ice has its own pale colour.
  - an element gem beside every damage number (Physical: none), default on ("Element markers", B-294b)
  - resistance shown by size, transparency and glow, not by a darker or different colour: a darker shade would
    move a number down the lightness ladder that tells Lightning, Earth and Fire apart for protan/deutan viewers
  - a palette sheet: every element's number as seen in normal, protan, deutan and tritan vision (Machado 2009)

Drafts: hit, palette, command, status_bars.
    python Tools/ui_drafts/combat_hud_v3.py
Writes docs/Design/screen_mockups/combat/hud/v3/.
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import kit  # noqa: E402
import combat_hud_v1 as v1  # noqa: E402
import combat_hud_v2 as v2  # noqa: E402  (v2's plates, bars and gems are reused as they are)

SCREEN, VERSION = "combat/hud", "v3"

# ElementColors.h (B-294), sRGB 8-bit. v1/v2 pieces read v1.ELEMENT, so the palette is applied there.
v1.ELEMENT.update({
    "fire": (176, 42, 24), "lightning": (255, 204, 0), "earth": (208, 120, 46), "wind": (46, 174, 146),
    "water": (42, 120, 224), "reality": (90, 46, 212), "void": (196, 140, 255), "light": (255, 251, 216),
    "darkness": (39, 39, 39), "generic": (203, 155, 124),
})
ICE = (196, 241, 255)
PHYSICAL = (255, 255, 255)
TEXT_OUTLINE_PALE = (230, 239, 244)   # ElementColors::TextOutline, for Darkness numbers
TEXT_OUTLINE_DARK = (10, 10, 14)
IMMUNE_GREY = (104, 106, 118)

# Resistance -> how the number is drawn (size factor, alpha, glow). Colour stays the element's.
WEAK, NEUTRAL, RESISTED, IMMUNE = "weak", "neutral", "resisted", "immune"
NUMBER_STYLE = {WEAK: (1.25, 255, True), NEUTRAL: (1.0, 255, False), RESISTED: (0.8, 153, False),
                IMMUNE: (0.7, 255, False)}
GEM_HEIGHT = 0.8                      # gem height as a share of the number's size

# Machado 2009, severity 1.0, applied in linear RGB (ElementColorVision.cpp).
VISIONS = {
    "protan": [[0.152286, 1.052583, -0.204868], [0.114503, 0.786281, 0.099216], [-0.003882, -0.048116, 1.051998]],
    "deutan": [[0.367322, 0.860646, -0.227968], [0.280085, 0.672501, 0.047413], [-0.011820, 0.042940, 0.968881]],
    "tritan": [[1.255528, -0.076749, -0.178779], [-0.078411, 0.930809, 0.147602], [0.004733, 0.691367, 0.303900]],
}


def colour_of(element):
    return {"ice": ICE, "physical": PHYSICAL}.get(element, v1.ELEMENT.get(element, PHYSICAL))


def damage_number(canvas, centre, value, element, state=NEUTRAL, size=40):
    """A damage number with its element gem on the leading edge (D-106)."""
    factor, alpha, glow = NUMBER_STYLE[state]
    px = round(size * factor)
    fill = IMMUNE_GREY if state == IMMUNE else colour_of(element)
    outline = TEXT_OUTLINE_PALE if element == "darkness" else TEXT_OUTLINE_DARK
    layer = Image.new("RGBA", (kit.W, kit.H))
    if glow:
        kit.glow(layer, centre, px * 1.4, fill, strength=0.45)
    f = kit.font(kit.UI_BOLD, px)
    text_w = ImageDraw.Draw(layer).textlength(str(value), font=f)
    if element != "physical":
        gem = round(px * GEM_HEIGHT)
        gem_element = "water" if element == "ice" else element   # Ice: Water's gem until it has its own
        v2.element_gem(layer, (centre[0] - text_w / 2 - gem * 0.6, centre[1]), gem_element, gem)
    v1.outlined(layer, centre, str(value), f, fill, outline=outline)
    if alpha < 255:
        a = layer.getchannel("A").point(lambda v: v * alpha // 255)
        layer.putalpha(a)
    canvas.alpha_composite(layer)


def simulate(img, matrix):
    """The image as seen with a colour-vision deficiency (sRGB -> linear -> Machado -> sRGB)."""
    rgb = np.asarray(img.convert("RGB"), dtype=np.float64) / 255.0
    lin = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    sim = np.clip(lin @ np.asarray(matrix).T, 0.0, 1.0)
    out = np.where(sim <= 0.0031308, sim * 12.92, 1.055 * sim ** (1 / 2.4) - 0.055)
    return Image.fromarray(np.uint8(np.round(out * 255)), "RGB").convert("RGBA")


# --- the drafts -------------------------------------------------------------------------------------------------

def draw_hit():
    canvas = v1.board(active=0)
    v2.turn_banner(canvas, "TURN 3 · GARRICK")
    hits = [(0, 31, WEAK), (1, 54, NEUTRAL), (2, 12, RESISTED)]   # a Fire sweep over three targets
    v2.enemy_plate(canvas, v1.ENEMIES[v1.TARGET], ghost=0.09, hp=(v1.TARGET_HP_BEFORE - 54, 600), buildup=("fire", 1.0),
                   shattering=True, statuses=(("effect_damage_up_down", "debuff"), ("effect_damage_over_time_fire", "debuff")))
    for index, dmg, state in hits:
        cx, cy = v1.ENEMIES[index]["centre"]
        damage_number(canvas, (cx + 40, cy - 46), dmg, "fire", state, 44 if index == v1.TARGET else 36)
    key_y = 640
    for i, (label, state) in enumerate((("weak", WEAK), ("neutral", NEUTRAL), ("resisted", RESISTED), ("immune", IMMUNE))):
        x = 190 + i * 110
        damage_number(canvas, (x, key_y), 88, "fire", state, 26)
        kit.text(canvas, (x, key_y + 26), label, kit.font(kit.UI_FONT, 11), fill=kit.DIM, shadow=False)
    kit.text(canvas, (190 - 40, key_y - 30), "MOCKUP KEY · size / alpha / glow = target's resistance", kit.font(kit.UI_BOLD, 10),
             fill=kit.DIM, anchor="lm", shadow=False)
    v2.party_row(canvas, acting=0)
    cx, cy = v1.ENEMIES[v1.TARGET]["centre"]
    (wx, wy), (rx, ry) = v1.ENEMIES[0]["centre"], v1.ENEMIES[2]["centre"]
    notes = [
        (1, (wx + 80, wy - 80), "Weak target", ["Bigger, with a glow; same Fire colour", "Gem on the leading edge says Fire"]),
        (2, (cx + 90, cy - 80), "Neutral target", ["Fire colour, normal size"]),
        (3, (rx + 80, ry - 80), "Resisting target", ["Smaller and see-through (60%), same hue: not darker",
                                                     "Immune: small and grey"]),
        (4, (130, 640), "Mockup key", ["Not in game: the four states side by side"]),
    ]
    return canvas, notes


def draw_palette():
    canvas = kit.background("dfhb_battle", blur=3, dark=0.6)
    kit.text(canvas, (kit.W // 2, 36), "ELEMENT PALETTE · AS SEEN WITH COLOUR-BLINDNESS", kit.font(kit.DISPLAY_BOLD, 22), tracking=2)
    order = v2.ELEMENT_ORDER + ["ice", "physical"]
    step, x0 = 104, 120
    strip_h = 110
    strip = Image.new("RGBA", (kit.W, strip_h))
    strip.alpha_composite(canvas.crop((0, 70, kit.W, 70 + strip_h)))
    for i, element in enumerate(order):
        damage_number(strip, (x0 + i * step, 44), 128, element, NEUTRAL, 30)
        kit.text(strip, (x0 + i * step, 86), element.capitalize(), kit.font(kit.UI_FONT, 12), fill=kit.DIM, shadow=False)
    rows = [("normal", strip)] + [(name, simulate(strip, m)) for name, m in VISIONS.items()]
    notes = []
    for r, (name, img) in enumerate(rows):
        y = 70 + r * (strip_h + 30)
        canvas.alpha_composite(img, (0, y))
        kit.text(canvas, (24, y + 44), name, kit.font(kit.UI_BOLD, 13), anchor="lm", shadow=False)
    notes.append((1, (x0 - 60, 114), "Normal vision", ["The nine elements, Ice and Physical as damage numbers",
                                                        "Darkness: near-black with a pale outline (Crown)"]))
    notes.append((2, (x0 - 60, 114 + 3 * (strip_h + 30)), "Simulated", ["Machado 2009 protan / deutan / tritan",
                                                                        "Every element pair stays >= 10 apart (WoR.Elements.Palette)",
                                                                        "Closest: Water / Void for protan; the gem tells them apart"]))
    return canvas, notes


if __name__ == "__main__":
    for name, draw in (("hit", draw_hit), ("palette", draw_palette), ("command", v2.draw_command),
                       ("status_bars", v2.draw_status_bars)):
        plain, notes = draw()
        kit.save_draft(plain, notes, SCREEN, VERSION, name)
