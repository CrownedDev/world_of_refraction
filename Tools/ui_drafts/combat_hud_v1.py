"""B-101: combat HUD mockup v1 (screens 31-40, Expedition 33 layout, UI_DeckbuilderReuse.md sections 1-2).

Four states of one fight, drawn on a shared board:
  command  - a player's turn, main command diamond open
  target   - Attack picked: target picker with infusion controls
  hit      - the attack lands: damage number, BURN! proc, HP ghost
  stunned  - a stunned actor's diamond: Attack + Defend only (D-001, D-023 A, B-092)

    python Tools/ui_drafts/combat_hud_v1.py
Writes docs/Design/screen_mockups/combat/hud/v1/.
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import kit  # noqa: E402

SCREEN, VERSION = "combat/hud", "v1"

# Element colours from Source/.../Infusion/ElementColors.h (0-1 floats -> 8-bit), softened for UI fills.
ELEMENT = {"fire": (235, 70, 50), "lightning": (255, 150, 40), "earth": (230, 200, 60), "wind": (90, 200, 110),
           "water": (60, 140, 255), "void": (165, 80, 255), "reality": (110, 60, 170), "light": (240, 240, 245),
           "darkness": (40, 40, 48), "generic": (170, 125, 80)}
HP_FILL, HP_GHOST, EP_FILL = (205, 60, 70), (250, 220, 200), (90, 170, 255)
TROUGH, RIM = (16, 18, 26), (120, 124, 134)
PANEL_BORDER = 28            # panel_raised corner size for nine-slice
WHITE_ART_TINT_MID = 250     # kit.tint midpoint for white/pale art, so the tint colour survives instead of washing out

# One fight: three party members, three enemies (Garrick is acting).
PARTY = [
    {"name": "Garrick", "role": "Generic", "card": "garrick", "element": "generic", "hp": (84, 100), "ep": (30, 60),
     "buildup": ("fire", 0.35), "effects": [("effect_shield", "buff"), ("effect_damage_up_down", "buff")]},
    {"name": "Sera", "role": "Refractor · Void", "card": "common_void_refractor", "element": "void", "hp": (61, 80),
     "ep": (72, 90), "buildup": ("water", 0.6), "effects": [("effect_defense_up_down", "debuff")], "bd": (3, 0.82)},
    {"name": "Bram", "role": "Resonator", "card": "common_resonator", "element": "earth", "hp": (97, 110),
     "ep": (25, 50), "buildup": None, "effects": []},
]
ENEMIES = [
    {"name": "Ashen Hound", "centre": (500, 360), "scale": 0.8, "hp": (40, 60)},
    {"name": "Cinder Warden", "centre": (705, 320), "scale": 1.25, "hp": (412, 600)},
    {"name": "Ashen Hound", "centre": (895, 372), "scale": 0.8, "hp": (60, 60)},
]
TARGET = 1   # the Warden
# Turn order (top = acting now): (who, kind, badge) - kind party/enemy, badge None/bonus/ritual.
TURN_ORDER = [("Garrick", "party", None), ("Cinder Warden", "enemy", None), ("Sera", "party", "bonus"),
              ("Ashen Hound", "enemy", None), ("Bram", "party", None), ("Cinder Warden", "enemy", "ritual"),
              ("Garrick", "party", None)]


# --- small pieces ---------------------------------------------------------------------------------------------

def hexagon(cx, cy, r):
    return [(cx, cy - r), (cx + r * 0.866, cy - r / 2), (cx + r * 0.866, cy + r / 2),
            (cx, cy + r), (cx - r * 0.866, cy + r / 2), (cx - r * 0.866, cy - r / 2)]


def portrait(card, size, glow_colour, lit=True):
    """Gem-cut (hexagon) portrait from the card art's face, steel rim, inner glow in the character's element."""
    art = kit.piece("cards", card)
    face = art.crop((18, 26, 254, 262)).resize((size, size), Image.LANCZOS)
    mask = Image.new("L", (size, size))
    r = size / 2
    ImageDraw.Draw(mask).polygon(hexagon(r, r, r - 3), fill=255)
    out = Image.new("RGBA", (size, size))
    out.paste(face, (0, 0), mask)
    glow_layer = Image.new("RGBA", (size, size))
    ImageDraw.Draw(glow_layer).polygon(hexagon(r, r, r - 4), outline=glow_colour + (230,), width=max(3, size // 12))
    out.alpha_composite(glow_layer.filter(ImageFilter.GaussianBlur(size / 18)))
    d = ImageDraw.Draw(out)
    d.polygon(hexagon(r, r, r - 2), outline=(205, 210, 220, 255) if lit else (110, 114, 124, 255), width=2)
    if not lit:
        out = Image.blend(Image.new("RGBA", out.size), out, 0.55)
    return out


def enemy_token(size, lit=True):
    """Enemy portrait stand-in: a dark horned silhouette in a hexagon (no enemy art in the kit)."""
    out = Image.new("RGBA", (size, size))
    r = size / 2
    d = ImageDraw.Draw(out)
    d.polygon(hexagon(r, r, r - 3), fill=(40, 22, 22, 255))
    d.ellipse((r - size * 0.2, r - size * 0.14, r + size * 0.2, r + size * 0.3), fill=(12, 10, 12, 255))
    d.polygon([(r - size * 0.18, r - size * 0.05), (r - size * 0.3, r - size * 0.34), (r - size * 0.08, r - size * 0.12)],
              fill=(12, 10, 12, 255))
    d.polygon([(r + size * 0.18, r - size * 0.05), (r + size * 0.3, r - size * 0.34), (r + size * 0.08, r - size * 0.12)],
              fill=(12, 10, 12, 255))
    d.ellipse((r - size * 0.1, r + size * 0.02, r - size * 0.04, r + size * 0.07), fill=(255, 120, 60, 255))
    d.ellipse((r + size * 0.04, r + size * 0.02, r + size * 0.1, r + size * 0.07), fill=(255, 120, 60, 255))
    d.polygon(hexagon(r, r, r - 2), outline=(205, 210, 220, 255) if lit else (110, 114, 124, 255), width=2)
    if not lit:
        out = Image.blend(Image.new("RGBA", out.size), out, 0.55)
    return out


def bar(canvas, x, y, w, h, frac, colour, ghost=0.0):
    """A thin crystal-tube bar: dark trough, steel rim, fill with a lighter top line; optional damage ghost."""
    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle((x, y, x + w, y + h), radius=h // 2, fill=TROUGH + (235,), outline=RIM + (255,), width=1)
    fw = max(0, round((w - 2) * frac))
    if ghost:
        gw = round((w - 2) * min(1.0, frac + ghost))
        d.rounded_rectangle((x + 1, y + 1, x + 1 + gw, y + h - 1), radius=(h - 2) // 2, fill=HP_GHOST + (210,))
    if fw:
        d.rounded_rectangle((x + 1, y + 1, x + 1 + fw, y + h - 1), radius=(h - 2) // 2, fill=colour + (255,))
        light = tuple(min(255, c + 70) for c in colour)
        d.line((x + 3, y + 2, x + fw - 2, y + 2), fill=light + (200,), width=1)


def glyph(canvas, centre, letter, colour):
    """A gamepad face-button glyph (Xbox layout) - stand-in until the input glyph set is picked."""
    x, y = centre
    d = ImageDraw.Draw(canvas)
    d.ellipse((x - 11, y - 11, x + 11, y + 11), fill=(20, 20, 26, 255), outline=colour + (255,), width=2)
    d.text((x, y + 1), letter, font=kit.font(kit.UI_BOLD, 13), fill=colour + (255,), anchor="mm")


GLYPH_COLOUR = {"A": (110, 200, 90), "B": (230, 80, 70), "X": (80, 150, 240), "Y": (240, 200, 60)}


def effect_icon(name, kind, size=22):
    colour = (110, 210, 120) if kind == "buff" else (235, 90, 80)
    return kit.tint(kit.scaled(kit.piece("icons", name), height=size), colour, mid=WHITE_ART_TINT_MID)


def panel(w, h):
    return kit.nine_slice(kit.piece("frames", "panel_raised"), w, h, PANEL_BORDER)


def outlined(canvas, xy, s, f, fill, outline=(10, 10, 14), stroke=3):
    d = ImageDraw.Draw(canvas)
    d.text(xy, s, font=f, fill=fill + (255,), anchor="mm", stroke_width=stroke, stroke_fill=outline + (255,))


# --- board ----------------------------------------------------------------------------------------------------

def enemy_body(canvas, e, lit=True, highlight=False):
    cx, cy = e["centre"]
    k = e["scale"]
    shadow = Image.new("RGBA", (kit.W, kit.H))
    ImageDraw.Draw(shadow).ellipse((cx - 70 * k, cy + 70 * k, cx + 70 * k, cy + 92 * k), fill=(0, 0, 0, 150))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(6)))
    if highlight:
        kit.glow(canvas, (cx, cy + 10 * k), 95 * k, kit.CRYSTAL, strength=0.35, squash=1.1)
    body = Image.new("RGBA", (kit.W, kit.H))
    d = ImageDraw.Draw(body)
    tone = (22, 16, 18, 255) if lit else (22, 16, 18, 170)
    d.ellipse((cx - 50 * k, cy - 30 * k, cx + 50 * k, cy + 80 * k), fill=tone)                 # body
    d.ellipse((cx - 28 * k, cy - 70 * k, cx + 28 * k, cy - 18 * k), fill=tone)                 # head
    d.polygon([(cx - 22 * k, cy - 55 * k), (cx - 46 * k, cy - 100 * k), (cx - 8 * k, cy - 66 * k)], fill=tone)
    d.polygon([(cx + 22 * k, cy - 55 * k), (cx + 46 * k, cy - 100 * k), (cx + 8 * k, cy - 66 * k)], fill=tone)
    for ex in (-11, 11):
        d.ellipse((cx + (ex - 4) * k, cy - 50 * k, cx + (ex + 4) * k, cy - 42 * k), fill=(255, 120, 60, 255))
    rim = body.filter(ImageFilter.GaussianBlur(3))
    canvas.alpha_composite(kit.tint(rim, (255, 90, 40)) if lit else rim)
    canvas.alpha_composite(body)


def enemy_plate(canvas, e, ghost=0.0, hp=None, buildup=("fire", 0.55), statuses=(("effect_damage_up_down", "debuff"),)):
    """Floating enemy plate above the target: name, HP (with damage ghost), buildup tinted to the pending element."""
    cx, cy = e["centre"]
    w = 260
    x, y = cx - w // 2, int(cy - 110 * e["scale"]) - 60
    plate = kit.five_slice(kit.piece("frames", "nameplate"), w)
    canvas.alpha_composite(kit.scaled(plate, width=w), (x, y))
    kit.text(canvas, (x + 22, y + 17), e["name"], kit.font(kit.DISPLAY_BOLD, 17), anchor="lm", shadow=False)
    cur, top = hp or e["hp"]
    kit.text(canvas, (x + w - 22, y + 17), f"{cur}/{top}", kit.font(kit.UI_BOLD, 13), fill=kit.DIM, anchor="rm",
             shadow=False)
    bar(canvas, x + 22, y + 31, w - 44, 7, cur / top, HP_FILL, ghost=ghost)
    if buildup:
        element, frac = buildup
        bar(canvas, x + 22, y + 41, w - 44, 4, frac, ELEMENT[element])
    for i, (icon, kind) in enumerate(statuses):
        canvas.alpha_composite(effect_icon(icon, kind, 20), (x + 22 + i * 24, y + 52))
    return x, y, w


def turn_strip(canvas, active=0):
    """Vertical turn order on the left edge (Expedition 33): acting portrait big and lit, the rest queued below."""
    x0, y = 26, 92
    kit.text(canvas, (x0 + 40, y - 18), "TURN ORDER", kit.font(kit.UI_BOLD, 11), fill=kit.DIM, tracking=2, shadow=False)
    order = TURN_ORDER[active:] + TURN_ORDER[:active]
    for i, (who, kind, badge) in enumerate(order):
        size = 80 if i == 0 else 58
        cx = x0 + 40
        member = next((p for p in PARTY if p["name"] == who), None)
        if kind == "party":
            img = portrait(member["card"], size, ELEMENT[member["element"]], lit=(i == 0))
        else:
            img = enemy_token(size, lit=(i == 0))
        if i == 0:
            kit.glow(canvas, (cx, y + size // 2), 52, kit.CRYSTAL, strength=0.3)
        canvas.alpha_composite(img, (cx - size // 2, y))
        if badge:
            b = kit.scaled(kit.piece("badges", "gem_a" if badge == "bonus" else "tier_s"), height=20)
            if badge == "bonus":
                b = kit.tint(b, (60, 210, 130), mid=WHITE_ART_TINT_MID)      # Emerald bonus turn
            canvas.alpha_composite(b, (cx + size // 2 - 16, y + size - 20))
        y += size + 10


def turn_banner(canvas, text):
    """Top-centre banner. Placeholder chrome: the header-bar pick is still open (UI_DeckbuilderReuse open item 1)."""
    w = 440
    plate = kit.scaled(kit.five_slice(kit.piece("frames", "nameplate"), w), width=w)
    canvas.alpha_composite(plate, ((kit.W - w) // 2, 14))
    kit.text(canvas, (kit.W // 2, 14 + plate.height // 2), text, kit.font(kit.DISPLAY_BOLD, 20), tracking=1)


def party_card(canvas, x, y, member, acting=False, stunned=False):
    """Compact party card (bottom-right): gem portrait, name, HP/EP, status buildup, effect icons, BD gauge."""
    w, h = 200, 150
    if acting:
        y -= 12
        kit.glow(canvas, (x + w // 2, y + h // 2), 110, kit.CRYSTAL, strength=0.22, squash=0.7)
    canvas.alpha_composite(panel(w, h), (x, y))
    canvas.alpha_composite(portrait(member["card"], 58, ELEMENT[member["element"]]), (x + 12, y + 12))
    kit.text(canvas, (x + 80, y + 26), member["name"], kit.font(kit.DISPLAY_BOLD, 18), anchor="lm", shadow=False)
    kit.text(canvas, (x + 80, y + 46), member["role"], kit.font(kit.UI_FONT, 12), fill=kit.DIM, anchor="lm", shadow=False)
    hp, hp_max = member["hp"]
    ep, ep_max = member["ep"]
    small = kit.font(kit.UI_BOLD, 11)
    kit.text(canvas, (x + 14, y + 84), "HP", small, fill=kit.DIM, anchor="lm", shadow=False)
    bar(canvas, x + 36, y + 79, 112, 9, hp / hp_max, HP_FILL)
    kit.text(canvas, (x + w - 12, y + 84), f"{hp}", small, anchor="rm", shadow=False)
    kit.text(canvas, (x + 14, y + 101), "EP", small, fill=kit.DIM, anchor="lm", shadow=False)
    bar(canvas, x + 36, y + 96, 112, 9, ep / ep_max, EP_FILL)
    kit.text(canvas, (x + w - 12, y + 101), f"{ep}", small, anchor="rm", shadow=False)
    if member["buildup"]:
        element, frac = member["buildup"]
        bar(canvas, x + 36, y + 111, 112, 4, frac, ELEMENT[element])
    icons = list(member["effects"]) + ([("effect_extra_turn_skip_turn", "debuff")] if stunned else [])
    for i, (icon, kind) in enumerate(icons):
        canvas.alpha_composite(effect_icon(icon, kind, 20), (x + 14 + i * 24, y + 121))
    if stunned:
        kit.text(canvas, (x + 14 + len(icons) * 24 + 4, y + 131), "Stunned · 1 turn", kit.font(kit.UI_BOLD, 11),
                 fill=(235, 90, 80), anchor="lm", shadow=False)
    if "bd" in member:
        stacks, overload = member["bd"]
        bx, by = x + w - 70, y + 121
        for i in range(5):                       # absorption stacks as pips
            fill = ELEMENT["void"] + (255,) if i < stacks else TROUGH + (255,)
            ImageDraw.Draw(canvas).polygon([(bx + i * 12 + 5, by), (bx + i * 12 + 10, by + 6), (bx + i * 12 + 5, by + 12),
                                            (bx + i * 12, by + 6)], fill=fill, outline=RIM + (255,))
        bar(canvas, bx, by + 16, 58, 4, overload, (255, 196, 0))
    return w, h


def party_row(canvas, acting=0, stunned=False):
    x0, y0 = 636, 560
    for i, member in enumerate(PARTY):
        party_card(canvas, x0 + i * 210, y0, member, acting=(i == acting), stunned=stunned and i == acting)


def board(active=0, highlight=None, dim_others=False):
    canvas = kit.background("dfhb_battle", blur=1.2, dark=0.35)
    floor = Image.new("RGBA", (kit.W, kit.H))
    ImageDraw.Draw(floor).ellipse((300, 380, 1180, 520), fill=(30, 34, 50, 140))
    canvas.alpha_composite(floor.filter(ImageFilter.GaussianBlur(30)))
    kit.motes(canvas, 34, seed=101, area=(0, 360, kit.W, kit.H))
    for i, e in enumerate(ENEMIES):
        enemy_body(canvas, e, lit=not (dim_others and i != highlight), highlight=(i == highlight))
    kit.vignette(canvas, 0.55)
    turn_strip(canvas, active)
    return canvas


# --- command diamond -------------------------------------------------------------------------------------------

DIAMOND_CENTRE, DIAMOND_STEP, BUTTON = (1130, 382), 76, 78


def diamond_button(canvas, slot, label, icon, colour_name, glyph_letter, enabled=True):
    dx, dy = {"top": (0, -1), "left": (-1, 0), "right": (1, 0), "bottom": (0, 1)}[slot]
    cx, cy = DIAMOND_CENTRE[0] + dx * DIAMOND_STEP, DIAMOND_CENTRE[1] + dy * DIAMOND_STEP
    if not enabled:
        d = ImageDraw.Draw(canvas)
        d.polygon(hexagon(cx, cy, BUTTON // 2 - 6), outline=(80, 84, 94, 200), width=2)
        return cx, cy
    btn = kit.scaled(kit.piece("buttons", f"{colour_name}_icon_square"), width=BUTTON)
    canvas.alpha_composite(btn, (cx - btn.width // 2, cy - btn.height // 2))
    ic = kit.scaled(kit.piece("icons", icon), height=34)
    ic = kit.tint(ic, (245, 240, 230))
    canvas.alpha_composite(ic, (cx - ic.width // 2, cy - ic.height // 2 - 6))
    kit.text(canvas, (cx, cy + 22), label, kit.font(kit.UI_BOLD, 13))
    glyph(canvas, (cx + BUTTON // 2 - 8, cy - BUTTON // 2 + 8), glyph_letter, GLYPH_COLOUR[glyph_letter])
    return cx, cy


def command_diamond(canvas, stunned=False):
    kit.glow(canvas, DIAMOND_CENTRE, 120, (40, 30, 70), strength=0.5)
    diamond_button(canvas, "top", "Attack", "effect_damage_up_down", "orange", "Y")
    diamond_button(canvas, "left", "Skills", "energy_generic", "purple", "X", enabled=not stunned)
    diamond_button(canvas, "right", "Items", "effect_heal", "purple", "B", enabled=not stunned)
    diamond_button(canvas, "bottom", "Defend", "action_guard", "purple", "A")
    kit.text(canvas, (DIAMOND_CENTRE[0], DIAMOND_CENTRE[1] + DIAMOND_STEP + 60),
             "Iron Sword IV" if not stunned else "Stunned: plain attack or Defend",
             kit.font(kit.UI_FONT, 13), fill=kit.DIM)


def hint_bar(canvas, hints):
    """Bottom-left input hints (glyph + verb)."""
    x, y = 140, 690
    for letter, verb in hints:
        glyph(canvas, (x, y), letter, GLYPH_COLOUR.get(letter, kit.DIM))
        kit.text(canvas, (x + 18, y), verb, kit.font(kit.UI_FONT, 14), anchor="lm", shadow=False)
        x += 40 + ImageDraw.Draw(canvas).textlength(verb, font=kit.font(kit.UI_FONT, 14))


# --- the four drafts ------------------------------------------------------------------------------------------

def draw_command():
    canvas = board(active=0)
    turn_banner(canvas, "TURN 3 · GARRICK")
    enemy_plate(canvas, ENEMIES[TARGET])
    command_diamond(canvas)
    party_row(canvas, acting=0)
    hint_bar(canvas, [("Y", "Attack"), ("X", "Skills"), ("B", "Items"), ("A", "Defend")])
    notes = [
        (1, (118, 106), "Turn order (32)", ["Vertical strip, left edge (Expedition 33)", "Acting: 80 px, lit + crystal glow; queue 58 px dimmed",
                                            "Emerald gem = bonus turn, S hex = ritual/execution turn", "Motion: strip slides up one slot on turn change"]),
        (2, (880, 40), "Turn banner", ["Placeholder chrome (nameplate): header-bar pick still open", "Spectral SC, 'TURN n · <actor>'"]),
        (3, (575, 140), "Enemy plate (31/34)", ["Floats above the enemy: name, HP, buildup bar", "Buildup bar tinted to the pending element",
                                                "Status icons under the bars (tinted kit icons)"]),
        (4, (1056, 296), "Command diamond (33)", ["Face-button diamond: Attack Y / Skills X / Items B / Defend A",
                                                   "Orange = primary action, purple = navigate (art direction)",
                                                   "Weapon name under it; Defend is new (B-092)"]),
        (5, (650, 548), "Party cards (31)", ["Bottom-right, three compact cards; acting card raised + glow",
                                              "Gem portrait with element inner glow, HP / EP bars", "Thin buildup bar + effect icons row"]),
        (6, (1050, 650), "BD gauge (40)", ["Broken Darkness member: absorption stacks as pips", "Gold bar = overload toward the 1.30 cap"]),
        (7, (140, 666), "Input hints", ["Glyph + verb; stand-in glyphs until the glyph set is picked"]),
    ]
    return canvas, notes


def draw_target():
    canvas = board(active=0, highlight=TARGET, dim_others=False)
    turn_banner(canvas, "TURN 3 · GARRICK")
    x, y, w = enemy_plate(canvas, ENEMIES[TARGET])
    cx, cy = ENEMIES[TARGET]["centre"]
    d = ImageDraw.Draw(canvas)
    d.polygon([(cx - 12, y - 18), (cx + 12, y - 18), (cx, y - 4)], fill=kit.CRYSTAL + (255,))    # target chevron
    for i, e in enumerate(ENEMIES):
        if i != TARGET:
            ex, ey = e["centre"]
            d.ellipse((ex - 46, ey + 52, ex + 46, ey + 66), outline=kit.CRYSTAL + (110,), width=2)
    # Target picker replaces the diamond (right side)
    px, py, pw, ph = 1000, 236, 256, 300
    canvas.alpha_composite(panel(pw, ph), (px, py))
    kit.text(canvas, (px + pw // 2, py + 30), "Attack", kit.font(kit.DISPLAY_BOLD, 20))
    kit.text(canvas, (px + pw // 2, py + 52), "Iron Sword IV · single enemy", kit.font(kit.UI_FONT, 12), fill=kit.DIM)
    for i, e in enumerate(ENEMIES):
        ry = py + 74 + i * 52
        colour = "purple" if i == TARGET else "slate"
        row = kit.scaled(kit.piece("buttons", f"{colour}_little_plain_normal"), width=pw - 36)
        canvas.alpha_composite(row, (px + 18, ry))
        kit.text(canvas, (px + 34, ry + row.height // 2), e["name"], kit.font(kit.UI_BOLD, 14), anchor="lm")
        kit.text(canvas, (px + pw - 34, ry + row.height // 2), f"{e['hp'][0]}", kit.font(kit.UI_FONT, 13), anchor="rm")
    # Infusion controls
    iy = py + ph - 66
    lvl = kit.scaled(kit.piece("buttons", "slate_little_epic_normal"), width=pw - 36)
    canvas.alpha_composite(lvl, (px + 18, iy))
    kit.text(canvas, (px + pw // 2, iy + lvl.height // 2), "Raw: L1  ·  cycle", kit.font(kit.UI_BOLD, 14))
    party_row(canvas, acting=0)
    hint_bar(canvas, [("A", "Confirm"), ("B", "Back"), ("X", "Infusion level"), ("Y", "Source")])
    notes = [
        (1, (cx, y - 30), "Target marker (34)", ["Crystal chevron over the picked enemy + soft crystal glow",
                                                  "Other valid targets: faint floor ring"]),
        (2, (px - 6, py + 20), "Target picker", ["Replaces the diamond; skill name + target rule at the top",
                                                 "Rows: purple = current pick, slate = other targets (HP on the right)",
                                                 "D-pad / stick moves the pick; it mirrors the in-world marker"]),
        (3, (px - 6, iy + 20), "Infusion (Raw: L1)", ["Cycle level / source, only for infusable actions",
                                                      "Hidden for items, immune skills and stunned actors (D-023 A)"]),
        (4, (x - 6, y + 20), "Enemy plate", ["Stays up while picking: confirms what the hit will land on"]),
        (5, (140, 666), "Input hints", ["Confirm / Back / infusion level / source"]),
    ]
    return canvas, notes


def draw_hit():
    canvas = board(active=0, highlight=None)
    turn_banner(canvas, "TURN 3 · GARRICK")
    e = ENEMIES[TARGET]
    x, y, w = enemy_plate(canvas, e, ghost=0.12, hp=(340, 600), buildup=("fire", 1.0),
                          statuses=(("effect_damage_up_down", "debuff"), ("effect_energy_gain_drain", "debuff")))
    cx, cy = e["centre"]
    kit.glow(canvas, (cx, cy - 20), 120, (255, 140, 60), strength=0.35)
    outlined(canvas, (cx + 100, cy - 40), "72", kit.font(kit.UI_BOLD, 54), (255, 236, 210))
    outlined(canvas, (cx + 100, cy), "CRITICAL", kit.font(kit.UI_BOLD, 15), (255, 196, 0), stroke=2)
    outlined(canvas, (cx - 100, cy - 10), "18", kit.font(kit.UI_BOLD, 32), ELEMENT["fire"])
    # Element proc banner (39), below the enemies so it never covers the numbers
    band = kit.scaled(kit.piece("misc", "banner_event"), width=520)
    by = 452
    canvas.alpha_composite(band, ((kit.W - band.width) // 2 + 40, by))
    outlined(canvas, (kit.W // 2 + 40, by + band.height // 2 - 2), "BURN!", kit.font(kit.DISPLAY_BOLD, 40),
             (255, 210, 120), outline=(60, 10, 6), stroke=3)
    party_row(canvas, acting=0)
    notes = [
        (1, (cx + 150, cy - 70), "Damage number (37)", ["Lato Bold, dark outline; floats up and fades",
                                                         "Crit: bigger + gold 'CRITICAL' tag under it"]),
        (2, (cx - 140, cy - 30), "Element damage", ["Elemental part in the element colour (fire)", "One number per hit / element"]),
        (3, (kit.W // 2 - 230, by + 20), "Proc banner (39)", ["'BURN!' when the buildup bar caps", "Element-coloured text, short band, then gone",
                                                               "One loud effect at a time (art direction)"]),
        (4, (x - 6, y + 30), "HP ghost (31)", ["Pale ghost shows the HP just lost, then drains", "Buildup bar full -> resets after the proc"]),
        (5, (x + 82, y + 62), "New status", ["New debuff icon pops in on the plate"]),
    ]
    return canvas, notes


def draw_stunned():
    canvas = board(active=0)
    turn_banner(canvas, "TURN 3 · GARRICK")
    enemy_plate(canvas, ENEMIES[TARGET])
    command_diamond(canvas, stunned=True)
    party_row(canvas, acting=0, stunned=True)
    hint_bar(canvas, [("Y", "Attack"), ("A", "Defend")])
    notes = [
        (1, (1056, 296), "Stunned diamond", ["Only Attack (Y) and Defend (A) (D-001)", "Skills / Items slots shown as empty sockets",
                                              "Attack is a plain swing: no infusion controls (D-023 A)"]),
        (2, (650, 536), "Stun on the card", ["Skip-turn icon + 'Stunned · 1 turn' in red on the acting card"]),
        (3, (140, 666), "Input hints", ["Only the two live verbs"]),
    ]
    return canvas, notes


if __name__ == "__main__":
    for name, draw in (("command", draw_command), ("target", draw_target), ("hit", draw_hit), ("stunned", draw_stunned)):
        plain, notes = draw()
        kit.save_draft(plain, notes, SCREEN, VERSION, name)
