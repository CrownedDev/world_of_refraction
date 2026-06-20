# Cast Launch Origin — projectile spawn point (D6 extension)

**Status:** Design locked, pending build. One open question (Target offset axes, §6).
**Touches:** `FSkillCastEntry`, `ASkillProjectile::InitializeCommon`. Per-delivery (Cast entry), not per-skill.

## 1. Problem

Projectile deliveries spawn at `Caster->GetActorLocation()` (the actor root) in two places:
`ActionExecutor::SpawnProjectileActor` (spawn seed) and `ASkillProjectile::InitializeCommon`
(authoritative reposition — overwrites the spawn seed). Both hardcode the caster root, so every
cast leaves the body's center. No way to launch from a hand, a weapon tip, a point floating in
front of the caster, or near the target.

## 2. Core insight — the caster is the only stable origin

In grid combat the fight's world position isn't known at author time. A data asset can't bake a
world coordinate, because it can't know where the encounter spawns. The **caster** (and, by
extension, the **target**) is the only reference that exists at runtime. So every launch point is
expressed relative to the caster or target — never as an absolute world coord.

This is why an absolute `World` origin was considered and **rejected**: it's the one option that
isn't caster-centered, and it only works in hand-placed setpiece arenas where the projectile and
the arena are authored together. Dead weight for the general (procedural/grid) case.

## 3. The model — `ELaunchOrigin`

A per-entry enum on `FSkillCastEntry`. Only affects travel-actor deliveries
(Projectile / Homing / Beam); AOE / Instant resolve at the target and ignore it.

| Value               | Positional center      | Offset axes              | Use                                  |
| ------------------- | ---------------------- | ------------------------ | ------------------------------------ |
| `ActorOrigin`       | caster root            | — (no offset)            | default — today's behavior           |
| `Socket`            | caster mesh socket     | —                        | hand muzzle, weapon tip, off-hand    |
| `CasterOffsetLocal` | caster                 | rotate with facing       | in front of / behind / beside caster |
| `CasterOffsetWorld` | caster                 | world-aligned            | pillar straight overhead, fixed axis |
| `Target`            | target                 | rotate with caster facing (OPEN) | portal behind the enemy      |

Both offset modes keep the caster as the center of the world; they differ only on whether
"forward/up" follows the caster's rotation:

- **Local axes** — `+X` = "in front of me", `+Z` = up relative to facing. Rotates as the caster turns.
- **World axes** — caster is still the (0,0,0) center, but `+Z` = straight up, `+X` = fixed
  compass direction, regardless of which way the caster faces.

## 4. Fields on `FSkillCastEntry` (EditCondition-gated)

| Field                 | Type            | Shown when `LaunchOrigin ==`            |
| --------------------- | --------------- | --------------------------------------- |
| `LaunchOrigin`        | `ELaunchOrigin` | always                                  |
| `LaunchSocket`        | `FName`         | `Socket`                                |
| `LaunchOffset`        | `FVector`       | `CasterOffsetLocal` / `CasterOffsetWorld` / `Target` |

(`World` + `LaunchWorldLocation` were dropped — see §2.)

## 5. Resolution (in `InitializeCommon`, replaces the `SetActorLocation(Caster->GetActorLocation())` seed)

Pseudocode — `Target` is already in scope; `Caster` is `AActor*` (socket read needs `Cast<ACharacter>`):

```
LaunchLocation = Caster->GetActorLocation()          // ActorOrigin / fallback
switch (LaunchOrigin):
  Socket:            if caster is ACharacter + mesh has socket → GetSocketLocation
  CasterOffsetLocal: LaunchLocation += CasterRotation.RotateVector(LaunchOffset)
  CasterOffsetWorld: LaunchLocation += LaunchOffset            // no rotation
  Target:            LaunchLocation  = (Target ? Target loc : Caster loc)
                                     + CasterRotation.RotateVector(LaunchOffset)  // axes OPEN
SetActorLocation(LaunchLocation)
```

The direction calc immediately below reads `GetActorLocation()` *after* the set, so aim
auto-corrects to the new origin — **no separate rotation change needed.**

## 6. OPEN — Target offset axes

Should the `Target` offset be caster-local or world-aligned?
- "Behind the enemy" reads naturally as **caster-local** (behind = away from caster's facing).
- "Directly overhead the enemy" reads as **world** (`+Z`).
- Leaning: default `Target` to **local** for the portal-behind case; cover overhead via
  `CasterOffsetWorld`. Not yet decided.

## 7. Integration notes

- All projectile spawns route through `SpawnProjectileActor` (`DispatchSpellCast`, the entry
  dispatch, and `SpawnNextBurstProjectile` all call it), so the single seed covers barrage/burst.
- A burst (`Count > 1`) shares one Cast entry → one launch origin for all shots in the volley.
  Correct for the per-delivery model.
- `ActorOrigin` default + NAME_None / zero-offset fields = byte-identical to today. Migrated
  content unchanged.
- Migrated/legacy `USpellData*` projectiles use the non-entry init overload → defaults to
  `ActorOrigin`. Untouched.

## 8. Debug

Extend `USkillCastDebug::GetCastArrayString` to print `Origin=<value>` per entry (optional,
keeps the cast array inspectable per project debug-tool rules).

## 9. Build plan (clustered)

1. **Data layer:** `ELaunchOrigin.h` (new) + `FSkillCastEntry` fields. Compiles alone (nothing
   reads them yet).
2. **Runtime:** `ASkillProjectile` members + `InitializeCommon` switch + entry-init copy.
   PIE-verify: ActorOrigin unchanged; CasterOffset spawns off-body; Target spawns at enemy.
3. **Debug (optional):** cast-array print line.
