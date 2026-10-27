"""Shared pieces for the flat 1280x720 screen drafts (D-025, B-100).

Ported from wor-deckbuilder tools/ui_drafts/kit.py + kit2.py (helpers only, no screens). Art is read from the
licensed UI kit library outside this repo, at the path in the WOR_UI_KIT environment variable; the kit itself is never
copied in. Drafts are written to docs/Design/screen_mockups/<area>/<screen>/vN/ as <name>_plain.png and
<name>_annotated.png (LFS) so Crown can review them from the phone.
"""
import functools
import io
import os
import pathlib
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(os.environ.get("UI_DRAFTS_OUT") or REPO / "docs/Design/screen_mockups")   # UI_DRAFTS_OUT: draw elsewhere
WINDOWS_FONTS = pathlib.Path(r"C:\Windows\Fonts")
ALLOW_RESTORE = os.environ.get("UI_DRAFTS_RESTORE") == "1"   # redraw an existing version on purpose


def _kit_root():
    raw = os.environ.get("WOR_UI_KIT")
    if not raw:
        sys.exit("WOR_UI_KIT is not set. Point it at the UI kit library, e.g.\n"
                 "  C:\\Users\\<you>\\Documents\\github\\wor-deckbuilder\\wor.game\\assets\\ui\\library\n"
                 "(PowerShell: [Environment]::SetEnvironmentVariable('WOR_UI_KIT', '<path>', 'User'), then reopen the shell).")
    root = pathlib.Path(raw).resolve()
    if not (root / "frames").is_dir():
        sys.exit(f"WOR_UI_KIT='{raw}' doesn't look like the UI kit library (no frames/ folder in it).")
    return root


KIT = _kit_root()
# The game's fonts (deckbuilder Crown, 2026-10-13): Spectral SC for titles/display, Lato for everything else (SIL OFL).
# They sit next to the library in the deckbuilder (assets/fonts); Windows fonts stand in if they aren't there.
GAME_FONT_DIR = KIT.parents[1] / "fonts"

W, H = 1280, 720
STEEL, STEEL_L = (140, 144, 152), 142
INK = (240, 234, 222)          # near-white, warm
DIM = (160, 165, 178)
PLATINUM = (214, 222, 236)
CRYSTAL = (205, 238, 255)      # crystal light: pale cyan-white
PURPLE, GREEN, RED, ORANGE, SLATE = (170, 100, 240), (90, 200, 110), (225, 60, 60), (255, 150, 40), (70, 72, 90)
SIZE_TITLE, SIZE_UI, SIZE_SMALL = 34, 18, 13   # three text sizes per screen (deckbuilder Art Direction section 3)

# --- fonts ---------------------------------------------------------------------------------------------------

DISPLAY, DISPLAY_BOLD, UI_FONT, UI_BOLD = "display", "display_bold", "ui", "ui_bold"
_GAME_FONT = {DISPLAY: "SpectralSC/SpectralSC-Medium.ttf", DISPLAY_BOLD: "SpectralSC/SpectralSC-SemiBold.ttf",
              UI_FONT: "Lato/Lato-Regular.ttf", UI_BOLD: "Lato/Lato-Bold.ttf"}
_WINDOWS_FONT = {DISPLAY: "constan.ttf", DISPLAY_BOLD: "constanb.ttf", UI_FONT: "segoeui.ttf", UI_BOLD: "seguisb.ttf"}
SYMBOLS = "✓⚠✎▾◆★"   # not in Lato / Segoe UI: drawn with Segoe UI Symbol


@functools.lru_cache(None)
def _font_path(role):
    game = GAME_FONT_DIR / _GAME_FONT[role]
    if game.exists():
        return game
    print(f"Note: game font {game} not found; drawing '{role}' text with Windows {_WINDOWS_FONT[role]} instead.")
    return WINDOWS_FONTS / _WINDOWS_FONT[role]


@functools.lru_cache(None)
def font(role, size):
    return ImageFont.truetype(str(_font_path(role)), size)


def symbol_safe(f, s):
    return ImageFont.truetype(str(WINDOWS_FONTS / "seguisym.ttf"), f.size) if any(c in SYMBOLS for c in s) else f


# --- kit access ----------------------------------------------------------------------------------------------

