"""B-111: combat HUD mockup v2 (Crown's v1 review, 2026-10-26).

Same fight and layout as v1 (combat_hud_v1.py), with:
  - the four header-bar candidates plus a crystal-themed one, side by side (D-017, UI_DeckbuilderReuse.md section 3)
  - a harder crystal look: faceted crystal-tube bars, crystal-cut panel corners, element gems (D-024)
  - the stunned chip shows its duration: "Stunned · 1 turn" (the row said 2 from D-030; D-090 later cut every
    status break to 1 of the holder's turns, STATUS_CAP_TURNS)
  - Broken Darkness on the party card: no pips or gold bar; the EP value shows the absorption stacks as a
    multiplier, "108 (x3)", and the overload sits on the EP bar itself (absorbed element's colour up to full, dark
    intensifying past full, up to 1.30 x MaxEP; BrokenDarkness.md Energy). Sera is BD, so she reads Darkness.
  - damage numbers coloured on a gradient by the target's resistance to the hit (no Weak/Resist labels)
  - status bars crack as they fill, shatter on the break, stay broken during it
  - Reality badge indigo, Void violet (the element gem strip on status_bars; Reality drawn, the kit has none)

Drafts: header_candidates, command, hit, stunned, status_bars.
    python Tools/ui_drafts/combat_hud_v2.py
Writes docs/Design/screen_mockups/combat/hud/v2/.
"""
import pathlib
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import kit  # noqa: E402
import combat_hud_v1 as v1  # noqa: E402  (v1's board, enemies, diamond and turn strip are reused as they are)

SCREEN, VERSION = "combat/hud", "v2"

# v1's pieces read these module tables, so v2's colour and cast changes are applied there.
v1.ELEMENT.update({
    "reality": (78, 70, 200),     # indigo (ElementColors.h Reality), lifted to read on dark stone
    "void": (160, 60, 255),       # violet (ElementColors.h Void)
    "darkness": (118, 104, 160),  # Darkness is near-black in game; lifted so its glow and fills read in a draft
})
SERA = next(p for p in v1.PARTY if p["name"] == "Sera")
SERA.update({"role": "Refractor · Darkness (BD)", "element": "darkness", "ep": (108, 90), "bd": 3})
BD_ABSORBED = "fire"              # the element Sera has absorbed (its colour fills her EP bar)
BD_OVERLOAD_CAP = 1.30            # BrokenDarknessManager: EP may reach 1.30 x MaxEP
STUN_TURNS = 1                    # D-090: a status break lasts 1 of the holder's turns (STATUS_CAP_TURNS)

GEM = {"fire": "element_fire", "earth": "element_earth", "darkness": "element_darkness", "void": "element_void",
       "water": "element_water", "wind": "element_wind", "lightning": "element_lightning", "light": "element_light"}
ELEMENT_ORDER = ["fire", "lightning", "earth", "wind", "water", "void", "reality", "light", "darkness"]   # the nine
CRACK_LIGHT = (235, 245, 255)
SHARD_COUNT = 6

# Resistance gradient for damage numbers: (resistance to this hit, colour). Below 0 = weak, 1 = immune.
DAMAGE_GRADIENT = [(-0.5, (255, 176, 56)), (0.0, (248, 244, 236)), (0.5, (138, 172, 214)), (1.0, (104, 106, 118))]


# --- crystal pieces -------------------------------------------------------------------------------------------

def tube(x, y, w, h):
    """A crystal tube: a long hexagon with pointed ends."""
    k = h / 2
    return [(x + k, y), (x + w - k, y), (x + w, y + k), (x + w - k, y + h), (x + k, y + h), (x, y + k)]


def lighter(colour, amount):
    return tuple(max(0, min(255, c + amount)) for c in colour)


