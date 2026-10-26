# Combat Camera

**Status:** designed. Architecture from `Resources_Design.md` "Combat camera — COMPLETE ARCHITECTURE" (Crown, 2026-06-25, Expedition 33 model); open parts decided 2026-10-26 (`DesignDecisions.md` D-074).
**Today:** no combat camera exists. The old `CombatCameraManager` was deleted (2026-07-20); the view stays on the possessed pawn.

## 1. The model

- **Each character owns one camera.** Every camera follows the **same mode rules**; game state (whose turn, who's acting) picks each camera's mode. No central manager computes positions: a small selector picks which camera the viewer sees.
- **What you see follows what you control:**
  - **Solo:** the camera moves to each of your members as their turn comes.
  - **Co-op:** close on your turns; teammates' turns pan out.
  - **PvP:** the same logic.

## 2. Modes

| Mode | Camera |
|---|---|
| **Turn** (choosing an action) | **Default: centred behind the acting character, a bit wider**, with the whole enemy team in view (D-074). Settings: close over-the-shoulder (Expedition 33) or wide tactical side-on |
| **Action** (skill confirmed) | The skill's **Level Sequence** plays, bound at runtime to the caster and target(s) combat already knows |
| **Not your turn** | Pan out wide |
| **Defence** | Probably none: the defender sees the incoming hit through the attacker's Action sequence. **Play-test first**: if parry/dodge timing isn't readable, add a Defence mode |

**Settings:**
- Turn-view style (centred-wide / over-the-shoulder / tactical).
- **Camera movement off:** a static wide camera, for motion sickness (Expedition 33 ships one).
- **Cinematic settings (D-078, defaults untested, check in PIE):** skill cinematics on/off (default: off when camera movement is off), enemy skill cinematics on/off (default on), skip a cinematic after its hit lands (default off: plays to the end or the next turn).

## 3. Skill cinematics (Level Sequences)

- **Templates by skill type** for most skills: melee swing, ranged shot, area spell, buff and more as needed. **Hand-made sequences for signature moves**: lord skills, bosses, big spells (D-074).
- A skill's data asset references its sequence (`TSoftObjectPtr<ULevelSequence>`, per the D-057 soft-reference rule); empty means use its type's template.
- **Layering:** the sequence is camera and presentation only, running alongside the existing timing. Montage → notify → damage and defence windows stay in C++ and are **not** moved into Sequencer event tracks.
- **Runtime binding:** the sequence's caster/target placeholder tracks are bound to this action's actors; when it ends, the view returns to the Turn or Not-your-turn camera.

## 4. Build order (D-074: a basic version soon)

1. **Code (CC):** per-character camera component, the mode selector driven by combat events (turn start, action confirmed, action end), the settings, the skill → sequence field, and the play/bind/cleanup lifecycle. Debug: `wor.Camera` state print.
2. **Content (CC+UE):** three template sequences (melee, ranged, spell) and the camera rig tuning, with screenshots for Crown.
3. **Later:** more templates, signature sequences, and the Defence-mode play-test.

## 5. Hub camera

Separate from combat: a standard third-person follow camera tuned toward Expedition 33's exploration feel (longer arm, light lag). `Resources_Design.md` "Hub camera".

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created from the June design + D-074 | docs/combat-camera |
