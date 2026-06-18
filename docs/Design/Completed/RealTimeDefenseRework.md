# Real-Time Defense Rework — Turn-Start, All-Defenders-Watching, Per-Hit Active Defense

> **Status: DESIGN SPEC — re-anchored post-movement-unhook.** Supersedes the prior version (which
> was authored pre-movement-unhook against `StartApproach` / `CombatMovementComponent`, both now
> deleted, and predated the all-defenders-watching + AOE-per-hit + condition-hook refinements).
> Branch: `feature/realtime-defense`.
> All `file:line` anchors below are current **as of the June 2026 defense re-anchor survey** — re-verify
> before picking up each stage, as the surrounding code will shift.
>
> **2026-06-16 — Beam delivery REMOVED (Stage 6).** Any Beam-delivery references below are HISTORICAL.
> `ESpellDeliveryType::Beam`, the `ASkillProjectile` beam tick machinery, `OnBeamTick`, and the
> `BeamDuration`/`BeamTickInterval` fields are deleted. A beam is now authored as a **burst of projectiles**
> (`Projectile`, `Count>1`) and defended per-arrival by the Stage-6 cluster-6 burst path (even-split). The
> per-cast-entry defense work (difficulty/damage/burst) is live on `feature/realtime-defense`.
>
> **2026-06-16 — Hit → Impact RENAME (shipped symbols).** This spec was authored with the planned names
> `ECombatNotifyFamily::Hit`, the `OnHitFrame` delegate, and the `HitsLanded` field. Those shipped renamed
> (commit `ffafd3e0`, "unified impact concept" — a notify and a projectile arrival are both "impacts"):
> the live symbols are **`ECombatNotifyFamily::Impact`** (`CombatNotify.h:19`; an `EnumRedirect` maps
> authored "Hit" notifies to "Impact"), **`FOnImpactFrame OnImpactFrame(Executor, ImpactIndex)`**
> (`ActionExecutor.h:95`, broadcast `ActionExecutor.cpp:5134`), and **`FPendingDefenseContext::ImpactsLanded`**
> (`ActionStructs.h:327`). Any `Hit`/`OnHitFrame`/`HitsLanded` wording below is the historical spec name for
> the same concept.
>
> **2026-06-16 — Instant is now DEFENDABLE per-impact (Stage 6).** `Instant` was "unavoidable" (applied
> directly via `ResolveInstantSpell`, no window). It now converts to the per-impact path like AOE: the Cast
> notify is the impact moment, all three defenses (Block/Parry/Dodge) are available, gated by **Hard**
> difficulty (NOT excluded like AOE's Dodge). Requires per-asset authoring (telegraph lead + Hard tiers) for
> the window to be fair. Commit `9eb65756`. Any "Instant = unavoidable" wording below is HISTORICAL.

---

## 1. Status

This is the **complete reactive-defense spec** and staged build plan. **It is no longer pure design —
the build has since proceeded against this plan.** Stage 0 (`4986d450`), Stage 1 (window persist +
count-based close, `38922cb5`), Stage 2 (per-impact damage, `f2d241b7`/`0d4217e6` + the Hit→Impact
unification `ffafd3e0`), Stage 3 (per-impact timed defense + Reflex, `93d9f5c0`; attacker-speed window
duel, `e26d9256`), the per-impact **defense-difficulty** axis (`e4568e68`/`c436f7e9`/`9150020e`/`3690af4c`),
and the Stage-6 per-cast-entry spell path (difficulty/damage/burst, `0edb3d20`/`d6811365`/`43d64e58`/
`769018b7`/`076f6bf5`) have all **landed on `feature/realtime-defense`**. Per-hit AI (Stage 4) and per-hit
UI (Stage 5) remain. The forward-looking prose below is preserved as the design record — re-verify each
stage's *current* state against code before picking it up. It re-anchors the original stale doc against
the warp-based positioning architecture (replacing the old approach-movement layer) and captures Crown's
**full** locked intent.

The three pillars locked this design session:

1. **Windows open ON TARGET CONFIRMATION** — for exactly the targeted defenders (single → one, AOE →
   all targeted). *(Revised this session — supersedes the earlier "all defenders watch from turn-start"
   pillar, which assumed independent defenders; single-player has one player controlling the whole
   team. See §2 / §3a Control Model.)*
2. **Per-hit active defense** anchored on the impact, with a lead-in timing window and a PERFECT band.
3. **Condition-hook platform** — every defense outcome fires a condition that effects/items subscribe
   to. The rework builds the *platform* (timing + base outcomes + hooks); the *payoffs* (reflect,
   counters, buffs) are content built later.

Stage 0 (impact-frame notifies) is **DONE** (commit `4986d450`); Stages 1–3 and the Stage-6 per-cast-entry
spell defense have since landed (see the Status paragraph above). The remaining unbuilt work is per-hit AI
(Stage 4) and per-hit UI (Stage 5).

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
- **AOE = per-defender, per-hit, Block + Parry — NO Dodge (RESOLVED).** Every AOE-hit defender runs the
  per-hit defense flow, but the options are **Block and Parry only — you can't dodge an area attack**. This
  supersedes the old "AOE can only be blocked" *lumped* limitation: AOE went from **Block-only (lumped)** to
  **Block + Parry (per-impact)**, NOT to full options. Dodge-exclusion is enforced by authoring
  **`Dodge = Impossible`** on the AOE cast entries (the resolve-layer gate — a Dodge press's window is ~0
  and never matches), plus **`CanBeParried()` → AOE** so the HUD shows Block + Parry. This is the
  "Impossible replaces structural gates" pattern — per-attack tunable (a future *dodgeable shockwave* would
  author a real Dodge tier instead). *(Matches the implemented AOE per-impact conversion.)*
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

**RESOLVED (2026-06-15) — see §13.** Folded into the full multi-target model: solo = sequential
windows (one active defender at a time, pointer advances on resolve, all input to the one player); MP =
simultaneous windows (one per player, routed to the owning player). Crown's "one input per AOE beat"
lean survives as the solo per-impact case. Built at Stage 6 (multi-target); Stage 3 ships the
single-target foundation.

---

## 3. Timing Model

Defense is a **timing** mechanic anchored on the **IMPACT**, with two impact sources for the same
mechanic:

- **Melee impact** = the **Impact-family Combat Notify** (`ECombatNotifyFamily::Impact`, `CombatNotify.h:19`) firing on the montage (the impact frame).
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
| 5 | **Impact-frame notify** | *(baseline: was a log-only STUB.)* **Now WIRED (Stage 0+):** `ECombatNotifyFamily::Impact` (renamed from `Hit`, `ffafd3e0`) fires on the montage and broadcasts the per-impact trigger. | enum `ECombatNotifyFamily` `CombatNotify.h:17–23`; broadcast `OnImpactFrame` at `ActionExecutor.cpp:5134`. |
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
- **AOE single-resolution, block-only → per-defender per-hit, Block + Parry (no Dodge)** (anchor 7). Every
  AOE-hit defender runs the per-hit flow with Block/Parry; Dodge is excluded (can't dodge an area attack)
  via authored `Dodge = Impossible` (resolve gate) + `CanBeParried()` → AOE (HUD).
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
| **0. Hit-frame notifies** *(DONE — commit `4986d450`)* | Wire the existing **Hit-family Combat Notify stub** to be the melee impact trigger — replace the log-only return with a real per-hit entry point. `OnCombatNotifyReceived` now broadcasts `OnImpactFrame(Executor, ImpactIndex)` per impact (delegate `FOnImpactFrame`, `ActionExecutor.h:95`; broadcast `ActionExecutor.cpp:5134`); fired into the void until Stage 2 bound it. Crown **authors Hit notifies on every multi-hit montage** (see index-match contract in §9). **BLOCKED Stages 1–4 → now unblocked.** | **[Anim]** + small **[Code]** | Small |
| **1. Window lifecycle (FORCED FIRST — the foundation)** *(DONE — `38922cb5`; `ImpactsLanded`/`ExpectedImpacts` on `FPendingDefenseContext`, `ActionStructs.h:327/334`; 8s max-duration failsafe)* | **Make the window persist across hits and close on a COUNT-BASED universal trigger** — close when `HitsLanded == ExpectedImpacts` (per-defender counter reaching a per-defender expected count, **§8a**), NOT a melee-specific "last montage hit-frame." Drop the fixed `0.3s`/`AoeWindowDuration` timer as the closer (`DefenseSystem.cpp:73–86`); keep the `FDefenseState` + `FPendingDefenseContext` alive across **all** hits. **Stage 1 wires MELEE** (each Hit notify increments `HitsLanded`; `ExpectedImpacts = HitCount` set at the melee open); projectile/AOE/beam plug into the **same counter** at Stage 6 (**extend, not replace** — §8a). **This unblocks Stages 2–3** — until the window survives the hits, per-hit resolution/defense have nothing to read (§9). **The window still opens at ~execution-time (on target confirmation, §2) exactly as it does today — there is NO turn-start inversion** (dropped this session; §3a). The only change is *when it closes* (count-complete, not 0.3s) and that its state *persists* through the hits. **⚠ MAX-DURATION FAILSAFE IS NON-NEGOTIABLE — see §9.** | **[Code]** | Medium |
| **2. Per-hit resolution + base outcomes + condition firing** *(PARTIAL — per-impact `ApplyHit` + base outcomes shipped, `f2d241b7`/`0d4217e6`/`93d9f5c0`; the §5 typed condition-hook platform is NOT built — outcomes surface via the consolidated `FOnDefenseResolved`/`FOnDefensePerfect` delegates (carrying `DefenseType`/`bPerfect`/`ImpactIndex`), not the named `OnBlock`/`OnPerfect*`/`OnDefend` hooks)* | Drive `ApplyHit` (`:2342`) off **each Hit notify** via `OnHitFrame` (and **projectile impact** `:3322` for ranged), each gated by *that hit's* defense result — now readable because the window persists (Stage 1). Replace the lumped loop (`ApplyDamageAfterDefense` `:1702`). Index into `ResolvedDamageSplit[Index]` **with a bounds guard** (§9 index-match contract). Implement the **base outcomes** (§4: block reduce / dodge zero / parry negate+status). **Fire the conditions** (§5: `OnBlock`/`OnDodge`/`OnParry` + `OnPerfect*` + `OnDefend` + `OnHit`) at each resolution — the hooks fire even before any subscriber exists. | **[Code]** | Large |
| **3. Per-hit defense state + timing/PERFECT** *(DONE, single-target foundation — `93d9f5c0`/`e26d9256`; `PerfectThreshold`/`bPerfect`/`FOnDefensePerfect` (`DefenseSystem.h:434/99/207`), per-impact `EDefenseDifficulty` lead-in window, Reflex, player routing via `GetActiveDefenderForLocalPlayer`. Multi-target §3a-Q is Stage 6.)* | Rework `FDefenseState` / `SubmitDefenseInput` to **re-arm input per hit-frame** (clear `bInputReceived` each hit; `DefenseSystem.cpp:211–216`/`:229–231`). Implement the **lead-in window per defense type** and the **PERFECT band** (§3); `CalculateDefenseResult` evaluated **per hit**, producing the outcome band that selects which condition fires. Route input per the **Control Model** (§3a: player-team → player input, enemies → AI). **RESOLVE the multi-target input question (§3a-Q)** here — Crown's lean is one input per AOE beat. | **[Code]** | Large |
| **4. Per-hit AI** | Replace the single `ScheduleDefenseDecision` (`AIDecisionManager.cpp:306`) with **N scheduled decisions per defender, one per hit-frame**, each judged independently against that hit's lead-in window. Reaction delays scaled to the PlayRate-adjusted hit-frame times. (See §9 AI-degradation note — the single-decision path must survive Stages 1–3 gracefully until replaced here.) | **[Code]** | Medium |
| **5. Per-hit UI** | Per-hit defense prompt / feedback — surface each incoming hit, its lead-in window, and the outcome band (including PERFECT) (prompt widget currently TODO-stubbed). Must reflect the multi-target input decision (§3a-Q) — e.g. one shared AOE-beat prompt vs per-character prompts. | **[Code]** + UI | Medium |
| **6. AOE/projectile/beam → counter + source-threading** *(PARTIAL — the spell per-cast-entry path shipped: per-delivery difficulty + per-cast-entry spell damage + burst even-split per-impact defense, `0edb3d20`/`d6811365`/`43d64e58`/`769018b7`/`076f6bf5`; AOE full dodge/parry/block and projectile attacker-source-threading NOT proven shipped)* | **Plug projectile/AOE/beam into the count-based close (§8a)** — each adds a one-line `HitsLanded` increment (`OnProjectileImpact` `:3334`, `SpawnAOEEffect` `:3102`, `OnBeamTick` `:3381`) and sets its own `ExpectedImpacts` at open (single projectile = 1; barrage = **`FSkillCastEntry::Count`**, authored + wired; beam = `BeamTickCount`; AOE = 1, `Count` not honored for AOE today) — see the spell formula in §8a. AOE: every AOE-hit defender runs the per-hit flow with **Block/Parry — NO Dodge** (block-only → Block+Parry per-impact, NOT full options; Dodge excluded via authored Dodge=Impossible + `CanBeParried()`→AOE). Projectile: thread the **attacker source** through to `OnProjectileImpact` (fix the NULL attacker `:3351`) so base parry-negate works and `OnPerfectParry` subscribers (e.g. reflect) have the source. **Inherent extra lifts (any close model):** (a) projectile/AOE windows currently open **at impact**, decoupled from cast — must move to **target-confirmation + persist until last impact**; (b) **multi-pulse AOE is unbuilt** — the AOE branch ignores `entry.Count` today (`:3281–3286`); honoring it is an optional Stage 6 add; (c) projectile/beam impacts can arrive **after `CurrentExecutionContext` tears down** (montage finished first) — keep the context alive until the last impact (a melee-Stage-1 non-issue, since the montage spans the hits). | **[Code]** | Medium–Large |

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

**Counter location — two fields on `FPendingDefenseContext`** (`ActionStructs.h:327`, the per-defender
record). Shipped as `ImpactsLanded` (the spec's planned `HitsLanded`, renamed in `ffafd3e0`):

```
int32 ImpactsLanded = 0;    // ActionStructs.h:327 — incremented by ANY impact source
int32 ExpectedImpacts = 0;  // ActionStructs.h:334 — set by the opener; melee = HitCount
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

### §8b. Damage model — physical vs spell totals, per-cast-entry spell damage (STAGE 6)

**Crown's decision (lands at Stage 6, with the spell-damage-on-impact convergence — NOT Stage 2).**
Stage 2 (melee per-impact damage) builds on the **current** `BaseDamage`/`ResolvedDamageSplit` model,
which this decision **confirms is the correct melee model**. The rename + physical/spell split +
per-cast-entry spell damage are **one unit at Stage 6**, when spells need damage-on-impact — not
piecemeal.

**The model:**

- **`BaseDamage` → `RawDamage`** (the total). A clearer name for "the action's total damage before
  defense / before per-impact splitting."
- **Separate PHYSICAL vs SPELL damage totals.** An action can carry **both** — e.g. *Force Slash* =
  **physical** on the swing **+** **spell** on the projectile. Each is its own total, split and
  delivered by its own mechanism.
- **Physical / melee damage:** a total, split across `HitCount` via **`ResolvedDamageSplit`** — the
  existing per-impact table, **KEPT** (this decision confirms it correct for melee). Each melee impact
  deals its `ResolvedDamageSplit[ImpactIndex]` share (Stage 2).
- **Spell damage:** a total, where **each cast entry (`FSkillCastEntry`) authors its own DAMAGE
  PORTION** of the spell total — the entry says how much of the spell's total it deals. When that
  entry's projectile **arrives**, it deals **that entry's authored portion**. Within a burst
  (`Count > 1`), the entry's portion splits across its `Count` deliveries (equally by default, or per
  the entry).
- **This answers "how does a cast know its impact damage" — it is AUTHORED on the cast entry**, the
  same place `Count`/`BurstInterval`/delivery type already live (`SkillCastEntry.h`). It supersedes
  the open framing in §8a (where spell `ExpectedImpacts` summed entry impacts but the per-entry
  *damage* was unstated): the **count** comes from `Count`, the **damage** comes from the entry's
  authored portion.

**Why Stage 6, not Stage 2:** spell damage today lands at the **cast-time lumped window**, not at
impact (the collision trace, §7-projectile / Stage 6 row). Splitting `RawDamage` into physical/spell
totals and routing spell damage to per-cast-entry impact portions is part of moving spell damage
**onto the arrival** — the Stage 6 convergence. Doing it before then would churn the damage pipeline
with no consumer. Stage 2 leaves `BaseDamage`/`ResolvedDamageSplit` as-is for melee; Stage 6 renames
and splits.

### §8c. Broken Darkness per-impact absorption (PLANNED — after Stage 3)

Broken Darkness (BD) is a **defense-outcome consumer**: a BD-transformed defender that parries/blocks
**absorbs ENERGY** from the attack. Today it absorbs once per action at window-close
(`OnDefenseResolved`). Per-impact apply (Stage 2) + per-impact defense input (Stage 3) let it absorb
**per landed parry/block**.

- **Absorption is based on the attack's ENERGY COST, not damage.** It stays on the existing
  `CalculateAbsorptionEnergy(DefenseType, AttackEnergyCost)` — energy in = energy-cost × the defense
  rate (`ParryAbsorptionRate` 1.0 / `BlockAbsorptionRate` 0.5). *(This corrects the earlier
  "use the damage stash for BD consistency" framing — BD never followed the damage result; it follows
  energy cost. The Stage 2 light damage stash is unrelated to and sufficient for BD.)*
- **The change — split the attack's ENERGY COST across the impacts, PROPORTIONAL TO THE DAMAGE
  SPLIT.** Energy follows damage: an impact carrying X% of the damage (`ResolvedDamageSplit[Index]`)
  carries X% of the energy cost. When BD parries/blocks **that** impact, it absorbs *that share ×
  rate*. So a bigger hit feeds more energy, and the **same `ResolvedDamageSplit` proportions drive
  both per-impact damage AND per-impact energy absorption**. A multi-hit attack feeds BD energy **per
  landed parry/block** instead of one lump. *(Resolved: proportional, not even ÷ HitCount.)*
- **Enabled by:** Stage 2 (per-impact apply) **+** Stage 3 (per-impact input — *which* impacts were
  parried vs blocked). Hooks into the per-impact path: on a parried/blocked impact for a BD defender,
  grant `(AttackEnergyCost × ResolvedDamageSplit[Index]%) × rate`.
- **BD = energy absorption, NOT damage reflection.** The stale reflection path is dropped; the Stage 2
  lightweight damage stash is sufficient (BD reads energy cost, not the reflected-damage field).
- **Sequencing:** **after Stage 3** — a BD-mechanic refinement, not core defense plumbing.

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

- **AOE defense options — RESOLVED: Block + Parry, NO Dodge.** AOE is parryable and blockable per-defender
  per-hit, but **not dodgeable** ("can't dodge an area attack"). Block-only (lumped) → Block + Parry
  (per-impact), NOT full options. Dodge-exclusion via authored `Dodge = Impossible` (resolve gate) +
  `CanBeParried()` → AOE (HUD) — the "Impossible replaces structural gates" pattern, per-attack tunable.
- **Projectile reflect — RESOLVED: an `OnPerfectParry` condition effect (content).** Base parry on a
  projectile negates it (projectile disappears); reflect-back is a perfect-parry subscriber, out of
  scope for the rework but the reason projectiles need source-threading.
- **Parry full-negate — RESOLVED: YES.** Parry fully negates damage + reduces status; balance comes
  from its **tightest lead-in window / highest miss-risk**, not from chip damage.
- **Perfect-timing payoffs — RESOLVED: a condition platform, not hardcoded.** The defense system fires
  `OnPerfect*` / `OnDefend` / etc.; payoffs are equipped effects/items. Defense behavior beyond the
  base is a loadout choice.

---

## 13. Multi-target defense — solo vs multiplayer (window cadence differs, mechanism identical)

An attack declares its ORDERED target sequence ([A, B, C] — who it hits, in order). How the defense
windows open depends on the MODE, because the difference is who controls the targets:

### Solo — sequential windows (one active defender at a time)
The player controls the WHOLE team — one person can't defend multiple characters simultaneously. So
windows open SEQUENTIALLY in the declared order:
- A's window opens → player defends A (or not) → A's impact RESOLVES (hit / miss / defended) →
  ADVANCE → B's window opens → defend B → resolves → advance → C's window opens → ...
- One active defender at a time. The pointer advances through the target sequence when each impact
  resolves. The player's defense input always applies to the CURRENT active target.
- Sequencing exists to give the lone player a fair reaction chance at each character (they can't do
  them all at once).

### Multiplayer — simultaneous windows (one per player)
Each character is controlled by a DIFFERENT player. So all windows open AT ONCE:
- A, B, C windows all open simultaneously → each routes to the player controlling that character →
  each player defends their OWN character in parallel.
- No sequencing — multiple humans each handle their own character at the same time.

### Same mechanism, different cadence
The per-impact windows, lead-in timing, perfect-timing, base outcomes, conditions — ALL IDENTICAL
between modes. Only two things differ:
- WINDOW CADENCE: solo = sequential (advance on resolve); MP = simultaneous (all open at once).
- INPUT ROUTING: solo = all to the one player (whoever's the active defender); MP = each window to
  its owning player.

### Implementation implications
- The attack needs a DECLARED ORDERED target sequence (extend the existing target list with order).
- A pointer/index tracks the active defender (solo); MP opens all and maps each to its player.
- Advance-on-resolve: the pointer moves when an impact resolves (hit/miss/defended) — solo only.
- The defender lookup evolves: solo = "the current-index target"; MP = "the target this player
  controls." The Stage 3 single-target lookup (GetActiveDefenderForLocalPlayer) is the seed.

## Sequencing
- Stage 3 (now): single-target per-impact (one character, multiple hits = combo). One defender, no
  switching — the foundation.
- Stage 6 (later): multi-target — solo sequential (declared sequence + advance-on-resolve) and MP
  simultaneous (all windows, per-player routing), plus projectile-arrival timing. Built on the
  single-target foundation.

---

## Changelog

- **2026-06-16** — **`Instant` delivery now DEFENDABLE per-impact (all three defenses, Hard).** `9eb65756`,
  `feature/realtime-defense`. Instant converts to the per-impact path like AOE — Cast notify = impact moment
  (no travel), the count-based window (opened early at action start) is the reaction buffer, the cast
  animation + telegraph are the cue, difficulty sizes the match window. **Routing:** added to `bRequiresAsync`
  (`CombatOrchestrator.cpp`) so pure-Instant spells stop falling to the rejected sync path. **Gate:** merged
  into the AOE cluster-6 conversion branch (`AOE || Instant`, byte-identical — count-based re-open,
  `ExpectedImpacts=1`, full damage, the double-window fix). **Resolve:** `ResolveInstantSpell` swapped its
  direct `ApplyDamage` for the per-impact resolve (`ResolveImpactDefense` → `ApplyOneImpact` →
  `CloseDefenseWindow`; VFX + no-`DefenseSystem` fallback kept; `CastEntryIndex` threaded). **Defenses:** all
  three via four `SpellData` edits — `CanBeBlocked()`→true, `CanBeParried`/`CanBeDodgedByTiming` += Instant,
  removed the `GetAvailableDefenses` Instant early-return. Unlike AOE (Dodge excluded via `Dodge = Impossible`),
  Instant **allows Dodge** — gated only by **Hard** (data-authored, not code-excluded). Converting subsumes the
  prior single-entry-Instant double-apply. Updated delivery-defense matrix (single-entry, per-impact):

  | Delivery | Per-impact defense | Notes |
  | --- | --- | --- |
  | Projectile (single / burst) | Block + Parry + Dodge | burst even-split per arrival |
  | AOE | Block + Parry (NO Dodge) | Dodge excluded via authored `Dodge = Impossible` |
  | Instant | Block + Parry + Dodge | **Hard** difficulty; Cast-notify = impact |

  Beam / Homing removed (earlier). Single-entry deliveries (Projectile/AOE/Instant) are all now converted to
  per-impact; **multi-entry / `HitCount>1` spells and abilities stay on the lumped path** — the live applier
  + 8s failsafe, NOT retired (by design; the lumped path is also the failsafe for converted deliveries). |
  feature/realtime-defense
- **2026-06-16** — **Status migration: §8 stage table marked against shipped code.** Stage 1 (window persist + count-based close + failsafe) and Stage 3 (per-impact state + timing/PERFECT + Reflex, single-target) marked **DONE** with proof symbols; Stage 2 (per-impact resolution + base outcomes) and Stage 6 (spell per-cast-entry difficulty/damage/burst) marked **PARTIAL** — Stage 2's §5 typed condition-hook platform ships only as the consolidated `FOnDefenseResolved`/`FOnDefensePerfect` delegates, and Stage 6's AOE-full-options + projectile source-threading are unproven; Stages 4 (per-hit AI — `ScheduleDefenseDecision` still one-per-window) and 5 (per-hit UI — `DefensePromptWidget` stub) left unbuilt. | feature/realtime-defense
- **2026-06-16** — **Doc-sync: spec is no longer pre-implementation; Hit→Impact rename reconciled.** §1
  Status corrected — it previously claimed "no source has been changed," but Stages 0–3 and the Stage-6
  per-cast-entry spell defense (difficulty/damage/burst) have **landed** on `feature/realtime-defense`
  (commits `4986d450`/`38922cb5`/`f2d241b7`/`0d4217e6`/`93d9f5c0`/`e26d9256`/`e4568e68`/`c436f7e9`/
  `9150020e`/`3690af4c`/`0edb3d20`/`d6811365`/`43d64e58`/`769018b7`/`076f6bf5`); per-hit AI (Stage 4) and
  UI (Stage 5) remain. Added a top banner reconciling the spec's planned names to the **shipped** symbols
  (`ffafd3e0`): `ECombatNotifyFamily::Impact` (`CombatNotify.h:19`), `OnImpactFrame` (`ActionExecutor.h:95`,
  broadcast `:5134`), `FPendingDefenseContext::ImpactsLanded` (`ActionStructs.h:327`). Fixed the
  load-bearing inline references (§3 definition, §6 anchor 5 — the notify is wired, not a stub —, §8 Stage 0
  delegate, §8a counter-field block) and led each with its symbol so future drift survives. Historical-record
  prose elsewhere keeps the original `Hit`/`OnHitFrame`/`HitsLanded` names per the banner. | feature/realtime-defense
- **2026-06-16** — **`Homing` delivery type REMOVED** (`feature/realtime-defense`; follows the
  earlier `Beam` removal). Tracking is meaningless without a spatial dodge — Crown-confirmed a homing
  shot was just a projectile with a curvy path, i.e. dead weight. Deleted `ASkillProjectile::TickHoming`
  + the Tick `Homing` case + the `OnHitBoxOverlap` Homing branch (function/binding kept as a Projectile
  no-op), the `HomingStrength` field across `FSkillCastEntry`/`USpellData`/`ASkillProjectile` + its
  migration, and every `Homing` clause in the defense helpers, EditConditions, the cluster-4/6 conversion
  gate, the async-decision, and the dispatch switches. Mid-enum removal (value 1) so a CoreRedirect
  (`ESpellDeliveryType` `Homing`→`Projectile`) was added; enum is now `Projectile=0 / AOE=1 / Instant=2`.
  The §8a `ExpectedImpacts` formula rows now collapse to **Projectile : `entry.Count`** (the prior
  Projectile/Homing pairing and the already-removed Beam row are both gone); shared projectile + count-based
  per-impact defense plumbing is untouched. The §8a/§8b prose retains its original delivery-type wording as
  a historical design record.
- **2026-06-15** — **Multi-target defense model RESOLVED (§13): solo sequential vs MP simultaneous.**
  The per-defender AOE question from §3a-Q is now resolved into a full model. Solo = one player controls
  the whole team → windows open SEQUENTIALLY in a declared ordered target sequence, a pointer advances
  on each impact's resolve, input always applies to the current active target. MP = each character is a
  different player → all windows open SIMULTANEOUSLY, each routed to its owning player. Mechanism is
  identical between modes; only **window cadence** (sequential vs simultaneous) and **input routing**
  (one player vs per-player) differ. Sequencing confirmed: Stage 3 = single-target foundation;
  multi-target (declared sequence + advance-on-resolve, MP per-player routing, projectile arrival
  timing) lands at Stage 6. Marked §3a-Q resolved with a pointer to §13.
- **2026-06-14** — **Broken Darkness per-impact absorption recorded (§8c) — after Stage 3.** BD
  absorbs **ENERGY (attack ENERGY COST × rate), not damage** — stays on `CalculateAbsorptionEnergy`
  (`ParryAbsorptionRate` 1.0 / `BlockAbsorptionRate` 0.5). Change: **split the energy cost across the
  impacts** so each parried/blocked impact feeds BD its share (per-landed instead of one lump). Split
  method **CONFIRMED proportional to the damage split** (energy follows `ResolvedDamageSplit` — the
  same proportions drive per-impact damage and per-impact absorption; not even ÷ HitCount): grant
  `(AttackEnergyCost × ResolvedDamageSplit[Index]%) × rate`. Enabled by Stage 2 (per-impact apply) +
  Stage 3 (which impacts were parried/blocked); sequenced **after Stage 3**.
  Corrects the earlier "use the damage stash for BD consistency" framing — BD follows energy cost, not
  the damage result; the Stage 2 light stash is unrelated/sufficient. Reflection path dropped (BD =
  energy absorption).
- **2026-06-14** — **Damage-model decision recorded (§8b) — Stage 6.** `BaseDamage` → `RawDamage`
  (the total); **separate PHYSICAL vs SPELL totals** (one action can carry both, e.g. Force Slash =
  physical swing + spell projectile). **Physical/melee** = total split across `HitCount` via
  `ResolvedDamageSplit` (existing table **KEPT** — confirmed the correct melee model). **Spell** =
  total where **each `FSkillCastEntry` authors its own damage PORTION**; an entry's projectile deals
  that portion on arrival, splitting across its `Count` burst. Answers "how does a cast know its impact
  damage" — **authored on the cast entry** (alongside `Count`/`BurstInterval`). Sequenced as **one
  unit at Stage 6** (the spell-damage-on-impact convergence), NOT piecemeal: **Stage 2 builds on the
  current `BaseDamage`/`ResolvedDamageSplit`**, which this decision confirms correct for melee.
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
  design questions (§12): AOE defense = Block + Parry, no Dodge; projectile reflect = `OnPerfectParry` effect; parry
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
