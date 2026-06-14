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

1. **Windows open ON TARGET CONFIRMATION** — for exactly the targeted defenders (single → one, AOE →
   all targeted). *(Revised this session — supersedes the earlier "all defenders watch from turn-start"
   pillar, which assumed independent defenders; single-player has one player controlling the whole
   team. See §2 / §3a Control Model.)*
2. **Per-hit active defense** anchored on the impact, with a lead-in timing window and a PERFECT band.
3. **Condition-hook platform** — every defense outcome fires a condition that effects/items subscribe
   to. The rework builds the *platform* (timing + base outcomes + hooks); the *payoffs* (reflect,
   counters, buffs) are content built later.

Stage 0 (hit-frame notifies) is **DONE** (commit `4986d450`). After this revision Crown reviews, then
we build **Stage 1** (window persist + close-at-last-hit + max-duration failsafe).

---

## 2. Locked Design Intent (Crown)

E33-style **active defense**: the defender watches and times the oncoming attack in real time,
defending each impact individually rather than committing one defense for the whole action.

- **Window opens ON TARGET CONFIRMATION.** The moment the attacker has **committed an action and
  confirmed its targets**, windows open for **exactly the targeted characters** — single-target → a
  window for that one defender; AOE/multi-target → windows for all targeted defenders. You open
  knowing **who** you are defending. *(SUPERSEDES the earlier "turn-start / all-defenders-watching"
  decision — see §3a Control Model for why: in single-player the player controls the whole team, so
  "watch for all three before knowing who's targeted" is not a coherent state for one player to be
  in. Target-confirmation open is ≈ the current execution-time open point, after target commit.)*
- **ONE window per targeted defender, spanning the whole attack.** target-confirmation → warp-approach
  → all hit-frames → closes at the last hit. The **approach (warp-in) is part of the threat and is
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
- **Window closes when the attack stops hitting.** Post-attack lingering (during the attacker's
  warp-return to position) is mechanically pointless — the window ends when the **last** impact lands.
  *(Mechanically this is the **count-based** close — `HitsLanded == ExpectedImpacts`, §8a — which is
  source-agnostic, not a melee "last montage hit-frame.")*
- **Speed tightens windows for free.** PlayRate (montage speed) makes hit-frames arrive sooner →
  tighter reaction windows, with no extra wiring once resolution rides the montage clock. *(A
  speed-stone dependency exists — `SpellSpeed`/`ActionSpeed` gain their defensive teeth from this
  rework — but Crown has deprioritized it; it is mentioned, not gated on.)*

---

## 3a. Control Model (who defends what)

Who supplies the per-hit defense input depends on **who controls the defender** — the system already
distinguishes this per defender (the open path logs *"…is not AI-controlled, skipping AI defense"* vs
*"Scheduling AI defense for …"*, `DefenseSystem.cpp:105–120`).

- **AI-controlled characters defend INDEPENDENTLY.** Each enemy runs its **own** per-hit AI decisions
  (the AI path, Stage 4) — no shared input, each defender judged on its own hit-frames.
- **Single-player (current — no multiplayer yet): the PLAYER controls ALL their team members.** Every
  **player-team** defender routes to **player input**. When an attack targets multiple player
  characters, they are *all* the player's to defend — there is no second human.
- **Multiplayer (future): each player defends their own characters.** Same per-defender routing, just
  more than one human source. No architecture change anticipated beyond the existing AI-vs-player
  split per defender.

**This is the reason the window opens on target confirmation, not turn-start.** The turn-start /
all-defenders-watching model implicitly assumed **independent defenders** (a multiplayer-ish world
where each defender is its own agent watching from the turn flip). In single-player one human controls
the whole team — they cannot be in a "watching" state for three characters *before* knowing which are
targeted. Opening on target confirmation means the player (or each AI) defends a **known** set of
targeted characters. *(See the multi-target input open question in §3a-Q below — it falls out of "one
player defends several targeted characters at once.")*

### §3a-Q — OPEN QUESTION: multi-target defense input (resolve at Stage 3 / Stage 5)

When an AOE hits **multiple player-controlled characters at the same hit-frame**, how does the **single
player** input defense for all of them? Options:

- **(a) One timed input defends all characters hit at that moment** — AOE is **one reactive beat**;
  the player's single block/parry/dodge applies to every player-target hit by that impact. *(Crown's
  lean.)*
