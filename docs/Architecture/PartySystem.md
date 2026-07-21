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
- `IsTrialPartyMember(Actor)` — data-based membership test (compares the actor's
  `UCharacterDataComponent->CharacterData` against `Party->Members[].CharacterData`).
  See the Identity Model note below.
- `GetMemberSlotByData(CharacterData)` — slot index of the first member with that
  asset, or `INDEX_NONE`. The lookup `IsTrialPartyMember` is built on.
- `InviteMember` / `DismissMember` / `SetDisplayName` / `RebindLeader`.
- `OnPartyChanged` (`FOnPartyChanged`, `BlueprintAssignable`) — multicast, fires on
  every composition change: `CreateSoloParty`, `InviteMember` success,
  `DismissMember` success. Carries the `UParty*`; consumers re-read state on fire
  rather than trusting the payload to be a diff.

⚠️ `DismissMember(0)` is **refused** — slot 0 is the party leader. Use
`RebindLeader` to change leadership.

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
or trial — **not at battle bootstrap.** By
bootstrap time `AGameModeBase` has already given PC0 a pawn from the *battle level's*
`DefaultPawnClass`, so the `PC->GetPawn()->GetClass()` link would capture the stage
default instead of the player's character. This is invisible today only because the
hub, trial, and battle GameModes happen to share a `DefaultPawnClass`.

Leadership resolves from `GetPlayerController(this, 0)`, **not** the triggering
pawn's controller — since every `ACombatCharacter` auto-possesses an
`ACombatAIController`, an AI companion tripping the encounter has no
`APlayerController` to offer. Leadership is a session property, not a
whoever-walked-in property.

### Party creation ordering

Creation moved from lazy encounter-sphere creation to **hub `BeginPlay`**
(`BP_HubPlayerController` calls `EnsureLocalParty` — now `BlueprintCallable`).
Recruiting happens in the hub, before any encounter fires, so the party must exist
first; a recruitable's `InviteMember` with no party would warn and drop the invite.

`UEncounterComponent::HandleOverlap` still calls `EnsureLocalParty`, but on the
get-or-create's *get* path it is now effectively a **leader rebind**: `OpenLevel`
destroyed the `PlayerController` the hub bound, and the encounter re-points
`Leader` at the current PC0. The lazy-create branch survives as a fallback for a
trial entered without hub party creation.

Two entry points, both valid:

- `EnsureLocalParty(PC, PawnClass)` — the BP-facing default. A null/unconnected
  `PawnClass` routes through the `PC pawn → DefaultSoloPawnClass` config fallback,
  so the config stays the single source of truth.
- `CreateSoloParty(Leader, LeaderPawnClass)` — remains valid but requires an
  explicit class; callers duplicate the config value to use it from BP.
  Consolidation is banked (see TODOs).

## `UPartyInteractionComponent` (UActorComponent)

Walk-up-E invite/dismiss on hub NPCs (Party Assembly POC). Drop on a
`BP_CombatCharacter_*` Blueprint; every placed instance becomes recruitable.

- **Sphere trigger created in `BeginPlay` via `NewObject`** — not the constructor
  — attached to the owner's root and registered, because the component may be
  added to an already-constructed actor where no constructor-time attach point
  exists. Collision profile mirrors the merchant/door triggers. `EndPlay` unbinds
  the overlap handlers and destroys the sphere.
- **`FindNearestInRange(APawn*)` is an overlap query**, not a `TActorIterator`
  sweep: the trigger sphere IS the range, single source of truth, no duplicated
  distance check. Signature matches `AMerchantInteractable::FindNearestInRange` /
  `ATrialDoor::FindNearestInRange` so the hub controller wires all three the same
  way.
- **`IsWithinViewCone` facing filter** (private static): compares camera forward
  (`GetPlayerViewPoint` when a controller exists, pawn transform fallback) against
  the direction to the target. `ViewConeThreshold`: `0.0` = anywhere in front
  (default), `1.0` = must look directly at, `-1.0` degenerates to always-true.
- **`Interact()` resolves membership via `GetMemberSlotByData`** on the owner's
  `CharacterData`: `INDEX_NONE` → `InviteMember(owner class)`, else
  `DismissMember(slot)`. Slot-0 leader protection is the subsystem's — no
  special-casing in the component.
