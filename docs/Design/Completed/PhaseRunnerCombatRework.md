# Fused-Montage Animation Model — Notify-Driven Execution + VFX

> **Status: DESIGN — UNPROVEN. Gated on a spike.** The core model (one fused montage per
> skill, warped onto the target, driven by notifies) has NOT been validated in-engine. Do
> not build the shared structs or collapse the animation paths until the spike confirms the
> fused look + warp + notify-VFX handshake works on a real skill. See §6.
> Companion to `PhaseRunnerCombatRework.md` — this is the animation/VFX side of the same arc.

---

## 0. The two decisions (read this first — they are easy to misread)

These are distinct and **non-conflicting**. A future session must not collapse them:

1. **Data types stay SEPARATE.** `USpellData` / `UAbilityData` / `UWeaponAttackData` are NOT
   merged. A spell and an attack remain different things with different stats, element, cost.
   *(This is the pre-existing decision, unchanged.)*
2. **The ANIMATION pipeline unifies.** The three execution/animation paths
   (`PlaySpellAnimation` / `PlayAbilityAnimation` / `PlayAttackAnimation`, and the three
   `Execute*Async` chains behind them) collapse into ONE fused-montage execution path. The
   asset type just selects which data that one path reads.

One line: **separate data, shared execution.** The runner is the shared execution path;
spell/ability/attack are data tags it reads. This is the phase-runner consolidation seen
from the animation side — the survey already confirmed the three paths run identical order,
different data, so the separateness is historical, not structural.

**What "shared" means here:** shared *behavioral scaffolding* (the VFX timeline struct, the
phase list, the notify model) embedded in each of the three asset types as the same field —
NOT a merged asset type and NOT a shared base class they inherit from. Same struct, three
independent homes.

---

## 1. The fused-montage model

Each skill is **one continuous montage** that does its own travel, cast, strike, and recovery
— rather than separate approach / action / return montages stitched at runtime (today's
seam). The runner plays the montage, warps it onto the target, and reacts to notifies.

This is the Tier 2 model from `PhaseRunnerCombatRework.md`, now proposed as the **default for
all skill types**, not just hero signatures — pending spike validation.

### Why it collapses the three paths
Once spell/ability/attack are all "one fused montage the runner plays and warps," the runner
does the identical three things for each:
- Play the montage (warped onto the target enemy)
- Listen for notifies (hits, VFX, defense beats)
- Wait for done → finalize

Type-specific behavior (damage formula, cost, element, which montage) is **data**, not a code
path. Hence one execution path, three data types.

---

## 2. Targeting & warp

In this combat the player selects an enemy; **that enemy is both the target and the
defender.** So the warp target is never ambiguous — it is the selected opponent's position.

- The montage's root motion is authored for *some* distance.
- **Motion Warping** bends that root trajectory onto the selected enemy, at any distance/slot.
- Teleport approaches are just part of the anim — a notify (or root-motion snap) relocates the
  character; warp/teleport needs no separate "approach phase" doing teleport logic.

**Dependency:** Motion Warping plugin (NOT yet installed) + root motion on the montages. The
root-motion status of existing montages is still unconfirmed — the spike answers this by
trying it (§6).

---

## 3. Notify-driven everything (the core principle)

**The montage is a timeline of tagged moments; the runner is a generic dispatcher that reacts
to each tag.** Three kinds of tag, one mechanism:

- `AttackImpact` → resolve a hit (damage/defense) — see `PhaseRunnerCombatRework.md` §5
- `VFX_*` → spawn a VFX (see §4)
- defense-open / phase-boundary → runner beats

This is what lets a **generic** runner drive **bespoke** skills: uniqueness lives in the
montage + its notifies + its data, not in code. A skill = one fused montage + a row of
notifies + a data asset. Hundreds of distinct skills, one code path.

