# Agent play tool

## Overview
The tool an agent plays the game with in PIE, over unreal-mcp (B-573, D-251). It sends keys, mouse and wheel to the game window, runs timed input sequences with screenshots, sets the window's size and runs console commands in the game's world. It exists because the first agent run could not move the mouse, scroll, resize, tap a key the game would see, or set up a party and a full grid.

How to use it: `docs/Conventions/HandsOffPlan.md`, "How an agent plays".

## Architecture
Everything but the setup command lives in the editor-only module `world_of_refractionEditor` (`Source/world_of_refractionEditor/`), so none of it is in a packaged game.

| File | What it holds |
|---|---|
| `AgentPlay/AgentPlaySchedule` | Pure. `FWoRAgentPlayStep` (time, action, key, hold, point, wheel). `BuildEvents` turns steps into timed events: a tap or click is a press and a later release, a mouse step moves the pointer first. `CountDue` picks a frame's events and keeps a release out of its press's frame. `Validate` refuses bad steps with the reason |
| `AgentPlay/AgentPlayToolset` | `UWoRAgentPlayToolset` (`UToolsetDefinition`): the tools. One runner plays a sequence: inputs go in on `FCoreDelegates::OnBeginFrame` (before the game reads input), screenshots are taken on `OnEndFrame` (once that frame is drawn). The tool call returns at once with a `UWoRAgentPlayResult`, completed when the last event is sent. `FindGame` finds the PIE world, game picture (`SViewport`) and window |
| `AgentPlay/AgentPlayHold` (private) | `Take`, `Release`, `IsTaken`, `IsPointerParked`: the agent's own pointer and the background throttle, see below |
| `AgentPlay/AgentPlayDebug` | `GetStateString` (the `GetState` tool) and `wor.Agent.PrintState` |
| `world_of_refractionEditorModule.cpp` | Registers the toolset with `UToolsetRegistry` |
| `Debug/Inventory/AgentTestKitDebug` (game module) | `wor.Debug.GiveTestKit`: apparel, adapters, evolutions, learned abilities, a second party member, the test weapons. Assets are found by kind through the asset registry. Compiled out of shipping |
| `Debug/Inventory/QAKitDebug` (game module) | `wor.Debug.QAKit`: an ability that costs EP (the one named after `skill`, if given) in the weapon in hand's skill slot (through `WoRLoadoutSkills`), and a Caster party member as a player's seat. Compiled out of shipping |

**Why it works where the Slate inspector's `PressKey` did not:** that tool sends the press and the release in one frame, so the game's input never sees the key down. Here the release is always a later frame, and a tap's default hold (50 ms) is under Enhanced Input's 0.2 s Tap limit.

**Timing:** an event is sent on the first frame whose start is at or after its time, on the real clock (`FPlatformTime`), counted from the sequence's first frame. The answer gives each event's real time and frame.

**Mouse:** points are pixels of the game picture. The agent has its own pointer; the real one is never set or read (see the hold). Mouse events go in as the mouse's own Slate user and pointer (`GetUserIndexForMouse`, `CursorPointerIndex`), so the game can't tell them from a player's.

