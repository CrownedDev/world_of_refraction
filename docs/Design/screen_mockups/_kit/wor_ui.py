"""Shared drawing kit for the out-of-battle screen mockups (docs/Design/UIScreens.md).

Same look as the approved battle HUD (combat/hud/refine_final, built on refine_v2's helpers): UI_Theme.md tokens,
cut-corner panels, teal focus, coloured face-button glyphs, hints at the bottom right.
Fonts: DejaVu (bundled with most Linux / Python installs) stands in for the real faces:
DejaVu Serif Bold for Spectral SC (titles), DejaVu Sans for Lato (labels, body, numbers).
Flat layout only, not the Hidris art. 1280x720."""
import os
from PIL import Image, ImageDraw, ImageFont

W, H = 1280, 720
_FONT_DIRS = ["/usr/share/fonts/truetype/dejavu", os.path.join(os.path.dirname(__file__), "fonts")]


def _font(name, size):
    for d in _FONT_DIRS:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.truetype(name, size)  # let PIL search (matplotlib ships DejaVu too)


_cache = {}


def F(size, bold=False):
    """Body / label face (Lato in the game)."""
    k = ("s", size, bold)
    if k not in _cache:
        _cache[k] = _font("DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf", size)
    return _cache[k]


def T(size):
    """Title face (Spectral SC SemiBold in the game)."""
    k = ("t", size)
    if k not in _cache:
        _cache[k] = _font("DejaVuSerif-Bold.ttf", size)
    return _cache[k]


# ---- UI_Theme.md tokens (same values as refine_v2)
BG, PANEL, PLATE, STEEL = (16, 18, 25), (20, 22, 28), (46, 46, 46), (140, 144, 152)
TRACK = (42, 45, 53)
TEAL, CYAN, TXT, DIM, OFF = (51, 127, 141), (159, 236, 245), (238, 241, 245), (169, 175, 184), (107, 112, 120)
ROSE, ROSE_LOW, EP = (224, 64, 127), (255, 143, 180), (111, 214, 228)
WARN, KILL, GOOD = (242, 162, 58), (255, 70, 70), (110, 200, 110)
GOLD = (232, 201, 121)
ELEMENT = {
    "Fire": (229, 72, 58), "Lightning": (242, 162, 58), "Earth": (216, 184, 66), "Wind": (79, 201, 138),
    "Water": (58, 123, 234), "Light": (243, 235, 208), "Darkness": (167, 155, 194), "Void": (166, 75, 240),
    "Reality": (91, 71, 200), "Generic": (176, 138, 92), "None": (140, 144, 152),
}
GLYPH = {"A": (110, 200, 110), "B": (230, 90, 90), "X": (90, 140, 240), "Y": (240, 200, 70)}
TIER = {"F": (107, 112, 120), "E": (140, 144, 152), "D": (169, 175, 184), "C": (199, 203, 210), "B": (225, 228, 233),
        "A": (232, 201, 121), "S": (217, 180, 74)}


def canvas(scene=None):
    """A blank screen. scene='world' draws a dimmed stand-in for the 3D world behind an overlay menu."""
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    if scene == "world":
        for i in range(12):  # soft horizon bands, dimmed: the world under a menu
            c = (24 + i * 2, 28 + i * 2, 40 + i)
            d.rectangle((0, 300 + i * 35, W, 335 + i * 35), fill=c)
        d.rectangle((0, 0, W, 300), fill=(20, 24, 36))
        for x, h in ((140, 160), (260, 110), (980, 190), (1130, 130)):  # pillars / ruins silhouettes
            d.rectangle((x, 300 - h, x + 46, 520), fill=(14, 16, 22))
    return im, d


def cut(d, box, fill, outline=None, c=14):
    """Hidris's shape: straight edges, one 45 degree cut corner (top-left)."""
    x0, y0, x1, y1 = box
    d.polygon([(x0 + c, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0 + c)], fill=fill, outline=outline)


def panel(d, box, focus=False, c=14):
    cut(d, box, PANEL, CYAN if focus else STEEL, c)


def bar(d, x, y, w, frac, col, h=14):
    d.rectangle((x, y, x + w, y + h), fill=TRACK)
    d.rectangle((x, y, x + int(w * max(0, min(1, frac))), y + h), fill=col)


def text(d, xy, s, size=18, bold=False, fill=TXT, anchor="la"):
    d.text(xy, s, font=F(size, bold), fill=fill, anchor=anchor)


def title(d, s, sub=None, x=48, y=34):
    d.text((x, y), s, font=T(34), fill=TXT)
    if sub:
        d.text((x, y + 48), sub, font=F(18), fill=DIM)


