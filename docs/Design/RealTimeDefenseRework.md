# Real-Time Defense Rework — E33-Style Per-Hit Active Defense

> **Status: DESIGN ONLY — not being built now.** Captured for a future dedicated arc.
> Synthesized from two completed surveys (defense-window lifecycle survey + feature-sizing survey).
> All `file:line` anchors reflect the codebase at time of writing — re-verify before picking this up.

---

## 1. Goal / Design Intent (Crown, locked)

E33-style **active defense**: the defender watches and times the oncoming attack in real time, defending
each impact individually rather than tapping a single parry moment.

- **Window opens BEFORE the approach.** The attacker's rush-in is part of the threat and is defendable —
  the window is open for the whole attack, not just the swing.
- **Multi-hit = ONE montage, MULTIPLE hit-frames.** `HitCount` is authored per attack/ability/spell. A
  3-hit attack is a single montage with three impact frames.
- **Each hit is defended INDEPENDENTLY.** The defender can parry hit 1, eat hit 2, dodge hit 3. There is no
  single "defense choice for the action."
- **Each hit resolves at ITS hit-frame.** The defender's input *at that moment* is judged against *that*
  hit, and that hit's damage lands there — resolution is anchored to the visual impact, not a lumped
  end-of-window event.
- **Window may close at the last hit.** Post-attack lingering (e.g. during the attacker's return-to-
  position) is mechanically pointless — ignore it.
- **Speed is the intended payoff.** `SpellSpeed` / `ActionSpeed` raise montage PlayRate (and ActionSpeed
  also raises approach/return movement speed), so hit-frames arrive sooner → tighter reaction windows.
  This **falls out for free** once resolution is hit-frame-notify-driven (the notifies ride the montage
  clock), with no extra wiring. It is **not** reachable by hooking today's code.

---

## 2. Current State (what the code does today)

| Concern | Today | Anchor |
| --- | --- | --- |
| Window open | Opens at **montage-start**, *after* the approach completes (synchronous in `Execute<Type>Async`) | `ActionExecutor.cpp:797` (spell), `:956` (ability), `:1088` (attack) |
| Window close | **Fixed 0.3s timer**, decoupled from the animation. `// TODO: get from spell data` | `DefenseSystem.h:310` (`DefaultWindowDuration = 0.3f`); `ActionExecutor.cpp:811`; close via `OnWindowTimerExpired` → `CloseDefenseWindow` (`DefenseSystem.cpp:445` / `:124`) |
| Multi-hit application | **LUMPED**: one defense result split across N hits in a **synchronous, single-frame loop** — no per-impact timing | `ApplyDamageAfterDefense` loop `ActionExecutor.cpp:1371`; `DamagePerHit = FinalDamage / HitCount` `:1359` |
| Defense input model | **One committed choice per action.** A second input is rejected (`bInputReceived` guard) | `SubmitDefenseInput` `DefenseSystem.cpp:189`, reject at `:211–216`; single `FDefenseState.DefenseChosen` |
| Resolution entry | One `FDefenseResult` per action, applied once | `OnDefenseWindowClosed` `ActionExecutor.cpp:1261` |
| Hit-frame notify | **Does not exist.** Only `SpellCastStart` / `SpellRelease` (VFX-only) | `OnSpellAnimNotify` `ActionExecutor.cpp:3822` |
| Multi-hit timing source | **None** — it's an instant code loop, not timer/notify driven | `ActionExecutor.cpp:1371` |
| AI defense | Schedules **ONE** decision per window | `ScheduleDefenseDecision` `DefenseSystem.cpp:119` |
| HitCount authoring | Per-asset field on all three data assets | `UWeaponAttackData::HitCount` (soft-warn >2, `WeaponAttackData.cpp:73`), `UAbilityData::HitCount`, `USpellData::HitCount` |

**Key mitigant:** `ApplyHit` (`ActionExecutor.cpp:1970`) is already a clean, self-contained **per-hit
applier** taking a single `FActionHitInput`. The damage-application primitive is already per-hit shaped —
the rework rewires *when / under which defense result* it is called, not *how* a hit applies.

---

## 3. The Gap (plainly stated)

**Current = a fixed-length, single-resolution tap-timing minigame.** The window opens after the approach,
runs a magic 0.3s unrelated to what the defender sees, takes one input, and applies one defense result to
all hits in an instant loop.

**Intended = per-hit active defense spanning the whole attack.** Three distinct mismatches:

- **Open — too late:** opens at montage-start, so the approach lunge is not defendable.
- **Close — decoupled:** fixed 0.3s timer, unrelated to montage length or hit-frames. If the montage is
  longer than 0.3s the window closes mid-attack; if shorter it outlasts the visible attack.
- **Resolution — lumped:** one input governs every hit; the defender cannot react to hits individually,
  and damage does not land at the visual impact moments.

Multi-hit being lumped (no per-impact events, no hit-frame notifies, one-shot input model) is what makes
this a structural rework rather than a hook-up.

---

