"""Battle-HUD mockup refine_v3f: refine_v3b's b_gear_tabs layout, revamped. No row labels.
One KIND row (only the kinds the character has); above it, one row tied to the selected kind
(schools, adapters, weapon types or Breakthrough sources); the cards below. One image per kind.
No Resonate (W) kind: weapon spells are gone (B-098), weapon skills are Abilities.
Target/infusion images stay in refine_v3b. Reuses its helpers (loading it redraws v3b and v2). Run: python make.py"""
import importlib.util
import os

OUT = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("v3b", os.path.join(OUT, "..", "refine_v3b", "make.py"))
v = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(v)
STEELY, VOID = (150, 160, 175), (166, 75, 240)

GARRICK = ("Garrick", "Generic:", (("Sera", .22, "18/84"), ("Bram", .9, "87/97")))
BRAM = ("Bram", "Resonator:", (("Garrick", .8, "67/84"), ("Sera", .22, "18/84")))


def screen(name, who, top, top_lit, kinds, kind_lit, title, page, rows, top_word):
    im, d = v.base(*who)
    if top:
        v.tabs(d, 20, 150, top, top_lit)
    v.tabs(d, 20, 190, kinds, kind_lit)
    v.skill_panel(d, title, page, rows, pager="Skills %s")
    lines = ["L2 / R2  Kind"]
    if top:
        lines.append("D-pad left/right  " + top_word)
    v.hints(d, lines + ["D-pad up/down  More skills", "Y / X / A  Use skill", "R1  Defend      B  Back"])
    im.save(os.path.join(OUT, name))


# Abilities: no second row, except for a Generic carrying two weapons (weapon types, never weapon names).
ABILITIES = [
    ("Y", "Cleave", "4 EP", STEELY, "Slash · 1 hit · one enemy", "Heavy damage."),
    ("X", "Whirl", "8 EP", STEELY, "Slash · 1 hit · all enemies", "Medium damage to the front row."),
    ("A", "Guard Break", "5 EP", STEELY, "Impact · 1 hit · one enemy", "Lowers the target's defence for 2 turns."),
]
# Abilities is the weapon's own six plus the crystal's extra slots, so it can run past 2 pages (no augment-stone slots).
screen("a_abilities_two_weapons.png", GARRICK, ["Sword", "Hammer"], 0, ["Abilities"], 0, "Abilities · Sword", "1/3", ABILITIES, "Weapon")
screen("a_abilities_one_weapon.png", GARRICK, [], 0, ["Abilities", "Resonate (A)"], 0, "Abilities", "1/3", ABILITIES, "")

# Refractions: the row above is the schools (only schools that hold a spell).
screen("b_refractions.png", (), ["Destruction", "Enhancement", "Impairment"], 0, ["Refractions", "Breakthrough"], 0,
       "Refractions · Destruction", "1/2", [
    ("Y", "Shade Lance", "6 EP", v.SHADE, "Darkness · 1 hit · one enemy", "Heavy damage. Builds Blind."),
    ("X", "Umbral Rain", "12 EP", v.SHADE, "Darkness · 3 hits · all enemies", "Light damage per hit."),
    ("A", "Night Fang", "8 EP", v.SHADE, "Darkness · 2 hits · one enemy", "Consumes 1 Shade stack for more damage."),
], "School")

# Breakthrough: the row above is its sources.
screen("c_breakthrough.png", BRAM, ["Evolution", "Storm Ring"], 0, ["Abilities", "Breakthrough", "Resonate (A)"], 1,
       "Breakthrough · Evolution", "1/1", [
    ("Y", "Eclipse", "16 EP", VOID, "Void · Destruction · all enemies", "Heavy damage. Builds Silence."),
    ("X", "Null Ward", "10 EP", VOID, "Void · Enhancement · all allies · 2 turns", "Absorbs the next spell hit."),
], "Source")

# Resonate (A): the row above is the adapters.
screen("d_resonate_adapter.png", BRAM, ["Storm Ring", "Ember Ring", "Tide Ring"], 1, ["Abilities", "Breakthrough", "Resonate (A)"], 2,
       "Resonate (A) · Ember Ring", "1/2", [
    ("Y", "Flame Lance", "0 EP", v.FIRE, "Fire · Destruction · one enemy", "Heavy damage. Builds Burn."),
    ("X", "Ember Veil", "0 EP", v.FIRE, "Fire · Enhancement · one ally · 3 turns", "Adds Fire to the ally's attacks."),
    ("A", "Kindle", "0 EP", v.FIRE, "Fire · Enhancement · self · 2 turns", "Raises spell damage."),
], "Adapter")
print("ok")
