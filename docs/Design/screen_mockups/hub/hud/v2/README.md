# Hub and trial HUD v2 (2026-11-15; D-216, D-217, Q-503l)

Replaces `../1_hub_hud_prompt.png` for the HUD. The talk and recruit pictures there stand. Values and names are placeholders. Run `python make.py` to redraw.

| Picture | Shows |
|---|---|
| `1_hub.png` | Hub: name-only cards, each shrunk to fit its name. The played member has the violet plate. No HP, EP, status or strip. Place name, money chip, interact prompt. |
| `2_trial.png` | Trial: full cards (HP and EP with the numbers on the bars, then the build-up bar with a quiet "Build-up" label). Garrick is played; his strip reads **Fire Sword**. |
| `3_trial_wheel_open.png` | LB held: the wheel opens above the strip, showing Garrick's choices (Sword, Fire Sword, Earth Sword, Lightning Sword). The centre names the highlighted entry's source ("from Garnet"). |
| `4_trial_caster.png` | Sera, a Refractor on the Cinder Staff's Garnet. No weapon line: the strip reads **Fire**. |

## What changed from v1

- **One card, the battle card's style** (D-216): it uses the battle HUD's name, HP bar and EP bar (`combat/hud/refine_final`). The hub shows the name only; the trial adds HP, EP and build-up; the battle shows everything. The v1 hub card's red HP line is gone.
- **The field attack strip** (D-217), trial only, directly above the played card. It shows the element diamond, the name, and an LB key cap.
  - Strip names (Q-503l): "Sword", "Fire Sword", just "Fire" for a Refractor or Resonator, "Unknown" or "(R) Fire" for Reality, "Dark Fire" for Broken Darkness.
- **The LB wheel** (D-217): it opens on an LB hold and shows every source and element in one ring. The stick or mouse wheel picks, letting go confirms, and a tap steps to the next entry. The hints change while it is open.
- **Field key hints**, bottom right: RB Attack (left mouse), LB Infusion with "hold: wheel" (R), X Interact (E). The gamepad names are shown; the keyboard names swap in live (B-481).
- **Kept:** the place name (top left), the money chip (top right, which fades in on a change or near a vendor), and the interact prompt by the thing you face.

## Choices made here (reference-backed, Crown may change)

- **The wheel is a small ring of up to about 8 slices, each with its element diamond and name, and the source in the centre.** Doom Eternal's and GTA V's weapon wheels show the whole set around one centre, and players like never losing their place (D-217's own reference). Showing the source in the centre answers "which gear is this?" without longer slice names, as GTA V's centre panel names the highlighted weapon. Plain "Sword" sits at the top, so the wheel always starts from the same place.
- **The played member is marked by a violet edge** (in the hub, a violet plate, since there is no strip). This is the same focus colour as the menus (UI_Theme section 3). Expedition 33's field party shows the leader the same way, with the frame highlighted and no extra label.
- **The numbers sit on the bars, at the right end** (as the battle card's HP reads). The battle HUD mockups put the number at the bar's end so it can be read without moving the eye, and Final Fantasy XVI's party bars do the same.
- **The build-up label is the word "Build-up" in the dimmest text colour**, with the bar coloured by its element's status. Note from D-216: SB-18 empties build-up after every fight, so between fights this bar is normally empty. The pictures show some build-up only to show the look.
- **The money chip shows in the trial too.** It is the same chip, with the same fade-in rule as the hub.

## For the build job

- InputMap: the wheel (hold LB, the stick or mouse wheel picks, letting go confirms, a tap steps) needs its row when it is built. The current row describes the press / hold split.
- Stated in the decisions, not drawn: the battle level of the card (D-216), and the 2-second cycle flash (B-503m dropped it).
