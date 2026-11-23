"""Crown's idea (2026-11-23): the target panel stays small; X shows the weapon row, Y swaps it for the infusion rows.
Draws over refine_final's helpers, from a scratchpad copy so the repo's PNGs are untouched."""
import importlib.util, os, sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
FINAL = os.path.join(HERE, "screen_mockups", "combat", "hud", "refine_final")
spec = importlib.util.spec_from_file_location("final", os.path.join(FINAL, "make.py"))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
v, F = m.v, m.F
OUT = os.path.join(HERE, "out")
os.makedirs(OUT, exist_ok=True)


def panel(d, heading, summary=None, rows=(), readout=None, warn=None):
    bottom = 536
    top = bottom - 50 - (30 if summary else 0) - 40 * len(rows) - (32 if readout else 0) - (30 if warn else 0)
    tab_w = lambda labels: sum(int(d.textlength(t, font=F(18, True))) + 34 for t in labels)
    inner = max([int(d.textlength(heading, font=F(22, True)))] + [tab_w(l) for l, _ in rows]
                + [int(d.textlength(t, font=F(18, True))) for t in (summary, readout, warn) if t])
    v.cut(d, (20, top, 20 + inner + 32, bottom), v.PANEL, v.CYAN)
    d.text((36, top + 6), heading, font=F(22, True), fill=v.TXT)
    y = top + 44
    if summary:
        d.text((36, y), summary, font=F(18, True), fill=v.DIM)
        y += 30
    for labels, lit in rows:
        m.row(d, 36, y, labels, lit)
        y += 40
    if readout:
        d.text((36, y + 2), readout, font=F(18, True), fill=v.EP)
        y += 32
    if warn:
        d.text((36, y), warn, font=F(18, True), fill=v.WARN)


TARGET, FOOT = m.TARGET, m.FOOT
shots = []


def shot(name, who, marker_args, panel_args, hints, base_kw=None):
    im, d = m.base(*who, **(base_kw or {}))
    m.marker(d, v.ENEMIES[1], .68, **marker_args)
    panel(d, **panel_args)
    v.hints(d, hints)
    im.save(os.path.join(OUT, name + ".png"))
    shots.append((name, im))


ATTACK = dict(dmg=.07, build=.35, add=.12, build_col=(242, 162, 58))
shot("1_attack_small", m.GARRICK, ATTACK,
     dict(heading="Attack  ·  choose a target", summary="Hammer  ·  Lightning  ·  Lv I"),
     ["X  Weapon      Y  Infusion", TARGET, FOOT])
shot("2_attack_X_weapon", m.GARRICK, ATTACK,
     dict(heading="Attack  ·  weapon", rows=[(["Sword", "Hammer"], 1)]),
     ["X  Next weapon      Y  Infusion", TARGET, FOOT])
shot("3_attack_Y_infusion", m.GARRICK, ATTACK,
     dict(heading="Attack  ·  infusion", rows=[(["None", "Raw", "Fire", "Lightning"], 3), (["—", "I", "II"], 1)]),
     ["X  Weapon      L1  Level", "L2 / R2  Element", TARGET, FOOT])
SPELL = dict(dmg=.16, build=.35, add=.18, build_col=(167, 155, 194))
COST = dict(hp_cost=11 / 84, ep_cost=12 / 40)
shot("4_spell_small", ("Sera", "Darkness Refractor:", (("Garrick", .8, "67/84"), ("Bram", .9, "87/97"))), SPELL,
     dict(heading="Shade Lance  ·  choose a target", summary="Lv II", readout="Size x2.0", warn="Level II backlash:  -11 HP"),
     ["Y  Infusion level", TARGET, FOOT], COST)
shot("5_spell_Y_level", ("Sera", "Darkness Refractor:", (("Garrick", .8, "67/84"), ("Bram", .9, "87/97"))), SPELL,
     dict(heading="Shade Lance  ·  infusion", rows=[(["—", "I", "II"], 2)], readout="Size x2.0", warn="Level II backlash:  -11 HP"),
     ["L1  Level      Y  Close", TARGET, FOOT], COST)

W, H = 1280, 720
sheet = Image.new("RGB", (W, len(shots) * (H + 30)), "white")
sd = ImageDraw.Draw(sheet)
for i, (name, im) in enumerate(shots):
    sd.text((10, i * (H + 30) + 4), name.replace("_", " "), font=F(20, True), fill=(0, 0, 0))
    sheet.paste(im, (0, i * (H + 30) + 30))
sheet.save(os.path.join(OUT, "sheet.png"))
print("ok")