@functools.lru_cache(None)
def _cached_piece(kind, name):
    path = KIT / kind / f"{name}.png"
    if not path.exists():
        sys.exit(f"Kit piece missing: {kind}/{name}.png (under WOR_UI_KIT={KIT})")
    return Image.open(path).convert("RGBA")


def piece(kind, name):
    """A kit library piece (kind = backgrounds, badges, buttons, frames, icons, misc, ...), as a fresh copy the caller
    may draw on. Fails loudly if missing."""
    return _cached_piece(kind, name).copy()


# Vendor originals the library doesn't hold (e.g. the four header-bar candidates, UI_Asset_Picks #2) are read from the
# deckbuilder's Godot import cache: a .ctex there embeds the original as lossless WebP. Read-only, nothing is copied.
GODOT_IMPORTED = KIT.parents[3] / ".godot" / "imported"
VENDOR_MAX_SIDE = 2048   # the originals are up to 6144 px; drafts never need more


@functools.lru_cache(None)
def _cached_vendor_piece(name):
    matches = sorted(GODOT_IMPORTED.glob(f"{name}.png-*.ctex"))
    if not matches:
        sys.exit(f"Vendor piece missing: {name}.png has no import cache under {GODOT_IMPORTED}")
    data = matches[0].read_bytes()
    riff = data.find(b"RIFF")
    if riff < 0:
        sys.exit(f"Vendor piece {matches[0].name} holds no lossless WebP (re-import it lossless in the deckbuilder)")
    size = int.from_bytes(data[riff + 4:riff + 8], "little")
    img = Image.open(io.BytesIO(data[riff:riff + 8 + size])).convert("RGBA")
    img = img.crop(img.getbbox())
    k = VENDOR_MAX_SIDE / max(img.size)
    return img.resize((round(img.width * k), round(img.height * k)), Image.LANCZOS) if k < 1 else img


def vendor_piece(name):
    """A vendor original by file name (no extension), decoded from the deckbuilder's import cache. Fresh copy."""
    return _cached_vendor_piece(name).copy()


def scaled(img, width=None, height=None):
    k = width / img.width if width else height / img.height
    return img.resize((max(1, round(img.width * k)), max(1, round(img.height * k))), Image.LANCZOS)


def tint(img, colour, mid=128):
    a = np.asarray(img, dtype=np.float32)
    lum = a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114
    out = np.empty_like(a)
    for ch, c in enumerate(colour):
        out[..., ch] = np.where(lum <= mid, c * lum / mid, c + (255 - c) * (lum - mid) / (255 - mid))
    out[..., 3] = a[..., 3]
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA")


def steel(img):
    """Frames to steel; a panel's dark fill keeps its colour (pixels weighted by lightness)."""
    a = np.asarray(img, dtype=np.float32)
    lum = a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114
    rim = (lum > 90) & (a[..., 3] > 200)
    k = STEEL_L / (lum[rim].mean() if rim.any() else STEEL_L)
    w = np.clip((lum - 40) / 50, 0, 1)
    nl = lum * (1 + (k - 1) * w)
    out = a.copy()
    for ch, c in enumerate(STEEL):
        out[..., ch] = a[..., ch] * (1 - w) + nl * c / STEEL_L * w
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA")


