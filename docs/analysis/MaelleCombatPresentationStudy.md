# Combat presentation study: Expedition 33, Painted Maelle (Alicia) fight

Sections: **Camera** (§1–§3), **VFX** (§6), **UI** (§7), **Feedback and timing** (§8). Unclear points (§4) and questions (§5) cover all four. Earlier name: `MaelleCameraStudy.md`.

**Purpose:** research for later combat-camera jobs. What the camera does in Expedition 33's Alicia / Painted Maelle fight, the patterns behind it, and what that means for our camera (`docs/Architecture/CombatCamera.md`, `docs/Design/CombatCamera.md`).
**Source:** YouTube `LItGIL9XfzE` ("Alicia Secret Boss Fight (4K)"). The fight runs **5:27 → 7:00** (about 90 s, one phase, then the victory screen and a long cutscene). Frames were stepped about every 1 s, denser around key moments.
**Date:** 2026-11-09. Docs only, no code.

Fight set-up, for reading the table: Maelle alone against **Painted Maelle**, a dark fencer who looks like her (white hair, rapier). A giant doll ("Alicia") sits on a throne of broken wood far behind the arena, framed in almost every shot. Turn order runs down the left edge; the boss bar is top-centre; the enemy's stance panel sits on the right.

## 1. Shot table

| Moment | Camera shot | What triggers it | Time(s) |
|---|---|---|---|
| Fight intro | Black-and-white cutscene of the throne with two fencers; then a low shot skimming the floor boards as the fight starts | Encounter start | 5:18, 5:27 |
| Opening wide | High, wide, from behind and left of Maelle. She is small in the middle, the throne fills the back. Slow drift in | Fight loaded, before the first menu | 5:28–5:29 |
| **Hero turn start / main menu** | Behind Maelle, over her shoulder, at hip-to-chest height. She sits in the **lower-left third**, back to us; the enemy and throne sit **centre / centre-right**, far. Medium-wide lens. Menu floats beside her | Her turn begins | 5:30–5:31, 5:55, 6:43 |
| **Skill list open** | Same side, **closer and lower**: she fills the lower-left corner (head and shoulders, cut by the frame), enemy still centred | Opening the skill sub-menu | 5:32.7, 5:56–5:57, 6:45–6:51 |
| Browsing / picking a target | **No camera move.** The frame stays still while she scrolls; only the menu changes | Scrolling skills / target | 6:45–6:51 |
| **Melee skill: confirm** | A fast **forward rush** with heavy motion blur, following her as she dashes at the enemy | Skill confirmed | 5:33.8, 6:52.7 |
| **Melee skill: hit** | Medium shot from the side / three-quarter front, both fighters in frame, low. Big orange damage number at the hit point, a "weakness" tag | Each landed hit | 5:35.4–5:36.4 |
| **Big skill (special)** | Wind-up: camera low and close behind her, slight tilt. Leap: **tilted (dutch) angle**, low, close, wide lens. Impact: **very close**, the effect (purple flash, light bars) fills the frame. Then a **blur whip** back out | Signature skill confirmed; timed button prompts during it | 5:59–6:03 |
| Area / fire skill on the boss | Mid-wide, behind and left of her; she is small, the boss engulfed in fire in the centre. Camera **holds still** so the whole effect fits. The timed-press ring sits beside the boss | Skill with timed presses | 6:53–6:56 |
| Return to turn view | Blur whip, then a wide reaction shot of the enemy, then back behind Maelle as she walks back to her spot | Her action ends | 6:03–6:04, 5:53–5:54, 6:41 |
| **Enemy turn start (buff)** | **Close, full-body front shot of the enemy**, low camera, red aura filling the screen. Ability name banner at the top | Enemy uses a self-buff | 6:06–6:07 |
| **Enemy attack: approach** | Wide or mid-wide from behind / beside the defender; the attacker walks or leaps in from centre-right | Enemy attack begins | 5:37–5:38, 6:08, 6:20 |
| **Enemy attack: the defend window (parry / dodge)** | **Low side-on two-shot**: both fighters in frame, one in the near foreground (often cut by the left edge), the other mid-ground, the throne behind. Camera mostly **holds or slides slowly**; a few clean cuts between side angles across a long combo. Never close enough to lose the attacker's sword | Each swing of the enemy combo | 5:38–5:52, 6:10–6:15, 6:21–6:31 |
| Parry success | "PERFECT" text at the defender; a bright white streak; on some, the frame **goes black-and-white** for an instant | Perfect parry / dodge | 5:38.9, 6:10.8, 6:12.7, 6:39 |
| Counter / damage | White numbers (hundreds) during the combo; large orange numbers (tens of thousands) on the enemy, usually in a wide shot | Counter-attack after parries | 5:40, 5:51, 6:16, 6:33 |
| Enemy hit reaction | Quick **push-in** to a close shoulder shot of the enemy staggering | Enemy hit hard | 6:17 |
| Enemy wind-up of a new attack | Medium front shot of the enemy, golden glow behind | Enemy starts its next attack | 6:18–6:19 |
| Between enemy beats | Back to a **wide** shot from high and behind | Enemy resets / jumps back | 5:45, 5:53, 6:20, 6:32 |
| Kill / phase end | Dark vignette, colour drained, wide and still; the boss collapses in the centre. Victory panel over the same wide shot | Final blow | 6:57–7:02 |
| Phase change | **None in this fight**: it ends in one phase (boss killed in about 3 rounds) | — | — |

