# Save system (persistence keystone)

## Overview

One save slot per local player (`WoR_Main`) keeps what lives between play sessions: the party, the player's banked currencies and the shared pool's owned items. It is built the standard Unreal way (a `ULocalPlayerSaveGame` subclass with a version stamp), with explicit save structs rather than `SaveGame` property tags (D-059). Until the first public build, a save below the minimum version is wiped with a warning (D-060).

## Architecture

- `Save/WoRSaveTypes.h`: the save structs, plain data with no pointers.
  - `FSavedAssetRef`: an asset as `FPrimaryAssetId` plus its path. `Resolve` tries the id through the Asset Manager first (it survives an asset move), then the path.
  - `FSavedParty` / `FSavedPartyMember`: character, pawn class, formation cell, Player/AI control, carried HP/EP (always written as unset until the run is saved: B-208, Q-090).
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

- **Inventory isn't kept yet.** Pawns still rebuild their gear from their authored `CharacterData->Inventory`, and nothing in play adds to the pool. The pool round-trips, but it is filled only by `wor.PopulatePool`. Gear that persists is B-085 (loadouts point at owned gear), B-091 (equip from the pool) and B-124 (save each entry whole: crystal, spells, rolls).
- **No run save:** quitting mid-trial abandons the run, so carried HP/EP, crystal wear and absorption are not written (Q-090). Resuming a run is B-208.
- Gold is never banked, so it still resets on a level change.
- One slot, shared by every PIE instance and the standalone game. No split screen.
- The backlog row named the print command `wor.PrintSave`; it is `wor.Save.Print`, next to `wor.Save.Now` / `Delete`.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created (B-090): `UWoRSaveGame`, `USaveSubsystem`, save points, wallet carry, Asset Manager types, `WoR.Save`. | feature/B-090-save-keystone |
