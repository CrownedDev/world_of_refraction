# Encounter Composition System

**Status:** Arc 1 (Foundation) **COMPLETE** — shipped, merged `e5908739`. Arcs 2 (Composition) and 3 (Trial Pool) design-locked, not built.
**Author:** Crown + Claude session, 2026-07-20
**Prerequisite reading:** `PartyMatchSetup.md`, `CombatCharacter.md`, `AIArchitecture.md`, `Roadmap.md`

---

## Executive Summary

The current combat loop (T-C1) supports one player pawn vs one placed enemy. This design extends it to a full composition system:

- **Parties** — 3 active player-side characters, assembled at hub
- **Encounters** — multi-enemy compositions defined either by hand-authored roster (Fixed) or role-slot rolls from typed enemy pools (Pool)
- **Ghost battles** — player-built characters spawning as AI opponents (Lord/Contender meta-layer foundation)
- **Identity split** — CharacterData describes *what* a character is; engine possession describes *who's controlling it right now*

The system ships across three arcs: **Foundation** (party + identity + runtime config), **Composition** (encounter data + multi-enemy battle mode), **Trial Pool** (typed enemy pools + spawner variants).

---

## Design Principles

1. **Identity ≠ Control.** A CharacterData asset describes stats/class/element/origin. Who's driving the pawn is a runtime concern handled by engine possession. This lets one character be player-controlled in solo play and AI-controlled as a ghost/companion without asset duplication.

2. **Encounter is composition, not a pawn.** The current model conflates "enemy pawn in the world" with "the encounter." Splitting them lets one visible enemy trigger a multi-member fight, and lets the same battle stage host many encounter configurations.

3. **Nightreign visibility.** Player sees the enemy they're about to fight before engaging (rolled at level load for Pool spawners). Turn-based combat rewards knowledge; hiding the opponent hurts strategy.

4. **Data-driven authoring.** `UEncounterData` and typed trial pools mean designers author compositions without touching code. Ghost battles feed the same pipeline from player runs.

5. **Backward-compatible migration.** Existing T-C1 single-enemy placements convert to Fixed spawners with a one-entry roster. No content is lost.

---

## Architecture Overview

### The 11 pieces

| # | Piece | Type | Arc |
|---|---|---|---|
| 1 | `UPartySessionSubsystem` | GI subsystem | Foundation |
| 2 | `ECharacterOrigin` enum on `UCharacterData` | Enum | Foundation |
| 3 | `bIsAIControlled` removal + callsite migration | Refactor | Foundation |
| 4 | AI companion fill for solo player | Runtime logic | Foundation |
| 5 | `UBattleConfigComponent` | Native component | Foundation |
| 6 | `UEncounterData` | Data asset | Composition |
| 7 | Multi-enemy support in `ABattleGameMode` | Refactor | Composition |
| 8 | Placeholder AI grid formation | Runtime logic | Composition |
| 9 | `UTrialData` typed enemy pools | Data asset extension | Trial Pool |
| 10 | `AEncounterSpawner_Pool` | Actor | Trial Pool |
| 11 | `AEncounterSpawner_Fixed` | Actor | Trial Pool |

### How they connect

```
Hub authoring:
    Placed character actors (BP_CombatCharacter_*)
    Cube invite point (multiplayer)
                    ↓
    UPartySessionSubsystem (GI-scoped)
        └─ FParty { Leader, Members[3], DisplayName }
                    ↓
    Trial entry (existing TrialRunSubsystem::EnterTrial)
                    ↓
Trial-level authoring:
    AEncounterSpawner_Fixed (references UEncounterData with hand-picked roster)
    AEncounterSpawner_Pool (references trial's typed pool + role slots)
    Both place the rolled/authored pawn visible in world
                    ↓
    Player triggers encounter → sphere → 2s window
                    ↓
    UTrialRunSubsystem::EnterEncounter(TrialData, LocalParty, OpposingParty, Diff)
        └─ Stashes hard-refs to pawn BP classes for both sides
                    ↓
    OpenLevel to encounter stage
                    ↓
    ABattleGameMode::Bootstrap
        └─ Reads party from PartySession + roster from stash
        └─ Spawns pawns via SpawnActorDeferred (per T-C1a pattern)
        └─ Sets BattleConfig per pawn (grid pos, team, party ref)
        └─ Reads engine possession for AI/player control
        └─ Starts combat via CombatOrchestrator (unchanged)
                    ↓
    Combat resolves → ExitEncounter → OpenLevel back to trial
```

