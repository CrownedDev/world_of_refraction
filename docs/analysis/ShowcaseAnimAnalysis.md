# Showcase animation analysis

Picks for the showcase battle's skills, made by **reading the bone motion**, not the clip names. One section per skill; Crown approves each before the Code Writer places notifies or builds montages.

**Method.** Each clip is sampled every frame (40 fps) with the read-only bone sampler (`Scripts/ue/sample_bones.sh`, the read part of B-373): root, pelvis, spine, head, both hands, right forearm, `weapon_r`, both feet. Forward is **+Y** (the way the character faces), side is X, up is Z. Phases come from the right hand's speed and path:
- **wind-up**: from the first move until the hand starts its fast strike;
- **contact**: where the hand is furthest along the strike, just after its speed peak (where the blade reaches the target);
- **hold**: the struck pose kept almost still;
- **recovery**: back to the clip's first pose (hand and pelvis within 6 cm).

Root drift, airborne time (both feet over 25 cm off the floor) and the start and end stance (pelvis height) are measured too. Timings are ±1 frame (25 ms).

**E33 reference.** `MaelleCombatPresentationStudy.md` §14. One hero swing: wind-up ~0.7 s → contact (~0.1 s) → effect hold ~0.3 s → recovery ~0.4–0.5 s. One enemy swing: wind-up ~0.6 s → contact → hold ~0.3 s → recovery ~0.5 s.

**Notify rules used** (from code): `UCombatNotify` families Impact / Cast / VFX / SFX, named `<Family><Index>` (`Impact0`); B-368 wants Impact `0..HitCount-1` in time order; a `Cinematic Window` (B-370) must end ≥ 0.75 s before the first Impact minus its telegraph lead; the defence window needs **no notify** (a guard / dodge counts if pressed up to `DefenseInputWindow` 0.5 s before each Impact); hit-stop (~40 ms, FeelGuide 4), hit VFX choice (Q-369) and camera punch are code, driven by the Impact notify. No recovery / cancel notify exists today: a montage blend-out time is proposed instead.

### Interrupt marker (Crown, 2026-11-12; not built)

**What it is:** a strike notify that also marks **"this attack can be interrupted here"**. It is D-161's interrupt point: if **every target Perfect Guards** that strike, the attacker's skill ends there (ED-37c). It is **not** our hit cancelling the target's ritual (that idea is dropped).

**Crown's rule: an interrupted attack ends and the attacker goes back to their spot** (the return leg plays).

**How interrupt points are authored today** (data, not a notify or curve):
- `bInterruptable` on a hit entry in the skill's damage split (`SkillDataBase.h:74`, keyed by hit number) and on a spell cast entry (`SkillCastEntry.h:89`).
- The Impact notify `Impact<N>` fires the hit; the executor looks up hit N's flag (`ActionExecutorMontage.cpp:436-445`) and, if every target Perfect Guarded it, calls `InterruptAsyncAction` (`ActionExecutor.cpp:2815`).
- Debug: `USkillCastDebug::GetInterruptPointsString`.
- So the flag lives in the skill asset, and the strike frame lives in the montage. Nothing in the montage shows which strike is the choke point.

**Proposal: one marker, on the Impact notify.**
- Add `bInterruptPoint` to `UCombatNotify` (Impact and Cast families only). When ticked, the montage-editor label reads **`Interrupt<N>`** instead of `Impact<N>` (or `Cast<N>`), with its own track colour. It is still hit N: the hit count, B-368's `0..HitCount-1` order and outcome VFX don't change.
- The executor treats hit N as an interrupt point if **either** the notify or the old data flag says so (transition). The B-368 validator warns when the two disagree.
- After Crown's PIE pass, the data flags are moved onto the notifies and removed (per "remove old code only after PIE").
- Why the notify, not the data: the choke point is a property of **this strike in this animation**; authors see it on the timeline next to the strike, and swapping the animation forces a re-check.
- Code Writer's job; no asset is changed here.

