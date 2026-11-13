"""Journal (UIScreens.md #6): Quests (Main, Side, Secret; a secret quest is hidden until you show it, and shows only
its current step: D-171, Q-397b), Memories, Lore, Conversations. Titles and texts are placeholders: the words are
Crown's. Run: python make.py"""
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OUT, "..", "..", "_kit"))
from wor_ui import *  # noqa: E402,F401

PAGES = ["Quests", "Memories", "Lore", "Conversations"]
KIND = {"Main": GOLD, "Side": STEEL, "Secret": ELEMENT["Void"]}


def header(d, page):
    title(d, "Journal")
    x = tabs(d, 48, 96, PAGES, page, lead="LB")
    text(d, (x + 4, 103), "RB", 16, True, CYAN)


def quest_row(d, y, kind, name, sub, focus=False, done=False, tracked=False, masked=False):
    cut(d, (48, y, 560, y + 62), TEAL if focus else PLATE, CYAN if focus else STEEL, 8)
    d.rectangle((48, y + 10, 52, y + 62), fill=KIND[kind])
    text(d, (68, y + 8), name, 19, True, OFF if done else TXT)
    text(d, (68, y + 36), sub, 14, fill=(TXT if focus else DIM) if not done else OFF)
    text(d, (544, y + 12), kind, 13, True, KIND[kind] if not done else OFF, anchor="ra")
    if tracked:
        text(d, (544, y + 36), "tracked", 13, fill=CYAN, anchor="ra")


def steps(d, x, y, items):
    """A quest's steps: done ones ticked, the current one lit, later ones not shown at all for a secret quest."""
    for state, s in items:
        col = {"done": OFF, "now": TXT, "later": OFF}[state]
        mark = {"done": "✓", "now": "►", "later": "·"}[state]
        text(d, (x, y), mark, 16, True, GOOD if state == "done" else (CYAN if state == "now" else OFF))
        y = wrap(d, (x + 26, y), s, 400, 16, col, bold=(state == "now")) + 8
    return y


# ---- 1: Quests: Main and Side list every step reached; the focused one can be tracked (shows on the trial HUD).
im, d = canvas()
header(d, 0)
tabs(d, 48, 146, ["All", "Main", "Side", "Secret"], 0, h=30, size=15)
quest_row(d, 188, "Main", "The Cinder Oath", "Beat the mini-boss in the ash fields without using items.", focus=True, tracked=True)
quest_row(d, 258, "Side", "A Blade Reforged", "Bring a B-tier sword to the Blacksmith.")
quest_row(d, 328, "Secret", "Ash on the Anvil", "Wear the Ashen Blade into the Ember Trial.")
quest_row(d, 398, "Secret", "Secret Quest 2", "hidden  ·  X to show")
quest_row(d, 468, "Side", "First Steps", "done", done=True)
panel(d, (590, 146, 1232, 654), focus=True)
text(d, (612, 158), "The Cinder Oath", 26, True)
text(d, (612, 196), "Main quest", 15, True, GOLD)
y = steps(d, 612, 236, [("done", "Enter the Ember Trial."), ("done", "Find the Warden's camp."),
                        ("now", "Beat the mini-boss in the ash fields without using items."), ("later", "…")])
d.line((612, y + 6, 1212, y + 6), fill=STEEL)
text(d, (612, y + 20), "Reward", 15, True, DIM)
text(d, (612, y + 46), "Shown when the quest is done.", 15, fill=DIM)
footer(d, [("A", "Track"), ("Y", "Filter"), ("B", "Back")])
save(im, OUT, "1_quests.png")

# ---- 2: a secret quest, in both states. Hidden (as the journal opens, so a shared screen doesn't spoil it): only a
#         numbered stand-in name; X shows it (one toggle button). Shown: only the current step; no later steps, no reward, no hint of the
#         end memory; X hides it again.
def secret(name, shown):
    im, d = canvas()
    header(d, 0)
    tabs(d, 48, 146, ["All", "Main", "Side", "Secret"], 3, h=30, size=15)
    if shown:
        quest_row(d, 188, "Secret", "Ash on the Anvil", "Wear the Ashen Blade into the Ember Trial.", focus=True)
    else:
        quest_row(d, 188, "Secret", "Secret Quest 1", "hidden  ·  X to show", focus=True)
    quest_row(d, 258, "Secret", "Secret Quest 2", "hidden  ·  X to show")
    text(d, (48, 340), "Secret quests are hidden when the journal opens,", 14, fill=OFF)
    text(d, (48, 360), "so a shared screen doesn't spoil them.", 14, fill=OFF)
    panel(d, (590, 146, 1232, 654), focus=True)
    text(d, (612, 158), "Ash on the Anvil" if shown else "Secret Quest 1", 26, True)
    text(d, (612, 196), "Secret quest", 15, True, ELEMENT["Void"])
    if shown:
        y = steps(d, 612, 236, [("done", "(found)"), ("now", "Wear the Ashen Blade into the Ember Trial.")])
        d.line((612, y + 6, 1212, y + 6), fill=STEEL)
        wrap(d, (612, y + 20), "What comes next stays hidden until you reach it.", 600, 15, DIM)
        footer(d, [("A", "Select"), ("X", "Hide"), ("Y", "Hide all"), ("B", "Back")])
    else:
        text(d, (612, 236), "Hidden.", 16, True, DIM)
        footer(d, [("A", "Select"), ("X", "Show"), ("Y", "Hide all"), ("B", "Back")])
    save(im, OUT, name)


secret("2a_secret_quest_hidden.png", False)
secret("2b_secret_quest_shown.png", True)

