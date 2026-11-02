# Co-op state ownership (B-130)

**What this is:** for every system that holds state, where that state lives today, who should own it once co-op is built, and what breaks first. It is the map for the co-op work, which is scheduled after B-090 (save) and B-105 (new HUD) (D-065). Until then, D-064 applies: **new per-fight state goes on the battle's actors, GameState or PlayerState, never in a new subsystem field.**

**Surveyed:** 2026-10-27, on `dev` after B-207. The line numbers are in `Source/world_of_refraction/Public/` unless a path says otherwise.

## Why GameInstance subsystems can't hold co-op state

- **Every machine has its own GameInstance, and it never replicates.** A `UGameInstanceSubsystem` on the host is invisible to a client, and the client's copy is empty. Today 15 of the 21 subsystems hold state there: 11 per-fight, 4 per-run (the tables below).
- **Unreal's standard shape for a networked match** (Epic's networking docs; Lyra; Cedric Neukirchen's *Network Compendium*):
  - **GameMode** (server only): the rules, and spawning.
  - **GameState** (replicated to all): match state everyone sees, such as turn order, the current actor and weather. Lyra splits this into GameState *components* (`ULyraExperienceManagerComponent`, `ULyraPlayerSpawningManagerComponent`), one per system.
  - **PlayerState** (replicated, one per player): what belongs to one player, such as the wallet and their characters. With seamless travel, it survives a level change through `CopyProperties`.
  - **Actors and their components** (replicated): per-combatant state, such as HP, effects, buildup and grid position.
  - **GameInstance** (local only): things that outlive levels and aren't shared, such as settings, the save slot and the local journal.
- **Proposal** (decided when co-op is scheduled, D-065; not a ruling yet): follow Lyra and give each subsystem's fight state its own **GameState component**, or put it on the combatant's own component where it is per-actor. Keep the subsystem as a stateless service that reads from those. The calls stay the same (`GetSubsystem<UTurnManager>()->...`); only the storage moves. Migrating this way is incremental, one subsystem at a time, which fits the project's working principles.

## Per system

Owner key: **GS** = a GameState component; **Actor** = a component on the combatant (replicated); **PS** = PlayerState; **Server** = server-only, not replicated (e.g. timers, pending async work); **Local** = stays in the GameInstance on each machine.

### Combat (per-fight)

