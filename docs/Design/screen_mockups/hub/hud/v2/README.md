# Hub and trial HUD v2 (2026-11-15; D-216, D-217, D-222 the quick strip, Q-503l)

Replaces `../1_hub_hud_prompt.png` for the HUD. The talk and recruit pictures there stand. Values and names are placeholders. Run `python make.py` to redraw.

| Picture | Shows |
|---|---|
| `1_hub.png` | Hub: name-only cards, each shrunk to fit its name. The played character (Garrick) is the first card. No HP, EP, status or strip. Place name, money chip, interact prompt. |
| `2_trial.png` | Trial: cards with the name, HP and EP, the numbers on the bars (no build-up: battle only, D-216). Garrick is played, so he is the first card, with his strip reading **Fire Sword**. |
| `3_trial_strip_open.png` | **RB held: the quick strip** (D-222). A row of Garrick's gear as icon tiles through the screen centre: Ashen Blade (a sword, **IN HAND**), Flame Sword and Void Shield (a pair: both icons on one tile), Storm Ring, Ironheart (his evolution). The pair is highlighted: framed, a little bigger, with ▲ ▼ for the source. Under the row: **"Flame Sword and Void Shield (Fire)"**, and on a quieter line the held type, "Sword and Shield". The world dims behind. |
| `3b_trial_strip_ring.png` | The same strip with **Storm Ring** highlighted: the text changes to **"Storm Ring (Lightning)"**, with "Ring · an energy pulse" under it. |
| `3_trial_wheel_open.png` | **Superseded by the quick strip (D-222), kept for the record.** RB held: the wheel opens above the strip. One slice per **loadout entry**: Sword, Hammer, Storm Ring, Ironheart. Garrick is a Generic, so he has no innate entry (Q-503). The stick is on Sword. The centre shows **◄ Fire ►**, the element the D-pad has set, and below it "Fire Sword", what the strip will read. |
| `4_trial_caster.png` | Sera, a Refractor on the Cinder Staff's Garnet, played and first. No weapon line: the strip reads **Fire**. |

## What changed from v1

- **One card, the battle card's style** (D-216): it uses the battle HUD's name, HP bar and EP bar (`combat/hud/refine_final`). The hub shows the name only; the trial adds HP and EP; the battle shows everything, the status build-up included (Crown, D-216). The v1 hub card's red HP line is gone.
- **The played character's card is always first** (Crown, D-217), so no card is highlighted. In the trial the strip above it also marks it.
- **The field attack strip** (D-217), trial only, directly above the first card. It shows the element diamond, the name, and an RB key cap.
  - Strip names (Q-503l): "Sword", "Fire Sword", just "Fire" for a Refractor or Resonator, "Unknown" or "(R) Fire" for Reality, "Dark Fire" for Broken Darkness.
- **The quick strip on RB, held** (Crown, D-222; replaces the round wheel, which was too compressed). A tap of RB still attacks. It shows one tile per piece of gear (each weapon or pair, each ring or adapter, the innate, the evolution), each with its icon. The piece in hand is marked **IN HAND**.
  - **Left / right** (D-pad or left stick) moves along the pieces. **Up / down** cycles the source (the element) of the highlighted piece. **A** picks, **B** closes.
  - The source is **never a tile**: it is text under the row, the piece's name with the source in brackets ("Excalibur (Fire)"), or just the name when plain. Under that, on a quieter line, is the held type ("Sword and Shield").
  - Keyboard (D-222): A / D or the mouse wheel moves along the pieces, W / S changes the source, a click picks.
- **Field key hints**, bottom right: **RB Attack** (a tap; left mouse), **RB hold Gear** (the quick strip; hold R), **X Interact** (E). While the strip is open: **◄ ► Piece**, **▲ ▼ Source**, **A Pick**, **B Close**, **RB Attack**. The gamepad names are shown; the keyboard names swap in live (B-481).
- **Kept:** the place name (top left), the money chip (top right, which fades in on a change or near a vendor; it shows in the trial too), and the interact prompt by the thing you face.

## Choices made here (reference-backed, Crown may change)

- **The highlighted tile grows a little and gets a double violet frame, with ▲ ▼ above and below it.** Breath of the Wild's quick-select frames the item under the cursor and shows its name under the row (D-222's reference). The arrows stand in for BotW's short vertical column: they show that up / down is live, without drawing the sources as tiles.
- **IN HAND is a small tag on the tile's bottom edge.** BotW marks the equipped item on its tile in the same way, so the player can see what they hold and what they are looking at.
- **A pair is one tile with both icons.** It is one loadout entry, so it is one stop on the strip.
- **The world dims behind the open strip, and the cards stay.** BotW slows and darkens the world behind its quick-select.
- **Picture 3 is saved in full colour**, because the icon art bands in the 128-colour palette the other mockups use.
- **The numbers sit on the bars, at the right end** (as the battle card's HP reads). Final Fantasy XVI's party bars do the same: they can be read without moving the eye.

## For the build job

- InputMap: the quick strip on RB (hold; left / right piece, up / down source, A pick, B close) needs its row when it is built, with RB tap staying the attack.
- Icons: the weapon tiles use `T_Icon_Weapon_*` (originals in `SourceArt/UI/Deckbuilder/weapons/`). The **ring and shield have no `T_Icon_*` yet**: the picture uses PONETI's `64_ring.png` and `17_Shield.png` from the deckbuilder clone, and the build job adds them to the icon table. The evolution tile uses its element badge (`SourceArt/UI/Deckbuilder/badges/element_earth.png`).
- Not drawn: the battle level of the card (D-216).
