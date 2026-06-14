# Real-Time Defense Rework — Turn-Start, All-Defenders-Watching, Per-Hit Active Defense

> **Status: DESIGN SPEC — re-anchored post-movement-unhook.** Supersedes the prior version (which
> was authored pre-movement-unhook against `StartApproach` / `CombatMovementComponent`, both now
> deleted, and predated the all-defenders-watching + AOE-per-hit + condition-hook refinements).
> Branch: `feature/realtime-defense`.
> All `file:line` anchors below are current **as of the June 2026 defense re-anchor survey** — re-verify
> before picking up each stage, as the surrounding code will shift.

---

## 1. Status

This is the **complete reactive-defense spec** and staged build plan. It is design/spec authoring — no
source has been changed. It re-anchors the stale doc against the current architecture (warp-based
positioning replacing the old approach-movement layer) and captures Crown's **full** locked intent.

The three pillars locked this design session:

1. **All potential defenders watch from turn-start** (not per-target-at-execution).
2. **Per-hit active defense** anchored on the impact, with a lead-in timing window and a PERFECT band.
3. **Condition-hook platform** — every defense outcome fires a condition that effects/items subscribe
   to. The rework builds the *platform* (timing + base outcomes + hooks); the *payoffs* (reflect,
   counters, buffs) are content built later.

After this rewrite Crown reviews, then we start **Stage 0** (hit-frame notifies).

---

## 2. Locked Design Intent (Crown)

E33-style **active defense**: the defender watches and times the oncoming attack in real time,
defending each impact individually rather than committing one defense for the whole action.

- **Window opens at TURN-START.** The moment the turn flips to the attacker, **all potential
  defenders enter a watching state** — *before* the attacker commits an action or a target. A
  defender cannot assume they are the target; the attack could be single-target or AOE. They watch
  and time the threat from the turn flip onward.
- **ONE window per defender, spanning the whole attack.** turn-start → warp-approach → all
  hit-frames → closes at the last hit. The **approach (warp-in) is part of the threat and is
  defendable** — it is inside the window, not before it.
- **Multi-hit = ONE montage, MULTIPLE hit-frames.** `HitCount` is authored per skill
  (attack/ability/spell). A 3-hit attack is a single montage with three impact frames.
- **Each hit is defended INDEPENDENTLY.** The defender can **parry hit 1, eat hit 2, dodge hit 3**.
  There is no single "defense choice for the action."
- **Each hit RESOLVES at its hit-frame.** The defender's input *at that moment* is judged against
  *that* hit, and *that hit's* damage lands there — resolution is anchored to the visual impact, not
  a lumped end-of-window event.
- **AOE = per-defender, per-hit, FULL defense options (RESOLVED).** Every AOE-hit defender runs the
  full per-hit defense flow — **dodgeable, parryable, and blockable**, same as single-target. This
  supersedes the old "AOE can only be blocked" limitation. *(Crown resolved the prior design
  question: AOE is no longer block-only.)*
- **Window closes at the last hit.** Post-attack lingering (during the attacker's warp-return to
  position) is mechanically pointless — the window ends when the last hit resolves.
- **Speed tightens windows for free.** PlayRate (montage speed) makes hit-frames arrive sooner →
  tighter reaction windows, with no extra wiring once resolution rides the montage clock. *(A
  speed-stone dependency exists — `SpellSpeed`/`ActionSpeed` gain their defensive teeth from this
  rework — but Crown has deprioritized it; it is mentioned, not gated on.)*

---

## 3. Timing Model

Defense is a **timing** mechanic anchored on the **IMPACT**, with two impact sources for the same
mechanic:

- **Melee impact** = the **Hit-family Combat Notify** firing on the montage (the impact frame).
- **Ranged impact** = the **projectile's ARRIVAL** at the target.

Same defend mechanic; the only difference is what marks the impact moment.

### Lead-in window

You defend within a **LEAD-IN WINDOW that precedes the impact** — you commit the input *before* the
hit lands, and timing quality is judged against the impact moment. Each defense type has its **own
lead-in duration** (tunable in PIE — placeholders only):

