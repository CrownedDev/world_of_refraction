# Character Forms

## Overview

An authored character the player can unlock as a skin (`docs/Design/CharacterForms.md`). Each form has a **loadout-match meter**: every fight won adds credit for the pieces of the form's reference loadout the party wore. At 100% the form unlocks. Skins, outfits and the mini-boss use come with B-279b to B-279d.

## Architecture

| Class | Role |
|---|---|
| `UCharacterFormData` (`Character/Forms/CharacterFormData.h`) | `UPrimaryDataAsset`, asset type `CharacterForm` (scanned via `DefaultGame.ini`). `FormName`, `Character` (its `Inventory` is the reference loadout), `FormMesh` / `FormMaterials` (soft refs, D-057), `LoreIds`. `GetReferencePieces()`; Details-panel `PrintForm`. |
| `CharacterFormMeter` (same header) | `GetFightFill(Reference, Equipped)` = `FULL_SET_FILL_PER_FIGHT` x matched share. Each reference piece counts once: exact tier (or no tier) = 1, other tier = `OTHER_TIER_CREDIT`. Both tunables are `// TODO: tune`. |
| `UCharacterFormSubsystem` (`Character/Forms/CharacterFormSubsystem.h`) | `UGameInstanceSubsystem`. Loads every `CharacterForm` asset at start. `RecordFightWon(Fighters)` gathers each fighter's active-loadout pieces and adds the **best** fighter's fill to each locked form, capped at 1. `GetProgress`, `IsUnlocked`, `OnFormUnlocked`. |
| `UCharacterFormDebug` | `wor.Forms.Print`: every form and its meter. |

Progress is account state like the pool (saved), not per-fight state, so it sits on a game-instance subsystem for now; it moves to the player state with the co-op plumbing.

## Integration Points

- `ACombatOrchestrator::FinishCombat`: on `Victory`, `RecordFightWon(LocalPartyCombatants)` before the party arrays empty.
- `USaveSubsystem`: `FWoRSaveData::Forms` (`FSavedForms`, progress by form asset name); save version 4. Older saves load with empty meters.
- Pieces gathered per slot: gear asset at instance tier, the gear's spells, the slot's spell overrides and abilities.

## Known Limitations / TODOs

- Evolutions aren't on loadout slots yet, so they are left out of the reference too (both sides count the same pieces).
- Progress is saved by form asset name: renaming a form asset drops its saved meter.
- Every form loads at game start (`TryLoad`); move to an async load if the form count grows.
- Skills match by asset only (no instance tier yet). Who counts and how tiers credit: Q-279.
- No UI shows the meter yet; `wor.Forms.Print` and the `OnFormUnlocked` log are the way to see it.

## Outfits (B-279c)

- `Character/Outfit/OutfitPieceData.h`: `EOutfitSlot` (Helmet, Jacket, Shirt, Waist, Trousers, Footwear), `UOutfitPieceData` (primary asset type `OutfitPiece`: slot, soft mesh / material, colour keys, hides-hair), `FOutfitSet` (a soft piece per slot; `SetPiece` replaces only its slot; saved as six soft paths).
- `UCharacterFormData::AuthoredOutfit`; `UCharacterFormSubsystem` holds the player's current outfit: a skin brings its authored outfit (announced after the skin), `TryWearOutfitPiece` / `TryClearOutfitSlot` where skins switch, `RestoreOutfit` for the save (v10), `OnOutfitChanged`.
- `Character/Outfit/OutfitComponent.h`: `UOutfitComponent` wears a set on a pawn, one skeletal mesh component per filled slot attached to the body and following it by Leader Pose, the material's colour keys through a dynamic instance; an empty set hides every slot. `AWoRPlayerState::ApplyFormLook` adds it to the local player's pawn and re-applies on `OnOutfitChanged`.
- Debug: `wor.Outfit.Print`, placeholders `wor.Outfit.Wear <path>` / `wor.Outfit.Clear <slot>`, `UOutfitPieceData::PrintPiece`.
- Limits: co-op replication of the outfit (B-228+); combat actors aren't dressed; pieces load synchronously; no wardrobe screen yet.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-279: form asset, meter math, meter subsystem with victory hook, `wor.Forms.Print`, save (version 4); spec `WoR.Character.Forms` | feature/B-279-character-forms |
| 2026-11-03 | B-279b: skins. `CharacterFormSwitch::CanSwitch` (unlocked, not in a fight, hub or rest spot); `UCharacterFormSubsystem::TryWearForm` / `TrySetBaseForm` (Manny / Quinn) / `SetAtRestSpot` / `GetSkinCharacterFor` / `OnWornFormChanged`; `UCharacterFormData::GetRevealedLoadoutText`. Bound gear gives its bonus in its owner's skin (`IsBoundBonusActive(Wearer, WornSkin)`, D-033). `AWoRPlayerState` replicates `WornForm` / `BaseForm` and dresses its pawn (the default leaves the authored pawn). Save v8. Placeholder `wor.UI.Forms`; `wor.Forms.Unlock`, `wor.Forms.AtRestSpot`. Spec `WoR.Character.FormSkins` | feature/B-279b-form-skins |
| 2026-11-03 | B-279d: forms as mini-bosses. `UEncounterData::MiniBossForm` puts the form's character first in the roster (cap 3) and `UEncounterData::GetFightRank` raises the fight to at least MiniBoss; the character fights with its authored loadout; boss phases come from any `UBossData` naming it. `UCharacterFormSubsystem::FindFormForCharacter`; `ABattleGameMode` dresses such an enemy with `UCharacterFormData::DressMesh` (shared with the skin look). Spec `WoR.Character.FormMiniBoss` | feature/B-279d-form-minibosses |
| 2026-11-03 | B-279c: outfits. `EOutfitSlot` (six slots), `UOutfitPieceData` (slot, soft mesh / material, colour keys, hides-hair; asset type `OutfitPiece`), `FOutfitSet` (a piece per slot; `SetPiece` replaces only its slot; `ToSaved` / `FromSaved`) in `Character/Outfit/OutfitPieceData.h`; `UCharacterFormData::AuthoredOutfit`; `UCharacterFormSubsystem` current outfit (a skin brings its authored outfit; `TryWearOutfitPiece` / `TryClearOutfitSlot` where skins switch; `OnOutfitChanged`); `UOutfitComponent` wears it by Leader Pose, added by `AWoRPlayerState::ApplyFormLook` for the local player (co-op replication TODO); `wor.Outfit.Print`; spec `WoR.Character.Outfit`. Unbuilt (cloud). | feature/B-279c-outfits |