---

## Data Model

### `UCharacterData` (existing, modified)

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly)
ECharacterOrigin Origin = ECharacterOrigin::Enemy;

// REMOVED: bIsAIControlled (moved to runtime engine possession)
```

```cpp
UENUM(BlueprintType)
enum class ECharacterOrigin : uint8
{
    Enemy   UMETA(DisplayName = "Enemy"),
    Player  UMETA(DisplayName = "Player Build"),
};
```

**Ghost = `Origin::Player` character being AI-controlled at runtime.**

### `UBattleConfigComponent` (new, native on `ACombatCharacter`)

Runtime combat state per pawn. Set by `ABattleGameMode` on spawn; read by combat systems.

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
FGridPosition GridPosition;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
TWeakObjectPtr<UParty> OwningParty;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
int32 TeamIndex = 0;  // 0 = local perspective ally, 1 = local perspective opposing

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
FText DisplayContext;  // e.g. "Crown's Party — Slot 2"
```

**⚠️ Not on `CharacterData`:** these are per-run, per-encounter runtime values. CharacterData is asset-immutable.

### `UParty` (new, UObject)

A `UObject`, not a `USTRUCT`: `UBattleConfigComponent` holds a weak back-reference,
and `TWeakObjectPtr` only accepts `UObject` subclasses. It also gives the type real
identity and somewhere to grow replication.

```cpp
USTRUCT(BlueprintType)
struct FPartyMember
{
    UPROPERTY(BlueprintReadOnly) TSoftClassPtr<ACombatCharacter> PawnClass;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UCharacterData> CharacterData;  // resolved at invite
};

UCLASS(BlueprintType)
class UParty : public UObject
{
    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;  // "<Leader>'s Party" by default; SetDisplayName overrides

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<APlayerController> Leader;  // WEAK — OpenLevel recreates the PC

    UPROPERTY(BlueprintReadOnly)
    TArray<FPartyMember> Members;  // Max 3 (PartyConstants::MAX_PARTY_MEMBERS)
};
```

**One slot struct, not two parallel arrays.** Pawn class and CharacterData travel
together so they cannot drift out of sync.

**⚠️ `Leader` is weak and goes stale on every level transition** — a non-seamless
`OpenLevel` destroys and recreates the `PlayerController`. Call
`UPartySessionSubsystem::RebindLeader` after a transition; membership is unaffected.

**⚠️ Parties must be outered to the GameInstance**, never the world. A
world-outered `UObject` is destroyed by `OpenLevel` — the same trap
`UTrialRunSubsystem` documents for runtime-created `CharacterData` — and the party
would silently empty on the first hub→trial swap.

### `UPartySessionSubsystem` (new, GI-scoped)

```cpp
UCLASS()
class UPartySessionSubsystem : public UGameInstanceSubsystem
{
    UFUNCTION(BlueprintCallable) FParty GetLocalParty() const;
    UFUNCTION(BlueprintCallable) void CreateSoloParty(APlayerController* Leader);
    UFUNCTION(BlueprintCallable) bool InviteMember(TSoftClassPtr<ACombatCharacter> PawnClass);
    UFUNCTION(BlueprintCallable) void DismissMember(int32 SlotIndex);
    UFUNCTION(BlueprintCallable) void SetDisplayName(FText NewName);

    // ...
};
```

**Party assembly at hub:**
- Local player triggers `CreateSoloParty` on hub entry
- Walks up to character actor in hub, presses E → `InviteMember(that actor's class)`
- Cube invite point (multiplayer stub) — banked for real networking

### `UEncounterData` (new, data asset)

