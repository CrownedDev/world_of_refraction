# Save system (persistence keystone)

## Overview

One save slot per local player (`WoR_Main`) keeps what lives between play sessions: the party, the player's banked currencies and the shared pool's owned items. It is built the standard Unreal way (a `ULocalPlayerSaveGame` subclass with a version stamp), with explicit save structs rather than `SaveGame` property tags (D-059). Until the first public build, a save below the minimum version is wiped with a warning (D-060). **One save version while in dev (D-138):** `MIN_SUPPORTED` equals `LATEST`, every format change bumps both, and no old-shape reader or migration is kept; no class property carries a `SaveGame` tag (B-348).

## Architecture

- `Save/WoRSaveTypes.h`: the save structs, plain data with no pointers.
  - `FSavedAssetRef`: an asset as `FPrimaryAssetId` plus its path. `Resolve` tries the id through the Asset Manager first (it survives an asset move), then the path.
  - `FSavedParty` / `FSavedPartyMember`: character, pawn class, formation cell, Player/AI control, carried HP/EP; during a run also crystal wear and absorption (B-208).
  - `FSavedWallet` / `FSavedBalance`: every banked currency (Prisms, Diamond, Gear/Skill Essence, the 14 typed essences). Gold is never banked.
  - `FSavedPool` / `FSavedOwnedItem` / `FSavedCrystalStack`: the pool's weapons, rings, evolutions, spells, abilities (asset, instance id, tier, quality; evolution durability) and crystal stacks.
  - `FWoRSaveData`: all three.
