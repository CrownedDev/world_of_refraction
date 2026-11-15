# Battle Transition

## Overview

There is no loading screen into a fight (D-212). At contact the frame is captured and broken into 3D shards in front of the camera. The shards drift while the join window runs and the battle map loads, then blow away onto the battle. The target is storyboard D (`docs/Design/screen_mockups/combat/transition/D_sheet.png`).

B-500 is the rough version: shards and motion. B-500b adds the prism light (code side; the material steps are in `docs/PIE/B-500b.md`). Still to come: sound and timing polish (B-500c), first strike and ambush looks (B-500d).

## Architecture

| Piece | File | Job |
|---|---|---|
| `UBattleTransitionSubsystem` (game instance) | `Combat/Transition/BattleTransitionSubsystem.h` | Starts, carries and ends the effect. Holds the clock, the seed, the hit point, the captured frame and the held picture, because the effect starts in the trial world and ends in the battle world. It holds no fight state. |
| `ABattleTransitionShards` | `Combat/Transition/BattleTransitionShards.h` | The shards in the world: one `UProceduralMeshComponent` per shard (its own transform gives it motion blur), held in front of the local camera, plus an unbound post-process component for bloom, motion blur and depth of field. Ticks in `TG_PostUpdateWork`, after the camera has moved. Dies with its world. |
| `BattleTransition` namespace | `Combat/Transition/BattleTransitionMath.h` | Plain maths, no world: `BuildShards` (cuts the screen from a seed), `EvaluatePose` (where a shard is at a time), `BuildShardMesh` (thick bevelled prism), `FClock` (the phases). |
| `UBattleTransitionData` | `Combat/Transition/BattleTransitionData.h` | Timings and look values. One asset at `/Game/Data/Combat/DA_BattleTransition_Default`; with none, the class defaults play. |

**Phases (`BattleTransition::EState`):**

1. **Playing.** `Play` captures the frame with a one-shot scene capture into a render target, spawns the shards and starts the clock. Cracks spread from the hit point (each shard tilts when the crack reaches it), the frame breaks, the shards drift toward the camera. The drift slows and never reaches the camera, so the hold can be any length.
2. **Frozen.** The map is about to change (`PreLoadMapWithContext` for OpenLevel, `OnSeamlessTravelStart` for ServerTravel). The last frame on screen is read back and drawn over the viewport as a Slate picture, above the level, so the map change can't remove it. The shards are destroyed with their world and the clock stands still.
3. **Arriving.** `PostLoadMapWithWorld`: the same shards are rebuilt on the battle map from the same seed, behind the picture, and wait `ArrivalSettleSeconds`.
4. **Releasing.** The picture fades off the live shards and they blow past the camera. Then everything is dropped.

**Pace (B-500a, Crown: "starts slow, then speeds up").** The opening is slow and the finish fast. Cracks spread over `CrackSeconds` (0.5). The break takes `BreakSeconds` (0.7) and is eased both ways by `BreakEase` (`FMath::InterpEaseInOut`: out of the wall slowly, slowing again into the drift). The blow-away takes `BlowAwaySeconds` (0.4) as `t ^ BlowAwayEase` (3): the first third barely moves, then it accelerates hard. An ease of 1 is an even pace. Crack plus break should stay under the encounter's join window (2 s), or the map change cuts the opening short.