- **Debug hint (Green, `GetUniqueID()`-keyed) runs parallel to
  `OnAvailabilityChanged`.** The hint is PIE feedback until the party UI ships;
  the delegate is the UI's binding point. Interact() refreshes the hint in place
  so the invite/dismiss verb flips without leaving the trigger.

### Hub interaction arbitration

`BP_HubPlayerController`'s `IA_Interact` (Started pin) is **sequential priority —
merchant → trial door → party — not nearest-wins**: each branch is
`FindNearestInRange` → `IsValid`, with the next class hung off `Is Not Valid`.
Merchant and door have no facing filter yet, so inside their trigger bounds they
win regardless of where the player looks. **Recruitable placement must clear
merchant/door trigger bounds** — this is why `Recruitable_Fire` sits at X −300
instead of the surveyed −500 (the trial door's scaled trigger box would have
swallowed E in the overlap band).

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

Anyone else inside the arena sphere is a **spectator** — the "not a Trial Party
member" filter (`IsTrialPartyMember`) rejects them from the Battle Party by
comparing their `CharacterData` against the roster. The join window is therefore not
dead code: it is the mechanism that derives Battle Party from Trial Party.

⚠️ **POC limits (Arc 1):**

- The Trial Party is created *from* the triggering pawn's class, whose CDO supplies
  the same `CharacterData` the pawn instance carries, so the membership gate cannot
  reject the *triggering* pawn — it is real code exercising an always-passing path
  for that one actor until Arc 2 ships hub party assembly. The join-window gate
  (`HandleJoin`) can already reject, since a joiner need not be on the roster.
- Identity is the **`CharacterData` asset**, so two members sharing one asset are
  indistinguishable. Per-instance identity arrives with the Arc 2 grid work (each
  member picks an exclusive row slot, which needs per-instance identity on
  `UBattleConfigComponent`).

## Identity Model

**A party member's identity is its `CharacterData` asset**, not its pawn class.

- **Solo (today)** — `FPartyMember.CharacterData` *is* the identity. `PawnClass` is a
  spawn detail: it says how to put a body in the world, not who that body is.
  `GetMemberSlotByData` and `IsTrialPartyMember` both key off the asset.
- **PvP / duplicates (future)** — two members backed by the same asset are
  indistinguishable under this model, and a networked match needs per-instance and
  per-user identity. That is a separate arc; do not retrofit it onto `CharacterData`
  comparison.
- **Level-designer override** — a placed instance can override its
  `CharacterDataComponent->CharacterData` in the Details panel, diverging from its
  Blueprint CDO. Data-based lookup treats that instance as the **overridden**
  character, not the CDO character. **This is a feature, not a bug**: the designer
  said who this actor is, and the roster comparison honours it. Class-based lookup
  used to get this wrong in the permissive direction.

Identity is the **asset pointer**, not its contents — two assets with identical
stats are still two characters. Concrete consequences (Cluster 3 survey):

- **Player/Generic collision — RESOLVED.** `BP_TestCharacterBase` and
  `BP_CombatCharacter_Generic` used to share one asset (originally
  `DA_Character_GenericLord`, briefly `DA_Character_Template`), making a placed
  Generic recruitable **identity-equal to the player** — the hint read "dismiss"
  and Interact() hit the slot-0 guard. Now split: the player's base uses
  `DA_Character_Template`; `BP_CombatCharacter_Generic` overrides **explicitly**
  with `DA_Character_GenericLord` (an explicit override, not inheritance — an
  inherited value silently tracks any future base repoint, which is exactly how
  the collision happened). Generic is a recruitable identity in its own right.
- `BP_CombatCharacter_Darkness`'s CDO carries `DA_Enemy_Darkness` — an
  **Enemy-origin** DA on a player-family Blueprint. Excluded; content fix banked.
- **Invite/hint asymmetry (known gap):** `InviteMember` resolves `CharacterData`
  off the class **CDO**, while the component's hint text and membership check read
  the **instance**. A designer's per-instance `CharacterData` override is
  therefore ignored by the invite even though the hint names the overridden
  character. Harmless while no placed recruitable overrides its data; an
  instance-taking invite path fixes it properly.

## Integration Points

- `UEncounterComponent::HandleOverlap` — calls `EnsureLocalParty`, then gates Battle
  Party membership on `IsTrialPartyMember` (data-based; the actor's
  `UCharacterDataComponent` is already guaranteed present by the `IsPlayerCombatant`
  pre-guard). `HandleJoin` applies the same gate to arena joiners.