| Defense | Lead-in window | Feel |
| --- | --- | --- |
| **Parry** | Tightest | High-skill, high-reward |
| **Dodge** | Medium | Balanced |
| **Block** | Most forgiving | Reliable, low-skill |

### PERFECT band

**PERFECT** = the input lands **exactly on the impact**, within a small **perfect-threshold** (tuned
per defense type). Perfect is a distinct outcome band, not just "successful" — it is what the
condition-hook platform keys its strongest payoffs off (see §5).

### Window shape (per hit)

```
[ too early ] [ valid lead-in window ] [ PERFECT (on impact) ] [ missed ]
   no-op         base success           success + perfect       hit lands, defense fails
```

- **Too early:** input before the lead-in window opens → no defense registered (don't punish-spam,
  but don't reward random mashing — exact early-input handling tuned in PIE).
- **Valid lead-in window:** base success for that defense type.
- **PERFECT:** input within the perfect-threshold of the impact → success **and** fires the
  `OnPerfect*` condition.
- **Missed:** no valid input by the impact → the hit lands, the failure condition (`OnHit` /
  `OnTakeDamage`) fires.

### Per-hit independence

Multi-hit = multiple impacts = **multiple independent lead-in windows**, each with its **own perfect
chance**. Hit 1's parry has no bearing on hit 2's window — a 3-hit attack is three separate timing
problems back-to-back. (This is why the input model must re-arm per hit-frame — see §8 Stage 2.)

---

## 4. Base Defense Outcomes

The behavior that **always** happens on a successful defend, before any condition-subscriber payoff.
All values tunable in PIE.

| Defense | Base outcome |
| --- | --- |
| **Block** | **Reduce damage** — reliable partial reducer (takes chip damage). |
| **Dodge** | **Avoid full damage** — take **zero** for that hit. |
| **Parry** | **Negate damage + reduce status** — zero damage **and** cut status buildup for that hit. |

Notes:
- **Dodge = full avoid** (zero damage), but no status mitigation beyond avoiding the hit's own
  application.
- **Parry = full negate + status reduction** (RESOLVED: parry fully negates; balance comes from its
  **tightest window / highest miss-risk**, not from chip damage).
- **Block = reliable partial** — the safe option; always reduces, never zero.
- **PERFECT** does not change the base outcome by itself — it fires the `OnPerfect*` condition. Any
  bonus beyond the base (e.g. perfect-block reflect chunk, perfect-dodge speed buff) is a
  **condition-subscriber effect** (§5), not a hardcoded defense behavior.

---

## 5. Condition-Hook Architecture (the key design decision)

**Every defense outcome fires a CONDITION/EVENT that effects, items, and abilities subscribe to.** The
defense system resolves the **base behavior** and **fires the appropriate condition** — it does **not**
hardcode payoffs.

### Conditions fired

| Outcome | Condition(s) fired |
| --- | --- |
| Block (valid) | `OnBlock` (+ general `OnDefend`) |
| Block (perfect) | `OnPerfectBlock`, `OnBlock`, `OnDefend` |
| Dodge (valid) | `OnDodge` (+ `OnDefend`) |
| Dodge (perfect) | `OnPerfectDodge`, `OnDodge`, `OnDefend` |
| Parry (valid) | `OnParry` (+ `OnDefend`) |
| Parry (perfect) | `OnPerfectParry`, `OnParry`, `OnDefend` |
| Defense fails / none | `OnHit` / `OnTakeDamage` |

(A general `OnDefend` fires for **any** successful defense, so build effects can hook "on any
successful defense" without subscribing to all three types.)

### Division of responsibility

- **Defense system (this rework):** resolve base outcome (§4) + **fire the condition**. Nothing more.
- **Payoffs are CONTENT:** effects/items/abilities **subscribe** to these conditions. Example: *"on
  perfect parry, reflect projectile damage back to the attacker"* is **one such effect** — content,
  not a defense-system behavior.

### Why this matters — defense as a platform

Perfect-timing **and every defense outcome** become a **platform**. Build variety comes from **which
on-defense effects a character has equipped** — defense behavior beyond the base is a **loadout
choice**. This fits the "build a Lord" pitch: two players with the same base parry can play
completely differently depending on what their `OnPerfectParry` / `OnDodge` / `OnDefend` effects do.

### Scope boundary (critical)

- **IN scope (the rework builds the platform):** per-hit timing, the PERFECT band, base outcomes
  (§4), and the **condition-firing hooks** (`OnBlock`/`OnPerfectBlock`/`OnDodge`/`OnPerfectDodge`/
  `OnParry`/`OnPerfectParry`/`OnDefend`/`OnHit`).
- **OUT of scope (built later as subscribers):** the **payoffs** — reflect, counters, buffs, etc.
  These are built when the **effects/items system** is worked on. The rework **fires the events**; it
  does **not** design every payoff.

---

## 6. Current State (post-movement-unhook) — the 10 anchors

The movement unhook (commit `9d064648`) deleted `CombatMovementComponent` / `StartApproach` and
replaced melee approach with warp positioning (`BeginSkillExecution`). It left the defense system
architecture **unchanged** — every anchor below is still in the lumped / fixed-timer / one-shot state.

| # | Concern | Today | Anchor (file:line) |
| --- | --- | --- | --- |
| 1 | **Window open** | Opens **inside** each `Execute<Type>Async`, AFTER `BeginSkillExecution` warps the caster in and animation is queued — not at turn-start. | `ActionExecutor.cpp:1105–1120` (spell), `:1272–1282` (ability), `:1409–1419` (attack) → all converge at `OpenDefenseWindowsForTargets` `:1505–1578`. Warp set in `BeginSkillExecution` `:4158–4222` (target at `:4216`). |
| 2 | **Window close** | **Fixed 0.3s timer** (0.5s AOE), decoupled from the montage. `// TODO: get from spell data`. | `DefenseSystem.h:308–314` (`DefaultWindowDuration=0.3f`, `AoeWindowDuration=0.5f`); timer `DefenseSystem.cpp:73–86`; expiry `:445–456` → `CloseDefenseWindow` `:124–186`. Durations passed at `ActionExecutor.cpp:1119`, `:3093` (AOE), `:3336` (projectile). |
| 3 | **Multi-hit** | **LUMPED** — one `FDefenseResult` per defender, split across `HitCount` in a synchronous loop. **Mitigant:** `ApplyHit` is per-hit-shaped and `ResolvedDamageSplit` is already per-hit. | `ApplyDamageAfterDefense` loop `ActionExecutor.cpp:1702–1734`; `ApplyHit` `:2342`; split resolved at `FinalizeDamageInputs` `:1101` → `ResolvedDamageSplit`. |
| 4 | **Defense state** | **One-shot** — `bInputReceived` rejects a 2nd input; never cleared. | `FDefenseState` `DefenseSystem.h:57–98`; guard `DefenseSystem.cpp:211–216`; record `:229–231`; `SubmitDefenseInput` / `CalculateDefenseResult` in `DefenseSystem.cpp`. |
| 5 | **Hit-frame notify** | **EXISTS but STUB.** `ECombatNotifyFamily::Hit` fires on the montage but the handler is log-only and returns (`"…stub — damage wiring is SC4"`). This is the candidate melee impact trigger. | enum `CombatNotify.h:14–20`; handler `ActionExecutor.cpp:4803–4806`. |
| 6 | **Projectile impact** | Opens a defense window on impact, then applies damage via the same lumped path. **Attacker is NULL** — the projectile doesn't track its source. | `OnProjectileImpact` `ActionExecutor.cpp:3322–3351`; open `:3338` (`OpenDefenseWindow(nullptr, …)`); fallback apply `:3349`. |
| 7 | **AOE path** | **Per-target single-resolution, block-only.** One window per target at `AoeWindowDuration`; comment: "AOE can only be blocked (no dodge, no parry)". | dispatch `ActionExecutor.cpp:2904–2910`; `SpawnAOEEffect` open `:3090–3100`. |
| 8 | **AI defense** | **One decision per window** — one reaction-delay timer → one `ChooseDefenseType` → one `SubmitDefenseInput`, then locked. | trigger `DefenseSystem.cpp:111–121`; `ScheduleDefenseDecision` `AIDecisionManager.cpp:306–409`. |
| 9 | **Target resolution timing** | Targets are **committed in the FAction before windows open** — windows open for the already-locked valid-target subset at Execute time, NOT for all defenders at turn-start. | filter `ExecuteSpellAsync:1048` (`FilterValidTargets(Action.Targets)`); open `:1105–1120`. |
| 10 | **HitCount** | **Authored per skill** (UPROPERTY), inherited by attack/ability/spell via `CastableSkillDataBase`; per-hit distribution via DamageSplit (even split if empty). **Correct — no gap.** | `SkillDataBase.h:67` (`int32 HitCount=1`), DamageSplit `:69–71`, `ResolveDamageSplit` `:43`; `WeaponAttackData.h:28/70`. |

**DefenseSystem class:** `Public/Combat/Defense/DefenseSystem.{h}` / `Private/Combat/Defense/DefenseSystem.cpp`;
enums `Public/Combat/Defense/EDefenseType.h`, `EDefenseDirection.h`.

---

## 7. The Gap

Three structural mismatches, plus Crown's refinements:

- **Open too late + not all-defenders-watching.** The window opens inside `Execute<Type>Async`
  (post-warp, after target commit), per the already-locked target subset (anchor 1, 9). The locked
  design opens at **turn-start for all potential defenders, before the attacker commits a target** —
  the approach/warp-in must be inside the window.
- **Close decoupled.** Fixed 0.3s/0.5s timer (anchor 2), unrelated to montage length or hit-frames.
  It can close mid-attack or outlast the visible hits. Must move to **last-hit / montage-coupled**.
- **Resolution lumped.** One input governs all hits (anchors 3, 4). **The mitigant:** per-hit
  *damage* already exists (`ApplyHit` per-hit-shaped + `ResolvedDamageSplit` per-hit). Only per-hit
  *defense input* (and the timing/PERFECT model in §3, the base outcomes in §4, and the condition
  hooks in §5) is missing. This is what keeps the work a rework, not a rebuild.
- **No condition hooks.** The system applies a flat reduction and never fires `OnBlock`/`OnParry`/
  etc. The platform (§5) does not exist yet.
- **AOE single-resolution, block-only → per-defender per-hit, full options** (anchor 7). Every
  AOE-hit defender must run the full per-hit flow with dodge/parry/block.
- **Projectile NULL attacker → source-threading needed** (anchor 6). See projectile reflect below.

### Projectile parry & reflect

- **Parry on a projectile (BASE outcome):** the projectile **DISAPPEARS** — negated, consistent with
  parry = negate (§4). This is defense-system behavior, in scope.
- **Reflect (CONTENT — an `OnPerfectParry` condition effect):** on **perfect** parry, the projectile
  is sent **BACK to the attacker**. This is the **first concrete example** of an `OnPerfectParry`
  subscriber (§5) — a payoff, **out of scope** for the rework itself.
- **This is WHY projectiles need source-threading.** To reflect back you must know **who fired it**,
  and `OnProjectileImpact` currently opens with `attacker = nullptr` (`:3338`). The rework threads the
  source (in scope, Stage 6); the reflect *effect* that consumes it is built later with effects/items.

---

## 8. Staged Plan (re-staged for current architecture)

Each stage marked **[Code]** / **[Anim]** (animation-authoring, Crown's work) / both. Ordered by
dependency. **Scope reminder:** base outcomes + per-hit timing + condition-FIRING hooks are **in**;
condition payoffs (reflect/counters/buffs) are **out** — built later as subscribers (§5).

| Stage | Work | Type | Size |
| --- | --- | --- | --- |
| **0. Hit-frame notifies** | Wire the existing **Hit-family Combat Notify stub** (`ActionExecutor.cpp:4803`) to be the melee impact trigger — replace the log-only return with a real per-hit entry point. Crown **authors Hit notifies on every multi-hit montage**. **BLOCKS Stages 1, 2, 3, 4.** | **[Anim]** + small **[Code]** | Small |
| **1. Per-hit resolution + base outcomes + condition firing** | Drive `ApplyHit` (`:2342`) off **each Hit notify** (and **projectile impact** `:3322` for ranged), each gated by *that hit's* defense result. Replace the lumped loop (`ApplyDamageAfterDefense` `:1702`). Implement the **base outcomes** (§4: block reduce / dodge zero / parry negate+status). **Fire the conditions** (§5: `OnBlock`/`OnDodge`/`OnParry` + `OnPerfect*` + `OnDefend` + `OnHit`) at each resolution — the hooks fire even before any subscriber exists. | **[Code]** | Large |
| **2. Per-hit defense state + timing/PERFECT** | Rework `FDefenseState` / `SubmitDefenseInput` to **re-arm input per hit-frame** (clear `bInputReceived` each hit; `DefenseSystem.cpp:211–216`/`:229–231`). Implement the **lead-in window per defense type** and the **PERFECT band** (§3); `CalculateDefenseResult` evaluated **per hit**, producing the outcome band that selects which condition fires. | **[Code]** | Large |
| **3. Window lifecycle (RISKIEST)** | Open the window **at TURN-START for ALL potential defenders** — in the turn-start path, NOT in `Execute<Type>Async`; **before target commit** (the structural inversion of anchor 1/9). Close at the **last hit-frame**. Drop the fixed timer but **KEEP a max-duration failsafe**. Split "open empty watching window" from "arm with per-hit data once target/damage is known." | **[Code]** | Medium–Large |
| **4. Per-hit AI** | Replace the single `ScheduleDefenseDecision` (`AIDecisionManager.cpp:306`) with **N scheduled decisions per defender, one per hit-frame**, each judged independently against that hit's lead-in window. Reaction delays scaled to the PlayRate-adjusted hit-frame times. | **[Code]** | Medium |
| **5. Per-hit UI** | Per-hit defense prompt / feedback — surface each incoming hit, its lead-in window, and the outcome band (including PERFECT) (prompt widget currently TODO-stubbed). | **[Code]** + UI | Medium |
| **6. AOE per-hit + projectile source-threading** | AOE: every AOE-hit defender runs the full per-hit flow with **dodge/parry/block** (`SpawnAOEEffect` `:3090`; remove the block-only limitation). Projectile: thread the **attacker source** through to `OnProjectileImpact` (fix the NULL attacker `:3338`) so the base parry-negate works and `OnPerfectParry` subscribers (e.g. reflect) have the source to act on. | **[Code]** | Medium |

**OUT OF SCOPE (separate future effort — effects/items system):** the condition **payoffs** —
projectile reflect, counters, on-defense buffs, etc. The rework fires `OnBlock`/`OnParry`/`OnPerfect*`
/`OnDefend`/`OnHit`; subscribers that *do something* with them are content built when the effects/items
system is worked on.

**Free fallout (after Stages 1+3):** once hits fire on montage-riding notifies, PlayRate
(`SpellSpeed`/`ActionSpeed`) makes hit-frames arrive sooner → tighter lead-in windows automatically,
no extra code. The speed payoff is a *consequence* of the rework.

---

## 9. Risks / Notes

- **Failsafe timer is mandatory.** Today the 0.3s timer *is* the closer. Once close moves to
  last-hit / montage-end, a missing montage or a missing/late Hit notify would hang the window open
  forever. Retain a **max-duration backstop** (mirror the existing `AsyncTimeoutHandle` failsafe
  pattern in ActionExecutor).
- **AI duration must follow PlayRate.** `ScheduleDefenseDecision` is handed `WindowDuration`. With
  montage-driven timing, the per-hit schedule must use **PlayRate-scaled hit-frame times**, or the AI
  reacts on a stale 0.3s clock.
- **Projectile NULL-attacker threading.** `OnProjectileImpact` opens with `attacker = nullptr`
  (`:3338`). Base parry-negate works without it, but `OnPerfectParry` reflect needs the source.
  Scoped into Stage 6.
- **Turn-start open is a structural inversion (Stage 3 — the biggest lift).** Windows currently open
  at execution, per committed target, inside `Execute<Type>Async`. Opening at turn-start for ALL
  potential defenders — before the attacker commits action/target — moves the open out of the
  execution path entirely and decouples "watching" from "target/damage known." Sequence it after
  per-hit resolution (1) and state (2) are proven.
- **Condition hooks fire into the void at first.** Stage 1 fires `OnBlock`/`OnParry`/etc. with **no
  subscribers**. That is intentional — the platform must exist before payoffs. Verify the events fire
  (logging) even though nothing consumes them yet.

---

## 10. Key Mitigant — why this is a rework, not a rebuild

`ApplyHit` (`ActionExecutor.cpp:2342`) is already a clean, self-contained **per-hit applier**, and
`ResolvedDamageSplit` (resolved at `FinalizeDamageInputs` `:1101`) already holds **per-hit damage
values**. Per-hit **DAMAGE** exists today; the loop just consumes it all in one synchronous pass.
The rework adds per-hit **DEFENSE** — re-arming input, resolving an outcome band at each hit-frame,
applying the base outcome, and firing the condition — and rewires *when / under which defense result*
`ApplyHit` is called. The damage primitive itself is untouched. That existing per-hit shape is the
single biggest de-risker.

---

## 11. Sizing

**LARGE — a fresh dedicated arc.** Do not fold into an unrelated session.

- Multi-hit resolution is lumped — Stage 1 restructures the core resolution + adds base outcomes +
  condition firing.
- The defense state machine is one-shot — Stage 2 makes it per-hit re-armable and adds the
  timing/PERFECT model.
- The window open is a turn-start/all-defenders structural inversion — Stage 3, the riskiest piece.
- The AI scheduler is one-decision — Stage 4 makes it per-hit.
- **Animation-authoring is on the critical path** — Stage 0 (Hit notifies on multi-hit montages) is
  Crown's work and **blocks** Stages 1–4.
- Heavy **PIE verification of real-time feel** (lead-in window tuning per defense type, perfect
  threshold, per-hit reaction tightness, speed-scaled difficulty, all-defenders-watching) — not
  unit-testable.

The only piece that de-risks the size is `ApplyHit` already being per-hit shaped; everything around
it (resolution sequencing, timing/PERFECT model, base outcomes, condition hooks, input re-arm,
turn-start window lifecycle, per-hit AI, UI, AOE per-hit, projectile source-threading, notify
authoring) is net-new.

---

## 12. Resolved Design Questions

- **AOE dodge/parry — RESOLVED: YES.** AOE is dodgeable, parryable, and blockable, per-defender
  per-hit. No longer block-only.
- **Projectile reflect — RESOLVED: an `OnPerfectParry` condition effect (content).** Base parry on a
  projectile negates it (projectile disappears); reflect-back is a perfect-parry subscriber, out of
  scope for the rework but the reason projectiles need source-threading.
- **Parry full-negate — RESOLVED: YES.** Parry fully negates damage + reduces status; balance comes
  from its **tightest lead-in window / highest miss-risk**, not from chip damage.
- **Perfect-timing payoffs — RESOLVED: a condition platform, not hardcoded.** The defense system fires
  `OnPerfect*` / `OnDefend` / etc.; payoffs are equipped effects/items. Defense behavior beyond the
  base is a loadout choice.

---

## Changelog

- **2026-06-14** — Added the **timing model** (impact-anchored lead-in windows per defense type +
  PERFECT band, §3), **base defense outcomes** (block reduce / dodge zero / parry negate+status, §4),
  and the **condition-hook architecture** (§5: defense fires `OnBlock`/`OnParry`/`OnPerfect*`/
  `OnDefend`/`OnHit`; payoffs are content subscribers — the "build a Lord" platform). Resolved four
  design questions (§12): AOE dodge/parry = yes; projectile reflect = `OnPerfectParry` effect; parry
  full-negate = yes; perfect payoffs = condition platform. Expanded the projectile section with parry
  negate (base) vs reflect (content) and the source-threading rationale (§7). Folded base-outcome +
  condition-firing scope into the staged plan and marked payoffs out of scope (§8). Renumbered
  sections.
- **2026-06-14** — Full rewrite, re-anchored post-movement-unhook (branch `feature/realtime-defense`).
  Supersedes the stale pre-movement-unhook version. Added Crown's two locked refinements
  (all-defenders-watching at turn-start; AOE per-defender-per-hit), corrected all 10 anchors to
  current `file:line`, re-staged the plan for the warp architecture.
- *(creation)* — Authored from the lifecycle + sizing surveys. Design-only. Branch:
  `feature/weapon-stones`.
