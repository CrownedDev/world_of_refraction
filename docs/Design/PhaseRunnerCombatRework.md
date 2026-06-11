# Phase-Runner Combat Rework — Seamless Cinematic Actions + E33-Style Active Defense

> **Status: DESIGN ONLY — not being built now.** Captured for a dedicated arc.
> **Supersedes** `RealTimeDefenseRework.md` — that doc scoped per-hit defense only; this absorbs it.
> The defense rework is now **one feature of the phase runner**, not a standalone project.
> All `file:line` anchors reflect the codebase at time of writing — re-verify before picking this up.

---

## 0. Why this got bigger (the reframe)

The defense rework asked "how do we make hits land at their visual impact?" Pulling that thread
exposed the real constraint: **the action pipeline hardcodes phase order in its delegate chain.** It can
only express *approach-then-action* (spells "skip" the approach by returning `ExecutionRange = 0`). It
cannot represent a skill that goes cast → approach → strike, or cast → strike, or strike → reposition →
strike. There is no seam in the code to insert a "cast" before the approach because *cast is not a thing
the pipeline knows about.*

Making hits land correctly **and** supporting unique skill structures **and** getting the smooth E33
cinematic feel all resolve to the same change: **make phase order data, and let a runner walk it.** The
defense rework rides on top of that runner.

### Locked design pillars (Crown)

1. **Cinematic feel is a presentation layer.** Sim (turn order, commit, resolution, damage) is authoritative
   and deterministic; presentation (montages, warping, camera, VFX) is a local view rendered from sim
   events. This is the shared-world invariant — presentation never drives the outcome.
2. **A skill is an ordered list of phases.** `[Cast?][Approach?][Attack]`. Attack is the mandatory
   terminal phase; Cast and Approach are optional lead-ins.
3. **Two tiers, one runner** (see §3). The tier is *emergent from how many slots a skill fills* — not two
   code paths.
4. **Approaches are a reusable vocabulary**, not bolted to one skill. Walk / Dash / Teleport / Fly are
   phase archetypes with their own defense profiles, reusable across every skill.
5. **Resolution is data, the animation is the view.** A hit resolves via a `ResolveHit()` function; the
   trigger (notify now, server timer later) just *calls* it.

---

## 1. The phase model

A skill data asset (`USpellData` / `UAbilityData` / `UWeaponAttackData`) declares an ordered phase list.
The executor becomes a **phase runner**: play phase → wait for its done-signal → advance.

| Phase | Does | Done-signal | Carries |
| --- | --- | --- | --- |
| **Cast** | In-place windup / charge, no translation | montage notify or duration | VFX cues |
| **Approach** | Move to target (Walk / Dash / Teleport / Fly) | `OnMovementComplete` or montage-end | movement archetype + optional defense window |
| **Strike** | The hit montage | hit-frame notifies / montage-end | hit-times, defense resolution |
| **Return** | Move back to grid slot | `OnMovementComplete` | — |
| **Channel** *(future)* | Sustained beam / hold | duration / interrupt | per-tick resolution |

A unique skill = a unique **ordering and content** of this shared vocabulary, not a unique code path:

- Cast-then-charge-then-strike: `[Cast, Approach, Strike, Return]`
- In-place nuke: `[Cast, Strike]`
- Two-stage combo: `[Approach, Strike, Strike, Return]`

### Why this is a consolidation, not bloat

It collapses the three bespoke async executors — `ExecuteSpellAsync` (`ActionExecutor.cpp:516`),
`ExecuteAbilityAsync` (`:637`), `ExecuteAttackAsync` (`:744`) — into one driver. Those three currently
carry **known divergence bugs** the analysis docs already flag: the async-attack buildup leak, the missing
`FinalizeAsyncAction` Ability branch (`Ability->Effects[]` may not fire), per-orchestrator atomicity
differences (`Codebase_Analysis_Pass2_ApplyConsolidation.md` §3.8). One runner removes the class of bug.

---

## 2. The seam — why current actions look spliced

Today translation and animation are decoupled and stitched at a hard handoff:

- `CombatMovementComponent` **lerps** actor translation via tick (`MoveToward` / `SetActorLocation`) while
  an **in-place** movement montage plays on top.
- `OnMovementComplete` (`ActionExecutor.cpp`) → **hard switch** → separate action montage
  (`PlayActionMontage`).
- Two separately-authored montages blended at runtime → the run-in pose never matches the attack's start
  pose → visible pop.

E33 avoids this entirely by authoring each skill as **one Sequencer cinematic** with actors bound in — the
seamlessness is *authored as one unit*, never blended at runtime. We don't adopt the Sequencer pipeline
(33-person authoring machine); we get the same *feel* with the two-tier montage model below.

---

## 3. The two tiers

