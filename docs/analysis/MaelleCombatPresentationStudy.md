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

Second pass (2026-11-09): frames re-captured from the 4K source, with close crops of effects. Times are ±0.5 s.

| Moment | What is shown | Time(s) |
|---|---|---|
| Fight start | The screen sweeps low over the floor in a dark, grainy shot, then opens on the arena. Gold light threads drift through the air for the whole fight (arena life, no gameplay meaning) | 5:27–5:28 |
| Skill confirm | Strong radial motion blur on the camera rush; no extra particle effect | 5:33.8, 6:52.7 |
| Melee hit (fire skill) | Orange-red flame burst at the blade's contact point with **purple glyph sparks** around it; the blade leaves a bright orange-to-purple trail | 5:35.9–5:36.3 |
| Signature skill: timed press | A gold **diamond with a button icon and a shrinking ring** sits beside the target; orange flame and light bars around it | 6:00.4–6:00.7 |
| Signature skill: impact | The **whole screen floods purple**, thin vertical light bars, dark debris flying outward; heavy flash | 6:02.2 |
| Area fire skill | A burst of orange streaks and embers swallows the enemy and hangs for ~2 s | 6:53.9–6:55.7 |
| Status on the enemy | Burn: flame icon and a "BURN" tag on the numbers, small embers on the body; no big looping aura | 6:00.7, 6:17.3 |
| Enemy buff | Red glow on the blade, then a **red fog** that floods the lower screen and a swarm of red sparks | 6:06.4–6:07.3 |
| Enemy wind-up | Golden glow / arc behind the attacker | 6:06.4, 6:18.5 |
| Enemy swings | White-gold **sword arcs** trace every swing; easy to read even in wide shots | 5:41–5:52, 6:21–6:31 |
| Perfect parry | At the clash point: a **white star-burst** (thin crossing lines) and a bright yellow-white streak, plus the word "PERFECT" (§7) | 5:38.9, 6:12.7 |
| Counter after parries | Red slash lines across the attacker, white beams across the screen | 5:40.1, 5:51.0 |
| Special counter | The frame turns **black-and-white** with a **radial burst and speed lines** centred on the defender, for a fraction of a second | 6:39.0–6:39.4 |
| Kill | Stacked gold numbers, then a dark vignette and colour drain on a wide shot; the victory panel opens over it | 6:57–7:00.6 |
| Phase change | None in this fight | — |
| Screen shake | Not visible in frames on normal hits or during defence. Motion blur and flashes do the work | — |

**Patterns:**
- The **attacker's element sets the colour** of trails, bursts and flashes (fire = orange, with the skill's own purple accent).
- **Every defendable swing leaves an arc.** The defender reads the swing from the trail, not from a prompt.
- **Screen-wide effects are rare** and each has one meaning: purple flood = signature hit, black-and-white burst = special counter, red fog = enemy power-up, vignette and drain = kill.
- **Statuses are quiet in the world** (small embers) and loud in the numbers and icons.

**Rules for our game:**
- V1: every skill hit = element-coloured weapon trail + one hit burst at the contact point. Our assets: Big Pack Magic VFX `*_Slash` and `*_Hit` cover 7 elements (`AssetAudit.md` §2); Void needs a recolour, Reality is missing.
- V2: every **defendable** enemy swing must have a visible trail (FeelGuide 14). Add a trail to each enemy attack montage; no trail means no readable defence.
- V3: perfect parry = a short star-burst at the clash point + "Perfect" text, no pause (FeelGuide 3). Assets: Magic Circle 1 tinted to the element (AssetAudit "parry-effect cosmetic"); the spark burst itself is **missing** (make a small Niagara burst).
- V4: one full-screen effect per meaning, and only these: signature hit (colour flood), special counter (colour drain + radial burst), enemy power-up (coloured fog), kill (vignette + drain). These are post-process materials plus timing, not pack assets.
- V5: enemy buffs = a short aura on the caster (`*_Aura` / `*_Buff` exist per element), then a small lingering effect, not a constant loop.
- V6: status effects in the world stay small (embers, drips); the status itself is shown by icons and number tags (§7). Blood and Poison sets cover bleed/poison markers; stun and silence markers are missing.
- V7: arena ambience (drifting particles) per biome. SoStylized wind lines / fog are a start.

