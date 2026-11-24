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