```cpp
UCLASS(BlueprintType)
class UEncounterData : public UPrimaryDataAsset
{
    UPROPERTY(EditAnywhere) FText EncounterName;

    UPROPERTY(EditAnywhere) EEncounterType Type = EEncounterType::Normal;

    UPROPERTY(EditAnywhere) EAIDifficulty Difficulty = EAIDifficulty::Medium;

    UPROPERTY(EditAnywhere)
    TArray<FEncounterEntry> Roster;
};

USTRUCT(BlueprintType)
struct FEncounterEntry
{
    UPROPERTY(EditAnywhere) TSoftClassPtr<ACombatCharacter> PawnClass;
    UPROPERTY(EditAnywhere) FGridPosition PreferredGridPosition;
};

UENUM(BlueprintType)
enum class EEncounterType : uint8
{
    Normal,
    Elite,
    Boss,
    Ambush,  // exception: no visible enemy pre-trigger
};
```

### `UTrialData` (existing, extended)

```cpp
// Existing fields: Name, Description, Level, EncounterLevel, Element

// NEW — typed enemy pools for AEncounterSpawner_Pool to roll from
UPROPERTY(EditAnywhere, Category="Enemy Pools")
TArray<TSoftClassPtr<ACombatCharacter>> GenericPool;

UPROPERTY(EditAnywhere, Category="Enemy Pools")
TArray<TSoftClassPtr<ACombatCharacter>> ElitePool;

UPROPERTY(EditAnywhere, Category="Enemy Pools")
TArray<TSoftClassPtr<ACombatCharacter>> BossPool;

UPROPERTY(EditAnywhere, Category="Enemy Pools")
TArray<TSoftClassPtr<ACombatCharacter>> AmbushPool;
```

### Spawner Actors (new)

**`AEncounterSpawner_Fixed`:**
```cpp
UPROPERTY(EditAnywhere) TObjectPtr<UEncounterData> Encounter;

// On BeginPlay: spawns the roster's pawns as visible enemies at authored positions
// On overlap: starts encounter with Encounter->Roster
```

**`AEncounterSpawner_Pool`:**
```cpp
UPROPERTY(EditAnywhere) FEnemyPoolRequest RolledEnemies;
UPROPERTY(EditAnywhere) EAIDifficulty Difficulty = EAIDifficulty::Medium;

// On BeginPlay: rolls from TrialData's typed pools per the request, spawns as
// visible enemies at authored positions (or auto-arranged)
// On overlap: starts encounter with the rolled roster
```

```cpp
USTRUCT(BlueprintType)
struct FEnemyPoolRequest
{
    UPROPERTY(EditAnywhere) int32 GenericCount = 0;
    UPROPERTY(EditAnywhere) int32 EliteCount = 0;
    UPROPERTY(EditAnywhere) int32 BossCount = 0;
    UPROPERTY(EditAnywhere) int32 AmbushCount = 0;
};
```

---

## Ghost Battle Architecture

Ghosts are the Lord/Contender meta-layer's opponent type. Design details deferred to a dedicated arc, but the foundation ships now.

### What ships in Foundation arc

- `ECharacterOrigin::Player` value on the enum
- `UCharacterData` assets can be authored as Player-origin (or serialised from player runs later)
- Engine possession as source of truth for control → any Player-origin CharacterData spawned with an AI controller is functionally a Ghost

### What defers to Ghost arc (later)

- Player run capture — serialising a player's `UCharacterData` + `ULoadoutComponent` state as a ghost profile
- Ghost pool storage — where captured ghosts live (per-account save? shared service?)
- Ghost injection into trial pools — how a Ghost enters the encounter pipeline
- Adaptive AI — Tekken-style learning behaviour (aspirational, not required for the meta-layer)

### The critical foundation

The identity split (Origin vs Possession) means ghosts do not need special-case handling in combat code. A ghost is just a `Player`-origin CharacterData being bot-controlled. Combat systems query `IsPlayerControlled()` where they need to know who's driving; nothing else changes.

---

## Runtime Flows

### Party Assembly (hub)

1. Player enters hub → `UPartySessionSubsystem::CreateSoloParty(LocalPC)` auto-fires on `BeginPlay` (empty party with leader set)
2. Player walks to character actor `BP_CombatCharacter_Caster` in hub → press E
3. Interaction component calls `PartySession->InviteMember(pawn class ref)`
4. Party gains member; UI (later) reflects
5. Player repeats up to 2 more times → party of 3