## 2. The patterns, in plain words

1. **Our side owns the near-left of the frame; the enemy owns the centre.** On every player turn the camera sits behind the hero's shoulder, hero bottom-left, enemy in the middle. The direction never flips, so the player always knows where "us" and "them" are.
2. **Menus are calm.** While you are deciding, the camera only changes once (a step closer when you open the skill list) and then stays dead still. No drifting while you read.
3. **Confirming is the loud moment.** The instant a skill is chosen, the camera rushes forward with blur. The cut from "still" to "fast" is what makes the button press feel strong.
4. **Hits are shown from the side, close enough to feel, wide enough to read.** Normal hits use a side or three-quarter medium shot with both fighters in it. Only the big skill goes tight, tilted and frame-filling.
5. **Big effects get room.** For area or fire skills the camera pulls back and holds, so the whole effect and the timing ring fit on screen.
6. **Defending is about reading, not drama.** During enemy combos the camera is low, side-on and fairly steady, with the attacker's body and weapon fully visible. Few cuts, no close-ups, no shake during the swings. Feedback comes from text, flashes and the brief black-and-white frame, not from moving the camera.
7. **Wide shots are the breathing room.** Between beats (attack done, enemy jumps back, turn handover) the camera goes wide and high. That resets the player's sense of the whole arena.
8. **The enemy gets a "hero shot" on its own turn.** A self-buff or a new attack's wind-up is shown close and from the front, so the threat is announced before the swings start.
9. **The boss is framed with its setting.** The throne and the giant doll stay in the back of nearly every shot, and wide shots put the boss small against it. Scale comes from the backdrop, not from tilting up at the boss. (There are no normal enemies in this fight to compare against; see §4.)
10. **Damage numbers sit in the shot, not on a HUD.** They appear at the hit point and are big enough to read in a wide shot.

## 3. Rules for our camera

Mapped to our turn flow: turn start → menu (one or more actions, B-170 action pips) → skill → shot patterns (B-371) → enemy turn → cinematic window → Defence view (B-370) → next turn.

