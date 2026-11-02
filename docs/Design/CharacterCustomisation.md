# Character Customisation (outfit slots)

**Status:** decided in outline (Crown, 2026-10-29); details below marked open. Companion to `CharacterForms.md` (skins and the meter) and `CharacterPipeline.md` (VRoid route).

## 1. Outfit slots (Monster Hunter model)

Every form (the player's Manny / Quinn form and each authored character) wears **six outfit slots**, switchable piece by piece:

| Slot | Covers |
|---|---|
| Helmet | head: hats, hoods, masks, crowns (hair shows unless the piece hides it) |
| Jacket | outer layer: coats, cloaks; **the uniform coat is the upper coat only** (body, sleeves, collar) |
| Shirt | inner top |
| Waist | belts, sashes, pouches; **the uniform coat's lower half** (the skirt / tails below the belt) is a Waist piece, so the coat is **two parts: Jacket + Waist** (Crown, 2026-10-29) |
| Trousers | legs |
| Footwear | boots, shoes |

- Pieces are **components**: each is its own mesh (skinned to the shared V-template body / Manny) with material parameters (the gem colour, trims), so one coat mesh serves every colour.
- Pieces mix freely across characters once owned; a character's authored outfit is just a saved set of six.
- **Cosmetic only** (no stats): stats live in gear (weapons, rings, crystals, evolutions). Open: whether some pieces are also gear (e.g. a helmet that is a ring's visual) — leaning no.

## 2. Authoring order for named characters (Crown, 2026-10-29)

1. **Create the character first**: the plain body, face, hair, colours, from the sheet (VRoid V template, `CharacterPipeline.md`).
2. **Then create the outfit**, piece by piece in the six slots.
3. **Save each piece as a component** in the shared outfit library, and the set of six as that character's **authored outfit**. New characters reuse library pieces where they fit (the uniform coat in a new gem colour is a material instance, not a new mesh).

## 3. Player side

- The player's form (Manny / Quinn, V's material) wears any owned pieces.
- A character skin (`CharacterForms.md`) comes with its authored outfit; the player may swap pieces on it afterwards (open: whether swapping off the authored outfit affects anything, e.g. the loadout-match meter: leaning no, the meter reads gear, not outfit).
- Pieces are unlocked by finding, buying, or with a skin; open: exact sources.

## 4. Data shape (for B-279 / the dev editor)

- `UOutfitPieceData` (data asset): slot, mesh (`TSoftObjectPtr<USkeletalMesh>`), material instance + exposed colour keys, hides-hair flag, a name (FText).
- `FOutfitSet`: six soft references, one per slot; stored on `UCharacterFormData` as the authored outfit and on the player's save as the current set (D-059 explicit struct).
- Runtime: Leader Pose assembly (one skeletal mesh component per slot following the body), as the pipeline research suggests; mesh merging only if profiling demands it.
- Co-op: the current set replicates as six small ids on the PlayerState.

## 5. Open

- Piece sources (find / buy / skin); whether any piece is also gear.
- Slot names in player-facing text (Crown's).
- Whether the uniform coat is one mesh for all body shapes (V template says yes). The coat is two pieces (Jacket upper + Waist lower), so any Jacket pairs with any Waist: a different belt or tails on the same upper, or a different jacket on the same lower half.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-29 | Created from Crown's six-slot outline and authoring order | docs/character-customisation |
| 2026-10-29 | The uniform coat splits into Jacket (upper) + Waist (lower) (Crown) | docs/coat-split |
