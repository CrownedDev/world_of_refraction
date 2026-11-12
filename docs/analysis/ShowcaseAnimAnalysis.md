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

### Interrupt notify (Crown's proposal, 2026-11-12; not built)

**What Crown asked for:** a strike notify that lands the hit and, at that frame, cancels or staggers the **target's** charge, cast or ritual.

**How it fits what exists:**
- Today's "interrupt" runs the other way: a defender's **Perfect Guard on an interrupt point** (`bInterruptable` on a hit or cast entry) ends the **attacker's** skill (D-161, ED-37c, `GuardRules.h:46`, `ActionExecutor::InterruptAsyncAction`). The new one is the attacker's hit stopping the target. Keep the names apart: suggested **"Interrupt"** for the new notify and keep calling the old one an **"interrupt point"**.
- In this turn-based game a target is never mid-swing during your turn. What it can have in progress is an **armed ritual** (a deferred spell waiting for its Execution turn, ET-16). Infusion "charge" (ED-10) is picked with the action, not held. So in practice the Interrupt hits a ritual (and any future boss charge-up).
- **Conflict to decide (Crown):** SB-36 says a ritual armed before a stun **still goes off**. An Interrupt that cancels rituals changes that. Options: cancel the ritual; delay it a turn; or weaken it (drop its charge level). Not decided here.
- **Shape (proposal for the Code Writer's job):** keep it an **Impact** notify so the hit count, B-368's `0..HitCount-1` order and the outcome VFX all stay as they are. Either a flag on the notify (label `Interrupt<N>` in the montage editor, still counted as `Impact<N>`) or a flag on the skill's hit entry N, the way `bInterruptable` already lives on hit entries. The hit entry is the better home (data, per skill, readable by the AI); the label is an authoring aid.
- **Defence:** it lands only if the hit lands; a successful Guard or Dodge on that impact cancels the interrupt (same rule as any on-hit effect, to confirm).

**Where it goes:** not on light hits (it would make the cheapest action the ritual answer). Best fits: the **crippling strike** (stagger is its job) and the **heavy hit** (an overhead break). Proposed per skill as each is analysed.

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

Root drift 0; the pelvis drops 31 cm in the lunge and comes back; starts and ends in the standing pose (pelvis 93), so it blends straight back to idle. Feet: the front foot plants at the contact frame (16) and settles ~10 cm by frame 20; the back foot drags ~35 cm forward close to the floor (0.40–0.50 s), a natural catch-up step but worth a look in PIE for sliding.

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

`HitCount` 1, so only `Impact0` (B-368 passes). Interrupt: **none proposed** on the light hit; if Crown wants one, it replaces `Impact0` at the same frame (0.40 s, f16).

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

Feel: a quick jab with almost no tell; from behind it is a pure push away from the camera, so it reads small unless shot from the side. Fits: a **single-target piercing** hit (e.g. ignores some armour). Starts and ends in the low guard stance (pelvis 76), not the standing pose: needs a matching idle or a 0.2 s blend in and out. Notifies: `VFX0` 0.375 (f15), `SFX0` 0.40 (f16), `Impact0` 0.45 (f18), blend-out from 0.85 s.

### Runner-up 2: V1 `Anim_SAS_Combo2_01_IP`

| Phase | Time (s) | Frames |
|---|---|---|
| Wind-up (crouch, sword drawn to the right side; held 0.35–0.65) | 0.10–0.65 | 4–26 |
| **Contact** (sweep crosses the front, right to left) | **0.70** | **28** |
| Follow-through | 0.70–0.75 | 28–30 |
| Hold (low) | 0.75–0.85 | 30–34 |
| Recovery (rise to stand) | 0.85–1.20 | 34–48 |
| End | 1.225 | 49 |

Feel: a long, readable coil (0.55 s, close to E33's 0.7) and then a whip-fast sweep that lasts only two frames, so the swing itself can flicker. Fits: a **horizontal cleave**, every enemy in the front row, light damage each, better as its own skill than as the light hit. Starts and ends standing. Notifies: `VFX0` 0.65 (f26), `SFX0` 0.65 (f26), `Impact0` 0.70 (f28), blend-out from 1.0 s.

### Rejected

| Clip | Why |
|---|---|
| V1 `Slash1` | jump attack, 2.0 s, crouched hold to 1.5 s; too slow for a light hit (finisher candidate) |
| V1 `Combo1_01` | jump with the sword overhead, smash into the ground at 0.825 s; a clean **heavy hit** candidate |
| V2 `ComboAttack01_01` | rising cut into a somersault; acrobatic, not a knight with a shield |
| V1 `Stab1` | same thrust as `Stab2` but the feet scissor ~90 cm; `Stab2` is cleaner |

**Contact-pose pictures:** later, when Crown is at the PC (editor preview must be in front to redraw).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-12 | Created: method, screen of all DSAS in-place attacks, light hit pick (V2 ComboAttack02_01) with runners-up | docs/showcase-anim-analysis |
| 2026-11-12 | Crown's notes: flip and jump check by torso tilt (only ComboAttack01_01 flips; the pick has none), Interrupt notify proposal (fit, SB-36 conflict, placement) | docs/showcase-anim-analysis |
