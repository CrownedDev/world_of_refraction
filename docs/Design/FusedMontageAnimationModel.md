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

## Changelog
- *(this revision)* — Initial capture. Decisions locked: separate data / unified animation;
  notify-driven VFX handshake (anim=when, asset=what); `FSkillVFXEntry` shape (attach-per-entry,
  notify-type-decides-lifetime, per-entry tint, all three asset types as embedded struct not
  merged/inherited); single whole spell montages; spike-gated before any build. Branch context:
  fix/evolution-wear-bd-reality just merged; phase-runner arc not yet started.