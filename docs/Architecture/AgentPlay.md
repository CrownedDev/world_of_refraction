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
| `AgentPlay/AgentPlayDebug` | `GetStateString` (the `GetState` tool) and `wor.Agent.PrintState` |
| `world_of_refractionEditorModule.cpp` | Registers the toolset with `UToolsetRegistry` |
| `Debug/Inventory/AgentTestKitDebug` (game module) | `wor.Debug.GiveTestKit`: apparel, adapters, evolutions, learned abilities, a second party member, the test weapons. Assets are found by kind through the asset registry. Compiled out of shipping |

**Why it works where the Slate inspector's `PressKey` did not:** that tool sends the press and the release in one frame, so the game's input never sees the key down. Here the release is always a later frame, and a tap's default hold (50 ms) is under Enhanced Input's 0.2 s Tap limit.

**Timing:** an event is sent on the first frame whose start is at or after its time, on the real clock (`FPlatformTime`), counted from the sequence's first frame. The answer gives each event's real time and frame.

**Mouse:** points are pixels of the game picture. The real pointer is moved (`SetCursorPos`), because Slate reads it every frame; a Slate move event follows so hover updates at once.

**Keyboard focus:** a key press gives focus to the game picture when focus is outside it (the editor, the console). Focus inside the game (a menu's widget) is left alone.

## Integration Points
- `UToolsetRegistry::RegisterToolsetClass` (ToolsetRegistry plugin, turned on by AllToolsets); the ModelContextProtocol plugin serves it.
- `GEditor->GetPIEWorldContext()`, `UGameViewportClient::GetGameViewportWidget / GetWindow`.
- `FSlateApplication`: `ProcessKeyDownEvent / ProcessKeyUpEvent`, `ProcessMouseMoveEvent / ButtonDown / ButtonUp / WheelOrGesture`, `TakeScreenshot`.
- Console: `ULocalPlayer::Exec` in the PIE world; the log is listened to while the command runs, since most `wor.` commands answer there.
- Setup: `UInventoryComponent::AddRing / AddEvolutionInstance`, `UPoolSubsystem::AddApparelToPool / LearnSkillCopy`, `UPartySessionSubsystem::InviteMemberWithData`, `WoRGiveTestWeapons::GiveTestWeapons`.

## Known Limitations / TODOs
- Not yet run in PIE: built with the editor closed. The brain's live check is `docs/PIE/B-573.md`.
- No modifier keys (Shift, Ctrl, Alt), no typing into a text box (no character events) and no pad sticks or triggers; pad buttons only.
- The apparel part counts every character's owned apparel toward its 60, so it gives fewer when others already own pieces.
- Mouse tools move the real pointer, so the PIE window must not be covered by another editor window at that point.
- `SetWindowSize` needs PIE in its own window.
- The skill part of the kit picks the longest asset names, as a stand-in for the longest shown names.
- The member part's fallback puts another `UCharacterData` in the played character's body; it is only used when the level has no recruitable character.
- An agent still can't hear, and judges feel poorly (HandsOffPlan, Limits).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-22 | B-573: new. Editor module `world_of_refractionEditor`, `WoRAgentPlayToolset` (PlaySequence, TapKey, KeyDown, KeyUp, MoveMouse, Click, Wheel, Screenshot, SetWindowSize, RunConsoleCommand, GetState), `wor.Agent.PrintState`, `wor.Debug.GiveTestKit`. Specs `WoR.AgentPlay.Schedule`, `WoR.Debug.GiveTestKit`. | `feature/B-573-agent-test-tool` |