| # | Rule | What to change or add |
|---|---|---|
| R1 | **Turn view: hero lower-left, enemies centre.** | Make `OverTheShoulder` frame the actor in the lower-left third, enemy group centred. Crown chose `CentredWide` as default (D-074); keep that, but this study argues for testing OverTheShoulder as the default in PIE (Q below) |
| R2 | **One step closer on sub-menu.** | Add a "menu depth" input to the Turn view: opening Skills / Items moves the camera a short step closer and lower (blend ~0.3 s). Back out on cancel. Needs a HUD → director event (`OnMenuDepthChanged` or similar) |
| R3 | **Still while choosing.** | No camera motion while the menu or target picker is open: no drift, no idle sway. Target cycling should **not** move the camera (it didn't in the video); highlight the target instead. If a target is off-screen, a small re-aim is allowed |
| R4 | **Action pips: don't reset between actions.** | With several actions per turn (B-170c), after each action return to the **same** turn view of the same character, not a fresh blend from wide. Only the turn change (`OnActorTurnStarted`) does the full move |
| R5 | **Confirm = fast rush.** | B-371 WindUp is a "slow push in". Add a short (0.2–0.3 s) fast push toward the target with motion blur for melee skills on confirm. Keep the slow push for spells (Cast) |
| R6 | **Normal hit = side medium two-shot.** | B-371 Impact: frame caster and target from the side / three-quarter front, both in frame, low height. Keep the small FOV punch; no big shake on normal hits |
| R7 | **Big skills earn tight shots.** | Hand-made sequences (or a `CameraTuning` "Signature" style) for signature skills only: tilt, close, frame-filling impact, then a quick whip back. Normal skills must not use these |
| R8 | **Area effects pull back and hold.** | B-371 Spread already pulls out until all targets fit; also make it **hold still** for the whole effect, and keep any timing prompt on screen |
| R9 | **Enemy turn opens on the enemy.** | At an enemy turn start, a short front medium shot of the acting enemy (about 1 s) before its attack, especially for buffs and new big attacks. Today NotYourTurn is just "pan out wide" |
| R10 | **Defence view = low side-on two-shot, steady.** | B-370 Defence view is "behind that character, attacker in front". The video shows something wider and more side-on: both fighters fully in frame, attacker's weapon always visible. Make the Defence offset more side-on and lower; **no shake and no FOV punch** during defend windows; cut angles only between swings, never inside one. Keep the 0.75 s read time |
| R11 | **Parry feedback is not camera motion.** | A perfect parry/dodge gets text, a flash, maybe a brief colour drain; the camera stays put. Any hit-stop or shake waits until the combo ends |
| R12 | **Wide reset between beats.** | After an action or an enemy combo finishes, blend through a wide high shot (or cut to it) before the next turn view. This is our NotYourTurn wide, used as a short beat, not a whole turn |
| R13 | **Bosses framed with their backdrop.** | For boss fights, Turn and wide shots keep the boss and its set piece in frame; allow a per-encounter "backdrop point" the wide shot keeps in view. Don't tilt up at the boss for scale |
| R14 | **Kill shot.** | On the final enemy death: wide, still, slight slow-down and colour drain before the victory UI. Small add to `EndCombat` timing |
| R15 | **Motion-off setting still works.** | Every rule above has to fall back to the static wide camera when `bCameraMovementOff` is on (rush, whips, tilts and enemy hero shots skipped) |

Suggested order: R1–R4 and R10 first (they shape every turn and are cheap tweaks to existing views); R5, R6, R8, R12 with B-371 tuning; R9, R11, R13, R14 later; R7 with the hand-made signature sequences.

## 4. What was unclear from frames alone

- **Who is who in some defend shots.** Both fighters are white-haired fencers, so in a few side-on shots it is not clear whether the near-foreground figure is the attacker or the defender. The overall framing (side-on, both in frame) is clear; the exact side is not.
- **Blend speeds.** Frames show positions, not how fast the camera blends. Times above (0.2–0.3 s rush, ~0.3 s sub-menu step) are estimates.
- **Lens values.** "Wide" and "tight" are by eye; actual field-of-view numbers can't be read.
- **What the player pressed.** It isn't always clear when a parry vs a dodge happened, or whether the black-and-white frame means a special counter.
- **Phase change and normal enemies.** This fight has neither: it ends in one phase, and Painted Maelle fights alone. Boss vs normal-enemy framing and phase-change cameras need another fight.
- **Target picking.** The player picks quickly with one enemy; with several enemies the camera might re-aim on target change. Not visible here.
- **Party turns.** Maelle fights solo, so handover between party members isn't shown.

## 5. Questions for Crown (suggested, not decided)

- Q1: Should the default turn view become over-the-shoulder (R1), replacing CentredWide (D-074)?
- Q2: Defence view: switch from "behind the defender" to the low side-on two-shot (R10)?
- Q3: Want the enemy "hero shot" at enemy turn start (R9) for every enemy, or bosses and big attacks only?

## 6. VFX

| Moment | What is shown | Time(s) |
|---|---|---|
| Melee hit | Element-coloured trail on the blade (purple-orange flame), a short spark burst at the contact point, then the damage number | 5:35–5:36 |
| Signature skill | Coloured flame column, vertical light bars, a full-screen purple flash at impact, debris | 6:00–6:02 |
| Area fire skill | A burst of orange streaks that swallows the boss, held for about 2 s | 6:53–6:56 |
| Enemy buff | Red aura and a red fog that floods the lower half of the screen | 6:06–6:07 |
| Enemy wind-up | Golden glow behind the attacker | 6:18 |
| Enemy swing | Pale white sword arcs (motion trails) that show each swing's path | 5:41, 5:47, 6:24 |
| Parry / dodge success | Short white streak or flash at the defender; "PERFECT" text; on some, the whole frame drains to black-and-white for an instant | 5:38.9, 6:12.7, 6:39 |
| Counter | Bright white beams cross the screen | 5:51 |
| Set dressing | Gold light threads drift through the arena all fight; they make every shot feel alive without moving the camera | throughout |
| Kill | Dark vignette and colour drain on a wide shot | 6:57–6:59 |
| Phase change | None in this fight | — |
| Screen shake | Little or none on normal hits and none during defence; motion blur is used on camera rushes instead | — |

**Patterns:** effects carry the element colour; enemy swings always leave a readable trail; the screen-wide effects (flash, colour drain, fog) are kept for big moments (signature hit, perfect parry, buff, kill).

**Rules for us:**
- V1: every skill hit = element-coloured trail + small hit burst. Big Pack Magic VFX already covers this per element (`*_Slash`, `*_Hit`; AssetAudit §2) for 7 elements.
- V2: every defendable enemy swing gets a visible weapon trail, so the swing reads in the Defence view (FeelGuide 14).
- V3: a landed parry = small element-coloured circle flash at the defender (Magic Circle 1, AssetAudit "parry-effect cosmetic") + sparks; no pause (FeelGuide 3). Sparks are missing: make a small Niagara burst.
- V4: enemy buffs = aura on the caster (`*_Aura` / `*_Buff` exist per element).
- V5: full-screen effects (flash, colour drain, vignette) only for crits/weakness breaks, perfect parry, and the kill. Screen effects are post-process, not assets.
- V6: arena ambience (drifting particles) per biome is cheap life; SoStylized wind lines / fog are a start.

## 7. UI

| Element | Where | Size / behaviour |
|---|---|---|
| Turn order | Left edge, vertical column of small portraits, top to bottom | Always on screen, small; the current actor sits at the top |
| Boss bar | Top centre, thin red bar with the boss name under it | Always on; very slim |
| Ability name | Small banner under the boss bar | Appears when any skill or enemy attack starts, gone when it ends |
| Player HP / energy | Bottom right: portrait, HP bar, energy pips, small numbers | Always on; compact |
| Special charge counter | Bottom right, above the player block ("0/3" style) | Always on |
| Enemy stance / status panel | Right side, middle: enemy portrait with stance word and a short list of effects | Shown on the player's turn; hidden in most enemy-turn shots |
| Action menu | Floats beside the hero, right of her body, a few short rows with face-button icons | Only on the player's turn; sub-menu (skills) opens as a wider list with cost and short text |
| Target marker | Not visible in our frames (only one enemy) | — |
| Damage numbers | At the hit point in the world; large for big hits (orange), smaller white for small/counter hits; stack when several land | Stay about 1–2 s, then fade |
| Defend prompt | No big prompt; the defence is read from the attack itself; feedback text "PERFECT" appears at the defender | Only after the input |
| Timed-press ring | Circle with a button icon beside the target during the player's special skill | Only during the skill |
| Hints | Bottom left, small button hints ("Back", "Skip") | Only when useful |

**Patterns:** HUD hugs the edges and corners; the centre stays clear for the fight. Menus attach to the hero, not to a fixed panel. Everything not needed in the enemy turn hides.

**Rules for us** (current widgets in `UISystem.md`):
- U1: keep `UTurnOrderStripWidget` but consider a vertical left-edge layout; current actor first.
- U2: `UCharacterPanelWidget` for the player goes compact, bottom right; enemy details move to a side panel shown on our turn only.
- U3: the action menu sits next to the acting hero (screen-projected), not fixed; with action pips, show remaining pips beside the menu.
- U4: add an ability-name banner under the boss bar for every skill, ours and the enemy's (no widget for this yet).
- U5: `UDefensePromptWidget`: keep the on-screen prompt minimal; the attack animation is the cue. Show result text ("Perfect", "Dodged") at the defender.
- U6: damage numbers in world space; colour by weakness (FeelGuide 5), size by hit strength.
- U7: hide the menu, enemy panel and hints during enemy turns and cinematics; keep HP, turn order and the boss bar.

## 8. Feedback and timing

- **Hit pause:** no visible freeze on normal hits in the frames (sampling at ~1 s can't see 40 ms). The signature hit holds on the impact for roughly 0.5 s with the screen flash.
- **Perfect parry:** text + flash, sometimes a black-and-white instant; the combo keeps flowing; no camera move.
- **Whose turn:** three signals together: the turn-order column updates, the camera moves behind the actor, and the menu appears beside them. On enemy turns, the ability-name banner plus a front shot of the enemy.
- **Pace:** each player action about 3–5 s from confirm to back in the turn view; an enemy combo about 10–15 s; the whole round about 25 s.

**Rules for us:**
- F1: keep FeelGuide 4 values (normal ~40 ms, big ~100 ms freeze); add a ~0.4 s hold for signature impacts.
- F2: never pause on defence (FeelGuide 3), matching the video.
- F3: turn start = strip highlight + camera move + menu, all on the same frame.
- F4: enemy turn start = banner + short front shot before the first swing (fits the 0.75 s read time).
