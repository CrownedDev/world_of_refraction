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
GARRICK = ("Garrick", "Generic:", (("Sera", .22, "18/84"), ("Bram", .9, "87/97")))
BRAM = ("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))
SKILL_HINTS = ["D-pad up/down  More skills", "Y / X / A  Use skill", "R1  Defend      B  Back"]


def save(im, name):
    im.save(os.path.join(OUT, name))


# ---- Main menu: Attack / Skills / Items above the active card. Defend and Back are corner hints.
im, d = v.base()
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
    im, d = v.base(*who)
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
def marker(d, enemy, hp, card=True):
    x, y, r, name = enemy
    if card:  # the one target: name, HP, rank, tier
        v.cut(d, (x - 170, y - r - 138, x + 170, y - r - 44), v.PANEL, v.CYAN)
        d.text((x - 152, y - r - 134), name, font=F(26, True), fill=v.TXT)
        d.text((x + 70, y - r - 128), "412/600", font=F(20, True), fill=v.TXT)
        d.text((x - 152, y - r - 98), "Rank 2 · Tier C", font=F(18), fill=v.DIM)
        v.bar(d, x - 152, y - r - 68, 304, hp, v.ROSE, 12)
    else:  # one of many: name and HP bar only
        d.text((x - 70, y - r - 76), name, font=F(18, True), fill=v.TXT)
        v.bar(d, x - 70, y - r - 50, 140, hp, v.ROSE, 8)
    d.polygon([(x, y - r - 8), (x - 18, y - r - 36), (x + 18, y - r - 36)], fill=v.CYAN)


ROW_WIDTH = 590  # inside the strip


def row(d, x, y, labels, lit):
    """A tab row that never overflows: it shows the window of tabs around the lit one and counts the hidden
    ones at each end ("< 2", "1 >"). Cycling moves the window; it wraps at the ends."""
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
    v.tabs(d, x, y, labels[first:last], lit - first, h=32)
    if last < len(labels):
        d.text((x + used + 4, y + 4), "%d >" % (len(labels) - last), font=F(17, True), fill=v.CYAN)


def strip(d, heading, sources, lit, level, readout, warn=None, warn_col=v.WARN, weapons=None, weapon_lit=0, via=None):
    """Same tab rows as the skill menu: [weapon types, Attack only], infusion sources, levels. No labels.
    A fixed source is a single lit tab. No infusion at all (items, a stunned character): pass sources=None."""
    rows = ([weapons] if weapons else []) + ([sources, ["—", "I", "II"]] if sources else [])
    lits = ([weapon_lit] if weapons else []) + ([lit, level] if sources else [])
    bottom = 536
    top = bottom - 50 - 40 * len(rows) - (32 if sources else 0) - (30 if warn else 0) - (28 if via else 0)
    v.cut(d, (20, top, 640, bottom), v.PANEL, v.CYAN)
    d.text((36, top + 6), heading, font=F(22, True), fill=v.TXT)
    y = top + 44
    for labels, on in zip(rows, lits):
        row(d, 36, y, labels, on)
        y += 40
    if sources:
        d.text((36, y + 2), readout, font=F(18, True), fill=v.EP)
        y += 32
    if via:  # which source pays for the picked element
        d.text((36, y), via, font=F(17), fill=v.DIM)
        y += 28
    if warn:
        d.text((36, y), warn, font=F(18, True), fill=warn_col)


INFUSE = ["L2 / R2  Infusion element", "L1  Infusion level"]
TARGET = "< >  Target      A  Confirm"
FOOT = "R1  Defend      B  Back"

# The source row shows ELEMENTS (None, Raw, then each element the character can infuse); duplicates collapse.
# A small "via" line names the source that pays. Normal Attack, a two-weapon Generic: Lightning comes from the
# Hammer's crystal, so picking it switches to the Hammer (free) and the weapon row follows.
im, d = v.base(*GARRICK)
marker(d, v.ENEMIES[1], .68)
strip(d, "Attack  ·  choose a target", ["None", "Raw", "Fire", "Lightning"], 3, 1, "x1.15 damage · 0 EP",
      weapons=["Sword", "Hammer"], weapon_lit=1, via="via Hammer crystal  ·  wear")
v.hints(d, ["D-pad up/down  Weapon"] + INFUSE + [TARGET, FOOT])
save(im, "6_target_attack_weapon.png")

# A spell is origin-bound: its element is a single lit tab, only the level changes.
im, d = v.base()
marker(d, v.ENEMIES[1], .68)
strip(d, "Shade Lance  ·  choose a target", ["Darkness"], 0, 2, "x1.30 damage · size x2.0 · 12 EP", "Level II backlash:  -11 HP",
      via="via Innate  ·  12 EP")
v.hints(d, ["L1  Infusion level", TARGET, FOOT])
save(im, "7_target_spell_fixed_source.png")

# A Resonator's ability: weapon crystal (Fire), Storm / Ember / Tide Ring (Lightning, Fire, Water), evolution (Void).
# Two sources give Fire: one tab marked x2. The action uses both, each pays its own cost, and the hit gains
# status buildup per extra source (Crown's rule; built as its own job). The via line lists every source and cost.
ELEMENTS = ["None", "Raw", "Fire ×2", "Lightning", "Water", "Void"]
im, d = v.base(*BRAM)
marker(d, v.ENEMIES[1], .68)
strip(d, "Cleave  ·  choose a target", ELEMENTS, 2, 2, "x1.30 damage · +25% status buildup", "Level II:  crystal wear, may break",
      via="via Sword crystal + Ember Ring  ·  wear on both  ·  0 EP")
v.hints(d, INFUSE + [TARGET, FOOT])
save(im, "8_target_ability_shared_element.png")

# The evolution's element counts like any other; it pays EP, and HP at level II.
im, d = v.base(*BRAM)
marker(d, v.ENEMIES[1], .68)
strip(d, "Cleave  ·  choose a target", ELEMENTS, 5, 2, "x1.30 damage · 10 EP", "Level II backlash:  -9 HP", via="via Evolution  ·  10 EP")
v.hints(d, INFUSE + [TARGET, FOOT])
save(im, "8b_target_ability_evolution_element.png")

# All enemies / all allies / self: nothing to choose, so the wording is "confirm" and everything in range is marked.
# An evolution spell of a Broken Darkness / Reality character is the one spell with two sources.
im, d = v.base()
for enemy, hp in zip(v.ENEMIES, (.9, .68, .4)):
    marker(d, enemy, hp, card=False)
strip(d, "Eclipse  ·  all enemies  ·  confirm", ["Void", "Darkness"], 0, 2, "x1.30 damage · size x2.0 · 16 EP",
      "Level II backlash:  -24 HP  ·  this would kill you", v.KILL, via="via Evolution  ·  16 EP")
v.hints(d, INFUSE + ["A  Confirm", FOOT])
save(im, "9_target_all_enemies_confirm.png")

# No infusion (an item, a broken-adapter spell, a stunned character): no rows at all.
im, d = v.base()
marker(d, v.ENEMIES[1], .68)
strip(d, "Fire Bomb  ·  choose a target", None, 0, 0, "")
v.hints(d, [TARGET, FOOT])
save(im, "10_target_no_infusion.png")
print("ok")
