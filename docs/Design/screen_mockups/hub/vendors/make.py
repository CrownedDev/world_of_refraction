"""Vendors (UIScreens.md #15-18): one shop screen for every vendor (stock, cart, buy for), with each vendor's
services as extra tabs: Blacksmith (Dismantle, Merge), Jeweler (Item slots), Spiritualist (Repair, Clear a curse).
Vendor names from WoRGameNames.cpp. Item-slot price: 200 + 100 per slot bought (ItemSlotUpgrade.h).
Repair price by tier: 5 10 15 25 40 60 90 (GearRepair.h). Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401


def header(d, vendor, sub, tab_names, lit, gold="8,420"):
    title(d, vendor, sub)
    x = tabs(d, 48, 108, tab_names, lit, lead="LB")
    text(d, (x + 4, 115), "RB", 16, True, CYAN)
    x = chip(d, 900, 44, "Gold", gold, GOLD)
    chip(d, x, 44, "Gear Essence", "112", STEEL)


def stock_row(d, y, name, kind, el, tier, price, focus=False, qty=0, cant=None):
    cut(d, (48, y, 640, y + 58), TEAL if focus else PLATE, CYAN if focus else STEEL, 8)
    icon(d, (58, y + 6, 104, y + 52), ELEMENT[el])
    text(d, (118, y + 6), name, 19, True, OFF if cant else TXT)
    text(d, (118, y + 32), cant or kind, 14, fill=WARN if cant else (TXT if focus else DIM))
    tier_badge(d, 470, y + 29, tier, 12)
    text(d, (624, y + 29), price, 18, True, OFF if cant else TXT, anchor="rm")
    if qty:
        text(d, (530, y + 29), "x%d" % qty, 16, True, CYAN, anchor="rm")


# ---- 1: Buy. Stock left, the focused item and the cart right. "Buying for" switches who receives (D-111).
im, d = canvas("world")
header(d, "Blacksmith", "weapons, augment stones", ["Buy", "Sell", "Dismantle", "Merge"], 0)
stock = [("Ashen Blade", "Sword · Fire", "Fire", "B", "620", True, 1, None), ("Quarry Maul", "Hammer · Earth", "Earth", "C", "380", False, 0, None),
         ("Gale Spear", "Spear · Wind", "Wind", "C", "400", False, 1, None), ("Cinder Fists", "Gauntlets", "None", "D", "210", False, 0, None),
         ("Night Scythe", "Scythe · Darkness", "Darkness", "A", "1,400", False, 0, "Garrick already owns one"),
         ("Edge Stone", "Augment stone · Raw Damage", "Generic", "C", "260", False, 0, None)]
for i, r in enumerate(stock):
    stock_row(d, 160 + i * 66, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7])
text(d, (48, 562), "6 of 14  ▼", 14, fill=OFF)
panel(d, (670, 160, 1232, 400), focus=True)
text(d, (690, 170), "Ashen Blade", 24, True)
tier_badge(d, 690 + int(d.textlength("Ashen Blade", font=F(24, True))) + 20, 185, "B")
text(d, (690, 204), "Sword · Fire", 15, fill=DIM)
for i, (lab, val, cmp_) in enumerate([("Power", "42", "+14 vs Garrick's sword"), ("Scaling", "Body B · Mind D", ""), ("Abilities", "Cleave, Whirl, Guard Break +3", "")]):
    text(d, (690, 236 + i * 28), lab, 15, fill=DIM)
    text(d, (800, 236 + i * 28), val, 15, True)
    text(d, (1212, 236 + i * 28), cmp_, 14, fill=GOOD, anchor="ra")
cut(d, (690, 334, 1212, 384), PLATE, STEEL, 8)
text(d, (706, 359), "Buying for", 15, fill=DIM, anchor="lm")
text(d, (1196, 359), "‹  Garrick  ›", 17, True, CYAN, anchor="rm")
panel(d, (670, 414, 1232, 654))
text(d, (690, 424), "Cart", 16, True, DIM)
for i, (n, who, p) in enumerate([("Ashen Blade", "for Garrick", "620"), ("Gale Spear", "for yourself", "400")]):
    text(d, (690, 454 + i * 30), n, 16, True)
    text(d, (900, 454 + i * 30), who, 14, fill=DIM)
    text(d, (1212, 454 + i * 30), p, 16, True, anchor="ra")
d.line((690, 530, 1212, 530), fill=STEEL)
text(d, (690, 544), "Total", 17, True)
text(d, (1212, 542), "1,020 Gold", 20, True, GOLD, anchor="ra")
text(d, (690, 580), "After purchase", 15, fill=DIM)
text(d, (1212, 580), "7,400 Gold", 15, True, anchor="ra")
cut(d, (690, 606, 1212, 644), TEAL, CYAN, 8)
text(d, (951, 625), "X   Purchase", 17, True, anchor="mm")
footer(d, [("A", "Add"), ("Y", "Buying for"), ("X", "Purchase"), ("B", "Leave")], y=694)
save(im, OUT, "1_shop_buy.png")

# ---- 2: Jeweler · Item slots: one row per party member, price rising per slot bought.
im, d = canvas("world")
header(d, "Jeweler", "adapters, item slots", ["Buy", "Sell", "Item slots"], 2)
text(d, (48, 160), "Each slot lets a character's pouch hold one more kind of item (10 to start, 20 at most).", 15, fill=DIM)
for i, (n, have, price, ok) in enumerate([("Sera", 11, "300", True), ("Garrick", 10, "200", True), ("Bram", 20, None, False)]):
    y = 196 + i * 92
    f = i == 0
    cut(d, (48, y, 760, y + 80), TEAL if f else PLATE, CYAN if f else STEEL, 10)
    text(d, (68, y + 10), n, 22, True)
    for k in range(20):
        px = 68 + k * 22
        d.rectangle((px, y + 48, px + 16, y + 64), fill=CYAN if k < have else TRACK)
    text(d, (540, y + 14), "%d of 20" % have, 16, fill=TXT if f else DIM)
    text(d, (740, y + 40), (price + " Gold") if ok else "full", 20, True, TXT if ok else OFF, anchor="rm")
panel(d, (790, 196, 1232, 460), focus=True)
text(d, (810, 208), "Buy one item slot", 20, True)
wrap(d, (810, 244), "Sera's pouch will hold 12 kinds of item.", 400, 16)
text(d, (810, 300), "Price", 15, fill=DIM)
text(d, (1212, 298), "300 Gold", 18, True, GOLD, anchor="ra")
text(d, (810, 330), "Next slot after this", 15, fill=DIM)
text(d, (1212, 330), "400 Gold", 15, True, anchor="ra")
wrap(d, (810, 380), "Item slots are sold in the hub, not during a run.", 400, 14, DIM)
footer(d, [("A", "Buy"), ("B", "Leave")])
save(im, OUT, "2_jeweler_item_slots.png")

# ---- 3: Spiritualist · Repair: the quote first (every worn piece, its wear, its price), then one button.
im, d = canvas("world")
header(d, "Spiritualist", "evolutions, crystals, repairs", ["Buy", "Sell", "Repair", "Curses"], 2)
gear = [("Ashen Blade", "Garrick", "Fire", .88, "B", 0), ("Garnet (crystal)", "Garrick · Ashen Blade", "Fire", .35, "B", 40),
        ("Storm Ring", "Garrick", "Lightning", .6, "C", 25), ("Ember Ring", "Bram", "Fire", .0, "C", 25), ("Night Staff", "Sera", "Darkness", 1.0, "D", 0)]
for i, (n, who, el, wear, t, price) in enumerate(gear):
    y = 160 + i * 66
    cut(d, (48, y, 760, y + 58), TEAL if i == 1 else PLATE, CYAN if i == 1 else STEEL, 8)
    icon(d, (58, y + 6, 104, y + 52), ELEMENT[el])
    text(d, (118, y + 6), n, 18, True)
    text(d, (118, y + 32), who, 14, fill=TXT if i == 1 else DIM)
    bar(d, 420, y + 24, 150, wear, KILL if wear == 0 else (WARN if wear < .5 else GOOD), 10)
    text(d, (580, y + 19), "broken" if wear == 0 else "%d%%" % (wear * 100), 14, True, KILL if wear == 0 else TXT)
    text(d, (744, y + 29), ("%d Gold" % price) if price else "—", 17, True, TXT if price else OFF, anchor="rm")
panel(d, (790, 160, 1232, 460), focus=True)
text(d, (810, 172), "Repair everything", 20, True)
text(d, (810, 212), "3 pieces", 15, fill=DIM)
text(d, (1212, 210), "90 Gold", 20, True, GOLD, anchor="ra")
text(d, (810, 244), "After repair", 15, fill=DIM)
text(d, (1212, 244), "8,330 Gold", 15, True, anchor="ra")
wrap(d, (810, 292), "A broken crystal gives no infusion and no elemental abilities until it's repaired. A broken adapter casts only its weakest spells.",
     400, 14, DIM)
cut(d, (810, 400, 1212, 444), TEAL, CYAN, 8)
text(d, (1011, 422), "X   Repair all  ·  90 Gold", 17, True, anchor="mm")
footer(d, [("A", "Repair this"), ("X", "Repair all"), ("B", "Leave")])
save(im, OUT, "3_spiritualist_repair.png")

# ---- 4: Spiritualist · Curses: what each curse does, what clearing costs, and that it may come back.
im, d = canvas("world")
header(d, "Spiritualist", "evolutions, crystals, repairs", ["Buy", "Sell", "Repair", "Curses"], 3)
for i, (n, who, effect, price) in enumerate([("Curse name", "Sera", "What the curse does, in the curse's own words.", "450"),
                                              ("Curse name", "Garrick", "What the curse does, in the curse's own words.", "200")]):
    y = 160 + i * 96
    cut(d, (48, y, 760, y + 84), TEAL if i == 0 else PLATE, CYAN if i == 0 else STEEL, 10)
    d.regular_polygon((80, y + 42, 18), 4, rotation=45, fill=ELEMENT["Void"])
    text(d, (116, y + 10), n, 20, True)
    text(d, (116, y + 38), who + " holds it", 14, fill=TXT if i == 0 else DIM)
    text(d, (116, y + 58), effect, 14, fill=TXT if i == 0 else DIM)
    text(d, (744, y + 42), price + " Gold", 18, True, anchor="rm")
panel(d, (790, 160, 1232, 420), focus=True)
text(d, (810, 172), "Clear this curse", 20, True)
text(d, (810, 212), "Price", 15, fill=DIM)
text(d, (1212, 210), "450 Gold", 20, True, GOLD, anchor="ra")
wrap(d, (810, 256), "It is cleared. It may come back.", 400, 16)
wrap(d, (810, 300), "Curses can be cleared in the hub, not during a run.", 400, 14, DIM)
footer(d, [("A", "Clear"), ("B", "Leave")])
save(im, OUT, "4_spiritualist_curses.png")
print("ok")
