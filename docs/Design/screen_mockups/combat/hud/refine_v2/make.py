"""Combined battle-HUD mockup, refine_v2 (flat layout only, Windows fonts). Run: python make.py"""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 1280, 720
OUT = os.path.dirname(os.path.abspath(__file__))
F = lambda s, b=False: ImageFont.truetype("C:/Windows/Fonts/%s.ttf" % ("segoeuib" if b else "segoeui"), s)
BG, PANEL, PLATE, STEEL = (16, 18, 25), (20, 22, 28), (46, 46, 46), (140, 144, 152)
TEAL, CYAN, TXT, DIM = (51, 127, 141), (159, 236, 245), (238, 241, 245), (169, 175, 184)
ROSE, ROSE_LOW, EP = (224, 64, 127), (255, 143, 180), (111, 214, 228)
ENEMIES = [(420, 400, 58, "Ember Imp"), (720, 370, 92, "Cinder Warden"), (1020, 410, 56, "Ember Imp")]


def cut(d, box, fill, outline=None, c=14):
    x0, y0, x1, y1 = box
    d.polygon([(x0 + c, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0 + c)], fill=fill, outline=outline)


def bar(d, x, y, w, frac, col, h=14):
    d.rectangle((x, y, x + w, y + h), fill=(42, 45, 53))
    d.rectangle((x, y, x + int(w * frac), y + h), fill=col)


def base(name="Sera", cls="Darkness Refractor:", allies=(("Garrick", .8, "67/84"), ("Bram", .9, "87/97"))):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    for x, y, r, _ in ENEMIES:  # no HP bars: enemy HP is hidden until targeted
        d.ellipse((x - r, y - r, x + r, y + r), fill=(8, 6, 8))
        for s in (-1, 1):
            d.ellipse((x + s * r * .33 - 7, y - r * .25 - 7, x + s * r * .33 + 7, y - r * .25 + 7), fill=(255, 110, 50))
    for i, ch in enumerate("SEGEB"):  # turn order, top right
        x = 1010 + i * 50
        d.regular_polygon((x + 22, 44, 22), 6, fill=PANEL, outline=CYAN if i == 0 else STEEL)
        d.text((x + 15, 30), ch, font=F(18, True), fill=TXT if i == 0 else DIM)
    d.text((1010, 76), "Next turns", font=F(16), fill=DIM)
    # active card, bottom left
    cut(d, (20, 548, 400, 704), PANEL, CYAN)
    d.text((38, 554), name, font=F(28, True), fill=TXT)
    d.text((38, 596), "HP", font=F(20, True), fill=DIM)
    bar(d, 80, 602, 210, .22, ROSE_LOW)
    d.text((302, 588), "18/84", font=F(26, True), fill=ROSE_LOW)
    d.text((38, 630), "EP", font=F(20, True), fill=DIM)
    bar(d, 80, 636, 210, .75, EP)
    d.text((302, 622), "30/40", font=F(26, True), fill=TXT)
    d.text((38, 666), cls, font=F(20, True), fill=DIM)
    for i, (s, col) in enumerate([("B", (229, 72, 58)), ("S", (166, 75, 240)), ("D", (58, 123, 234))]):
        x = 50 + int(d.textlength(cls, font=F(20, True))) + i * 38
        d.rectangle((x, 666, x + 28, 694), fill=col)
        d.text((x + 7, 666), s, font=F(20, True), fill=(255, 255, 255))
    # idle allies: name + HP only
    for i, (n, hp, v) in enumerate(allies):
        x = 420 + i * 250
        cut(d, (x, 652, x + 230, 704), PANEL, STEEL, 10)
        d.text((x + 12, 654), n, font=F(20, True), fill=DIM)
        d.text((x + 150, 656), v, font=F(18, True), fill=DIM)
        bar(d, x + 12, 686, 206, hp, ROSE, 8)
    return im, d


def menu(d, focus):
    items = [("Attack", "A", (110, 200, 110)), ("Skills", "X", (90, 140, 240)), ("Items", "Y", (240, 200, 70)), ("Defend", "B", (230, 90, 90))]
    for i, (lab, g, c) in enumerate(items):
        y = 296 + i * 60
        f = i == focus
        cut(d, (20, y, 280, y + 52), TEAL if f else PLATE, CYAN if f else STEEL)
        d.ellipse((32, y + 12, 60, y + 40), outline=c, width=3)
        d.text((40, y + 13), g, font=F(16, True), fill=c)
        d.text((76, y + 9), lab, font=F(24, True), fill=TXT)


# A: command state, Skills open with the detail panel
im, d = base()
menu(d, 1)
cut(d, (300, 236, 660, 536), PANEL, STEEL)
d.text((320, 244), "Skills", font=F(22, True), fill=DIM)
rows = [("Shade Lance", "6 EP", (167, 155, 194)), ("Night Veil", "4 EP", (167, 155, 194)), ("Arc Bolt", "8 EP", (242, 162, 58)), ("Mend", "10 EP", (79, 201, 138))]
for i, (n, c, col) in enumerate(rows):
    y = 286 + i * 58
    cut(d, (320, y, 640, y + 50), TEAL if i == 0 else PLATE, CYAN if i == 0 else STEEL, 10)
    d.ellipse((332, y + 14, 354, y + 36), fill=col)
    d.text((366, y + 9), n, font=F(24, True), fill=TXT)
    d.text((566, y + 11), c, font=F(20, True), fill=EP)
cut(d, (680, 236, 1180, 420), PANEL, STEEL)
d.text((700, 246), "Shade Lance", font=F(28, True), fill=TXT)
d.text((700, 288), "One enemy · Darkness · 6 EP", font=F(20), fill=DIM)
d.text((700, 324), "Hurls a lance of shadow.", font=F(22), fill=TXT)
d.text((700, 368), "EP  30 -> 24", font=F(24, True), fill=EP)
im.save(os.path.join(OUT, "a_command.png"))

# B: target state. Only the targeted enemy shows name + HP, on the marker
im, d = base()
x, y, r, name = ENEMIES[1]
cut(d, (x - 170, y - r - 138, x + 170, y - r - 44), PANEL, CYAN)
d.text((x - 152, y - r - 134), name, font=F(26, True), fill=TXT)
d.text((x + 70, y - r - 128), "412/600", font=F(20, True), fill=TXT)
d.text((x - 152, y - r - 98), "Rank 2 · Tier C", font=F(18), fill=DIM)
bar(d, x - 152, y - r - 68, 304, .68, ROSE, 12)
d.polygon([(x, y - r - 8), (x - 18, y - r - 36), (x + 18, y - r - 36)], fill=CYAN)
d.text((ENEMIES[0][0] - 110, 370), "<", font=F(54, True), fill=CYAN)
d.text((ENEMIES[2][0] + 80, 380), ">", font=F(54, True), fill=CYAN)
cut(d, (20, 480, 400, 532), TEAL, CYAN)
d.text((36, 489), "Shade Lance  ·  choose a target", font=F(22, True), fill=TXT)
d.text((440, 606), "< > / Stick / Mouse: target     A / Click: confirm     B: back", font=F(20), fill=TXT)
im.save(os.path.join(OUT, "b_target.png"))
print("ok")
