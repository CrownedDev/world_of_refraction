# Online sessions

## Overview
B-233 (D-103, Q-233): co-op players host, find and join a session through Unreal's online subsystem. The code is written against the generic session interface (`IOnlineSession`), so the platform is a config choice: the **Null** subsystem (LAN) now, EOS (with Steam friends on top) once Crown's EOS ids exist.

## Architecture
- `UWoRSessionSubsystem` (game instance, `Online/WoRSessionSubsystem.h`): `HostSession` (create, then `ServerTravel` the current level `?listen`), `FindSessions` (LAN query on Null, lobbies otherwise), `JoinFoundSession` (join, then `ClientTravel` to the resolved address), `LeaveSession`, `GetSessionString`.
- `WoRSession::MakeHostSettings`: 3 public connections (D-150), advertised, join in progress, invites; LAN on Null, presence and lobbies otherwise.
- Config: `.uproject` enables OnlineSubsystem / OnlineSubsystemNull / OnlineSubsystemUtils; `DefaultEngine.ini` `[OnlineSubsystem] DefaultPlatformService=Null`.

## Integration Points
- After the travel, the D-150 join flow (`URunStateComponent::RequestJoin` / `AcceptJoin`) seats the joiner.
- Debug: `wor.Session.Host`, `wor.Session.Find`, `wor.Session.Join <index>`, `wor.Session.Print`.

## Known Limitations / TODOs
- EOS config, Steam on top, the hub-menu invite UI, joining a run at a rest spot and a leaving friend dropping out: B-233b.
- No handling of a lost host (host hand-over is B-234 / D-103).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-08 | Created. B-233 part 1: `UWoRSessionSubsystem` on the Null subsystem, `wor.Session.*`, the plugins and config. Unbuilt (cloud pre-write). | feature/B-233-sessions-null-cloud |
| 2026-11-08 | B-234 part 1: checkpoint copies. `USaveSubsystem::SaveNow` on a listen server sends the whole save after its own write (`SaveGameToMemory`, zlib, 2 KB chunks, at most 128, `WoRCheckpoint::SendToClients`) by Client RPC on each remote `AWoRPlayerState`; the client reassembles it (`FCoopCheckpointAssembler`) and keeps it in slot `WoRCoopCheckpoint`. `wor.Coop.PrintCheckpoint`; spec `WoR.Coop.Checkpoint`. Hand-over: B-234b. | `feature/B-234-checkpoint-copies-cloud` (UNBUILT) |
| 2026-11-08 | B-234b part 1: host succession. `AWoRGameState::HostOrder` (join order, replicated; `AWoRPlayerState::OnSetUniqueId` adds a joiner), kept on each client by `UWoRHostHandOverSubsystem`, which on a lost host (`GEngine->OnNetworkFailure`) picks the next player: it adopts its copy (`USaveSubsystem::AdoptCheckpoint`; writes then go to the copy slot) and hosts, the others find it (`UWoRSessionSubsystem::FindSessionIndexByOwner`, `OnSessionsFound`) and join. `wor.Coop.PrintSuccession`, `wor.Coop.SimulateHostDrop`; spec `WoR.Coop.HostSuccession`. Planned hand-over: B-234c. | `feature/B-234b-host-hand-over-cloud` (in dev) |
| 2026-11-08 | B-234c part 1: the planned hand-over. The rest menu's Hand over host (`WoRHostSuccession::CanHandOverFrom`: a listen server with another player) calls `UWoRHostHandOverSubsystem::BeginPlannedHandOver`: a sync save (copies sent), `AWoRPlayerState::Client_BeginPlannedHandOver` on each remote PlayerState after its copy's chunks (each client hands over and disconnects), then the host disconnects after `PLANNED_LEAVE_DELAY_SECONDS` and rejoins the next host. Pool reset after the run: B-234d. | `feature/B-234c-planned-host-hand-over-cloud` (in dev) |