**Anim owns WHEN. Asset owns WHAT.** A notify never contains the effect inline — it names a
slot, and the runner resolves the actual effect/damage from the asset at that moment. (Same
discipline as `ResolveHit` — the notify *calls/looks up*, it doesn't *contain*.)

---

## 4. VFX system

### The handshake
A `VFX_<Tag>` notify fires → the runner looks up the entry tagged `<Tag>` in the skill asset's
VFX array → spawns that effect, attached per the entry's mode, tinted by the skill's element.
The montage author and the asset author work independently as long as they agree on tags.
**Match by tag, never by index.**

This generalizes the existing `SpellRelease` notify (which already defers VFX to spawn-time
and reads the spell data) into a full timeline of cues.

### The shared struct (build AFTER the spike)
A shared struct, embedded in spell / ability / attack as the same `TArray` field (NOT a shared
base — see §0):

```cpp
USTRUCT(BlueprintType)
struct FSkillVFXEntry
{
    GENERATED_BODY()

    /** Matches the montage notify name — "Aura", "Release", "Trail", etc.
     *  FUNCTIONAL key. The runner scans for this. Match by tag, never by index. */
    UPROPERTY(EditAnywhere) FName VFXTag;

    /** Designer-facing note — "blue projectile burst that flies to target".
     *  NEVER read by code; keeps the array readable in the Details panel. */
    UPROPERTY(EditAnywhere) FString Label;

    /** The actual effect (Lord Enot Niagara). Soft ref. */
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UNiagaraSystem> VFXAsset;

    /** Where it spawns/attaches. */
    UPROPERTY(EditAnywhere) EVFXAttachMode AttachMode;   // Caster / Target / ImpactLocation

    /** Tint via ElementColors/HybridSpellColors, or use as-authored. */
    UPROPERTY(EditAnywhere) bool bElementTinted = true;
};

// embedded identically in USpellData, UAbilityData, UWeaponAttackData:
UPROPERTY(EditAnywhere, Category="VFX") TArray<FSkillVFXEntry> VFXTimeline;
```

### Locked field decisions (Crown)
1. **Attach mode per entry** — YES. Aura→caster, impact burst→target, trail→moving thing.
2. **Lifetime decided by notify TYPE, not the entry** — point notify = one-shot
   (spawn-and-forget); notify-STATE (begin/end) = persistent (spawn on begin, destroy on end).
   Auras/channels/glows use notify-states; flashes/bursts use point notifies. Keeps the entry
   simple.
3. **Per-entry element-tint flag** — YES. Fireball tints (Fire→orange); generic dust kick
   stays as-authored.
4. **All three asset types** — spell, ability, AND attack each get the field. Same struct,
   replicated across the three (not merged, not inherited).
5. **Designer label per entry** — a free-text `Label` field, never read by code, so a
   multi-effect timeline stays readable in the Details panel. (Tag is the functional key;
   Label is the human note for why the VFX is there.)

### Reference chain (aura example)
```
Montage:   ──[VFX_Aura state begins]────────[VFX_Aura state ends]──►
                    │                              │
Runner:        spawn the aura                  kill the aura
                    │
Asset:         VFXTimeline entry tagged "Aura" → AuraVFX (soft ref) + Element
                    │
Tint:          ElementColors[Element] applied to the spawned Niagara system
```
No Niagara path is ever hardcoded in code. Swap the asset's entry → different VFX, no code,
no montage change. Element tints it.

### Tinting source
Uses the already-scoped `ElementColors` / `HybridSpellColors`. The `ItemVFXTable` work in the
backlog is the same shape of lookup — the notify listener would use this machinery.

---

## 5. Cast phase note

Spells already have a cast animation (windup before release). Under this model the cast is part
of the single fused montage, not a separate montage. **Abilities get an optional cast span too**
(e.g. hand-signs) — same single-montage treatment, default-empty so nothing changes until a
skill authors one. Spell montages are NOT re-cut into separate cast/strike montages — they
stay whole (a grab-spell that flies in and seizes the target is one continuous anim, warped;
splitting it would re-introduce the seam this whole model exists to kill).

---

## 6. THE SPIKE — validate before building any of this

**Nothing in §1–4 gets built until a throwaway spike confirms the model looks good in-engine.**
This is an aesthetic + feasibility question that cannot be answered from a design doc.

### The question the spike answers
"If I play an ability/spell as one fused montage warped onto the target enemy, with a notify
spawning an asset-assigned element-tinted VFX partway through — does it look good, and do my
montages even have root motion to make warp work?"

### Spike rules
- Throwaway. Own branch (`spike/fused-montage-warp`). Deleted after — never merged.
- Quick and dirty. No debug tools, no clean architecture, no turn/defense integration.
  Hardcode freely. Crude single VFX field, not the real `FSkillVFXEntry` array.
- Goal: see it in PIE on a debug key, fire it at enemies at varying distances.

### Spike steps
1. Enable the Motion Warping plugin (the one real setup step).
2. Find/pick one montage WITH movement (report root-motion status of candidates — this also
   answers the long-deferred root-motion check).
3. Play it fused, with a warp target on the selected enemy.
4. Fire one notify partway through → spawn a VFX, element-tinted (the real handshake, crude
   field).
5. Trigger from a debug key; fire at near + far enemies. Look.

### Three outcomes (all useful)
- **Root motion + warp looks good** → fused model viable; the §1 path collapse + §4 array get
  built. Stage A simplifies toward fused-default.
- **Root motion + warp distorts at distance** → fused works for signatures only; keep modular
  approach for travel (two-tier model from the companion doc holds).
- **Montages are in-place (no root motion)** → fused impossible on existing anims without
  re-authoring. Critical to learn now, before building a runner around the assumption.

---

## 7. Relationship to PhaseRunnerCombatRework.md

- That doc: the phase-runner spine (Stage A) + per-hit defense + two-tier presentation.
- This doc: the animation/VFX execution detail — what "play a phase" actually does once skills
  are fused montages.
- **Stage A (the runner spine) needs NO animation work and ships first** — it reuses existing
  montages sequenced through a list. This fused-montage model is the *later* animation stage
  that Stage A's runner makes possible. Structure first, anims second (Crown's ordering).