**The hold (`AgentPlayHold`):** taken by the first play tool of a PIE session, given back by `Release`, the end of PIE (`FEditorDelegates::EndPIE`) or the module unloading.
- *Its own pointer.* Slate asks the platform application where the mouse is, every frame, and re-checks what is under it. With the real pointer elsewhere (a person working in another app) that re-check cleared any hover the tool had set one frame earlier. So while the hold is taken, `FSlateApplication::OverridePlatformApplication` puts a stand-in in front of the real application: its cursor is a position only (`FAgentPlayCursor`), it says the pointer is over a window once the agent has put it on the game, and everything else is passed to the real application. This is the engine's own pattern (Pixel Streaming's application wrapper, for a remote player's mouse). No OS input is injected.
- *What a person still has:* the real pointer everywhere outside this editor; real clicks in this editor (they carry their own position). What they lose until release: hover in this editor.
- *No background throttle.* `UEditorPerformanceSettings::bThrottleCPUWhenNotForeground` is set false in memory and put back on release; it is never saved. With it on, a background editor ran PIE at about 3 fps and a 100 ms hold became 331 ms.
- *Pointer capture in the background.* A `UButton` (click method Down And Up, the default) asks to hold the pointer when pressed and clicks on release only if it still holds it (`SButton::OnMouseButtonUp`: hovered and `HasMouseCapture`). Slate grants that hold only while this editor is the active application (`FSlateApplication::ProcessReply`: `bHandleDeviceInputWhenApplicationNotActive || bAppIsActive || a virtual window`). The agent plays with the editor in the background, so a button hovered and drew pressed but never fired; a widget that acts on the press itself (the Gate screen) did. The hold now sets `FSlateApplication::SetHandleDeviceInputWhenApplicationNotActive(true)` and puts it back on release if it was off. The pointer held is the agent's: the stand-in application still passes no capture, lock or raw-mouse mode on to the real mouse, and nothing activates a window or takes the person's keyboard focus. One other reader of that switch: CommonUI's `FCommonInputPreprocessor::IsRelevantInput`, which with it on counts the agent's input in the background as it would in the foreground (it tracks whether the player is on mouse, keyboard or pad).