**Does it return the attacker today? Partly.** `InterruptAsyncAction` plays the skill's `ReturnMontage` (warped back to the saved grid spot) **if one is set**; without one it stops the montage where it is and ends the action (`ActionExecutor.cpp:2884-2900`), which leaves an attacker who dashed in standing at the target. Backlog **B-423** closes the gap. Also: the comments on both `bInterruptable` fields still say "parried OR dodged"; the rule (ED-37c, B-367b) is Perfect Guard only.

**Where interrupt points go:**
- A **one-hit** skill gains nothing: there is no "rest of the attack" to end, the return plays anyway. So no interrupt point on the light hit.
- On **multi-hit** skills (heavy combo, finisher), mark the strike before the big blow: guarding it perfectly cancels the finisher. One per skill to start; more only on boss attacks.
- **Enemy use:** the player's main reward for timing (E33's parry gives the counter; here, cancelling the rest of the combo).
- **Player use:** only matters if enemies can Perfect Guard (AI defence). Author the same points; they do nothing until then.
- Proposed per candidate below.

**Rituals have two animations (Crown):**
- **Arm:** an action like a cast (e.g. a prayer, with VFX and SFX, no strike). It has no Impact, so no interrupt point.
- **Trigger** (the later Execution turn): a separate attack animation that holds the strikes, and the interrupt points.
- Today `RitualCastMontage` plays straight before `SkillMontage` in one chain (`SkillDataBase.h:321-327`). Whether the armed (deferred, ET-16) path plays it on the arm turn and the strike on the trigger turn is not checked here (to check when the first ritual is built).

## All sword and shield in-place attacks (DSAS V1 / V2), screened

Root drift is 0 on every clip (truly in place). "Stand" = pelvis 93 cm, the normal standing pose; 76 = the low guard stance of the V1 thrusts; lower start values are combo pieces that begin from the previous piece's end pose.

| Clip | Length | Airborne | Start / end pelvis | Main hit (s) | Strike shape |
|---|---|---|---|---|---|
| V1 `Slash1` | 2.00 | 0.47–0.93 | 93 / 93 | 0.95 | leap, slash on landing, long crouched hold |
| V1 `Stab1` | 1.25 | – | 76 / 76 | 0.40 | forward thrust, feet switch places |
| V1 `Stab2` | 1.05 | – | 76 / 76 | 0.45 | forward thrust, small step |
| V1 `Kick` | 1.20 | – | 93 / 93 | – | kick (no sword strike) |
| V1 `Combo1_01` | 1.35 | 0.30–0.78 | 93 / 93 | 0.825 | jump, overhead smash into the ground |
| V1 `Combo1_02` | 1.62 | – | 70 / 93 | 0.375, 0.60, 1.00 | three forward cuts (combo middle) |
| V1 `Combo1_03` | 1.30 | – | 73 / 93 | – | slow finish, no clear strike |
| V1 `Combo2_01` | 1.23 | – | 93 / 93 | 0.70 | crouch, horizontal sweep right to left |
| V1 `Combo2_02` | 1.70 | – | 73 / 93 | 0.60 | weak (combo end) |
| V1 `Combo3_01` | 1.82 | – | 76 / 76 | 0.375 | forward thrust from a deep crouch |
| V1 `Combo3_02` | 1.82 | – | 63 / 93 | 0.65 | downward forward chop |
| V1 `Combo3_03` | 2.00 | 0.53–0.93 | 46 / 93 | 0.50, 0.85 | leap, backward-travelling cuts |
| V1 `Combo4_01` | 1.32 | – | 93 / 93 | 0.50 | crouch, rising diagonal, sword held high |
| V1 `Combo4_02` | 1.05 | – | 89 / 93 | 0.375 | fast diagonal down-cut |
| V1 `Combo4_03` | 1.30 | – | 72 / 93 | 0.40 | downward side cut |
| V1 `Combo5_01` | 2.23 | 0.85–1.38 | 93 / 93 | 1.40 | big leap, downward slam |
| V1 `Combo5_02` | 1.65 | – | 63 / 93 | 0.75 | backward-pulling cut |
| V1 `Combo5_03` | 1.25 | – | 67 / 93 | 0.40 | sideways down-cut |
| V1 `Combo6_01` | 1.77 | 0.30–0.75 | 93 / 93 | 0.775 | jump attack |
| V1 `Combo6_02` | 1.30 | – | 38 / 93 | 0.475 | rising side sweep from a kneel |
| V1 `Combo6_03` | 1.70 | – | 74 / 93 | 0.475 | diagonal down-cut |
| V2 `ComboAttack01_01` | 1.60 | 0.45–0.85 | 93 / 93 | 0.45 | rising cut, somersault, landing |
| V2 `ComboAttack01_02` | 1.43 | – | 59 / 93 | 0.20 | forward thrust (combo middle) |
| V2 `ComboAttack02_01` | 1.23 | – | 93 / 93 | 0.40 | raised sword, stepping diagonal chop |
| V2 `ComboAttack02_02` | 1.00 | – | 64 / 93 | – | no clear strike |
| V2 `ComboAttack02_03` | 0.80 | – | 62 / 93 | 0.175 | short forward cut (combo middle) |
| V2 `ComboAttack02_04` | 1.35 | – | 72 / 93 | 0.45, 0.725 | two cuts, the second pulling back |
| V2 `ComboAttack03_01` | 1.50 | 0.38–0.62 | 93 / 93 | 0.575 | hop, downward side cut |
| V2 `ComboAttack03_02` | 0.65 | – | 66 / 93 | 0.20 | short cut (combo middle) |
| V2 `ComboAttack03_03` | 1.20 | – | 76 / 93 | 0.50 | forward rising thrust |

