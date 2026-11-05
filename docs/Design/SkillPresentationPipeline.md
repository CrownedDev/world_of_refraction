# Skill Presentation Pipeline

**Status:** designed 2026-11-05 (Crown, via the stand-in brain). Decision **D-162** (cinematic window, then the Defence view); defaults in **Q-369 / Q-370 / Q-371 / Q-373**. Jobs **B-368..B-373**; B-157 rewritten to match.

**Goal:** Crown supplies a skill's assets (animation, VFX, SFX) and Claude Code does the rest: notifies, impact visuals, cameras, data. Crown only checks what's flagged and approves a few shared looks once. Gameplay stays seamless because every skill is checked by rules before it plays.

---

## 1. One analysis feeds everything

An attack animation's bone motion gives its key moments. Every later step reads those moments; none of them needs a person to watch the clip.

| Moment | How it's found (bone data, sampled per frame) | Used by |
|---|---|---|
| Wind-up start | the quiet frame before the striking limb speeds up | wind-up VFX, cinematic window start |
| Wind-up end | the striking limb starts its fast move | cinematic window end (§4) |
| **Cast** (spell release) | the casting hand's furthest reach / speed peak | Cast notify, muzzle VFX |
| **Impact** (melee contact) | the weapon `Tip` (or striking hand/foot) speed peaks, then drops sharply | Impact notify, impact VFX, camera punch |
| Hit count | the number of such peaks | checked against the skill's `HitCount` |
| Footsteps | a foot bone at floor height and still | footstep SFX |

**Confidence:** one clear peak per hit is placed automatically. Spins, flurries whose peaks run together, slow swings with long follow-through, and root-motion clips are **flagged**, not guessed. Each flag comes with a contact sheet (the frames around the guess) for Crown.

**Tooling (Q-373):** an editor Python script under `Scripts/editor/` (plain text, re-runnable, run through unreal-mcp on Crown's PC). It reads bones with `unreal.AnimationLibrary.get_bone_pose_for_time` and places `UCombatNotify` with `add_animation_notify_event`. Unreal's Animation Modifiers do the same job but need an editor module or a Blueprint asset; they're an option later, not now.

**Proof before trust (B-373):** run the script on 3 to 5 montages Crown already placed by hand, without letting it see those notifies, and compare. If it lands within a couple of frames on the clear ones and flags the unclear ones, the pipeline is trusted for new assets.

## 2. Intake standards (what makes it work)

- **One project skeleton.** Packs are retargeted on import, so bone names stay the same (`hand_r`, `foot_l`, …).
- **Weapon meshes carry `Tip` and `Base` sockets.** Without them, contact is read from the hand (less exact).
- **Attack animations are In-Place by default**; root motion is opt-in and marked (CLAUDE.md gotcha).
- **Names follow `docs/Conventions/ContentNaming.md`**, so the name already says type and likely hit count (`AM_Sword_Combo3`).
- **Characters carry a hit socket** (`HitPoint`, chest height) for impact VFX (§3).

## 3. Impact VFX come from the outcome (Q-369)

Animators place **timing** only. **What** plays on an Impact frame is chosen in code after defense resolves (the way Lyra's GameplayCues and most action games pick a hit effect per result):

| Outcome | Visual |
|---|---|
| Hit | element-tinted burst |
| Crit | the same, larger |
| Guard | a steel spark |
| Perfect Guard | the existing flash (`CosmeticsData::PerfectGuardFlashVFX`) |
| Dodge | a small whiff, or nothing |

- **Fallback chain:** the skill's own Impact-role `VFXArray` entry → the element default → a generic hit. Element and outcome defaults live in `CosmeticsData` (soft references, D-057).
- **Where:** the target's `HitPoint` socket; the actor location if it has none.
- Projectiles keep their own impact path (`ActionExecutor.cpp:3528`) and use the same outcome choice.

## 4. Cameras: cinematic window, then the Defence view (D-162)

This game asks the player to defend in real time inside turn-based combat (Guard, Dodge, interrupt points, D-161), and in co-op each player defends their own character. So **a cinematic may never cost a player the time to react**.

**The cinematic window.** A montage carries a **Cinematic notify state**: a range, not a point. Inside the range the dramatic camera plays (a code-built shot, §5, or the skill's own Level Sequence). When the range ends, the camera hands over:
- **to the Defence view** on every machine whose own character is a target: behind and slightly beside that character, the attacker in front, so the incoming hit reads clearly;
- **to the watch view** (wide) on machines with no character targeted.

**The rule that keeps it fair (checked by B-368):** the window must end at least `MinDefenseReadSeconds` before the first defendable moment: the first Impact or Cast, minus its Telegraph lead. First guess **0.75 s** (`TODO: tune` in PIE). Without a placed range, the window runs from the montage start to the wind-up end (§1), clamped by the same rule.

**Per viewer, not per skill.** A viewer who doesn't have to defend (your own attack, a teammate's target) can keep the cinematic to the end of the skill. A viewer who must defend always gets the hand-off. One rule covers solo, co-op and PvP.

**Settings stay as decided (D-078):** camera movement off gives one static wide view (no cinematic, no Defence-view move); "skip cinematic after hit" still applies.

**Reference:** Expedition 33's enemy-turn camera keeps the defending party member in the foreground facing the attacker, and its parry and dodge timing is widely praised as readable. Its signature skill cameras play on the player's own turn, when nothing needs to be defended.

## 5. Shot patterns (Q-371)

Three layers. Most turn-based RPGs with large skill lists reuse a few shot patterns and keep hand-made cinematics for signature moves (Persona 5, Octopath Traveler, Honkai: Star Rail; D-074 already chose this split).

1. **Code-built shots, timed by the moments in §1 (no work per skill):**

   | Moment | Shot |
   |---|---|
   | Wind-up | over the caster's shoulder, slow push in |
   | Cast | hold, or follow the projectile |
   | Impact | frame the target, small shake and FOV punch (larger on a crit) |
   | AOE / several targets | pull out until every target is in frame |
   | End | blend back to the turn view |

2. **Per-skill tuning without Sequencer:** a small struct on the skill (shot style, distance, side, shake strength, follow projectile on/off).
3. **Hand-made Level Sequences** only for ultimates and boss moments (the existing `USkillDataBase::CameraSequence`). They play inside the cinematic window like any other shot.

**What Crown approves:** the 4 or 5 patterns, once. Every new skill then gets cameras automatically.

## 6. Checks (no PIE needed)

- **Notify validator (B-368, headless spec):** every skill's montage has Impact notifies `0..HitCount-1` in order; Cast and VFX indices fit their arrays; every cinematic window obeys §4's rule.
- **Framing checks (B-372):** caster and targets project inside the screen; no wall between camera and action (line trace); camera not inside geometry; camera on the viewer's side (already `TowardEnemies`).
- **Taste** stays Crown's: contact sheets for flagged notifies, screenshots of each shot pattern.

## 7. Jobs

| Job | What | Worker |
|---|---|---|
| B-368 | Notify validator spec, cinematic-window rule | Cloud writer / builder |
| B-369 | Outcome impact VFX, element defaults, `HitPoint` socket | Cloud writer / builder |
| B-370 | Cinematic notify state, Defence-view hand-off per viewer (D-162) | Cloud writer / builder |
| B-371 | Code-built shot patterns and per-skill tuning (replaces B-157's templates) | Cloud writer / builder |
| B-372 | Framing checks and screenshot review | Builder + editor |
| B-373 | Notify placement script and its proof test | Editor worker (Crown's PC) |
| B-157 | Crown approves the shot patterns in PIE; rig tuning | Crown + editor |
