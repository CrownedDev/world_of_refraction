"""Vendor screens v2 (Crown, 2026-11-25): the approved equipment-picker layout (menus/loadout/v5) for every shop.
Left: the vendor, its tabs and the stock as a grid; middle: the fixed details panel for the focused piece;
right: Details, what the action changes (wallet before and after, who it is for, owned count) and the cart.
No pop-ups. Values are placeholders. Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "..", "menus", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE


def vendor_top(d, vendor, sub, tab_names, lit, wallet):
    d.text((48, 28), vendor, font=T(30), fill=TXT)
    text(d, (48, 66), sub, 15, fill=DIM)
    x = tabs(d, 48, 90, tab_names, lit, lead="Q")
    text(d, (x + 4, 97), "E", 16, True, CYAN)
    x = 880
    for label, value, col in wallet:
        x = chip(d, x, 34, label, value, col)


def stock_left(d, title_l, count_label, filters, items, focus_i, legend="can't buy (why: in the details)", cart=True):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), title_l, 17, True)
    text(d, (GX1 - 16, TOP + 16), count_label, 13, True, DIM, anchor="ra")
    icon_filters(d, GX0 + 18, TOP + 46, filters, 0, "", size=30)
    cell, gap, cols = 62, 10, 5
    ty, tx = TOP + 92, GX0 + 20
    for i in range(cols * 7):
        r, c = divmod(i, cols)
        b = (tx + c * (cell + gap), ty + r * (cell + gap), tx + c * (cell + gap) + cell, ty + r * (cell + gap) + cell)
        if b[3] > BOTTOM - 34:
            break
        if i >= len(items):
            empty_tile(d, b)
            continue
        it = items[i]
        tile(d, b, it["kind"], it.get("tier"), it.get("count", 1), it.get("col"), it.get("worn", False),
             it.get("locked", False), i == focus_i, it.get("picked", False))
    scroll_bar(d, GX1 - 12, ty, BOTTOM - 40, 0.0, 0.6)
    ly = BOTTOM - 18
    lx = GX0 + 18
    if cart:
        cut(d, (lx, ly - 8, lx + 16, ly + 6), TEAL, CYAN, 3)
        text(d, (lx + 24, ly), "in the cart", 13, fill=OFF, anchor="lm")
        lx += 114
    lock(d, lx, ly - 3)
    text(d, (lx + 12, ly), legend, 13, fill=OFF, anchor="lm")


def details_right(d, sub, lines, cart, total, after):
    """Details: what the focused action changes, then the cart (scrolls in its own box), total and wallet after."""
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), sub, 14, fill=DIM)
    y = TOP + 66
    for label, old, new, better in lines:
        stat_line(d, SX0 + 18, SX1 - 18, y, label, old, new, better)
        y += 24
    if cart is None:
        return
    y += 8
    d.line((SX0 + 18, y, SX1 - 18, y), fill=(52, 54, 62))
    y += 12
    text(d, (SX0 + 18, y), "Cart", 15, True)
    text(d, (SX1 - 18, y), "%d lines" % len(cart), 13, fill=DIM, anchor="ra")
    y += 26
    box = (SX0 + 14, y, SX1 - 14, y + 150)
    cut(d, box, BG, (52, 54, 62), 6)
    cy = y + 8
    for name, price in cart[:5]:
        text(d, (SX0 + 26, cy), name, 14)
        text(d, (SX1 - 30, cy), price, 14, True, anchor="ra")
        cy += 27
    scroll_bar(d, SX1 - 22, y + 6, y + 144, 0.0, 0.8 if len(cart) > 5 else 1.0)
    y += 162
    for label, value, col in (("Total", total, TXT), ("After purchase", after, GOOD)):
        text(d, (SX0 + 18, y), label, 15, fill=DIM)
        text(d, (SX1 - 18, y), value, 15, True, col, anchor="ra")
        y += 24


GOLD_C, ESS_C = GOLD, (150, 170, 190)


def screen_buy():
    """Combat Master, Buy: sword skills as tiles; Over Under focused, two lines in the cart."""
    im, d = canvas("world")
    vendor_top(d, "Combat Master", "Sword skills  ·  buying for the Generic", ["Buy", "Sell"], 0,
               [("Gold", "9,999", GOLD_C), ("Dormant Essence", "9,999", ESS_C)])
    items = [dict(kind="Sword", tier="F", worn=True, locked=True), dict(kind="Sword", tier="E", picked=True),
             dict(kind="Sword", tier="E"), dict(kind="Sword", tier="D", picked=True), dict(kind="Sword", tier="D")]
    stock_left(d, "Stock", "5 skills", ["All", "Sword"], items, 2, legend="known or can't buy")
    middle_details(d, "Over Under", "E", "Sword", None, "Sword skill  ·  one target  ·  2 hits",
                   [("EP", "18"), ("Power", "40 + 60"), ("Targets", "One"), ("Camera", "Melee rush")],
                   [],
                   "A rising cut, then a falling one. The second hit lands harder.")
    details_right(d, "If you buy Over Under", [("Owned", "0", "1", True), ("Gold", "9,999", "9,949", None),
                                                ("Dormant Essence", "9,999", "9,986", None)],
                  [("Quick Strike", "50 G  13 E"), ("Heavy Attack", "100 G  25 E")],
                  "150 G  ·  38 E", "9,849 G  ·  9,961 E")
    footer(d, [("A", "Add to cart"), ("X", "Buy the cart"), ("R", "Buying for"), ("Y", "Sort"), ("B", "Back")])
    return im


def screen_sell():
    """Blacksmith, Sell: owned gear; a worn piece is refused with its reason."""
    im, d = canvas("world")
    vendor_top(d, "Blacksmith", "Sell gear  ·  the Generic's bag", ["Buy", "Sell", "Upgrade", "Dismantle"], 1,
               [("Gold", "9,999", GOLD_C), ("Dormant Essence", "9,999", ESS_C)])
    items = [dict(kind="Sword", tier="F", count=2), dict(kind="Axe", tier="E"), dict(kind="Shield", tier="F"),
             dict(kind="Sword", tier="S", worn=True, locked=True), dict(kind="Axe", tier="S", worn=True, locked=True),
             dict(kind="Bow", tier="F"), dict(kind="Gun", tier="F")]
    stock_left(d, "Your gear", "7 pieces", ["All", "Sword", "Axe", "Shield", "Bow"], items, 0,
               legend="worn (take it off first)", cart=False)
    middle_details(d, "Arming Sword", "F", "Sword", None, "Sword  ·  one hand  ·  2 in your bag",
                   [("Power", "40"), ("Scaling", "Body C"), ("Skill slots", "1"), ("Sells for", "12 G each")],
                   [(None, "Augment socket: empty"), (None, "Augment socket: empty")],
                   "A plain soldier's blade.")
    details_right(d, "If you sell one", [("Owned", "2", "1", None), ("Gold", "9,999", "10,011", True)],
                  None, None, None)
    footer(d, [("A", "Sell one"), ("X", "Sell all"), ("Y", "Sort"), ("B", "Back")])
    return im


def screen_upgrade():
    """Blacksmith, Upgrade (B-609): owned gear; the details show this tier and the next, the cost on the right."""
    im, d = canvas("world")
    vendor_top(d, "Blacksmith", "Upgrade  ·  one tier per confirm", ["Buy", "Sell", "Upgrade", "Dismantle"], 2,
               [("Gold", "9,999", GOLD_C), ("Dormant Essence", "9,999", ESS_C)])
    items = [dict(kind="Sword", tier="F", worn=True), dict(kind="Axe", tier="S", worn=True, locked=True),
             dict(kind="Sword", tier="F", count=2), dict(kind="Axe", tier="E"), dict(kind="Adapter", tier="D", col=ELEMENT["Fire"])]
    stock_left(d, "Your gear", "5 pieces", ["All", "Sword", "Axe", ("Adapter", ELEMENT["Fire"])], items, 0, legend="already S", cart=False)
    middle_details(d, "Arming Sword", "F", "Sword", None, "Sword  ·  worn by the Generic",
                   [("Tier", "F  →  E"), ("Power", "40  →  46"), ("Skill slots", "1  →  2"), ("Dismantles for", "6  →  10 E")],
                   [],
                   "Each confirm raises it one tier. Every loadout that wears it follows.")
    details_right(d, "If you upgrade it", [("Tier", "F", "E", True), ("Dormant Essence", "9,999", "9,979", None),
                                            ("Gold", "9,999", "9,999", None)],
                  None, None, None)
    footer(d, [("A", "Upgrade"), ("Y", "Sort"), ("B", "Back")])
    return im


def screen_skill_upgrade():
    """Combat Master, Upgrade: every skill the character has learned; one tier per confirm."""
    im, d = canvas("world")
    vendor_top(d, "Combat Master", "Upgrade your skills  ·  the Generic", ["Buy", "Upgrade", "Sell"], 1,
               [("Gold", "9,999", GOLD_C), ("Dormant Essence", "9,999", ESS_C)])
    items = [dict(kind="Sword", tier="F"), dict(kind="Sword", tier="E"), dict(kind="Sword", tier="E"),
             dict(kind="Sword", tier="D"), dict(kind="Sword", tier="D"), dict(kind="Axe", tier="F"),
             dict(kind="Axe", tier="S", locked=True)]
    stock_left(d, "Learned skills", "7 skills", ["All", "Sword", "Axe"], items, 1, legend="already S", cart=False)
    middle_details(d, "Over Under", "E", "Sword", None, "Sword skill  ·  slotted on the Arming Sword",
                   [("Tier", "E  →  D"), ("Power", "40 + 60  →  46 + 69"), ("EP", "18  →  18"), ("Buildup", "5  →  6")],
                   [],
                   "Each confirm raises the skill one tier, wherever it is slotted.")
    details_right(d, "If you upgrade Over Under", [("Tier", "E", "D", True), ("Dormant Essence", "9,999", "9,974", None),
                                                    ("Gold", "9,999", "9,999", None)],
                  None, None, None)
    footer(d, [("A", "Upgrade"), ("Y", "Sort"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_buy(), OUT, "1_buy.png")
    save(screen_sell(), OUT, "2_sell.png")
    save(screen_upgrade(), OUT, "3_upgrade.png")
    save(screen_skill_upgrade(), OUT, "4_skill_upgrade.png")
    print("ok")