**Flip and jump check (added after Crown's note, 2026-11-12).** The hand alone can miss a flip, so every clip was also checked by its torso: the angle of the pelvis-to-head line from vertical, whether the head ever drops below the pelvis, and both feet's height. (Pelvis Euler rotations wrap round ±180° on these clips, so they were not used.)
- **Upside down:** only V2 `ComboAttack01_01` (torso 122° from vertical at 0.75 s, head 35 cm *below* the pelvis): the backflip.
- **Deep leans (over 60°), no flip:** `Slash1`, `Combo3_03`, `Combo5_01`, `Combo5_02`, `Combo5_03`.
- **Everything else** stays within 57°; V2 `ComboAttack02_01` peaks at 34° with the head always ≥ 55 cm above the pelvis, feet never above 57 cm, no airborne frame. The full chain `ComboAttack02_IP` (3.03 s, 02_01 → 02_04 joined) has no flip either.
- Root rotation is 0 on every clip (no turn baked into the root).

**Skeleton check (intake rule, SkillPresentationPipeline §2):** V1 clips use `SK_Manny_DSAS1`, V2 clips `SK_Manny_DSAS2`, the pack's own skeletons. Confirm they play on our showcase character's skeleton (retarget or compatible skeleton) before the montage is built.

## Light hit (Destruction)

**Of the four first candidates, three are jump attacks:** `Slash1` (in the air 0.47–0.93 s), `Combo1_01` (0.30–0.78 s) and `V2 ComboAttack01_01` (0.45–0.85 s, a somersault with the feet above the head). They are heavy-hit and finisher material, not a light hit. The fourth, `Stab1`, is grounded but its feet switch places in a big scissor step; `Stab2` is the same thrust, cleaner. So the light hit was re-picked from the full screen above: grounded, starts and ends in the standing pose, one clear contact, ~1.2 s.

### Pick: V2 `Anim_SAS_V2_ComboAttack02_01_IP` ★

`/Game/CombatMasterAnimBundle/Animations/DSAS_V2/Manny_UE5/InPlace/Attack/Combo/Anim_SAS_V2_ComboAttack02_01_IP` (1.225 s, 49 frames, imported from `Anim_SAS_V2_ComboAttack02_01_IP.FBX`)

**Crown saw a backflip (2026-11-12).** The bone data shows none in this clip (flip check above). The clip that does backflip is `ComboAttack01_01`, which was open in the editor beside it. Please re-open this exact path to confirm by eye; if it still flips on screen, the preview is showing another asset or mesh, and the pick moves to runner-up 1.

**The motion:** the sword rises overhead and is held there (a clear tell), then the right foot steps forward and the blade comes down diagonally in front, ending low; a short hold, then the feet step back to stand. One blow, straight at the target.

| Phase | Time (s) | Frames | E33 hero swing |
|---|---|---|---|
| Wind-up (sword up and back; held high 0.15–0.30) | 0.00–0.30 | 0–12 | 0.7 s |
| Strike (step in, blade down) | 0.30–0.40 | 12–16 | — |
| **Contact** (hand furthest forward, 66 cm ahead) | **0.40** | **16** | ~0.1 s |
| Follow-through (blade ends low) | 0.40–0.45 | 16–18 | — |
| Hold (still, low) | 0.45–0.65 | 18–26 | ~0.3 s |
| Recovery (feet step back, rise to stand) | 0.65–1.15 | 26–46 | ~0.4–0.5 s |
| End | 1.225 | 49 | — |

Root drift 0; the pelvis drops 31 cm in the lunge and comes back; starts and ends in the pack's standing pose (pelvis 93). The combat idle is a lower guard stance (pelvis 76), so it needs a ~0.2 s blend into idle; with an approach and return (below) it never touches idle directly. Feet: the front foot plants at the contact frame (16) and settles ~10 cm by frame 20; the back foot drags ~35 cm forward close to the floor (0.40–0.50 s), a natural catch-up step but worth a look in PIE for sliding.

**Against E33:** contact, hold and recovery are close (0.2 s hold vs 0.3, 0.5 s recovery vs 0.4–0.5). The wind-up is **less than half** E33's (0.3 vs 0.7 s): right for a light hit (snappy), but tight if an enemy uses it (below).

**Camera:** the strike runs forward and down along the line to the target, so it is foreshortened from behind the attacker. It reads best in the side or three-quarter medium two-shot (study R6), which is our Impact shot already. No cinematic window: contact at 0.40 s is too early for the 0.75 s read rule, and a light hit gets none anyway (FeelGuide 2).

**Notifies:**

| Notify | Time (s) | Frame | What it drives |
|---|---|---|---|
| `VFX0` | 0.30 | 12 | blade trail / slash arc for the strike (every defendable swing needs a visible arc, study V2) |
| `SFX0` | 0.325 | 13 | swing whoosh |
| `Impact0` | 0.40 | 16 | damage, hit VFX by outcome (code), hit SFX, ~40 ms hit-stop and the small camera punch (code); the front foot's step lands here too |
| `SFX1` | 1.10 | 44 | foot step back |
| Telegraph on `Impact0` | lead 0.3 s | — | only if enemies use this skill: a cosmetic glint while the sword is held high |
| Defence window | — | — | none to place: code opens it 0.5 s before `Impact0` |
| Blend-out (montage setting, not a notify) | 0.95–1.225 | 38–49 | 0.25 s blend back to idle; the next action can start from 0.95 s |

`HitCount` 1, so only `Impact0` (B-368 passes). **Interrupt point: none** (one hit, nothing left to cancel), for player and enemy use alike.

**Skill that fits the motion:** a **single-target, one-hit downward chop**: low cost, small flinch on the target, no area. The overhead tell and one clean blade-down are its identity. If an enemy uses it, the defender sees only 0.4 s from the first move to contact (less than E33's ~0.7 s), so either keep the telegraph above or play it at ~0.85 speed for enemies (`TODO:` a design call, not taken here).

### Runner-up 1: V1 `Anim_SAS_Stab2_IP`

| Phase | Time (s) | Frames |
|---|---|---|
| Settle / small shift | 0.00–0.20 | 0–8 |
| Wind-up (hand draws back) | 0.20–0.35 | 8–14 |
| **Contact** (arm fully out, 73 cm ahead) | **0.45** | **18** |
| Hold (arm extended) | 0.45–0.65 | 18–26 |
| Recovery | 0.65–1.00 | 26–40 |
| End | 1.05 | 42 |

Feel: a quick jab with almost no tell; from behind it is a pure push away from the camera, so it reads small unless shot from the side. Fits: a **single-target piercing** hit (e.g. ignores some armour). Starts and ends in the low guard stance (pelvis 76), not the standing pose: needs a matching idle or a 0.2 s blend in and out. Notifies: `VFX0` 0.375 (f15), `SFX0` 0.40 (f16), `Impact0` 0.45 (f18), blend-out from 0.85 s. Interrupt point: none (one hit).

### Runner-up 2: V1 `Anim_SAS_Combo2_01_IP`

| Phase | Time (s) | Frames |
|---|---|---|
| Wind-up (crouch, sword drawn to the right side; held 0.35–0.65) | 0.10–0.65 | 4–26 |
| **Contact** (sweep crosses the front, right to left) | **0.70** | **28** |
| Follow-through | 0.70–0.75 | 28–30 |
| Hold (low) | 0.75–0.85 | 30–34 |
| Recovery (rise to stand) | 0.85–1.20 | 34–48 |
| End | 1.225 | 49 |

Feel: a long, readable coil (0.55 s, close to E33's 0.7) and then a whip-fast sweep that lasts only two frames, so the swing itself can flicker. Fits: a **horizontal cleave**, every enemy in the front row, light damage each, better as its own skill than as the light hit. Starts and ends standing. Notifies: `VFX0` 0.65 (f26), `SFX0` 0.65 (f26), `Impact0` 0.70 (f28), blend-out from 1.0 s. Interrupt point: none (one hit).

### Rejected

| Clip | Why |
|---|---|
| V1 `Slash1` | jump attack, 2.0 s, crouched hold to 1.5 s; too slow for a light hit (finisher candidate) |
| V1 `Combo1_01` | jump with the sword overhead, smash into the ground at 0.825 s; a clean **heavy hit** candidate |
| V2 `ComboAttack01_01` | rising cut into a somersault; acrobatic, not a knight with a shield |
| V1 `Stab1` | same thrust as `Stab2` but the feet scissor ~90 cm; `Stab2` is cleaner |

### Reaching the target: approach → strike → return

**Superseded in part (2026-11-12):** the 2×/3× warp below distorts the clips; see "Travel without stretching" for the recommended air-burst dash. Light-hit pick confirmed by Crown: `ComboAttack02_01_IP`.

**What the game does today** (code read, not changed):
- There is no separate approach step. The skill montage itself must carry the move (`ActionExecutor.h:1074`).
- Before the montage, the game sets a Motion Warping target named `WarpTarget` 100 cm in front of the target (`ExecutionRange`, `ActionExecutor.cpp:5192-5212`). Only a **root-motion section with a warp window** moves the attacker there. An in-place clip ignores it, so **an in-place strike alone never reaches the target**.
- Chain: `[RitualCastMontage] → SkillMontage → [ReturnMontage]` (`SkillDataBase.h:310-340`). The return montage is warped back to the saved grid spot (`ActionExecutorMontage.cpp:188-220`). With no return montage the attacker stays where the strike ended.
- Distance: teams stand 800 cm apart (`CombatGridConstants.h:69`), cells 200 cm. With the 100 cm stop, a front-row attacker travels **~700 cm** to a front-row target, more to the back rows.

**Clips that exist for sword and shield:** V1/V2 have only walks (V2 `Move_F_Root` 178 cm / 1.22 s, `Move_B_Root` 146 cm, `Turn_L180_Root` 0.80 s), no run or dash. The **DSAS V3** set (same style, same skeleton) has them:

| Clip (root motion) | Length | Travel | Shape | Use |
|---|---|---|---|---|
| ★ `Anim_DSS3_Dodge_F_RM` | 0.83 s | +354 cm | low hop 0.22–0.38, starts and ends standing | **approach** (dash-in) |
| ★ `Anim_DSS3_Evade_B_RM` | 0.83 s | −226 cm | backward skip, grounded, standing | **return** |
| `Anim_DSS3_Run_F_RM` | 0.67 s loop | +285 cm | run cycle (pelvis 86) | longer approach for back rows, if the dash stretches too far |
| `Anim_DSS3_DodgeAttack_RM` | 1.22 s | +313 cm | dash with a swing at 0.65 s | a dash-in attack on its own (later candidate) |
| `/Game/Data/Skills/Weapons/Anim_Approach_Leap` / `Anim_Return_Leap` | 1.40 s each | from `Anim_SAS_Combo1` (377 cm) | the existing project test montages: a jump | heavy-hit approach (a leap reads heavy) |

All paths: `/Game/CombatMasterAnimBundle/Animations/DSAS_V3/Manny_UE5/RootMotion/{Dodge,Evade,Locomotion}/`.

**Proposal (no code change):** the light hit's skill montage = two sections, `Dodge_F_RM` (warp window over its whole travel, 0.00–0.75 s) then the in-place strike; return montage = `Evade_B_RM`.
- **Join:** `Dodge_F` ends in exactly the strike's start pose (pelvis 93, sword hand within 1 cm), so a **0.1 s blend** is enough. Idle → `Dodge_F` and `Evade_B` → idle each need ~0.2 s (idle is the lower guard stance, pelvis 76).
- **In range:** yes. The in-place strike reaches ~66 cm (hand) + ~80 cm (blade) ≈ 146 cm forward, past the 100 cm stop.
- **Why not the strike's own root-motion version** (`ComboAttack02_01_Root`, 116 cm lunge by contact): starting at the 100 cm stop it would run through the target, and warping only that 0.1 s lunge over 700 cm would look like a teleport.
- **Check in PIE:** the dash is stretched ~2× (354 → ~700 cm) and the return ~3× (226 → ~700 cm). If that floats, use `Run_F_RM` ×N then the strike for the way in, and `Turn_L180` + `Run_F` + turn for the way back.
- **Camera:** the 0.25 s camera rush (B-403) is timed for a wind-up with no approach. With a 0.83 s dash first, it should start with the dash.

**Whole action, pick** (strike starts at 0.73 s after the 0.1 s blend; return starts at the strike's blend-out):

| Segment | Time (s) | E33 hero (study §14) |
|---|---|---|
| Approach (`Dodge_F_RM`, warped) | 0.00–0.83 | rush 0.2 |
| Strike wind-up | 0.73–1.03 | 0.7 |
| **Contact** `Impact0` | **1.13** | ~1.2 after confirm |
| Hold | 1.18–1.38 | 0.3 |
| Recovery | 1.38–1.68 | 0.4 |
| Return (`Evade_B_RM`, warped) | 1.68–2.51 | — |
| **Total** | **~2.5** | **~2.8** |

Notifies in whole-action time: `VFX0` 1.03, `SFX0` 1.055, `Impact0` 1.13. `SFX1` (foot step back, 1.83 in whole-action time) falls inside the return and can be dropped. A dash `SFX` on the approach is optional.

**Runners-up in the same frame** (same approach and return):

| Option | Join blend | Strike | Contact | Return | Total |
|---|---|---|---|---|---|
| `Stab2_IP` | 0.2 s (starts in the low guard, pelvis 76) | 0.63–1.48 | 1.08 | 1.48–2.31 | ~2.3 s |
| `Combo2_01_IP` | 0.1 s | 0.73–1.73 | 1.43 | 1.73–2.56 | ~2.6 s |

**`Stab2` has no reach of its own:** its root-motion version moves 22 cm out and back. As a light hit it only works with the dash in front. Better as its **own ranged skill candidate**: a thrust from the grid spot that releases a pressure wave or projectile at contact (0.45 s). No approach or return, ~1.05 s total.

### Travel without stretching (research, 2026-11-12)

**Crown's concern:** warping the 354 cm dash over ~700 cm (2×) and the back-step 3× distorts the clips. Runs are ruled out (they break the cinematic feel). The question: how to keep approach → strike → return one continuous flourish with the clips we have.

**Key fact:** what looks wrong in a stretched clip is mostly **feet sliding on the floor** while the body moves faster than the steps. A body in the air has no feet to slide. Motion Warping's Skew Warp only bends root motion **inside its window** and reaches the target at the window's end ([UE docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine)). Put the window only over the airborne frames and the extra distance goes where nobody can see feet. Frames on the ground play at their own speed.

**Airborne windows measured** (both feet off the floor):

| Clip | Length | Travel | Off the ground | Travel in the air |
|---|---|---|---|---|
| V3 `Anim_DSS3_Dodge_F_RM` | 0.83 s | +354 cm | 0.18–0.42 s | 155 cm |
| `HandsomeSwordAndShield/SwordAnimations/Evade/Anim_HSAS_Evade` | 1.27 s | −192 cm | 0.17–0.40 s | 76 cm (back hop) |
| `HandsomeKatanaAnims/.../Evade/Katana_Dodge` | 1.50 s | +378 cm | 0.10–0.47 s | 216 cm |
| V3 `Anim_DSS3_Evade_B_RM` | 0.83 s | −226 cm | never (feet stay down) | — |

`Evade_B` has no airborne frames, so any warp on it slides the feet: that's the 3× problem. Replace it.

**Options** (runs ruled out):

| Option | How it looks | Clips | Cost | Limits |
|---|---|---|---|---|
| ★ **A. Air-burst dash:** warp only the hop (0.18–0.42); hop VFX smear; camera rush on the same 0.24 s | One dash; the hop becomes a burst (~2,100 cm/s front row, ~3,000 back row) and lands at striking range. The ground steps are untouched | `Dodge_F_RM`, strike, `HSAS_Evade` back | Small: one montage, warp-target offset (below), a dash-smear VFX | Very long hops read as a blink, not a jump: needs the smear to sell it. Retarget check on `HSAS_Evade` (another pack) |
| B. Two chained dashes | Two hops, ~1.6 s; reads as a skip-in | `Dodge_F_RM` ×2 (708 cm ≈ front row exactly, no warp) | Montage only | Twice E33's rush time; back rows still need warp (split over two airborne windows, ~1.3×) |
| C. Camera cut hides the travel | Shot on the attacker's hop, cut to the two-shot at the target; travel happens off screen | `Dodge_F_RM` or none | Small: one camera cut on the CombatCamera beat | Breaks the single continuous flourish Crown wants; good as a fallback for very long distances |
| D. Closer spacing | `Dodge_F` covers it unwarped | none | Changes `TEAM_SEPARATION` 800 → ~450: the camera, ranged readability and every layout | Back rows still 200–400 cm further. A battle-layout decision, not an animation one |
| E. Lunge or leap attacks, warped a little | Travel is part of the attack | `ComboAttack02_01_Root` (116 cm by contact), `HSAS_Slash_Dodge` (427 cm, swing 0.80), `Sword_1H_Attack_Jump1` (242), V1 `Combo1` (377) | Montage only | None covers 700 cm alone, so they still need A in front. Best for the heavy hit (a leap reads heavy) |

**UE tools checked and not needed:**
- Distance Matching: for run start, loop and stop, so it's ruled out with runs.
- Stride Warping: fixes foot length when ground speed changes; not needed when only airborne frames warp.
- Pose Search / Motion Matching: needs a large locomotion database; we have five travel clips.
- Play rate: speeds up the whole pose, not just the distance. ≤1.2× on the approach is fine for timing.

**Other games** (from footage, not measured): E33's hero closes in a fast rush of ~0.2 s from confirm while the camera rushes along (study §14). Persona 5, Metaphor and Honkai: Star Rail use a quick dash or leap with a fast camera follow or cut, then a fixed-length strike, and a quick hop or cut back. All of them make the travel **short and stylised**, not a natural run, which is what A does.

**Recommendation: A (air-burst dash), with C as the fallback past a set distance.**

Whole action, light hit:

| Segment | Time (s) |
|---|---|
| Dash (`Dodge_F_RM`); burst 0.18–0.42 | 0.00–0.83 |
| Strike (`ComboAttack02_01_IP`), 0.1 s join | 0.73–1.95 |
| `Impact0` | **1.13** |
| Back hop (`HSAS_Evade`); burst 0.17–0.40 of the clip | 1.68–2.95 |
| **Total** | **~2.9** (E33 ~2.8) |

Joins: `Dodge_F` ends in the strike's start pose (within 1 cm). The strike's end pose matches `HSAS_Evade`'s start within 3 cm.

**Prototype plan for the Code Writer** (light hit only):
1. **Montage** (editor): `Dodge_F_RM` section, then a `ComboAttack02_01_IP` section with a 0.1 s blend. A Motion Warping notify state (Skew Warp, target `WarpTarget`, translation and rotation) over **0.18–0.42 s only**. Notifies `VFX0` 1.03, `SFX0` 1.055, `Impact0` 1.13 in montage time.
2. **Warp target offset** (code, `ActionExecutor.cpp:5192-5212`): the clip still travels ~124 cm after the window, so aim the warp target that much short of the stop point. Read the post-window travel from the montage's root motion, not a constant. First check whether UE 5.8's warp modifier already has an option to reach the target at the animation's end.
3. **Return montage**: `HSAS_Evade`, window 0.17–0.40 to the grid spot, same offset. Check the retarget (Handsome pack) by eye.
4. **Feel:** align the B-403 camera rush (0.25 s) with the burst window. Add a dash-smear or afterimage VFX over the burst. **Missing asset:** a dash-trail Niagara, to make or find.
5. **Debug:** log the warp distance and burst speed per action. Over a threshold (`TODO:` tune, ~3,500 cm/s), use the camera cut (C) instead.
6. **PIE:** attack a front-row and a back-row enemy. Check that the feet never slide, that the burst reads as a dash not a teleport, and that the join into the swing and the hop back are seamless.

**Contact-pose pictures:** later, when Crown is at the PC (editor preview must be in front to redraw).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-12 | Created: method, screen of all DSAS in-place attacks, light hit pick (V2 ComboAttack02_01) with runners-up | docs/showcase-anim-analysis |
| 2026-11-12 | Crown's notes: flip and jump check by torso tilt (only ComboAttack01_01 flips; the pick has none), Interrupt notify proposal (fit, SB-36 conflict, placement) | docs/showcase-anim-analysis |
| 2026-11-12 | Approach → strike → return: current movement code, V3 dash/evade clips, join blends, whole-action timings; Stab2 flagged as a ranged-thrust candidate; idle-stance correction | docs/showcase-anim-analysis |
| 2026-11-12 | Interrupt reworked per Crown: a marker on the Impact notify for D-161's interrupt points (not a ritual cancel); today's authoring, merge proposal, return-on-interrupt gap (B-423), ritual arm/trigger split | docs/showcase-anim-analysis |
| 2026-11-12 | Travel without stretching: airborne-window warp (air-burst dash) recommended, options compared, prototype plan; pick confirmed; B-412 renumbered B-423 | docs/showcase-anim-analysis |
