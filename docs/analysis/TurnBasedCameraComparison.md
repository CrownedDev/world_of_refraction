# Turn-based camera comparison: Expedition 33, Persona 5 Royal, Metaphor, FFX

**Purpose:** evidence for three camera asks from Crown: (a) the target-pick view, (b) whether an attack's camera ends only when the attacker is back home and how long it waits, (c) the defend view. Companion to `MaelleCombatPresentationStudy.md` (same method). Our camera: `docs/Architecture/CombatCamera.md`, `docs/Design/CombatCamera.md`.
**Method:** each video paused and stepped about every 1–2 s, then every 0.25–0.5 s around rushes, hits and hand-overs. Times are video time (m:ss). Blend speeds and lens values are by eye.
**Date:** 2026-11-24. Docs only, no code.

| Game | Video | Fight | Fight span |
|---|---|---|---|
| Clair Obscur: Expedition 33 | `pAq90JUTego` (Goblu, 4K) | Gustave, Lune, Maelle vs one big boss | 1:28 → ~11:50 |
| Persona 5 Royal | `-XgbXSpFw8g` (Madarame) | Four party members vs boss, later vs several copies | 0:36 → ~8:00 |
| Metaphor: ReFantazio | `TqA-j_-MbVg` (Homo Avades, PS5) | Four party members vs one huge boss | 3:20 → ~9:30 |
| Final Fantasy X HD | `2N-_D7Fz4YA` (Klikk, PC) | Tidus (later + Rikku) vs one enemy | 0:50 → ~2:50 |

## 1. Expedition 33 — `pAq90JUTego`

| Phase | What the camera does | Time(s) |
|---|---|---|
| Turn start / menu | Over the actor's shoulder, low (hip–chest). Actor lower-left, boss centre. Command wheel steps a little closer; skill list closer again | 1:30, 1:32–1:34, 1:36–1:37.5, 1:42–1:48 |
| **Target pick** | Cuts to a **low shot on the target**: the boss fills the frame, seen slightly from below; the actor is out of frame. Target info box top-left. No high or top-down angle at any point | 1:38, 2:22–2:24 |
| Melee: rush | Behind the attacker as she runs in, low | 1:38.5–1:39, 2:28 |
| Melee: hit + hold | Low, close, three-quarter; big damage number at the hit. Holds about 1–1.3 s | 1:39.5–1:40.8, 2:28.5–2:29 |
| **Return home** | Cut to a **wide shot from behind the party** (next actor in the near-left foreground). The attacker is seen flipping back in that wide. Landing ~1:41.8; the next menu appears at 1:42.0. So the camera starts the next view early, but **the next turn waits until she lands (~0.2–0.5 s)** | 1:41.0–1:42.0; also 2:30–2:31 → enemy turn 2:32 (~1 s) |
| Ranged (Gustave free aim) | Tight over the shoulder, gun arm in the left foreground, boss centre. Camera holds while aiming | 2:25–2:27 |
| **Enemy attack (defend)** | Area slam on all three: **wide, low, from behind and beside the party**, party small in the near-left, boss centre. Steady for ~6 s; numbers float over each hero. (Single-target combos in `LItGIL9XfzE`: low side-on two-shot, see the Maelle study R10) | 2:32–2:38 |
| Hand-over | Next player view ~1 s after the last damage number | 2:38 → 2:39 |

## 2. Persona 5 Royal — `-XgbXSpFw8g`

| Phase | What the camera does | Time(s) |
|---|---|---|
| Turn start / menu | Close, low over-the-shoulder; the actor fills the **right** side, enemy centre-left. Skill list does not move the camera | 0:38–0:42, 0:53–0:55, 3:20 |
| **Target pick** | **Stays low behind the actor.** A red reticle / "WEAK" tag jumps between enemies; the camera only re-aims a little toward the chosen one. With several enemies (copies of the boss) still low, never high | 0:43, 3:45–3:49, 6:20 |
| Melee | **No run-in seen.** Physical skills are done by the summoned Persona while the hero stays home; side-on medium as it strikes | 0:56–0:58 |
| Hit + hold | Front-wide of the enemy, number pops, ~1 s hold | 0:46, 0:59–1:00 |
| Return home | Not needed (attacker never left). Gun "attack all" is side-on, both sides in frame, ~3 s | 1:53–1:56, 6:12–6:13 |
| Spell | Persona summon from the side, then front-wide of the target for the hit | 0:44–0:46, 3:46–3:51 |
| **Enemy attack (defend)** | Skill name banner, then **low over the attacker's shoulder looking at the party** (enemy big in the foreground, party small behind); hit lands, shot holds ~2 s with a party callout | 4:17–4:21 |
| Hand-over | Wide beat (~1 s), then a hard cut to the next actor's shoulder view | 0:47–0:48, 4:25–4:27 |