def nine_slice(img, width, height, border):
    """Stretch a framed panel to any size: corners (border px) stay sharp, edges and centre stretch."""
    w0, h0 = img.size
    b = min(border, width // 2, height // 2, w0 // 2, h0 // 2)   # corners must not overlap, in the art or the target
    out = Image.new("RGBA", (width, height))
    xs, ys = (0, b, w0 - b, w0), (0, b, h0 - b, h0)
    tx, ty = (0, b, width - b, width), (0, b, height - b, height)
    for i in range(3):
        for j in range(3):
            part = img.crop((xs[i], ys[j], xs[i + 1], ys[j + 1]))
            size = (max(1, tx[i + 1] - tx[i]), max(1, ty[j + 1] - ty[j]))
            out.alpha_composite(part.resize(size, Image.LANCZOS), (tx[i], ty[j]))
    return out


def five_slice(img, width, end=0.2, centre=0.06):
    """Stretch a symmetric piece to a width without distorting its ends or its centre stud."""
    w0, h = img.size
    if width <= w0:
        return img.resize((width, h), Image.LANCZOS)
    e, c = round(w0 * end), round(w0 * centre)
    mid0, mid1 = w0 // 2 - c, w0 // 2 + c
    left_run = round((width - 2 * e - (mid1 - mid0)) / 2)
    right_run = width - 2 * e - (mid1 - mid0) - left_run
    out = Image.new("RGBA", (width, h))
    out.alpha_composite(img.crop((0, 0, e, h)), (0, 0))
    out.alpha_composite(img.crop((e, 0, mid0, h)).resize((max(1, left_run), h), Image.LANCZOS), (e, 0))
    out.alpha_composite(img.crop((mid0, 0, mid1, h)), (e + left_run, 0))
    out.alpha_composite(img.crop((mid1, 0, w0 - e, h)).resize((max(1, right_run), h), Image.LANCZOS), (e + left_run + mid1 - mid0, 0))
    out.alpha_composite(img.crop((w0 - e, 0, w0, h)), (width - e, 0))
    return out


# --- canvas pieces -------------------------------------------------------------------------------------------

def background(name="dfhb_battle", blur=2.2, dark=0.5):
    """A full-screen kit background, blurred and darkened toward stone blue-grey (deckbuilder defaults; 0 = off)."""
    img = piece("backgrounds", name)
    k = max(W / img.width, H / img.height)
    img = img.resize((round(img.width * k), round(img.height * k)), Image.LANCZOS)
    img = img.crop(((img.width - W) // 2, (img.height - H) // 2, (img.width - W) // 2 + W, (img.height - H) // 2 + H))
    if blur:
        img = img.filter(ImageFilter.GaussianBlur(blur))
    return Image.blend(img, Image.new("RGBA", (W, H), (14, 16, 24, 255)), dark) if dark else img


def vignette(canvas, strength=0.7):
    y, x = np.mgrid[0:H, 0:W]
    d = np.sqrt(((x - W / 2) / (W / 2)) ** 2 + ((y - H / 2) / (H / 2)) ** 2)
    alpha = np.clip((d - 0.55) / 0.75, 0, 1) ** 1.6 * strength * 255
    canvas.alpha_composite(Image.fromarray(np.dstack([np.zeros((H, W, 3)), alpha]).astype(np.uint8), "RGBA"))


def glow(canvas, centre, radius, colour=CRYSTAL, strength=0.4, squash=1.0):
    """A soft light pool (bloom stand-in)."""
    layer = Image.new("RGBA", (W, H))
    cx, cy = centre
    ImageDraw.Draw(layer).ellipse((cx - radius, cy - radius * squash, cx + radius, cy + radius * squash),
                                  fill=colour + (int(255 * strength),))
    canvas.alpha_composite(layer.filter(ImageFilter.GaussianBlur(radius * 0.45)))


def motes(canvas, count, seed, area, colour=CRYSTAL):
    """Crystal motes: small pale points with a halo (seeded, so a draft rebuilds pixel-identical)."""
    rng = random.Random(seed)
    halo, core = Image.new("RGBA", (W, H)), Image.new("RGBA", (W, H))
    dh, dc = ImageDraw.Draw(halo), ImageDraw.Draw(core)
    x0, y0, x1, y1 = area
    for _ in range(count):
        x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
        r, a = rng.uniform(1.0, 2.6), rng.uniform(0.35, 0.95)
        dh.ellipse((x - r * 4, y - r * 4, x + r * 4, y + r * 4), fill=colour + (int(70 * a),))
        dc.ellipse((x - r, y - r, x + r, y + r), fill=colour + (int(255 * a),))
    canvas.alpha_composite(halo.filter(ImageFilter.GaussianBlur(5)))
    canvas.alpha_composite(core.filter(ImageFilter.GaussianBlur(0.6)))


def text(canvas, xy, s, f, fill=INK, anchor="mm", tracking=0, shadow=True, alpha=255):
    """Text with optional letter-spacing and a soft drop shadow (light from the top left)."""
    f = symbol_safe(f, s)
    layer = Image.new("RGBA", (W, H))
    d = ImageDraw.Draw(layer)
    if tracking:
        widths = [d.textlength(ch, font=f) for ch in s]
        total = sum(widths) + tracking * (len(s) - 1)
        x = xy[0] - total / 2 if anchor[0] == "m" else (xy[0] - total if anchor[0] == "r" else xy[0])
        for ch, w in zip(s, widths):
            d.text((x, xy[1]), ch, font=f, fill=fill + (alpha,), anchor="l" + anchor[1])
            x += w + tracking
    else:
        d.text(xy, s, font=f, fill=fill + (alpha,), anchor=anchor)
    if shadow:
        a = np.asarray(layer)[..., 3]
        sh = Image.fromarray(np.dstack([np.zeros((H, W, 3)), a * 0.7]).astype(np.uint8), "RGBA")
        canvas.alpha_composite(sh.transform((W, H), Image.AFFINE, (1, 0, -2, 0, 1, -3)).filter(ImageFilter.GaussianBlur(3)))
    canvas.alpha_composite(layer)


# --- output --------------------------------------------------------------------------------------------------

_OPENED_THIS_RUN = set()   # a script may write several drafts into the version it just created


def out_dir(screen, version):
    """docs/Design/screen_mockups/<area>/<screen>/<version>/. An existing version is delivered and never overwritten
    (deckbuilder rule, Crown 2026-10-08): draw a new vN, or set UI_DRAFTS_RESTORE=1 to redraw it on purpose."""
    path = OUT / screen / version
    if path not in _OPENED_THIS_RUN and path.exists() and any(path.iterdir()) and not ALLOW_RESTORE:
        sys.exit(f"{screen}/{version} already exists and is frozen: write a new version (or UI_DRAFTS_RESTORE=1 to redraw it).")
    path.mkdir(parents=True, exist_ok=True)
    _OPENED_THIS_RUN.add(path)
    return path


LEGEND_TITLE = "Annotations: layout, hierarchy, kit piece, motion / VFX"


def annotate(plain, notes):
    """The plain draft with numbered callouts and a two-column legend under it. notes: [(n, (x, y), title, lines)]."""
    head, body = font(UI_BOLD, 15), font(UI_FONT, 13)
    entry_h = lambda lines: 24 + 17 * len(lines) + 10  # noqa: E731
    half = (len(notes) + 1) // 2
    columns = [notes[:half], notes[half:]]
    legend_h = 56 + max(sum(entry_h(n[3]) for n in col) for col in columns)
    out = Image.new("RGBA", (W, H + legend_h), (22, 24, 30, 255))
    out.alpha_composite(plain, (0, 0))
    d = ImageDraw.Draw(out)
    for n, (x, y), _, _ in notes:
        d.ellipse((x - 15, y - 15, x + 15, y + 15), fill=(255, 196, 0, 255), outline=(20, 20, 20, 255), width=2)
        d.text((x, y), str(n), font=font(UI_BOLD, 16), fill=(20, 20, 20, 255), anchor="mm")
    d.text((24, H + 16), LEGEND_TITLE, font=font(UI_BOLD, 18), fill=INK + (255,))
    for c, col in enumerate(columns):
        x0, y = 24 + c * (W // 2), H + 52
        for n, _, title, lines in col:
            d.ellipse((x0, y, x0 + 22, y + 22), fill=(255, 196, 0, 255))
            d.text((x0 + 11, y + 11), str(n), font=font(UI_BOLD, 12), fill=(20, 20, 20, 255), anchor="mm")
            d.text((x0 + 32, y + 2), title, font=symbol_safe(head, title), fill=INK + (255,))
            y += 24
            for line in lines:
                d.text((x0 + 32, y), line, font=symbol_safe(body, line), fill=DIM + (255,))
                y += 17
            y += 10
    return out


def save_draft(plain, notes, screen, version, name):
    """Write <name>_plain.png and <name>_annotated.png into the screen's version folder; returns that folder."""
    plain_rgb, annotated_rgb = plain.convert("RGB"), annotate(plain, notes).convert("RGB")   # render both before writing
    folder = out_dir(screen, version)
    plain_rgb.save(folder / f"{name}_plain.png")
    annotated_rgb.save(folder / f"{name}_annotated.png")
    print(f"Wrote {folder.relative_to(REPO) if folder.is_relative_to(REPO) else folder}/{name}_plain.png + _annotated.png")
    return folder
