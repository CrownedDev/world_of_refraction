"""Scratch mockups for the battle-UI refinement proposal (flat, Windows fonts). Run: python make.py"""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 1280, 720
OUT = os.path.dirname(os.path.abspath(__file__))
F = lambda s, b=False: ImageFont.truetype("C:/Windows/Fonts/%s.ttf" % ("segoeuib" if b else "segoeui"), s)
BG, PANEL, STEEL = (16, 18, 25), (20, 22, 28), (140, 144, 152)
TEAL, CYAN, TXT, DIM = (51, 127, 141), (159, 236, 245), (238, 241, 245), (169, 175, 184)
ROSE, EP = (224, 64, 127), (111, 214, 228)


def cut(d, box, fill, outline=None, c=14):
    x0, y0, x1, y1 = box
    pts = [(x0 + c, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0 + c)]
    d.polygon(pts, fill=fill, outline=outline)


def base():
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    for i, (x, y, r) in enumerate([(300, 470, 62), (640, 440, 95), (980, 480, 58)]):
        d.ellipse((x - r, y - r, x + r, y + r), fill=(10, 8, 10))
        d.ellipse((x - r * .4, y - r * .3, x - r * .25, y - r * .15), fill=(255, 110, 50))
        d.ellipse((x + r * .25, y - r * .3, x + r * .4, y - r * .15), fill=(255, 110, 50))
    return im, d


def bar(d, x, y, w, frac, col, h=10):
    d.rectangle((x, y, x + w, y + h), fill=(42, 45, 53))
    d.rectangle((x, y, x + int(w * frac), y + h), fill=col)


def card(d, x, y, name, hp, ep, active=False, compact=False):
    w, h = (250, 92) if not compact else (250, 44)
    cut(d, (x, y, x + w, y + h), PANEL, CYAN if active else STEEL)
    d.text((x + 14, y + 6), name, font=F(20, True), fill=TXT if active else DIM)
    bar(d, x + 14, y + 36, w - 70, hp, ROSE)
    d.text((x + w - 48, y + 28), str(int(hp * 100)), font=F(16, True), fill=TXT)
    if not compact:
        bar(d, x + 14, y + 58, w - 70, ep, EP)
        d.text((x + w - 48, y + 50), str(int(ep * 40)), font=F(16, True), fill=TXT)


def note(d, text, y=14):
    d.rectangle((0, 0, W, 0), fill=BG)


def btn(d, x, y, label, glyph, gcol, focus=False, w=260):
    cut(d, (x, y, x + w, y + 52), (46, 46, 46) if not focus else TEAL, CYAN if focus else STEEL)
    d.ellipse((x + 12, y + 12, x + 40, y + 40), outline=gcol, width=3)
    d.text((x + 20, y + 13), glyph, font=F(16, True), fill=gcol)
    d.text((x + 56, y + 11), label, font=F(24, True), fill=TXT)


def save(im, name):
    im.save(os.path.join(OUT, name))


# 1 In-world target picking
im, d = base()
d.polygon([(640, 300), (620, 270), (660, 270)], fill=CYAN)  # marker over target
d.rectangle((560, 118, 720, 120), fill=CYAN)
cut(d, (470, 130, 810, 232), PANEL, CYAN)
d.text((490, 138), "Cinder Warden", font=F(26, True), fill=TXT)
d.text((490, 172), "Rank 2 · Tier C", font=F(18), fill=DIM)
bar(d, 490, 206, 300, .68, ROSE, 12)
d.text((250, 330), "<", font=F(60, True), fill=CYAN)
d.text((985, 330), ">", font=F(60, True), fill=CYAN)
d.text((205, 392), "Ember Imp", font=F(18), fill=DIM)
d.text((940, 392), "Ember Imp", font=F(18), fill=DIM)
card(d, 20, 600, "Garrick", .8, .6, True)
d.text((20, 676), "", font=F(16))
d.text((330, 672), "< > / Stick / Mouse: choose target    A / Click: confirm    B: back    Y: allies", font=F(20), fill=TXT)
save(im, "1_target_in_world.png")

# 2 Command menu with detail panel
im, d = base()
for i, (lab, g, c) in enumerate([("Attack", "A", (110, 200, 110)), ("Skills", "X", (90, 140, 240)), ("Items", "Y", (240, 200, 70)), ("Defend", "B", (230, 90, 90))]):
    btn(d, 30, 400 + i * 60, lab, g, c, focus=(i == 1))
cut(d, (310, 330, 700, 650), PANEL, STEEL)
d.text((330, 340), "Skills", font=F(22, True), fill=DIM)
rows = [("Flame Lance", "6 EP", True, (229, 72, 58)), ("Ember Veil", "4 EP", False, (229, 72, 58)), ("Arc Bolt", "8 EP", False, (242, 162, 58)), ("Mend", "10 EP", False, (79, 201, 138))]
for i, (n, c, f, col) in enumerate(rows):
    y = 380 + i * 56
    cut(d, (330, y, 680, y + 48), TEAL if f else (46, 46, 46), CYAN if f else STEEL, 10)
    d.ellipse((342, y + 14, 362, y + 34), fill=col)
    d.text((374, y + 9), n, font=F(22, True), fill=TXT)
    d.text((600, y + 11), c, font=F(20, True), fill=EP)