## 3. Metaphor: ReFantazio — `TqA-j_-MbVg`

| Phase | What the camera does | Time(s) |
|---|---|---|
| Turn start / menu | Behind the actor, low–mid, boss centre. Skill list opens over a red close-up of the actor's archetype | 3:26, 3:31, 3:40, 5:09 |
| **Target pick (enemy)** | Behind the actor, low–mid, the archetype in the left foreground, boss centre. Not high | 3:27–3:28 |
| **Target pick (ally, heal/buff)** | Raised a bit, looking **down at the chosen ally** (about 30°), close. The only "higher" pick view; still not top-down | 5:10–5:13 |
| Melee | The archetype lunges in, close from the side | 3:29 |
| Hit + hold | Wide from behind the party, number over the boss, ~0.8 s | 3:29.5–3:30.3 |
| Return home | No visible walk back: a white slash wipe cuts straight to the next actor | 3:30.5–3:30.8 |
| Spell | Behind the caster, mid height, effect on the boss | 3:36–3:39 |
| **Enemy attack (defend)** | "Enemy turn" banner, close front shot of the enemy, hits on the party from the front, low. After a party-wide hit: **a high, wide overhead of the whole arena for ~4 s** while numbers settle. A single-target swing is a side-on wide with both sides in frame | 3:43–3:50, 5:04–5:08 |
| Hand-over | Wipe or hard cut, ~0.3 s after the hold | 3:30.5, 5:14–5:15 |

## 4. Final Fantasy X HD — `2N-_D7Fz4YA`