- **(b) Defend each character separately** — independent input per targeted character (heavy: N
  simultaneous timing problems for one human).
- **(c) Pick / cycle focus** — the player selects which character the input applies to.

**Crown's lean: (a)** — one input per AOE beat. **Unresolved — decide when Stage 3 (per-hit defense
state) and Stage 5 (per-hit UI) are built**, since it shapes both the input model and the prompt UI.

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
| 1 | **Window open** | Opens **inside** each `Execute<Type>Async`, AFTER `BeginSkillExecution` warps the caster in and animation is queued — i.e. **after target commit**. **This is now ≈ the desired open point** (target confirmation, §2) — NOT a gap. The only lifecycle change is persist-across-hits + close-at-last-hit (Stage 1); **no turn-start inversion needed**. | `ActionExecutor.cpp:1105–1120` (spell), `:1272–1282` (ability), `:1409–1419` (attack) → all converge at `OpenDefenseWindowsForTargets` `:1505–1578`. Warp set in `BeginSkillExecution` `:4158–4222` (target at `:4216`). |
| 2 | **Window close** | **Fixed 0.3s timer** (0.5s AOE), decoupled from the montage. `// TODO: get from spell data`. | `DefenseSystem.h:308–314` (`DefaultWindowDuration=0.3f`, `AoeWindowDuration=0.5f`); timer `DefenseSystem.cpp:73–86`; expiry `:445–456` → `CloseDefenseWindow` `:124–186`. Durations passed at `ActionExecutor.cpp:1119`, `:3093` (AOE), `:3336` (projectile). |
| 3 | **Multi-hit** | **LUMPED** — one `FDefenseResult` per defender, split across `HitCount` in a synchronous loop. **Mitigant:** `ApplyHit` is per-hit-shaped and `ResolvedDamageSplit` is already per-hit. | `ApplyDamageAfterDefense` loop `ActionExecutor.cpp:1702–1734`; `ApplyHit` `:2342`; split resolved at `FinalizeDamageInputs` `:1101` → `ResolvedDamageSplit`. |
| 4 | **Defense state** | **One-shot** — `bInputReceived` rejects a 2nd input; never cleared. | `FDefenseState` `DefenseSystem.h:57–98`; guard `DefenseSystem.cpp:211–216`; record `:229–231`; `SubmitDefenseInput` / `CalculateDefenseResult` in `DefenseSystem.cpp`. |
| 5 | **Hit-frame notify** | **EXISTS but STUB.** `ECombatNotifyFamily::Hit` fires on the montage but the handler is log-only and returns (`"…stub — damage wiring is SC4"`). This is the candidate melee impact trigger. | enum `CombatNotify.h:14–20`; handler `ActionExecutor.cpp:4803–4806`. |
| 6 | **Projectile impact** | Opens a defense window on impact, then applies damage via the same lumped path. **Attacker is NULL** — the projectile doesn't track its source. | `OnProjectileImpact` `ActionExecutor.cpp:3322–3351`; open `:3338` (`OpenDefenseWindow(nullptr, …)`); fallback apply `:3349`. |
| 7 | **AOE path** | **Per-target single-resolution, block-only.** One window per target at `AoeWindowDuration`; comment: "AOE can only be blocked (no dodge, no parry)". | dispatch `ActionExecutor.cpp:2904–2910`; `SpawnAOEEffect` open `:3090–3100`. |
| 8 | **AI defense** | **One decision per window** — one reaction-delay timer → one `ChooseDefenseType` → one `SubmitDefenseInput`, then locked. | trigger `DefenseSystem.cpp:111–121`; `ScheduleDefenseDecision` `AIDecisionManager.cpp:306–409`. |
| 9 | **Target resolution timing** | Targets are **committed in the FAction before windows open** — windows open for the already-locked valid-target subset at Execute time. **This MATCHES the target-confirmation open model (§2) — correct by design, no gap.** *(Previously flagged as "not turn-start"; the turn-start model is dropped.)* | filter `ExecuteSpellAsync:1048` (`FilterValidTargets(Action.Targets)`); open `:1105–1120`. |
| 10 | **HitCount** | **Authored per skill** (UPROPERTY), inherited by attack/ability/spell via `CastableSkillDataBase`; per-hit distribution via DamageSplit (even split if empty). **Correct — no gap.** | `SkillDataBase.h:67` (`int32 HitCount=1`), DamageSplit `:69–71`, `ResolveDamageSplit` `:43`; `WeaponAttackData.h:28/70`. |