**Solo AI companion fill:** if the player never invites anyone, party enters trial with size 1. If they invite up to 2 AI companions, party enters with size 2 or 3.

**Multiplayer invite cube:** placed in hub. On interaction, opens invite UI (banked). Second player's `PlayerController` joins the local party.

### Encounter Trigger (trial)

1. `AEncounterSpawner_Fixed` / `_Pool` placed in trial level, spawned rolled/authored pawns at `BeginPlay`
2. Player walks into trigger sphere on the encounter (existing `UEncounterComponent`-derived logic on the spawner or lead pawn)
3. Sphere activates 2s join window (existing)
4. Window closes → spawner packages the encounter:
   - `OpposingParty` = spawner's roster (pawn BP classes)
   - `LocalParty` = `PartySession->GetLocalParty().MemberPawnClasses`
5. `TrialRunSubsystem::EnterEncounter(TrialData, LocalPartyClasses, OpposingClasses, Difficulty)` (existing signature extended)
6. `OpenLevel(EncounterLevel)`

### Battle Bootstrap

1. `ABattleGameMode::BeginPlay` → next tick → `BootstrapCombat`
2. Reads stashed classes from `TrialRunSubsystem`
3. For each entry: `SpawnActorDeferred` → assign `CharacterData` on `CharacterDataComponent` → **set `BattleConfig`** (grid pos, team, party ref, display context) → `FinishSpawning`
4. Possess:
   - Local party member 0 → `PC0`
   - Local party members 1+ → AI controller (auto-spawned)
   - Opposing party → AI controllers
5. Grid subsystem places pawns via each pawn's `BattleConfig->GridPosition`
6. `CombatOrchestrator->StartCombat(LocalParty, OpposingParty, Difficulty)` (existing, unchanged)

### Grid Position Selection

**Local party positioning:**
- From `BattleConfig->GridPosition` set at spawn
- Value flows: `PartySession → UParty → member preference → BattleConfig at spawn`
- Party assembly UI (later) lets player customise
- Default heuristic if unset: front row for tanks, back for casters

**Grid rules (Arc 2 authoring — refined 2026-07-20):** each character picks **one
exclusive row slot** (Front / Middle / Back) plus a **strategic column pick** within
that row. Row exclusivity means **no two party members occupy the same row** — a
3-member party spans all three rows, one member each. This makes row assignment a
real party-composition decision (who tanks front, who casts from back) rather than a
free-for-all, and needs **per-instance identity** on `UBattleConfigComponent` so two
members backed by the same character can hold distinct rows (membership is keyed on
the `CharacterData` asset and cannot distinguish them — see the PartySystem.md
Identity Model).

**Opposing party positioning:**
- Fixed encounters: `FEncounterEntry::PreferredGridPosition` (authored)
- Pool encounters: AI heuristic (`EncounterUtils::PickAIFormation`)
- Heuristic v1: Caster class → back row, others → middle row (placeholder — evolve later)

### AI/Player Control at Runtime

Queried per pawn via:
- `Pawn->IsPlayerControlled()` — human is driving
- `Pawn->IsBotControlled()` — AI is driving

Every current `CharacterData->bIsAIControlled` callsite migrates to `!Pawn->IsPlayerControlled()` (inverse) or the direct engine check.

---

## Build Sequence

### Arc 1: Foundation — ✅ SHIPPED (merged `e5908739`, 2026-07-20)