**Keyboard focus:** a key press gives focus to the game picture when focus is outside it (the editor, the console). Focus inside the game (a menu's widget) is left alone.

## Integration Points
- `UToolsetRegistry::RegisterToolsetClass` (ToolsetRegistry plugin, turned on by AllToolsets); the ModelContextProtocol plugin serves it.
- `GEditor->GetPIEWorldContext()`, `UGameViewportClient::GetGameViewportWidget / GetWindow`.
- `FSlateApplication`: `ProcessKeyDownEvent / ProcessKeyUpEvent`, `ProcessMouseMoveEvent / ButtonDown / ButtonUp / WheelOrGesture`, `TakeScreenshot`.
- Console: `ULocalPlayer::Exec` in the PIE world; the log is listened to while the command runs, since most `wor.` commands answer there.
- Setup: `UInventoryComponent::AddRing / AddEvolutionInstance`, `UPoolSubsystem::AddApparelToPool / LearnSkillCopy`, `UPartySessionSubsystem::InviteMemberWithData`, `WoRGiveTestWeapons::GiveTestWeapons`.

## Known Limitations / TODOs
- Not yet run in PIE: built with the editor closed. The brain's live check is `docs/PIE/B-573.md`.
- The click fix is proven headless one step below the tool's entry: `WoR.AgentPlay.Hold` routes a press and a later release along a hand-built path to a button, with the hold taken and the application set inactive. The tool's own entry (`ProcessMouseButtonDownEvent / UpEvent`) finds its target by a hit test on a shown, painted window, which a headless run has none of; that part is the brain's live check.
- Not looked into: Down + Enter on the old HUD did nothing visible in the same live check. A focused button clicks on Enter's release, so that may be focus, not capture.
- No modifier keys (Shift, Ctrl, Alt), no typing into a text box (no character events) and no pad sticks or triggers; pad buttons only.
- The apparel part counts every character's owned apparel toward its 60, so it gives fewer when others already own pieces.
- The PIE window must not be covered by another window of this editor at the point used (another app in front is fine).
- While the hold is taken, the real mouse can't hover in this editor (clicks work, a drag that leaves the window is dropped), and the game can't take the real mouse for camera look, hide it or keep it inside the window: taking the hold lets go of any of those.
- Engine code that treats the platform application as the Windows one by a plain cast (the Fab browser, Reflex markers, the Wintab stylus and RawInput plugins) must not run while the hold is taken. None is used by this project's game; the same limit applies to Pixel Streaming.
- Something that starts listening to the platform application's own events (display change, virtual keyboard) while the hold is taken hears nothing until release.
- If a person saves the editor's preferences while the hold is taken, the throttle setting is saved as off.
- `SetWindowSize` needs PIE in its own window.
- The skill part of the kit picks the longest asset names, as a stand-in for the longest shown names.
- The member part's fallback puts another `UCharacterData` in the played character's body; it is only used when the level has no recruitable character.
- An agent still can't hear, and judges feel poorly (HandsOffPlan, Limits).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-22 | B-573: new. Editor module `world_of_refractionEditor`, `WoRAgentPlayToolset` (PlaySequence, TapKey, KeyDown, KeyUp, MoveMouse, Click, Wheel, Screenshot, SetWindowSize, RunConsoleCommand, GetState), `wor.Agent.PrintState`, `wor.Debug.GiveTestKit`. Specs `WoR.AgentPlay.Schedule`, `WoR.Debug.GiveTestKit`. | `feature/B-573-agent-test-tool` |
| 2026-11-22 | B-573 follow-up (the brain's live check): the hold. The agent has its own mouse pointer (a stand-in platform application, as Pixel Streaming does), so the real pointer is never set and never disturbs the game; the background CPU throttle is off while it plays, in memory only. New tool `Release`; `GetState` shows the hold. Spec `WoR.AgentPlay.Hold`. | `feature/B-573-agent-test-tool` |
| 2026-11-22 | B-573 click fix (the brain's live check: a click hovered and pressed a `UButton` but never fired it). Cause: Slate refuses a pressed button's pointer capture while the editor is not the active application, and a Down And Up button only clicks on release if it holds the capture. The hold sets `SetHandleDeviceInputWhenApplicationNotActive(true)` and restores it on release. `WoR.AgentPlay.Hold` gains the test, which failed before the fix (capture refused, 0 clicks). Game code unchanged. | `fix/B-573-agent-click-capture` |
| 2026-11-24 | QA setup (Crown's overnight queue): `wor.Debug.QAKit [skill] [caster [element]]` (`QAKitDebug`), spec `WoR.Debug.QAKit` | `chore/qa-kit` |
| 2026-11-24 | QA kit on a brand-new character (the brain's live check): `WoRQAKit::LearnForKit` keeps the member's known skills in the store first when it has none yet (a new game's first draw may not find it in the party), then learns the ability in the store and on the member's inventory (the list the skills page reads). A failed skill part lists each fitting ability. | `chore/qa-kit` |
| 2026-11-24 | B-595a: `wor.Debug.QAKit skill <asset name>` takes only that ability (`WoRQAKit::IsNamed`: asset name or shown name, any case), for QA of one named skill; spec case. Also `wor.Editor.BuildQuickStrikeMontage` (`LightHitMontageBuilder.cpp`): Quick Strike's dash + slash montage, Impact0 on the slash's contact frame | `feature/B-595a-sword-quick-strike` |
| 2026-11-24 | B-595e: `wor.Editor.BuildForceStrikeMontage` (`LightHitMontageBuilder.cpp`): Force Strike's in-place slash, Cast0 at the cut (0.95 s) firing the ability's CastArray[0] projectile; no warp | `feature/B-595e-force-strike` |
| 2026-11-24 | B-595b: `wor.Editor.BuildOverUnderMontage` (`LightHitMontageBuilder.cpp`): Over Under's dash + two cuts (Combo1 0.65 to 1.65 s), Impact0 and Impact1 on the contact frames; `AddCombatNotify` takes a notify index | `feature/B-595b-sword-over-under` |
| 2026-11-24 | B-595c: `wor.Editor.BuildHeavyAttackMontages` (`LightHitMontageBuilder.cpp`): Heavy Attack's wind-up (the Buff gesture, its RitualCastMontage) and its strike (the dash + `Anim_SwordV2_HeavyAttack` 0.78 to 1.75 s, Impact0 at the lunge) | `feature/B-595c-sword-heavy-attack` |
| 2026-11-24 | B-595d: `wor.Editor.BuildWhirlWindMontage` (`LightHitMontageBuilder.cpp`): WhirlWind's dash + `Anim_SwordV2_Slash3` (a full spin ending in a slam), Impact0 on the slam (0.9 s into the clip) | `feature/B-595d-sword-whirlwind` |