**DefenseSystem class:** `Public/Combat/Defense/DefenseSystem.{h}` / `Private/Combat/Defense/DefenseSystem.cpp`;
enums `Public/Combat/Defense/EDefenseType.h`, `EDefenseDirection.h`.

---

## 7. The Gap

Two structural mismatches (the open-timing mismatch is **resolved by the target-confirmation model** —
see below), plus Crown's refinements:

- **Open timing — NOT a gap (resolved).** The window opens inside `Execute<Type>Async` (post-warp,
  after target commit), per the already-locked target subset (anchor 1, 9). Under the
  **target-confirmation** open model (§2) this is **≈ correct** — you open knowing who is targeted.
  *(The earlier spec called this "too late / not all-defenders-watching" and demanded a turn-start
  inversion. That requirement is DROPPED: in single-player one player controls the whole team, so
  turn-start/all-defenders is incoherent — §3a. No open-side structural change is needed.)*
- **Close decoupled.** Fixed 0.3s/0.5s timer (anchor 2), unrelated to montage length or hit-frames.
  It can close mid-attack or outlast the visible hits. Must move to **last-hit / montage-coupled**.
  **This is the one window-lifecycle change that remains** (Stage 1).
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
**data dependency** — re-ordered post-sequencing-survey: **the window lifecycle is the FOUNDATION, not
the capstone.** The per-hit stages have no live window / result / context to attach to until the window
persists across the hit-frames. See §9 for the forcing argument. **Scope reminder:** base outcomes +
per-hit timing + condition-FIRING hooks are **in**; condition payoffs (reflect/counters/buffs) are
**out** — built later as subscribers (§5).