- The "one animation path" of this doc IS the phase runner seen from the animation side.

---

## 7.5 UNIFIED SKILL MODEL — locked decisions (from the disposition survey)

Decisions made walking the field-disposition table (47 fields across the 5 classes). These
supersede earlier sketches where they conflict. Remaining open forks listed at the end.

### D1 — Hits → `HitCount` + `DamageSplit` (author by exception)
`HitCount` (int) STAYS. Add a **`DamageSplit`** array of `{ HitNumber, Percent }` — **exceptions
only**. Resolution: at action start (BEFORE any hit fires), resolve the full per-hit table —
authored hits take their %, the REMAINING hits split the leftover evenly. Hit notifies then just
**index the precomputed table** (no per-hit math, no waiting).
- Example: 4 hits, `DamageSplit=[{3, 70}]` → table `[10,10,70,10]`.
- Empty `DamageSplit` → even split = **current behavior** (migration-safe).
- Over-100% authored → warn (debug checker).
- Supersedes the §8 flat per-hit-weight array. Attack's dead `FirstHitPercent`/`SecondHitPercent`
  (never applied at runtime — even split always ran) → migrate to `DamageSplit` entries if
  honoring authored intent, else leave empty.

### D2 — `SkillMontage` on the base class (Option A)
`CastAnimation` / `ExecutionMontage` / `AttackMontage` → ONE `SkillMontage` field on
`UCastableSkillDataBase` (all three inherit). §0's "no shared base" applies to MERGING THE TYPES
and complex behavior — NOT plain shared fields like a montage ref (the base already holds
`Tier`/`BaseDamage`/`DeliveryType`). Migration: `PostLoad` copies each old field → `SkillMontage`
(wrap-don't-rip: keep old fields deprecated, remove later once verified). Three animation readers
collapse to one `Skill->SkillMontage` read.

### D3 — `ExecutionType` becomes DESCRIPTIVE metadata
The animation (root motion + warp) handles movement now, so `ExecutionType` (melee/ranged) STOPS
controlling approach. Keep it as a **descriptive tag** for UI / AI filtering / categorization.
Unhook from `RequiresApproach()`. Now generic → moves to the base (an attack is "melee", a
projectile spell "ranged" — uniform tagging).

### D4 — Approach system REMOVED (animation owns movement)
`ApproachData` / `ExecutionRange` / `RequiresApproach()` are superseded by the fused montage
(warp-in) + return montage. Approach = **cast + return** now. DEAD-REMOVE — but migration-gated:
delete only AFTER the fused runner handles movement (wrap-don't-rip).

### D5 — VFX roles + Trail split
VFX entries carry a **Role** classifying what kind they are:
- **Muzzle** — launch flash (caster/socket)
- **Impact** — hit/landing burst (target). **Doubles as the melee hit visual** — unifies melee +
  ranged hit feedback (projectile landing AND `Hit` notify both fire Impact-role VFX on target).
- **Cosmetic** — auras, charge glows, ambient visuals; no mechanical tie.
- **Trail** is NOT a VFX-array role — it lives on the **Cast entry / projectile** (it must follow
  the moving projectile, which a location-spawned array entry can't).

Role CLASSIFIES; **index DISTINGUISHES** — many entries per role, notifies pick which (`VFX0` =
normal hit spark, `VFX1` = finisher burst; hit 3's frame gets a `VFX1` notify). Damage and visual
stay independent notifies (`Hit` does damage, a `VFX` notify at the same frame does the visual).
Replaces all old loose VFX fields: old `MuzzleVFX`→Muzzle, `ImpactVFX`→Impact, `SpellVFX`(travel)
→Trail on Cast entry; dead ability `NormalVFX`/`InfusedVFX`/`ProjectileVFX` removed.

### D6 — Size is per-Cast-entry, shaped by delivery type
Skill-wide `BaseSize`/`HitboxRatio` → **REMOVED**. Size splits two ways:
- **Hitbox / impact radius (mechanical)** → on the **Cast entry**; its MEANING follows the
  entry's delivery type — Projectile = moving collision (travels with the actor), AOE = static
  ground radius (instant). Both already built in `ASpellProjectile`. So fireball-then-pillar =
  two Cast entries: `{Projectile, hitbox}` (moving) + `{AOE, radius}` (ground). Delivery type
  encodes moving-vs-static; the entry carries the numbers.
- **Visual scale (cosmetic)** → travels with its visual: trail scale on the Cast entry,
  muzzle/impact scale on the VFX entries. Decouples spectacle from hitbox (big-looking, small
  hitbox is now possible).

### D7 — `BaseAnimSpeed` generalizes to the base
Attack-only designer play-rate scalar → **base** (`UCastableSkillDataBase`), a per-skill montage
play-rate scalar; stat scaling layers on top. Uniform with `SkillMontage`. A slow heavy spell or
a quick jab ability now get the same authoring lever.

### D8 — `TurnCost` → base, NET-NEW feature (implement)
`TurnCost` is currently authored but UNREAD (never implemented). Promote to the **base** (all
three) AND **build the multi-turn-cost mechanic** — wire it into the turn system. This is net-new
work, not cleanup.

### D9 — Tier-gap DAMAGE scaling (separate from wear) — UNIFORM across all three

**The damage mechanic stands alone — wear is a SEPARATE, pre-existing system; do not conflate.**

An action's **own tier** is compared to the tier of the **channel it's used through**, scaling its
damage. Applies UNIFORMLY to all three (attacks swap too — `FWeaponLoadoutEntry::OverrideAttack`
proves attacks are not intrinsic to their weapon):

| Action  | Action tier (its OWN)                                           | Channel (gaps against)            |
| ------- | --------------------------------------------------------------- | --------------------------------- |
| Spell   | `SpellData->Tier`                                               | the **crystal** it's cast through |
| Ability | `AbilityData->Tier`                                             | the **weapon**                    |
| Attack  | the attack's own `Tier` (exists, currently UNREAD — wire it up) | the **weapon**                    |

Weapon TYPE must already match (hard `ValidateAbilities` gate — a sword ability only goes on a
sword); the gap is about TIER, not type. A high-tier skill on a low-tier (but correct-type) weapon
is weakened; a low-tier skill on a high-tier weapon is boosted.

Symmetric, both directions:
- Action tier BELOW channel (weak action, strong channel) → **boosted** (×>1)
- Action tier ABOVE channel (strong action, weak channel) → **weakened** (×<1)
- Equal → **neutral** (×1.0)

**Balance guardrail:** the boost **nudges toward** the channel's tier but does NOT equalize — a true
high-tier action still out-damages a boosted low-tier one (higher base). Weak-through-strong = good
*utility*; high-tier = raw *power*. Both viable; neither dominates.

**Replaces** the hard `RequiredWeaponType`→penalty framing entirely — tier-gap is the "wrong fit =
weaker" mechanic. (Weapon-TYPE matching stays a hard gate; tier is the soft scaling axis.)

**Convention ruling (fixes a pre-existing contradiction):** the code disagreed — wear path used
`Weapon->Tier` as the ability's action tier (`ActionExecutor.cpp:4338-4349`, "action tier inherits
from weapon"), BD-break used `AbilityData->Tier`. **For the DAMAGE mechanic, action tier = the
skill's OWN tier, always.** WEAR keeps its existing convention (untouched — separate system). We are
NOT changing wear; we are defining the damage mechanic's convention cleanly.

**Reuse:** mirrors the tier-keyed table pattern (`GetSubstatBudget`/`GetBaseBreakChance` — inline
switch in a `*Constants.h`). The multiplier is a tier-gap-keyed lookup; applies multiplicatively at
damage assembly (the `× (1 − RequirementPenalty)` precedent — `AbilityData.cpp:20`, attack `:1017`,
spell `:719`). Sits beside `GetTierGap` (`TierHelpers`, only 3 callers, all in `BreakCalculator`).

**Prerequisites (from the gap survey):**
1. **Channel-tier resolution per cast.** Ability/attack: `Weapon->Tier` accessible at cast time
   (read 4× already). Spell: crystal tier resolved ONLY on the infused path today — uninfused casts
   never look up their crystal. Build a per-cast channel-tier resolver (machinery exists:
   `FindAttachedItemByHolder` + `Crystal.Id.Tier`; just not invoked uninfused). De-dups the 4×
   copied channel-resolution block in `ApplyCommitCosts` (:4333/:4390/:4469/:4536).
2. **AI preview parity** — AI previews damage via `UDamageCalculator` (`AIDecisionManager.cpp:714/
   :769`); the multiplier must be visible to the AI's damage input.
3. **Attack tier** — `WeaponAttackData`'s `Tier` exists but is unread; wire it up.

**NOTE on wear:** wear is its OWN system (action vs CRYSTAL, commit-time, luck-skippable). NOT part
of this. Both use tier comparisons but are independent.

**ANIMATION PHASE TODO:** rituals currently fire using the skill's normal animation. A distinct
ritual visual (an arming gesture at cast, and/or a bespoke eruption when it fires at turn-start) is
DEFERRED to the animation phase (Phase 2) — built with the runner + spike notify system as
reference, not ad-hoc now. The mechanic works without it; this is feel/polish.

**NEW design, not unification cleanup. Build first.**

### D10 — Infused weapon glow → on the WEAPON (re-home)
Ability `InfusedVFX` is dead → **removed**. The CONCEPT (a glowing weapon when infused) re-homes
as a **new VFX on the WEAPON** (a weapon property: "when infused, show this glow"), not the
skill. Different field, different owner.

### D11 — Dead-field cleanup (confirmed dispositions)
- **REMOVE:** `RequiredEvolutionCrystal`? → **KEEP** (spell-only, planned validation slot);
  ability `NormalVFX` → remove (concept = VFX roles); ability `InfusedVFX` → remove (re-homed to
  weapon, D10); ability `ProjectileVFX` → remove (replaced by Cast-entry VFX).
- **KEEP:** `Icon` (BP-read risk, keep); construct cluster `bIsConstruct`/`ConstructedWeapon`/
  `bSealsSpells` (type-specific, kept — constructs are a committed/door-open feature; sealing
  may extend to abilities later).

### Open forks (still to decide)
- **SFX array** — DEFERRED. Same pattern as VFX (sound + attach + index), would likely mirror
  VFX roles (Muzzle/Impact/Cosmetic) + a non-positional `TwoD` attach option. Not needed for the
  pitch demo; spec when audio work begins. The spike already has a throwaway `FSpikeSFXEntry`.
- **`bRequiresDualWeapon`** — RESOLVED by the type-hard lock. Since weapon TYPE matching stays a
  hard gate (only TIER is the soft tier-gap axis), `bRequiresDualWeapon` stays a hard check like its
  parent gate (LoadoutComponent.cpp:828). No soft-penalty path — a dual-only ability on a
  single-wield weapon is blocked, not penalized. The earlier "soft-only" framing was superseded.

---

## 8. Hit resolution — the `Hit` array with authored damage weights

**Validated direction (real-system feature; spike does FAKE hits only).**

### Rename
`Strike` → **`Hit`** everywhere. "Strike" implied the whole attack; `Hit` is one impact.
The notify the runner listens for is **`Hit`** (one universal tag, hardcoded in the reader —
that one `== "Hit"` check is the entire "standard"; a montage that names its impact anything
else won't resolve).

### The data change: `HitCount` int → `Hits` array
Today the skill assets carry a plain `HitCount` integer and multi-hit splits damage evenly
(`TotalDamage / HitCount`). Replace with an **array of hit entries**, each carrying an authored
**damage weight** (its portion of the attack's total damage):

```cpp
USTRUCT(BlueprintType)
struct FHit
{
    GENERATED_BODY()
    /** Portion of the attack's total damage this hit deals. Weights across the array
     *  should sum to 1.0 (validate/normalize — see below). */
    UPROPERTY(EditAnywhere) float Weight = 1.0f;
    // extensible later: bool bCanCrit; ESpellElement ElementOverride; etc.
};

// replaces HitCount on USpellData / UAbilityData / UWeaponAttackData:
UPROPERTY(EditAnywhere, Category="Hits") TArray<FHit> Hits;
```

Example — a ramping 3-hit combo:
```
Hits = [ {Weight: 0.2}, {Weight: 0.3}, {Weight: 0.5} ]   // 20% / 30% / 50%
```

### Notify → array mapping (the array IS the index)
The montage carries plain **`Hit`** notifies, one per impact frame, **in order**. The array
position *is* the index — no numbered notifies needed. The reader maps the **Nth `Hit` notify
to `Hits[N]`**:

- 1st `Hit` notify fires → apply `Hits[0].Weight × TotalDamage`
- 2nd → `Hits[1]`, 3rd → `Hits[2]`...

So "add a hit" = add an array entry + place another `Hit` notify. "Reshape the combo's feel" =
change the weights. This makes damage distribution **authorable** (ramp up, front-load, etc.)
instead of the current even split.

### Invariant + debug
- **Notify count must equal array count, in order.** A `HitsDebug` pair must verify this per
  skill (mismatch = misattributed damage). Rule on mismatch: extra notifies beyond the array →
  ignore + warn; fewer → the trailing weights never fire + warn.
- **Weight sum:** decide normalize-to-1.0 vs apply-as-authored (weights summing to 0.8 → 80%
  total, or rescale?). Flag for the build; lean: validate they sum to ~1.0, warn otherwise.

### Why authored per-montage, not a data field for timing
The hit *timing* lives on the montage (the `Hit` notify — anim owns *when*). The hit *content*
(weight, eventually crit/element) lives in the `Hits` array (asset owns *what/how much*). The
notify never carries the damage — it says "resolve the next hit now," the reader pulls the
weight from the array. Same anim=when / asset=what split as VFX (§3–4). The notify calls
`ResolveHit(index)` — it does not contain damage logic (the online-clean discipline from
`PhaseRunnerCombatRework.md` §5).

### Migration note
`HitCount`→`Hits` is a data-structure change to all three asset types — existing assets need
migration (`PostLoad`, not `PostInitProperties` — see project gotchas). Per the
separate-data/unified-animation decision (§0), `FHit`/`Hits` is the SAME struct embedded in
each of the three types, not a merged type.

---

## 9. Spike status — VALIDATED END-TO-END

The full fused-montage loop is proven in PIE on a real skill (`Anim_SAS_Combo1`):

- **Warp-in: VALIDATED.** Fused root-motion montage + Motion Warping onto the target enemy
  looks excellent at real combat distance (~1050+ units).
- **Root motion confirmed present** on the test ability — the gating question is closed for
  this rig: "annotate what we have," not "re-author."
- **Multi-hit notifies: VALIDATED.** Four `Strike` notifies fire in sequence at their impact
  frames; each resolves a (fake) hit at its own moment — the per-hit model working as designed.
- **Tag-matched VFX: VALIDATED.** Notify name → array lookup by tag → Niagara spawns on the
  target (attach=1). `Strike` entry fires the hit VFX correctly.
- **Return loop: VALIDATED.** Second (trimmed) montage warps back; loop completes cleanly.

### Critical finding that the REAL build must address
- **Notify type:** the engine's `OnPlayMontageNotifyBegin` (which `CombatAnimInstance` listens
  on) ONLY fires for **"Montage Notify"** (`UAnimNotify_PlayMontageNotify`) instances — NOT
  name-only notifies set to "Branching Point." The spike uses "Montage Notify." **The production
  build should use a custom `UCombatNotify : UAnimNotify` class** (see §10) carrying a tag field,
  rather than name-only montage notifies — scales to the tag model and can carry structured data.
- **Root-motion vs lerp/grid:** existing combat translation is hand-lerped `SetActorLocation`;
  the fused montage moves the real capsule via root motion, and nothing returns it to grid. The
  real build must reconcile which slot the character occupies after a root-motion approach.
- **Approach-end snap-back (open):** the approach montage resets the capsule to origin at its
  end (root motion not consumed into the capsule), so the return starts from the wrong spot.
  The intended flow is: approach → STAY at enemy → return from enemy. Spike-level fix: pin the
  actor at its end-of-approach location before playing return. Logged, not yet fixed.

## 11. Cast Array — per-effect delivery (DESIGN; spike wires the REAL delivery system)

**Major update:** the projectile/delivery system is ALREADY BUILT (`ASpellProjectile`,
`ESpellDeliveryType`, `SpawnSpellDelivery`, `SpawnBurst`, `SpellProjectileTestActor`). Cast is
NOT new scope — it's connecting notifies to existing delivery. The spike wires it directly.

### Delivery is PER-EFFECT, not per-skill
The current `ESpellDeliveryType` lives on `USpellData` (per-spell — assumes one spell = one
delivery). **That's too coarse.** A single skill can have multiple, mixed deliveries:
- A spell: fireball **barrage** (Projectile ×5) THEN a flame **pillar** (AOE ×1) — two
  deliveries, one skill.
- An **ability**: mostly melee hits (HitArray) but ALSO lobs a projectile mid-combo — physical
  AND ranged in one skill.

So delivery is a property of each **launch**, not the skill. Delivery moves onto the **Cast
entry** (per-effect). A skill is a bag of effects; each effect that launches something declares
its own delivery.

### Rename: SKILL delivery type, not spell
`ESpellDeliveryType` → conceptually **skill/effect delivery type** — abilities and attacks use
it too, not just spells. (Enum can keep its name for now or be renamed `EDeliveryType`; the
*meaning* is skill-wide.) The five values are unchanged and already built:
`Projectile / Homing / AOE / Instant / Beam`. Projectile/Homing/Beam spawn `ASpellProjectile`;
AOE/Instant spawn VFX + resolve directly (no travel actor).

### The four-array model (now uniform)
Every skill (spell/ability/attack — same struct, three embedded homes per §0) carries up to four
tag-matched arrays. Notify name → array entry → do the thing. One pattern, four arrays:

| Array         | Entry fires                                              | Needs target?         | Tag e.g.         |
| ------------- | -------------------------------------------------------- | --------------------- | ---------------- |
| **HitArray**  | a melee/contact hit (damage weight, §8)                  | yes (contact)         | `Hit`            |
| **CastArray** | a launched sub-attack, carries its own **delivery type** | per delivery          | `Cast1`, `Cast2` |
| **VFXArray**  | a visual, carries an **attach mode** (§ below)           | only if attach=Target | `VFX1`           |
| **SFXArray**  | a sound, same attach model                               | only if attach=Target | `SFX1`           |

A skill mixes freely: pure melee (HitArray only), pure projectile (one Cast entry), barrage+pillar
(two Cast entries, different deliveries), melee+projectile hybrid (HitArray + Cast entry).

### Cast entry shape
```cpp
USTRUCT(BlueprintType)
struct FCastEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName Tag;                       // matches the Cast notify
    UPROPERTY(EditAnywhere) ESpellDeliveryType Delivery;     // PER-EFFECT delivery
    UPROPERTY(EditAnywhere) TSubclassOf<ASpellProjectile> ProjectileClass; // Projectile/Homing/Beam
    UPROPERTY(EditAnywhere) /* AOE VFX / def */;             // AOE/Instant (no travel actor)
    UPROPERTY(EditAnywhere) int32 Count = 1;                 // barrage = >1
    // VFX is NOT here — projectile carries its own muzzle/trail/impact via SetVFXAssets;
    // caster-side cast VFX (windup/glow) lives in VFXArray, fired by its own notifies.
};
```

### Projectile VFX vs VFXArray (important distinction)
A projectile carries its OWN muzzle/trail/impact VFX via `ASpellProjectile::SetVFXAssets`
(from spell data) — because the trail must FOLLOW the moving projectile, which a location-spawned
VFXArray entry can't do. So:
- **Projectile's own VFX** (muzzle/trail/impact) → from the projectile/spell data, carried by
  the actor.
- **VFXArray** → caster-side cast effects (windup glow, charge aura, hand muzzle) fired by VFX
  notifies BEFORE/AROUND the launch.

### Timing (reuses existing patterns)
Simultaneous barrage → one Cast notify, `Count>1`. Staggered rapid-fire → multiple Cast notifies
at spaced frames (or `BurstInterval` stagger within one). Same as multi-target §8.

### Migration debt
`ESpellDeliveryType` currently on `USpellData` (per-spell) → moves to per-effect Cast entries.
The old per-spell field becomes: the default when a skill has no explicit Cast entries, OR
migrated into a Cast entry. Real-build data migration (`PostLoad`). The spike ALREADY models
delivery per-Cast-entry — it's doing the right thing.

**Status:** delivery SYSTEM built; the per-effect Cast-array model is the design; spike wires
notifies → real `ASpellProjectile` directly (like `SpellProjectileTestActor`).

---

## 13. Authoring model — index-by-position (validated)

Array entries have **no Tag field** — their **array position IS their identity**. Notifies are
named `<Family><Index>`; the handler parses family + index and indexes the matching array.

- `VFX0` → `VFXArray[0]`, `VFX1` → `VFXArray[1]`; `Cast0` → `CastArray[0]`; `SFX0` → `SFXArray[0]`
- Bare `VFX` (no digit) → index 0 (friendly default)
- **Multiple notifies can share a name** (`VFX0` ×3) → all fire entry 0 (same effect at 3
  frames). Different indices → different entries.

Authoring win: fill the array entry — **no tag typed**, position is the name. Only the notify is
numbered. Removes the tag-mismatch error class; reader bounds-checks every index (out-of-range →
warn, never crash). Production `UCombatNotify` (§10) can carry an **index field** (picked, not
typed) so nothing is typed at all — spike stays name-based, real build uses the field.

## 14. Skill data-asset restructure (the unification target)

**Insight from the spell test:** the current spell asset has loose animation/VFX-sequence fields
(`CastAnimation`, `MuzzleVFX`/`SpellVFX`/`ImpactVFX`) that code stitches at runtime in a fixed
order — the OLD model. The fused model replaces it.

**A skill (attack OR ability OR spell) = one montage + the four arrays + referenced data:**
- **One montage reference** — the authored animation, carrying ALL notifies (the when-timeline).
- **Hit / Cast / VFX / SFX arrays** — what notifies index into (anim owns *when*, asset *what*).
- **Referenced data** — damage, element, projectile class, Niagara assets — pulled by handlers.

Drops fixed `CastAnimation` + loose VFX fields for montage + arrays. Projectile carries its own
muzzle/trail/impact (§11); caster-side VFX lives in the VFX array.

### This IS the unification
Same montage+arrays structure embedded in all three asset types (`UWeaponAttackData` /
`UAbilityData` / `USpellData`) — same struct, three homes, NOT merged (§0). The three
`Execute*Async` paths collapse into one notify-driven runner (= the phase-runner spine,
`PhaseRunnerCombatRework.md`). **Data unification and execution unification are the SAME arc.**
Migration: restructure the three assets, rename `SpellProjectile` → `SkillProjectile`, move
`ESpellDeliveryType` per-effect, generalize spell-named-but-skill-general concepts.

## 15. Spell flow — VALIDATED

Proven in PIE (`AN_Cast_PrepareAndFire` + `DA_Spells_Fire_Orb`):
- `VFX0` → aura on caster (attach mode working) ✓
- `Cast0` → real `ASpellProjectile` Initialized → Launched → travels → overlaps target → Impact ✓
- Index-by-position authoring (no tags) ✓
- Both models validated through ONE notify system: **melee** (warp/multi-hit/return) and
  **spell** (aura/projectile/impact).

Spike borrows the spell asset only for projectile VFX/stats; plays `SpikeMontage`, not the
asset's animation — confirms the *flow*, not the final structure (§14 changes it). **Bug logged
(fix later):** `SpellProjectileTestActor::SpawnWithSpellData`/`SpawnBurst` never call `Launch()`
→ test-actor projectiles don't move; only the real path and the spike launch correctly.
Spike-only debug knobs (`SpikeWarpDistanceOverride`, `TestDamage`, opposing-team[0]) do NOT
carry to the real build.

---

## 12. VFX/SFX attach modes — `EVFXAttach` (most VFX don't need a target)

A VFX/SFX entry carries an **attach mode** that determines WHERE it spawns and whether it needs
a target. Key realization: **only target-attached (hit) VFX require a target; most VFX don't.**

```cpp
UENUM(BlueprintType)
enum class EVFXAttach : uint8
{
    Caster,        // on casting actor — no target needed (aura, glow)
    Target,        // on target enemy — NEEDS a target (hit/impact)
    ImpactPoint,   // at a hit location — needs a point
    CasterSocket,  // named bone/socket on caster (hand muzzle, weapon tip) — no target; has SocketName
    World          // a fixed/world point — no target
};
```

| Attach       | Needs target? | Use                                                                     |
| ------------ | ------------- | ----------------------------------------------------------------------- |
| Caster       | No            | aura, charge glow, buff                                                 |
| Target       | **Yes**       | hit flash, impact                                                       |
| ImpactPoint  | No (a point)  | ground burst                                                            |
| CasterSocket | No            | muzzle at hand, weapon glow (+ `SocketName` field, EditCondition-gated) |
| World        | No (a point)  | ground marker                                                           |

Reader rules: `Caster`/`CasterSocket` always work; `Target` → skip+warn if no target (never
spawn at zero); `ImpactPoint`/`World` need a location. `CasterSocket` uses
`GetMesh()->GetSocketLocation(SocketName)` and prefers `SpawnSystemAttached` so it follows the
hand. This reinforces keeping **Hit** (always target-bound) separate from **VFX** (mostly not).

---

## 10. Production notify approach — `UCombatNotify` (Option C, for the real build)

The spike uses "Montage Notify" (name-only, engine delegate) — fast, throwaway. The production
build should use a **custom `UAnimNotify` subclass**:

```cpp
UCLASS()
class UCombatNotify : public UAnimNotify
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName Tag;          // "Hit", "VFX1", "SFX0"...
    // extensible: int32 HitIndex; ESpellElement ElementOverride; etc.
    virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim, ...) override;
    // Notify() finds the anim instance from Mesh and broadcasts Tag on OnActionNotify
    // (the same delegate the system already listens on) — written ONCE.
};
```

Why over the spike's name-only notifies:
- **Carries a tag FIELD** (not an overloaded notify name) + room for structured per-hit data.
- **One class, infinite tags** — place it, set the tag, done. No per-tag code (unlike the
  `AnimNotify_<Name>` UFUNCTION path, which doesn't scale).
- **Wiring is once:** `Notify()` resolves the anim instance from the mesh and broadcasts the tag
  on `OnActionNotify` — then every existing handler catches it, no downstream change.

---



## Changelog
- *(this revision)* — Added §8 Hit-array damage-weight model (`Strike`→`Hit` rename, `HitCount`
  int → `Hits` array with per-hit authored weights, notify-order→array-index mapping, debug
  invariant, migration note). Added §9 spike status: warp-in VALIDATED at combat distance, root
  motion confirmed, full-loop wired pending PIE, root-motion-vs-lerp architecture item flagged.
- *(prior)* — Initial capture. Decisions locked: separate data / unified animation;
  notify-driven VFX handshake (anim=when, asset=what); `FSkillVFXEntry` shape (attach-per-entry,
  notify-type-decides-lifetime, per-entry tint, all three asset types as embedded struct not
  merged/inherited); single whole spell montages; spike-gated before any build. Branch context:
  fix/evolution-wear-bd-reality just merged; phase-runner arc not yet started.