def crystal_bar(canvas, x, y, w, h, frac, colour, ghost=0.0, cracks=0.0, broken=False, overload=0.0, seed=0):
    """Faceted crystal-tube bar: dark trough, steel rim, fill with a light upper facet and darker lower facet.
    cracks 0-1: crack lines grow with the fill; broken: the tube lies in shards (status break in progress);
    overload 0-1: the fill darkens and intensifies past full (Broken Darkness EP above max)."""
    if broken:
        shards(canvas, x, y, w, h, colour, seed=seed)
        return
    d = ImageDraw.Draw(canvas)
    d.polygon(tube(x, y, w, h), fill=v1.TROUGH + (235,))
    layer = Image.new("RGBA", canvas.size)
    ld = ImageDraw.Draw(layer)
    if ghost:
        ld.polygon(tube(x + 1, y + 1, w - 2, h - 2), fill=v1.HP_GHOST + (210,))
        clip(layer, x + 1 + round((w - 2) * min(1.0, frac + ghost)))
        canvas.alpha_composite(layer)
        layer = Image.new("RGBA", canvas.size)
        ld = ImageDraw.Draw(layer)
    ld.polygon(tube(x + 1, y + 1, w - 2, h - 2), fill=colour + (255,))
    ld.polygon([(x + h / 2, y + 1), (x + w - h / 2, y + 1), (x + w - 2, y + h / 2), (x + 2, y + h / 2)],
               fill=lighter(colour, 55) + (255,))                                                     # upper facet
    if overload:
        dark = Image.new("RGBA", canvas.size)
        ImageDraw.Draw(dark).polygon(tube(x + 1, y + 1, w - 2, h - 2), fill=(18, 6, 26, int(50 + 110 * overload)))
        facets = round(3 + 6 * overload)
        for i in range(facets):                                                                       # dark facets
            fx = x + 6 + i * (w - 12) / facets
            ImageDraw.Draw(dark).line((fx, y + 1, fx + h * 0.8, y + h - 1), fill=(60, 20, 70, 200), width=1)
        layer.alpha_composite(dark)
    clip(layer, x + 1 + round((w - 2) * frac))
    canvas.alpha_composite(layer)
    if cracks:
        crack_lines(canvas, x, y, round((w - 2) * frac), h, cracks, seed)
    d.polygon(tube(x, y, w, h), outline=v1.RIM + (255,))
    if frac > 0.04:
        tip = x + 1 + round((w - 2) * frac)
        d.line((tip, y + 1, tip, y + h - 1), fill=lighter(colour, 110) + (230,), width=1)            # fill edge glint


def clip(layer, right):
    """Erase everything at or right of x = right (a bar's fill length)."""
    ImageDraw.Draw(layer).rectangle((right, 0, layer.width, layer.height), fill=(0, 0, 0, 0))


def crack_lines(canvas, x, y, w, h, amount, seed):
    """Jagged pale cracks across the filled part; more and longer as the bar fills."""
    rng = random.Random(seed)
    d = ImageDraw.Draw(canvas)
    for _ in range(int(1 + amount * 7)):
        cx = x + rng.uniform(0.1, 0.95) * w
        pts, px, py = [], cx, y + 1
        while py < y + h - 1:
            pts.append((px, py))
            px += rng.uniform(-3, 3)
            py += rng.uniform(1.5, 3)
        pts.append((px, y + h - 1))
        d.line(pts, fill=CRACK_LIGHT + (int(120 + 120 * amount),), width=1)


def shards(canvas, x, y, w, h, colour, seed=0, flying=False):
    """The tube in pieces: shards with gaps, dimmed (status broken, during the break); flying = the moment it shatters."""
    rng = random.Random(seed)
    step = w / SHARD_COUNT
    layer = Image.new("RGBA", canvas.size)
    ld = ImageDraw.Draw(layer)
    for i in range(SHARD_COUNT):
        sx = x + i * step + 2
        spread = max(4, h)   # shards fly about a bar's height
        dx, dy = (rng.uniform(-spread, spread * 1.4), rng.uniform(-spread * 1.4, spread)) if flying else (rng.uniform(-1, 1), rng.uniform(-1.5, 1.5))
        poly = [(sx + dx, y + dy + rng.uniform(0, 2)), (sx + step - 3 + dx, y + dy), (sx + step - 5 + dx, y + h + dy),
                (sx + 1 + dx, y + h + dy - rng.uniform(0, 2))]
        tone = colour if flying else tuple(int(c * 0.45) for c in colour)
        ld.polygon(poly, fill=tone + (255 if flying else 200,), outline=lighter(tone, 80) + (220,))
    if flying:
        kit.glow(canvas, (x + w / 2, y + h / 2), w * 0.35, colour, strength=0.35, squash=0.4)
        for _ in range(16):
            px, py = x + rng.uniform(0, w), y + rng.uniform(-h * 1.5, h * 2.5)
            ld.ellipse((px - 1.2, py - 1.2, px + 1.2, py + 1.2), fill=CRACK_LIGHT + (230,))
    canvas.alpha_composite(layer)