`Release` (the fight didn't start, or a preview) goes from Playing straight to Releasing in the same world.

**Units.** The shard actor sits on the camera and is scaled so one unit is one screen width at the wall. Motion values are in screen widths, and depth is a fraction of the wall's distance (0 = in the wall, 1 = at the camera), so the effect looks the same at any resolution and field of view.

**Shard material contract.** `ShardMaterial` gets the captured frame in its `Frame` texture parameter, read with UV0 (the screen). Vertex colour: R = 1 on bevels and sides, G = a random per shard, B = distance from the hit point, A = the shard's hue (B-500b). With no material set, the shards use the engine's basic material tinted Void and show no frame.

**Prism light (B-500b).** The code sends numbers; the material draws the light. All shards share one material instance (`ABattleTransitionShards::ShardMaterial`), so per-shard differences come from vertex colour.

| Material scalar | Set | From |
|---|---|---|
| `Flash` | every frame | `FlashStrength` at contact, falling to 0 over `FlashSeconds` (squared, so a quick fall and a soft tail) |
| `LightReach` | every frame | 0 at contact, 1 when the crack reaches the farthest shard, ending at 1.5 so a soft front clears it. Same clock as the cracks: a shard is lit when `LightReach` passes its vertex colour B |
| `EdgeGlow` | every frame | `EdgeGlowStrength` while the cracks spread, settling to `EdgeGlowStrength × EdgeGlowHoldShare` over the break, kept through the hold and the blow-away |
| `ShardColor` | once, at build | `ShardColorStrength` |
| `WhiteReach` | once, at build | `WhiteReach`: the flash covers, and the light stays white, this far from the hit |

The three per-frame values are `BattleTransition::EvaluatePrismLight` (pure, in the spec). A shard's hue is `BattleTransition::GetShardHue`: its direction from the hit point around the colour wheel (red right, yellow-green below, cyan left, violet above, as on storyboard D), plus up to 0.12 of the wheel from its own random. A material without these parameters ignores them and shows the plain frame. Parameter names: `BattleTransitionAsset` (`BattleTransitionData.h`).

## Integration Points

- `UEncounterComponent::HandleOverlap` calls `Play(ArenaCenter)` when an encounter opens: walking in, an enemy walking in, and a field hit (`StrikeFromField`) all go through it. The join window is unchanged and runs under the effect.
- `UEncounterComponent::RearmEncounter` calls `Release` when the fight doesn't start.
- Engine delegates: `FCoreUObjectDelegates::PreLoadMapWithContext`, `FWorldDelegates::OnSeamlessTravelStart`, `FCoreUObjectDelegates::PostLoadMapWithWorld`, `UEngine::OnTravelFailure`.
- Cooking: the asset manager's `BattleTransitionData` rule (`DefaultGame.ini`, AlwaysCook).
- Debug: `wor.Transition.Play [hold seconds]`, `wor.Transition.State` (`Debug/Combat/BattleTransitionDebug.h`); the data asset's Log Values button.
- Spec: `WoR.Combat.BattleTransition`.

## Known Limitations / TODOs

- **The load itself is a still frame.** OpenLevel blocks the game, so nothing can move during it; the shards drift before it and blow away after it. Reading the battle map in the background during the join window would shorten the still part.
- **The boss arena** (`ABossArenaTrigger::Interact`) starts its fight without an encounter component and travels at once, so it plays no transition.
- **Co-op guests** are untested and may see no transition: the effect plays only on a machine where the encounter's overlap fires and there is a local view.
- **D-064:** the subsystem's fields (clock, seed, hit point, captured frame) are presentation state that must cross the map change, not fight state; they are cleared when the effect ends.
- **The captured frame** comes from a one-shot scene capture, so it has no temporal anti-aliasing and can differ a little from the real frame. If the match is poor, reading the viewport back (as the held picture does) is the alternative.
- **The push in** moves the shard wall toward the camera (the frame grows); the real camera does not move.
- **Field of view** is read as horizontal (the engine default).
- **The prism light needs the material** (B-500b): until `M_BattleTransition_Shard` reads the five scalars and vertex colour A (`docs/PIE/B-500b.md`), nothing new shows.
- **The flash blooms through the level's own bloom**: the transition's lens (stronger bloom) comes on with the break, after the flash.
- **The world behind is not dimmed** in the hold (storyboard frame 4); that needs a translucent material.
- **The spinner** for a load past 2 seconds (D-212) is not built.
- **Safety:** if no map change comes within `MaxHoldSeconds` of contact, the shards blow away where they are. From the map change on, a core-ticker watch (`WatchTravel`) drops the effect if the new map hasn't come after 120 seconds or the rebuilt shards are lost, so the held picture can't stay on screen.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-14 | B-500 (D-212): first version. Subsystem, shard actor, maths, data asset, debug commands, spec; started from `UEncounterComponent`. | `feature/B-500-battle-transition-rough` |
| 2026-11-14 | B-500a (Crown's PIE): slow open, fast finish. Defaults `CrackSeconds` 0.5, `BreakSeconds` 0.7, `BlowAwaySeconds` 0.4; new `BreakEase` (2, ease in and out) and `BlowAwayEase` (3, ease in) read by `EvaluatePose`; the debug line shows them; one more spec case. | `feature/B-500a-transition-slow-open-fast-finish` |
| 2026-11-15 | B-500b (D-212; Crown: "weren't we gonna add colours?"): the prism light, code side. `EvaluatePrismLight` (flash, light reach, edge glow) set on the shared shard material every frame; `GetShardHue` in vertex colour A; six **Prism** values in the data asset; the debug lines show them; two more spec cases. Material steps for the editor worker in `docs/PIE/B-500b.md`. | `feature/B-500b-transition-prism-light` |
