"""B-490 (D-212): three one-second battle transitions, storyboarded as key frames. Field and stage are stand-ins.
A  Refraction shatter: the frozen frame cracks from the clash point and breaks into shards that bend the light.
B  Prism flash: a white flash, the picture splits into its colours and folds into one beam through a prism.
C  Facet turn: the world turns into crystal facets from the clash point; the facets flip over onto the stage.
Each option: frames <option>_<n>_<name>.png plus a sheet <option>_sheet.png (timing, sound, loading per frame).
strike_variants.png: player-first strike, plain clash and surprise attack on option A.
Run: python make.py"""
import colorsys
import math
import os
import random
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401
from PIL import ImageChops, ImageFilter  # noqa: E402

CX, CY = 640, 430  # the clash point: where the party leader touches the enemy


def spectral(a, s=0.55, v=1.0):
    r, g, b = colorsys.hsv_to_rgb(a % 1.0, s, v)
    return int(r * 255), int(g * 255), int(b * 255)


def gradient(top, bottom, y0=0, y1=H, im=None):
    im = im or Image.new("RGB", (W, H), top)
    d = ImageDraw.Draw(im)
    for y in range(y0, y1):
        t = (y - y0) / max(1, y1 - y0)
        d.line((0, y, W, y), fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return im


def figure(d, x, y, col, rim, s=1.0, sword=True):
    """A party member stand-in: body, head, a sword."""
    d.ellipse((x - 22 * s, y - 92 * s, x + 22 * s, y), fill=col, outline=rim, width=2)
    d.ellipse((x - 15 * s, y - 124 * s, x + 15 * s, y - 94 * s), fill=col, outline=rim, width=2)
    if sword:
        d.line((x + 18 * s, y - 50 * s, x + 58 * s, y - 108 * s), fill=(220, 226, 240), width=max(2, int(4 * s)))


def beast(d, x, y, col, s=1.0):
    """An enemy stand-in: a crystal-backed beast with glowing eyes."""
    pts = [(x - 60 * s, y), (x - 70 * s, y - 50 * s), (x - 40 * s, y - 70 * s), (x - 30 * s, y - 120 * s),
           (x - 5 * s, y - 80 * s), (x + 15 * s, y - 135 * s), (x + 30 * s, y - 78 * s), (x + 60 * s, y - 100 * s),
           (x + 65 * s, y - 40 * s), (x + 55 * s, y)]
    d.polygon(pts, fill=col, outline=(255, 150, 120))
    for ex in (-38, -22):
        d.ellipse((x + ex * s - 4, y - 52 * s - 4, x + ex * s + 4, y - 52 * s + 4), fill=(255, 230, 120))


def field():
    """The trial map at the moment of the clash: leader and enemy touching at (CX, CY)."""
    im = gradient((24, 18, 46), (70, 52, 110), 0, 360)
    gradient((44, 38, 62), (20, 18, 28), 360, H, im)
    d = ImageDraw.Draw(im)
    rnd = random.Random(3)
    for _ in range(70):
        x, y = rnd.randrange(W), rnd.randrange(300)
        d.point((x, y), fill=(200, 190, 240))
    for x, h in ((80, 170), (210, 120), (1010, 200), (1150, 140), (900, 90)):
        d.rectangle((x, 360 - h, x + 50, 380), fill=(30, 26, 44))
    d.polygon([(560, 720), (720, 720), (680, 360), (600, 360)], fill=(52, 46, 70))
    figure(d, CX - 40, CY + 40, (64, 74, 104), CYAN)
    beast(d, CX + 80, CY + 40, (120, 44, 60))
    return im


def stage():
    """The battle stage after the swap: the party left, the enemies right, a lit floor ring."""
    im = gradient((10, 8, 20), (44, 30, 70), 0, 400)
    gradient((36, 28, 54), (12, 10, 18), 400, H, im)
    d = ImageDraw.Draw(im)
    d.ellipse((140, 420, 1140, 700), outline=(150, 120, 230), width=3)
    d.ellipse((200, 450, 1080, 670), outline=(80, 64, 130), width=2)
    for x, y in ((330, 560), (230, 620), (430, 640)):
        figure(d, x, y, (64, 74, 104), CYAN, 0.9)
    beast(d, 900, 560, (120, 44, 60), 1.0)
    beast(d, 1040, 630, (110, 40, 70), 0.8)
    return im


def void(seed=5):
    """What is behind the broken picture: a dark violet void with a faint star field."""
    im = Image.new("RGB", (W, H), (8, 6, 14))
    d = ImageDraw.Draw(im)
    for r in range(520, 0, -20):
        k = 1 - r / 520
        d.ellipse((CX - r * 1.4, CY - 80 - r, CX + r * 1.4, CY - 80 + r), fill=(int(8 + 40 * k), int(6 + 20 * k), int(14 + 60 * k)))
    rnd = random.Random(seed)
    for _ in range(120):
        d.point((rnd.randrange(W), rnd.randrange(H)), fill=(180, 170, 230))
    return im


def glow(im, x, y, r, col, strength=1.0):
    """An additive soft light at (x, y)."""
    g = Image.new("RGB", im.size, (0, 0, 0))
    d = ImageDraw.Draw(g)
    for k in range(12, 0, -1):
        rr = r * k / 12
        c = tuple(int(col[i] * strength * (1 - k / 13)) for i in range(3))
        d.ellipse((x - rr, y - rr, x + rr, y + rr), fill=c)
    return ImageChops.add(im, g.filter(ImageFilter.GaussianBlur(r / 6)))


def rays(im, x, y, n, length, col, width=2, seed=1):
    d = ImageDraw.Draw(im)
    rnd = random.Random(seed)
    for i in range(n):
        a = i / n * 2 * math.pi + rnd.random() * 0.2
        l0, l1 = length * 0.25, length * (0.6 + 0.4 * rnd.random())
        d.line((x + math.cos(a) * l0, y + math.sin(a) * l0, x + math.cos(a) * l1, y + math.sin(a) * l1), fill=col, width=width)
    return im


def tag(im, s):
    d = ImageDraw.Draw(im)
    w = d.textlength(s, font=F(15, True))
    d.rectangle((16, 14, 36 + w, 44), fill=(10, 10, 16))
    text(d, (26, 20), s, 15, True, TXT)
    return im


def spinner(im):
    """Only if the load runs past about 2 s (D-212): a small turning crystal, bottom right."""
    d = ImageDraw.Draw(im)
    x, y = 1210, 650
    d.regular_polygon((x, y, 16), 6, rotation=15, outline=CYAN, width=2)
    d.regular_polygon((x, y, 7), 6, rotation=45, fill=CYAN)
    text(d, (x - 28, y - 7), "(only past 2 s)", 11, False, DIM, anchor="ra")
    return im


# ---------- A: refraction shatter
def shards(seed=7, cols=9, rows=6):
    """Jittered grid split into triangles: the pieces the picture breaks into."""
    rnd = random.Random(seed)
    pts = [[(c * W / cols + (rnd.uniform(-45, 45) if 0 < c < cols else 0),
             r * H / rows + (rnd.uniform(-35, 35) if 0 < r < rows else 0)) for c in range(cols + 1)] for r in range(rows + 1)]
    tris = []
    for r in range(rows):
        for c in range(cols):
            a, b, e, f = pts[r][c], pts[r][c + 1], pts[r + 1][c], pts[r + 1][c + 1]
            tris += [(a, b, f), (a, f, e)] if (r + c) % 2 else [(a, b, e), (b, f, e)]
    return tris


TRIS = shards()


def shatter(src, back, t, crack_col=(235, 240, 255), tint=0.25):
    """The picture broken into TRIS; t 0..1 moves each shard away from the clash point, turns and tints it."""
    im = back.copy()
    rnd = random.Random(11)
    for tri in TRIS:
        mx = sum(p[0] for p in tri) / 3
        my = sum(p[1] for p in tri) / 3
        dx, dy = mx - CX, my - CY
        dist = math.hypot(dx, dy) or 1
        speed = (0.6 + rnd.random()) * (1.2 - dist / 1600)
        ox, oy = dx / dist * t * 520 * speed, dy / dist * t * 520 * speed + t * t * 160
        rot = (rnd.random() - 0.5) * 70 * t
        x0, y0 = int(min(p[0] for p in tri)), int(min(p[1] for p in tri))
        x1, y1 = int(max(p[0] for p in tri)) + 1, int(max(p[1] for p in tri)) + 1
        mask = Image.new("L", (x1 - x0, y1 - y0), 0)
        ImageDraw.Draw(mask).polygon([(p[0] - x0, p[1] - y0) for p in tri], fill=255)
        piece = src.crop((x0, y0, x1, y1))
        hue = spectral(math.atan2(dy, dx) / (2 * math.pi), 0.6)
        piece = Image.blend(piece, Image.new("RGB", piece.size, hue), tint * min(1, t * 3 + 0.3))
        rgba = piece.convert("RGBA")
        rgba.putalpha(mask)
        ImageDraw.Draw(rgba).polygon([(p[0] - x0, p[1] - y0) for p in tri], outline=crack_col + (255,))
        scale = max(0.2, 1 - t * 0.5)
        rgba = rgba.resize((max(1, int(rgba.width * scale)), max(1, int(rgba.height * scale))))
        rgba = rgba.rotate(rot, expand=True, resample=Image.BICUBIC)
        im.paste(rgba, (int(mx + ox - rgba.width / 2), int(my + oy - rgba.height / 2)), rgba)
    return im


def cracks(src, reach, col=(235, 240, 255), fringe=True, origin=(CX, CY)):
    """Crack lines spreading from origin: every shard edge inside the reach, with a split-colour fringe."""
    im = src.copy()
    d = ImageDraw.Draw(im)
    ox, oy = origin
    for tri in TRIS:
        for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
            if math.hypot(a[0] - ox, a[1] - oy) < reach and math.hypot(b[0] - ox, b[1] - oy) < reach:
                if fringe:
                    d.line((a[0] - 2, a[1], b[0] - 2, b[1]), fill=(255, 80, 120), width=2)
                    d.line((a[0] + 2, a[1], b[0] + 2, b[1]), fill=(80, 160, 255), width=2)
                d.line((a, b), fill=col, width=2)
    return im


def freeze(im, k=0.35):
    """The frame stops: colour drains a little."""
    return Image.blend(im, im.convert("L").convert("RGB"), k)


A = [("1_clash", "0.00–0.08 s", "The clash freezes the frame; a white burst at the touch point, colour drains.",
      "A sharp crystal 'tink' plus a low hit; the field music cuts.", "Load starts (the battle map, read during the join window)."),
     ("2_cracks", "0.08–0.25 s", "Cracks run out from the touch point with a red / blue split edge, like light through glass.",
      "Glass splintering, rising.", "Loading."),
     ("3_break", "0.25–0.45 s", "The whole picture breaks into tinted shards, each bending the colour; dark void in the gaps.",
      "The break: a bright crash with a choir swell.", "Loading."),
     ("4_fall", "0.45–0.80 s (hold)", "Shards drift outward over the violet void and one prism beam. This frame can hold still.",
      "A ringing shimmer that can loop.", "The load hides here: hold this frame until the stage is ready; spinner only past 2 s."),
     ("5_stage", "0.80–1.00 s", "The last shards fly off the edges and the battle stage is there; the HUD slides in.",
      "Battle music downbeat.", "Done: stage loaded.")]


def option_a(variant="plain"):
    f = field()
    st = stage()
    vd = void()
    beam = glow(vd, CX, CY - 80, 260, (120, 90, 200), 0.8)
    ImageDraw.Draw(beam).line((0, CY - 80, W, CY - 80), fill=(200, 190, 255), width=2)
    crack_col = {"plain": (235, 240, 255), "party": CYAN, "enemy": (255, 110, 100)}[variant]
    origin = {"plain": (CX, CY), "party": (CX + 20, CY - 30), "enemy": (0, CY)}[variant]
    fz = freeze(f)
    if variant == "enemy":
        fz = Image.blend(ImageChops.offset(fz, 14, -6), Image.new("RGB", fz.size, (120, 20, 30)), 0.18)
    f1 = rays(glow(fz, *origin, 140, (255, 255, 255) if variant != "enemy" else (255, 120, 110)), *origin, 26, 260,
              (255, 255, 255), 2)
    f2 = cracks(glow(fz, *origin, 90, (220, 220, 255)), 420 if variant != "enemy" else 700, crack_col, origin=origin)
    f3 = shatter(fz, vd, 0.12, crack_col)
    f4 = spinner(shatter(fz, beam, 0.55, crack_col, 0.35))
    f5 = shatter(fz, st, 1.25, crack_col, 0.4)
    return [f1, f2, f3, f4, f5]


# ---------- B: prism flash
def prism(im, x, y, r):
    d = ImageDraw.Draw(im)
    tri = [(x, y - r), (x - r * 0.9, y + r * 0.6), (x + r * 0.9, y + r * 0.6)]
    d.line((0, y + 10, x - r * 0.45, y - 0), fill=(255, 255, 255), width=4)
    for i in range(7):
        a0 = -0.20 + i * 0.07
        col = spectral(i / 7.5, 0.85)
        p0 = (x + r * 0.45, y)
        d.polygon([p0, (W, y + math.tan(a0) * 700), (W, y + math.tan(a0 + 0.07) * 700)], fill=col)
    d.polygon(tri, fill=(30, 34, 54), outline=(230, 236, 255))
    d.line((tri[0], (x - r * 0.1, y + r * 0.6)), fill=(120, 130, 170), width=1)
    return im


def squeeze(src, h, back):
    im = back.copy()
    band = src.resize((W, max(2, h)))
    y = CY - 80 - h // 2
    im.paste(band, (0, y))
    d = ImageDraw.Draw(im)
    for i in range(7):
        col = spectral(i / 7.5, 0.8)
        d.line((0, y - 3 - i * 3, W, y - 3 - i * 3), fill=col, width=2)
        d.line((0, y + h + 3 + i * 3, W, y + h + 3 + i * 3), fill=spectral((6 - i) / 7.5, 0.8), width=2)
    return im


def split(src, off):
    r, g, b = src.split()
    return Image.merge("RGB", (ImageChops.offset(r, -off, 0), g, ImageChops.offset(b, off, 0)))


B = [("1_flash", "0.00–0.06 s", "A white flash bursts from the touch point; the picture goes almost white.",
      "A bright glass 'ping'.", "Load starts."),
     ("2_split", "0.06–0.20 s", "The picture splits into its colours: red one way, blue the other, light streaks across.",
      "A rising whoosh, pitch climbing.", "Loading."),
     ("3_fold", "0.20–0.35 s", "The world folds flat into a thin band of light with rainbow edges.",
      "The whoosh snaps shut.", "Loading."),
     ("4_beam", "0.35–0.75 s (hold)", "Dark screen: one white beam passes through a prism and fans into the spectrum.",
      "A held crystal hum that can loop.", "The load hides here: hold until the stage is ready; spinner only past 2 s."),
     ("5_open", "0.75–1.00 s", "The beam opens up and down like an eye: the battle stage is inside it.",
      "Battle music downbeat.", "Done.")]


def option_b():
    f = field()
    st = stage()
    dark = Image.new("RGB", (W, H), (6, 6, 10))
    f1 = rays(glow(Image.blend(f, Image.new("RGB", f.size, (255, 255, 255)), 0.55), CX, CY, 220, (255, 255, 255)),
              CX, CY, 36, 420, (255, 255, 255), 3)
    f2 = split(Image.blend(f, Image.new("RGB", f.size, (255, 255, 255)), 0.15), 22)
    d = ImageDraw.Draw(f2)
    for i in range(9):
        y = 120 + i * 60
        d.line((0, y, W, y + 6), fill=spectral(i / 9, 0.6), width=1)
    f3 = squeeze(split(f, 10), 70, dark)
    f4 = spinner(glow(prism(dark.copy(), 520, CY - 80, 90), 520, CY - 80, 160, (140, 140, 200), 0.6))
    f5 = dark.copy()
    h = 380
    f5.paste(st.crop((0, CY - 80 - h // 2, W, CY - 80 + h // 2)), (0, CY - 80 - h // 2))
    d = ImageDraw.Draw(f5)
    for i in range(5):
        d.line((0, CY - 80 - h // 2 - i * 3, W, CY - 80 - h // 2 - i * 3), fill=spectral(i / 7.5, 0.8), width=2)
        d.line((0, CY - 80 + h // 2 + i * 3, W, CY - 80 + h // 2 + i * 3), fill=spectral((6 - i) / 7.5, 0.8), width=2)
    return [f1, f2, f3, f4, f5]


# ---------- C: facet turn
HR = 74  # hex radius


def hex_centres():
    out = []
    dx, dy = HR * 1.5, HR * math.sqrt(3)
    for c in range(-1, int(W / dx) + 2):
        for r in range(-1, int(H / dy) + 2):
            out.append((c * dx, r * dy + (dy / 2 if c % 2 else 0)))
    return out


HEXES = hex_centres()


def hex_pts(x, y, r, sx=1.0):
    return [(x + r * sx * math.cos(math.pi / 3 * k), y + r * math.sin(math.pi / 3 * k)) for k in range(6)]


def facet(im, src, x, y, sx=1.0, refract=0.18, tint=None, edge=(230, 236, 255)):
    """One hex of src, sampled pulled toward the clash point (the refraction), squashed to sx for a flip."""
    sxp = int(x + (CX - x) * refract)
    syp = int(y + (CY - y) * refract)
    box = (sxp - HR, syp - HR, sxp + HR, syp + HR)
    piece = src.crop(box)
    if tint:
        piece = Image.blend(piece, Image.new("RGB", piece.size, tint), 0.22)
    w = max(2, int(2 * HR * abs(sx)))
    piece = piece.resize((w, 2 * HR))
    mask = Image.new("L", piece.size, 0)
    ImageDraw.Draw(mask).polygon(hex_pts(w / 2, HR, HR - 1, abs(sx)), fill=255)
    im.paste(piece, (int(x - w / 2), int(y - HR)), mask)
    ImageDraw.Draw(im).polygon(hex_pts(x, y, HR - 1, abs(sx)), outline=edge)


def facets(src, back, reach, refract=0.18, flip=None, other=None):
    im = back.copy()
    for x, y in HEXES:
        dist = math.hypot(x - CX, y - CY)
        if dist > reach:
            continue
        tint = spectral(math.atan2(y - CY, x - CX) / (2 * math.pi), 0.5)
        if flip is None:
            facet(im, src, x, y, 1.0, refract, tint)
            continue
        phase = max(0.0, min(1.0, (flip - dist) / 260))  # the flip wave runs out from the clash point
        a = phase * math.pi
        if phase >= 1.0:
            facet(im, other, x, y, 1.0, 0.0, None, (120, 100, 190))
        elif a < math.pi / 2:
            facet(im, src, x, y, math.cos(a), refract, tint)
        else:
            facet(im, other, x, y, -math.cos(a), 0.0, None, (255, 255, 255))
    return im


C = [("1_crystal", "0.00–0.08 s", "A small crystal hex lights up at the touch point; the frame freezes.",
      "A crystal 'tink'.", "Load starts."),
     ("2_spread", "0.08–0.25 s", "Facets grow out from the touch point; each one bends the world toward the clash.",
      "Ice-like crackle spreading.", "Loading."),
     ("3_facets", "0.25–0.70 s (hold)", "The whole screen is crystal facets, each tinted; they shimmer slowly in place.",
      "A held shimmer that can loop.", "The load hides here: hold until the stage is ready; spinner only past 2 s."),
     ("4_flip", "0.70–0.90 s", "The facets flip over in a wave from the touch point; their backs show the battle stage.",
      "Quick ticking flips, rising.", "Done: stage loaded before the flip starts."),
     ("5_stage", "0.90–1.00 s", "The last facet edges fade off the stage; the HUD slides in.",
      "Battle music downbeat.", "Done.")]


def option_c():
    f = freeze(field(), 0.25)
    st = stage()
    f1 = glow(f, CX, CY, 120, (210, 220, 255))
    ImageDraw.Draw(f1).polygon(hex_pts(CX, CY, HR - 1), outline=(240, 244, 255))
    f1 = rays(f1, CX, CY, 6, 140, (240, 244, 255), 2)
    f2 = facets(f, f, 330)
    f3 = spinner(facets(f, f, 5000, 0.22))
    f4 = facets(f, f, 5000, 0.22, flip=520, other=st)
    f5 = st.copy()
    ov = Image.new("RGB", (W, H), (0, 0, 0))
    od = ImageDraw.Draw(ov)
    for x, y in HEXES:
        if math.hypot(x - CX, y - CY) > 620:
            od.polygon(hex_pts(x, y, HR - 1), outline=(70, 60, 110))
    f5 = ImageChops.add(f5, ov)
    return [f1, f2, f3, f4, f5]


# ---------- output
def write(frames, prefix, rows, name):
    for im, (n, when, *_rest) in zip(frames, rows):
        save(tag(im.copy(), "%s · %s · %s" % (prefix, n.split("_", 1)[1], when)), OUT, "%s_%s.png" % (prefix, n))
    tw, th = 480, 270
    sheet = Image.new("RGB", (W, 90 + len(rows) * (th + 20)), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (24, 24), "%s  ·  %s  ·  about 1 second" % (prefix, name), 26, True)
    for i, (im, (n, when, see, sound, load)) in enumerate(zip(frames, rows)):
        y = 80 + i * (th + 20)
        sheet.paste(im.resize((tw, th)), (24, y))
        x = 528
        text(d, (x, y + 4), "%d · %s" % (i + 1, when), 20, True, CYAN)
        wrap(d, (x, y + 40), see, 700, 17)
        wrap(d, (x, y + 130), "Sound: " + sound, 700, 15, DIM)
        wrap(d, (x, y + 190), "Loading: " + load, 700, 15, GOLD if "hides" in load else DIM)
    sheet.convert("P", palette=Image.ADAPTIVE, colors=256).save(os.path.join(OUT, "%s_sheet.png" % prefix), optimize=True)


def variants():
    """Option A three ways: the crack frame (top) and the arrival (bottom)."""
    tw, th = 400, 225
    sheet = Image.new("RGB", (W, 640), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (24, 20), "Who struck first (option A)", 24, True)
    cols = [("party", "Player first strike", "Violet cracks from the leader's blade", "FIRST STRIKE", CYAN),
            ("plain", "Plain clash", "White cracks from the touch point", "", TXT),
            ("enemy", "Surprise attack", "Red cracks from behind the player, the frame jolts", "AMBUSH", KILL)]
    for k, (v, head, line, banner, col) in enumerate(cols):
        fr = option_a(v)
        x = 24 + k * (tw + 16)
        text(d, (x, 64), head, 18, True, col)
        sheet.paste(fr[1].resize((tw, th)), (x, 96))
        arrive = fr[4].resize((tw, th))
        if banner:
            ad = ImageDraw.Draw(arrive)
            ad.rectangle((0, 18, tw, 54), fill=(10, 10, 16))
            text(ad, (tw // 2, 24), banner, 22, True, col, anchor="ma")
        sheet.paste(arrive, (x, 96 + th + 12))
        wrap(d, (x, 96 + 2 * th + 24), line, tw, 14, DIM)
    save(sheet, OUT, "strike_variants.png")


# ---------- D: Crown's pick (D-212): A's shatter kept in the world (Expedition 33: never a black card), B's prism
#            inside it; thick lit bevelled shards, bloom, motion blur, depth of field, a small camera push.
LIGHT = (-0.6, -0.8)  # the key light comes from the top left


def push(im, k):
    """The camera pushes in toward the clash point by k (1.0 = none)."""
    w, h = W / k, H / k
    x0, y0 = CX - CX / k, CY - CY / k
    return im.crop((int(x0), int(y0), int(x0 + w), int(y0 + h))).resize((W, H), Image.LANCZOS)


def bloom(im, thr=205, r=14, k=0.9):
    mask = im.convert("L").point(lambda v: 255 if v > thr else 0)
    bright = Image.composite(im, Image.new("RGB", im.size), mask).filter(ImageFilter.GaussianBlur(r))
    return ImageChops.add(im, bright.point(lambda v: int(v * k)))


def shade(col, k):
    return tuple(max(0, min(255, int(c * k))) for c in col)


def shard3d(layer, src, tri, off, rot, s, hue, tint, thick=1.0):
    """One shard as a thick glass slab: side faces along the depth, the captured frame on the face (tinted with the
    colour it caught), a bevel lit from the top left, a glint on the lit edge. Drawn onto the RGBA layer."""
    cx = sum(p[0] for p in tri) / 3
    cy = sum(p[1] for p in tri) / 3
    tx, ty = cx + off[0], cy + off[1]
    co, si = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    pts = [(tx + s * (co * (x - cx) - si * (y - cy)), ty + s * (si * (x - cx) + co * (y - cy))) for x, y in tri]
    depth = (6 * s * thick, 10 * s * thick)
    d = ImageDraw.Draw(layer)
    glass = shade(hue, 0.55)
    for i in range(3):  # side faces, lit by how they face the light
        a, b = pts[i], pts[(i + 1) % 3]
        nx, ny = b[1] - a[1], a[0] - b[0]
        ln = math.hypot(nx, ny) or 1
        mx, my = (a[0] + b[0]) / 2 - tx, (a[1] + b[1]) / 2 - ty
        if nx * mx + ny * my < 0:
            nx, ny = -nx, -ny
        lit = (nx * LIGHT[0] + ny * LIGHT[1]) / ln
        d.polygon([a, b, (b[0] + depth[0], b[1] + depth[1]), (a[0] + depth[0], a[1] + depth[1])],
                  fill=shade(glass, 0.55 + 0.45 * max(0, -lit)) + (255,))
    # the face: the captured frame, mapped through the same move
    inv_a, inv_b = co / s, si / s
    face = src.transform(src.size, Image.AFFINE, (inv_a, inv_b, cx - (co * tx + si * ty) / s,
                                                  -inv_b, inv_a, cy - (-si * tx + co * ty) / s), Image.BILINEAR)
    face = Image.blend(face, Image.new("RGB", face.size, hue), tint)
    mask = Image.new("L", layer.size, 0)
    ImageDraw.Draw(mask).polygon(pts, fill=255)
    layer.paste(face.convert("RGBA"), (0, 0), mask)
    for i in range(3):  # the bevel: bright on edges facing the light, dark on the others
        a, b = pts[i], pts[(i + 1) % 3]
        nx, ny = b[1] - a[1], a[0] - b[0]
        mx, my = (a[0] + b[0]) / 2 - tx, (a[1] + b[1]) / 2 - ty
        if nx * mx + ny * my < 0:
            nx, ny = -nx, -ny
        lit = nx * LIGHT[0] + ny * LIGHT[1] > 0
        d.line((a, b), fill=(shade(hue, 1.5) if lit else shade(hue, 0.35)) + (255,), width=max(2, int(3 * s)))
        if lit:
            d.line((a, b), fill=(255, 255, 255, 255), width=1)
    inner = [(tx + (x - tx) * 0.86, ty + (y - ty) * 0.86) for x, y in pts]
    d.polygon(inner, outline=shade(hue, 1.3) + (140,))


def shard_field(src, t, spread=1.0, tint=0.3, near_from=None):
    """All shards at time t: pushed out from the clash point and toward the camera (bigger). Returns (far, near)
    layers so the near ones can be blurred (depth of field)."""
    far = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    near = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    rnd = random.Random(11)
    order = []
    for tri in TRIS:
        cx = sum(p[0] for p in tri) / 3
        cy = sum(p[1] for p in tri) / 3
        dx, dy = cx - CX, cy - CY
        dist = math.hypot(dx, dy) or 1
        z = rnd.random()
        sp = (0.5 + z) * spread
        off = (dx / dist * t * 420 * sp, dy / dist * t * 300 * sp - t * 60)
        s = 1 + t * (0.3 + 1.4 * z)
        rot = (rnd.random() - 0.5) * 90 * t
        hue = spectral(math.atan2(dy, dx) / (2 * math.pi) + 0.08, 0.75)
        order.append((s, tri, off, rot, hue))
    for s, tri, off, rot, hue in sorted(order, key=lambda o: o[0]):
        target = near if (near_from and s > near_from) else far
        shard3d(target, src, tri, off, rot, s, hue, tint, thick=1.0 + 0.6 * t)
    return far, near


def zoom_blur(layer, amount=0.08, steps=6):
    """Motion blur for shards flying past the camera: the layer smeared outward from the clash point."""
    acc = layer
    for k in range(1, steps + 1):
        z = push(layer, 1 - amount * k / steps)
        acc = Image.blend(acc, z, 1 / (k + 1))
    return acc


def light_cracks(im, reach, origin=(CX, CY)):
    """The hit's light running through the cracks: white at the hit, splitting into the spectrum further out."""
    lay = Image.new("RGB", im.size)
    d = ImageDraw.Draw(lay)
    for tri in TRIS:
        for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
            da = math.hypot(a[0] - origin[0], a[1] - origin[1])
            db = math.hypot(b[0] - origin[0], b[1] - origin[1])
            if max(da, db) > reach:
                continue
            mx, my = (a[0] + b[0]) / 2 - origin[0], (a[1] + b[1]) / 2 - origin[1]
            k = min(1, math.hypot(mx, my) / 260)
            col = spectral(math.atan2(my, mx) / (2 * math.pi), 0.85 * k)
            d.line((a, b), fill=col, width=3)
    return ImageChops.add(ImageChops.add(im, lay.filter(ImageFilter.GaussianBlur(5))), lay)


D = [("1_hit", "0.00–0.08 s", "The hit flashes white in the world; the camera starts a small push in; the first cracks.",
      "A sharp crystal 'tink' over a low hit; the field music ducks.", "Load starts (the battle map, read in the background)."),
     ("2_light", "0.08–0.25 s", "The light runs through the cracks, white at the hit, splitting into the spectrum further out.",
      "Glass splintering with a rising shimmer.", "Loading."),
     ("3_break", "0.25–0.45 s", "The frame breaks into thick lit shards, each catching a colour; the world behind stays, "
      "out of focus.", "The shatter: a bright crash with a choir swell.", "Loading."),
     ("4_hold", "0.45–0.80 s (hold)", "Coloured shards drift slowly toward the camera, the near ones soft (depth of "
      "field), the world dim behind them. Holds as long as the load needs.",
      "A shimmer loop.", "The load hides here: the world behind is the captured frame, drawn above the level; spinner only past 2 s."),
     ("5_battle", "0.80–1.00 s", "The shards blow past the camera with motion blur onto the battle stage; bloom on the "
      "last edges; the HUD slides in.", "Battle music downbeat.", "Done: stage loaded.")]


def option_d():
    f0 = freeze(field(), 0.12)
    st = stage()
    f1 = push(f0, 1.02)
    f1 = rays(glow(f1, CX, CY, 170, (255, 255, 255)), CX, CY, 20, 230, (255, 255, 255), 2)
    f1 = bloom(cracks(f1, 150, (255, 255, 255), fringe=False))
    cap = push(f0, 1.04)  # the frame the shards carry
    far, _ = shard_field(cap, 0.0, tint=0.12)
    f2 = cap.copy()
    f2.paste(far, (0, 0), far)
    f2 = bloom(light_cracks(glow(f2, CX, CY, 110, (255, 255, 255), 0.8), 900), 200, 16, 1.0)
    back = Image.blend(push(f0, 1.06), Image.new("RGB", f0.size, (10, 8, 20)), 0.3).filter(ImageFilter.GaussianBlur(7))
    far, _ = shard_field(cap, 0.12, tint=0.2)
    f3 = back.copy()
    f3.paste(far, (0, 0), far)
    f3 = bloom(light_cracks(f3, 260), 200, 12, 0.8)
    far, near = shard_field(cap, 0.42, spread=1.25, tint=0.24, near_from=1.45)
    f4 = Image.blend(push(f0, 1.08), Image.new("RGB", f0.size, (10, 8, 20)), 0.45).filter(ImageFilter.GaussianBlur(9))
    f4.paste(far, (0, 0), far)
    near = near.filter(ImageFilter.GaussianBlur(4))
    f4.paste(near, (0, 0), near)
    f4 = spinner(bloom(f4, 210, 12, 0.7))
    far, near = shard_field(cap, 1.15, spread=1.4, tint=0.4)
    f5 = st.copy()
    lay = zoom_blur(Image.alpha_composite(far, near), 0.10)
    f5.paste(lay, (0, 0), lay)
    f5 = bloom(f5, 200, 14, 0.8)
    return [f1, f2, f3, f4, f5]


write(option_d(), "D", D, "Picked: shatter in the world + prism")
write(option_a(), "A", A, "Refraction shatter")
write(option_b(), "B", B, "Prism flash")
write(option_c(), "C", C, "Facet turn")
variants()
print("ok")
