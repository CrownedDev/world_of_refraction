# Hub and trial HUD v2 (2026-11-15; D-216, D-217 with "The wheel on RB", Q-503l)

Replaces `../1_hub_hud_prompt.png` for the HUD. The talk and recruit pictures there stand. Values and names are placeholders. Run `python make.py` to redraw.

| Picture | Shows |
|---|---|
| `1_hub.png` | Hub: name-only cards, each shrunk to fit its name. The played character (Garrick) is the first card. No HP, EP, status or strip. Place name, money chip, interact prompt. |
| `2_trial.png` | Trial: full cards (HP and EP with the numbers on the bars, then the build-up bar with a quiet "Build-up" label). Garrick is played, so he is the first card, with his strip reading **Fire Sword**. |
| `3_trial_wheel_open.png` | RB held: the wheel opens above the strip. One slice per **loadout entry**: Sword, Hammer, Storm Ring, Ironheart. Garrick is a Generic, so he has no innate entry (Q-503). The stick is on Sword. The centre shows **◄ Fire ►**, the element the D-pad has set, and below it "Fire Sword", what the strip will read. |
| `4_trial_caster.png` | Sera, a Refractor on the Cinder Staff's Garnet, played and first. No weapon line: the strip reads **Fire**. |

## What changed from v1

- **One card, the battle card's style** (D-216): it uses the battle HUD's name, HP bar and EP bar (`combat/hud/refine_final`). The hub shows the name only; the trial adds HP, EP and build-up; the battle shows everything. The v1 hub card's red HP line is gone.
- **The played character's card is always first** (Crown, D-217), so no card is highlighted. In the trial the strip above it also marks it.
- **The field attack strip** (D-217), trial only, directly above the first card. It shows the element diamond, the name, and an RB key cap.
  - Strip names (Q-503l): "Sword", "Fire Sword", just "Fire" for a Refractor or Resonator, "Unknown" or "(R) Fire" for Reality, "Dark Fire" for Broken Darkness.
- **The wheel is on RB, held** (Crown, D-217); a tap of RB still attacks. Its slices are the loadout entries (each weapon, each ring, the evolution, the innate) with the element diamond of each one's current element.
  - The **left stick** moves around the wheel. **D-pad ◄ ►** changes the element of the entry under the stick, where it has more than one (a weapon's infusion, "(R) Fire", "Dark Fire"). **Letting go** confirms.
  - Keyboard (D-217): hold R, the mouse picks a slice, the mouse wheel or A / D changes the element.
- **Field key hints**, bottom right: **RB Attack** (a tap; left mouse), **RB hold Change source** (the wheel; hold R; D-217 "RB, tap and hold"), **X Interact** (E). While the wheel is open: **L-stick Pick**, **D-pad ◄ ► Element**, **RB Let go: confirm**. The gamepad names are shown; the keyboard names swap in live (B-481).
- **Kept:** the place name (top left), the money chip (top right, which fades in on a change or near a vendor; it shows in the trial too), and the interact prompt by the thing you face.

## Choices made here (reference-backed, Crown may change)

- **The element is shown in the centre with arrows**, not on the slice. The centre has room for "◄ Fire ►" plus the result ("Fire Sword"), so the slices keep just the entry's name. Doom Eternal's and GTA V's wheels (D-217's reference) name the highlighted choice in the centre; GTA V also puts the second choice there (its ammo or attachment, changed with the D-pad), which is the same job as the element here.
- **Each slice's diamond shows that entry's current element**, so the wheel shows at a glance what each entry would throw.
- **The numbers sit on the bars, at the right end** (as the battle card's HP reads). Final Fantasy XVI's party bars do the same: they can be read without moving the eye.
- **The build-up label is the word "Build-up" in the dimmest text colour**, with the bar coloured by its element's status. Note from D-216: SB-18 empties build-up after every fight, so between fights this bar is normally empty. The pictures show some build-up only to show the look.

## For the build job

- InputMap: the wheel on RB (hold; the stick picks, D-pad ◄ ► changes the element, letting go confirms) needs its row when it is built, with RB tap staying the attack. The current rows say RB attack and LB source.
- Not drawn: the battle level of the card (D-216).
