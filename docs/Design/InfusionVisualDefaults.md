# Infusion visual defaults

The looks an infusion shows when the player owns no infusion cosmetic. Every character always has these. They are **defaults behind the scenes**, not items. Cosmetic looks that the player owns and picks replace them. For now those cosmetics are items (Crown, 2026-11-25).

Picked by Crown in the editor (B-626b, 2026-11-25). Assets are unsaved scratch copies in `/Game/QA_Scratch/B626/` until they're promoted. Details and history: `docs/PIE/qa/2026-11-25/B-626b/README.md`.

## Three places, two levels

| Place | Who | Level 2 rule |
|---|---|---|
| Weapon | infused weapon (Generic / Caster) | the same effect with more particles |
| Body | the infused character | the same effect, `Scale_All` x1.6 and every `Velocity_*` x1.5 |
| Hand | Resonator (ring / adapter), on the palm and knuckles | the same as the body version at hand size: `Scale_All` = body x0.2; L2 x1.6 size, x1.5 speed |

Colour: every layer is set to the element's colour (`Color_*` = element colour x the layer's brightness). Darkness is black, Light is white. Glowing (additive) layers can't show black, so Darkness uses dark, non-glowing layers.

## Per element

| Element | Weapon | Body (L1) | Hand (Resonator) |
|---|---|---|---|
| Fire | `NS_FireSword_Tint` | `NS_Aura25_FireOrbs`: only the fire dots (Sparks1) | body version |
| Water | `NS_BloodSword_NoSmoke` | `NS_Body_Water_Loop` | body version, floor ring off (`Scale_Wave1` 0) |
| Wind | `NS_Vortex_Tint` | `NS_Body_Wind_Loop` | body version |
| Lightning | `NS_ElectricAttract_NoSmoke` | `NS_Body_Lightning_Loop` | body version: just lightning, no bubble |
| Ice | `NS_Aura9_Loop` | `NS_Aura9_Loop`, element-tinted | body version, floor ring off (`Scale_Plane` 0), mist and sparks on |
| Earth | stand-in (`NS_Vortex_Tint`), **open** | `NS_EarthFlow_Loop`: energy-material rocks, yellow, `Scale_Stone1` 1, `Velocity_Stone1` 0.5 (L2: `Scale_Stone1` 1.3) | body version, floor ring off (`Scale_Plane` 0) |
| Void | `NS_Vortex_Tint` | `NS_Aura2_Loop`, element-tinted | body version |
| Reality | `NS_KikiAttractor_Tint`, look again later | `NS_KikiAttractor_Body`, thin streaks: Rate1 15000, Rate2 1500, Speed1 500, MeshOffset1 45 (L2: Speed1 1000) | the same effect, Speed1 100, MeshOffset1 9 |
| Light | stand-in, **open** | `NS_Aura25_Loop`, dots only, white | body version |
| Darkness | stand-in, **open** | `NS_Aura25_Loop`, black Plane2 + Wave2 | body version, floor ring off (`Scale_Wave2` 0) |

## Still to do

- Pick the Earth, Light and Darkness weapon effects. Look at Reality's weapon effect again.
- Resonator Level 2 at the hand: placed, but not yet seen working.
- Promote the scratch copies to real assets (`docs/Conventions/ContentNaming.md`), and put these defaults into data (see "Cosmetics" below).

## Cosmetics (Crown, 2026-11-25)

Crown: make infusion data assets usable for cosmetics, with the defaults above behind the scenes; for now cosmetics are items. Needs code (a data asset type per infusion look: element, weapon / body / hand effect, L1 / L2 settings), a defaults set the game falls back to, and an item that grants a look. This is a Writer job; not built yet.