| Stage | Work | Type | Size |
| --- | --- | --- | --- |
| **0. Hit-frame notifies** *(DONE — commit `4986d450`)* | Wire the existing **Hit-family Combat Notify stub** to be the melee impact trigger — replace the log-only return with a real per-hit entry point. `OnCombatNotifyReceived` now broadcasts `OnHitFrame(Executor, Index)` per hit (`ActionExecutor.cpp:4815–4828`); fires into the void until Stage 2 binds it. Crown **authors Hit notifies on every multi-hit montage** (see index-match contract in §9). **BLOCKED Stages 1–4 → now unblocked.** | **[Anim]** + small **[Code]** | Small |
| **1. Window lifecycle (FORCED FIRST — the foundation)** | **Make the window persist across hits and close on a COUNT-BASED universal trigger** — close when `HitsLanded == ExpectedImpacts` (per-defender counter reaching a per-defender expected count, **§8a**), NOT a melee-specific "last montage hit-frame." Drop the fixed `0.3s`/`AoeWindowDuration` timer as the closer (`DefenseSystem.cpp:73–86`); keep the `FDefenseState` + `FPendingDefenseContext` alive across **all** hits. **Stage 1 wires MELEE** (each Hit notify increments `HitsLanded`; `ExpectedImpacts = HitCount` set at the melee open); projectile/AOE/beam plug into the **same counter** at Stage 6 (**extend, not replace** — §8a). **This unblocks Stages 2–3** — until the window survives the hits, per-hit resolution/defense have nothing to read (§9). **The window still opens at ~execution-time (on target confirmation, §2) exactly as it does today — there is NO turn-start inversion** (dropped this session; §3a). The only change is *when it closes* (count-complete, not 0.3s) and that its state *persists* through the hits. **⚠ MAX-DURATION FAILSAFE IS NON-NEGOTIABLE — see §9.** | **[Code]** | Medium |
| **2. Per-hit resolution + base outcomes + condition firing** *(needs #1)* | Drive `ApplyHit` (`:2342`) off **each Hit notify** via `OnHitFrame` (and **projectile impact** `:3322` for ranged), each gated by *that hit's* defense result — now readable because the window persists (Stage 1). Replace the lumped loop (`ApplyDamageAfterDefense` `:1702`). Index into `ResolvedDamageSplit[Index]` **with a bounds guard** (§9 index-match contract). Implement the **base outcomes** (§4: block reduce / dodge zero / parry negate+status). **Fire the conditions** (§5: `OnBlock`/`OnDodge`/`OnParry` + `OnPerfect*` + `OnDefend` + `OnHit`) at each resolution — the hooks fire even before any subscriber exists. | **[Code]** | Large |
| **3. Per-hit defense state + timing/PERFECT** *(needs #1, #2)* | Rework `FDefenseState` / `SubmitDefenseInput` to **re-arm input per hit-frame** (clear `bInputReceived` each hit; `DefenseSystem.cpp:211–216`/`:229–231`). Implement the **lead-in window per defense type** and the **PERFECT band** (§3); `CalculateDefenseResult` evaluated **per hit**, producing the outcome band that selects which condition fires. Route input per the **Control Model** (§3a: player-team → player input, enemies → AI). **RESOLVE the multi-target input question (§3a-Q)** here — Crown's lean is one input per AOE beat. | **[Code]** | Large |
| **4. Per-hit AI** | Replace the single `ScheduleDefenseDecision` (`AIDecisionManager.cpp:306`) with **N scheduled decisions per defender, one per hit-frame**, each judged independently against that hit's lead-in window. Reaction delays scaled to the PlayRate-adjusted hit-frame times. (See §9 AI-degradation note — the single-decision path must survive Stages 1–3 gracefully until replaced here.) | **[Code]** | Medium |
| **5. Per-hit UI** | Per-hit defense prompt / feedback — surface each incoming hit, its lead-in window, and the outcome band (including PERFECT) (prompt widget currently TODO-stubbed). Must reflect the multi-target input decision (§3a-Q) — e.g. one shared AOE-beat prompt vs per-character prompts. | **[Code]** + UI | Medium |
| **6. AOE/projectile/beam → counter + source-threading** | **Plug projectile/AOE/beam into the count-based close (§8a)** — each adds a one-line `HitsLanded` increment (`OnProjectileImpact` `:3334`, `SpawnAOEEffect` `:3102`, `OnBeamTick` `:3381`) and sets its own `ExpectedImpacts` at open (single projectile = 1; barrage = **`FSkillCastEntry::Count`**, authored + wired; beam = `BeamTickCount`; AOE = 1, `Count` not honored for AOE today) — see the spell formula in §8a. AOE: every AOE-hit defender runs the full per-hit flow with **dodge/parry/block** (remove the block-only limitation). Projectile: thread the **attacker source** through to `OnProjectileImpact` (fix the NULL attacker `:3351`) so base parry-negate works and `OnPerfectParry` subscribers (e.g. reflect) have the source. **Inherent extra lifts (any close model):** (a) projectile/AOE windows currently open **at impact**, decoupled from cast — must move to **target-confirmation + persist until last impact**; (b) **multi-pulse AOE is unbuilt** — the AOE branch ignores `entry.Count` today (`:3281–3286`); honoring it is an optional Stage 6 add; (c) projectile/beam impacts can arrive **after `CurrentExecutionContext` tears down** (montage finished first) — keep the context alive until the last impact (a melee-Stage-1 non-issue, since the montage spans the hits). | **[Code]** | Medium–Large |

**OUT OF SCOPE (separate future effort — effects/items system):** the condition **payoffs** —
projectile reflect, counters, on-defense buffs, etc. The rework fires `OnBlock`/`OnParry`/`OnPerfect*`
/`OnDefend`/`OnHit`; subscribers that *do something* with them are content built when the effects/items
system is worked on.

**Free fallout (after Stages 1+2):** once hits fire on montage-riding notifies and the window spans
them, PlayRate (`SpellSpeed`/`ActionSpeed`) makes hit-frames arrive sooner → tighter lead-in windows
automatically, no extra code. The speed payoff is a *consequence* of the rework.

### §8a. Window close model — COUNT-BASED / UNIVERSAL (not "last montage hit-frame")

The window closes when **`HitsLanded == ExpectedImpacts`** — a **per-defender counter** reaching a
**per-defender expected count**. This is source-agnostic and works for every attack type; it is
**NOT** the melee-specific "close on the last montage hit-frame (`Index == HitCount-1`)."

**`ExpectedImpacts` is set by the OPENER, per attack type — NOT hardcoded to `HitCount`:**

| Attack type | Hit event (increments `HitsLanded`) | `ExpectedImpacts` |
| --- | --- | --- |
| Melee combo | montage Hit notify (`ActionExecutor.cpp:4815`) | `HitCount` |
| Single projectile | `OnProjectileImpact` arrival (`:3334`) | **1** (= `Count` 1) |
| Multi-projectile / barrage | each projectile arrival | **`FSkillCastEntry::Count`** (authored + wired) |
| AOE | `SpawnAOEEffect` resolution per defender (`:3102`) | **1** (`Count` not honored for AOE today) |
| Beam / channel | `OnBeamTick` (`:3381`) | **`BeamTickCount`** |

> For a multi-entry spell the value is a **sum across `CastArray` entries** — see the spell formula below.

**WHY NOT compare against `HitCount` directly.** `HitCount` (`SkillDataBase.h:67`, default 1) is the
**melee montage hit-frame count** (and drives `ResolvedDamageSplit`). It is **not** a universal
expected-impact count:
- **Beam** uses `BeamTickCount` — a *separate* init-computed field
  (`SkillProjectile.h:249–250`, `= max(1, RoundToInt(BeamDuration / BeamTickInterval))`; per-tick
  damage `= BaseDamage / BeamTickCount`). A beam with `HitCount=1` but `BeamTickCount=8` would close
  after **one** tick if compared against `HitCount`.
- **Single projectile / AOE** deliver **1** impact regardless of the skill's `HitCount` (one
  arrival/resolution carries the lumped damage). `HitCount>1` would stall the counter → failsafe.

So the close target **must** be the stored `ExpectedImpacts`, not `HitCount`.

**Spell `ExpectedImpacts` = the BURST count (Stage 6).** A spell's impacts are authored as
**`FSkillCastEntry::Count`** (`SkillCastEntry.h:87`, default 1) — Crown's "×3 fireball" = `Count=3` —
staggered by **`BurstInterval`** (`:91`, default 0.15s). It is **authored on the spell's `CastArray`
entries and already wired**: `DispatchCastEntry` (`ActionExecutor.cpp:3253–3262`) honors `Count` for
projectile/homing/beam deliveries, and `SpawnNextBurstProjectile` (`:3297`) drains the staggered
queue — each burst is a **separate projectile → separate `OnProjectileImpact` arrival**, so
`Count` = arrival count. *(The earlier survey's "no `ProjectileCount`/`BarrageCount` field" finding
searched the wrong name — the count is `Count`.)* Per-defender formula, **summed across the `CastArray`
entries that hit that defender**, per delivery type:

```
Spell ExpectedImpacts(defender) = Σ over CastArray entries hitting that defender of:
    Projectile / Homing : entry.Count        (barrage — wired today)
    Beam                : BeamTickCount       (Count usually 1; derived from BeamDuration /
                                               BeamTickInterval, recomputable at open)
    AOE  / Instant      : 1                   (Count NOT honored for AOE today; multi-pulse AOE
                                               is unbuilt — a Stage 6 option)
```

- **Common case:** one Projectile entry `Count=3` → `ExpectedImpacts = 3`. **Single projectile = `Count` 1**, not a special case.
- **Mixed (fireball-then-pillar):** `[Projectile Count=3, AOE Count=1]` hitting one defender → `3 + 1 = 4`. It is a **SUM across cast entries**, not a single field read.
- **Visible at open-time:** `Count` is authored and read at cast/dispatch **before** spawn, so the Stage 6 opener reads it straight off the `CastArray` — **no projectile-side threading for barrage**. Only **beam** derives its count (`BeamTickCount`), and even that is recomputable from authored entry data at open.

**Counter location — two new fields on `FPendingDefenseContext`** (`ActionStructs.h:293`, already the
per-defender record carrying `HitCount`):

```
int32 HitsLanded = 0;       // incremented by ANY hit source
int32 ExpectedImpacts = 0;  // set by the opener; melee = HitCount
```

The counter belongs with the **per-target attack record**, not `FDefenseState` — `DefenseSystem`
shouldn't know about hit-counting. All hit sources increment
`CurrentExecutionContext->PendingDefenses[Target].HitsLanded` and close that defender's window when it
reaches `ExpectedImpacts`.

**EXTEND, NOT REPLACE.** Stage 1 builds the mechanism wired for **melee** (notify increments;
`ExpectedImpacts = HitCount`). Stage 6 adds projectile/AOE/beam — **each a one-line increment + sets
its own `ExpectedImpacts` at open.** The close mechanism is built **once** (Stage 1), **extended**
(Stage 6), and **never replaced.** This is the whole reason to build count-based now: the melee-locked
`Index == HitCount-1` check would have to be unpicked for projectiles; the counter does not.

**Robustness.** Count-based is **order-independent** (a misordered notify is fine — only the *count*
matters), handles **melee cleave** (each defender has its own counter, closes independently),
**defenders added/removed mid-attack**, and **overshoot** (a duplicate notify closes one hit early —
harmless; `CloseDefenseWindow` on an already-closed window just warns and returns). **Tail cases**
(target dies mid-combo, leaves the beam, a barrage projectile misses) leave `HitsLanded` short of
`ExpectedImpacts` → the **max-duration failsafe** closes them — acceptable. The messiest tail is
**beam-target-leaving** (`bTargetInBeam==false` ticks don't land, `:3390`): `ExpectedImpacts` should
count **scheduled** ticks, not in-beam ticks — a **Stage 6 refinement**.

---

## 9. Risks / Notes

- **⚠ MAX-DURATION FAILSAFE IS NON-NEGOTIABLE (Stage 1).** Today the 0.3s timer *is* the only
  thing that guarantees the window ever closes. Once close moves off that timer to the **count-based
  trigger** (`HitsLanded == ExpectedImpacts`, §8a), any case where **`HitsLanded` never reaches
  `ExpectedImpacts`** — a missing montage, a missing / late / dropped Hit notify, fewer notifies than
  `ExpectedImpacts`, or a tail case (target dies mid-combo / leaves beam / barrage miss) — **hangs the
  window open forever** → `CheckAndFinalizeAsyncAction` never fires → the turn never advances →
  **FROZEN combat** (not a missed defense — a hard hang). Stage 1 **must** ship a **max-duration
  backstop timer** that force-closes/resolves any window whose counter never completes, modeled on the
  existing `AsyncTimeoutHandle` failsafe (`ActionExecutor.cpp:~1480`, the `5.0f` `OnAsyncActionTimeout`).
  This is the single most important line item in the stage — without it the failure mode is a frozen game.
- **WHY window-first is FORCED (the data-dependency argument — supersedes the old "prove easy parts
  first" note).** The window is the **data FOUNDATION** the per-hit stages read from. On the current
  window, `CloseDefenseWindow` (`DefenseSystem.cpp:124`) at **~0.3s** *destroys all of it before the
  first hit-frame fires*: it `ActiveDefenseStates.Remove(Defender)` (`:143`, kills the `FDefenseState`),
  broadcasts `OnDefenseWindowClosed` → `ApplyDamageAfterDefense` applies **all** damage in one lumped
  pass (`ActionExecutor.cpp:1603`), then `PendingDefenses.Remove(Defender)` (`:1639`, kills the
  `FPendingDefenseContext`). By the time hit-frame 1 fires (later in the ~3.6s montage) there is **no
  `FDefenseResult`, no `FPendingDefenseContext`, and the damage is already spent.** So per-hit
  resolution (Stage 2) and per-hit defense (Stage 3) have **nothing to attach to** — they *cannot* be
  built until the window persists across the hits (Stage 1). **Window-first is not a preference;
  it is forced by data lifetime.** *(The prior note — "sequence the window after per-hit resolution
  and state are proven" — was risk-management reasoning and is WRONG: it had the dependency backwards.)*
- **NO turn-start inversion — Stage 1 is just persist + close-at-last-hit + failsafe.** The earlier
  spec carried a structural inversion (open at turn-start for ALL potential defenders, before target
  commit, out of `Execute<Type>Async`) as the riskiest piece of the arc. **It is DROPPED this
  session.** The window now opens **on target confirmation** (§2) — which is ≈ the current
  execution-time open point — because in single-player one player controls the whole team and cannot
  watch for an unknown target set (§3a Control Model). So Stage 1 keeps the existing open and changes
  **only the close** (last-hit, not 0.3s) plus **state persistence** across the hits. The riskiest
  structural lift is **gone** — Stage 1 is now Medium, not Medium–Large.
- **Hit-notify INDEX-MATCH authoring contract (Stage 0 ↔ Stage 2).** Crown must author Hit notifies
  indexed **`0, 1, 2 … HitCount-1`**, one per impact, so Stage 2 can read `ResolvedDamageSplit[Index]`
  for *that* hit's damage. The `Index` carried by `OnHitFrame(Executor, Index)` is the split index.
  Stage 2 **must** bounds-guard (`ResolvedDamageSplit.IsValidIndex(Index)`) and **log loudly** on
  mismatch (notify count ≠ `HitCount`, or out-of-range index) — a miscounted notify otherwise silently
  reads the wrong hit's damage (or trips the failsafe by never delivering the last hit).
- **AI degradation during Stages 1–3 (replaced in Stage 4).** `ScheduleDefenseDecision`
  (`DefenseSystem.cpp:119`) is handed `WindowDuration` and reacts on the 0.3s clock. After Stage 1
  the window lives **seconds longer**; the single-decision AI path is not rewritten until Stage
  4. **Confirm it degrades gracefully** — one stale decision on the longer window — rather than
  crashing or mis-firing. (At Stage 4 the per-hit schedule must use **PlayRate-scaled hit-frame
  times**, not a fixed duration.)
- **Projectile NULL-attacker threading.** `OnProjectileImpact` opens with `attacker = nullptr`
  (`:3338`). Base parry-negate works without it, but `OnPerfectParry` reflect needs the source.
  Scoped into Stage 6.
- **Condition hooks fire into the void at first.** Stage 2 fires `OnBlock`/`OnParry`/etc. with **no
  subscribers**. That is intentional — the platform must exist before payoffs. Verify the events fire
  (logging) even though nothing consumes them yet. (Stage 0's `OnHitFrame` is likewise already firing
  into the void until Stage 2 binds it.)

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

- The window self-destructs at 0.3s — **Stage 1** makes it persist across hit-frames + close at the
  last hit + adds the max-duration failsafe. *(No turn-start inversion — dropped this session; the
  window opens on target confirmation, §2/§3a. The arc's riskiest piece is GONE.)*
- Multi-hit resolution is lumped — **Stage 2** restructures the core resolution + adds base outcomes +
  condition firing.
- The defense state machine is one-shot — **Stage 3** makes it per-hit re-armable and adds the
  timing/PERFECT model.
- The AI scheduler is one-decision — **Stage 4** makes it per-hit.
- **Animation-authoring is on the critical path** — Stage 0 (Hit notifies on multi-hit montages) is
  Crown's work and **blocked** Stages 1–4 *(now DONE, commit `4986d450`)*.
- Heavy **PIE verification of real-time feel** (lead-in window tuning per defense type, perfect
  threshold, per-hit reaction tightness, speed-scaled difficulty, multi-target input feel §3a-Q) —
  not unit-testable.

The only piece that de-risks the size is `ApplyHit` already being per-hit shaped; everything around
it (window persist + close-at-last-hit + failsafe, resolution sequencing, timing/PERFECT model, base
outcomes, condition hooks, input re-arm, control-model routing, per-hit AI, UI, AOE per-hit,
projectile source-threading, notify authoring) is net-new.

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

- **2026-06-14** — **Corrected the "missing `ProjectileCount`" finding + recorded the spell
  `ExpectedImpacts` formula (§8a).** The barrage-count field **exists**: **`FSkillCastEntry::Count`**
  (`SkillCastEntry.h:87`, default 1) with **`BurstInterval`** (`:91`, 0.15s) the inter-burst stagger —
  authored on the spell's `CastArray` entries and **already wired** (`DispatchCastEntry` honors `Count`
  for projectile/homing/beam; `SpawnNextBurstProjectile` drains the staggered queue; each burst is a
  separate projectile → separate arrival). The prior survey searched `ProjectileCount`/`BarrageCount`/
  `NumProjectiles` — wrong name. Recorded **spell `ExpectedImpacts(defender)` = Σ over `CastArray`
  entries hitting that defender** (Projectile/Homing = `entry.Count`; Beam = `BeamTickCount`; AOE/Instant
  = 1), a **sum across cast entries, not a single field read**; single projectile = `Count` 1; mixed
  fireball-then-pillar sums (3+1=4). `Count` is **visible at open-time** (read at dispatch before spawn)
  → no projectile-side threading for barrage (only beam derives its count, recomputable). Noted **AOE =
  1 today** (`Count` not honored for AOE; multi-pulse AOE unbuilt — a Stage 6 option) and corrected the
  Stage 6 row's stale "barrage has no field" lift. **Stage 1 (melee = `HitCount`) unaffected.**
- **2026-06-14** — **Window close is COUNT-BASED / universal, not "last montage hit-frame."** The
  window closes when **`HitsLanded == ExpectedImpacts`** — a per-defender counter reaching a per-defender
  expected count (new **§8a**) — source-agnostic across melee / projectile / barrage / AOE / beam.
  **`ExpectedImpacts` is an explicit per-defender field set by the OPENER** (melee = `HitCount`; single
  projectile/AOE = 1; beam = `BeamTickCount`; barrage = projectile count) — **NOT a hardcoded `HitCount`
  comparison** (`HitCount` is the melee montage hit-frame count and does not map: a beam with `HitCount=1`
  but `BeamTickCount=8` would close after one tick). Counter lives as two new fields on
  `FPendingDefenseContext` (`HitsLanded`, `ExpectedImpacts`), not `FDefenseState`. Built **melee-first**
  (Stage 1: notify increments) and **extended, not replaced** at Stage 6 (projectile/AOE/beam each a
  one-line increment + own `ExpectedImpacts`). Updated Stage 1 + Stage 6 rows; noted Stage 6's inherent
  lifts (open-at-cast-not-impact, no `ProjectileCount` field yet, keep context alive past montage end)
  and the beam-target-leaving tail (`ExpectedImpacts` = scheduled ticks). Re-aligned the §9 failsafe to
  the "counter never completes" framing.
- **2026-06-14** — **Window opens ON TARGET CONFIRMATION, not turn-start; control model added; window
  stage simplified.** (1) Window-open changed from "turn-start / all-defenders-watching" to **on target
  confirmation** (after action+target commit), windows for exactly the targeted defenders — single →
  one, AOE → all targeted (§2). Rationale: turn-start/all-defenders assumed independent defenders
  (multiplayer-ish); in single-player one player controls the whole team and can't watch for an unknown
  target set. (2) Added **§3a Control Model**: AI characters defend independently; single-player → the
  player controls ALL player-team defenders (all route to player input); multiplayer (future) → each
  player defends their own. (3) **Window stage simplified** — the old turn-start structural inversion
  (Half B) is **DROPPED**; Stage 1 is now **persist-across-hits + close-at-last-hit + max-duration
  failsafe only** (the window keeps its current execution-time open). Removed the Half A/B split;
  updated §6 anchors 1/9, §7 gap, §8/§9/§11 accordingly — the arc's riskiest piece (the inversion) is
  gone, Stage 1 drops to Medium. (4) Logged an **open question (§3a-Q)**: multi-target defense input —
  one input per AOE beat (Crown's lean) vs per-character vs focus-cycle — to **resolve at Stage 3 / 5**.
- **2026-06-14** — **Stage re-order per the sequencing survey: window lifecycle moves 3 → 1 (FORCED
  first, not last).** Proved the per-hit stages have **no live window / result / context to attach to**
  — the current window destroys its `FDefenseState` + `FPendingDefenseContext` and applies all damage
  at ~0.3s (`CloseDefenseWindow` → `ApplyDamageAfterDefense` → `PendingDefenses.Remove`) **before the
  first hit-frame fires**. Window-first is **data-forced, not preference**. Split Stage 1 into **Half A**
  (persist-across-hits + close-at-last-hit + max-duration failsafe — the forced blocker, build now) and
  **Half B** (turn-start/all-defenders inversion — riskier, deferrable to Stage 6). Made the
  **max-duration failsafe NON-NEGOTIABLE** (missing/late/miscounted Hit notify → frozen combat). Added
  the **index-match authoring contract** (notifies `0…HitCount-1`; Stage 2 bounds-guards
  `ResolvedDamageSplit[Index]`) and the **AI-degradation note** (single-decision path must survive
  Stages 1–3 gracefully until Stage 4). Flipped the §9 sequencing rationale (the old "prove easy parts
  first" note had the dependency backwards). Marked Stage 0 **DONE** (commit `4986d450`).
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
