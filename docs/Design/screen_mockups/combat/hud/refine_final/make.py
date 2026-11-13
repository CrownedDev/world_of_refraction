"""Battle-HUD mockups, the final set for Crown's review (B-436): main menu, one image per skill kind,
and the target / infusion views. Flat layout only (Windows fonts, plain shapes), not the Hidris art.
Reuses refine_v3b's helpers (loading it redraws v3b and v2). Run: python make.py"""
import importlib.util
import os

OUT = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("v3b", os.path.join(OUT, "..", "refine_v3b", "make.py"))
v = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(v)
F = v.F
GLYPH = {"Y": (240, 200, 70), "X": (90, 140, 240), "A": (110, 200, 110)}
STEELY, VOID = (150, 160, 175), (166, 75, 240)
ROSE_LOW, BG = (255, 143, 180), (16, 18, 25)
# HP and EP bars: a darker red and a darker blue, so the white value reads clearly (white on each is over 5:1);
# low HP is a hotter pink-red (apart from Burn's orange). Same colours as the menu mockups' kit.
BAR = {v.ROSE: (168, 36, 60), ROSE_LOW: (214, 48, 96), v.EP: (36, 92, 170)}
GARRICK = ("Garrick", "Generic:", (("Sera", .22, "18/84"), ("Bram", .9, "87/97")))
BRAM = ("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))
SKILL_HINTS = ["D-pad up/down  More skills", "Y / X / A  Use skill", "R1  Defend      B  Back"]


def vbar(d, x, y, w, h, frac, col, text=None, ghost=0.0, ghost_col=None, size=17):
    """A bar with its value centred inside. ghost < 0: the chunk a hit would remove (drawn at the end of the fill);
    ghost > 0: what it would add (drawn after the fill). In game the ghost pulses."""
    col = BAR.get(col, col)
    d.rectangle((x, y, x + w, y + h), fill=(42, 45, 53))
    keep = max(0.0, frac + min(ghost, 0.0))
    d.rectangle((x, y, x + int(w * keep), y + h), fill=col)
    if ghost:
        a, b = (keep, frac) if ghost < 0 else (frac, min(1.0, frac + ghost))
        # A dimmed fill with light stripes (reads as "not yet"), dark enough that the plain white value stays readable.
        dim = tuple((c + t) // 2 for c, t in zip(col, (42, 45, 53)))
        d.rectangle((x + int(w * a), y, x + int(w * b), y + h), fill=dim)
        for sx in range(x + int(w * a) + 2, x + int(w * b) - 3, 8):
            d.line((sx, y + h, sx + 4, y), fill=ghost_col or (255, 235, 240))
    if text:
        tw = d.textlength(text, font=F(size, True))
        d.text((x + (w - tw) / 2, y + (h - size) / 2 - 3), text, font=F(size, True), fill=(255, 255, 255), stroke_width=2, stroke_fill=(16, 18, 25))


BURN = (229, 72, 58)


def base(name="Sera", cls="Darkness Refractor:", allies=(("Garrick", .8, "67/84"), ("Bram", .9, "87/97")), hp_cost=0.0, ep_cost=0.0):
    """refine_v2's frame, with the cards redrawn: values inside the bars, the tier beside the name, a status-buildup
    bar on every card. hp_cost / ep_cost: what the picked infusion would cost, ghosted on the character's own bars."""
    im, d = v.base(name, cls, allies)
    v.cut(d, (20, 548, 400, 704), v.PANEL, v.CYAN)
    d.text((38, 552), name, font=F(28, True), fill=v.TXT)
    d.text((38 + d.textlength(name + " ", font=F(28, True)), 552), "(C)", font=F(28, True), fill=v.DIM)
    vbar(d, 38, 592, 344, 24, .22, ROSE_LOW, "18/84", ghost=-hp_cost)
    vbar(d, 38, 620, 344, 24, .75, v.EP, "30/40", ghost=-ep_cost)
    vbar(d, 38, 648, 344, 18, .4, BURN, "40/100", size=13)
    # The class and the status icons, as one line centred on the card (sized so the longest class fits).
    line = cls.rstrip(":") + ":"
    line_w = int(d.textlength(line, font=F(17, True)))
    left = 20 + (380 - (line_w + 10 + 3 * 30 - 6)) // 2
    d.text((left, 673), line, font=F(17, True), fill=v.DIM)
    for i, (s_, col) in enumerate([("B", (229, 72, 58)), ("S", (166, 75, 240)), ("D", (58, 123, 234))]):
        x = left + line_w + 10 + i * 30
        d.rectangle((x, 673, x + 24, 697), fill=col)
        d.text((x + 6, 673), s_, font=F(17, True), fill=(255, 255, 255))
    for i, (n, hp, val) in enumerate(allies):
        x = 420 + i * 250
        d.rectangle((x - 2, 644, x + 234, 708), fill=BG)
        v.cut(d, (x, 646, x + 230, 704), v.PANEL, v.STEEL, 10)
        d.text((x + 12, 647), n + " (C)", font=F(18, True), fill=v.DIM)
        vbar(d, x + 12, 672, 206, 18, hp, v.ROSE, val, size=13)
        vbar(d, x + 12, 692, 206, 7, .3, BURN)
    return im, d


def save(im, name):
    im.save(os.path.join(OUT, name))


# ---- Main menu: Attack / Skills / Items above the active card. Defend and Back are corner hints.
im, d = base()
for i, (label, g) in enumerate([("Attack", "A"), ("Skills", "X"), ("Items", "Y")]):
    y = 362 + i * 60
    v.cut(d, (20, y, 280, y + 52), v.TEAL if i == 0 else v.PLATE, v.CYAN if i == 0 else v.STEEL)
    d.ellipse((32, y + 12, 60, y + 40), outline=GLYPH[g], width=3)
    d.text((40, y + 13), g, font=F(16, True), fill=GLYPH[g])
    d.text((76, y + 9), label, font=F(24, True), fill=v.TXT)
v.hints(d, ["R1  Defend      B  Back"])
save(im, "1_main_menu.png")


# ---- Skills: the kind row on top, the selected kind's row under it (schools, sources, adapters, weapon types), cards below.
def skills(name, who, kinds, kind_lit, under, under_lit, under_word, title, page, rows):
    im, d = base(*who)
    v.tabs(d, 20, 150 if under else 190, kinds, kind_lit)
    if under:
        v.tabs(d, 20, 190, under, under_lit)
    v.skill_panel(d, title, page, rows, pager="Skills %s")
    v.hints(d, ["L2 / R2  Kind"] + (["D-pad left/right  " + under_word] if under else []) + SKILL_HINTS)
    save(im, name)


ABILITIES = [
    ("Y", "Cleave", "4 EP", STEELY, "Slash · 1 hit · one enemy", "Heavy damage."),
    ("X", "Whirl", "8 EP", STEELY, "Slash · 1 hit · all enemies", "Medium damage to the front row."),
    ("A", "Guard Break", "5 EP", STEELY, "Impact · 1 hit · one enemy", "Lowers the target's defence for 2 turns."),
]
# Abilities: the weapon's own six plus the crystal's extra slots (no augment-stone slots, no Resonate (W)).
skills("2_abilities_one_weapon.png", GARRICK, ["Abilities", "Resonate"], 0, [], 0, "", "Abilities", "1/3", ABILITIES)
skills("2b_abilities_two_weapons.png", GARRICK, ["Abilities"], 0, ["Sword", "Hammer"], 0, "Weapon", "Abilities · Sword", "1/3", ABILITIES)
skills("3_refractions.png", (), ["Refractions", "Breakthrough"], 0, ["Destruction", "Enhancement", "Impairment"], 0, "School",
       "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", v.SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", v.SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", v.SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
])
skills("4_breakthrough.png", BRAM, ["Abilities", "Breakthrough", "Resonate"], 1, ["Evolution", "Storm Ring"], 0, "Source",
       "Breakthrough · Evolution", "1/1", [
    ("Y", "Eclipse", "16 EP", VOID, "Void · Destruction · all enemies", "Heavy damage. Builds Silence."),
    ("X", "Null Ward", "10 EP", VOID, "Void · Enhancement · all allies · 2 turns", "Absorbs the next spell hit."),
])
skills("5_resonate.png", BRAM, ["Abilities", "Breakthrough", "Resonate"], 2, ["Storm Ring", "Ember Ring", "Tide Ring"], 1, "Adapter",
       "Resonate · Ember Ring", "1/2", [
    ("Y", "Flame Lance", "0 EP", v.FIRE, "Fire · Destruction · one enemy", "Heavy damage. Builds Burn."),
    ("X", "Ember Veil", "0 EP", v.FIRE, "Fire · Enhancement · one ally · 3 turns", "Adds Fire to the ally's attacks."),
    ("A", "Kindle", "0 EP", v.FIRE, "Fire · Enhancement · self · 2 turns", "Raises spell damage."),
])


# ---- Target picker with the infusion strip.
FIRE_UI = (229, 72, 58)


def marker(d, enemy, hp, card=True, hp_max=600, dmg=.12, build=.35, add=.2, build_col=FIRE_UI):
    """The target marker. The hit's preview is ON the bars: the HP chunk it would remove and the status
    buildup it would add, ghosted, updating live with the infusion element and level. Values sit inside the bars."""
    x, y, r, name = enemy
    hp_text = "%d/%d" % (round(hp * hp_max), hp_max)
    if card:  # the one target: name, rank, tier, HP, status buildup
        v.cut(d, (x - 170, y - r - 166, x + 170, y - r - 44), v.PANEL, v.CYAN)
        d.text((x - 152, y - r - 162), name, font=F(26, True), fill=v.TXT)
        vbar(d, x - 152, y - r - 124, 304, 24, hp, v.ROSE, hp_text, ghost=-dmg)
        vbar(d, x - 152, y - r - 97, 304, 20, build, build_col, "%d/100" % round(build * 100), ghost=add,
             ghost_col=tuple(min(255, c + 90) for c in build_col), size=13)
        rank = "Tier C"  # no rank; centred at the bottom, under the bars
        d.text((x - d.textlength(rank, font=F(18)) / 2, y - r - 74), rank, font=F(18), fill=v.DIM)
    else:  # one of many: name, HP and buildup bars
        d.text((x - 70, y - r - 92), name, font=F(18, True), fill=v.TXT)
        vbar(d, x - 70, y - r - 66, 140, 18, hp, v.ROSE, hp_text, ghost=-dmg, size=13)
        vbar(d, x - 70, y - r - 46, 140, 8, build, build_col, ghost=add, ghost_col=tuple(min(255, c + 90) for c in build_col))
    d.polygon([(x, y - r - 8), (x - 18, y - r - 36), (x + 18, y - r - 36)], fill=v.CYAN)


ROW_WIDTH = 590  # inside the strip


def wear_col(left):
    return (110, 200, 110) if left > .5 else v.WARN if left > .25 else v.KILL


def row(d, x, y, labels, lit, wear=None):
    """A tab row that never overflows: it shows the window of tabs around the lit one and counts the hidden
    ones at each end ("< 2", "1 >"). Cycling moves the window; it wraps at the ends.
    wear: {label: [durability left, one per source behind that element]}, drawn as small bars inside the tab."""
    width = lambda t: int(d.textlength(t, font=F(18, True))) + 34
    first, last = lit, lit + 1
    used = width(labels[lit])
    while True:  # grow right, then left, while the next tab fits (room kept for the two counters)
        grew = False
        for side in ("right", "left"):
            i = last if side == "right" else first - 1
            if 0 <= i < len(labels) and used + width(labels[i]) <= ROW_WIDTH - 40:
                used += width(labels[i])
                first, last = (first, last + 1) if side == "right" else (first - 1, last)
                grew = True
        if not grew:
            break
    if first > 0:
        d.text((x, y + 4), "< %d" % first, font=F(17, True), fill=v.CYAN)
        x += 44
    v.tabs(d, x, y, labels[first:last], lit - first, h=46 if wear else 32)
    tx = x
    for lab in labels[first:last] if wear else []:
        for i, left in enumerate(wear.get(lab, [])):  # one durability bar per source
            bx = tx + 14 + i * 40
            d.rectangle((bx, y + 32, bx + 34, y + 39), fill=(42, 45, 53))
            d.rectangle((bx, y + 32, bx + int(34 * left), y + 39), fill=wear_col(left))
        tx += width(lab)
    if last < len(labels):
        d.text((x + used + 4, y + 4), "%d >" % (len(labels) - last), font=F(17, True), fill=v.CYAN)


def strip(d, kind, heading, sources, lit, level, readout=None, warn=None, warn_col=v.WARN, weapons=None, weapon_lit=0, wear=None):
    """Same tab rows as the skill menu: [weapon types, Attack only], infusion elements, levels. No labels.
    A fixed source is a single lit tab. No infusion at all (items, a stunned character): pass sources=None.
    wear: the durability left on each source behind an element tab (see row)."""
    rows = ([(weapons, weapon_lit, None)] if weapons else []) + ([(sources, lit, wear), (["—", "I", "II"], level, None)] if sources else [])
    bottom = 536
    if not heading.startswith(kind):  # the category leads the title line; "Attack" is not repeated
        heading = kind + "  ·  " + heading
    top = bottom - 50 - sum(54 if w else 40 for _, _, w in rows) - (32 if readout else 0) - (30 if warn else 0)
    # The frame is as wide as its content: the heading, the widest row, the readout and the warning.
    tab_w = lambda labels: sum(int(d.textlength(t, font=F(18, True))) + 34 for t in labels)
    inner = max([int(d.textlength(heading, font=F(22, True)))] + [min(ROW_WIDTH, tab_w(labels)) for labels, _, _ in rows]
                + [int(d.textlength(t, font=F(18, True))) for t in (readout, warn) if t])
    v.cut(d, (20, top, 20 + inner + 32, bottom), v.PANEL, v.CYAN)
    d.text((36, top + 6), heading, font=F(22, True), fill=v.TXT)
    y = top + 44
    for labels, on, w in rows:
        row(d, 36, y, labels, on, w)
        y += 54 if w else 40
    if readout:
        d.text((36, y + 2), readout, font=F(18, True), fill=v.EP)
        y += 32
    if warn:
        d.text((36, y), warn, font=F(18, True), fill=warn_col)


INFUSE = ["L2 / R2  Infusion element", "L1  Infusion level"]
TARGET = "< >  Target      A  Confirm"
FOOT = "R1  Defend      B  Back"

# The source row shows ELEMENTS (None, Raw, then each element the character can infuse); duplicates collapse.
# Each element tab carries the durability left on the source(s) behind it. Normal Attack, a two-weapon Generic: Lightning comes from the
# Hammer's crystal, so picking it switches to the Hammer (free) and the weapon row follows.
im, d = base(*GARRICK)
marker(d, v.ENEMIES[1], .68, dmg=.07, build=.35, add=.12, build_col=(242, 162, 58))
strip(d, "Attack", "Attack  ·  choose a target", ["None", "Raw", "Fire", "Lightning"], 3, 1,
      weapons=["Sword", "Hammer"], weapon_lit=1, wear={"Fire": [.8], "Lightning": [.62]})
v.hints(d, ["D-pad up/down  Weapon"] + INFUSE + [TARGET, FOOT])
save(im, "6_target_attack_weapon.png")

# A spell is origin-bound: its element is a single lit tab, only the level changes.
im, d = base(hp_cost=11 / 84, ep_cost=12 / 40)  # the EP cost and the backlash, ghosted on Sera's own bars
marker(d, v.ENEMIES[1], .68, dmg=.16, build=.35, add=.18, build_col=(167, 155, 194))
strip(d, "Refractions", "Shade Lance  ·  choose a target", ["Darkness"], 0, 2, "Size x2.0", "Level II backlash:  -11 HP")
v.hints(d, ["L1  Infusion level", TARGET, FOOT])
save(im, "7_target_spell_fixed_source.png")

# A Resonator's ability: weapon crystal (Fire), Storm / Ember / Tide Ring (Lightning, Fire, Water), evolution (Void).
# Two sources give Fire: one tab marked x2. The action uses both, each pays its own cost, and the hit gains
# status buildup per extra source (Crown's rule; built as its own job). The tab shows both sources' durability.
ELEMENTS = ["None", "Raw", "Fire ×2", "Lightning", "Water", "Void"]
WEAR = {"Fire ×2": [.8, .22], "Lightning": [.9], "Water": [.55]}
im, d = base(*BRAM)
marker(d, v.ENEMIES[1], .68, dmg=.13, build=.35, add=.30)
strip(d, "Abilities", "Cleave  ·  choose a target", ELEMENTS, 2, 2, None, "Level II:  crystal wear, may break", wear=WEAR)
v.hints(d, INFUSE + [TARGET, FOOT])
save(im, "8_target_ability_shared_element.png")

# The evolution's element counts like any other; it pays EP, and HP at level II: both ghosted on Bram's own bars.
im, d = base(*BRAM, hp_cost=9 / 84, ep_cost=10 / 40)
marker(d, v.ENEMIES[1], .68, dmg=.13, build=.35, add=.22, build_col=(166, 75, 240))
strip(d, "Abilities", "Cleave  ·  choose a target", ELEMENTS, 5, 2, None, "Level II backlash:  -9 HP", wear=WEAR)
v.hints(d, INFUSE + [TARGET, FOOT])
save(im, "8b_target_ability_evolution_element.png")

# All enemies / all allies / self: nothing to choose, so the wording is "confirm" and everything in range is marked.
# An evolution spell of a Broken Darkness / Reality character is the one spell with two sources.
im, d = base(hp_cost=24 / 84, ep_cost=16 / 40)
for enemy, hp, hp_max in zip(v.ENEMIES, (.9, .68, .4), (180, 600, 180)):
    marker(d, enemy, hp, card=False, hp_max=hp_max, dmg=.3 if hp_max == 180 else .14, build=.2, add=.25, build_col=(166, 75, 240))
strip(d, "Breakthrough", "Eclipse  ·  all enemies  ·  confirm", ["Void", "Darkness"], 0, 2, "Size x2.0",
      "Level II backlash:  -24 HP  ·  this would kill you", v.KILL)
v.hints(d, INFUSE + ["A  Confirm", FOOT])
save(im, "9_target_all_enemies_confirm.png")

# No infusion (an item, a broken-adapter spell, a stunned character): no rows at all.
im, d = base()
marker(d, v.ENEMIES[1], .68, dmg=.1, build=.35, add=.15)
strip(d, "Item", "Fire Bomb  ·  choose a target", None, 0, 0)
v.hints(d, [TARGET, FOOT])
save(im, "10_target_no_infusion.png")
print("ok")