## 7. UI

Positions are on a 16:9 screen, read from crops of the 4K frames.

| Piece | Where and how big | When it shows / hides | Time(s) |
|---|---|---|---|
| **Turn order** | Left edge, top half: a vertical column of ~8 small portrait cards (about 5% of screen width). Our side in colour, the enemy in greyscale; the current actor at the top with a marker | Always on, from the first frame of the fight to the kill | 5:28.5, 5:34 |
| **Boss bar** | Top centre, about 40% of screen width, a very thin red bar in a decorative frame; boss name in small caps under it. **Status icons sit at its ends** (e.g. a burn icon with a count on the left) | Always on | 5:34, 6:06.9 |
| **Ability name banner** | Small dark banner under the boss name | Appears when any skill or enemy attack starts; gone when it ends | 5:38.9, 6:06.4, 6:17.3, 6:57.4 |
| **Player block** | Bottom right, about 25% wide: portrait, stance word, red HP bar with numbers, and an **action-point counter: a number in a blue diamond plus a row of blue pips**. A special-charge gauge ("0/3") sits to its left | Always on | 5:34 |
| **Action menu** | Fanned **diagonally** beside the hero's right hip, 4 rows, each a face-button icon + a word (attack, skills, items, special attack); an aim option to the hero's left. The **AP counter repeats above the menu** at the hero's shoulder | Only on the player's turn; appears once the camera settles behind the hero | 5:30.5–5:30.8 |
| **Skill list** | Replaces the menu at the same spot: two columns of cards. Each card: face button, skill name, **AP cost in a diamond** at the right, and 2–3 lines of text with **element words coloured** and small status icons | Only while choosing | 5:57.7–5:58.5 |
| **Enemy panel** | Right side, middle: the enemy's portrait in a torn-paper frame, its **stance** word, and a short list of its effects with icons (stance bonuses as plus/minus percentages) | Shown on the player's turn; mostly hidden in enemy-turn shots | 5:34, 5:58.5, 6:02.2 |
| **Defend hints** | Bottom left, small: four rows with button icons (special parry, parry, dodge, jump) | Appear at the **enemy turn start** and stay through the combo; hidden on our turn | 5:38.9, 5:51, 6:06.4–6:07.3 |
| **Defend result** | "PERFECT" in large serif letters, placed in the world **between the two fighters at the clash** | Only after a successful input; fades in ~0.5 s | 5:38.9, 6:12.7 |
| **Damage numbers** | In the world at the hit point. Big hits: large gold serif digits with a dark outline, a **"Weakness" tag**, plus a status tag ("BURN") and icon. Small hits and counters: white digits, one large and one small copy. Heals: small green "+" numbers | Stay ~1–2 s; several stack upward | 5:36.3, 5:40.1, 6:00.7, 6:02.2, 6:57.4 |
| **Floating HP bar** | A small red bar with a number over the character just hit | Only for a moment after a hit | 5:40.1, 6:39.4 |
| **AP gain** | A small **blue diamond** pops at the defender after a parry (reads as an action point gained) | After a parry | 5:40.1 |
| **Timed-press prompt** | A gold diamond with the button icon and a shrinking ring, beside the target | Only during skills with timed presses | 6:00.7 |
| **Target marker** | Not seen (only one enemy) | — | — |
| **Victory panel** | Left: title, loot, kills, and stats (damage dealt and taken, time, **perfect parries and dodges**); right: character card with XP | Opens about 2 s after the kill, over the wide shot | 7:00.6 |

**How it moves with the camera:** the edge HUD (turn order, boss bar, player block, hints) is fixed to the screen and **stays on through every shot**, including the big skill. The action menu and skill list are flat on the screen but **placed next to the hero**, and they only exist while the camera is still. Damage numbers, "PERFECT", the floating HP bar, the AP diamond and the timed-press prompt are **anchored in the world** and move with the shot.

