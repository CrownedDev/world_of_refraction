# Party System

## Overview

The party is the player-side roster as a **first-class runtime entity**: a
session-scoped object that survives every level transition and is the authority on
who fights on the player's side. It replaces the old model where the battle stage
sourced its player team from a placed actor or a hardcoded single pawn.

Shape as of Arc 1 (Foundation):

- **Session-scoped** — the party lives on the `GameInstance`, so it survives the
  hub → trial → battle → trial `OpenLevel` chain that destroys every actor.
- **Hub-only assembly** — the party is assembled at the hub (Arc 2), locked at
  trial entry, and persists through combat. No mid-run recruitment.
- **Solo player + AI companions** — a solo player enters as a party of one; up to
  two AI companions fill the remaining slots (companion fill lands in Arc 2).
- **Max 3 active, no reserves** — `PartyConstants::MAX_PARTY_MEMBERS`. A dead
  member is out until revived.

This document covers Arc 1's plumbing. Encounter composition, ghost battles, and
typed enemy pools are later arcs — see `docs/Design/EncounterCompositionSystem.md`.

## `UParty` (UObject)

A `UObject`, not a `USTRUCT`. Two reasons:

- `UBattleConfigComponent` holds a `TWeakObjectPtr<UParty>` back-reference, and
  `TWeakObjectPtr` only accepts `UObject` subclasses.
- It gives the party real identity and somewhere to grow replication.

Fields: `DisplayName` (FText, "<Leader>'s Party" by default), `Leader`
(`TWeakObjectPtr<APlayerController>` — weak because `OpenLevel` recreates the PC),
and `TArray<FPartyMember> Members`.

⚠️ **Outer must be the `GameInstance`, never the world.** A world-outered `UObject`
is destroyed by `OpenLevel` — the same trap `UTrialRunSubsystem` documents for
runtime-created `CharacterData` — and the party would silently empty on the first
transition. `UPartySessionSubsystem` creates parties with `GetGameInstance()` as
outer for exactly this reason.

## `FPartyMember` (slot struct)

One member slot, **not two parallel arrays**:

```cpp
USTRUCT(BlueprintType)
struct FPartyMember
{
    TSoftClassPtr<ACombatCharacter> PawnClass;      // authored identity
    TObjectPtr<UCharacterData>      CharacterData;  // resolved at invite time
};
```

The pawn class is the authored identity; the `CharacterData` is resolved from that
class's CDO when the member is invited and cached (hard ref) so the battle stash —
which takes `TArray<UCharacterData*>` — does not re-load the class mid-transition.
Keeping the two in one struct means they cannot drift out of sync, which a
parallel-array shape could not enforce.

## `UPartySessionSubsystem` (UGameInstanceSubsystem)

`UGameInstanceSubsystem`, GameInstance-scoped for the same reason
`UTrialRunSubsystem` is: the party must outlive the level swaps. Holds
`TArray<TObjectPtr<UParty>>` (hard refs — the subsystem owns party lifetime; an
array, not a single pointer, because multiplayer adds remote parties, though Arc 1
only ever fills index 0).

Key methods:

- `GetLocalParty() const` — pure query, returns null when none exists yet.
- `EnsureLocalParty(PC, PawnClass)` — get-or-create (non-const).
- `IsTrialPartyMember(Actor)` — class-based membership test (see below).
- `InviteMember` / `DismissMember` / `SetDisplayName` / `RebindLeader`.

### Config-driven default pawn

A `UGameInstanceSubsystem` has **no asset or Blueprint** for a designer to edit, so
`EditDefaultsOnly` on it would compile and be permanently unreachable. The idiom is
`UCLASS(Config = Game)` + `UPROPERTY(Config)`, authored in `DefaultGame.ini` — the
same pattern `UTrialRunSubsystem::HubLevel` uses:

```ini
[/Script/world_of_refraction.PartySessionSubsystem]
DefaultSoloPawnClass=/Game/Blueprints/Characters/Base/BP_TestCharacterBase.BP_TestCharacterBase_C
```

### Lazy-create flow

`EnsureLocalParty` creates the solo party on demand, resolving the member pawn class
through a chain:

```
passed-in PawnClass  →  PC->GetPawn()->GetClass()  →  DefaultSoloPawnClass  →  fail (log error, return null)
```

⚠️ **It must be called where the player possesses their REAL character** — the hub
or trial, via `UEncounterComponent::HandleOverlap` — **not at battle bootstrap.** By
bootstrap time `AGameModeBase` has already given PC0 a pawn from the *battle level's*
`DefaultPawnClass`, so the `PC->GetPawn()->GetClass()` link would capture the stage
default instead of the player's character. This is invisible today only because the
hub, trial, and battle GameModes happen to share a `DefaultPawnClass`.

Leadership resolves from `GetPlayerController(this, 0)`, **not** the triggering
pawn's controller — since every `ACombatCharacter` auto-possesses an
`ACombatAIController`, an AI companion tripping the encounter has no
`APlayerController` to offer. Leadership is a session property, not a
whoever-walked-in property.

## Two-layer model: Trial Party vs Battle Party

The party splits into two layers:

- **Trial Party** — `GetLocalParty()`, session-scoped. The full roster the player
  brought into the trial. Survives every transition.
- **Battle Party** — the **subset of the Trial Party present when the encounter join
  window closes**. Derived per-encounter by `UEncounterComponent`, handed to
  `ABattleGameMode` via the stash, and not persisted. `BattleGameMode` fails loudly
  on an empty Battle Party rather than falling back to another roster source, and
  cross-checks Battle against Trial (`"Battle Party N of Trial Party N"`), warning if
  Battle exceeds Trial.

Anyone else inside the arena sphere is a **spectator** — the existing "not a Trial
Party member" filter (`IsTrialPartyMember`) rejects them from the Battle Party. The
join window is therefore not dead code: it is the mechanism that derives Battle
Party from Trial Party.

⚠️ **POC limits (Arc 1):**

- The Trial Party is created *from* the triggering pawn's class, so the membership
  gate cannot currently reject anything — it is real code exercising an
  always-passing path until Arc 2 ships hub party assembly.
- Membership is **class-based**, so two members of the same class are
  indistinguishable. Instance identity arrives with the Arc 2 grid work (each member
  picks an exclusive row slot, which needs per-instance identity on
  `UBattleConfigComponent`).

## Integration Points

- `UEncounterComponent::HandleOverlap` — calls `EnsureLocalParty`, then gates Battle
  Party membership on `IsTrialPartyMember`.
- `ABattleGameMode` — reads `GetPendingBattleParty()` for the player side (enemy
  roster still comes from the encounter, not the party); empty-party abort +
  cross-check.
- `UBattleConfigComponent` — holds the `TWeakObjectPtr<UParty>` back-reference per
  spawned combatant.
- `ACombatCharacter` — `AutoPossessAI` makes engine possession authoritative, which
  is what lets an AI companion be a `Player`-origin character under a bot controller.

## Known Limitations / TODOs

- No party assembly UI — Arc 2. Solo-of-one is the only shape Arc 1 produces.
- AI companion fill deferred to Arc 2.
- Class-based membership (see POC limits) — instance identity with Arc 2 grid.
- Multiplayer invite (second `PlayerController` joins the local party) is a stub.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-07-20 | Initial documentation. Encounter Composition Arc 1 (Party Foundation, merged `e5908739`): `UParty`, `UPartySessionSubsystem`, `FPartyMember`, the config-driven `DefaultSoloPawnClass`, the `EnsureLocalParty` lazy-create chain, and the two-layer Trial Party / Battle Party model. | feature/encounter-composition-arc1 |
