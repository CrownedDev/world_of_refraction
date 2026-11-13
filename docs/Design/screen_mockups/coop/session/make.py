"""Co-op (UIScreens.md #31-33): invite and seat friends, a join request on the host's screen, and the host
hand-over / host-lost notice. Words follow InviteWidget.cpp, JoinSeatWidget.cpp. Run: python make.py"""
import os
import sys
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def dim(im, k=0.5):
    im = Image.blend(im, Image.new("RGB", im.size, (0, 0, 0)), k)
    return im, ImageDraw.Draw(im)


SEATS = [("Sera", "Darkness", "You (host)", CYAN), ("Garrick", "Generic", "AI seat", DIM), ("Bram", "Fire", "AI seat", DIM)]


def seats(d, x, y, seat_list, focus=None, w=440):
    text(d, (x, y - 26), "Seats", 16, True, DIM)
    for i, (n, el, who, col) in enumerate(seat_list):
        ry = y + i * 62
        cut(d, (x, ry, x + w, ry + 54), TEAL if focus == i else PLATE, CYAN if focus == i else STEEL, 8)
        icon(d, (x + 8, ry + 6, x + 50, ry + 48), ELEMENT[el])
        text(d, (x + 64, ry + 27), n, 18, True, anchor="lm")
        text(d, (x + w - 14, ry + 27), who, 16, True, col, anchor="rm")


# ---- 1: Invite and seat friends (from the party screen or a rest spot): friends left, seats right.
im, d = canvas("world")
im, d = dim(im)
title(d, "Play together", "You host a session: friends can join")
text(d, (48, 134), "Friends", 16, True, DIM)
for i, (n, state, c) in enumerate([("Kestrel", "online", GOOD), ("Dune", "invited · waiting", WARN), ("Marrow", "in another game", OFF), ("Ilex", "offline", OFF)]):
    y = 160 + i * 62
    cut(d, (48, y, 600, y + 54), TEAL if i == 0 else PLATE, CYAN if i == 0 else STEEL, 8)
    d.ellipse((66, y + 19, 82, y + 35), fill=c)
    text(d, (96, y + 27), n, 19, True, OFF if c == OFF else TXT, anchor="lm")
    text(d, (584, y + 27), state, 15, fill=TXT if i == 0 else DIM, anchor="rm")
text(d, (48, 420), "A friend brings their own character and takes an AI seat.", 14, fill=DIM)
text(d, (48, 442), "In a run, new friends are seated at a rest spot.", 14, fill=DIM)
seats(d, 680, 160, SEATS)
cut(d, (680, 360, 1120, 410), PLATE, STEEL, 8)
text(d, (698, 385), "Stop hosting", 17, True, anchor="lm")
footer(d, [("A", "Invite"), ("X", "Seats"), ("B", "Back")])
save(im, OUT, "1_invite.png")

# ---- 2: a join request, on the host's screen, anywhere: a small card, not a full screen. Pick the seat or refuse.
im, d = canvas("world")
panel(d, (760, 60, 1232, 366), focus=True)
text(d, (780, 72), "Kestrel wants to join", 22, True)
text(d, (780, 106), "with Vael · Wind Refractor", 15, fill=DIM)
tier_badge(d, 1206, 86, "B")
seats(d, 780, 164, [("Garrick", "Generic", "AI seat · give", CYAN), ("Bram", "Fire", "AI seat · give", DIM)], focus=0, w=432)
text(d, (780, 300), "Vael replaces the seat's character for this run.", 14, fill=DIM)
text(d, (780, 330), "Hold View to answer  ·  then  A  Give this seat   X  Refuse", 14, fill=CYAN)
text(d, (48, 40), "In the world (host's screen)", 15, fill=OFF)
save(im, OUT, "2_join_request.png")

# ---- 3: the host left without handing over: everyone sees this while the next player takes over.
im, d = canvas("world")
im, d = dim(im, 0.7)
panel(d, (360, 220, 920, 480), focus=True)
text(d, (640, 250), "The host left", 28, True, anchor="ma")
text(d, (640, 300), "Kestrel is taking over as host.", 18, anchor="ma")
for i in range(4):  # the hand-over's steps as pips
    d.regular_polygon((592 + i * 32, 360, 9), 4, rotation=45, fill=CYAN if i < 2 else TRACK)
text(d, (640, 386), "Reopening the trial for everyone…", 15, fill=DIM, anchor="ma")
text(d, (640, 430), "Your run, gear and progress are kept.", 15, fill=GOOD, anchor="ma")
footer(d, [("B", "Leave to the Void Realm")])
save(im, OUT, "3_host_lost.png")
print("ok")