cut(d, (720, 330, 1240, 520), PANEL, STEEL)
d.text((740, 342), "Flame Lance", font=F(26, True), fill=TXT)
d.text((740, 380), "One enemy · Fire · 6 EP", font=F(19), fill=DIM)
d.text((740, 414), "Hurls a lance of flame. Burns on break.", font=F(19), fill=TXT)
d.text((740, 450), "EP  30 -> 24", font=F(20, True), fill=EP)
d.text((330, 676), "A: pick    B: back    Mouse: hover to focus, wheel to scroll", font=F(20), fill=TXT)
card(d, 1010, 600, "Garrick", .8, .6, True)
save(im, "2_menu_detail_panel.png")

# 3 Declutter: active card big, others compact; turn strip slim
im, d = base()
card(d, 20, 590, "Garrick", .8, .6, True)
card(d, 290, 646, "Sera", .6, .9, compact=True)
card(d, 560, 646, "Bram", .9, .4, compact=True)
for i, c in enumerate([CYAN, STEEL, STEEL, STEEL, STEEL]):
    x = 1010 + i * 50
    d.regular_polygon((x + 22, 40, 22), 6, fill=PANEL, outline=c)
    d.text((x + 15, 26), "GSBEE"[i], font=F(18, True), fill=TXT if i == 0 else DIM)
d.text((1010, 76), "Next turns", font=F(16), fill=DIM)
btn(d, 30, 360, "Attack", "A", (110, 200, 110))
btn(d, 30, 420, "Skills", "X", (90, 140, 240), focus=True)
btn(d, 30, 480, "Items", "Y", (240, 200, 70))
btn(d, 30, 600 - 0, "Defend", "B", (230, 90, 90)) if False else None
d.text((860, 676), "", font=F(16))
d.text((20, 20), "Gone: top banner, input-hint row, full cards for idle allies", font=F(18), fill=DIM)
save(im, "3_declutter.png")

# 4 Readability: sizes
im, d = base()
cut(d, (20, 520, 400, 700), PANEL, CYAN)
d.text((38, 530), "Garrick", font=F(30, True), fill=TXT)
d.text((38, 574), "HP", font=F(20, True), fill=DIM)
bar(d, 80, 580, 220, .22, (255, 143, 180), 16)
d.text((312, 566), "18/84", font=F(26, True), fill=(255, 143, 180))
d.text((38, 616), "EP", font=F(20, True), fill=DIM)
bar(d, 80, 622, 220, .6, EP, 16)
d.text((312, 608), "24/40", font=F(26, True), fill=TXT)
for i, (s, col) in enumerate([("B", (229, 72, 58)), ("S", (166, 75, 240)), ("D", (58, 123, 234))]):
    d.rectangle((38 + i * 44, 654, 70 + i * 44, 686), fill=col)
    d.text((47 + i * 44, 656), s, font=F(22, True), fill=(255, 255, 255))
d.text((176, 658), "RB: all statuses", font=F(18), fill=DIM)
d.text((500, 140), "-142", font=F(72, True), fill=(255, 215, 90))
d.text((810, 150), "-38", font=F(40, True), fill=TXT)
d.text((30, 20), "Minimum 24 px text at 1080p · HP pulses rose below 30% · damage numbers: gold = weakness, white = normal, grey = resisted", font=F(18), fill=DIM)
save(im, "4_readability.png")

# 5 All-statuses overlay
im, d = base()
ov = Image.new("RGBA", (W, H), (0, 0, 0, 150))
im.paste(ov, (0, 0), ov)
d = ImageDraw.Draw(im)
cut(d, (300, 120, 980, 600), PANEL, CYAN)
d.text((330, 134), "Cinder Warden — statuses", font=F(28, True), fill=TXT)
for i, (n, t, col) in enumerate([("Burn", "Takes fire damage each turn · 2 turns", (229, 72, 58)), ("Shock buildup", "62 / 100 · breaks into Stun", (242, 162, 58)), ("Attack down", "-15% · 3 turns", (166, 75, 240))]):
    y = 190 + i * 100
    cut(d, (330, y, 950, y + 84), (30, 32, 40), STEEL, 10)
    d.rectangle((346, y + 18, 394, y + 66), fill=col)
    d.text((412, y + 8), n, font=F(24, True), fill=TXT)
    d.text((412, y + 44), t, font=F(19), fill=DIM)
d.text((330, 560), "< > switch character    B: close", font=F(20), fill=TXT)
save(im, "5_status_overlay.png")
print("ok")