**Rules for our game** (widgets from `UISystem.md`):
- U1: `UTurnOrderStripWidget`: try a vertical left-edge column; our side in colour, enemies greyed; current actor at the top. Keep the pre-spawned slots (TransBuffer rule).
- U2: `UCharacterPanelWidget` for the player: compact block bottom right (portrait, HP, EP). For action pips (B-170), show **a number plus a row of pips**, and repeat it next to the menu while choosing.
- U3: a new **boss bar** widget: thin, top centre, status icons at its ends. Normal enemies don't need it.
- U4: a new **ability name banner** under the boss bar for every skill, ours and the enemy's. Cheap, and it names the telegraph (FeelGuide 14).
- U5: the action menu sits next to the acting hero (screen position from the hero), one face button per row (FeelGuide 13). Skill cards show the **pip cost** on the right and element words in element colours (`ElementColors`).
- U6: an enemy info panel on the right, shown on our turn only: portrait, stance/state, effects. Statuses as small icons (FeelGuide 5); no weakness icons.
- U7: `UDefensePromptWidget`: **no big centre prompt**. Show the defend buttons as small bottom-left hints for the whole enemy turn, and the result ("Perfect", "Dodged") in the world at the clash point.
- U8: damage numbers in the world: size by hit strength, **colour plus a tag word for weakness** (FeelGuide 5: colour, not icons), a status tag when a status applies, green for heals.
- U9: a small floating HP bar over a character for a moment after it is hit.
- U10: hide only the menu and the enemy panel during enemy turns and cinematics; turn order, HP, boss bar and hints stay.
- U11: our victory flow has no results screen (FeelGuide 6), so their victory panel doesn't carry over; a perfect-defence count could go in the reward popup.

## 8. Feedback and timing

| What | How it's shown | Time(s) |
|---|---|---|
| Whose turn | Three signals at once: the turn-order column updates, the camera settles behind the actor, the menu fans out beside them | 5:30.5, 5:55, 6:43 |
| Enemy turn | The ability banner and the bottom-left defend hints appear; the camera gives a front shot of the enemy | 6:06.4 |
| Hit weight | No freeze long enough to see at these frame steps; weight comes from the burst, the big number and the flash | 5:35.9 |
| Signature hit | Holds on the impact flash for ~0.5 s before whipping back | 6:02.2–6:03.4 |
| Perfect parry | Star-burst + "PERFECT" + a blue AP diamond; the combo **keeps flowing**, no pause, no camera move | 5:38.9–5:40.1 |
| Counter | After the combo, a counter strike with big numbers, often in a wide shot | 5:40, 5:51, 6:16 |
| Special counter | The only freeze-like beat: a black-and-white radial frame for a fraction of a second | 6:39 |
| Kill | ~2 s of wide, darkened shot before the victory panel | 6:57–7:00.6 |
| Pace | A player action ~3–5 s from confirm to back in the turn view; an enemy combo ~10–15 s; a full round ~25 s | — |

**Rules for our game:**
- F1: keep FeelGuide 4 (normal ~40 ms freeze, big ~100 ms); add a ~0.4 s hold on signature impacts only.
- F2: never pause on a defence (FeelGuide 3); a parry is text + burst + (if we grant one) a pip-gain marker.
- F3: if a parry grants an action pip (B-170d, `GainActions`), show it as a pip popping at the defender **and** filling in the player block.
- F4: turn start = strip highlight + camera move + menu, on the same frame.
- F5: enemy turn start = banner + defend hints + a short front shot, all before the first defendable moment (inside the 0.75 s read time, B-370).
- F6: the kill gets a ~1.5–2 s darkened wide beat, then our reward popups (FeelGuide 6).

**Unclear (VFX, UI, timing):** freezes under ~100 ms can't be seen at these frame steps; a dodge's feedback wasn't caught apart from the parry's; the blue diamond after a parry is read as an AP gain but isn't confirmed; the aim option wasn't used, so free-aim UI isn't shown; with one enemy, target markers never appear.