def crystal_corners(canvas, x, y, w, h, colour=kit.CRYSTAL, size=7):
    """Small cut-crystal studs on a panel's corners, with a faint glow."""
    for cx, cy in ((x + 9, y + 9), (x + w - 9, y + 9), (x + 9, y + h - 9), (x + w - 9, y + h - 9)):
        kit.glow(canvas, (cx, cy), size * 2.2, colour, strength=0.28)
        d = ImageDraw.Draw(canvas)
        d.polygon([(cx, cy - size), (cx + size * 0.7, cy), (cx, cy + size), (cx - size * 0.7, cy)], fill=colour + (235,),
                  outline=(250, 252, 255, 255))
        d.line((cx, cy - size, cx, cy + size), fill=lighter(colour, -60) + (160,), width=1)


def crystal_panel(canvas, x, y, w, h):
    canvas.alpha_composite(v1.panel(w, h), (x, y))
    d = ImageDraw.Draw(canvas)
    d.line((x + 20, y + 5, x + w - 20, y + 5), fill=kit.CRYSTAL + (90,), width=1)          # crystal light on the top edge
    crystal_corners(canvas, x, y, w, h)


def element_gem(canvas, centre, element, size=20):
    """The element's kit gem; Reality has none in the kit, so it is drawn as an indigo crystal."""
    if element == "reality":
        drawn_gem(canvas, centre, v1.ELEMENT["reality"], size)
        return
    if element not in GEM:
        return
    gem = kit.scaled(kit.piece("badges", GEM[element]), height=size)
    canvas.alpha_composite(gem, (round(centre[0] - gem.width / 2), round(centre[1] - gem.height / 2)))


def drawn_gem(canvas, centre, colour, size):
    cx, cy = centre
    r = size / 2
    kit.glow(canvas, centre, size * 0.9, colour, strength=0.35)
    d = ImageDraw.Draw(canvas)
    d.polygon(v1.hexagon(cx, cy, r), fill=colour + (255,), outline=(235, 238, 250, 255))
    d.polygon([(cx, cy - r), (cx + r * 0.866, cy - r / 2), (cx, cy), (cx - r * 0.866, cy - r / 2)], fill=lighter(colour, 60) + (255,))
    d.polygon([(cx, cy), (cx + r * 0.866, cy + r / 2), (cx, cy + r), (cx - r * 0.866, cy + r / 2)], fill=lighter(colour, -30) + (255,))


# --- header-bar candidates -------------------------------------------------------------------------------------

HEADER_W, HEADER_H = 460, 60
HEADERS = [
    ("A", "text_frame_silver", "PONETI text frame, silver (the library's header_bar)"),
    ("B", "text_frame_gold", "PONETI text frame, gold"),
    ("C", "Main_big_bar_horizontal", "PONETI big bar, nine-sliced to a strip"),
    ("D", "Main_big_bar_transparent", "PONETI big bar, ~45% see-through"),
    ("E", None, "Crystal-themed (drawn, no kit piece): faceted glass, crystal studs"),
]


def header_plate(source, w=HEADER_W, h=HEADER_H):
    if source in ("text_frame_silver", "text_frame_gold"):
        return kit.five_slice(kit.scaled(kit.vendor_piece(source), height=h), w, end=0.08, centre=0.02)
    if source:
        art = kit.scaled(kit.vendor_piece(source), width=480)
        return kit.nine_slice(art, w, h, 22)
    plate = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(plate)
    d.polygon(tube(2, 4, w - 4, h - 8), fill=(20, 26, 40, 225))
    d.polygon([(h / 2, 6), (w - h / 2, 6), (w - 10, h / 2), (10, h / 2)], fill=(60, 80, 110, 90))      # glass top facet
    d.polygon(tube(2, 4, w - 4, h - 8), outline=(200, 230, 250, 255))
    d.polygon(tube(7, 9, w - 14, h - 18), outline=kit.CRYSTAL + (110,))
    for cx in (h / 2 - 6, w - h / 2 + 6):
        d.polygon([(cx, 4), (cx + 9, h / 2), (cx, h - 4), (cx - 9, h / 2)], fill=kit.CRYSTAL + (240,), outline=(255, 255, 255, 255))
    out = Image.new("RGBA", (w, h))
    out.alpha_composite(plate.filter(ImageFilter.GaussianBlur(4)))
    out.alpha_composite(plate)
    return out