| System | State today | Co-op owner | Notes |
|---|---|---|---|
| `UTurnManager` (`Combat/TurnManager.h:349-394`) | `Combatants`, `PendingTurns`, `TurnBelt`, `CurrentActor`, bonus/turn type/payload, `GlobalTurnCount`, `bCombatActive` | **GS** (`UTurnStateComponent`): `TurnBelt`, `CurrentActor`, turn type, turn count, active flag replicated; `Combatants` debt and `PendingTurns` **Server** | Clients only need the belt and whose turn it is to draw the HUD. |
| `UActionExecutor` (`Combat/Actions/ActionExecutor.h:503-1217`) | Pending spell (caster, data, targets, size, damage, element), montage chain, async action context and callbacks, burst queue, telegraph timers, grid cache | **Server** (the action in flight); results reach clients through replicated actor state (HP, effects) plus a multicast for the cosmetic part (montage, VFX) | The biggest one. `EffectIDCounter` (:677) never resets: make it server-only and per-fight. |
| `UDefenseSystem` (`Combat/Defense/DefenseSystem.h:480-483`) | `ActiveDefenseStates` (incl. the input buffer), `WindowTimerHandles` | **Server**; window open/close sent to the defender's owning client (Client RPC) | See "what breaks first" #3. The tuning values (:434-474) are config and stay. |
| `USkillEffectManager` (`Skills/Effects/SkillEffectManager.h:480-494`) | `ActiveEffects` per actor, `FiredOnceThisMatch`, `ArmedConditionals`, `NextInstanceID` | **Actor** (`ActiveEffects` on each combatant, replicated for the HUD); fired-once set and armed conditionals **Server** | The B-042/B-062 effect rework is the natural moment to move it. |
| `UStatusBuildupManager` (`Skills/Effects/StatusBuildupManager.h:204`) | `StatusBarStates` per actor | **Actor** (replicated: the bars are on the HUD) | |
| `UCombatGridSubsystem` (`Combat/Grid/CombatGridSubsystem.h:282`) | `ActorPositions` | **Actor** (each combatant's row/slot, replicated), or **GS** if it stays a map | |
| `UInfusionChargeManager` (`Infusion/InfusionChargeManager.h:274-293`) | Charge state, actor, type, source, level, time, timer | The charging player's input is **owning client**, the committed level **Server** (sent by Server RPC) | Like the defense input: the server must judge timing. |
| `UWeatherStateManager` (`Combat/Mechanics/WeatherStateManager.h:82-94`) | Both party hierarchies, active flag | **GS** (everyone sees the weather) | `LastBroadcastBlendValue` is diagnostic only. |
| `UAIDecisionManager` (`AI/AIDecisionManager.h:123-130`) | `CurrentCombat`, thinking timer, `PendingActor` | **Server** only (AI runs on the server) | Ally AI too. |
| `UCombatCommandMenuSubsystem` (`UI/Combat/CombatCommandMenuSubsystem.h:123-162`) | Menu depth, pending action and targets, current actor | **Local** (each player's own menu); the chosen action goes to the server by Server RPC | The one fight system that rightly stays local, but it must stop *executing* and only *request*. |
| `UTrialRunSubsystem` pending battle hand-off (`Trial/TrialRunSubsystem.h:197-229`) | `PendingBattleParty`, `PendingOpposingParty`, difficulty, first strike, zone, sources, rank, return level | **Server** (GameMode reads it on the battle map) | The accepted transport exception to D-064. It moves with the travel change below. |

### Run and session

| System | State today | Co-op owner | Notes |
|---|---|---|---|
| `UTrialRunSubsystem` run state (`Trial/TrialRunSubsystem.h:186-222`) | Active trial, seed, variation, difficulty, `BeatenEncounters`, `FoundFeatures`, `RunMemories` | **GS** (shared run: everyone sees the same beaten encounters and found features); the seed is **Server** | Joining a run mid-way (pillar 7) needs this replicated to the joiner. |
| `UPartySessionSubsystem` (`Party/PartySessionSubsystem.h:230`) | `Parties` (characters, trial-scoped HP/EP) | **PS** (each player's characters) plus **GS** (the combined `LocalParty` for the fight) | `PartyMatchSetup.md` already models co-op as two players in one `LocalParty`. |
| `UPoolSubsystem` (`Pool/PoolSubsystem.h:167-229`) | Owned weapons, rings, evolutions, spells, abilities, crystals, stones, all currencies | **PS** while playing; the **save** (B-090) between sessions | The shared pool (D-020) is per player. B-090's save struct must include `PoolGold` (:211), which unlike its siblings has no `SaveGame` tag; the tags themselves are ignored (D-059). |
| `AWoRPlayerState` wallet (`Player/WoRPlayerState.h`) | One wallet per player (D-046) | **PS** already | Not yet carried across OpenLevel (see #1 below). |
| `UMemoryJournalSubsystem` (`Memory/MemoryJournalSubsystem.h:68-74`) | Memories, lore, heard V lines, played conversations | **Local** + the save (each player's own journal) | Shared run memories are the run state above. |
| `UMerchantShopSubsystem` (`Shop/MerchantShopSubsystem.h:73-79`) | Open shop widget, merchant, pawn, modal flag | **Local** (UI); the purchase goes to the server | Stock lives in the trial run / data assets. |

### Stateless (no move needed)

`UEconomyService`, `UDamageCalculator`, `UItemExecutor`, `UWeaponManager`, `URingManager`, `UCrystalManager`: only cached pointers to other subsystems or config. They keep working as services. `UEconomyService` (`Private/Currency/EconomyService.cpp:1533-1630`) does look up player 0 (see #4).

## What breaks first

1. **The battle `OpenLevel` drops a client.** `UTrialRunSubsystem` opens every level with `UGameplayStatics::OpenLevelBySoftObjectPtr` (`Private/Trial/TrialRunSubsystem.cpp:79, 126, 162, 194`). On a listen server that is a hard travel: clients are disconnected. Fix: `World->ServerTravel` with **seamless travel** on the GameModes, and `AWoRPlayerState::CopyProperties` to carry the wallet. This also answers the wallet's "not carried across OpenLevel" note. It is the first co-op job, because nothing else can be tested with two players until it works.
2. **Fight state lives in GameInstance subsystems.** The client's `UTurnManager`, `USkillEffectManager` and the rest are empty, so its HUD shows nothing and its menu has no current actor. Fix: the per-system moves above, starting with the turn state (the HUD needs it first), then effects and buildup, then the action executor.
3. **Client defense input never reaches the server.** `ACombatPlayerController` (`Private/Combat/CombatPlayerController.cpp:95-142`) calls `UDefenseSystem::SubmitDefenseInput` directly on its own machine. There are **no** Server/Client RPCs in the module yet. Fix: a `Server_SubmitDefenseInput` on the player controller, with the server judging timing against its own window. The usual allowance for latency is to stamp the input with the client's time and accept it within the window plus a small grace. Fighting games use rollback for this; for a turn-based parry, a server-side grace window is the standard, cheap answer. The same applies to infusion charge commits and command-menu picks.
4. **Player-0 lookups.** `GetFirstPlayerController` / `GetPlayerController(…, 0)` in the pool (`PoolSubsystem.cpp:449, 496, 519`), economy (`EconomyService.cpp:1533, 1564, 1599, 1630`), wallet (`CurrencyWallet.cpp:41, 72`), battle mode (`BattleGameMode.cpp:93, 433`), encounter (`EncounterComponent.cpp:176`), orchestrator (`CombatOrchestrator.cpp:164, 1292`) and camera (`CombatCameraDirector.cpp:383`). On the server, player 0 is always the host, so a client's rewards and purchases go to the host. This is **B-132** (blocked on B-030). The UI and test uses (`PartyControlWidget`, `DismantleWidget`, `MemoryJournalWidget`, `WoRTestScreen`, `NativeUIDebug`, `HUDTestActor`) are local and fine.
5. **Almost nothing replicates.** Only `UCurrencyComponent` sets `SetIsReplicatedByDefault(true)`. `UCharacterDataComponent` and the GAS attribute sets declare `GetLifetimeReplicatedProps` but the component isn't replicated (**B-131**). No actor sets `bReplicates`, and there is no `AGameStateBase` subclass yet: one (`AWoRGameState`) is needed to hold the GameState components above.

## Suggested order (a proposal, decided when co-op is scheduled, D-065)

1. `ServerTravel` + seamless travel + `AWoRGameState` + `CopyProperties` (#1, #5).
2. B-131 (character data replicates) and B-132 (no player-0 lookups).
3. Turn state to a GameState component; the HUD reads it.
4. Server RPCs for the three inputs: defense, infusion commit, command-menu pick (#3).
5. Effects, buildup and grid onto the combatants' components.
6. Action executor: the action in flight stays on the server, cosmetics by multicast.
7. Run state to the GameState; party to PlayerState + GameState.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | B-130: first survey of all 21 GameInstance subsystems, owners and the first breakages. | chore/B-130-coop-state-doc |
| 2026-10-27 | B-227: #1 built (UNBUILT, cloud): `UTrialRunSubsystem::TravelTo` (listen server: `World->ServerTravel`, clients follow; standalone: OpenLevel as before), `bUseSeamlessTravel` + `AWoRGameState` on the trial and battle game modes, `AWoRPlayerState::CopyProperties` carries the wallet. A Blueprint hub game mode must pick `AWoRGameState` and seamless travel too. Spec `WoR.Coop.SeamlessTravel`; debug `wor.GameState.Print`. | feature/B-227-seamless-travel |