def tabs(d, x, y, labels, lit, h=34, size=18, lead=None):
    """A tab row (same as the battle HUD's kind / school rows). lead: a small bumper hint before the row."""
    if lead:
        d.text((x, y + 6), lead, font=F(16, True), fill=CYAN)
        x += int(d.textlength(lead, font=F(16, True))) + 10
    for i, lab in enumerate(labels):
        w = int(d.textlength(lab, font=F(size, True))) + 28
        cut(d, (x, y, x + w, y + h), TEAL if i == lit else PLATE, CYAN if i == lit else STEEL, 8)
        d.text((x + 14, y + (h - size) // 2 - 1), lab, font=F(size, True), fill=TXT if i == lit else DIM)
        x += w + 6
    return x


def glyph(d, x, y, key, r=14):
    """A face-button glyph (Xbox letters, PlayStation in the docs). Other keys draw as a small plate."""
    if key in GLYPH:
        d.ellipse((x - r, y - r, x + r, y + r), outline=GLYPH[key], width=3)
        d.text((x, y + 1), key, font=F(int(r * 1.1), True), fill=GLYPH[key], anchor="mm")
        return r * 2
    w = int(d.textlength(key, font=F(14, True))) + 14
    cut(d, (x - r, y - 12, x - r + w, y + 12), PLATE, STEEL, 5)
    d.text((x - r + w / 2, y + 1), key, font=F(14, True), fill=TXT, anchor="mm")
    return w


def footer(d, items, y=688, right=1240):
    """Input hints along the bottom right, right to left: [(key, verb), ...]. Clickable in the game; a setting hides them."""
    x = right
    for key, verb in reversed(items):
        tw = int(d.textlength(verb, font=F(17)))
        x -= tw
        d.text((x, y), verb, font=F(17), fill=DIM, anchor="lm")
        gw = 28 if key in GLYPH else int(d.textlength(key, font=F(14, True))) + 14
        x -= gw + 8
        glyph(d, x + 14, y, key)
        x -= 26


def row(d, box, label, focus=False, right=None, sub=None, dot=None, dim=False, size=20):
    """A list row: focus = teal plate (Hidris selected state)."""
    x0, y0, x1, y1 = box
    cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 8)
    tx = x0 + 16
    if dot:
        d.ellipse((tx, (y0 + y1) // 2 - 8, tx + 16, (y0 + y1) // 2 + 8), fill=dot)
        tx += 26
    col = OFF if dim else TXT
    if sub:
        d.text((tx, y0 + 6), label, font=F(size, True), fill=col)
        d.text((tx, y0 + 10 + size), sub, font=F(15), fill=DIM if not dim else OFF)
    else:
        d.text((tx, (y0 + y1) // 2), label, font=F(size, True), fill=col, anchor="lm")
    if right:
        d.text((x1 - 14, (y0 + y1) // 2), right, font=F(18, True), fill=DIM if not focus else TXT, anchor="rm")


def tier_badge(d, x, y, letter, r=13):
    """Combat Tier / item tier: octagon, dark letter (UI_Theme section 6)."""
    d.regular_polygon((x, y, r), 8, rotation=22.5, fill=TIER[letter])
    d.text((x, y + 1), letter, font=F(int(r * 1.15), True), fill=BG, anchor="mm")


def icon(d, box, col=STEEL, letter=None, empty=False):
    """A square item icon stand-in: cut plate with an element-coloured gem (or an empty-slot outline)."""
    x0, y0, x1, y1 = box
    if empty:
        cut(d, box, BG, OFF, 8)
        if letter:
            d.text(((x0 + x1) / 2, (y0 + y1) / 2), letter, font=F(14), fill=OFF, anchor="mm")
        return
    cut(d, box, PLATE, STEEL, 8)
    cx, cy, r = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) * 0.26
    d.regular_polygon((cx, cy, r), 6, fill=col)
    if letter:
        d.text((cx, cy + 1), letter, font=F(int(r), True), fill=BG, anchor="mm")


def chip(d, x, y, label, value, col=GOLD):
    """A currency / counter chip: coloured diamond, value, label."""
    d.regular_polygon((x + 10, y + 12, 9), 4, rotation=45, fill=col)
    d.text((x + 26, y + 12), value, font=F(18, True), fill=TXT, anchor="lm")
    vx = x + 32 + int(d.textlength(value, font=F(18, True)))
    d.text((vx, y + 12), label, font=F(15), fill=DIM, anchor="lm")
    return vx + int(d.textlength(label, font=F(15))) + 24


def wrap(d, xy, s, width, size=17, fill=TXT, bold=False, gap=6):
    """Draw text wrapped to a pixel width; returns the y after the last line."""
    x, y = xy
    words, line = s.split(), ""
    for w in words:
        t = (line + " " + w).strip()
        if d.textlength(t, font=F(size, bold)) > width and line:
            d.text((x, y), line, font=F(size, bold), fill=fill)
            y += size + gap
            line = w
        else:
            line = t
    if line:
        d.text((x, y), line, font=F(size, bold), fill=fill)
        y += size + gap
    return y


def save(im, folder, name):
    im = im.convert("P", palette=Image.ADAPTIVE, colors=128)  # flat art: a small palette keeps the PNGs small
    im.save(os.path.join(folder, name), optimize=True)