- `ABattleGameMode` — reads `GetPendingBattleParty()` for the player side (enemy
  roster still comes from the encounter, not the party); empty-party abort +
  cross-check.
- `UBattleConfigComponent` — holds the `TWeakObjectPtr<UParty>` back-reference per
  spawned combatant.
- `ACombatCharacter` — `AutoPossessAI` makes engine possession authoritative, which
  is what lets an AI companion be a `Player`-origin character under a bot controller.
- `BP_HubPlayerController` — creates the party at BeginPlay (`EnsureLocalParty`)
  and owns the three-way `IA_Interact` arbitration (see Hub interaction
  arbitration). `UPartyInteractionComponent::OnAvailabilityChanged` is the party
  UI's future binding point.

## Known Limitations / TODOs

- No party assembly UI — Arc 2. Solo-of-one is the only shape Arc 1 produces.
- AI companion fill deferred to Arc 2.
- `CharacterData`-keyed membership (see Identity Model) — per-instance identity with
  the Arc 2 grid; per-user identity with PvP.
- Multiplayer invite (second `PlayerController` joins the local party) is a stub.
- Invite/hint asymmetry on per-instance `CharacterData` overrides (see Identity
  Model).

### Banked

- **Darkness recruitable DA fix** — `BP_CombatCharacter_Darkness` still carries the
  Enemy-origin `DA_Enemy_Darkness`. (The Generic half of this chore is resolved —
  see Identity Model.)
- **Element-specific spell authoring** — 7 of 9 elements have zero spell assets
  (only Fire 18 / Water 17 exist); the 6 new element lords run Generic-pool kits
  that resolve at cast. Its own content arc.
- **Four orphaned inventories** — `DA_Inventory_Fire/Water/Darkness/BD` exist but
  their lords still point at `DA_Test_Inventory`; repoint or delete.
- **Pool spell targeting** — all 15 Generic pool spells are authored
  `targetType=Enemy`, including the heals and buffs (Mend, Restore, Grace…).
- **Merchant + trial door view-cone migration** — adopt the facing filter so hub
  arbitration can become look-based instead of pure priority.
- **`CreateSoloParty` / `EnsureLocalParty` consolidation** — one entry point once
  the hub-BeginPlay path is the proven default.
- **Character switching** — possession-based; switching swaps slot contents so
  slot 0 keeps meaning "you".
- **Per-character saved record** — gear, cosmetics, progression between the
  immutable template asset and the live component; folds into Persistence.
- **Cosmetics-vs-gear reset policy** — gated on the roguelite run model.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-07-21 | Clusters 2–3 (Arc 1.5a interaction + content): new `UPartyInteractionComponent` (BeginPlay `NewObject` sphere, overlap-query `FindNearestInRange`, `IsWithinViewCone`, invite/dismiss toggle via `GetMemberSlotByData`); `EnsureLocalParty` made `BlueprintCallable`, party creation moved to hub BeginPlay; recruitable trio (Resonator/Fire/Water) placed in TestLevel_Nav with a third `IA_Interact` arbitration branch; Identity Model extended with concrete asset-collision consequences; invite log prints occupancy. Follow-up same day: player/Generic identity collision resolved (`DA_Character_Template` for the base, explicit `DA_Character_GenericLord` override on `BP_CombatCharacter_Generic`); Banked list swept (element spell authoring, orphaned inventories, pool-spell targeting, character switching, per-character record, reset policy). | feature/party-assembly-poc |
| 2026-07-21 | Cluster 1 (Arc 1.5a plumbing): `FOnPartyChanged` multicast delegate broadcast from `CreateSoloParty` / `InviteMember` / `DismissMember`; new `GetMemberSlotByData`; `IsTrialPartyMember` migrated from pawn-class to `CharacterData` comparison; slot 0 protected from `DismissMember`. Added the Identity Model section. | feature/party-assembly-poc |
| 2026-07-20 | Initial documentation. Encounter Composition Arc 1 (Party Foundation, merged `e5908739`): `UParty`, `UPartySessionSubsystem`, `FPartyMember`, the config-driven `DefaultSoloPawnClass`, the `EnsureLocalParty` lazy-create chain, and the two-layer Trial Party / Battle Party model. | feature/encounter-composition-arc1 |
