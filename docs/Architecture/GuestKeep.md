# Guest Keep — a co-op guest keeps what it earns

## Overview
Crown, 2026-11-17 (Q-561): a guest in a friend's game keeps what its character earns there, as if it had played its own game. The guest joins as one of its own characters, or a new one (B-561, B-562). What that character earns goes back into its own save file for that character, by the solo run-end rules.

## Architecture
- **`UGuestKeepSubsystem`** (game instance subsystem, on the guest's machine) remembers the visit: the joined character's slot and picks, and how much of its wallet is already counted.
  - `BeginJoin(Slot, Picks, Wallet)`: called by `WoRTitleScreen::SendJoin`. `INDEX_NONE` means a new character, written to the first free slot now (`MakeNewCharacterSave`).
  - `NoteReportedWallet`: the wallet sent to the host on connect (B-425). This is the start of the count, once per visit.
  - `FoldCheckpoint(HostCopy, Wallet)`: the host's save copy (B-234) has arrived. It folds the wallet, then, if no run is open, the guest member's kept levels.
  - `FoldWallet(Wallet, bLeaving)`: the wallet alone. Leaving ends the visit.
  - Pure folds (specs): `AddWalletDelta` (own + now − counted, never below 0), `FindGuestMember` (the host's member with `bGuest` and the same picks), `FoldKeptLevels`.
- **`USaveSubsystem::WriteSlotData(Index, Data)`** writes a slot from outside play. The slot in play takes the data in memory and writes it now.
- What counts as earned:
  - **Currencies:** every player's own wallet is already paid in full (D-046, D-103), so the guest keeps its wallet's change since it connected. Gold is run-only (B-178) and never banked.
  - **Kept levels (D-237):** taken from a settled host save (no run open), and they replace the last ones.
  - **Gear (D-219) and skills (D-224):** B-564.

## Integration points
- `AWoRPlayerState::ClientInitialize` → `NoteReportedWallet`.
- `Client_ReceiveCheckpointChunk` (last chunk) → `FoldCheckpoint`.
- `EndPlay` (a client's own player state) → `FoldWallet`. Leaving is any reason other than `LevelTransition`.
- `WoRTitleScreen::SendJoin` → `BeginJoin`.
- Debug: `wor.Coop.PrintGuestKeep`.

## Known limitations / TODOs
- Unproven without a second client: the hooks on a real client.
- On a disconnect the local controller may already be gone at `EndPlay`. The wallet change since the last checkpoint is then lost; the last checkpoint's fold still stands.
- The wallet counted at connect is the slot in play's (B-425). Joining as another slot counts only the change, never that balance.
- Gear and skills come in B-564.
- D-064: this is per co-op visit, not per fight: the local player's link to its own save, on its own machine. A visit starts with each new server connection (`NoteReportedWallet`); a seamless travel keeps the same connection. A wallet not replicated yet (empty) is not folded.
- The new character's slot is written when the join is sent, so a refused join still leaves it on the title.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-17 | B-563: created. The visit, the wallet change, kept levels, the new character's slot, `WriteSlotData`. Spec `WoR.Coop.GuestKeep`. | `feature/B-563-guest-keeps-earnings` |