def turn_banner(canvas, text, source=None, y=14):
    """Top-centre turn banner, on header candidate E (crystal) unless another is given."""
    plate = header_plate(source)
    canvas.alpha_composite(plate, ((kit.W - plate.width) // 2, y))
    kit.text(canvas, (kit.W // 2, y + plate.height // 2), text, kit.font(kit.DISPLAY_BOLD, 20), tracking=1)


# --- party card -------------------------------------------------------------------------------------------------

def party_card(canvas, x, y, member, acting=False, stunned=False):
    """v2 card: crystal panel, element gem, crystal bars; BD shows stacks on the EP value and overload on the EP bar."""
    w, h = 200, 150
    if acting:
        y -= 12
        kit.glow(canvas, (x + w // 2, y + h // 2), 110, kit.CRYSTAL, strength=0.22, squash=0.7)
    crystal_panel(canvas, x, y, w, h)
    canvas.alpha_composite(v1.portrait(member["card"], 58, v1.ELEMENT[member["element"]]), (x + 12, y + 12))
    kit.text(canvas, (x + 80, y + 26), member["name"], kit.font(kit.DISPLAY_BOLD, 18), anchor="lm", shadow=False)
    element_gem(canvas, (x + w - 22, y + 26), member["element"], 20)
    kit.text(canvas, (x + 80, y + 46), member["role"], kit.font(kit.UI_FONT, 11), fill=kit.DIM, anchor="lm", shadow=False)
    hp, hp_max = member["hp"]
    ep, ep_max = member["ep"]
    small = kit.font(kit.UI_BOLD, 11)
    kit.text(canvas, (x + 14, y + 84), "HP", small, fill=kit.DIM, anchor="lm", shadow=False)
    crystal_bar(canvas, x + 36, y + 79, 104, 10, hp / hp_max, v1.HP_FILL)
    kit.text(canvas, (x + w - 12, y + 84), f"{hp}", small, anchor="rm", shadow=False)
    kit.text(canvas, (x + 14, y + 101), "EP", small, fill=kit.DIM, anchor="lm", shadow=False)
    if "bd" in member:
        over = max(0.0, (ep / ep_max - 1.0) / (BD_OVERLOAD_CAP - 1.0))
        crystal_bar(canvas, x + 36, y + 96, 104, 10, min(1.0, ep / ep_max), v1.ELEMENT[BD_ABSORBED], overload=over)
        kit.text(canvas, (x + w - 12, y + 101), f"{ep} (x{member['bd']})", small, fill=(255, 214, 170), anchor="rm",
                 shadow=False)
    else:
        crystal_bar(canvas, x + 36, y + 96, 104, 10, ep / ep_max, v1.EP_FILL)
        kit.text(canvas, (x + w - 12, y + 101), f"{ep}", small, anchor="rm", shadow=False)
    if member["buildup"]:
        element, frac = member["buildup"]
        crystal_bar(canvas, x + 36, y + 111, 104, 5, frac, v1.ELEMENT[element], cracks=frac, seed=len(member["name"]))
    icons = list(member["effects"]) + ([("effect_extra_turn_skip_turn", "debuff")] if stunned else [])
    for i, (icon, kind) in enumerate(icons):
        canvas.alpha_composite(v1.effect_icon(icon, kind, 20), (x + 14 + i * 24, y + 121))
    if stunned:
        stun_chip(canvas, x + 14 + len(icons) * 24 + 2, y + 122)
    return w, h


def stun_label():
    return f"Stunned · {STUN_TURNS} turn{'' if STUN_TURNS == 1 else 's'}"


def stun_chip(canvas, x, y):
    label = stun_label()
    f = kit.font(kit.UI_BOLD, 11)
    tw = ImageDraw.Draw(canvas).textlength(label, font=f)
    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle((x, y, x + tw + 14, y + 18), radius=9, fill=(70, 18, 20, 230), outline=v1.DEBUFF_RED + (255,))
    kit.text(canvas, (x + 7, y + 9), label, f, fill=(255, 200, 196), anchor="lm", shadow=False)


def party_row(canvas, acting=0, stunned=False):
    x0, y0 = 636, 560
    for i, member in enumerate(v1.PARTY):
        party_card(canvas, x0 + i * 210, y0, member, acting=(i == acting), stunned=stunned and i == acting)


# --- enemy plate ------------------------------------------------------------------------------------------------

def enemy_plate(canvas, e, ghost=0.0, hp=None, buildup=("fire", 0.55), broken=False, shattering=False,
                statuses=(("effect_damage_up_down", "debuff"),)):
    cx, cy = e["centre"]
    w = 260
    x, y = cx - w // 2, int(cy - 110 * e["scale"]) - 60
    plate = kit.five_slice(kit.piece("frames", "nameplate"), w)
    canvas.alpha_composite(kit.scaled(plate, width=w), (x, y))
    crystal_corners(canvas, x - 4, y - 2, w + 8, kit.scaled(plate, width=w).height + 4, size=5)
    kit.text(canvas, (x + 22, y + 17), e["name"], kit.font(kit.DISPLAY_BOLD, 17), anchor="lm", shadow=False)
    cur, top = hp or e["hp"]
    kit.text(canvas, (x + w - 22, y + 17), f"{cur}/{top}", kit.font(kit.UI_BOLD, 13), fill=kit.DIM, anchor="rm",
             shadow=False)
    crystal_bar(canvas, x + 22, y + 30, w - 44, 9, cur / top, v1.HP_FILL, ghost=ghost)
    if buildup:
        element, frac = buildup
        if shattering:
            shards(canvas, x + 22, y + 42, w - 44, 6, v1.ELEMENT[element], seed=7, flying=True)
        else:
            crystal_bar(canvas, x + 22, y + 42, w - 44, 6, frac, v1.ELEMENT[element], cracks=frac, broken=broken, seed=7)
    for i, (icon, kind) in enumerate(statuses):
        canvas.alpha_composite(v1.effect_icon(icon, kind, 20), (x + 22 + i * 24, y + 54))
    return x, y, w


# --- damage numbers ---------------------------------------------------------------------------------------------

def damage_colour(resistance):
    """The number's colour for the target's resistance to this hit (gradient, DAMAGE_GRADIENT)."""
    stops = DAMAGE_GRADIENT
    if resistance <= stops[0][0]:
        return stops[0][1]
    for (r0, c0), (r1, c1) in zip(stops, stops[1:]):
        if resistance <= r1:
            t = (resistance - r0) / (r1 - r0)
            return tuple(round(a + (b - a) * t) for a, b in zip(c0, c1))
    return stops[-1][1]


def gradient_key(canvas, x, y, w=220, h=10):
    """Mockup key only (not in game): where a number's colour sits on the resistance gradient."""
    d = ImageDraw.Draw(canvas)
    lo, hi = DAMAGE_GRADIENT[0][0], DAMAGE_GRADIENT[-1][0]
    for i in range(w):
        d.line((x + i, y, x + i, y + h), fill=damage_colour(lo + (hi - lo) * i / (w - 1)) + (255,))
    d.rectangle((x, y, x + w, y + h), outline=v1.RIM + (255,))
    small = kit.font(kit.UI_FONT, 11)
    kit.text(canvas, (x, y + h + 10), "weak", small, fill=kit.DIM, anchor="lm", shadow=False)
    kit.text(canvas, (x + w * 0.33, y + h + 10), "neutral", small, fill=kit.DIM, anchor="mm", shadow=False)
    kit.text(canvas, (x + w, y + h + 10), "immune", small, fill=kit.DIM, anchor="rm", shadow=False)
    kit.text(canvas, (x, y - 10), "MOCKUP KEY · number colour = target's resistance", kit.font(kit.UI_BOLD, 10),
             fill=kit.DIM, anchor="lm", shadow=False)


# --- the drafts -------------------------------------------------------------------------------------------------

def draw_header_candidates():
    canvas = kit.background("dfhb_battle", blur=3, dark=0.55)
    kit.vignette(canvas, 0.55)
    y = 70
    notes = []
    for i, (letter, source, why) in enumerate(HEADERS):
        turn_banner(canvas, "TURN 3 · GARRICK", source=source, y=y)
        kit.text(canvas, ((kit.W - HEADER_W) // 2 - 30, y + HEADER_H // 2), letter, kit.font(kit.DISPLAY_BOLD, 26),
                 fill=kit.CRYSTAL)
        notes.append((i + 1, ((kit.W + HEADER_W) // 2 + 26, y + HEADER_H // 2), f"Candidate {letter}",
                      [why, "Same size, text and place as the in-game turn banner"]))
        y += HEADER_H + 34
    return canvas, notes


def draw_command():
    canvas = v1.board(active=0)
    turn_banner(canvas, "TURN 3 · GARRICK")
    enemy_plate(canvas, v1.ENEMIES[v1.TARGET])
    v1.command_diamond(canvas)
    party_row(canvas, acting=0)
    v1.hint_bar(canvas, [("Y", "Attack"), ("X", "Skills"), ("B", "Items"), ("A", "Defend")])
    notes = [
        (1, (880, 40), "Turn banner", ["Shown on candidate E (crystal); see header_candidates for A-E", "Pick one (D-017)"]),
        (2, (575, 140), "Enemy plate", ["Crystal-tube HP + buildup bars; crystal studs on the plate",
                                        "Buildup bar already cracking at 55% (see status_bars)"]),
        (3, (650, 548), "Party cards", ["Crystal-cut corners, crystal light on the top edge",
                                        "Element gem next to the name (none for Generic)", "Faceted HP / EP tubes"]),
        (4, (1056, 650), "Sera: Broken Darkness", ["Reads Darkness (BD), not Void", "EP '108 (x3)': EP, then absorption stacks",
                                                   "EP bar full in the absorbed element (Fire);",
                                                   "past full it darkens and intensifies, up to 1.30 x MaxEP"]),
        (5, (1056, 296), "Command diamond", ["Unchanged from v1"]),
        (6, (118, 106), "Turn order", ["Unchanged from v1; Sera's glow now Darkness"]),
    ]
    return canvas, notes


def draw_hit():
    canvas = v1.board(active=0)
    turn_banner(canvas, "TURN 3 · GARRICK")
    hits = [(0, 31, -0.4), (1, 54, 0.0), (2, 12, 0.7)]   # (enemy, damage, resistance to this hit): a sweep, three targets
    enemy_plate(canvas, v1.ENEMIES[v1.TARGET], ghost=0.09, hp=(v1.TARGET_HP_BEFORE - 54, 600), buildup=("fire", 1.0),
                shattering=True, statuses=(("effect_damage_up_down", "debuff"), ("effect_damage_over_time_fire", "debuff")))
    for index, dmg, resistance in hits:
        cx, cy = v1.ENEMIES[index]["centre"]
        v1.outlined(canvas, (cx + 40, cy - 46), str(dmg), kit.font(kit.UI_BOLD, 44 if index == v1.TARGET else 34),
                    damage_colour(resistance))
    band = kit.scaled(kit.piece("misc", "banner_event"), width=520)
    by = 452
    canvas.alpha_composite(band, ((kit.W - band.width) // 2 + 40, by))
    v1.outlined(canvas, (kit.W // 2 + 40, by + band.height // 2 - 2), "BURN!", kit.font(kit.DISPLAY_BOLD, 40),
                (255, 210, 120), outline=(60, 10, 6), stroke=3)
    gradient_key(canvas, 150, 640)
    party_row(canvas, acting=0)
    cx, cy = v1.ENEMIES[v1.TARGET]["centre"]
    (wx, wy), (rx, ry) = v1.ENEMIES[0]["centre"], v1.ENEMIES[2]["centre"]
    notes = [
        (1, (wx + 80, wy - 80), "Weak target", ["Warm gold number: this hound is weak to the hit", "No 'Weak' label (rules page 02)"]),
        (2, (cx + 90, cy - 80), "Neutral target", ["Near-white number"]),
        (3, (rx + 80, ry - 80), "Resisting target", ["Cool steel-blue, dimmer: resists the hit; grey = immune"]),
        (4, (cx - 150, cy - 140), "Buildup bar shatters", ["The full Fire buildup tube bursts into shards on the break",
                                                           "Then lies broken while Burn lasts (status_bars)"]),
        (5, (kit.W // 2 - 230, by + 20), "Proc banner", ["As v1: 'BURN!' once, short, then gone"]),
        (6, (128, 640), "Mockup key", ["Not in game: shows the gradient the colours come from"]),
    ]
    return canvas, notes


def draw_stunned():
    canvas = v1.board(active=0)
    turn_banner(canvas, "TURN 3 · GARRICK")
    enemy_plate(canvas, v1.ENEMIES[v1.TARGET])
    v1.command_diamond(canvas, stunned=True)
    party_row(canvas, acting=0, stunned=True)
    v1.hint_bar(canvas, [("Y", "Attack"), ("A", "Defend")])
    notes = [
        (1, (650, 536), "Stun chip", [f"'{stun_label()}' (D-090: a status break lasts 1 holder turn)",
                                      "Red pill next to the skip-turn icon; counts down on the holder's turn"]),
        (2, (1056, 296), "Stunned diamond", ["As v1: Attack (Y) and Defend (A) only"]),
    ]
    return canvas, notes


def draw_status_bars():
    canvas = kit.background("dfhb_battle", blur=3, dark=0.6)
    kit.text(canvas, (kit.W // 2, 40), "STATUS BARS & BROKEN DARKNESS EP", kit.font(kit.DISPLAY_BOLD, 24), tracking=2)
    rows = [("Filling 20%", 0.2, {}), ("Filling 50%: first cracks", 0.5, {}), ("Filling 85%: cracked through", 0.85, {}),
            ("Break: it shatters", 1.0, {"shatter": True}), ("During the break: stays broken", 1.0, {"broken": True})]
    x, w, h = 360, 420, 14
    y = 100
    notes = []
    for i, (label, frac, how) in enumerate(rows):
        kit.text(canvas, (x - 20, y + h // 2), label, kit.font(kit.UI_BOLD, 14), anchor="rm", shadow=False)
        if how.get("shatter"):
            shards(canvas, x, y, w, h, v1.ELEMENT["fire"], seed=3, flying=True)
        else:
            crystal_bar(canvas, x, y, w, h, frac, v1.ELEMENT["fire"], cracks=frac if frac >= 0.5 else 0.0,
                        broken=how.get("broken", False), seed=3)
        y += 52
    notes.append((1, (x + w + 30, 100 + h // 2), "Status buildup tube", ["Clean below half; cracks spread as it fills",
                                                                         "Cracks: pale lines, more and longer near full"]))
    notes.append((2, (x + w + 30, 100 + 3 * 52 + h // 2), "Break", ["Bursts into shards with a short glow (one loud effect)",
                                                                    "Then the shards stay, dimmed, while the status lasts"]))
    y += 30
    max_ep = SERA["ep"][1]
    kit.text(canvas, (kit.W // 2, y), f"Broken Darkness EP (Sera, absorbed Fire, MaxEP {max_ep})", kit.font(kit.UI_BOLD, 15),
             shadow=False)
    y += 30
    for ep, stacks in ((60, 1), (90, 1), (99, 2), (108, 3), (117, 3)):
        over = max(0.0, (ep / max_ep - 1.0) / (BD_OVERLOAD_CAP - 1.0))
        crystal_bar(canvas, x, y, w, h, min(1.0, ep / max_ep), v1.ELEMENT[BD_ABSORBED], overload=over)
        kit.text(canvas, (x - 20, y + h // 2), f"{ep} (x{stacks})", kit.font(kit.UI_BOLD, 14), fill=(255, 214, 170),
                 anchor="rm", shadow=False)
        kit.text(canvas, (x + w + 20, y + h // 2), "cap 1.30 x MaxEP" if ep == 117 else ("full" if ep == max_ep else ""),
                 kit.font(kit.UI_FONT, 12), fill=kit.DIM, anchor="lm", shadow=False)
        y += 36
    notes.append((3, (x - 150, y - 5 * 36 + h // 2), "EP value", ["EP, then the absorption stacks as '(xN)', max 3",
                                                                   "No separate pips or gold bar (Crown, 2026-10-26)"]))
    notes.append((4, (x + w + 30, y - 2 * 36 + h // 2), "Overload on the bar", ["Full in the absorbed element's colour",
                                                                               "Past full: darker, denser facets, up to the 1.30 cap"]))
    gx = kit.W // 2 - (len(ELEMENT_ORDER) - 1) * 60 // 2
    for i, element in enumerate(ELEMENT_ORDER):
        element_gem(canvas, (gx + i * 60, 660), element, 30)
        kit.text(canvas, (gx + i * 60, 690), element.capitalize(), kit.font(kit.UI_FONT, 11), fill=kit.DIM, shadow=False)
    notes.append((5, (gx - 50, 660), "Element gems", ["The nine elements as they mark cards and skills",
                                                       "Reality: indigo crystal (drawn; no kit piece). Void: violet"]))
    return canvas, notes


if __name__ == "__main__":
    for name, draw in (("header_candidates", draw_header_candidates), ("command", draw_command), ("hit", draw_hit),
                       ("stunned", draw_stunned), ("status_bars", draw_status_bars)):
        plain, notes = draw()
        kit.save_draft(plain, notes, SCREEN, VERSION, name)