# ---- 3: Memories: by character; a memory is a short scene you can replay. Not-yet-found ones are not listed at all.
im, d = canvas()
header(d, 1)
x = tabs(d, 48, 146, ["Everyone", "Sera", "Garrick", "Bram"], 2, h=30, size=15, lead="LT")
text(d, (x + 4, 151), "RT", 15, True, CYAN)
for i, (n, where, new) in enumerate([("The Smith's Promise", "Ember Trial · Present", True), ("A Cold Forge", "Ember Trial · Past", False),
                                     ("The Bar Night", "Void Realm", False)]):
    y = 188 + i * 70
    cut(d, (48, y, 560, y + 62), TEAL if i == 0 else PLATE, CYAN if i == 0 else STEEL, 8)
    d.rectangle((48, y + 10, 52, y + 62), fill=GOLD)
    text(d, (68, y + 8), n, 19, True)
    text(d, (68, y + 36), where, 14, fill=TXT if i == 0 else DIM)
    if new:
        text(d, (544, y + 12), "NEW", 13, True, CYAN, anchor="ra")
text(d, (48, 410), "3 found", 14, fill=OFF)
panel(d, (590, 146, 1232, 654), focus=True)
cut(d, (612, 166, 1212, 400), BG, STEEL, 10)  # the scene still / replay window
text(d, (912, 283), "memory scene", 16, fill=OFF, anchor="mm")
text(d, (612, 416), "The Smith's Promise", 24, True)
text(d, (612, 452), "Garrick  ·  found in the Ember Trial, Present", 15, fill=DIM)
wrap(d, (612, 486), "The memory's text goes here: a few lines of the scene, in the character's voice.", 600, 16)
wrap(d, (612, 560), "Unlocked: a new conversation with Sera.", 600, 15, CYAN)
footer(d, [("A", "Replay"), ("B", "Back")])
save(im, OUT, "3_memories.png")
# ---- 4: Lore: things read or heard in the world (lore objects, logged dialogue), by where they were found. A reader, no scene.
im, d = canvas()
header(d, 2)
x = tabs(d, 48, 146, ["Everything", "Void Realm", "Ember Trial"], 2, h=30, size=15, lead="LT")
text(d, (x + 4, 151), "RT", 15, True, CYAN)
for i, (n, kind, new) in enumerate([("A scorched ledger", "Note", True), ("The Warden's order", "Note", False),
                                     ("Two voices at the gate", "Overheard", False), ("Ashen Blade", "Item description", False)]):
    y = 188 + i * 70
    cut(d, (48, y, 560, y + 62), TEAL if i == 0 else PLATE, CYAN if i == 0 else STEEL, 8)
    text(d, (68, y + 8), n, 19, True)
    text(d, (68, y + 36), kind, 14, fill=TXT if i == 0 else DIM)
    if new:
        text(d, (544, y + 12), "NEW", 13, True, CYAN, anchor="ra")
panel(d, (590, 146, 1232, 654), focus=True)
text(d, (612, 158), "A scorched ledger", 24, True)
text(d, (612, 194), "Note  ·  Ember Trial, Present  ·  found by Garrick", 15, fill=DIM)
y = wrap(d, (612, 236), "The note's text goes here, set like a page: a comfortable line length, room for a few paragraphs, and it scrolls "
         "with the right stick when it runs long.", 580, 17, gap=9)
wrap(d, (612, y + 14), "A second paragraph of placeholder text so the reader's rhythm can be judged.", 580, 17, gap=9)
footer(d, [("RS", "Scroll"), ("B", "Back")])
save(im, OUT, "4_lore.png")

# ---- 5: Conversations: talks between characters that memories unlocked (Expedition 33's camp). Ready ones are played
#         in the hub; played ones can be re-read here as a script.
im, d = canvas()
header(d, 3)
tabs(d, 48, 146, ["Ready  2", "Played"], 0, h=30, size=15)
for i, (a, b, ea, eb, where) in enumerate([("Sera", "Garrick", "Darkness", "Generic", "after: The Smith's Promise"),
                                           ("Bram", "V", "Fire", "Void", "after: A Cold Forge")]):
    y = 188 + i * 78
    cut(d, (48, y, 560, y + 70), TEAL if i == 0 else PLATE, CYAN if i == 0 else STEEL, 8)
    d.ellipse((64, y + 19, 96, y + 51), fill=ELEMENT[ea])
    d.ellipse((88, y + 19, 120, y + 51), fill=ELEMENT[eb], outline=BG, width=3)
    text(d, (136, y + 10), a + " and " + b, 19, True)
    text(d, (136, y + 40), where, 14, fill=TXT if i == 0 else DIM)
    text(d, (544, y + 12), "NEW", 13, True, CYAN, anchor="ra")
text(d, (48, 356), "New talks unlock as memories return.", 14, fill=OFF)
panel(d, (590, 146, 1232, 654), focus=True)
text(d, (612, 158), "Sera and Garrick", 24, True)
text(d, (612, 194), "Ready  ·  talk to Garrick in the Void Realm", 15, True, CYAN)
wrap(d, (612, 240), "Unlocked by the memory The Smith's Promise.", 580, 16)
d.line((612, 290, 1212, 290), fill=STEEL)
text(d, (612, 304), "Once played, the talk is kept here to re-read:", 14, fill=DIM)
for i, (who, line) in enumerate([("Sera", "First line of the talk, as a script."), ("Garrick", "The answer."), ("Sera", "…")]):
    text(d, (612, 340 + i * 34), who, 15, True, OFF)
    text(d, (712, 340 + i * 34), line, 15, fill=OFF)
footer(d, [("A", "Show where"), ("B", "Back")])
save(im, OUT, "5_conversations.png")
print("ok")
