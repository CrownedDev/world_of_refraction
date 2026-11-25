"""Journal v2 (Crown, 2026-11-25): the journal in the approved picker layout (menus/loadout/v5).
Pages on top (Quests, Memories, Lore, Conversations). Left: the page's list with a kind selector; middle: the focused
entry, always open (a quest's steps; a secret quest shows only its current step, D-171); right: Details, its status,
where it happens, what it gives and the actions. Titles and texts are placeholders: the words are Crown's.
Run: python make.py"""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "picker_v5", os.path.join(HERE, "..", "..", "loadout", "v5", "make.py"))
_v5 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_v5)
globals().update({k: v for k, v in vars(_v5).items() if not k.startswith("__")})  # v5 + v4 art + the kit
OUT = HERE

KIND = {"Main": GOLD, "Side": STEEL, "Secret": ELEMENT["Void"]}


def journal_top(d, page):
    d.text((48, 26), "Journal", font=T(30), fill=TXT)
    x = tabs(d, 48, 78, ["Quests", "Memories", "Lore", "Conversations"], page, lead="Q")
    text(d, (x + 4, 85), "E", 16, True, CYAN)


def list_left(d, title_l, count, kinds, lit, rows, focus_i):
    panel(d, (GX0, TOP, GX1, BOTTOM))
    text(d, (GX0 + 18, TOP + 14), title_l, 17, True)
    text(d, (GX1 - 16, TOP + 16), count, 13, True, DIM, anchor="ra")
    kind_selector(d, GX0 + 16, TOP + 44, kinds, lit)
    y = TOP + 92
    for i, (kind, name, sub, done, tracked) in enumerate(rows):
        focus = i == focus_i
        box = (GX0 + 16, y, GX1 - 16, y + 56)
        cut(d, box, TEAL if focus else PLATE, CYAN if focus else STEEL, 7)
        d.rectangle((box[0], y + 10, box[0] + 4, y + 56), fill=KIND.get(kind, STEEL))
        text(d, (box[0] + 18, y + 8), name, 16, True, OFF if done else TXT)
        text(d, (box[0] + 18, y + 32), sub, 13, fill=OFF if done else (TXT if focus else DIM))
        text(d, (box[2] - 12, y + 9), kind, 12, True, OFF if done else KIND.get(kind, STEEL), anchor="ra")
        if tracked:
            text(d, (box[2] - 12, y + 33), "tracked", 12, True, CYAN, anchor="ra")
        y += 63


def steps_middle(d, name, line, line_col, steps, blurb):
    panel(d, (DX0, TOP, DX1, BOTTOM))
    text(d, (DX0 + 20, TOP + 14), name, 22, True)
    text(d, (DX0 + 20, TOP + 46), line, 14, True, line_col)
    y = TOP + 84
    for state, s in steps:
        mark = {"done": "✓", "now": "►", "later": "·"}[state]
        text(d, (DX0 + 20, y), mark, 16, True, GOOD if state == "done" else (CYAN if state == "now" else OFF))
        y = wrap(d, (DX0 + 46, y), s, DX1 - DX0 - 70, 15, TXT if state == "now" else OFF, bold=state == "now", gap=5) + 8
    y += 6
    d.line((DX0 + 20, y, DX1 - 20, y), fill=(52, 54, 62))
    wrap(d, (DX0 + 20, y + 14), blurb, DX1 - DX0 - 40, 15, DIM, gap=5)


def facts_right(d, sub, rows, actions, note=None):
    panel(d, (SX0, TOP, SX1, BOTTOM))
    text(d, (SX0 + 18, TOP + 14), "Details", 17, True)
    text(d, (SX0 + 18, TOP + 38), sub, 14, fill=DIM)
    y = TOP + 70
    for label, value, col in rows:
        text(d, (SX0 + 18, y), label, 15, fill=DIM)
        text(d, (SX1 - 18, y), value, 15, True, col, anchor="ra")
        y += 25
    y += 10
    for key, verb, primary, dim in actions:
        action_button(d, (SX0 + 16, y, SX1 - 16, y + 38), key, verb, primary, dim)
        y += 46
    if note:
        wrap(d, (SX0 + 18, y + 2), note, SX1 - SX0 - 36, 14, DIM, gap=4)


def screen_quests():
    im, d = canvas("world")
    journal_top(d, 0)
    rows = [("Main", "The Cinder Oath", "Beat the mini-boss without items", False, True),
            ("Side", "A Blade Reforged", "Bring a B-tier sword to the Blacksmith", False, False),
            ("Secret", "Ash on the Anvil", "Wear the Ashen Blade into the Ember Trial", False, False),
            ("Secret", "Secret quest", "hidden  ·  Y to show", False, False),
            ("Side", "First Steps", "done", True, False)]
    list_left(d, "Quests", "4 open  ·  1 done", ["All", "Main", "Side", "Secret"], 0, rows, 0)
    steps_middle(d, "The Cinder Oath", "Main quest", GOLD,
                 [("done", "Enter the Ember Trial."), ("done", "Find the Warden's camp."),
                  ("now", "Beat the mini-boss in the ash fields without using items."), ("later", "…")],
                 "The Warden swore the oath first. Placeholder text: the words are Crown's.")
    facts_right(d, "The Cinder Oath",
                [("Status", "Step 3 of 4", TXT), ("Where", "Ember Trial", TXT), ("Tracked", "Yes, on the HUD", CYAN),
                 ("Reward", "shown when done", OFF)],
                [("A", "Stop tracking", True, False), ("Y", "Hide all secret quests", False, False)])
    footer(d, [("A", "Track"), ("‹ ›", "Kind"), ("Y", "Show / hide secrets"), ("B", "Back")])
    return im


def screen_memories():
    im, d = canvas("world")
    journal_top(d, 1)
    rows = [("Sword", "The First Cut", "Arming Sword  ·  unlocked", False, False),
            ("Sword", "Cinder Edge's Oath", "Cinder Edge  ·  1 of 3 conditions", False, False),
            ("Crystal", "A Garnet's Warmth", "Garnet  ·  locked", True, False)]
    list_left(d, "Memories", "1 of 3 unlocked", ["All", "Gear", "Characters"], 0, rows, 1)
    steps_middle(d, "Cinder Edge's Oath", "Gear memory  ·  Cinder Edge", GOLD,
                 [("done", "Upgrade it to C."), ("now", "Wear it as its owner."), ("later", "Face the Warden with it.")],
                 "A memory bound to this blade. It plays once every condition is met. Placeholder text.")
    facts_right(d, "Cinder Edge's Oath",
                [("Status", "1 of 3 met", TXT), ("Bound to", "Cinder Edge", TXT), ("Owner", "Garrick", TXT),
                 ("Unlocks", "a passive when done", OFF)],
                [("A", "Show the gear", True, False), ("Y", "Replay", False, True)],
                note="Replay opens once the memory has played.")
    footer(d, [("A", "Show gear"), ("‹ ›", "Kind"), ("Y", "Replay"), ("B", "Back")])
    return im


if __name__ == "__main__":
    save(screen_quests(), OUT, "1_quests.png")
    save(screen_memories(), OUT, "2_memories.png")
    print("ok")