| Phase | What the camera does | Time(s) |
|---|---|---|
| Turn start / menu | Low–mid, behind and to the side of the actor (three-quarter), enemy right. The angle varies between turns (one turn uses a far-corner wide) | 0:56, 1:02.5–1:03, 0:59.5 |
| **Target pick** | **No camera move** from the menu shot; only the enemy's name appears. Low, never high | 1:00, 1:03.5 |
| Melee: rush | Same shot: the actor runs across the frame toward the enemy (side-on) | 1:04, 0:56.5 |
| Hit + hold | Side-on two-shot, both in frame; number at the hit; ~0.5 s | 1:04.5, 0:57 |
| **Return home** | **Same continuous shot follows the backflip home.** Lands 1:05.5; enemy acts 1:06 (~0.5 s). Earlier: lands 0:58, cut 0:58.5, menu 0:59.5 | 1:05–1:06, 0:57.5–0:59.5 |
| Ranged (Rikku's grenade) | Wide side-on, low–mid, both sides in frame; actor stays home; blast fills the enemy side | 2:04–2:06, 2:18–2:20 |
| **Enemy attack (defend)** | **Same side-on two-shot as the player's attack**: enemy walks in, hits Tidus, walks back | 1:06–1:07 |
| Hand-over | Next menu ~0.5 s after the enemy is home | 1:07 → 1:07.5–1:08 |

## 5. What this means for our camera

**(a) Target-pick view — keep it low; never top-down.**
None of the four uses a high or top-down view to pick an enemy. They either keep the menu shot unchanged (FFX 1:03.5, P5R 3:45–3:49) or cut to a low shot of the target (E33 1:38, 2:22). The only raised view is Metaphor's ally pick, about 30° down at the ally (5:10–5:13). Rule: pick an enemy from the turn view or a low target shot; only tilt down when the target is an ally.

**(b) End the attack only when the attacker is home — yes, then about 0.5 s.**
Both games where the attacker runs in also wait for the landing before the next turn: FFX ~0.5 s (1:05.5 → 1:06), E33 ~0.2–0.5 s (1:41.8 → 1:42.0) and ~1 s before an enemy turn (2:31 → 2:32). P5R and Metaphor avoid the problem: the attacker never leaves home. Crown's "home + 0.5 s" matches. Note: E33 cuts to the wide hand-over view *while* the hero is still flying back (1:41.0); only the next menu waits. Either works; FFX's one continuous shot is the simplest.

**(c) Defend view — both fighters in frame, side-on or wide; not close behind the defender.**
FFX: the same side-on two-shot as an attack (1:06). E33: wide from behind and beside the party for an area hit (2:32–2:38); low side-on two-shot for single combos (Maelle study). P5R: over the *attacker's* shoulder at the party (4:18–4:21). Metaphor: front, then side-on wide (5:04–5:08). None sits tight behind one defender. Our "low behind the defender" should swing further to the side and pull back so the attacker's whole body and weapon stay in view. For a hit on the whole party, go wide (E33 2:32, Metaphor 3:46).

**Surprises**
- The only high, top-down-style shot found: Metaphor, **after** an enemy's party-wide hit (3:46–3:50). Never for picking.
- E33 hides the hero during target pick; the frame is all enemy (1:38, 2:22).
- FFX changes the idle menu angle between turns (0:59.5 vs 1:03).
- Hold on a hit is short everywhere: 0.5 s (FFX) to ~1.3 s (E33).

## 6. Follow-up: Expedition 33 hand-overs — `GByuD9VPa2I`

Official combat clip (Gustave and Lune vs Évêque, 4:43). Stepped every 2 s, then every 0.1–0.25 s around each hand-over (0:12, 0:17, 0:40, 1:01, 3:31). Added 2026-11-24.

| Moment | What the camera does | Time(s) |
|---|---|---|
| Hit, then hold | Close behind or beside the attacker at the enemy; number pops; held ~1 s | 0:11–0:12, 0:40, 3:31–3:31.5 |
| **Cut to the home wide** | Hard **cut** (no blend) to a low, wide shot from behind the party: both heroes small in the near-left and near-right, the enemy whole in the centre. The attacker is seen walking back home in it | 0:12.1, 0:40.5, 1:01.0, 3:32.0 |
| Attacker lands home | ~0.4–0.5 s after the cut | 0:12.5, 0:41.0 |
| **Sweep to the next hero** | ~0.1 s after the landing, a **fast sweep** (heavy motion blur) from the wide into the next hero's over-the-shoulder view. Takes ~0.3–0.4 s. The menu appears as it settles | 0:12.6 → 0:13.0, 1:01.5 → 1:02.0 |
| Next turn is the enemy's | No sweep: the same wide stays, the enemy's skill banner appears, and the camera slowly pushes in toward the enemy (~2–3 s) | 0:17.5 → 0:20–0:22, 0:41.25 → 0:44, 3:32.5 → 3:35 |
| Enemy → hero | Same pattern: cut from the defend shot to the wide (1:01.0) as the enemy walks back, then the sweep into Lune's view (1:01.5 → 1:02.0) | 1:01–1:02 |
| Next hero's turn view | Hero in the **left third**, back to us, framed knees or waist up, about 50–60% of frame height. The camera sits a little above the hero's waist, about chest height, **over the right shoulder** (hero left, enemy right of centre). The **whole enemy**, head to feet, is in frame, centre to centre-right, as tall as the hero or taller. The menu floats between them | 0:13, 1:02, 3:26 |

**Answers**
1. **Cut, then sweep.** On the hit's end it hard-cuts to a low wide from behind the party (0:12.1). The attacker lands ~0.4 s later (0:12.5). About 0.1 s after that, a fast blurred sweep (~0.3–0.4 s) carries the camera into the next hero's shoulder view (0:12.6 → 0:13.0). Same at 1:01–1:02. Total from hit end to the next menu is about 1 s.
2. **Next hero's view:** hero in the left third, seen from behind at about chest height, camera over the right shoulder. The enemy is fully in frame, centre to centre-right (0:13, 1:02).
3. **Whole party and field:** not only for all-target attacks. The wide from behind the party is the **hand-over shot after every action**, single-target too (Lune's single spell, 0:17.5). It is also the base of every enemy turn (0:41–0:44, 3:32–3:35). Only the menu, aim, rush and hit shots are close.

## 7. Expedition 33 skills by type — `j32WmbafGF8`

"ALL Party Skills Showcase" (13:16), one chapter per hero: Gustave 0:00, Verso 0:48, Maelle 2:42, Lune 4:49, Sciel 6:56, Monoco 8:47. Each chapter was stepped every 2–5 s, then every 0.5–0.75 s around one or two skills of each type. Added 2026-11-24.

**Limit:** the uploader trimmed most clips, so the walk back home is usually cut out. Use §6 for the return and the hand-over: hard cut to a low wide behind the party, then a sweep to the next hero.

| Type | Shot order, with timings | Example(s) |
|---|---|---|
| **Melee, single target** | Low wide behind the hero at home (~0.5 s) → **rush**: camera follows from behind, pushing in (~0.5–1 s) → **cut** to a close three-quarter on the target as the blow lands, with a big number → **hold** ~1 s → (return: §6) | Maelle 2:57.5–3:00; Gustave 0:07–0:08 |
| **Melee, multi-hit / combo** | Rush as above, then **a new camera angle on almost every hit**, about one cut each 0.5–1 s (behind, side, front of the enemy, close on its face). A timed-press ring sits on the target. Last hit gets the biggest number and ~1 s hold | Verso 1:14–1:18.5; Maelle 3:01–3:05; Sciel 7:04.5–7:05.5 |
| **Ranged physical (thrown / bolt)** | **Behind the hero at home, low, held.** The projectile flies away from the camera; the hit and its number are seen **in the same shot**, with no cut. Repeat throws reuse the shot (~1 s each) | Sciel cards 7:06–7:09.5; Gustave 0:30.5–0:33.5 |
| **Spell, single target** | Over the caster's shoulder at home → slow **push-in** while it charges (~1–1.5 s, ring on screen) → **cut** to a medium shot of the target for the hit → hold ~0.5–1 s. Beam spells skip the cut and stay behind the caster | Lune fire 4:55.5–4:58; Lune ice beam 4:58.5–5:00.5; Lune 5:12–5:14 (front of her, then cut to target) |
| **Spell, area / all enemies** | **One wide shot**, low to mid height, behind and beside the caster. The caster is small in the near-left, every target in frame. Held **still** for the whole effect (~3–4 s); numbers over each enemy | Lune 5:07–5:11, 5:15–5:18; Lune chain 5:01–5:04 (slow drift, same shot); Verso 1:19–1:23 |
| **Buff / heal** | **Front shot of the caster at home**, medium-close, ~1–2 s, with a slight orbit. Self-buff: stays on the caster. **Party buff:** then a **cut to a wide of the party** showing the effect on each ally (~1 s). No enemy in frame | Gustave 0:24.5–0:27.5 (party), 0:28–0:30 (self); Lune 5:20 |
| **Big finisher / ultimate** | Close on the hero → **blur whip** → **colour drains to black and white** → rapid cuts every ~0.5–0.75 s between close angles → white flash on impact → huge number held ~1.5 s → colour returns. Charged ones open with a wide behind, then a push-in during the charge, a front close-up, and a side shot of the leap | Monoco 12:39.5–12:52; Gustave 0:43–0:48.5; Sciel leap seen **from high overhead** 7:02.5–7:04 |

**Common thread:** physical skills move the camera *with* the hero (rush, then cuts at the target). Spells keep the camera at the caster's home spot and either cut once to the target (single) or hold one wide (area). Buffs never look at the enemy.

### Mapping to our `ESkillCameraStyle`

| E33 type | Our style | Fit and gaps |
|---|---|---|
| Melee, single target | **Melee Rush** | Good fit. E33's hit shot is a close three-quarter on the target, not a wide side two-shot; tune the Impact offset closer |
| Melee, multi-hit / combo | **Melee Rush**, plus something new | Gap: E33 re-cuts the angle on each hit. **New:** a "re-cut per hit" option on Melee Rush (cycle 2–3 offsets each hit) or a new **Melee Combo** style |
| Ranged physical | **Ranged Release** | Partial. Ours cuts at release to a side two-shot the projectile crosses. E33 **stays behind the thrower** and shows the hit in the same shot. Add a "hold over-shoulder through the hit" option |
| Spell, single target | **Standard** (short charge) or **Heavy Wind-Up** (long charge) | Good fit: push-in during the charge, then one cut to the target. Heavy Wind-Up cuts to a low *hero* angle at the hit; E33 cuts to the *target* |
| Spell, area / all | **Area Sweep** | Good fit, held still. Our description says "higher"; E33 is low to mid height behind the caster. Lower the height |
| Buff / heal | **none fits** | **New style needed: "Support"** — front shot of the caster at home; for a party target, a cut to a wide of the allies; no enemy framing. Today these would wrongly aim at an enemy or the ally from the enemy side |
| Finisher / ultimate | **Cinematic** | Good fit: hand-made CameraSequence. The colour drain and fast cuts belong in the sequence. The high overhead leap is a sequence shot, not a style |

**So:** five of seven types map to existing styles with small tuning. Two need something new: **Support** (buffs/heals) and a **per-hit re-cut** for combos.