**Delivered:**
- `Public/Party/Party.h` + `.cpp` — **`UParty` (UObject)**, not the originally-planned `FParty` struct (`TWeakObjectPtr<UParty>` needs a UObject), with one `FPartyMember` slot struct instead of parallel arrays.
- `Public/Party/PartySessionSubsystem.h` + `.cpp` — `UCLASS(Config = Game)`, `DefaultSoloPawnClass`, `EnsureLocalParty` lazy-create, and `CharacterData`-keyed membership lookup (`GetMemberSlotByData` / `IsTrialPartyMember`; migrated from pawn-class comparison in Arc 1.5a, alongside `OnPartyChanged` and the slot-0 leader guard).
- `Public/Combat/AI/CombatAIController.h` + `.cpp` — bare self-reaping `AAIController`; `AIModule` added to `Build.cs`.
- `Public/Character/BattleConfigComponent.h` + `.cpp` — 10th native on `ACombatCharacter`.
- `Public/Character/ECharacterOrigin.h` — new enum; `bIsAIControlled` removed, 4 callsites migrated (origin vs control split).
- `ACombatCharacter` — `AutoPossessAI` + `AIControllerClass` wiring, controller self-cleanup in `OnUnPossess`.
- Two-layer Trial Party / Battle Party model in `BattleGameMode`.

See `docs/Architecture/PartySystem.md` and `docs/Architecture/CombatCharacter.md`.

**PIE-verified end to end:** hub → trial door → engage enemy → level swap → HUD builds → AI possesses → combat + defense both directions → return to trial. `"Battle Party N of Trial Party N"` cross-check confirms both layers agree.

**Not shipped (Arc 2/3):** party UI, encounter data assets, spawner actors, pools, AI companion fill.

### Arc 2: Composition

**Deliverables (~6 files):**
- `Public/Encounter/EncounterData.h` + `.cpp`
- `Public/Encounter/EncounterTypes.h` (`EEncounterType`, `FEncounterEntry`)
- `Public/Trial/BattleGameMode.h` + `.cpp` — refactor to iterate rosters
- `Public/Utils/EncounterUtils.h` — AI grid formation placeholder

**Shipped:** hand-authored multi-enemy fights (1v3, boss + adds) via Fixed spawner referencing a `UEncounterData` asset.

**PIE verify:** replace existing placed enemy with an `AEncounterSpawner_Fixed` referencing a 3-enemy `UEncounterData`. Full loop works, all 3 enemies spawn on battle stage.

### Arc 3: Trial Pool

**Deliverables (~5 files):**
- `Public/Trial/TrialData.h` — add typed pool arrays
- `Public/Encounter/EncounterSpawner_Fixed.h` + `.cpp`
- `Public/Encounter/EncounterSpawner_Pool.h` + `.cpp`
- Migration: existing single-enemy T-C1 placements → `AEncounterSpawner_Fixed` with 1-entry roster

**Shipped:** typed enemy pools per trial, spawners that roll pool → placeholder-visible reveal, both spawner variants coexisting.

**PIE verify:** trial level has both a Fixed encounter (boss) and a Pool encounter (rolled from Generic pool). Each run rolls different generics.

---

## Design Rationale

### Why identity + possession split (not `bIsAIControlled` flag)

Current flag conflates asset identity ("this is an enemy") with runtime control ("AI is driving"). This breaks for:
- **Ghost battles** — Player-built character being AI-driven
- **PvP** — Enemy-side is now player-controlled
- **AI companions** — Player-origin character being AI-driven

Splitting them:
- `Origin` (asset-level) — describes what the character IS
- Engine possession (runtime) — describes who's DRIVING
- Ghost naturally = Player-origin + bot-controlled

### Why B (visible enemy) over A (mystery spawn)

- Turn-based combat rewards knowledge of the opponent
- Player agency: see enemy → prepare loadout → engage or skip
- Matches Nightreign/Elden Ring pattern (cited inspiration)
- Ambush encounters preserve the A pattern where the design wants surprise

### Why party at hub only (not mid-run recruitment)

- Session-scoped commitment
- Cleaner state management (party locked at trial entry, persists through combat)
- Nightreign precedent
- Mid-run recruitment can bank as future feature ("recruit fallen enemy?")

### Why 3 active, no reserves

- Fits Nightreign / Expedition 33 party feel
- Simplifies death handling (out = out until revive)
- No mid-combat swap complexity
- Reserves can bank as future feature

### Why typed pools (Generic/Elite/Boss/Ambush)

- Each type has different pacing/reward expectations
- Encounter authors can request "1 Elite" without knowing which specific Elite exists in this trial
- Same encounter site plays differently each run (Nightreign feel)
- Fixed spawners handle scripted encounters (specific bosses)