## 4. Staged Implementation Plan

Each stage marked **[Code]** or **[Anim]** (animation-authoring, Crown's work) or both.

| Stage | Work | Type | Size |
| --- | --- | --- | --- |
| **0. Hit-frame notifies** | Author impact notifies on every multi-hit montage (e.g. `AttackImpact`, **distinct** from `SpellRelease`). Bind the notify parallel to `OnSpellAnimNotify` (`ActionExecutor.cpp:3822`). | **[Anim]** + small **[Code]** | Small |
| **1. Per-hit resolution loop** | Replace the synchronous loop in `ApplyDamageAfterDefense` (`ActionExecutor.cpp:1371`) with a **notify-driven sequence** — one `ApplyHit` per impact notify, each gated by *that hit's own* defense result. `ApplyHit` (`:1970`) stays as-is (already per-hit shaped). | **[Code]** | Large |
| **2. Per-hit defense state** | Rework `FDefenseState` / `SubmitDefenseInput` (`DefenseSystem.cpp:189`) to **re-arm input per hit-frame** (clear `bInputReceived` each hit, or queue inputs). `CalculateDefenseResult` evaluated **per hit** instead of once per action. | **[Code]** | Large |
| **3. Window lifecycle** | Open the window **before** approach (around `ExecuteActionAsync` `:565` → before `StartApproach` `:570`): split "open empty window" from "arm with per-hit data" — note this requires **hoisting/restructuring the damage pre-computation** currently done inside `Execute<Type>Async`. Close at the **last hit-frame**; drop the fixed 0.3s timer but **keep a montage-end / max-duration failsafe**. | **[Code]** | Medium |
| **4. Per-hit AI** | Replace the single `ScheduleDefenseDecision` (`DefenseSystem.cpp:119`) with **N scheduled decisions, one per hit-frame**, each judged independently. | **[Code]** | Medium |
| **5. Per-hit UI** | Per-hit defense prompt / feedback (the prompt widget is currently TODO-stubbed) — surface each incoming hit and its result. | **[Code]** + UI | Medium |

**Free fallout (after Stages 1+3):** once hits fire on montage-riding notifies, `SpellSpeed`/`ActionSpeed`
(montage PlayRate) and ActionSpeed (approach/return movement speed) make hit-frames arrive sooner →
tighter windows automatically, no extra code. The speed payoff is a *consequence* of the rework, not a
separate task.

---

## 5. Risks / Notes

- **Failsafe timer is mandatory.** Today the 0.3s timer *is* the closer. Once close moves to hit-frame /
  montage-end, a missing montage or a missing/late notify would otherwise hang the window open forever.
  Retain a max-duration backstop (mirrors the existing `AsyncTimeoutHandle` failsafe pattern).
- **AI duration must follow PlayRate.** `ScheduleDefenseDecision` is handed `WindowDuration`. With montage-
  driven timing, the value (or per-hit schedule) must use the **PlayRate-scaled montage length**, or the AI
  reacts on a stale 0.3s clock.
- **AOE is a separate open path** (`ActionExecutor.cpp:2689`, `AoeWindowDuration`). It needs the same per-
  hit treatment if AOE is to couple to the visual; otherwise call out that AOE stays single-resolution.
- **Speed-stone dependency.** The combat teeth for `SpellSpeed`/`ActionSpeed` only materialize **after**
  this rework. See dependency note below.

### Dependency: Speed stones built first, wired here

`SpellSpeed` / `ActionSpeed` augment stones are slated to be built **before** this rework. At that point
they only speed up the *visual* (montage PlayRate, approach/return movement) with **no combat effect** —
the defense window is fixed and decoupled, so faster animation does not make an attack harder to defend.

**This rework is the second half of those stones.** Once resolution is hit-frame-notify-driven, the speed
stats gain real defensive teeth (sooner hit-frames = tighter reaction) for free. Treat "wire speed to
defense" as a deliverable of *this* arc, not the speed-stone arc.

---

## 6. Sizing

**LARGE.** Multi-stage, and structurally a rework rather than a hook-up:

- Multi-hit is lumped (one resolution, instant loop) — Stage 1 restructures the core resolution.
- The defense state machine is one-shot — Stage 2 makes it per-hit re-armable.
- The AI scheduler is one-decision — Stage 4 makes it per-hit.
- **Animation-authoring is on the critical path** — Stage 0 (impact notifies) is Crown's work and blocks
  Stages 1/3.
- Heavy **PIE verification of real-time feel** is required (window timing, per-hit reaction tightness,
  speed-scaled difficulty) — not unit-testable.

**Recommendation: a fresh dedicated arc.** Do not fold into an unrelated session. The only piece that
de-risks the size is `ApplyHit` already being per-hit shaped; everything around it (resolution sequencing,
input model, window lifecycle, AI, UI, notify authoring) is net-new.

---

## Changelog

- *(creation)* — Authored from the lifecycle + sizing surveys. Design-only; feature not yet built. Branch: `feature/weapon-stones`.