- `Save/WoRSaveGame.h` (`UWoRSaveGame`): holds `FWoRSaveData`. `WoRSaveVersion::LATEST` / `MIN_SUPPORTED`; `HandlePostLoad` wipes an old save (`WasWiped`).
- `Save/SaveSubsystem.h` (`USaveSubsystem`, GameInstance subsystem):
  - **Load:** when the first local player is added (PIE and standalone both add it before the first level's BeginPlay), so the saved party exists before anything calls `EnsureLocalParty`.
  - **Save points:** async at a trial's exit and a fight's exit (`UTrialRunSubsystem`); sync when the local player is removed at shutdown (before any subsystem deinitialises).
  - **Wallet carry:** `AWoRPlayerState` banks the local wallet at EndPlay and takes it back at BeginPlay, so banked balances also survive every OpenLevel.
  - Pure `Capture*` / `Restore*` / `ApplyWallet` functions, used by the flow and the spec.
- `UPartySessionSubsystem::RestoreLocalParty`: rebuilds the local party with no leader; the first `CreateSoloParty` / `EnsureLocalParty` rebinds it.
- `UPoolSubsystem::MarkPopulated`: a restored pool counts as populated, so the authored seed can't add a second copy.
- `Config/DefaultGame.ini`: the saved data-asset types are registered with the Asset Manager (scan `/Game`); the Map scan points at `/Game/Level`.

## Integration points

- Reads and writes `UPartySessionSubsystem`, `UPoolSubsystem` and the local `AWoRPlayerState` wallet.
- Called by `UTrialRunSubsystem` (save points) and `AWoRPlayerState` (wallet carry).

## Debug

- `wor.Save.Print`: what the slot holds (party, wallet, pool counts) and whether it is on disk.
- `wor.Save.Now`: capture and write now.
- `wor.Save.Delete`: delete the slot and stop automatic saves for the session, so the next start is fresh.
- `wor.Wallet.Add <currency> <amount>`: a test grant to the player's wallet.
- `USaveDebug::GetSaveString`. Spec: `WoR.Save`.

## Known limitations / TODOs

- **Gear:** the party's armoury lives in the pool since B-091 and is saved whole since B-124 (`FSavedGearDetail`: attached crystal, evolution or fusion with its wear, spell slots, rolls). Only what was pushed to the pool is saved, so a crystal's wear keeps its last pushed value. The party's carried items and storage are saved since B-203.
- **Run save (B-208, Q-208):** `FSavedRun` holds the trial, seed, variation, difficulty, beaten encounters, found features, run memories and run Gold. A reload resumes only when the game starts in the hub (`wor.Save.ResumeRun`), at the trial's start; started anywhere else, the run and the party's carried state are dropped. The journal is not saved. If the start map is ever a menu instead of the hub, resuming needs revisiting.
- Gold is never banked. Since B-178 it rides `USaveSubsystem::CarriedGold` in memory across level changes, and `ClearRunGold` zeroes it when a run starts and ends.
- One slot, shared by every PIE instance and the standalone game. No split screen.
- The backlog row named the print command `wor.PrintSave`; it is `wor.Save.Print`, next to `wor.Save.Now` / `Delete`.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created (B-090): `UWoRSaveGame`, `USaveSubsystem`, save points, wallet carry, Asset Manager types, `WoR.Save`. | feature/B-090-save-keystone |
| 2026-10-27 | B-178: the run's Gold carries across OpenLevel (`CarriedGold`, not written to the slot); `ClearRunGold` at trial entry and hub return. Case added to `WoR.Save`. | feature/B-178-run-repair |
| 2026-10-27 | B-208: the trial run is saved (`FSavedRun`, `FSavedCarriedGear`, version 2) and resumed from the hub; `wor.Save.Print` shows it; spec `WoR.Save.Run`. | feature/B-208-save-trial-run |
| 2026-10-27 | B-124 (D-059 = B): `FSavedOwnedItem::Gear` (`FSavedGearDetail`) saves a weapon's or ring's attachment (evolution item as a ref), spell slots (an empty slot as an unset ref), stat and resistance rolls and roll pools; save version 3 (older saves load with the asset defaults). The entries' partial `SaveGame` tags are gone. `wor.Save.Print` lists each item's detail. Spec `WoR.Save.GearDetail`. | fix/B-124-save-gear-detail |
| 2026-11-03 | B-308 (D-110 step 8, Q-308): save v5. `FSavedPool::Gear` holds owned weapons and adapters in one list (kind from the asset); `CapturePool` writes only it; `RestorePool` also reads the v3 / v4 `Weapons` / `Rings` lists (no wipe). `wor.Save.Print` prints the gear list. Spec case in `WoR.Save.GearDetail`. | refactor/B-308-gear-save-v5 |
| 2026-11-03 | B-311 (D-111): save v6. `FSavedPool::CustomLoadouts` (per character key, explicit `FSavedCustomLoadout`: pieces by owned GUID + asset ref, abilities / spells / Broken Darkness pools by asset ref; an adapter's spells ride on its gear detail). `CaptureInto` pushes every live inventory's custom loadouts to the pool first. Older saves load with none. | feature/B-311-custom-loadouts-saved |
| 2026-11-03 | B-321 (D-117): save v7. `FSavedPool::Owners` (`FSavedWearer` list: GearID -> character) keeps who owns each piece; an older save has none and each piece goes to its wearer, else the party leader, at the next draw |
| 2026-11-03 | B-279b: save v8. `FSavedForms::WornForm` (form asset name) and `BaseForm` (Manny / Quinn byte); older saves load with no skin and Manny; a saved skin that's unknown or locked loads as none |
| 2026-11-03 | B-322 (D-117): save v9 (v8 went to B-279b). `FSavedPool::Pouches` (`FSavedPouch`: character, stacks, slot count) keeps each character's pouch; `CarriedItems` / `ItemSlotCount` (v7, the party pouch) are read on load into the pool's legacy pouch, given to the party leader at the next draw, and written again only while no member has drawn it (so a save between load and draw loses nothing); bought slots alone also count. Built on the desktop. | feature/B-322-per-character-pouch |
| 2026-11-03 | B-279c: save v10. `FSavedForms::CurrentOutfit` (six soft paths, one per `EOutfitSlot`) keeps the player's current outfit; restored after the worn skin; an older save loads with none. Unbuilt (cloud). | feature/B-279c-outfits |
| 2026-11-04 | B-348 (D-138, Q-348): one save version. `LATEST` 11, `MIN_SUPPORTED = LATEST` (any older save is wiped with the D-060 warning). Deleted: `FSavedPool::Weapons` / `Rings` (v3 / v4), `CarriedItems` / `ItemSlotCount` (pre-v9 party pouch) with the pool's `RestoreLegacyCarriedItems` / `AdoptLegacyCarriedItems` / `LegacyCarriedItems`, `FSavedGearDetail::bSaved` (pre-B-124). The `SaveGame` tags on `UPoolSubsystem` (12), `UCurrencyComponent` (5), `UCharacterDataComponent` (4), `UCrystalInventoryComponent` (2) and `UEvolutionInventoryComponent` (1) are gone; their values already travel in `FSavedPool` / `FSavedWallet`, except the head-starts and `bIsBrokenDarkness` (never saved; B-348b). Specs in `WoR.Save`. Written unbuilt in a cloud session. | chore/B-348-one-save-version |