### Why pawn BP class refs (not raw CharacterData)

- Pawn BP holds visual + stats (via CharacterData default on its native component)
- One entry = one complete character
- Placeholder mesh in world = the actual rolled pawn (no "silhouette + reveal" desync)
- Visual variants of same stats = author two pawn BPs pointing at same CharacterData

---

## Migration Path

### T-C1 backward-compat

Existing enemies placed in `TestLevel_Trial_01`:
1. Arc 1 lands — combat still works (party subsystem returns party-of-1 with player's pawn class, encounter still spawns the placed enemy)
2. Arc 2 lands — placed enemies remain functional (single-pawn encounters bypass EncounterData; UEncounterComponent still triggers combat directly)
3. Arc 3 lands — placed enemies become `AEncounterSpawner_Fixed` with 1-entry roster. Same visual, formal composition.

No content lost, no PIE regressions between arcs.

### `bIsAIControlled` removal

Grep-heavy but mechanical:
- Every read of `CharacterData->bIsAIControlled` migrates to `!Pawn->IsPlayerControlled()` or context-specific check
- Field removed from `UCharacterData`
- Data asset re-save wave (accept that old refs to the property become defaults)
- Isolate as its own commit within Arc 1 (per T-C1 discipline)

### Trial pool migration

Existing `UTrialData` assets need typed pool arrays populated:
- Empty pools are legal (spawners with pool requests will log and skip)
- Fixed spawners work without pools (they reference `UEncounterData` directly)
- Pool authoring is opt-in per trial

---

## Known Gaps & Banked Follow-ups

### Not solved by this system

**Multi-instance combat** — Party of 3 splitting into concurrent fights still collides via GI singletons (`TurnManager`, `ActionExecutor`, etc.). Multi-instance retrofit required for splitting play. Banked as `Multi-Instance Retrofit` arc.

**Persistence** — Character HP/EP still wipes on encounter entry/exit (OpenLevel). Party composition survives (GI subsystem). Full persistence banked as `Persistence Keystone` arc.

**Networking** — Party invite cube is single-player stub. Real multiplayer requires PC0/PC1/PC2 possession, server-authoritative combat, replicated party state. Banked as `Networking Foundation` arc.

**Camera** — Combat still has no cinematic direction. Banked as `Camera System` arc.

### Banked follow-ups within this system

**Ghost capture pipeline** — serialise player runs into ghost `UCharacterData` + loadout. Requires save system.

**Reserves** — 4th+ characters that swap in on active death. Reject/accept debate: rejected for POC (matches Nightreign), can revisit.

**Party UI** — Currently walk-up-and-E on hub actors. Real party window banked.

**Encounter randomisation** — Pool spawners currently roll on level load. Deterministic seed (replay-friendly) vs fresh roll (variety-friendly) debate deferred.

**AI companion behaviour depth** — Current AI is enemy-tuned. "Friendly" AI variants (respecting player intent, buffing allies preferentially) banked.

**Ally death handling** — Locked: dead ally out of fight, revivable via existing Resurrection spell effect or out-of-combat mechanic. No auto-swap.

**Class balance** — Free party composition (any combination). Class synergy bonuses/penalties can bank as design polish.

**Encounter victory conditions beyond wipe** — Wave-based bosses, escape encounters, escort. Bank as boss-encounter arc.

**Loot per encounter type** — `ULootComponent` referencing loot pool assets. Banked as `Loot Pool` arc.

---

## Related Design Docs

- `PartyMatchSetup.md` — planned party system (superseded by this doc's Foundation arc)
- `CombatCharacter.md` — native component architecture (Arc 1 extends this with BattleConfig)
- `AIArchitecture.md` — AI decision-making (referenced for AI grid formation heuristic)
- `Roadmap.md` — bank list of arcs; this doc drives Encounter Composition entries
- `Resources_Design.md` — camera bank (unaffected by this arc)

---

## Changelog

- 2026-07-20 — Doc created after full design session. All 11 pieces locked, 3-arc sequence set. Prerequisite for build-side work.