Smoothness is **pose continuity at the phase boundary.** There are two ways to achieve it, and they are in
direct tension (a phase can't be *both* reusable/optional *and* seamlessly fused). So it's a per-skill
choice, expressed purely in data:

### Tier 1 — Modular (default; the bought-asset bulk)

- Fill **Cast / Approach / Attack** slots with separate montages.
- The **runner owns timing**: it knows the phase boundaries explicitly, drives Approach translation, and
  opens defense at the marked phase.
- Smooth-enough via **pose-matched boundaries + inertialization (UE5 inertial blending)** — preserves
  velocity through the transition, kills the foot-slide a linear crossfade causes.
- **Any purchased anim is Tier 1-capable out of the box**, including in-place anims (movement component
  handles the travel, the in-place anim is the strike).

### Tier 2 — Fused signature (hero skills; a later stage)

- Fill **only the Attack slot** — cast + approach + strike baked into one continuous montage. Zero seam.
- The **montage owns timing**: the runner sees one opaque phase. This forces two hard requirements:
  - **Root motion is mandatory.** The runner isn't translating the character — the montage is. An in-place
    anim here dances in place and never reaches the target. Root motion (+ Motion Warping onto the target)
    drives the path.
  - **Everything the runner reacts to must be a notify** — defense-window-open, hit-frames — because the
    runner can't infer beats from an opaque montage. (This is already how the defense rework wants
    hit-frames, so it's free.)
- Required for any **complex movement path** (e.g. an airborne dance into an air strike) — the path is an
  arc, not the straight point-to-point line the current lerp can do.

**The runner does not branch on tier.** It plays phases and waits for done-signals. A fused skill is just
"one long Attack phase with root motion." The tier lives in the data.

---

## 4. Purchased assets — what plugs in and what costs ten minutes

| Question | Answer | Cost |
| --- | --- | --- |
| **Where's the target?** (Tier 2 travel) | **Motion Warping** bends the anim's root trajectory onto the actual enemy, regardless of the distance the anim was authored for | Free / automatic |
| **Where are the hits?** | **You place `AttackImpact` notifies by eye** on the impact frames. This info does not exist in animation data — every game does this step | ~10 min / anim, mandatory |
| **Root motion present?** | If yes → Tier 2-eligible after notifies + warp target. If in-place → Tier 1 only | the import-checklist question for every asset |

Asset-import checklist, per anim: **root motion? → which tier it can serve. Then place impact notifies.**
(`Crown: most existing combat montages have root motion → Tier 2 is "annotate what we have" for the bulk.`)

---

## 5. Defense — folded in as a phase feature

### Window vs resolution (the core distinction)

- **Window = input availability.** Open from **turn start to turn end** (Crown's model) — the defender is in
  an active-defense state for the whole incoming attack, including the approach. One continuous window, not
  N windows.
- **Resolution = per-hit judgment.** Each hit samples *what the defender is doing at that hit's own impact
  frame* and resolves **independently** — parry hit 1, eat hit 2, dodge hit 3.

The bug to kill is today's **lumped** model: one input → one result split across all hits in a synchronous
single-frame loop (`ApplyDamageAfterDefense` `:1130`/`:1371`; `DamagePerHit = FinalDamage / HitCount`).
Continuous window, **per-hit-frame resolution** — never one lumped result at window-close.

### Hit-time = the notify's timestamp

A hit-time is **an offset in montage-local seconds from its phase's start to the moment the hit resolves.**
You never hand-type it: **placing the `AttackImpact` notify IS authoring the hit-time** — the notify's
position on the timeline is the number.

- **Cast / Approach** carry no hit-times (Approach can carry a *defense window*, but that's not a hit).
- **Strike** carries the list, e.g. a 3-hit strike → notifies at `[0.3, 0.6, 0.95]`.

### The one discipline that makes it online-clean

**The notify handler must not *do* damage — it must *call* the resolve function.**

```cpp
// NOT this — damage logic trapped in the notify; only reachable by playing the anim:
OnNotify(AttackImpact) {
    Target->HP -= Damage;
    ApplyBuildup(...);
}

// THIS — notify is just a trigger:
OnNotify(AttackImpact) {
    ResolveHit(HitIndex);   // server timer calls the SAME function later
}
```

- **Single-player (now):** the notify fires live → calls `ResolveHit`.
- **Server (later):** reads the **same notify timestamp as a plain float** (`Montage->GetAnimNotifies()`),
  schedules `ResolveHit` on a timer. No animation runs; the anim never affects the outcome.

Same number, two readers. Costs nothing now; the shared-world door is open instead of walled off.

### PlayRate scaling (the speed-stone payoff)

Hit-times are montage-local, so `SpellSpeed` / `ActionSpeed` (PlayRate) change real-world timing:

- **Notify:** rides the montage clock → fires sooner at higher PlayRate, free.
- **Server timer:** must scale → `real_offset = authored_offset / PlayRate`. One division. (Same fix as the
  doc's existing "AI duration must follow PlayRate" note.)

This is the "speed gains defensive teeth for free" payoff — faster montage → notify sooner → tighter
reaction window — and it falls out of measuring hit-time in montage-local seconds.

### Approach defense profiles

Approaches stop being filler and become the attack's opening threat beat. Each archetype pairs with a
defense profile (telegraph length, valid defenses, window tightness):

| Approach | Tell | Defender answer | Feel |
| --- | --- | --- | --- |
| **Walk / step** | long, fully readable | easy parry | honest baseline, feint setup |
| **Dash / charge** | committed straight line | dodge perpendicular | closes fast = tighter |
| **Teleport** | no travel tell — blinks in | pure reaction parry | high cost, the scary one |
| **Flying / aerial** | comes from above | changes dodge axis | different threat geometry |

`ECombatMovementType` already has `Direct / Dash / Teleport` — the scaffolding exists; the missing piece is
the per-archetype defense profile.

---

## 6. Staged implementation plan

Marked **[Code]** / **[Anim]** (Crown's authoring) / **[Setup]**.

### Stage A — Phase runner spine **[Code, Large]**
Replace the hardcoded approach→action delegate chain with a phase-list runner. Collapse the three
`Execute*Async` paths into one driver. `ApplyHit` (`:1970`) stays — it's already per-hit shaped. Phase list
authored on the three data assets. **Commit-first, own arc — this is the load-bearing change.**

### Stage B — Tier 1 modular slots + inertialization **[Code + Anim, Medium]**
Cast / Approach / Attack slots; pose-matched boundaries; inertial blending. Movement component drives
Approach translation as today. Bought assets drop in here unchanged.

### Stage C — Notify-driven hit resolution **[Code + Anim, Large]**
`AttackImpact` notifies on every Strike montage (Crown authoring, on critical path). Notify calls
`ResolveHit(index)`. Replace the lumped `ApplyDamageAfterDefense` loop with per-hit-frame resolution.

### Stage D — Per-hit defense state **[Code, Large]**
Rework `FDefenseState` / `SubmitDefenseInput` (`DefenseSystem.cpp:189`) to re-arm input per hit-frame.
Continuous window (turn-start → turn-end); `CalculateDefenseResult` evaluated per hit. Retain a
max-duration failsafe (the old 0.3s timer *was* the closer — see Risks).

### Stage E — Per-hit AI **[Code, Medium]**
Replace single `ScheduleDefenseDecision` (`DefenseSystem.cpp:119`) with N decisions, one per hit-frame,
each on the PlayRate-scaled clock.

### Stage F — Per-hit UI **[Code + UI, Medium]**
Per-hit defense prompt / feedback (currently TODO-stubbed).

### Stage G — Approach defense profiles **[Code + Anim, Medium]**
Wire the §5 table; defense window can attach to the Approach phase.

### Stage H — Tier 2 fused signatures **[Setup + Anim + Code, Medium]**
*After Tier 1 is proven.* Enable Motion Warping plugin (**[Setup]** — not yet installed). Root-motion
Attack-slot montages with warp targets + impact notifies. Runner treats them as one opaque Attack phase.

---

## 7. Risks / notes

- **Failsafe timer is mandatory.** Once close moves to hit-frame / turn-end, a missing montage or
  missing/late notify hangs the window open forever. Keep a max-duration backstop (mirrors
  `AsyncTimeoutHandle`).
- **Motion Warping is a Stage-H dependency only.** Tier 1 lands precisely via the movement component, so the
  plugin isn't day-one critical-path. Install it when Stage H starts.
- **Root motion is the Tier 2 gate.** Confirmed "most existing montages have it" — eyeball one root bone
  track before Stage H to be certain. In-place exceptions stay Tier 1.
- **AOE is a separate open path** (`ActionExecutor.cpp` `AoeWindowDuration`). Decide if AOE couples per-hit
  or stays single-resolution.
- **Resolve-off-data discipline must hold from Stage C.** If any notify handler does damage inline instead
  of calling `ResolveHit`, the online invariant breaks silently. Code-review gate.

---

## 8. Sizing

**LARGE — the largest single arc on the board.** It is a core-spine rework (Stage A) plus the full defense
rework (C–F) plus two presentation tiers (B, H). The mitigants: `ApplyHit` is already per-hit shaped, the
phase model *removes* three divergent code paths rather than adding one, and Tier 1 is demonstrable on its
own before Tier 2.

**Recommendation:** dedicated arc, committed-first. Ship **Stages A–G (Tier 1 + full defense)** as the
demonstrable shared-world-clean foundation; **Stage H (Tier 2)** as a promotion pass on top. Heavy PIE
verification of real-time feel is required throughout — not unit-testable.

---

## Changelog

- *(this revision)* — Restructured around the phase runner. Absorbs `RealTimeDefenseRework.md` (defense is
  now Stages C–F). Adds: phase model, two-tier presentation, purchased-asset annotation rules,
  continuous-window / per-hit-resolution split, notify-calls-`ResolveHit` online discipline, hit-time =
  notify-timestamp definition, approach defense profiles. Branch: `feature/weapon-stones`.
- *(prior)* — `RealTimeDefenseRework.md`, defense-only, authored from lifecycle + sizing surveys.
